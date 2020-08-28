/*
* EEZ PSU Firmware
* Copyright (C) 2015-present, Envox d.o.o.
*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.

* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.

* You should have received a copy of the GNU General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#if OPTION_DISPLAY

#include <assert.h>

#if defined(EEZ_PLATFORM_STM32)
#include <usbh_hid_keybd.h>
#endif

#include <eez/firmware.h>
#include <eez/sound.h>
#include <eez/system.h>
#include <eez/hmi.h>
#include <eez/keyboard.h>

#include <eez/modules/psu/psu.h>
#include <eez/modules/psu/calibration.h>
#include <eez/modules/psu/channel_dispatcher.h>
#include <eez/modules/psu/devices.h>
#include <eez/modules/psu/event_queue.h>
#include <eez/modules/psu/temperature.h>
#include <eez/modules/psu/trigger.h>
#include <eez/modules/psu/dlog_view.h>
#include <eez/modules/psu/dlog_record.h>
#include <eez/modules/psu/dlog_view.h>
#include <eez/modules/psu/sd_card.h>

#include <eez/modules/psu/gui/psu.h>
#include <eez/modules/psu/gui/animations.h>
#include <eez/modules/psu/gui/edit_mode.h>
#include <eez/modules/psu/gui/keypad.h>
#include <eez/modules/psu/gui/page_ch_settings.h>
#include <eez/modules/psu/gui/page_event_queue.h>
#include <eez/modules/psu/gui/page_sys_settings.h>
#include <eez/modules/psu/gui/page_user_profiles.h>
#include <eez/modules/psu/gui/password.h>
#include <eez/modules/psu/gui/file_manager.h>
#include <eez/modules/psu/gui/touch_calibration.h>

#if OPTION_ENCODER
#include <eez/modules/mcu/encoder.h>
#endif
#if EEZ_PLATFORM_STM32
#include <eez/modules/mcu/button.h>
#endif

#if defined(EEZ_PLATFORM_SIMULATOR)
#include <eez/platform/simulator/front_panel.h>
#endif

#define CONF_GUI_TOAST_DURATION_MS 2000L

#define CONF_GUI_ENTERING_STANDBY_PAGE_TIMEOUT_MS 2000
#define CONF_GUI_STANDBY_PAGE_TIMEOUT_MS 4000
#define CONF_GUI_DISPLAY_OFF_PAGE_TIMEOUT_MS 2000
#define CONF_GUI_WELCOME_PAGE_TIMEOUT_MS 2000

using namespace eez::psu::gui;

////////////////////////////////////////////////////////////////////////////////

namespace eez {
namespace psu {
namespace gui {

PsuAppContext g_psuAppContext;

static unsigned g_skipChannelCalibrations;
static unsigned g_skipDateTimeSetup;
static unsigned g_skipEthernetSetup;

static bool g_showSetupWizardQuestionCalled;
Channel *g_channel = nullptr;
int g_channelIndex = -1;
static WidgetCursor g_toggleOutputWidgetCursor;

#if EEZ_PLATFORM_STM32
static mcu::Button g_userSwitch(USER_SW_GPIO_Port, USER_SW_Pin, true, true);
#endif

Value g_progress;

static int16_t g_externalActionId = ACTION_ID_NONE;
static const size_t MAX_NUM_EXTERNAL_DATA_ITEM_VALUES = 20;
static struct {
    Value value;
    char text[128 + 1];
    int textIndex;
} g_externalDataItemValues[MAX_NUM_EXTERNAL_DATA_ITEM_VALUES];

SelectFromEnumPage g_selectFromEnumPage;

int g_displayTestColorIndex = 0;

bool showSetupWizardQuestion();
void onEncoder(int counter, bool clicked);

static void moveToNextFocusCursor();

////////////////////////////////////////////////////////////////////////////////

void showDebugTraceLog() {
    event_queue::setFilter(event_queue::EVENT_TYPE_DEBUG);
    pushPage(PAGE_ID_EVENT_QUEUE);
}

////////////////////////////////////////////////////////////////////////////////

PsuAppContext::PsuAppContext() {
    m_pushProgressPage = false;
    m_popProgressPage = false;
}

void PsuAppContext::stateManagment() {
    if (m_popProgressPage) {
        doHideProgressPage();
    }

    if (m_clearTextMessage) {
        m_clearTextMessage = false;
        if (getActivePageId() == PAGE_ID_TEXT_MESSAGE) {
            popPage();
            m_textMessage[0] = 0;
        }
    }

    // remove alert message after period of time
    uint32_t inactivityPeriod = eez::hmi::getInactivityPeriod();
    if (getActivePageId() == INTERNAL_PAGE_ID_TOAST_MESSAGE) {
        ToastMessagePage *page = (ToastMessagePage *)getActivePage();
        if (!page->hasAction() && inactivityPeriod >= CONF_GUI_TOAST_DURATION_MS) {
            popPage();
        }
    }

    uint32_t tickCount = millis();

    // wait some time for transitional pages
    int activePageId = getActivePageId();
    if (activePageId == PAGE_ID_STANDBY) {
        if (int32_t(tickCount - m_showPageTime) < CONF_GUI_STANDBY_PAGE_TIMEOUT_MS) {
            return;
        }
    } else if (activePageId == PAGE_ID_ENTERING_STANDBY) {
        if (int32_t(tickCount - m_showPageTime) >= CONF_GUI_ENTERING_STANDBY_PAGE_TIMEOUT_MS) {
            uint32_t saved_showPageTime = m_showPageTime;
            showStandbyPage();
            m_showPageTime = saved_showPageTime;
        }
        return;
    } else if (activePageId == PAGE_ID_WELCOME) {
        if (int32_t(tickCount - m_showPageTime) < CONF_GUI_WELCOME_PAGE_TIMEOUT_MS) {
            return;
        }
    }

#if EEZ_PLATFORM_STM32
    if (g_userSwitch.isLongPress()) {
        action_select_user_switch_action();
    } else if (g_userSwitch.isClicked()) {
        action_user_switch_clicked();
    }
#endif

    // turn the screen off if power is down and system is booted
    if (!psu::isPowerUp()) {
    	if (g_isBooted && !g_shutdownInProgress && getActivePageId() != PAGE_ID_NONE) {
    		showPage(PAGE_ID_NONE);
    		eez::mcu::display::turnOff();
    	}
        return;
    }

    // turn display on/off depending on displayState
    if (
        psu::persist_conf::devConf.displayState == 0 && 
        (activePageId != PAGE_ID_DISPLAY_OFF && isTouchCalibrated())
    ) {
        showPage(PAGE_ID_DISPLAY_OFF);
        return;
    } else if (psu::persist_conf::devConf.displayState == 1 && activePageId == PAGE_ID_DISPLAY_OFF) {
        eez::mcu::display::turnOn();
        showPage(getMainPageId());
        return;
    }

    // select page to go after transitional page
    if (activePageId == PAGE_ID_WELCOME || activePageId == PAGE_ID_STANDBY || activePageId == PAGE_ID_ENTERING_STANDBY) {
        if (!isTouchCalibrated()) {
            // touch screen is not calibrated
            showPage(PAGE_ID_TOUCH_CALIBRATION_INTRO);
        } else {
            showPage(getMainPageId());
        }
        return;
    }

    // start touch screen calibration automatically after period of time
    if (activePageId == PAGE_ID_TOUCH_CALIBRATION_INTRO) {
        if (inactivityPeriod >= 20 * 1000UL) {
            enterTouchCalibration();
            return;
        }
    }

    // TODO move this to some other place
#if OPTION_ENCODER
    int counter;
    bool clicked;
    mcu::encoder::read(counter, clicked);
#endif

    // handling of display off page
    if (activePageId == PAGE_ID_DISPLAY_OFF) {
        if (eez::mcu::display::isOn()) {
            if (int32_t(tickCount - m_showPageTime) >= CONF_GUI_DISPLAY_OFF_PAGE_TIMEOUT_MS) {
                eez::mcu::display::turnOff();
                m_showPageTime = tickCount;
            }
        }
        return;
    }

    // TODO move this to some other place
#if OPTION_ENCODER
    if (counter != 0 || clicked) {
        eez::hmi::noteActivity();
    }
    onEncoder(counter, clicked);
#endif

#if GUI_BACK_TO_MAIN_ENABLED
    uint32_t inactivityPeriod = eez::idle::getInactivityPeriod();

    if (
        activePageId == PAGE_ID_EVENT_QUEUE ||
        activePageId == PAGE_ID_USER_PROFILES ||
        activePageId == PAGE_ID_USER_PROFILE_0_SETTINGS ||
        activePageId == PAGE_ID_USER_PROFILE_SETTINGS
    ) {
        if (inactivityPeriod >= GUI_BACK_TO_MAIN_DELAY * 1000UL) {
            showPage(PAGE_ID_MAIN);
        }
    }
#endif

    //
    if (g_rprogAlarm) {
        g_rprogAlarm = false;
        errorMessage("Max. remote prog. voltage exceeded.\nPlease remove it immediately!");
    }

    // show startup wizard
    if (!isFrontPanelLocked() && activePageId == PAGE_ID_MAIN && int32_t(millis() - m_showPageTime) >= 250L) {
        if (showSetupWizardQuestion()) {
            return;
        }
    }

    if (m_pushProgressPage) {
        doShowProgressPage();
    }

    if (m_showTextMessage) {
        m_showTextMessage = false;
        if (getActivePageId() != PAGE_ID_TEXT_MESSAGE) {
            pushPage(PAGE_ID_TEXT_MESSAGE);
        } else {
            ++m_textMessageVersion;
        }
    } else {
        // clear text message if active page is not PAGE_ID_TEXT_MESSAGE
        if (getActivePageId() != PAGE_ID_TEXT_MESSAGE && m_textMessage[0]) {
            m_textMessage[0] = 0;
        }
    }

    dlog_view::stateManagment();

    if (m_showUncaughtScriptExceptionMessage) {
        m_showUncaughtScriptExceptionMessage = false;
        errorMessageWithAction("Uncaught script exception!", showDebugTraceLog, "Show debug trace log");
    }

    if (!sd_card::isMounted(nullptr)) {
        if (
            isPageOnStack(PAGE_ID_DLOG_PARAMS) ||
            isPageOnStack(PAGE_ID_DLOG_VIEW) ||
            isPageOnStack(PAGE_ID_USER_PROFILES) ||
            isPageOnStack(PAGE_ID_USER_PROFILE_SETTINGS) ||
            isPageOnStack(PAGE_ID_USER_PROFILE_0_SETTINGS)
        ) {
            showPage(PAGE_ID_MAIN);
        }
    }

    // call m_checkAsyncOperationStatus
    if (getActivePageId() == PAGE_ID_ASYNC_OPERATION_IN_PROGRESS) {
        if (m_asyncOperationInProgressParams.checkStatus) {
            m_asyncOperationInProgressParams.checkStatus();
        }
    }
}

bool PsuAppContext::isActiveWidget(const WidgetCursor &widgetCursor) {
    if (getActivePageId() == PAGE_ID_TOUCH_CALIBRATION) {
        if (touch::getEventType() != EVENT_TYPE_TOUCH_NONE) {
            if (widgetCursor.widget->type == WIDGET_TYPE_TEXT && isTouchPointActivated()) {
                return true;
            }
        }
    }

    return AppContext::isActiveWidget(widgetCursor);
}

int PsuAppContext::getMainPageId() {
    return PAGE_ID_MAIN;
}

bool isSysSettingsSubPage(int pageId) {
    return pageId == PAGE_ID_SYS_SETTINGS_TEMPERATURE ||
        pageId == PAGE_ID_SYS_SETTINGS_PROTECTIONS ||
        pageId == PAGE_ID_SYS_SETTINGS_IO ||
        pageId == PAGE_ID_SYS_SETTINGS_DATE_TIME ||
        pageId == PAGE_ID_SYS_SETTINGS_ENCODER ||
        pageId == PAGE_ID_SYS_SETTINGS_USB ||
        pageId == PAGE_ID_SYS_SETTINGS_ETHERNET ||
        pageId == PAGE_ID_SYS_SETTINGS_TRIGGER ||
        pageId == PAGE_ID_SYS_SETTINGS_RAMP_AND_DELAY ||
        pageId == PAGE_ID_SYS_SETTINGS_DISPLAY ||
        pageId == PAGE_ID_SYS_SETTINGS_SOUND ||
        pageId == PAGE_ID_SYS_INFO;
}

bool isChSettingsSubPage(int pageId) {
    return pageId == PAGE_ID_CH_SETTINGS_PROT_CLEAR ||
        pageId == PAGE_ID_CH_SETTINGS_PROT_OVP ||
        pageId == PAGE_ID_CH_SETTINGS_PROT_OCP ||
        pageId == PAGE_ID_CH_SETTINGS_PROT_OPP ||
        pageId == PAGE_ID_CH_SETTINGS_PROT_OTP ||
        pageId == PAGE_ID_CH_SETTINGS_TRIGGER ||
        pageId == PAGE_ID_CH_SETTINGS_LISTS ||
        pageId == PAGE_ID_CH_SETTINGS_ADV_OPTIONS ||
        pageId == PAGE_ID_CH_SETTINGS_ADV_RANGES ||
        pageId == PAGE_ID_CH_SETTINGS_ADV_VIEW ||
        pageId == PAGE_ID_CH_SETTINGS_INFO;
}


void PsuAppContext::onPageChanged(int previousPageId, int activePageId) {
    AppContext::onPageChanged(previousPageId, activePageId);

    if (previousPageId == PAGE_ID_WELCOME) {
        animateFadeOutFadeIn();
    }

    g_focusEditValue = Value();

    if (previousPageId == PAGE_ID_EVENT_QUEUE) {
        if (getActivePageId() == PAGE_ID_MAIN) {
            animateSlideUp();
        }
    } else if (previousPageId == PAGE_ID_MAIN) {
        if (activePageId == PAGE_ID_SYS_SETTINGS) {
            animateShowSysSettings();
        } else if (isSysSettingsSubPage(activePageId)) {
            animateShowSysSettings();
        } else if (activePageId == PAGE_ID_CH_SETTINGS) {
            animateSlideDown();
        } else if (isChSettingsSubPage(activePageId)) {
            animateSlideDown();
        } else if (activePageId == PAGE_ID_EVENT_QUEUE) {
            animateSlideDown();
        } else if (activePageId == PAGE_ID_USER_PROFILES) {
            animateSlideDown();
        } else if (activePageId == PAGE_ID_SYS_SETTINGS_TRACKING) {
            animateSlideDown();
        } else if (activePageId == PAGE_ID_DLOG_VIEW) {
            animateSlideDown();
        } else if (activePageId == PAGE_ID_FILE_MANAGER) {
            animateSlideDown();
        }
    } else if (previousPageId == PAGE_ID_USER_PROFILES) {
        if (activePageId == PAGE_ID_USER_PROFILE_0_SETTINGS) {
            animateSlideDown();
        } else if (activePageId == PAGE_ID_USER_PROFILE_SETTINGS) {
            animateSlideDown();
        } else if (activePageId == PAGE_ID_MAIN) {
            animateSlideUp();
        }
    } else if (previousPageId == PAGE_ID_USER_PROFILE_0_SETTINGS) {
        if (activePageId == PAGE_ID_MAIN) {
            animateSlideUp();
        } else if (activePageId == PAGE_ID_USER_PROFILES) {
            animateSlideUp();
        }
    } else if (previousPageId == PAGE_ID_USER_PROFILE_SETTINGS) {
        if (activePageId == PAGE_ID_MAIN) {
            animateSlideUp();
        } else if (activePageId == PAGE_ID_USER_PROFILES) {
            animateSlideUp();
        }
    } else if (previousPageId == PAGE_ID_SYS_SETTINGS_TRACKING) {
        if (activePageId == PAGE_ID_MAIN) {
            animateSlideUp();
        } else if (activePageId == PAGE_ID_SYS_SETTINGS_COUPLING) {
            animateSlideLeft();
        }
    } else if (previousPageId == PAGE_ID_SYS_SETTINGS_COUPLING) {
        if (activePageId == PAGE_ID_SYS_SETTINGS_TRACKING) {
            animateSlideRight();
        } else if (activePageId == PAGE_ID_MAIN) {
            animateSlideUp();
        }
    } else if (previousPageId == PAGE_ID_SYS_SETTINGS) {
        if (activePageId == PAGE_ID_MAIN) {
            animateHideSysSettings();
        } else if (isSysSettingsSubPage(activePageId)) {
            animateSettingsSlideLeft(activePageId == PAGE_ID_SYS_INFO);
        }
    } else if (previousPageId == PAGE_ID_SYS_SETTINGS_TRIGGER || previousPageId == PAGE_ID_SYS_SETTINGS_RAMP_AND_DELAY) {
        if (activePageId == PAGE_ID_MAIN) {
            animateHideSysSettings();
        } else if (activePageId == PAGE_ID_CH_SETTINGS_TRIGGER) {
            animateSlideUp();
        } else if (activePageId == PAGE_ID_SYS_SETTINGS) {
            animateSettingsSlideRight(false);
        }
    } else if (isSysSettingsSubPage(previousPageId)) {
        if (activePageId == PAGE_ID_MAIN) {
            animateHideSysSettings();
        } else if (activePageId == PAGE_ID_SYS_SETTINGS) {
            animateSettingsSlideRight(previousPageId == PAGE_ID_SYS_INFO);
        } else if (activePageId == PAGE_ID_SYS_SETTINGS_MQTT) {
            animateSettingsSlideLeft(false);
        }
    } else if (previousPageId == PAGE_ID_CH_SETTINGS) {
        if (activePageId == PAGE_ID_MAIN) {
            animateSlideUp();
        } else if (isChSettingsSubPage(activePageId)) {
            animateSlideLeft();
        }
    } else if (previousPageId == PAGE_ID_CH_SETTINGS_TRIGGER) {
        if (activePageId == PAGE_ID_MAIN) {
            animateSlideUp();
        } else if (activePageId == PAGE_ID_SYS_SETTINGS_TRIGGER || activePageId == PAGE_ID_SYS_SETTINGS_RAMP_AND_DELAY) {
            animateSlideDown();
        } else if (activePageId == PAGE_ID_CH_SETTINGS_LISTS) {
            animateSlideDown();
        } else if (activePageId == PAGE_ID_CH_SETTINGS) {
            animateSlideRight();
        }
    } else if (previousPageId == PAGE_ID_CH_SETTINGS_LISTS) {
        if (activePageId == PAGE_ID_CH_SETTINGS_TRIGGER) {
            animateSlideUp();
        }
    } else if (isChSettingsSubPage(previousPageId)) {
        if (activePageId == PAGE_ID_MAIN) {
            animateSlideUp();
        } else if (activePageId == PAGE_ID_CH_SETTINGS) {
            animateSlideRight();
        }
    } else if (previousPageId == PAGE_ID_DLOG_VIEW) {
        if (activePageId == PAGE_ID_MAIN) {
            animateSlideUp();
        }
    } else if (previousPageId == PAGE_ID_FILE_MANAGER) {
        if (activePageId == PAGE_ID_MAIN) {
            animateSlideUp();
        } else if (activePageId == PAGE_ID_IMAGE_VIEW) {
            animateFadeOutFadeIn();
        }
    } else if (previousPageId == PAGE_ID_IMAGE_VIEW) {
        animateFadeOutFadeIn();
    } else if (previousPageId == PAGE_ID_SYS_SETTINGS_MQTT) {
        if (activePageId == PAGE_ID_SYS_SETTINGS_ETHERNET) {
            animateSlideRight();
        }
    }
}

bool PsuAppContext::isFocusWidget(const WidgetCursor &widgetCursor) {
    if (isPageOnStack(PAGE_ID_CH_SETTINGS_LISTS)) {
        return ((ChSettingsListsPage *)getPage(PAGE_ID_CH_SETTINGS_LISTS))->isFocusWidget(widgetCursor);
    }

    return (widgetCursor.cursor == -1 || widgetCursor.cursor == g_focusCursor) && widgetCursor.widget->data == g_focusDataId && widgetCursor.widget->action != ACTION_ID_EDIT_NO_FOCUS && isEncoderEnabledInActivePage();
}

bool PsuAppContext::isAutoRepeatAction(int action) {
    return action == ACTION_ID_KEYPAD_BACK ||
        action == ACTION_ID_CHANNEL_LISTS_PREVIOUS_PAGE ||
        action == ACTION_ID_CHANNEL_LISTS_NEXT_PAGE;
}

void PsuAppContext::onPageTouch(const WidgetCursor &foundWidget, Event &touchEvent) {
    if (isFrontPanelLocked()) {
        errorMessage("Front panel is locked!");
        return;
    }

    int activePageId = getActivePageId();

    if (activePageId == PAGE_ID_TOUCH_CALIBRATION) {
        onTouchCalibrationPageTouch(foundWidget, touchEvent);
        return;
    }

    if (touchEvent.type == EVENT_TYPE_TOUCH_DOWN) {
        if (activePageId == PAGE_ID_EDIT_MODE_SLIDER) {
            edit_mode_slider::onTouchDown();
        } else if (activePageId == PAGE_ID_EDIT_MODE_STEP) {
            edit_mode_step::onTouchDown();
        }
    } else if (touchEvent.type == EVENT_TYPE_TOUCH_MOVE) {
        if (activePageId == PAGE_ID_EDIT_MODE_SLIDER) {
            edit_mode_slider::onTouchMove();
        } else if (activePageId == PAGE_ID_EDIT_MODE_STEP) {
            edit_mode_step::onTouchMove();
        }        
    } else if (touchEvent.type == EVENT_TYPE_TOUCH_UP) {
        if (activePageId == PAGE_ID_EDIT_MODE_SLIDER) {
            edit_mode_slider::onTouchUp();
        } else if (activePageId == PAGE_ID_EDIT_MODE_STEP) {
            edit_mode_step::onTouchUp();
        } else if (activePageId == PAGE_ID_TOUCH_CALIBRATION_INTRO) {
            enterTouchCalibration();
        }
    } else if (touchEvent.type == EVENT_TYPE_LONG_TOUCH) {
        if (activePageId == PAGE_ID_NONE || activePageId == PAGE_ID_STANDBY) {
            // wake up on long touch
            psu::changePowerState(true);
        } else if (activePageId == PAGE_ID_DISPLAY_OFF) {
            // turn ON display on long touch
            psu::persist_conf::setDisplayState(1);
        }
    } else if (touchEvent.type == EVENT_TYPE_EXTRA_LONG_TOUCH) {
        // start touch screen calibration in case of really long touch
        showPage(PAGE_ID_TOUCH_CALIBRATION_INTRO);
    }

    AppContext::onPageTouch(foundWidget, touchEvent);
}

bool PsuAppContext::testExecuteActionOnTouchDown(int action) {
    return action == ACTION_ID_CHANNEL_TOGGLE_OUTPUT || isAutoRepeatAction(action);
}

bool PsuAppContext::isBlinking(const Cursor cursor, int16_t id) {
    if (g_focusCursor == cursor && g_focusDataId == id && g_focusEditValue.getType() != VALUE_TYPE_NONE) {
        return true;
    }

    return AppContext::isBlinking(cursor, id);
}

bool PsuAppContext::isWidgetActionEnabled(const WidgetCursor &widgetCursor) {
    const Widget *widget = widgetCursor.widget;
    if (widget->action) {
        if (isFrontPanelLocked()) {
            int activePageId = getActivePageId();
            if (activePageId == PAGE_ID_KEYPAD ||
                activePageId == PAGE_ID_TOUCH_CALIBRATION_YES_NO ||
                activePageId == PAGE_ID_TOUCH_CALIBRATION_YES_NO_CANCEL ||
                activePageId == INTERNAL_PAGE_ID_TOAST_MESSAGE
            ) {
                return true;
            }
            
            if (widget->action != ACTION_ID_SYS_FRONT_PANEL_UNLOCK) {
                return false;
            }
        }
    
        if (widget->action == ACTION_ID_SHOW_EVENT_QUEUE) {
            static const uint32_t CONF_SHOW_EVENT_QUEUE_INACTIVITY_TIMESPAN_SINCE_LAST_SHOW_PAGE_MS = 500;
            if (millis() - m_showPageTime < CONF_SHOW_EVENT_QUEUE_INACTIVITY_TIMESPAN_SINCE_LAST_SHOW_PAGE_MS) {
                return false;
            }
        }

        if (widget->action == ACTION_ID_FILE_MANAGER_SELECT_FILE) {
            return file_manager::isSelectFileActionEnabled(widgetCursor.cursor);
        }

        if (widget->action == ACTION_ID_EDIT || widget->action == ACTION_ID_EDIT_NO_FOCUS) {
            if (widgetCursor.widget->data == DATA_ID_CALIBRATION_POINT_MEASURED_VALUE) {
                auto page = (ChSettingsCalibrationEditPage *)getPage(PAGE_ID_CH_SETTINGS_CALIBRATION_EDIT);
                return page->canEditMeasuredValue();
            }
            return channel_dispatcher::isEditEnabled(widgetCursor);
        }
    }

    return AppContext::isWidgetActionEnabled(widgetCursor);
}

void PsuAppContext::doShowProgressPage() {
    set(Cursor(-1), DATA_ID_ALERT_MESSAGE, Value(m_progressMessage));
    m_dialogCancelCallback = m_progressAbortCallback;
    pushPage(m_progressWithoutAbort ? PAGE_ID_PROGRESS_WITHOUT_ABORT : PAGE_ID_PROGRESS);
    m_pushProgressPage = false;
}

void PsuAppContext::showProgressPage(const char *message, void (*abortCallback)()) {
    m_progressMessage = message;
    m_progressWithoutAbort = false;
    m_progressAbortCallback = abortCallback;
    m_pushProgressPage = true;
    g_progress = Value();

    if (osThreadGetId() == g_guiTaskHandle) {
    	doShowProgressPage();
    }
}

void PsuAppContext::showProgressPageWithoutAbort(const char *message) {
    m_progressMessage = message;
    m_progressWithoutAbort = true;
    m_pushProgressPage = true;
    g_progress = Value();

    if (osThreadGetId() == g_guiTaskHandle) {
    	doShowProgressPage();
    }
}

bool PsuAppContext::updateProgressPage(size_t processedSoFar, size_t totalSize) {
    if (totalSize > 0) {
        g_progress = Value((int)round((processedSoFar * 1.0f / totalSize) * 100.0f), VALUE_TYPE_PERCENTAGE);
    } else {
        g_progress = Value((uint32_t)processedSoFar, VALUE_TYPE_SIZE);
    }

    if (m_pushProgressPage) {
        return true;
    }

    return isPageOnStack(m_progressWithoutAbort ? PAGE_ID_PROGRESS_WITHOUT_ABORT : PAGE_ID_PROGRESS);
}

void PsuAppContext::doHideProgressPage() {
    removePageFromStack(m_progressWithoutAbort ? PAGE_ID_PROGRESS_WITHOUT_ABORT : PAGE_ID_PROGRESS);
    m_popProgressPage = false;
}

void PsuAppContext::hideProgressPage() {
    if (m_pushProgressPage) {
        m_pushProgressPage = false;
    } else {
        m_popProgressPage = true;
    }

    if (osThreadGetId() == g_guiTaskHandle) {
    	doHideProgressPage();
    }
}

void PsuAppContext::showAsyncOperationInProgress(const char *message, void (*checkStatus)()) {
    m_asyncOperationInProgressParams.message = message;
    m_asyncOperationInProgressParams.checkStatus = checkStatus;

    if (osThreadGetId() == g_guiTaskHandle) {
        doShowAsyncOperationInProgress();
    } else {
        sendMessageToGuiThread(GUI_QUEUE_MESSAGE_TYPE_SHOW_ASYNC_OPERATION_IN_PROGRESS);
    }
}

void PsuAppContext::doShowAsyncOperationInProgress() {
    set(Cursor(-1), DATA_ID_ALERT_MESSAGE, Value(m_asyncOperationInProgressParams.message));

    if (getActivePageId() != PAGE_ID_ASYNC_OPERATION_IN_PROGRESS) {
        m_asyncOperationInProgressParams.startTime = millis();
        pushPage(PAGE_ID_ASYNC_OPERATION_IN_PROGRESS);
    }
}

void PsuAppContext::hideAsyncOperationInProgress() {
    if (osThreadGetId() == g_guiTaskHandle) {
        doHideAsyncOperationInProgress();
    } else {
        sendMessageToGuiThread(GUI_QUEUE_MESSAGE_TYPE_HIDE_ASYNC_OPERATION_IN_PROGRESS);
    }
}

void PsuAppContext::doHideAsyncOperationInProgress() {
    removePageFromStack(PAGE_ID_ASYNC_OPERATION_IN_PROGRESS);
}

uint32_t PsuAppContext::getAsyncInProgressStartTime() {
    return m_asyncOperationInProgressParams.startTime;
}

void PsuAppContext::setTextMessage(const char *message, unsigned int len) {
    len = MIN(len, MAX_TEXT_MESSAGE_LEN);
    strncpy(m_textMessage, message, len);
    m_textMessage[len] = 0;
    m_showTextMessage = true;
}

void PsuAppContext::clearTextMessage() {
    m_clearTextMessage =  true;
}

const char *PsuAppContext::getTextMessage() {
    return m_textMessage;
}

uint8_t PsuAppContext::getTextMessageVersion() {
    return m_textMessageVersion;
}

void PsuAppContext::showUncaughtScriptExceptionMessage() {
    m_showUncaughtScriptExceptionMessage = true;
}

void TextInputParams::onSet(char *value) {
    popPage();

    g_psuAppContext.m_textInputParams.m_input = value;
    g_psuAppContext.m_inputReady = true;
}

void TextInputParams::onCancel() {
    popPage();

    g_psuAppContext.m_textInputParams.m_input = nullptr;
    g_psuAppContext.m_inputReady = true;
}

const char *PsuAppContext::textInput(const char *label, size_t minChars, size_t maxChars, const char *value) {
    m_inputLabel = label[0] ? label : nullptr;
    m_textInputParams.m_minChars = minChars;
    m_textInputParams.m_maxChars = maxChars;
    m_textInputParams.m_input = value;

    m_inputReady = false;

    sendMessageToGuiThread(GUI_QUEUE_MESSAGE_TYPE_SHOW_TEXT_INPUT);

    while (!m_inputReady) {
        osDelay(5);
    }

    return m_textInputParams.m_input;
}

void PsuAppContext::doShowTextInput() {
    Keypad::startPush(m_inputLabel, m_textInputParams.m_input, m_textInputParams.m_minChars, m_textInputParams.m_maxChars, false, m_textInputParams.onSet, m_textInputParams.onCancel);
}

void NumberInputParams::onSet(float value) {
    popPage();

    g_psuAppContext.m_numberInputParams.m_input = value;
    g_psuAppContext.m_inputReady = true;
}

void NumberInputParams::onCancel() {
    popPage();

    g_psuAppContext.m_numberInputParams.m_input = NAN;
    g_psuAppContext.m_inputReady = true;
}

float PsuAppContext::numberInput(const char *label, Unit unit, float min, float max, float value) {
    m_inputLabel = label[0] ? label : nullptr;

    m_numberInputParams.m_options.editValueUnit = unit;
    m_numberInputParams.m_options.min = min;
    m_numberInputParams.m_options.enableMinButton();
    m_numberInputParams.m_options.max = max;
    m_numberInputParams.m_options.enableMaxButton();
    m_numberInputParams.m_options.flags.signButtonEnabled = m_numberInputParams.m_options.min < 0;
    m_numberInputParams.m_options.flags.dotButtonEnabled = true;

    m_numberInputParams.m_input = value;

    m_inputReady = false;
    sendMessageToGuiThread(GUI_QUEUE_MESSAGE_TYPE_SHOW_NUMBER_INPUT);

    while (!m_inputReady) {
        osDelay(5);
    }

    return m_numberInputParams.m_input;
}

void PsuAppContext::doShowNumberInput() {
    NumericKeypad::start(this, m_inputLabel, Value(m_numberInputParams.m_input, m_numberInputParams.m_options.editValueUnit), m_numberInputParams.m_options, m_numberInputParams.onSet, nullptr, m_numberInputParams.onCancel);
}

void IntegerInputParams::onSet(float value) {
    popPage();

    g_psuAppContext.m_integerInputParams.m_input = (int32_t)value;
    g_psuAppContext.m_integerInputParams.canceled = false;
    g_psuAppContext.m_inputReady = true;
}

void IntegerInputParams::onCancel() {
    popPage();

    g_psuAppContext.m_integerInputParams.canceled = true;
    g_psuAppContext.m_inputReady = true;
}

bool PsuAppContext::integerInput(const char *label, int32_t min, int32_t max, int32_t &value) {
    m_inputLabel = label[0] ? label : nullptr;

    m_integerInputParams.m_options.editValueUnit = UNIT_UNKNOWN;
    m_integerInputParams.m_options.min = (float)min;
    m_integerInputParams.m_options.enableMinButton();
    m_integerInputParams.m_options.max = (float)max;
    m_integerInputParams.m_options.enableMaxButton();
    m_integerInputParams.m_options.flags.signButtonEnabled = false;
    m_integerInputParams.m_options.flags.dotButtonEnabled = false;

    m_integerInputParams.m_input = value;

    m_inputReady = false;
    sendMessageToGuiThread(GUI_QUEUE_MESSAGE_TYPE_SHOW_INTEGER_INPUT);

    while (!m_inputReady) {
        osDelay(5);
    }

    if (!m_integerInputParams.canceled) {
        value = m_integerInputParams.m_input;
        return true;
    }

    return false;
}

void PsuAppContext::doShowIntegerInput() {
    NumericKeypad::start(this, m_inputLabel, Value((float)m_integerInputParams.m_input, UNIT_UNKNOWN), m_integerInputParams.m_options, m_integerInputParams.onSet, nullptr, m_integerInputParams.onCancel);
}

bool PsuAppContext::dialogOpen(int *err) {
    if (osThreadGetId() == g_guiTaskHandle) {
        if (!isPageOnStack(getExternalAssetsFirstPageId())) {
            dialogResetDataItemValues();
            pushPage(getExternalAssetsFirstPageId());
        }
    } else {
        sendMessageToGuiThread(GUI_QUEUE_MESSAGE_TYPE_DIALOG_OPEN);
        do {
            osDelay(1);
        } while (!isPageOnStack(getExternalAssetsFirstPageId()));
    }
    return true;
}

DialogActionResult PsuAppContext::dialogAction(uint32_t timeoutMs, const char *&selectedActionName) {
    if (timeoutMs != 0) {
        timeoutMs = millis() + timeoutMs;
        if (timeoutMs == 0) {
            timeoutMs = 1;
        }
    }

    while (
        (timeoutMs == 0 || (int32_t)(millis() - timeoutMs) < 0) &&
        g_externalActionId == ACTION_ID_NONE &&
        isPageOnStack(getExternalAssetsFirstPageId())
    ) {
        osDelay(5);
    }

    if (g_externalActionId != ACTION_ID_NONE) {
        selectedActionName = getActionName(g_externalActionId);
        g_externalActionId = ACTION_ID_NONE;
        return DIALOG_ACTION_RESULT_SELECTED_ACTION;
    }

    return isPageOnStack(getExternalAssetsFirstPageId()) ? DIALOG_ACTION_RESULT_TIMEOUT : DIALOG_ACTION_RESULT_EXIT;
}

void PsuAppContext::dialogResetDataItemValues() {
    for (uint32_t i = 0; i < MAX_NUM_EXTERNAL_DATA_ITEM_VALUES; i++) {
        g_externalDataItemValues[i].value = Value();
        g_externalDataItemValues[i].textIndex = 0;
    }
}

void PsuAppContext::dialogSetDataItemValue(int16_t dataId, Value& value) {
    if (dataId < 0) {
        dataId = -dataId;
    }
    dataId--;
    if ((uint16_t)dataId < MAX_NUM_EXTERNAL_DATA_ITEM_VALUES) {
        g_externalDataItemValues[dataId].value = value;
    }
}

void PsuAppContext::dialogSetDataItemValue(int16_t dataId, const char *str) {
    if (dataId < 0) {
        dataId = -dataId;
    }
    dataId--;
    if ((uint16_t)dataId < MAX_NUM_EXTERNAL_DATA_ITEM_VALUES) {
        int textIndex = g_externalDataItemValues[dataId].textIndex;
        if (textIndex == 1) {
            g_externalDataItemValues[dataId].textIndex = 0;
        } else {
            g_externalDataItemValues[dataId].textIndex = 1;
        }
        strcpy(g_externalDataItemValues[dataId].text + textIndex, str);
        g_externalDataItemValues[dataId].value = (const char *)g_externalDataItemValues[dataId].text + textIndex;
    }
}

void PsuAppContext::dialogClose() {
    if (osThreadGetId() == g_guiTaskHandle) {
        if (isPageOnStack(getExternalAssetsFirstPageId())) {
            removePageFromStack(getExternalAssetsFirstPageId());
        }
    } else {
        sendMessageToGuiThread(GUI_QUEUE_MESSAGE_TYPE_DIALOG_CLOSE);
    }
}

int PsuAppContext::getExtraLongTouchActionHook(const WidgetCursor &widgetCursor) {
    return ACTION_ID_SHOW_TOUCH_CALIBRATION_INTRO;
}

void MenuInputParams::onSet(int value) {
    popPage();

    g_psuAppContext.m_menuInputParams.m_input = value;
    g_psuAppContext.m_inputReady = true;
}

int PsuAppContext::menuInput(const char *label, MenuType menuType, const char **menuItems) {
    m_inputLabel = label[0] ? label : nullptr;

    m_menuInputParams.m_type = menuType;
    m_menuInputParams.m_items = menuItems;

    m_inputReady = false;
    sendMessageToGuiThread(GUI_QUEUE_MESSAGE_TYPE_SHOW_MENU_INPUT);

    while (!m_inputReady) {
        osDelay(5);
    }

    return m_menuInputParams.m_input;
}

void PsuAppContext::doShowMenuInput() {
    showMenu(this, m_inputLabel, m_menuInputParams.m_type, m_menuInputParams.m_items, m_menuInputParams.onSet);
}

int PsuAppContext::select(const char **options, int defaultSelection) {
    m_selectParams.m_options = options;
    m_selectParams.m_defaultSelection = defaultSelection;

    m_inputReady = false;
    sendMessageToGuiThread(GUI_QUEUE_MESSAGE_TYPE_SHOW_SELECT);
    do {
        osDelay(1);
    } while (!isPageOnStack(INTERNAL_PAGE_ID_SELECT_FROM_ENUM));

    while (!m_inputReady && isPageOnStack(INTERNAL_PAGE_ID_SELECT_FROM_ENUM)) {
        osDelay(5);
    }

    if (m_inputReady) {
        return m_selectParams.m_input;
    }
    
    return 0;
}

void SelectParams::enumDefinition(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET_VALUE) {
        value = (uint16_t)(cursor + 1);
    } else if (operation == DATA_OPERATION_GET_LABEL) {
        const char *label = g_psuAppContext.m_selectParams.m_options[cursor];
        if (label) {
            value = label;
        }
    }
}

void SelectParams::onSelect(uint16_t value) {
    popPage();

    g_psuAppContext.m_selectParams.m_input = value;
    g_psuAppContext.m_inputReady = true;
}


void PsuAppContext::doShowSelect() {
    pushSelectFromEnumPage(SelectParams::enumDefinition, m_selectParams.m_defaultSelection, nullptr, SelectParams::onSelect, false, true);
}

bool PsuAppContext::canExecuteActionWhenTouchedOutsideOfActivePage(int pageId, int action) {
    if (pageId == PAGE_ID_CH_SETTINGS_LISTS_INSERT_MENU) {
        return action == ACTION_ID_SHOW_CHANNEL_LISTS_DELETE_MENU || action == ACTION_ID_SHOW_CHANNEL_LISTS_FILE_MENU;
    }

    if (pageId == PAGE_ID_CH_SETTINGS_LISTS_DELETE_MENU) {
        return action == ACTION_ID_SHOW_CHANNEL_LISTS_INSERT_MENU || action == ACTION_ID_SHOW_CHANNEL_LISTS_FILE_MENU;
    }

    if (pageId == PAGE_ID_CH_SETTINGS_LISTS_FILE_MENU) {
        return action == ACTION_ID_SHOW_CHANNEL_LISTS_INSERT_MENU || action == ACTION_ID_SHOW_CHANNEL_LISTS_DELETE_MENU;
    }

    return false;
}

void PsuAppContext::updatePage(int i, WidgetCursor &widgetCursor) {
    AppContext::updatePage(i, widgetCursor);

    if (getActivePageId() == PAGE_ID_TOUCH_CALIBRATION_YES_NO || getActivePageId() == PAGE_ID_TOUCH_CALIBRATION_YES_NO_CANCEL) {
        auto eventType = touch::getEventType();
        if (eventType == EVENT_TYPE_TOUCH_DOWN || eventType == EVENT_TYPE_TOUCH_MOVE) {
            mcu::display::selectBuffer(m_pageNavigationStack[m_pageNavigationStackPointer].displayBufferIndex);
            int x = MIN(MAX(touch::getX(), 1), eez::mcu::display::getDisplayWidth() - 2);
            int y = MIN(MAX(touch::getY(), 1), eez::mcu::display::getDisplayHeight() - 2);
            eez::mcu::display::setColor(255, 255, 255);
            eez::mcu::display::fillRect(x - 1, y - 1, x + 1, y + 1);
        }
    } else if (getActivePageId() == PAGE_ID_TOUCH_TEST) {
        mcu::display::selectBuffer(m_pageNavigationStack[i].displayBufferIndex);

        Cursor cursor(-1);

        if (get(cursor, DATA_ID_TOUCH_CALIBRATED_PRESSED).getInt()) {
            int x = MIN(MAX(get(cursor, DATA_ID_TOUCH_CALIBRATED_X).getInt(), 1), eez::mcu::display::getDisplayWidth() - 2);
            int y = MIN(MAX(get(cursor, DATA_ID_TOUCH_CALIBRATED_Y).getInt(), 1), eez::mcu::display::getDisplayHeight() - 2);
            eez::mcu::display::setColor(0, 0, 255);
            eez::mcu::display::fillRect(x - 1, y - 1, x + 1, y + 1);
        }

        if (get(cursor, DATA_ID_TOUCH_FILTERED_PRESSED).getInt()) {
            int x = MIN(MAX(get(cursor, DATA_ID_TOUCH_FILTERED_X).getInt(), 1), eez::mcu::display::getDisplayWidth() - 2);
            int y = MIN(MAX(get(cursor, DATA_ID_TOUCH_FILTERED_Y).getInt(), 1), eez::mcu::display::getDisplayHeight() - 2);
            eez::mcu::display::setColor(0, 255, 0);
            eez::mcu::display::fillRect(x - 1, y - 1, x + 1, y + 1);
        }
    }
}

////////////////////////////////////////////////////////////////////////////////

bool isChannelCalibrationsDone() {
    for (int i = 0; i < CH_NUM; ++i) {
        Channel &channel = Channel::get(i);
        if (channel.isOk() && !channel.isCalibrationExists()) {
            return false;
        }
    }
    return true;
}

bool isDateTimeSetupDone() {
    return persist_conf::devConf.dateValid && persist_conf::devConf.timeValid;
}

void dateTimeYes() {
    executeAction(ACTION_ID_SHOW_SYS_SETTINGS_DATE_TIME);
}

void dateTimeNo() {
    persist_conf::setSkipDateTimeSetup(1);
}

void ethernetYes() {
    executeAction(ACTION_ID_SHOW_SYS_SETTINGS_ETHERNET);
}

void ethernetNo() {
    persist_conf::setSkipEthernetSetup(1);
}

bool showSetupWizardQuestion() {
    if (!g_showSetupWizardQuestionCalled) {
        g_showSetupWizardQuestionCalled = true;
        
        g_skipChannelCalibrations = persist_conf::devConf.skipChannelCalibrations;
        g_skipDateTimeSetup = persist_conf::devConf.skipDateTimeSetup;
        g_skipEthernetSetup = persist_conf::devConf.skipEthernetSetup;
    }


#if OPTION_ETHERNET
    if (!g_skipEthernetSetup) {
        g_skipEthernetSetup = 1;
        if (!persist_conf::isEthernetEnabled()) {
            yesNoLater("Do you want to setup ethernet?", ethernetYes, ethernetNo);
            return true;
        }
    }
#endif

    if (!g_skipDateTimeSetup) {
        g_skipDateTimeSetup = 1;
        if (!isDateTimeSetupDone()) {
            yesNoLater("Do you want to set date and time?", dateTimeYes, dateTimeNo);
            return true;
        }
    }

    return false;
}

static int g_iChannelSetValue;

void changeValue(Channel &channel, const Value &value, float minValue, float maxValue, float defValue, void (*onSetValue)(float)) {
    NumericKeypadOptions options;

    options.channelIndex = channel.channelIndex;

    options.editValueUnit = value.getUnit();

    options.min = minValue;
    options.max = maxValue;
    options.def = defValue;

    options.enableMaxButton();
    options.enableDefButton();
    options.flags.signButtonEnabled = true;
    options.flags.dotButtonEnabled = true;

    NumericKeypad::start(0, value, options, onSetValue, 0, 0);
}

void onSetVoltageLimit(float limit) {
    Channel &channel = Channel::get(g_iChannelSetValue);
    channel_dispatcher::setVoltageLimit(channel, limit);
    popPage();
    infoMessage("Voltage limit changed!");
}

void changeVoltageLimit(int iChannel) {
    g_iChannelSetValue = iChannel;
    Channel &channel = Channel::get(iChannel);
    float minLimit = channel_dispatcher::getUMin(channel);
    float maxLimit = channel_dispatcher::getUMax(channel);
    float defLimit = channel_dispatcher::getUMax(channel);
    changeValue(channel,
                MakeValue(channel_dispatcher::getULimit(channel), UNIT_VOLT),
                minLimit, maxLimit, defLimit, onSetVoltageLimit);
}

void onSetCurrentLimit(float limit) {
    Channel &channel = Channel::get(g_iChannelSetValue);
    channel_dispatcher::setCurrentLimit(channel, limit);
    popPage();
    infoMessage("Current limit changed!");
}

void changeCurrentLimit(int iChannel) {
    g_iChannelSetValue = iChannel;
    Channel &channel = Channel::get(iChannel);
    float minLimit = channel_dispatcher::getIMin(channel);
    float maxLimit = channel_dispatcher::getIMax(channel);
    float defLimit = channel_dispatcher::getIMax(channel);
    changeValue(channel,
                MakeValue(channel_dispatcher::getILimit(channel), UNIT_AMPER),
                minLimit, maxLimit, defLimit, onSetCurrentLimit);
}

void onSetPowerLimit(float limit) {
    Channel &channel = Channel::get(g_iChannelSetValue);
    channel_dispatcher::setPowerLimit(channel, limit);
    popPage();
    infoMessage("Power limit changed!");
}

void changePowerLimit(int iChannel) {
    g_iChannelSetValue = iChannel;
    Channel &channel = Channel::get(iChannel);
    float minLimit = channel_dispatcher::getPowerMinLimit(channel);
    float maxLimit = channel_dispatcher::getPowerMaxLimit(channel);
    float defLimit = channel_dispatcher::getPowerDefaultLimit(channel);
    changeValue(channel,
                MakeValue(channel_dispatcher::getPowerLimit(channel), UNIT_WATT),
                minLimit, maxLimit, defLimit, onSetPowerLimit);
}

void onSetPowerTripLevel(float level) {
    Channel &channel = Channel::get(g_iChannelSetValue);
    channel_dispatcher::setOppParameters(channel, channel.prot_conf.flags.p_state ? 1 : 0, level, channel.prot_conf.p_delay);
    popPage();
    infoMessage("Power protection level changed!");
}

void changePowerTripLevel(int iChannel) {
    g_iChannelSetValue = iChannel;
    Channel &channel = Channel::get(iChannel);
    float minLevel = channel_dispatcher::getOppMinLevel(channel);
    float maxLevel = channel_dispatcher::getOppMaxLevel(channel);
    float defLevel = channel_dispatcher::getOppDefaultLevel(channel);
    changeValue(channel,
        MakeValue(channel_dispatcher::getPowerProtectionLevel(channel), UNIT_WATT),
        minLevel, maxLevel, defLevel, onSetPowerTripLevel);
}

void onSetPowerTripDelay(float delay) {
    Channel &channel = Channel::get(g_iChannelSetValue);
    channel_dispatcher::setOppParameters(channel, channel.prot_conf.flags.p_state ? 1 : 0, channel_dispatcher::getPowerProtectionLevel(channel), delay);
    popPage();
    infoMessage("Power protection delay changed!");
}

void changePowerTripDelay(int iChannel) {
    g_iChannelSetValue = iChannel;
    Channel &channel = Channel::get(iChannel);
    float minDelay = channel.params.OPP_MIN_DELAY;
    float maxDelay = channel.params.OPP_MAX_DELAY;
    float defaultDelay = channel.params.OPP_DEFAULT_DELAY;
    changeValue(channel,
        MakeValue(channel.prot_conf.p_delay, UNIT_SECOND),
        minDelay, maxDelay, defaultDelay, onSetPowerTripDelay);
}

void onSetTemperatureTripLevel(float level) {
    Channel &channel = Channel::get(g_iChannelSetValue);
    channel_dispatcher::setOtpParameters(channel, temperature::getChannelSensorState(&channel) ? 1 : 0, level, temperature::getChannelSensorDelay(&channel));
    popPage();
    infoMessage("Temperature protection level changed!");
}

void changeTemperatureTripLevel(int iChannel) {
    g_iChannelSetValue = iChannel;
    Channel &channel = Channel::get(iChannel);
    float minLevel = OTP_AUX_MIN_LEVEL;
    float maxLevel = OTP_AUX_MAX_LEVEL;
    float defLevel = OTP_AUX_DEFAULT_LEVEL;
    changeValue(channel,
        MakeValue(temperature::getChannelSensorLevel(&channel), UNIT_CELSIUS),
        minLevel, maxLevel, defLevel, onSetTemperatureTripLevel);
}

void onSetTemperatureTripDelay(float delay) {
    Channel &channel = Channel::get(g_iChannelSetValue);
    channel_dispatcher::setOtpParameters(channel, temperature::getChannelSensorState(&channel) ? 1 : 0, temperature::getChannelSensorLevel(&channel), delay);
    popPage();
    infoMessage("Temperature protection delay changed!");
}

void changeTemperatureTripDelay(int iChannel) {
    g_iChannelSetValue = iChannel;
    Channel &channel = Channel::get(iChannel);
    float minDelay = OTP_AUX_MIN_DELAY;
    float maxDelay = OTP_AUX_MAX_DELAY;
    float defaultDelay = OTP_CH_DEFAULT_DELAY;
    changeValue(channel,
        MakeValue(temperature::getChannelSensorDelay(&channel), UNIT_SECOND),
        minDelay, maxDelay, defaultDelay, onSetTemperatureTripDelay);
}

void psuErrorMessage(const Cursor cursor, Value value, void (*ok_callback)()) {
    if (value.getType() == VALUE_TYPE_SCPI_ERROR) {
        int iChannel = cursor >= 0 ? cursor : (g_channel ? g_channel->channelIndex : 0);
        Channel &channel = Channel::get(iChannel);
        if (value.getScpiError() == SCPI_ERROR_VOLTAGE_LIMIT_EXCEEDED) {
            if (channel_dispatcher::getULimit(channel) < channel_dispatcher::getUMaxLimit(channel)) {
                if (ok_callback) {
                    ok_callback();
                }
                errorMessageWithAction(value, changeVoltageLimit, "Change voltage limit", iChannel);
                return;
            }
        } else if (value.getScpiError() == SCPI_ERROR_CURRENT_LIMIT_EXCEEDED) {
            if (channel_dispatcher::getILimit(channel) < channel_dispatcher::getIMaxLimit(channel)) {
                if (ok_callback) {
                    ok_callback();
                }
                errorMessageWithAction(value, changeCurrentLimit, "Change current limit", iChannel);
                return;
            }
        } else if (value.getScpiError() == SCPI_ERROR_POWER_LIMIT_EXCEEDED || value.getScpiError() == SCPI_ERROR_MODULE_TOTAL_POWER_LIMIT_EXCEEDED) {
            if (channel_dispatcher::getPowerLimit(channel) < channel_dispatcher::getPowerMaxLimit(channel)) {
                if (ok_callback) {
                    ok_callback();
                }
                errorMessageWithAction(value, changePowerLimit, "Change power limit", iChannel);
                return;
            }
        }
    }

    if (ok_callback) {
        ok_callback();
    }

    errorMessage(value);
}

////////////////////////////////////////////////////////////////////////////////

Cursor g_focusCursor = Cursor(0);
int16_t g_focusDataId = DATA_ID_CHANNEL_U_EDIT;
Value g_focusEditValue;

void setFocusCursor(const Cursor cursor, int16_t dataId) {
    g_focusCursor = cursor;
    g_focusDataId = dataId;
    g_focusEditValue = Value();
}

bool isFocusChanged() {
    return g_focusEditValue.getType() != VALUE_TYPE_NONE;
}

////////////////////////////////////////////////////////////////////////////////

#if OPTION_ENCODER

static bool g_isEncoderEnabledInActivePage;
uint32_t g_focusEditValueChangedTime;

float encoderIncrement(Value value, int counter, float min, float max, int channelIndex, float precision) {
    if (channelIndex != -1) {
        precision = psu::channel_dispatcher::getValuePrecision(psu::Channel::get(channelIndex), value.getUnit(), value.getFloat());
    }

    // TODO 
    if (precision == 0) {
        precision = 0.001f;
    }

    float step;

    if (mcu::encoder::g_encoderMode == mcu::encoder::ENCODER_MODE_AUTO) {
        StepValues stepValues;
        edit_mode_step::getStepValues(stepValues);
        step = stepValues.values[stepValues.count - mcu::encoder::getAutoModeStepLevel() - 1];
    } else {
        step = edit_mode_step::getCurrentEncoderStepValue().getFloat();
    }

    float newValue = value.getFloat() + step * counter;
    newValue = roundPrec(newValue, step);

    if (getAllowZero(g_focusCursor, g_focusDataId) && newValue < value.getFloat() && newValue < min) {
        newValue = 0;
    } else {
        newValue = clamp(newValue, min, max);
    }

    return newValue;
}

static bool isEncoderEnabledForWidget(const WidgetCursor &widgetCursor) {
    if (widgetCursor.widget->action != ACTION_ID_EDIT) {
        return false;
    }

    if (!g_psuAppContext.isWidgetActionEnabled(widgetCursor)) {
        return false;
    }

    return true;
}

static bool g_focusCursorIsEnabled;

void isEnabledFocusCursorStep(const WidgetCursor &widgetCursor) {
    if (isEncoderEnabledForWidget(widgetCursor)) {
        if (g_focusCursor == widgetCursor.cursor && g_focusDataId == widgetCursor.widget->data) {
            g_focusCursorIsEnabled = true;
        }
    }
}

bool isEnabledFocusCursor(Cursor cursor, int16_t dataId) {
    g_focusCursorIsEnabled = false;
    enumWidgets(&g_psuAppContext, isEnabledFocusCursorStep);
    return g_focusCursorIsEnabled;
}

void isEncoderEnabledInActivePageCheckWidget(const WidgetCursor &widgetCursor) {
    if (isEncoderEnabledForWidget(widgetCursor)) {
        g_isEncoderEnabledInActivePage = true;
    }
}

bool isEncoderEnabledInActivePage() {
    // encoder is enabled if active page contains widget with "edit" action
    g_isEncoderEnabledInActivePage = false;
    enumWidgets(&g_psuAppContext, isEncoderEnabledInActivePageCheckWidget);
    return g_isEncoderEnabledInActivePage;
}

////////////////////////////////////////////////////////////////////////////////

static void doUnlockFrontPanel() {
    popPage();

    psu::persist_conf::lockFrontPanel(false);
    infoMessage("Front panel is unlocked!");
}

static void checkPasswordToUnlockFrontPanel() {
    checkPassword("Password: ", psu::persist_conf::devConf.systemPassword, doUnlockFrontPanel);
}

void lockFrontPanel() {
    psu::persist_conf::lockFrontPanel(true);
    infoMessage("Front panel is locked!");
}

void unlockFrontPanel() {
    if (strlen(psu::persist_conf::devConf.systemPassword) > 0) {
        checkPasswordToUnlockFrontPanel();
    } else {
        psu::persist_conf::lockFrontPanel(false);
        infoMessage("Front panel is unlocked!");
    }
}

bool isFrontPanelLocked() {
    return psu::g_rlState != psu::RL_STATE_LOCAL;
}

////////////////////////////////////////////////////////////////////////////////

void showWelcomePage() {
    showPage(PAGE_ID_WELCOME);
}

void showEnteringStandbyPage() {
    showPage(PAGE_ID_ENTERING_STANDBY);
}

void showStandbyPage() {
    showPage(PAGE_ID_STANDBY);
}

void showSavingPage() {
    showPage(PAGE_ID_SAVING);
}

void showShutdownPage() {
    showPage(PAGE_ID_SHUTDOWN);
}

////////////////////////////////////////////////////////////////////////////////

Value g_alertMessage;
Value g_alertMessage2;
Value g_alertMessage3;

////////////////////////////////////////////////////////////////////////////////

void pushToastMessage(ToastMessagePage *toastMessage) {
    pushPage(INTERNAL_PAGE_ID_TOAST_MESSAGE, toastMessage);
}

void infoMessage(const char *message) {
    pushToastMessage(ToastMessagePage::create(INFO_TOAST, message));
}

void infoMessage(Value value) {
    pushToastMessage(ToastMessagePage::create(INFO_TOAST, value));
}

void infoMessage(const char *message, void (*action)(), const char *actionLabel) {
    pushToastMessage(ToastMessagePage::create(INFO_TOAST, message, action, actionLabel));
}

void errorMessage(const char *message, bool autoDismiss) {
    pushToastMessage(ToastMessagePage::create(ERROR_TOAST, message, autoDismiss));
    sound::playBeep();
}

void errorMessage(Value value) {
    pushToastMessage(ToastMessagePage::create(ERROR_TOAST, value));
    sound::playBeep();
}

void errorMessageWithAction(Value value, void (*action)(int param), const char *actionLabel, int actionParam) {
    pushToastMessage(ToastMessagePage::create(ERROR_TOAST, value, action, actionLabel, actionParam));
    sound::playBeep();
}

void errorMessageWithAction(const char *message, void (*action)(), const char *actionLabel) {
    pushToastMessage(ToastMessagePage::create(ERROR_TOAST, message, action, actionLabel));
    sound::playBeep();
}

////////////////////////////////////////////////////////////////////////////////

void yesNoDialog(int yesNoPageId, const char *message, void (*yes_callback)(), void (*no_callback)(), void (*cancel_callback)()) {
    set(Cursor(-1), DATA_ID_ALERT_MESSAGE, Value(message));

    g_psuAppContext.m_dialogYesCallback = yes_callback;
    g_psuAppContext.m_dialogNoCallback = no_callback;
    g_psuAppContext.m_dialogCancelCallback = cancel_callback;

    pushPage(yesNoPageId);
}

void yesNoLater(const char *message, void (*yes_callback)(), void (*no_callback)(), void (*later_callback)()) {
    set(Cursor(-1), DATA_ID_ALERT_MESSAGE, Value(message));

    g_psuAppContext.m_dialogYesCallback = yes_callback;
    g_psuAppContext.m_dialogNoCallback = no_callback;
    g_psuAppContext.m_dialogLaterCallback = later_callback;

    pushPage(PAGE_ID_YES_NO_LATER);
}

void areYouSure(void (*yes_callback)()) {
    yesNoDialog(PAGE_ID_YES_NO, "Are you sure?", yes_callback, 0, 0);
}

void areYouSureWithMessage(const char *message, void (*yes_callback)()) {
    yesNoDialog(PAGE_ID_ARE_YOU_SURE_WITH_MESSAGE, message, yes_callback, 0, 0);
}

////////////////////////////////////////////////////////////////////////////////

void dialogYes() {
    auto callback = g_psuAppContext.m_dialogYesCallback;

    popPage();

    if (callback) {
        callback();
    }
}

void dialogNo() {
    auto callback = g_psuAppContext.m_dialogNoCallback;

    popPage();

    if (callback) {
        callback();
    }
}

void dialogCancel() {
    auto callback = g_psuAppContext.m_dialogCancelCallback;

    popPage();

    if (callback) {
        callback();
    }
}

void dialogOk() {
    dialogYes();
}

void dialogLater() {
    auto callback = g_psuAppContext.m_dialogLaterCallback;

    popPage();

    if (callback) {
        callback();
    }
}

////////////////////////////////////////////////////////////////////////////////

void pushSelectFromEnumPage(
    AppContext *appContext,
    EnumDefinition enumDefinition,
    uint16_t currentValue,
    bool (*disabledCallback)(uint16_t value),
    void (*onSet)(uint16_t),
    bool smallFont,
    bool showRadioButtonIcon
) {
	g_selectFromEnumPage.init(appContext, g_enumDefinitions[enumDefinition], currentValue, disabledCallback, onSet, smallFont, showRadioButtonIcon);
    appContext->pushPage(INTERNAL_PAGE_ID_SELECT_FROM_ENUM, &g_selectFromEnumPage);
}

void pushSelectFromEnumPage(
    AppContext *appContext,
    void(*enumDefinitionFunc)(DataOperationEnum operation, Cursor cursor, Value &value),
    uint16_t currentValue,
    bool(*disabledCallback)(uint16_t value),
    void(*onSet)(uint16_t),
    bool smallFont,
    bool showRadioButtonIcon
) {
	g_selectFromEnumPage.init(appContext, enumDefinitionFunc, currentValue, disabledCallback, onSet, smallFont, showRadioButtonIcon);
    appContext->pushPage(INTERNAL_PAGE_ID_SELECT_FROM_ENUM, &g_selectFromEnumPage);
}

const EnumItem *getActiveSelectEnumDefinition() {
    if (g_selectFromEnumPage.appContext && g_selectFromEnumPage.appContext->getActivePage() == &g_selectFromEnumPage) {
        return g_selectFromEnumPage.getEnumDefinition();
    }
    return nullptr;
}

void popSelectFromEnumPage() {
    if (g_selectFromEnumPage.appContext) {
        g_selectFromEnumPage.appContext->popPage();
    }
}

void showMainPage() {
    showPage(PAGE_ID_MAIN);
}

void goBack() {
    if (getNumPagesOnStack() > 1) {
        action_show_previous_page();
    } else if (getActivePageId() != PAGE_ID_MAIN) {
        showMainPage();
    } else if (persist_conf::isMaxView()) {
        action_toggle_channels_max_view();
    }    
}

void takeScreenshot() {
    using namespace scpi;
    if (!g_screenshotGenerating) {
        g_screenshotGenerating = true;
        sendMessageToLowPriorityThread(THREAD_MESSAGE_SCREENSHOT);
    }
}

////////////////////////////////////////////////////////////////////////////////

static int g_findNextFocusCursorState = 0; 
static Cursor g_nextFocusCursor = Cursor(0);
static uint16_t g_nextFocusDataId = DATA_ID_CHANNEL_U_EDIT;

void findNextFocusCursor(const WidgetCursor &widgetCursor) {
    if (isEncoderEnabledForWidget(widgetCursor)) {
        if (g_findNextFocusCursorState == 0) {
            g_nextFocusCursor = widgetCursor.cursor;
            g_nextFocusDataId = widgetCursor.widget->data;
            g_findNextFocusCursorState = 1;
        }

        if (g_findNextFocusCursorState == 1) {
            if (g_focusCursor == widgetCursor.cursor && g_focusDataId == widgetCursor.widget->data) {
                g_findNextFocusCursorState = 2;
            }
        } else if (g_findNextFocusCursorState == 2) {
            g_nextFocusCursor = widgetCursor.cursor;
            g_nextFocusDataId = widgetCursor.widget->data;
            g_findNextFocusCursorState = 3;
        }
    }
}

static void moveToNextFocusCursor() {
    g_findNextFocusCursorState = 0;
    enumWidgets(&g_psuAppContext, findNextFocusCursor);
    if (g_findNextFocusCursorState > 0) {
        g_focusCursor = g_nextFocusCursor;
        g_focusDataId = g_nextFocusDataId;
    }
}

bool onEncoderConfirmation() {
    if (edit_mode::isActive(&g_psuAppContext) && !edit_mode::isInteractiveMode() && edit_mode::getEditValue() != edit_mode::getCurrentValue()) {
        edit_mode::nonInteractiveSet();
        return true;
    }
    return false;
}

Unit getCurrentEncoderUnit() {
    Page *activePage = getActivePage();
    if (activePage) {
        Unit unit = activePage->getEncoderUnit();
        if (unit != UNIT_UNKNOWN) {
            return unit;
        }
    }

    auto editValue = getEditValue(g_focusCursor, g_focusDataId);
    return editValue.getUnit();
}

void onEncoder(int counter, bool clicked) {
    if (g_shutdownInProgress || isFrontPanelLocked()) {
        return;
    }

    uint32_t tickCount = millis();

    // wait for confirmation of changed value ...
    if (isFocusChanged() && tickCount - g_focusEditValueChangedTime >= ENCODER_CHANGE_TIMEOUT * 1000L) {
        // ... on timeout discard changed value
        g_focusEditValue = Value();
    }

    if (!isEnabledFocusCursor(g_focusCursor, g_focusDataId)) {
        moveToNextFocusCursor();
    }

    int activePageId = getActivePageId();
    Page *activePage = getActivePage();

    if (counter != 0) {
        if (!isEnabledFocusCursor(g_focusCursor, g_focusDataId)) {
            moveToNextFocusCursor();
        }

        if (isEncoderEnabledInActivePage()) {
            if ((g_focusCursor >= 0 && g_focusCursor < CH_NUM) || activePageId != PAGE_ID_MAIN) {
                Value value;
                if (persist_conf::devConf.encoderConfirmationMode && g_focusEditValue.getType() != VALUE_TYPE_NONE) {
                    value = g_focusEditValue;
                } else {
                    value = getEditValue(g_focusCursor, g_focusDataId);
                }

                float min = getMin(g_focusCursor, g_focusDataId).getFloat();
                float max = getMax(g_focusCursor, g_focusDataId).getFloat();

                float newValue;

                Value stepValue = getEncoderStep(g_focusCursor, g_focusDataId);
                if (stepValue.getType() != VALUE_TYPE_NONE) {
                    float step;
                    if (mcu::encoder::g_encoderMode == mcu::encoder::ENCODER_MODE_AUTO) {
                        step = stepValue.getFloat();
                    } else {
                        step = edit_mode_step::getCurrentEncoderStepValue().getFloat();
                    }
                    newValue = roundPrec(value.getFloat() + counter * step, step);
                    if (getAllowZero(g_focusCursor, g_focusDataId) && newValue < value.getFloat() && newValue < min) {
                        newValue = 0;
                    } else {
                        newValue = clamp(newValue, min, max);
                    }
                } else {
                    float precision = getEncoderPrecision(g_focusCursor, g_focusDataId, 0);
                    newValue = encoderIncrement(value, counter, min, max, g_focusCursor, precision);
                }

                Value limitValue = getLimit(g_focusCursor, g_focusDataId);
                if (limitValue.getType() != VALUE_TYPE_NONE) {
                    float limit = limitValue.getFloat();
                    if (newValue > limit && value.getFloat() < limit) {
                        newValue = limit;
                    }
                }

                if (persist_conf::devConf.encoderConfirmationMode) {
                    g_focusEditValue = MakeValue(newValue, value.getUnit());
                    g_focusEditValueChangedTime = millis();
                } else {
                    Value result = set(g_focusCursor, g_focusDataId, MakeValue(newValue, value.getUnit()));
                    if (result.getType() == VALUE_TYPE_SCPI_ERROR) {
                        psuErrorMessage(g_focusCursor, result);
                    }
                }
            }
        }

        if (activePageId == PAGE_ID_EDIT_MODE_KEYPAD || activePageId == PAGE_ID_NUMERIC_KEYPAD) {
            ((NumericKeypad *)getActiveKeypad())->onEncoder(counter);
        }
#if defined(EEZ_PLATFORM_SIMULATOR)
        else if (activePageId == PAGE_ID_FRONT_PANEL_NUMERIC_KEYPAD) {
            ((NumericKeypad *)getActiveKeypad())->onEncoder(counter);
        }
#endif
        else if (activePageId == PAGE_ID_EDIT_MODE_STEP) {
            edit_mode_step::onEncoder(counter);
        } else if (activePageId == PAGE_ID_FILE_MANAGER || activePageId == PAGE_ID_FILE_BROWSER) {
            file_manager::onEncoder(counter);
        } else if (activePageId == PAGE_ID_SYS_SETTINGS_DISPLAY_TEST) {
            if (counter < 0) {
                counter = -counter;
            }
            g_displayTestColorIndex = (g_displayTestColorIndex + counter) % 4;
        } else if (activePage) {
            activePage->onEncoder(counter);
        }
    }

    if (clicked) {
        if (activePageId == PAGE_ID_SYS_SETTINGS_DISPLAY_TEST) {
            popPage();
        } else if (isEncoderEnabledInActivePage()) {
            if (isFocusChanged()) {
                // confirmation
                Value result = set(g_focusCursor, g_focusDataId, g_focusEditValue);
                if (result.getType() == VALUE_TYPE_SCPI_ERROR) {
                    psuErrorMessage(g_focusCursor, result);
                } else {
                    g_focusEditValue = Value();
                }
            } else if (!onEncoderConfirmation()) {
                moveToNextFocusCursor();
            }

            sound::playClick();
        } else {
            int activePageId = getActivePageId();
            if (activePageId == PAGE_ID_EDIT_MODE_KEYPAD || activePageId == PAGE_ID_NUMERIC_KEYPAD) {
                ((NumericKeypad *)getActiveKeypad())->onEncoderClicked();
            }
#if defined(EEZ_PLATFORM_SIMULATOR)
            else if (activePageId == PAGE_ID_FRONT_PANEL_NUMERIC_KEYPAD) {
                ((NumericKeypad *)getActiveKeypad())->onEncoderClicked();
            }
#endif
            else if (activePage) {
                activePage->onEncoderClicked();
            }
        }
    }
}

#endif

////////////////////////////////////////////////////////////////////////////////

static void channelInitiateTrigger() {
    int err = trigger::initiate();
    if (err != SCPI_RES_OK) {
        psuErrorMessage(g_toggleOutputWidgetCursor.cursor, MakeScpiErrorValue(err));
    }
}

void channelReinitiateTrigger() {
    trigger::abort();
    channelInitiateTrigger();
}

void clearTrip(int channelIndex) {
    Channel &channel = Channel::get(channelIndex);
    channel_dispatcher::clearProtection(channel);

    if (temperature::sensors[temp_sensor::AUX].isTripped()) {
        temperature::sensors[temp_sensor::AUX].clearProtection();
    }

    channelToggleOutput();
}

void doChannelToggleOutput() {
    Channel &channel = *g_channel;
    bool triggerModeEnabled =
        (channel_dispatcher::getVoltageTriggerMode(channel) != TRIGGER_MODE_FIXED ||
        channel_dispatcher::getCurrentTriggerMode(channel) != TRIGGER_MODE_FIXED) && !channel.isRemoteProgrammingEnabled();

    if (channel.isOutputEnabled()) {
        if (triggerModeEnabled) {
            trigger::abort();
        }

        channel_dispatcher::outputEnable(channel, false);
    } else {
        if (triggerModeEnabled) {
            if (trigger::isIdle()) {
                g_toggleOutputWidgetCursor = getFoundWidgetAtDown();
                channelInitiateTrigger();
            } else if (trigger::isInitiated()) {
                trigger::abort();
            } else {
                yesNoDialog(PAGE_ID_YES_NO_L, "Trigger is active. Re-initiate trigger?", channelReinitiateTrigger, 0, 0);
            }
        } else {
            channel_dispatcher::outputEnable(channel, true);
        }
    }
}

void channelCalibrationsYes() {
    executeAction(ACTION_ID_SHOW_CH_SETTINGS_CAL);
}

void channelCalibrationsNo() {
    persist_conf::setSkipChannelCalibrations(persist_conf::devConf.skipChannelCalibrations | (1 << g_channel->channelIndex));
    doChannelToggleOutput();
}

void channelToggleOutput() {
    selectChannelByCursor();
    Channel &channel = *g_channel;
    int channelIndex;
    if (channel_dispatcher::isTripped(channel, channelIndex)) {
        if (temperature::sensors[temp_sensor::AUX].isTripped()) {
            errorMessageWithAction("AUX temp. sensor is tripped!", clearTrip, "Clear", channelIndex);
        } else {
            errorMessageWithAction("Channel is tripped!", clearTrip, "Clear", channelIndex);
        }
    } else {
        if (!channel.isOutputEnabled() && !channel.isCalibrationExists()) {
            if (!(g_skipChannelCalibrations & (1 << channel.channelIndex))) {
                g_skipChannelCalibrations |= 1 << channel.channelIndex;
                yesNoLater("Do you want to calibrate channel?", channelCalibrationsYes, channelCalibrationsNo, doChannelToggleOutput);
                return;
            }
        }

        doChannelToggleOutput();
    }
}

void selectChannelByCursor() {
    if (getFoundWidgetAtDown().cursor >= 0 && getFoundWidgetAtDown().cursor < CH_NUM) {
        selectChannel(&Channel::get(getFoundWidgetAtDown().cursor));
    }
}

void selectChannel(Channel *channel) {
    g_channel = channel;
    if (g_channel) {
        hmi::selectSlot(g_channel->slotIndex);
        g_channelIndex = g_channel->channelIndex;
    } else {
        g_channelIndex = -1;
    }
}

bool isDefaultViewVertical() {
    return persist_conf::devConf.channelsViewMode == CHANNELS_VIEW_MODE_NUMERIC || persist_conf::devConf.channelsViewMode == CHANNELS_VIEW_MODE_VERT_BAR;
}

} // namespace gui
} // namespace psu
} // namespace eez

////////////////////////////////////////////////////////////////////////////////

namespace eez {
namespace mcu {
namespace display {

uint16_t transformColorHook(uint16_t color) {
    if (color == COLOR_ID_CHANNEL1 && g_channelIndex >= 0 && g_channelIndex < psu::CH_NUM) {
        return COLOR_ID_CHANNEL1 + g_channelIndex;
    }
    return color;
}

}
}
}

////////////////////////////////////////////////////////////////////////////////

namespace eez {
namespace gui {

////////////////////////////////////////////////////////////////////////////////

#if EEZ_PLATFORM_STM32
AppContext &getRootAppContext() {
    return g_psuAppContext;
}
#endif

void stateManagmentHook() {
#if defined(EEZ_PLATFORM_SIMULATOR)
    g_frontPanelAppContext.stateManagment();
#endif

    g_psuAppContext.stateManagment();
}

////////////////////////////////////////////////////////////////////////////////

using namespace eez::psu::gui;

static EventQueuePage g_EventQueuePage;
static ChSettingsOvpProtectionPage g_ChSettingsOvpProtectionPage;
static ChSettingsOcpProtectionPage g_ChSettingsOcpProtectionPage;
static ChSettingsOppProtectionPage g_ChSettingsOppProtectionPage;
static ChSettingsOtpProtectionPage g_ChSettingsOtpProtectionPage;
static ChSettingsAdvOptionsPage g_ChSettingsAdvOptionPage;
static ChSettingsAdvRangesPage g_ChSettingsAdvRangesPage;
static ChSettingsAdvViewPage g_ChSettingsAdvViewPage;
static ChSettingsTriggerPage g_ChSettingsTriggerPage;
static ChSettingsListsPage g_ChSettingsListsPage;
static ChSettingsCalibrationEditPage g_ChSettingsCalibrationEditPage;
static ChSettingsCalibrationViewPage g_ChSettingsCalibrationViewPage;
static SysSettingsDateTimePage g_SysSettingsDateTimePage;
#if OPTION_ETHERNET
static SysSettingsEthernetPage g_SysSettingsEthernetPage;
static SysSettingsEthernetStaticPage g_SysSettingsEthernetStaticPage;
static SysSettingsMqttPage g_SysSettingsMqttPage;
#endif
static SysSettingsProtectionsPage g_SysSettingsProtectionsPage;
static SysSettingsTriggerPage g_SysSettingsTriggerPage;
static SysSettingsIOPinsPage g_SysSettingsIOPinsPage;
static SysSettingsTemperaturePage g_SysSettingsTemperaturePage;
static SysSettingsSoundPage g_SysSettingsSoundPage;
#if OPTION_ENCODER
static SysSettingsEncoderPage g_SysSettingsEncoderPage;
#endif
static SysSettingsTrackingPage g_sysSettingsTrackingPage;
static SysSettingsCouplingPage g_sysSettingsCouplingPage;
static UserProfilesPage g_UserProfilesPage;
static file_manager::FileBrowserPage g_FileBrowserPage;
static SysSettingsRampAndDelayPage g_sysSettingsRampAndDelayPage;

////////////////////////////////////////////////////////////////////////////////

Page *getPageFromIdHook(int pageId) {
    Page *page = nullptr;

    switch (pageId) {
    case PAGE_ID_EVENT_QUEUE:
        page = &g_EventQueuePage;
        break;
    case PAGE_ID_CH_SETTINGS_PROT_OVP:
        page = &g_ChSettingsOvpProtectionPage;
        break;
    case PAGE_ID_CH_SETTINGS_PROT_OCP:
        page = &g_ChSettingsOcpProtectionPage;
        break;
    case PAGE_ID_CH_SETTINGS_PROT_OPP:
        page = &g_ChSettingsOppProtectionPage;
        break;
    case PAGE_ID_CH_SETTINGS_PROT_OTP:
        page = &g_ChSettingsOtpProtectionPage;
        break;
    case PAGE_ID_CH_SETTINGS_ADV_OPTIONS:
        page = &g_ChSettingsAdvOptionPage;
        break;
    case PAGE_ID_CH_SETTINGS_ADV_RANGES:
        page = &g_ChSettingsAdvRangesPage;
        break;
    case PAGE_ID_SYS_SETTINGS_TRACKING:
        page = &g_sysSettingsTrackingPage;
        break;
    case PAGE_ID_SYS_SETTINGS_COUPLING:
        page = &g_sysSettingsCouplingPage;
        break;
    case PAGE_ID_CH_SETTINGS_ADV_VIEW:
        page = &g_ChSettingsAdvViewPage;
        break;
    case PAGE_ID_CH_SETTINGS_TRIGGER:
        page = &g_ChSettingsTriggerPage;
        break;
    case PAGE_ID_CH_SETTINGS_LISTS:
        page = &g_ChSettingsListsPage;
        break;
    case PAGE_ID_CH_SETTINGS_CALIBRATION_EDIT:
        page = &g_ChSettingsCalibrationEditPage;
        break;
    case PAGE_ID_CH_SETTINGS_CALIBRATION_VIEW:
        page = &g_ChSettingsCalibrationViewPage;
        break;
    case PAGE_ID_SYS_SETTINGS_DATE_TIME:
        page = &g_SysSettingsDateTimePage;
        break;
#if OPTION_ETHERNET
    case PAGE_ID_SYS_SETTINGS_ETHERNET:
        page = &g_SysSettingsEthernetPage;
        break;
    case PAGE_ID_SYS_SETTINGS_ETHERNET_STATIC:
        page = &g_SysSettingsEthernetStaticPage;
        break;
    case PAGE_ID_SYS_SETTINGS_MQTT:
        page = &g_SysSettingsMqttPage;
        break;
#endif
    case PAGE_ID_SYS_SETTINGS_PROTECTIONS:
        page = &g_SysSettingsProtectionsPage;
        break;
    case PAGE_ID_SYS_SETTINGS_TRIGGER:
        page = &g_SysSettingsTriggerPage;
        break;
    case PAGE_ID_SYS_SETTINGS_IO:
        page = &g_SysSettingsIOPinsPage;
        break;
    case PAGE_ID_SYS_SETTINGS_TEMPERATURE:
        page = &g_SysSettingsTemperaturePage;
        break;
    case PAGE_ID_SYS_SETTINGS_SOUND:
        page = &g_SysSettingsSoundPage;
        break;
#if OPTION_ENCODER
    case PAGE_ID_SYS_SETTINGS_ENCODER:
        page = &g_SysSettingsEncoderPage;
        break;
#endif
    case PAGE_ID_USER_PROFILES:
    case PAGE_ID_USER_PROFILE_0_SETTINGS:
    case PAGE_ID_USER_PROFILE_SETTINGS:
        page = &g_UserProfilesPage;
        break;
    case PAGE_ID_FILE_BROWSER:
        page = &g_FileBrowserPage;
        break;
    case PAGE_ID_SYS_SETTINGS_RAMP_AND_DELAY:
        page = &g_sysSettingsRampAndDelayPage;
        break;
    default :
        for (int i = 0; i < NUM_SLOTS; i++) {
            page = g_slots[i]->getPageFromId(pageId);
            if (page) {
                break;
            }
        }
        break;
    }

    if (page) {
        page->pageAlloc();
    }

    return page;
}

void action_internal_select_enum_item() {
    g_selectFromEnumPage.selectEnumItem();
}

// from InternalActionsEnum
static ActionExecFunc g_internalActionExecFunctions[] = {
    0,
    // ACTION_ID_INTERNAL_SELECT_ENUM_ITEM
    action_internal_select_enum_item,

    // ACTION_ID_INTERNAL_DIALOG_CLOSE
    popPage,

    // ACTION_ID_INTERNAL_TOAST_ACTION
    ToastMessagePage::executeAction,

    // ACTION_ID_INTERNAL_TOAST_ACTION_WITHOUT_PARAM
    ToastMessagePage::executeActionWithoutParam,

    // ACTION_ID_INTERNAL_MENU_WITH_BUTTONS
    MenuWithButtonsPage::executeAction
};

void executeInternalActionHook(int actionId) {
    g_internalActionExecFunctions[actionId - FIRST_INTERNAL_ACTION_ID]();
}

uint16_t overrideStyleHook(const WidgetCursor &widgetCursor, uint16_t styleId) {
    if (widgetCursor.widget->data == DATA_ID_CHANNEL_DISPLAY_VALUE1 || widgetCursor.widget->data == DATA_ID_CHANNEL_DISPLAY_VALUE2) {
        if (styleId == STYLE_ID_YT_GRAPH_U_DEFAULT || styleId == STYLE_ID_YT_GRAPH_I_DEFAULT) {
            using namespace psu;
            using namespace psu::gui;
            int iChannel = widgetCursor.cursor >= 0 ? widgetCursor.cursor : (g_channel ? g_channel->channelIndex : 0);
            Channel &channel = Channel::get(iChannel);
            if (widgetCursor.widget->data == DATA_ID_CHANNEL_DISPLAY_VALUE1) {
                if (channel.flags.displayValue1 == DISPLAY_VALUE_VOLTAGE) {
                    return STYLE_ID_YT_GRAPH_U_DEFAULT;
                } else if (channel.flags.displayValue1 == DISPLAY_VALUE_CURRENT) {
                    return STYLE_ID_YT_GRAPH_I_DEFAULT;
                }
            } else {
                if (channel.flags.displayValue2 == DISPLAY_VALUE_VOLTAGE) {
                    return STYLE_ID_YT_GRAPH_U_DEFAULT;
                } else if (channel.flags.displayValue2 == DISPLAY_VALUE_CURRENT) {
                    return STYLE_ID_YT_GRAPH_I_DEFAULT;
                }
            }
            return STYLE_ID_YT_GRAPH_P_DEFAULT;
        } else if (styleId == STYLE_ID_BAR_GRAPH_U_DEFAULT || styleId == STYLE_ID_BAR_GRAPH_I_DEFAULT) {
            using namespace psu;
            using namespace psu::gui;
            int iChannel = widgetCursor.cursor >= 0 ? widgetCursor.cursor : (g_channel ? g_channel->channelIndex : 0);
            Channel &channel = Channel::get(iChannel);
            if (widgetCursor.widget->data == DATA_ID_CHANNEL_DISPLAY_VALUE1) {
                if (channel.flags.displayValue1 == DISPLAY_VALUE_VOLTAGE) {
                    return STYLE_ID_BAR_GRAPH_U_DEFAULT;
                } else if (channel.flags.displayValue1 == DISPLAY_VALUE_CURRENT) {
                    return STYLE_ID_BAR_GRAPH_I_DEFAULT;
                }
            } else {
                if (channel.flags.displayValue2 == DISPLAY_VALUE_VOLTAGE) {
                    return STYLE_ID_BAR_GRAPH_U_DEFAULT;
                } else if (channel.flags.displayValue2 == DISPLAY_VALUE_CURRENT) {
                    return STYLE_ID_BAR_GRAPH_I_DEFAULT;
                }
            }
            return STYLE_ID_BAR_GRAPH_P_DEFAULT;
        }
    } else if (!g_psuAppContext.isWidgetActionEnabled(widgetCursor)) {
        if (styleId == STYLE_ID_ENCODER_CURSOR_14_ENABLED) {
            return STYLE_ID_ENCODER_CURSOR_14_DISABLED;
        } else if (styleId == STYLE_ID_ENCODER_CURSOR_14_RIGHT_ENABLED) {
            return STYLE_ID_ENCODER_CURSOR_14_RIGHT_DISABLED;
        }
    }
    return styleId;
}

uint16_t overrideStyleColorHook(const WidgetCursor &widgetCursor, const Style *style) {
    if (widgetCursor.widget->type == WIDGET_TYPE_TEXT && widgetCursor.widget->data == DATA_ID_DLOG_VISIBLE_VALUE_LABEL) {
        auto &recording = psu::dlog_view::getRecording();
        int dlogValueIndex = psu::dlog_view::getDlogValueIndex(recording,
            !psu::dlog_view::isMulipleValuesOverlayHeuristic(recording) || psu::persist_conf::devConf.viewFlags.dlogViewLegendViewOption == psu::persist_conf::DLOG_VIEW_LEGEND_VIEW_OPTION_DOCK
            ? recording.selectedVisibleValueIndex : widgetCursor.cursor);
        style = ytDataGetStyle(widgetCursor.cursor, DATA_ID_RECORDING, dlogValueIndex);
    }
    return style->color;
}

uint16_t overrideActiveStyleColorHook(const WidgetCursor &widgetCursor, const Style *style) {
    if (widgetCursor.widget->type == WIDGET_TYPE_TEXT && widgetCursor.widget->data == DATA_ID_DLOG_VISIBLE_VALUE_LABEL) {
        auto &recording = psu::dlog_view::getRecording();
        int dlogValueIndex = psu::dlog_view::getDlogValueIndex(recording,
            !psu::dlog_view::isMulipleValuesOverlayHeuristic(recording) || psu::persist_conf::devConf.viewFlags.dlogViewLegendViewOption == psu::persist_conf::DLOG_VIEW_LEGEND_VIEW_OPTION_DOCK
            ? recording.selectedVisibleValueIndex : widgetCursor.cursor);
        style = ytDataGetStyle(widgetCursor.cursor, DATA_ID_RECORDING, dlogValueIndex);
    }
    return style->active_color;
}

int16_t getAppContextId(AppContext *pAppContext) {
#if defined(EEZ_PLATFORM_SIMULATOR)
    if (pAppContext == &g_frontPanelAppContext) {
        return 2;
    }
#endif
    return 1;
}

AppContext *getAppContextFromId(int16_t id) {
#if defined(EEZ_PLATFORM_SIMULATOR)
    if (id == 2) {
        return &g_frontPanelAppContext;
    }
#endif
    return &g_psuAppContext;
}

void onGuiQueueMessageHook(uint8_t type, int16_t param) {
    if (type == GUI_QUEUE_MESSAGE_TYPE_LISTS_PAGE_IMPORT_LIST_FINISHED) {
        g_ChSettingsListsPage.onImportListFinished(param);
    } else if (type == GUI_QUEUE_MESSAGE_TYPE_LISTS_PAGE_EXPORT_LIST_FINISHED) {
        g_ChSettingsListsPage.onExportListFinished(param);
    } else if (type == GUI_QUEUE_MESSAGE_TYPE_USER_PROFILES_PAGE_ASYNC_OPERATION_FINISHED) {
        g_UserProfilesPage.onAsyncOperationFinished(param);
    } else if (type == GUI_QUEUE_MESSAGE_TYPE_SHOW_TEXT_INPUT) {
        g_psuAppContext.doShowTextInput();
    } else if (type == GUI_QUEUE_MESSAGE_TYPE_SHOW_NUMBER_INPUT) {
        g_psuAppContext.doShowNumberInput();
    } else if (type == GUI_QUEUE_MESSAGE_TYPE_SHOW_INTEGER_INPUT) {
        g_psuAppContext.doShowIntegerInput();
    } else if (type == GUI_QUEUE_MESSAGE_TYPE_SHOW_MENU_INPUT) {
        g_psuAppContext.doShowMenuInput();
    } else if (type == GUI_QUEUE_MESSAGE_TYPE_SHOW_SELECT) {
        g_psuAppContext.doShowSelect();
    } else if (type == GUI_QUEUE_MESSAGE_TYPE_DIALOG_OPEN) {
        g_psuAppContext.dialogOpen(nullptr);
    } else if (type == GUI_QUEUE_MESSAGE_TYPE_DIALOG_CLOSE) {
        g_psuAppContext.dialogClose();
    } else if (type == GUI_QUEUE_MESSAGE_TYPE_SHOW_ASYNC_OPERATION_IN_PROGRESS) {
        g_psuAppContext.doShowAsyncOperationInProgress();
    } else if (type == GUI_QUEUE_MESSAGE_TYPE_HIDE_ASYNC_OPERATION_IN_PROGRESS) {
        g_psuAppContext.doHideAsyncOperationInProgress();
    } else if (type == GUI_QUEUE_MESSAGE_KEY_DOWN) {
        keyboard::onKeyDown((uint16_t)param);
    } 
}

float getDefaultAnimationDurationHook() {
    return psu::persist_conf::devConf.animationsDuration;
}

void executeExternalActionHook(int32_t actionId) {
    g_externalActionId = actionId;
}

void externalDataHook(int16_t dataId, DataOperationEnum operation, Cursor cursor, Value &value) {
    if (dataId < 0) {
        dataId = -dataId;
    }
    dataId--;
    if ((uint16_t)dataId < MAX_NUM_EXTERNAL_DATA_ITEM_VALUES) {
        if (operation == DATA_OPERATION_GET) {
            value = g_externalDataItemValues[dataId].value;
        }
    }
}

bool activePageHasBackdropHook() {
    if (getActivePageId() == INTERNAL_PAGE_ID_TOAST_MESSAGE || getActivePageId() == PAGE_ID_ASYNC_OPERATION_IN_PROGRESS) {
        return false;
    }
    return true;
}

} // namespace gui
} // namespace eez

////////////////////////////////////////////////////////////////////////////////

namespace eez {
namespace mp {

void onUncaughtScriptExceptionHook() {
    g_psuAppContext.dialogClose();
    g_psuAppContext.showUncaughtScriptExceptionMessage();
}

}
}

#endif
