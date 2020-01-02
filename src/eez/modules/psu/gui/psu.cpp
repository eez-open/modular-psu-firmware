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

#include <eez/modules/psu/psu.h>

#include <eez/modules/psu/calibration.h>
#include <eez/modules/psu/channel_dispatcher.h>
#include <eez/modules/psu/devices.h>
#include <eez/modules/psu/event_queue.h>
#include <eez/modules/psu/gui/animations.h>
#include <eez/modules/psu/gui/data.h>
#include <eez/modules/psu/gui/edit_mode.h>
#include <eez/modules/psu/gui/edit_mode_keypad.h>
#include <eez/modules/psu/gui/edit_mode_slider.h>
#include <eez/modules/psu/gui/edit_mode_step.h>
#include <eez/modules/psu/gui/numeric_keypad.h>
#include <eez/modules/psu/gui/page_ch_settings_adv.h>
#include <eez/modules/psu/gui/page_ch_settings_protection.h>
#include <eez/modules/psu/gui/page_ch_settings_trigger.h>
#include <eez/modules/psu/gui/page_event_queue.h>
#include <eez/modules/psu/gui/page_self_test_result.h>
#include <eez/modules/psu/gui/page_sys_settings.h>
#include <eez/modules/psu/gui/page_user_profiles.h>
#include <eez/modules/psu/gui/password.h>
#include <eez/modules/psu/gui/psu.h>
#include <eez/modules/psu/gui/file_manager.h>
#include <eez/modules/psu/idle.h>
#include <eez/modules/psu/temperature.h>
#include <eez/modules/psu/trigger.h>
#include <eez/modules/psu/dlog_view.h>
#include <eez/gui/dialogs.h>
#include <eez/gui/document.h>
#include <eez/gui/gui.h>
#include <eez/gui/touch.h>
#include <eez/modules/mcu/display.h>
#include <eez/sound.h>
#include <eez/system.h>

#if OPTION_ENCODER
#include <eez/modules/mcu/encoder.h>
#endif

#if OPTION_SD_CARD
#include <eez/modules/psu/dlog_record.h>
#endif

#if EEZ_PLATFORM_STM32
#include <eez/modules/mcu/button.h>
#endif

#if defined(EEZ_PLATFORM_SIMULATOR)
#include <eez/platform/simulator/front_panel.h>
#endif

#include <eez/modules/psu/gui/touch_calibration.h>

#include <eez/modules/psu/dlog_view.h>

#define CONF_GUI_ENTERING_STANDBY_PAGE_TIMEOUT 2000000L // 2s
#define CONF_GUI_STANDBY_PAGE_TIMEOUT 4000000L          // 4s
#define CONF_GUI_DISPLAY_OFF_PAGE_TIMEOUT 2000000L      // 2s
#define CONF_GUI_WELCOME_PAGE_TIMEOUT 2000000L          // 2s
#define CONF_GUI_KEEP_WELCOME_PAGE_AFTER_BOOT 100000L   // 0.1s

namespace eez {

namespace mp {

void onUncaughtScriptExceptionHook() {
    eez::psu::gui::g_psuAppContext.showUncaughtScriptExceptionMessage();
}

}

namespace gui {

#if EEZ_PLATFORM_STM32
AppContext &getRootAppContext() {
    return psu::gui::g_psuAppContext;
}
#endif

void stateManagmentHook() {
    AppContext *saved = g_appContext;

#if defined(EEZ_PLATFORM_SIMULATOR)
    g_appContext = &g_frontPanelAppContext;
    g_frontPanelAppContext.stateManagment();
#endif

    g_appContext = &psu::gui::g_psuAppContext;
    psu::gui::g_psuAppContext.stateManagment();

    g_appContext = saved;
}

}

namespace psu {

namespace gui {

PsuAppContext g_psuAppContext;

static unsigned g_skipChannelCalibrations;
static unsigned g_skipDateTimeSetup;
static unsigned g_skipSerialSetup;
static unsigned g_skipEthernetSetup;

static bool g_showSetupWizardQuestionCalled;
Channel *g_channel;
int g_channelIndex;
static WidgetCursor g_toggleOutputWidgetCursor;

#if EEZ_PLATFORM_STM32
static mcu::Button g_userSwitch(USER_SW_GPIO_Port, USER_SW_Pin, true, true);
#endif

bool showSetupWizardQuestion();
void onEncoder(int counter, bool clicked);


////////////////////////////////////////////////////////////////////////////////

PsuAppContext::PsuAppContext() {
    m_pushProgressPage = false;
    m_popProgressPage = false;
}

void PsuAppContext::stateManagment() {
    AppContext::stateManagment();

    uint32_t tickCount = micros();

    // wait some time for transitional pages
    int activePageId = getActivePageId();
    if (activePageId == PAGE_ID_STANDBY) {
        if (int32_t(tickCount - getShowPageTime()) < CONF_GUI_STANDBY_PAGE_TIMEOUT) {
            return;
        }
    } else if (activePageId == PAGE_ID_ENTERING_STANDBY) {
        if (int32_t(tickCount - getShowPageTime()) >= CONF_GUI_ENTERING_STANDBY_PAGE_TIMEOUT) {
            uint32_t saved_showPageTime = getShowPageTime();
            showStandbyPage();
            setShowPageTime(saved_showPageTime);
        }
        return;
    } else if (activePageId == PAGE_ID_WELCOME) {
        if (!psu::g_isBooted || int32_t(tickCount - psu::g_isBootedTime) < CONF_GUI_KEEP_WELCOME_PAGE_AFTER_BOOT || int32_t(tickCount - getShowPageTime()) < CONF_GUI_WELCOME_PAGE_TIMEOUT) {
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
    	if (psu::g_isBooted && getActivePageId() != INTERNAL_PAGE_ID_NONE) {
    		showPage(INTERNAL_PAGE_ID_NONE);
    		eez::mcu::display::turnOff();
    	}
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

    // turn display on/off depending on displayState
    if (psu::persist_conf::devConf.displayState == 0 &&
        (activePageId != PAGE_ID_DISPLAY_OFF && activePageId != PAGE_ID_SELF_TEST_RESULT &&
         isTouchCalibrated())) {
        showPage(PAGE_ID_DISPLAY_OFF);
        return;
    } else if (psu::persist_conf::devConf.displayState == 1 &&
               activePageId == PAGE_ID_DISPLAY_OFF) {
        eez::mcu::display::turnOn();
        showPage(getMainPageId());
        return;
    }

    // start touch screen calibration automatically after period of time
    uint32_t inactivityPeriod = psu::idle::getGuiAndEncoderInactivityPeriod();
    if (activePageId == PAGE_ID_TOUCH_CALIBRATION_INTRO) {
        if (inactivityPeriod >= 20 * 1000UL) {
            enterTouchCalibration();
            return;
        }
    }

    // handling of display off page
    if (activePageId == PAGE_ID_DISPLAY_OFF) {
        if (eez::mcu::display::isOn()) {
            if (int32_t(tickCount - getShowPageTime()) >= CONF_GUI_DISPLAY_OFF_PAGE_TIMEOUT) {
                eez::mcu::display::turnOff();
                setShowPageTime(tickCount);
            }
        }
        return;
    }

    // TODO move this to some other place
#if OPTION_ENCODER
    int counter;
    bool clicked;
    mcu::encoder::read(counter, clicked);
    if (counter != 0 || clicked) {
        idle::noteEncoderActivity();
    }
    onEncoder(counter, clicked);
#endif

#if GUI_BACK_TO_MAIN_ENABLED
    uint32_t inactivityPeriod = psu::idle::getGuiAndEncoderInactivityPeriod();

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
        errorMessage("Max. remote prog. voltage exceeded.", "Please remove it immediately!");
    }

    // show startup wizard
    if (!isFrontPanelLocked() && activePageId == PAGE_ID_MAIN && int32_t(micros() - getShowPageTime()) >= 50000L) {
        if (showSetupWizardQuestion()) {
            return;
        }
    }

    if (m_pushProgressPage) {
        data::set(data::Cursor(), DATA_ID_ALERT_MESSAGE, data::Value(m_progressMessage), 0);
        g_appContext->m_dialogCancelCallback = m_progressAbortCallback;
        pushPage(PAGE_ID_PROGRESS);
        m_pushProgressPage = false;
    }

    if (m_popProgressPage) {
        if (getActivePageId() == PAGE_ID_PROGRESS) {
            popPage();
        }
        m_popProgressPage = false;
    }

    if (m_showTextMessage) {
        m_showTextMessage = false;
        if (getActivePageId() != PAGE_ID_TEXT_MESSAGE) {
            pushPage(PAGE_ID_TEXT_MESSAGE);
        } else {
            ++m_textMessageVersion;
        }
    } else if (m_clearTextMessage) {
        m_clearTextMessage = false;
        if (getActivePageId() == PAGE_ID_TEXT_MESSAGE) {
            popPage();
            m_textMessage[0] = 0;
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
        errorMessageWithAction("Uncaught script exception!", action_show_debug_trace_log, "Show debug trace log");
    }

    if (m_showTextInputOnNextIter) {
        m_showTextInputOnNextIter = false;
        Keypad::startPush(m_inputLabel, m_textInput, m_textInputMinChars, m_textInputMaxChars, false, onSetTextInputResult, onCancelTextInput);

    }

    if (m_showNumberInputOnNextIter) {
        m_showNumberInputOnNextIter = false;
        NumericKeypad::start(m_inputLabel, Value(m_numberInput, m_numberInputOptions.editValueUnit), m_numberInputOptions, onSetNumberInputResult, nullptr, onCancelNumberInput);
    }

    if (m_showMenuInputOnNextIter) {
        m_showMenuInputOnNextIter = false;
        showMenu(this, m_inputLabel, m_menuType, m_menuItems, onSetMenuInputResult);
    }
}

bool PsuAppContext::isActiveWidget(const WidgetCursor &widgetCursor) {
    if (getActivePageId() == PAGE_ID_TOUCH_CALIBRATION) {
        if (touch::g_eventType != EVENT_TYPE_TOUCH_NONE) {
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
        pageId == PAGE_ID_SYS_SETTINGS_SERIAL ||
        pageId == PAGE_ID_SYS_SETTINGS_ETHERNET ||
        pageId == PAGE_ID_SYS_SETTINGS_CAL ||
        pageId == PAGE_ID_SYS_SETTINGS_TRIGGER ||
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

    g_focusEditValue = data::Value();

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
    } else if (previousPageId == PAGE_ID_SYS_SETTINGS_TRIGGER) {
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
        } else if (activePageId == PAGE_ID_SYS_SETTINGS_TRIGGER) {
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
    if (calibration::isEnabled()) {
        return false;
    }

    if (isPageOnStack(PAGE_ID_CH_SETTINGS_LISTS)) {
        return ((ChSettingsListsPage *)getPage(PAGE_ID_CH_SETTINGS_LISTS))->isFocusWidget(widgetCursor);
    }

    if (getActivePageId() != PAGE_ID_DLOG_VIEW) {
        // TODO this is not valid, how can we know cursor.i is channels index and not index of some other collection?
        int iChannel = widgetCursor.cursor.i >= 0 ? widgetCursor.cursor.i : (g_channel ? g_channel->channelIndex : 0);
        if (iChannel >= 0 && iChannel < CH_NUM) {
            if (channel_dispatcher::getVoltageTriggerMode(Channel::get(iChannel)) != TRIGGER_MODE_FIXED &&
                !trigger::isIdle()) {
                return false;
            }
        }
    }

    return (widgetCursor.cursor == -1 || widgetCursor.cursor == g_focusCursor) && widgetCursor.widget->data == g_focusDataId;
}

bool PsuAppContext::isAutoRepeatAction(int action) {
    return action == ACTION_ID_KEYPAD_BACK || 
        action == ACTION_ID_EVENT_QUEUE_PREVIOUS_PAGE || action == ACTION_ID_EVENT_QUEUE_NEXT_PAGE || 
        action == ACTION_ID_CHANNEL_LISTS_PREVIOUS_PAGE || action == ACTION_ID_CHANNEL_LISTS_NEXT_PAGE;
}

void PsuAppContext::onPageTouch(const WidgetCursor &foundWidget, Event &touchEvent) {
#ifdef DEBUG
    if (touchEvent.type == EVENT_TYPE_TOUCH_MOVE &&
        (getActivePageId() == PAGE_ID_TOUCH_CALIBRATION_YES_NO ||
         getActivePageId() == PAGE_ID_TOUCH_CALIBRATION_YES_NO_CANCEL)) {
        int x = touchEvent.x;
        if (x < 1)
            x = 1;
        else if (x > eez::mcu::display::getDisplayWidth() - 2)
            x = eez::mcu::display::getDisplayWidth() - 2;

        int y = touchEvent.y;
        if (y < 1)
            y = 1;
        else if (y > eez::mcu::display::getDisplayHeight() - 2)
            y = eez::mcu::display::getDisplayHeight() - 2;
        
#if OPTION_SDRAM
        mcu::display::selectBuffer(m_pageNavigationStack[m_pageNavigationStackPointer].displayBufferIndex);
#endif
        eez::mcu::display::setColor(255, 255, 255);
        eez::mcu::display::fillRect(x - 1, y - 1, x + 1, y + 1);
    }
#endif

    if (getActivePageId() == PAGE_ID_TOUCH_CALIBRATION_INTRO) {
        if (touchEvent.type == EVENT_TYPE_TOUCH_UP) {
            enterTouchCalibration();
        }
    } else if (getActivePageId() == PAGE_ID_TOUCH_CALIBRATION) {
        onTouchCalibrationPageTouch(foundWidget, touchEvent);
    }
    
    int activePageId = getActivePageId();

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
        }
    }
}

bool PsuAppContext::testExecuteActionOnTouchDown(int action) {
    return action == ACTION_ID_CHANNEL_TOGGLE_OUTPUT || isAutoRepeatAction(action);
}

bool PsuAppContext::isBlinking(const data::Cursor &cursor, uint16_t id) {
    if (g_focusCursor == cursor && g_focusDataId == id && g_focusEditValue.getType() != VALUE_TYPE_NONE) {
        return true;
    }

    return AppContext::isBlinking(cursor, id);
}

uint32_t PsuAppContext::getNumHistoryValues(uint16_t id) {
    return CHANNEL_HISTORY_SIZE;
}

uint32_t PsuAppContext::getCurrentHistoryValuePosition(const Cursor &cursor, uint16_t id) {
    int iChannel = cursor.i >= 0 ? cursor.i : (g_channel ? g_channel->channelIndex : 0);
    return Channel::get(iChannel).getCurrentHistoryValuePosition();
}

void PsuAppContext::showProgressPage(const char *message, void (*abortCallback)()) {
    g_psuAppContext.m_progressMessage = message;
    g_psuAppContext.m_progressAbortCallback = abortCallback;
    g_psuAppContext.m_pushProgressPage = true;
}

bool PsuAppContext::updateProgressPage(size_t processedSoFar, size_t totalSize) {
    if (totalSize > 0) {
        g_progress = data::Value((int)round((processedSoFar * 1.0f / totalSize) * 100.0f), VALUE_TYPE_PERCENTAGE);
    } else {
        g_progress = data::Value((uint32_t)processedSoFar, VALUE_TYPE_SIZE);
    }

    if (g_psuAppContext.m_pushProgressPage) {
        return true;
    }

    return g_psuAppContext.isPageOnStack(PAGE_ID_PROGRESS);
}

void PsuAppContext::hideProgressPage() {
    g_psuAppContext.m_popProgressPage = true;
}

void PsuAppContext::setTextMessage(const char *message, unsigned int len) {
    strncpy(g_psuAppContext.m_textMessage, message, len);
    g_psuAppContext.m_textMessage[len] = 0;
    g_psuAppContext.m_showTextMessage = true;
}

void PsuAppContext::clearTextMessage() {
    g_psuAppContext.m_clearTextMessage =  true;
}

const char *PsuAppContext::getTextMessage() {
    return g_psuAppContext.m_textMessage;
}

uint8_t PsuAppContext::getTextMessageVersion() {
    return g_psuAppContext.m_textMessageVersion;
}

void PsuAppContext::showUncaughtScriptExceptionMessage() {
    m_showUncaughtScriptExceptionMessage = true;
}

void PsuAppContext::onSetTextInputResult(char *value) {
    g_psuAppContext.popPage();

    g_psuAppContext.m_textInput = value;
    g_psuAppContext.m_inputReady = true;
}

void PsuAppContext::onCancelTextInput() {
    g_psuAppContext.popPage();

    g_psuAppContext.m_textInput = nullptr;
    g_psuAppContext.m_inputReady = true;
}

const char *PsuAppContext::textInput(const char *label, size_t minChars, size_t maxChars, const char *value) {
    m_inputLabel = label;
    m_textInputMinChars = minChars;
    m_textInputMaxChars = maxChars;
    m_textInput = value;

    m_inputReady = false;
    m_showTextInputOnNextIter = true;

    while (!m_inputReady) {
        osDelay(1);
    }

    return m_textInput;
}

void PsuAppContext::onSetNumberInputResult(float value) {
    g_psuAppContext.popPage();

    g_psuAppContext.m_numberInput = value;
    g_psuAppContext.m_inputReady = true;
}

void PsuAppContext::onCancelNumberInput() {
    g_psuAppContext.popPage();

    g_psuAppContext.m_numberInput = NAN;
    g_psuAppContext.m_inputReady = true;
}

float PsuAppContext::numberInput(const char *label, Unit unit, float min, float max, float value) {
    m_inputLabel = label;

    m_numberInputOptions.editValueUnit = unit;
    m_numberInputOptions.min = min;
    m_numberInputOptions.enableMinButton();
    m_numberInputOptions.max = max;
    m_numberInputOptions.enableMaxButton();
    m_numberInputOptions.flags.signButtonEnabled = m_numberInputOptions.min < 0;
    m_numberInputOptions.flags.dotButtonEnabled = true;

    m_numberInput = value;

    m_inputReady = false;
    m_showNumberInputOnNextIter = true;

    while (!m_inputReady) {
        osDelay(1);
    }

    return m_numberInput;
}

void PsuAppContext::onSetMenuInputResult(int value) {
    g_psuAppContext.popPage();

    g_psuAppContext.m_menuInput = value;
    g_psuAppContext.m_inputReady = true;
}

int PsuAppContext::menuInput(const char *label, MenuType menuType, const char **menuItems) {
    m_inputLabel = label;

    m_menuType = menuType;
    m_menuItems = menuItems;

    m_inputReady = false;
    m_showMenuInputOnNextIter = true;

    while (!m_inputReady) {
        osDelay(1);
    }

    return m_menuInput;
}

////////////////////////////////////////////////////////////////////////////////

bool isChannelCalibrationsDone() {
    for (int i = 0; i < CH_NUM; ++i) {
        Channel &channel = Channel::get(i);
        if (channel.isInstalled() && channel.isOk() && !channel.isCalibrationExists()) {
            return false;
        }
    }
    return true;
}

bool isDateTimeSetupDone() {
    return persist_conf::devConf.dateValid && persist_conf::devConf.timeValid;
}

void channelCalibrationsYes() {
    executeAction(ACTION_ID_SHOW_SYS_SETTINGS_CAL);
}

void channelCalibrationsNo() {
    persist_conf::setSkipChannelCalibrations(1);
}

void dateTimeYes() {
    executeAction(ACTION_ID_SHOW_SYS_SETTINGS_DATE_TIME);
}

void dateTimeNo() {
    persist_conf::setSkipDateTimeSetup(1);
}

void serialYes() {
    executeAction(ACTION_ID_SHOW_SYS_SETTINGS_SERIAL);
}

void serialNo() {
    persist_conf::setSkipSerialSetup(1);
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
        g_skipSerialSetup = persist_conf::devConf.skipSerialSetup;
        g_skipEthernetSetup = persist_conf::devConf.skipEthernetSetup;
    }

    if (!g_skipChannelCalibrations) {
        g_skipChannelCalibrations = 1;
        if (!isChannelCalibrationsDone()) {
            yesNoLater("Do you want to calibrate channels?", channelCalibrationsYes,
                        channelCalibrationsNo);
            return true;
        }
    }

    if (!g_skipSerialSetup) {
        g_skipSerialSetup = 1;
        if (!persist_conf::isSerialEnabled()) {
            yesNoLater("Do you want to setup serial port?", serialYes, serialNo);
            return true;
        }
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

void changeValue(Channel &channel, const data::Value &value, float minValue, float maxValue, float defValue, void (*onSetValue)(float)) {
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

void psuErrorMessage(const data::Cursor &cursor, data::Value value, void (*ok_callback)()) {
    if (value.getType() == VALUE_TYPE_SCPI_ERROR) {
        int iChannel = cursor.i >= 0 ? cursor.i : (g_channel ? g_channel->channelIndex : 0);
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
        } else if (value.getScpiError() == SCPI_ERROR_POWER_LIMIT_EXCEEDED) {
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

data::Cursor g_focusCursor = Cursor(0);
uint16_t g_focusDataId = DATA_ID_CHANNEL_U_EDIT;
data::Value g_focusEditValue;

void setFocusCursor(const data::Cursor &cursor, uint16_t dataId) {
    g_focusCursor = cursor;
    g_focusDataId = dataId;
    g_focusEditValue = data::Value();
}

bool isFocusChanged() {
    return g_focusEditValue.getType() != VALUE_TYPE_NONE;
}

////////////////////////////////////////////////////////////////////////////////

#if OPTION_ENCODER

static bool g_isEncoderEnabledInActivePage;
uint32_t g_focusEditValueChangedTime;

bool isEncoderEnabledForWidget(const Widget *widget) {
    return widget->action == ACTION_ID_EDIT;
}

static bool g_focusCursorIsEnabled;

void isEnabledFocusCursorStep(const WidgetCursor &widgetCursor) {
    if (isEncoderEnabledForWidget(widgetCursor.widget)) {
        if (g_focusCursor == widgetCursor.cursor && g_focusDataId == widgetCursor.widget->data) {
            g_focusCursorIsEnabled = true;
        }
    }
}

bool isEnabledFocusCursor(data::Cursor &cursor, uint16_t dataId) {
    g_focusCursorIsEnabled = false;
    enumWidgets(isEnabledFocusCursorStep);
    return g_focusCursorIsEnabled;
}

void isEncoderEnabledInActivePageCheckWidget(const WidgetCursor &widgetCursor) {
    if (isEncoderEnabledForWidget(widgetCursor.widget)) {
        g_isEncoderEnabledInActivePage = true;
    }
}

bool isEncoderEnabledInActivePage() {
    // encoder is enabled if active page contains widget with "edit" action
    g_isEncoderEnabledInActivePage = false;
    enumWidgets(isEncoderEnabledInActivePageCheckWidget);
    return g_isEncoderEnabledInActivePage;
}

static int g_findNextFocusCursorState = 0; 
static data::Cursor g_nextFocusCursor = Cursor(0);
static uint16_t g_nextFocusDataId = DATA_ID_CHANNEL_U_EDIT;

void findNextFocusCursor(const WidgetCursor &widgetCursor) {
    if (isEncoderEnabledForWidget(widgetCursor.widget)) {
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

void moveToNextFocusCursor() {
    g_findNextFocusCursorState = 0;
    enumWidgets(findNextFocusCursor);
    if (g_findNextFocusCursorState > 0) {
        g_focusCursor = g_nextFocusCursor;
        g_focusDataId = g_nextFocusDataId;
    }
}

bool onEncoderConfirmation() {
    if (edit_mode::isActive() && !edit_mode::isInteractiveMode() &&
        edit_mode::getEditValue() != edit_mode::getCurrentValue()) {
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

    auto editValue = data::getEditValue(g_focusCursor, g_focusDataId);
    return editValue.getUnit();
}

void onEncoder(int counter, bool clicked) {
    if (isFrontPanelLocked()) {
        return;
    }

    uint32_t tickCount = micros();

    // wait for confirmation of changed value ...
    if (isFocusChanged() && tickCount - g_focusEditValueChangedTime >= ENCODER_CHANGE_TIMEOUT * 1000000L) {
        // ... on timeout discard changed value
        g_focusEditValue = data::Value();
    }

    if (!isEnabledFocusCursor(g_focusCursor, g_focusDataId)) {
        moveToNextFocusCursor();
    }

    Page *activePage = getActivePage();

    if (counter != 0) {
        if (!isEnabledFocusCursor(g_focusCursor, g_focusDataId)) {
            moveToNextFocusCursor();
        }

        if (isEncoderEnabledInActivePage()) {
            data::Value value;
            if (persist_conf::devConf.encoderConfirmationMode && g_focusEditValue.getType() != VALUE_TYPE_NONE) {
                value = g_focusEditValue;
            } else {
                value = data::getEditValue(g_focusCursor, g_focusDataId);
            }

            float min = data::getMin(g_focusCursor, g_focusDataId).getFloat();
            float max = data::getMax(g_focusCursor, g_focusDataId).getFloat();

            float newValue;

            Value stepValue = data::getEncoderStep(g_focusCursor, g_focusDataId);
            if (stepValue.getType() != VALUE_TYPE_NONE) {
                float step;
                if (mcu::encoder::g_encoderMode == mcu::encoder::ENCODER_MODE_AUTO) {
                    step = stepValue.getFloat();
                } else {
                    step = psu::gui::edit_mode_step::getCurrentEncoderStepValue().getFloat();
                }
                newValue = clamp(value.getFloat() + counter * step, min, max);

            } else {
                newValue = mcu::encoder::increment(value, counter, min, max, g_focusCursor.i, 0);
            }

            Value limitValue = data::getLimit(g_focusCursor, g_focusDataId);
            if (limitValue.getType() != VALUE_TYPE_NONE) {
                float limit = limitValue.getFloat();
                if (newValue > limit && value.getFloat() < limit) {
                    newValue = limit;
                }
            }

            if (persist_conf::devConf.encoderConfirmationMode) {
                g_focusEditValue = MakeValue(newValue, value.getUnit());
                g_focusEditValueChangedTime = micros();
            } else {
                int16_t error;
                if (!data::set(g_focusCursor, g_focusDataId, MakeValue(newValue, value.getUnit()), &error)) {
                    psuErrorMessage(g_focusCursor, data::MakeScpiErrorValue(error));
                }
            }
        }

        int activePageId = getActivePageId();

        if (activePageId == PAGE_ID_EDIT_MODE_KEYPAD || activePageId == PAGE_ID_NUMERIC_KEYPAD) {
            if (((NumericKeypad *)getActiveKeypad())->onEncoder(counter)) {
                return;
            }
        }

#if defined(EEZ_PLATFORM_SIMULATOR)
        if (activePageId == PAGE_ID_FRONT_PANEL_NUMERIC_KEYPAD) {
            if (((NumericKeypad *)getActiveKeypad())->onEncoder(counter)) {
                return;
            }
        }
#endif

        if (activePageId == PAGE_ID_EDIT_MODE_STEP) {
            edit_mode_step::onEncoder(counter);
            return;
        }

        if (activePageId == PAGE_ID_FILE_MANAGER) {
            file_manager::onEncoder(counter);
            return;
        }
         
        if (activePageId == PAGE_ID_DEBUG_TRACE_LOG) {
            eez::debug::onEncoder(counter);
            return;
        }

        if (activePage) {
            if (activePage->onEncoder(counter)) {
                return;
            }
        }
    }

    if (clicked) {
        if (isEncoderEnabledInActivePage()) {
            if (isFocusChanged()) {
                // confirmation
                int16_t error;
                if (!data::set(g_focusCursor, g_focusDataId, g_focusEditValue, &error)) {
                    psuErrorMessage(g_focusCursor, data::MakeScpiErrorValue(error));
                } else {
                    g_focusEditValue = data::Value();
                }
            } else if (!onEncoderConfirmation()) {
                moveToNextFocusCursor();
            }

            sound::playClick();
        }

        int activePageId = getActivePageId();
        if (activePageId == PAGE_ID_EDIT_MODE_KEYPAD || activePageId == PAGE_ID_NUMERIC_KEYPAD) {
            if (((NumericKeypad *)getActiveKeypad())->onEncoderClicked()) {
                return;
            }
        }

#if defined(EEZ_PLATFORM_SIMULATOR)
        if (activePageId == PAGE_ID_FRONT_PANEL_NUMERIC_KEYPAD) {
            if (((NumericKeypad *)getActiveKeypad())->onEncoderClicked()) {
                return;
            }
        }
#endif

        if (activePage) {
            if (activePage->onEncoderClicked()) {
                return;
            }
        }
    }
}

#endif

void channelReinitiateTrigger() {
    trigger::abort();
    channelInitiateTrigger();
}

void channelToggleOutput() {
    Channel &channel =
        Channel::get(getFoundWidgetAtDown().cursor.i >= 0 ? getFoundWidgetAtDown().cursor.i : 0);
    if (channel_dispatcher::isTripped(channel)) {
        errorMessage("Channel is tripped!");
    } else {
        bool triggerModeEnabled =
            channel_dispatcher::getVoltageTriggerMode(channel) != TRIGGER_MODE_FIXED ||
            channel_dispatcher::getCurrentTriggerMode(channel) != TRIGGER_MODE_FIXED;

        if (channel.isOutputEnabled()) {
            if (triggerModeEnabled) {
                trigger::abort();
            }

            channel_dispatcher::outputEnable(channel, false);
        } else {
            if (triggerModeEnabled) {
                if (trigger::isIdle()) {
                    g_toggleOutputWidgetCursor = getFoundWidgetAtDown();
                    pushPage(PAGE_ID_CH_START_LIST);
                } else if (trigger::isInitiated()) {
                    trigger::abort();
                } else {
                    yesNoDialog(PAGE_ID_YES_NO, "Trigger is active. Re-initiate trigger?",
                                channelReinitiateTrigger, 0, 0);
                }
            } else {
                channel_dispatcher::outputEnable(channel, true);
            }
        }
    }
}

void channelInitiateTrigger() {
    popPage();
    int err = trigger::initiate();
    if (err == SCPI_RES_OK) {
        if (!trigger::isInitiated()) {
            Channel &channel = Channel::get(g_toggleOutputWidgetCursor.cursor.i >= 0 ? g_toggleOutputWidgetCursor.cursor.i : 0);
            channel_dispatcher::outputEnable(channel, true);
        }
    } else {
        psuErrorMessage(g_toggleOutputWidgetCursor.cursor, data::MakeScpiErrorValue(err));
    }
}

void channelSetToFixed() {
    popPage();

    Channel &channel = Channel::get(
        g_toggleOutputWidgetCursor.cursor.i >= 0 ? g_toggleOutputWidgetCursor.cursor.i : 0);
    if (channel_dispatcher::getVoltageTriggerMode(channel) != TRIGGER_MODE_FIXED) {
        channel_dispatcher::setVoltageTriggerMode(channel, TRIGGER_MODE_FIXED);
    }
    if (channel_dispatcher::getCurrentTriggerMode(channel) != TRIGGER_MODE_FIXED) {
        channel_dispatcher::setCurrentTriggerMode(channel, TRIGGER_MODE_FIXED);
    }
    channel_dispatcher::outputEnable(channel, true);
}

void channelEnableOutput() {
    popPage();
    Channel &channel = Channel::get(
        g_toggleOutputWidgetCursor.cursor.i >= 0 ? g_toggleOutputWidgetCursor.cursor.i : 0);
    channel_dispatcher::outputEnable(channel, true);
}

void selectChannel(Channel *channel) {
    if (channel) {
        g_channel = channel;
    } else {
        if (getFoundWidgetAtDown().cursor.i >= 0) {
            g_channel = &Channel::get(getFoundWidgetAtDown().cursor.i);
        } else if (!g_channel) {
            g_channel = &Channel::get(0);
        }
    }

    if (g_channel) {
        g_channelIndex = g_channel->channelIndex;
    } else {
        g_channelIndex = -1;
    }
}

} // namespace gui
} // namespace psu

namespace mcu {
namespace display {

uint16_t transformColorHook(uint16_t color) {
    if (color == COLOR_ID_CHANNEL1 && eez::psu::gui::g_channelIndex >= 0 && eez::psu::gui::g_channelIndex < CH_MAX) {
        return COLOR_ID_CHANNEL1 + eez::psu::gui::g_channelIndex;
    }
    return color;
}

}
}

namespace gui {

uint16_t overrideStyleHook(const WidgetCursor &widgetCursor, uint16_t styleId) {
    if (widgetCursor.widget->data == DATA_ID_CHANNEL_DISPLAY_VALUE1 || widgetCursor.widget->data == DATA_ID_CHANNEL_DISPLAY_VALUE2) {
        if (styleId == STYLE_ID_YT_GRAPH_U_DEFAULT || styleId == STYLE_ID_YT_GRAPH_I_DEFAULT) {
            using namespace psu;
            using namespace psu::gui;
            int iChannel = widgetCursor.cursor.i >= 0 ? widgetCursor.cursor.i : (g_channel ? g_channel->channelIndex : 0);
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
        }
    }
    return styleId;
}



uint16_t overrideStyleColorHook(const WidgetCursor &widgetCursor, const Style *style) {
    if (widgetCursor.widget->type == WIDGET_TYPE_TEXT && (widgetCursor.widget->data == DATA_ID_DLOG_VALUE_LABEL || widgetCursor.widget->data == DATA_ID_DLOG_VISIBLE_VALUE_LABEL)) {
        psu::dlog_view::Recording &recording = psu::dlog_view::getRecording();
        int dlogValueIndex = psu::dlog_view::getDlogValueIndex(recording, !psu::dlog_view::yAxisHasDifferentUnits(recording) ? recording.selectedVisibleValueIndex : widgetCursor.cursor.i);
        style = ytDataGetStyle(widgetCursor.cursor, DATA_ID_RECORDING, dlogValueIndex);
    }
    return style->color;
}

uint16_t overrideActiveStyleColorHook(const WidgetCursor &widgetCursor, const Style *style) {
    if (widgetCursor.widget->type == WIDGET_TYPE_TEXT && (widgetCursor.widget->data == DATA_ID_DLOG_VALUE_LABEL || widgetCursor.widget->data == DATA_ID_DLOG_VISIBLE_VALUE_LABEL)) {
        psu::dlog_view::Recording &recording = psu::dlog_view::getRecording();
        int dlogValueIndex = psu::dlog_view::getDlogValueIndex(recording, !psu::dlog_view::yAxisHasDifferentUnits(recording) ? recording.selectedVisibleValueIndex : widgetCursor.cursor.i);
        style = ytDataGetStyle(widgetCursor.cursor, DATA_ID_RECORDING, dlogValueIndex);
    }
    return style->active_color;
}

}

} // namespace eez

#endif
