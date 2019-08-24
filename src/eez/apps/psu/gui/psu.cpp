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

#include <eez/apps/psu/psu.h>

#include <eez/apps/psu/calibration.h>
#include <eez/apps/psu/channel_dispatcher.h>
#include <eez/apps/psu/devices.h>
#include <eez/apps/psu/event_queue.h>
#include <eez/apps/psu/gui/animations.h>
#include <eez/apps/psu/gui/data.h>
#include <eez/apps/psu/gui/edit_mode.h>
#include <eez/apps/psu/gui/edit_mode_keypad.h>
#include <eez/apps/psu/gui/edit_mode_slider.h>
#include <eez/apps/psu/gui/edit_mode_step.h>
#include <eez/apps/psu/gui/numeric_keypad.h>
#include <eez/apps/psu/gui/page_ch_settings_adv.h>
#include <eez/apps/psu/gui/page_ch_settings_protection.h>
#include <eez/apps/psu/gui/page_ch_settings_trigger.h>
#include <eez/apps/psu/gui/page_event_queue.h>
#include <eez/apps/psu/gui/page_self_test_result.h>
#include <eez/apps/psu/gui/page_sys_settings.h>
#include <eez/apps/psu/gui/page_user_profiles.h>
#include <eez/apps/psu/gui/password.h>
#include <eez/apps/psu/gui/psu.h>
#include <eez/apps/psu/idle.h>
#include <eez/apps/psu/temperature.h>
#include <eez/apps/psu/trigger.h>
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
#include <eez/apps/psu/dlog.h>
#endif

#if EEZ_PLATFORM_STM32
#include <eez/modules/mcu/button.h>
#endif

#define CONF_DLOG_COLOR 62464

namespace eez {
namespace psu {
namespace gui {

PsuAppContext g_psuAppContext;

static persist_conf::DeviceFlags2 g_deviceFlags2;
static bool g_showSetupWizardQuestionCalled;
Channel *g_channel;
static WidgetCursor g_toggleOutputWidgetCursor;

#if EEZ_PLATFORM_STM32
static mcu::Button g_userSwitch(USER_SW_GPIO_Port, USER_SW_Pin);
#endif

bool showSetupWizardQuestion();
void onEncoder(int counter, bool clicked);

////////////////////////////////////////////////////////////////////////////////

PsuAppContext::PsuAppContext() {
    showPageOnNextIter(getMainPageId());
}

void PsuAppContext::stateManagment() {
    AppContext::stateManagment();

#if EEZ_PLATFORM_STM32
    if (g_userSwitch.isClicked()) {
        action_user_switch_clicked();
    }
#endif

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

    int activePageId = getActivePageId();

#if GUI_BACK_TO_MAIN_ENABLED
    uint32_t inactivityPeriod = psu::idle::getGuiAndEncoderInactivityPeriod();

    if (activePageId == PAGE_ID_EVENT_QUEUE || activePageId == PAGE_ID_USER_PROFILES ||
        activePageId == PAGE_ID_USER_PROFILES2 || activePageId == PAGE_ID_USER_PROFILE_0_SETTINGS ||
        activePageId == PAGE_ID_USER_PROFILE_SETTINGS) {
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
}

int PsuAppContext::getMainPageId() {
    return PAGE_ID_MAIN;
}

bool isSysSettingsSubPage(int pageId) {
    return pageId == PAGE_ID_SYS_SETTINGS_AUX_OTP ||
        pageId == PAGE_ID_SYS_SETTINGS_PROTECTIONS ||
        pageId == PAGE_ID_SYS_SETTINGS_IO ||
        pageId == PAGE_ID_SYS_SETTINGS_DATE_TIME ||
        pageId == PAGE_ID_SYS_SETTINGS_ENCODER ||
        pageId == PAGE_ID_SYS_SETTINGS_SERIAL ||
        pageId == PAGE_ID_SYS_SETTINGS_ETHERNET ||
        pageId == PAGE_ID_SYS_SETTINGS_CAL ||
        pageId == PAGE_ID_SYS_SETTINGS_TRIGGER ||
        pageId == PAGE_ID_SYS_SETTINGS_DISPLAY ||
        pageId == PAGE_ID_SYS_SETTINGS_SOUND;
}

bool isChSettingsSubPage(int pageId) {
    return pageId == PAGE_ID_CH_SETTINGS_PROT_CLEAR ||
        pageId == PAGE_ID_CH_SETTINGS_PROT_OVP ||
        pageId == PAGE_ID_CH_SETTINGS_PROT_OCP ||
        pageId == PAGE_ID_CH_SETTINGS_PROT_OPP ||
        pageId == PAGE_ID_CH_SETTINGS_PROT_OTP ||
        pageId == PAGE_ID_CH_SETTINGS_TRIGGER ||
        pageId == PAGE_ID_CH_SETTINGS_LISTS ||
        pageId == PAGE_ID_CH_SETTINGS_ADV_REMOTE ||
        pageId == PAGE_ID_CH_SETTINGS_ADV_RANGES ||
        pageId == PAGE_ID_CH_SETTINGS_ADV_TRACKING ||
        pageId == PAGE_ID_CH_SETTINGS_ADV_COUPLING ||
        pageId == PAGE_ID_CH_SETTINGS_ADV_COUPLING_INFO ||
        pageId == PAGE_ID_CH_SETTINGS_ADV_VIEW ||
        pageId == PAGE_ID_CH_SETTINGS_INFO;
}


void PsuAppContext::onPageChanged() {
    AppContext::onPageChanged();

    g_focusEditValue = data::Value();

    if (m_previousPageId == PAGE_ID_EVENT_QUEUE) {
        if (m_activePageId == PAGE_ID_MAIN) {
            animateSlideUp();
        }
    } else if (m_previousPageId == PAGE_ID_MAIN) {
        if (m_activePageId == PAGE_ID_SYS_SETTINGS) {
            animateShowSysSettings();
        } else if (isSysSettingsSubPage(m_activePageId)) {
            animateShowSysSettings();
        } else if (m_activePageId == PAGE_ID_CH_SETTINGS) {
            animateSlideDown();
        } else if (isChSettingsSubPage(m_activePageId)) {
            animateSlideDown();
        } else if (m_activePageId == PAGE_ID_EVENT_QUEUE) {
            animateSlideDown();
        } else if (m_activePageId == PAGE_ID_USER_PROFILES) {
            animateSlideDown();
        } else if (m_activePageId == PAGE_ID_SYS_INFO) {
            animateSlideDown();
        }
    } else if (m_previousPageId == PAGE_ID_USER_PROFILES) {
        if (m_activePageId == PAGE_ID_USER_PROFILE_0_SETTINGS) {
            animateSlideDown();
        } else if (m_activePageId == PAGE_ID_USER_PROFILE_SETTINGS) {
            animateSlideDown();
        } else if (m_activePageId == PAGE_ID_MAIN) {
            animateSlideUp();
        } else if (m_activePageId == PAGE_ID_USER_PROFILES2) {
            animateSlideLeft();
        }
    } else if (m_previousPageId == PAGE_ID_USER_PROFILES2) {
        if (m_activePageId == PAGE_ID_USER_PROFILE_SETTINGS) {
            animateSlideDown();
        } else if (m_activePageId == PAGE_ID_MAIN) {
            animateSlideUp();
        } else if (m_activePageId == PAGE_ID_USER_PROFILES) {
            animateSlideRight();
        }
    } else if (m_previousPageId == PAGE_ID_USER_PROFILE_0_SETTINGS) {
        if (m_activePageId == PAGE_ID_MAIN) {
            animateSlideUp();
        } else if (m_activePageId == PAGE_ID_USER_PROFILES) {
            animateSlideUp();
        }
    } else if (m_previousPageId == PAGE_ID_USER_PROFILE_SETTINGS) {
        if (m_activePageId == PAGE_ID_MAIN) {
            animateSlideUp();
        } else if (m_activePageId == PAGE_ID_USER_PROFILES) {
            animateSlideUp();
        } else if (m_activePageId == PAGE_ID_USER_PROFILES2) {
            animateSlideUp();
        }
    } else if (m_previousPageId == PAGE_ID_SYS_INFO) {
        if (m_activePageId == PAGE_ID_MAIN) {
            animateSlideUp();
        }
    } else if (m_previousPageId == PAGE_ID_SYS_SETTINGS) {
        if (m_activePageId == PAGE_ID_MAIN) {
            animateHideSysSettings();
        } else if (isSysSettingsSubPage(m_activePageId)) {
            animateSettingsSlideLeft();
        }
    } else if (m_previousPageId == PAGE_ID_SYS_SETTINGS_TRIGGER) {
        if (m_activePageId == PAGE_ID_MAIN) {
            animateHideSysSettings();
        } else if (m_activePageId == PAGE_ID_CH_SETTINGS_TRIGGER) {
            animateSlideUp();
        } else if (m_activePageId == PAGE_ID_SYS_SETTINGS) {
            animateSettingsSlideRight();
        }
    } else if (isSysSettingsSubPage(m_previousPageId)) {
        if (m_activePageId == PAGE_ID_MAIN) {
            animateHideSysSettings();
        } else if (m_activePageId == PAGE_ID_SYS_SETTINGS) {
            animateSettingsSlideRight();
        }
    } else if (m_previousPageId == PAGE_ID_CH_SETTINGS) {
        if (m_activePageId == PAGE_ID_MAIN) {
            animateSlideUp();
        } else if (isChSettingsSubPage(m_activePageId)) {
            animateSlideLeft();
        }
    } else if (m_previousPageId == PAGE_ID_CH_SETTINGS_TRIGGER) {
        if (m_activePageId == PAGE_ID_MAIN) {
            animateSlideUp();
        } else if (m_activePageId == PAGE_ID_SYS_SETTINGS_TRIGGER) {
            animateSlideDown();
        } else if (m_activePageId == PAGE_ID_CH_SETTINGS_LISTS) {
            animateSlideDown();
        } else if (m_activePageId == PAGE_ID_CH_SETTINGS) {
            animateSlideRight();
        }
    } else if (m_previousPageId == PAGE_ID_CH_SETTINGS_LISTS) {
        if (m_activePageId == PAGE_ID_CH_SETTINGS_TRIGGER) {
            animateSlideUp();
        }
    } else if (isChSettingsSubPage(m_previousPageId)) {
        if (m_activePageId == PAGE_ID_MAIN) {
            animateSlideUp();
        } else if (m_activePageId == PAGE_ID_CH_SETTINGS) {
            animateSlideRight();
        }
    }
}

bool PsuAppContext::isFocusWidget(const WidgetCursor &widgetCursor) {
    if (isPageActiveOrOnStack(PAGE_ID_CH_SETTINGS_LISTS)) {
        return ((ChSettingsListsPage *)getActivePage())->isFocusWidget(widgetCursor);
    }

    // TODO this is not valid, how can we know cursor.i is channels index and not index of some other collection?
    int iChannel = widgetCursor.cursor.i >= 0 ? widgetCursor.cursor.i
                                              : (g_channel ? (g_channel->index - 1) : 0);
    if (iChannel >= 0 && iChannel < CH_NUM) {
		if (channel_dispatcher::getVoltageTriggerMode(Channel::get(iChannel)) != TRIGGER_MODE_FIXED &&
			!trigger::isIdle()) {
			return false;
		}
    }

    if (calibration::isEnabled()) {
        return false;
    }

    return (widgetCursor.cursor == -1 || widgetCursor.cursor == g_focusCursor) && widgetCursor.widget->data == g_focusDataId;
}

bool PsuAppContext::isAutoRepeatAction(int action) {
    return action == ACTION_ID_KEYPAD_BACK || 
        action == ACTION_ID_EVENT_QUEUE_PREVIOUS_PAGE || action == ACTION_ID_EVENT_QUEUE_NEXT_PAGE || 
        action == ACTION_ID_CHANNEL_LISTS_PREVIOUS_PAGE || action == ACTION_ID_CHANNEL_LISTS_NEXT_PAGE;
}

void PsuAppContext::onPageTouch(const WidgetCursor &foundWidget, Event &touchEvent) {
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

uint16_t PsuAppContext::getWidgetBackgroundColor(const WidgetCursor &widgetCursor,
                                                 const Style *style) {
#if OPTION_SD_CARD
    const Widget *widget = widgetCursor.widget;
    int iChannel = widgetCursor.cursor.i >= 0 ? widgetCursor.cursor.i
                                              : (g_channel ? (g_channel->index - 1) : 0);
    if (widget->data == DATA_ID_CHANNEL_U_EDIT || widget->data == DATA_ID_CHANNEL_U_MON_DAC) {
        if (dlog::g_logVoltage[iChannel]) {
            return CONF_DLOG_COLOR;
        }
    } else if (widget->data == DATA_ID_CHANNEL_I_EDIT) {
        if (dlog::g_logCurrent[iChannel]) {
            return CONF_DLOG_COLOR;
        }
    } else if (widget->data == DATA_ID_CHANNEL_P_MON) {
        if (dlog::g_logPower[iChannel]) {
            return CONF_DLOG_COLOR;
        }
    }
#endif

    return AppContext::getWidgetBackgroundColor(widgetCursor, style);
}

bool PsuAppContext::isBlinking(const data::Cursor &cursor, uint16_t id) {
    if ((g_focusCursor == cursor || channel_dispatcher::isCoupled()) && g_focusDataId == id &&
        g_focusEditValue.getType() != VALUE_TYPE_NONE) {
        return true;
    }

    return AppContext::isBlinking(cursor, id);
}

void PsuAppContext::onScaleUpdated(int dataId, bool scaleIsVertical, int scaleWidth,
                                   float scaleHeight) {
    if (dataId == DATA_ID_EDIT_VALUE) {
        edit_mode_slider::scale_is_vertical = scaleIsVertical;
        edit_mode_slider::scale_width = scaleWidth;
        edit_mode_slider::scale_height = scaleHeight;
    }
}

uint32_t PsuAppContext::getNumHistoryValues(uint16_t id) {
    return CHANNEL_HISTORY_SIZE;
}

uint32_t PsuAppContext::getCurrentHistoryValuePosition(const Cursor &cursor, uint16_t id) {
    int iChannel = cursor.i >= 0 ? cursor.i : (g_channel ? (g_channel->index - 1) : 0);
    return Channel::get(iChannel).getCurrentHistoryValuePosition();
}

Value PsuAppContext::getHistoryValue(const Cursor &cursor, uint16_t id, uint32_t position) {
    Value value(position, VALUE_TYPE_INT);
    g_dataOperationsFunctions[id](data::DATA_OPERATION_GET_HISTORY_VALUE, (Cursor &)cursor, value);
    return value;
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
    return persist_conf::devConf.flags.dateValid && persist_conf::devConf.flags.timeValid;
}

void channelCalibrationsYes() {
    executeAction(ACTION_ID_SHOW_SYS_SETTINGS_CAL);
}

void channelCalibrationsNo() {
    persist_conf::devConf2.flags.skipChannelCalibrations = 1;
    persist_conf::saveDevice2();
}

void dateTimeYes() {
    executeAction(ACTION_ID_SHOW_SYS_SETTINGS_DATE_TIME);
}

void dateTimeNo() {
    persist_conf::devConf2.flags.skipDateTimeSetup = 1;
    persist_conf::saveDevice2();
}

void serialYes() {
    executeAction(ACTION_ID_SHOW_SYS_SETTINGS_SERIAL);
}

void serialNo() {
    persist_conf::devConf2.flags.skipSerialSetup = 1;
    persist_conf::saveDevice2();
}

void ethernetYes() {
    executeAction(ACTION_ID_SHOW_SYS_SETTINGS_ETHERNET);
}

void ethernetNo() {
    persist_conf::devConf2.flags.skipEthernetSetup = 1;
    persist_conf::saveDevice2();
}

bool showSetupWizardQuestion() {
    if (!g_showSetupWizardQuestionCalled) {
        g_showSetupWizardQuestionCalled = true;
        g_deviceFlags2 = persist_conf::devConf2.flags;
    }

    if (!channel_dispatcher::isCoupled() && !channel_dispatcher::isTracked()) {
        if (!g_deviceFlags2.skipChannelCalibrations) {
            g_deviceFlags2.skipChannelCalibrations = 1;
            if (!isChannelCalibrationsDone()) {
                yesNoLater("Do you want to calibrate channels?", channelCalibrationsYes,
                           channelCalibrationsNo);
                return true;
            }
        }
    }

    if (!g_deviceFlags2.skipSerialSetup) {
        g_deviceFlags2.skipSerialSetup = 1;
        if (!persist_conf::isSerialEnabled()) {
            yesNoLater("Do you want to setup serial port?", serialYes, serialNo);
            return true;
        }
    }

#if OPTION_ETHERNET
    if (!g_deviceFlags2.skipEthernetSetup) {
        g_deviceFlags2.skipEthernetSetup = 1;
        if (!persist_conf::isEthernetEnabled()) {
            yesNoLater("Do you want to setup ethernet?", ethernetYes, ethernetNo);
            return true;
        }
    }
#endif

    if (!g_deviceFlags2.skipDateTimeSetup) {
        g_deviceFlags2.skipDateTimeSetup = 1;
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

    options.channelIndex = channel.index;

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
    float minDelay = channel.OPP_MIN_DELAY;
    float maxDelay = channel.OPP_MAX_DELAY;
    float defaultDelay = channel.OPP_DEFAULT_DELAY;
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
        int iChannel = cursor.i >= 0 ? cursor.i : (g_channel ? (g_channel->index - 1) : 0);
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

    if (counter != 0) {
        if (!isEnabledFocusCursor(g_focusCursor, g_focusDataId)) {
            moveToNextFocusCursor();
        }

        mcu::encoder::enableAcceleration(true);

        if (isEncoderEnabledInActivePage()) {
            data::Value value;
            if (persist_conf::devConf2.flags.encoderConfirmationMode && g_focusEditValue.getType() != VALUE_TYPE_NONE) {
                value = g_focusEditValue;
            } else {
                value = data::getEditValue(g_focusCursor, g_focusDataId);
            }

            float oldValue = value.getFloat();

            float newValue;
            if (mcu::encoder::g_encoderMode == mcu::encoder::ENCODER_MODE_AUTO) {
                float factor = Channel::get(g_focusCursor.i).getValuePrecision(value.getUnit(), oldValue);
                newValue = oldValue + factor * counter;
            } else {
                newValue = oldValue + edit_mode_step::getCurrentEncoderStepValue().getFloat() * counter;
            }

            newValue = Channel::get(g_focusCursor.i).roundChannelValue(value.getUnit(), newValue);

            if (mcu::encoder::g_encoderMode == mcu::encoder::ENCODER_MODE_AUTO) {
                float diff = fabs(newValue - oldValue);
                if (diff > 1) {
                    newValue = roundPrec(newValue, 1);
                } else if (diff > 0.1) {
                    newValue = roundPrec(newValue, 0.1f);
                } else if (diff > 0.01) {
                    newValue = roundPrec(newValue, 0.01f);
                } else if (diff > 0.001) {
                    newValue = roundPrec(newValue, 0.001f);
                } else if (diff > 0.0001) {
                    newValue = roundPrec(newValue, 0.0001f);
                }
            }

            float min = data::getMin(g_focusCursor, g_focusDataId).getFloat();
            if (newValue < min) {
                newValue = min;
            }

            float limit = data::getLimit(g_focusCursor, g_focusDataId).getFloat();
            if (newValue > limit && oldValue < limit) {
                newValue = limit;
            }

            float max = data::getMax(g_focusCursor, g_focusDataId).getFloat();
            if (newValue > max) {
                newValue = max;
            }

            if (persist_conf::devConf2.flags.encoderConfirmationMode) {
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
        if (activePageId == PAGE_ID_NUMERIC_KEYPAD2) {
            if (((NumericKeypad *)getActiveKeypad())->onEncoder(counter)) {
                return;
            }
        }
#endif

        if (activePageId == PAGE_ID_EDIT_MODE_STEP) {
            edit_mode_step::onEncoder(counter);
            return;
        }

        mcu::encoder::enableAcceleration(true);

        if (activePageId == PAGE_ID_EDIT_MODE_SLIDER) {
            edit_mode_slider::increment(counter);
            return;
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
            ((NumericKeypad *)getActiveKeypad())->onEncoderClicked();
        }
#if defined(EEZ_PLATFORM_SIMULATOR)
        if (activePageId == PAGE_ID_NUMERIC_KEYPAD2) {
            ((NumericKeypad *)getActiveKeypad())->onEncoderClicked();
        }
#endif
    }

    Page *activePage = getActivePage();
    if (activePage) {
        if (counter) {
            activePage->onEncoder(counter);
        }

        if (clicked) {
            activePage->onEncoderClicked();
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
                for (int i = 0; i < CH_NUM; ++i) {
                    Channel &channel = Channel::get(i);
                    if (channel.isOk()) {
						if (channel_dispatcher::getVoltageTriggerMode(channel) != TRIGGER_MODE_FIXED ||
							channel_dispatcher::getCurrentTriggerMode(channel) != TRIGGER_MODE_FIXED) {
							channel_dispatcher::outputEnable(Channel::get(i), false);
						}
                    }
                }
            } else {
                channel_dispatcher::outputEnable(channel, false);
            }
        } else {
            if (triggerModeEnabled) {
                if (trigger::isIdle()) {
                    g_toggleOutputWidgetCursor = getFoundWidgetAtDown();
                    pushPage(PAGE_ID_CH_START_LIST);
                } else if (trigger::isInitiated()) {
                    trigger::abort();
                    g_toggleOutputWidgetCursor = getFoundWidgetAtDown();
                    pushPage(PAGE_ID_CH_START_LIST);
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
        Channel &channel = Channel::get(
            g_toggleOutputWidgetCursor.cursor.i >= 0 ? g_toggleOutputWidgetCursor.cursor.i : 0);
        channel_dispatcher::outputEnable(channel, true);
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

void selectChannel() {
    if (getFoundWidgetAtDown().cursor.i >= 0) {
        g_channel = &Channel::get(getFoundWidgetAtDown().cursor.i);
    } else if (!g_channel || channel_dispatcher::isCoupled() || channel_dispatcher::isTracked()) {
        g_channel = &Channel::get(0);
    }
}

static bool isChannelTripLastEvent(int i, event_queue::Event &lastEvent) {
    if (lastEvent.eventId == (event_queue::EVENT_ERROR_CH1_OVP_TRIPPED + i) ||
        lastEvent.eventId == (event_queue::EVENT_ERROR_CH1_OCP_TRIPPED + i) ||
        lastEvent.eventId == (event_queue::EVENT_ERROR_CH1_OPP_TRIPPED + i) ||
        lastEvent.eventId == (event_queue::EVENT_ERROR_CH1_OTP_TRIPPED + i)) {
        return Channel::get(i).isTripped();
    }

    return false;
}

void onLastErrorEventAction() {
    event_queue::Event lastEvent;
    event_queue::getLastErrorEvent(&lastEvent);

    if (lastEvent.eventId == event_queue::EVENT_ERROR_AUX_OTP_TRIPPED &&
        temperature::sensors[temp_sensor::AUX].isTripped()) {
        showPage(PAGE_ID_SYS_SETTINGS_AUX_OTP);
    } else if (isChannelTripLastEvent(0, lastEvent)) {
        g_channel = &Channel::get(0);
        showPage(PAGE_ID_CH_SETTINGS_PROT_CLEAR);
    } else if (isChannelTripLastEvent(1, lastEvent)) {
        g_channel = &Channel::get(1);
        showPage(PAGE_ID_CH_SETTINGS_PROT_CLEAR);
    } else {
        showPage(PAGE_ID_EVENT_QUEUE);
    }
}

} // namespace gui
} // namespace psu
} // namespace eez

#endif
