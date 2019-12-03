/*
* EEZ PSU Firmware
* Copyright (C) 2018-present, Envox d.o.o.
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
* along with this program.  If not, see http://www.gnu.org/licenses.
*/

#if OPTION_DISPLAY

#include <eez/modules/psu/psu.h>

#include <math.h>
#include <stdio.h>

#include <eez/system.h>
#include <eez/util.h>
#include <eez/index.h>
#include <eez/file_type.h>

#include <eez/modules/psu/calibration.h>
#include <eez/modules/psu/channel_dispatcher.h>
#include <eez/modules/psu/dlog_record.h>
#include <eez/modules/psu/dlog_view.h>
#if OPTION_ENCODER
#include <eez/modules/mcu/encoder.h>
#endif
#include <eez/modules/mcu/battery.h>
#if OPTION_ETHERNET
#include <eez/modules/psu/ethernet.h>
#endif
#include <eez/modules/psu/event_queue.h>
#if OPTION_FAN
#include <eez/modules/aux_ps/fan.h>
#endif
#include <eez/modules/psu/list_program.h>
#include <eez/modules/psu/serial_psu.h>
#include <eez/modules/psu/temperature.h>
#include <eez/modules/psu/trigger.h>
#include <eez/modules/psu/ontime.h>

#include <eez/modules/psu/gui/calibration.h>
#include <eez/modules/psu/gui/data.h>
#include <eez/modules/psu/gui/edit_mode.h>
#include <eez/modules/psu/gui/edit_mode_keypad.h>
#include <eez/modules/psu/gui/edit_mode_step.h>
#include <eez/modules/psu/gui/file_manager.h>
#include <eez/modules/psu/gui/numeric_keypad.h>
#include <eez/modules/psu/gui/page_ch_settings_adv.h>
#include <eez/modules/psu/gui/page_ch_settings_protection.h>
#include <eez/modules/psu/gui/page_ch_settings_trigger.h>
#include <eez/modules/psu/gui/page_self_test_result.h>
#include <eez/modules/psu/gui/page_sys_settings.h>
#include <eez/modules/psu/gui/page_user_profiles.h>
#include <eez/modules/psu/gui/psu.h>
#include <eez/gui/dialogs.h>
#if OPTION_SD_CARD
#include <eez/modules/psu/sd_card.h>
#endif
#include <eez/gui/overlay.h>
#include <eez/gui/assets.h>
#include <eez/gui/widgets/container.h>
#include <eez/gui/widgets/yt_graph.h>

using namespace eez::gui;
using namespace eez::gui::data;
using data::EnumItem;
using namespace eez::psu;
using namespace eez::psu::gui;

namespace eez {
namespace gui {
namespace data {

const EnumItem *g_enumDefinitions[] = { 
    g_channelDisplayValueEnumDefinition,
    g_channelTriggerModeEnumDefinition,
    g_triggerSourceEnumDefinition,
    g_channelCurrentRangeSelectionModeEnumDefinition,
    g_channelCurrentRangeEnumDefinition,
    g_channelTriggerOnListStopEnumDefinition,
    g_ioPinsPolarityEnumDefinition,
    g_ioPinsInputFunctionEnumDefinition,
    g_ioPinsOutputFunctionEnumDefinition,
    g_serialParityEnumDefinition,
    g_dstRuleEnumDefinition,
    g_userSwitchActionEnumDefinition,
    g_fileManagerSortByEnumDefinition,
#if defined(EEZ_PLATFORM_SIMULATOR)
    g_moduleTypeEnumDefinition,
#endif
};

} // namespace data
} // namespace gui
} // namespace eez

namespace eez {
namespace gui {

EnumItem g_channelDisplayValueEnumDefinition[] = {
    { DISPLAY_VALUE_VOLTAGE, "Voltage (V)" },
    { DISPLAY_VALUE_CURRENT, "Current (A)" },
    { DISPLAY_VALUE_POWER, "Power (W)" },
    { 0, 0 }
};

EnumItem g_channelTriggerModeEnumDefinition[] = {
    { TRIGGER_MODE_FIXED, "Fixed" },
    { TRIGGER_MODE_LIST, "List" },
    { TRIGGER_MODE_STEP, "Step" },
    { 0, 0 } 
};

EnumItem g_triggerSourceEnumDefinition[] = {
    { trigger::SOURCE_BUS, "Bus" },
    { trigger::SOURCE_IMMEDIATE, "Immediate" },
    { trigger::SOURCE_MANUAL, "Manual" },
    { trigger::SOURCE_PIN1, "Pin1" },
    { trigger::SOURCE_PIN2, "Pin2" },
    { 0, 0 }
};

EnumItem g_channelCurrentRangeSelectionModeEnumDefinition[] = {
    { CURRENT_RANGE_SELECTION_USE_BOTH, "Best (default)" },
    { CURRENT_RANGE_SELECTION_ALWAYS_HIGH, "High (5A)" },
    { CURRENT_RANGE_SELECTION_ALWAYS_LOW, "Low (50mA)" },
    { 0, 0 }
};

EnumItem g_channelCurrentRangeEnumDefinition[] = {
    { CURRENT_RANGE_HIGH, "High" },
    { CURRENT_RANGE_LOW, "Low" },
    { 0, 0 }
};

EnumItem g_channelTriggerOnListStopEnumDefinition[] = {
    { TRIGGER_ON_LIST_STOP_OUTPUT_OFF, "Output OFF" },
    { TRIGGER_ON_LIST_STOP_SET_TO_FIRST_STEP, "Set to first step" },
    { TRIGGER_ON_LIST_STOP_SET_TO_LAST_STEP, "Set to last step" },
    { TRIGGER_ON_LIST_STOP_STANDBY, "Standby" },
    { 0, 0 }
};

EnumItem g_ioPinsPolarityEnumDefinition[] = {
    { io_pins::POLARITY_NEGATIVE, "Negative" },
    { io_pins::POLARITY_POSITIVE, "Positive" },
    { 0, 0 }
};

EnumItem g_ioPinsInputFunctionEnumDefinition[] = {
    { io_pins::FUNCTION_NONE, "None" },
    { io_pins::FUNCTION_INPUT, "Input" },
    { io_pins::FUNCTION_INHIBIT, "Inhibit" },
    { io_pins::FUNCTION_TINPUT, "Trigger input", "Tinput" },
    { 0, 0 }
};

EnumItem g_ioPinsOutputFunctionEnumDefinition[] = {
    { io_pins::FUNCTION_NONE, "None" },
    { io_pins::FUNCTION_OUTPUT, "Output" },
    { io_pins::FUNCTION_FAULT, "Fault" },
    { io_pins::FUNCTION_ON_COUPLE, "Channel ON couple", "ONcoup" },
    { io_pins::FUNCTION_TOUTPUT, "Trigger output", "Toutput" },
    { 0, 0 }
};

EnumItem g_serialParityEnumDefinition[] = {
    { serial::PARITY_NONE, "None" },   { serial::PARITY_EVEN, "Even" },
    { serial::PARITY_ODD, "Odd" },     { serial::PARITY_MARK, "Mark" },
    { serial::PARITY_SPACE, "Space" }, { 0, 0 }
};

EnumItem g_dstRuleEnumDefinition[] = {
    { datetime::DST_RULE_OFF, "Off" },
    { datetime::DST_RULE_EUROPE, "Europe" },
    { datetime::DST_RULE_USA, "USA" },
    { datetime::DST_RULE_AUSTRALIA, "Australia" },
    { 0, 0 }
};

EnumItem g_userSwitchActionEnumDefinition[] = {
	{ persist_conf::USER_SWITCH_ACTION_NONE, "None" },
    { persist_conf::USER_SWITCH_ACTION_ENCODER_STEP, "Encoder Step" },
    { persist_conf::USER_SWITCH_ACTION_SCREENSHOT, "Screenshot" },
    { persist_conf::USER_SWITCH_ACTION_MANUAL_TRIGGER, "Manual Trigger" },
    { persist_conf::USER_SWITCH_ACTION_OUTPUT_ENABLE, "Output Enable" },
    { persist_conf::USER_SWITCH_ACTION_HOME, "Home/Back" },
    { persist_conf::USER_SWITCH_ACTION_INHIBIT, "Inhibit" },
    { persist_conf::USER_SWITCH_ACTION_STANDBY, "Standby" },
    { 0, 0 }
};

EnumItem g_fileManagerSortByEnumDefinition[] = {
	{ SORT_FILES_BY_NAME_ASC, "Name \xa5" },
    { SORT_FILES_BY_NAME_DESC, "Name \xa6" },
    { SORT_FILES_BY_SIZE_ASC, "Size \xa5" },
    { SORT_FILES_BY_SIZE_DESC, "Size \xa6" },
    { SORT_FILES_BY_TIME_ASC, "Time \xa5" },
    { SORT_FILES_BY_TIME_DESC, "Time \xa6" },
    { 0, 0 }
};

#if defined(EEZ_PLATFORM_SIMULATOR)
EnumItem g_moduleTypeEnumDefinition[] = {
    { MODULE_TYPE_NONE, "None" },
    { MODULE_TYPE_DCP405, "DCP405" },
    { MODULE_TYPE_DCP405B, "DCP405B" },
    { MODULE_TYPE_DCM220, "DCM220" },
    { 0, 0 }
};
#endif

////////////////////////////////////////////////////////////////////////////////

Value MakeValue(float value, Unit unit) {
    return Value(value, unit);
}

Value MakeValueListValue(const Value *values) {
    Value value;
    value.type_ = VALUE_TYPE_VALUE_LIST;
    value.options_ = 0;
    value.unit_ = UNIT_UNKNOWN;
    value.pValue_ = values;
    return value;
}

Value MakeFloatListValue(float *pFloat) {
    Value value;
    value.type_ = VALUE_TYPE_FLOAT_LIST;
    value.options_ = 0;
    value.unit_ = UNIT_UNKNOWN;
    value.pFloat_ = pFloat;
    return value;
}

Value MakeEventValue(event_queue::Event *e) {
    Value value;
    value.type_ = VALUE_TYPE_EVENT;
    value.options_ = 0;
    value.unit_ = UNIT_UNKNOWN;
    value.pVoid_ = e;
    return value;
}

Value MakeLessThenMinMessageValue(float float_, const Value &value_) {
    Value value;
    if (value_.getType() == VALUE_TYPE_INT) {
        value.int_ = int(float_);
        value.type_ = VALUE_TYPE_LESS_THEN_MIN_INT;
    } else if (value_.getType() == VALUE_TYPE_TIME_ZONE) {
        value.type_ = VALUE_TYPE_LESS_THEN_MIN_TIME_ZONE;
    } else {
        value.float_ = float_;
        value.unit_ = value_.getUnit();
        value.type_ = VALUE_TYPE_LESS_THEN_MIN_FLOAT;
    }
    return value;
}

Value MakeGreaterThenMaxMessageValue(float float_, const Value &value_) {
    Value value;
    if (value_.getType() == VALUE_TYPE_INT) {
        value.int_ = int(float_);
        value.type_ = VALUE_TYPE_GREATER_THEN_MAX_INT;
    } else if (value_.getType() == VALUE_TYPE_TIME_ZONE) {
        value.type_ = VALUE_TYPE_GREATER_THEN_MAX_TIME_ZONE;
    } else {
        value.float_ = float_;
        value.unit_ = value_.getUnit();
        value.type_ = VALUE_TYPE_GREATER_THEN_MAX_FLOAT;
    }
    return value;
}

Value MakeMacAddressValue(uint8_t *macAddress) {
    Value value;
    value.type_ = VALUE_TYPE_MAC_ADDRESS;
    value.puint8_ = macAddress;
    return value;
}

////////////////////////////////////////////////////////////////////////////////

void printTime(double time, char *text, int count) {
    unsigned int d = (unsigned int)floor(time / (24 * 3600));
    time -= d * (24 * 3600);
    
    unsigned int h = (unsigned int)floor(time / 3600);
    time -= h * 3600;
    
    unsigned int m = (unsigned int)floor(time / 60);
    time -= m * 60;
    
    float s = (float)(floor(time * 1000) / 1000);

    if (d > 0) {
        if (h > 0) {
            snprintf(text, count - 1, "%ud %uh", d, h);
        } else if (m > 0) {
            snprintf(text, count - 1, "%ud %um", d, m);
        } else {
            snprintf(text, count - 1, "%ud %ds", d, (unsigned int)floor(s));
        }
    } else if (h > 0) {
        if (m > 0) {
            snprintf(text, count - 1, "%uh %um", h, m);
        } else {
            snprintf(text, count - 1, "%uh %ds", h, (unsigned int)floor(s));
        }
    } else if (m > 0) {
        snprintf(text, count - 1, "%um %us", m, (unsigned int)floor(s));
    } else {
        snprintf(text, count - 1, "%gs", s);
    }

    text[count - 1] = 0;
}

////////////////////////////////////////////////////////////////////////////////

void printTime(uint32_t time, char *text, int count) {
    printTime((double)time, text, count);
}

////////////////////////////////////////////////////////////////////////////////

event_queue::Event *getEventFromValue(const Value &value) {
    return (event_queue::Event *)value.getVoidPointer();
}

////////////////////////////////////////////////////////////////////////////////

bool compare_LESS_THEN_MIN_FLOAT_value(const Value &a, const Value &b) {
    return a.getUnit() == b.getUnit() && a.getFloat() == b.getFloat();
}

void LESS_THEN_MIN_FLOAT_value_to_text(const Value &value, char *text, int count) {
    char valueText[64];
    MakeValue(value.getFloat(), (Unit)value.getUnit()).toText(valueText, sizeof(text));
    snprintf(text, count - 1, "Value is less then %s", valueText);
    text[count - 1] = 0;
}

bool compare_GREATER_THEN_MAX_FLOAT_value(const Value &a, const Value &b) {
    return a.getUnit() == b.getUnit() && a.getFloat() == b.getFloat();
}

void GREATER_THEN_MAX_FLOAT_value_to_text(const Value &value, char *text, int count) {
    char valueText[64];
    MakeValue(value.getFloat(), (Unit)value.getUnit()).toText(valueText, sizeof(text));
    snprintf(text, count - 1, "Value is greater then %s", valueText);
    text[count - 1] = 0;
}

bool compare_CHANNEL_LABEL_value(const Value &a, const Value &b) {
    return a.getUInt8() == b.getUInt8();
}

void CHANNEL_LABEL_value_to_text(const Value &value, char *text, int count) {
    snprintf(text, count - 1, "Channel %d:", value.getUInt8());
    text[count - 1] = 0;
}

bool compare_CHANNEL_SHORT_LABEL_value(const Value &a, const Value &b) {
    return a.getUInt8() == b.getUInt8();
}

void CHANNEL_SHORT_LABEL_value_to_text(const Value &value, char *text, int count) {
    snprintf(text, count - 1, "Ch%d:", value.getUInt8());
    text[count - 1] = 0;
}

bool compare_CHANNEL_BOARD_INFO_LABEL_value(const Value &a, const Value &b) {
    return a.getInt() == b.getInt();
}

void CHANNEL_BOARD_INFO_LABEL_value_to_text(const Value &value, char *text, int count) {
    snprintf(text, count - 1, "CH%d board:", value.getInt() + 1);
    text[count - 1] = 0;
}

bool compare_LESS_THEN_MIN_INT_value(const Value &a, const Value &b) {
    return a.getInt() == b.getInt();
}

void LESS_THEN_MIN_INT_value_to_text(const Value &value, char *text, int count) {
    snprintf(text, count - 1, "Value is less then %d", value.getInt());
    text[count - 1] = 0;
}

bool compare_LESS_THEN_MIN_TIME_ZONE_value(const Value &a, const Value &b) {
    return true;
}

void LESS_THEN_MIN_TIME_ZONE_value_to_text(const Value &value, char *text, int count) {
    strncpy(text, "Value is less then -12:00", count - 1);
    text[count - 1] = 0;
}

bool compare_GREATER_THEN_MAX_INT_value(const Value &a, const Value &b) {
    return a.getInt() == b.getInt();
}

void GREATER_THEN_MAX_INT_value_to_text(const Value &value, char *text, int count) {
    snprintf(text, count - 1, "Value is greater then %d", value.getInt());
    text[count - 1] = 0;
}

bool compare_GREATER_THEN_MAX_TIME_ZONE_value(const Value &a, const Value &b) {
    return true;
}

void GREATER_THEN_MAX_TIME_ZONE_value_to_text(const Value &value, char *text, int count) {
    strncpy(text, "Value is greater then +14:00", count - 1);
    text[count - 1] = 0;
}

bool compare_EVENT_value(const Value &a, const Value &b) {
    auto aEvent = getEventFromValue(a);
    auto bEvent = getEventFromValue(b);

    if (aEvent == bEvent) {
        return true;
    }

    if (!aEvent || !bEvent) {
        return false;
    }

    return aEvent->dateTime == bEvent->dateTime && aEvent->eventId == bEvent->eventId;
}

void EVENT_value_to_text(const Value &value, char *text, int count) {
    auto event = getEventFromValue(value);
    if (!event) {
        text[0] = 0;
        return;
    }

    int year, month, day, hour, minute, second;
    datetime::breakTime(event->dateTime, year, month, day, hour, minute, second);

    int yearNow, monthNow, dayNow, hourNow, minuteNow, secondNow;
    datetime::breakTime(datetime::now(), yearNow, monthNow, dayNow, hourNow, minuteNow, secondNow);

    if (yearNow == year && monthNow == month && dayNow == day) {
        snprintf(text, count - 1, "%c [%02d:%02d:%02d] %s",
                 127 + event_queue::getEventType(event), hour, minute, second,
                 event_queue::getEventMessage(event));
    } else {
        snprintf(text, count - 1, "%c [%02d-%02d-%02d] %s",
                 127 + event_queue::getEventType(event), day, month, year % 100,
                 event_queue::getEventMessage(event));
    }

    text[count - 1] = 0;
}

bool compare_ON_TIME_COUNTER_value(const Value &a, const Value &b) {
    return a.getUInt32() == b.getUInt32();
}

void ON_TIME_COUNTER_value_to_text(const Value &value, char *text, int count) {
    ontime::counterToString(text, count, value.getUInt32());
}

bool compare_COUNTDOWN_value(const Value &a, const Value &b) {
    return a.getUInt32() == b.getUInt32();
}

void COUNTDOWN_value_to_text(const Value &value, char *text, int count) {
    printTime(value.getUInt32(), text, count);
}

bool compare_TIME_ZONE_value(const Value &a, const Value &b) {
    return a.getInt16() == b.getInt16();
}

void TIME_ZONE_value_to_text(const Value &value, char *text, int count) {
    formatTimeZone(value.getInt16(), text, count);
}

bool compare_DATE_value(const Value &a, const Value &b) {
    return a.getUInt32() == b.getUInt32();
}

void DATE_value_to_text(const Value &value, char *text, int count) {
    int year, month, day, hour, minute, second;
    datetime::breakTime(value.getUInt32(), year, month, day, hour, minute, second);
    snprintf(text, count - 1, "%d - %02d - %02d", year, month, day);
    text[count - 1] = 0;
}

bool compare_YEAR_value(const Value &a, const Value &b) {
    return a.getUInt16() == b.getUInt16();
}

void YEAR_value_to_text(const Value &value, char *text, int count) {
    snprintf(text, count - 1, "%d", value.getUInt16());
    text[count - 1] = 0;
}

bool compare_MONTH_value(const Value &a, const Value &b) {
    return a.getUInt8() == b.getUInt8();
}

void MONTH_value_to_text(const Value &value, char *text, int count) {
    snprintf(text, count - 1, "%02d", value.getUInt8());
    text[count - 1] = 0;
}

bool compare_DAY_value(const Value &a, const Value &b) {
    return a.getUInt8() == b.getUInt8();
}

void DAY_value_to_text(const Value &value, char *text, int count) {
    snprintf(text, count - 1, "%02d", value.getUInt8());
    text[count - 1] = 0;
}

bool compare_TIME_value(const Value &a, const Value &b) {
    return a.getUInt32() == b.getUInt32();
}

void TIME_value_to_text(const Value &value, char *text, int count) {
    int year, month, day, hour, minute, second;
    datetime::breakTime(value.getUInt32(), year, month, day, hour, minute, second);
    snprintf(text, count - 1, "%02d : %02d : %02d", hour, minute, second);
    text[count - 1] = 0;
}

bool compare_HOUR_value(const Value &a, const Value &b) {
    return a.getUInt8() == b.getUInt8();
}

void HOUR_value_to_text(const Value &value, char *text, int count) {
    snprintf(text, count - 1, "%02d", value.getUInt8());
    text[count - 1] = 0;
}

bool compare_MINUTE_value(const Value &a, const Value &b) {
    return a.getUInt8() == b.getUInt8();
}

void MINUTE_value_to_text(const Value &value, char *text, int count) {
    snprintf(text, count - 1, "%02d", value.getUInt8());
    text[count - 1] = 0;
}

bool compare_SECOND_value(const Value &a, const Value &b) {
    return a.getUInt8() == b.getUInt8();
}

void SECOND_value_to_text(const Value &value, char *text, int count) {
    snprintf(text, count - 1, "%02d", value.getUInt8());
    text[count - 1] = 0;
}

bool compare_USER_PROFILE_LABEL_value(const Value &a, const Value &b) {
    return a.getInt() == b.getInt();
}

void USER_PROFILE_LABEL_value_to_text(const Value &value, char *text, int count) {
    snprintf(text, count - 1, "[ %d ]", value.getInt());
    text[count - 1] = 0;
}

bool compare_USER_PROFILE_REMARK_value(const Value &a, const Value &b) {
    return a.getInt() == b.getInt();
}

void USER_PROFILE_REMARK_value_to_text(const Value &value, char *text, int count) {
    profile::getName(value.getInt(), text, count);
}

bool compare_EDIT_INFO_value(const Value &a, const Value &b) {
    return a.getInt() == b.getInt();
}

void EDIT_INFO_value_to_text(const Value &value, char *text, int count) {
    edit_mode::getInfoText(text, count);
}

bool compare_MAC_ADDRESS_value(const Value &a, const Value &b) {
    return memcmp(a.getPUint8(), b.getPUint8(), 6) == 0;
}

void MAC_ADDRESS_value_to_text(const Value &value, char *text, int count) {
    macAddressToString(value.getPUint8(), text);
}

bool compare_IP_ADDRESS_value(const Value &a, const Value &b) {
    return a.getUInt32() == b.getUInt32();
}

void IP_ADDRESS_value_to_text(const Value &value, char *text, int count) {
    ipAddressToString(value.getUInt32(), text);
}

bool compare_PORT_value(const Value &a, const Value &b) {
    return a.getUInt16() == b.getUInt16();
}

void PORT_value_to_text(const Value &value, char *text, int count) {
    snprintf(text, count - 1, "%d", value.getUInt16());
    text[count - 1] = 0;
}

bool compare_TEXT_MESSAGE_value(const Value &a, const Value &b) {
    return a.getUInt8() == b.getUInt8();
}

void TEXT_MESSAGE_value_to_text(const Value &value, char *text, int count) {
    strncpy(text, g_psuAppContext.getTextMessage(), count - 1);
    text[count - 1] = 0;
}

bool compare_SERIAL_BAUD_INDEX_value(const Value &a, const Value &b) {
    return a.getInt() == b.getInt();
}

void SERIAL_BAUD_INDEX_value_to_text(const Value &value, char *text, int count) {
    snprintf(text, count - 1, "%ld", serial::g_bauds[value.getInt() - 1]);
    text[count - 1] = 0;
}

bool compare_VALUE_LIST_value(const Value &a, const Value &b) {
    return a.getValueList() == b.getValueList();
}

void VALUE_LIST_value_to_text(const Value &value, char *text, int count) {
}

bool compare_FLOAT_LIST_value(const Value &a, const Value &b) {
    return a.getFloatList() == b.getFloatList();
}

void FLOAT_LIST_value_to_text(const Value &value, char *text, int count) {
}

bool compare_CHANNEL_TITLE_value(const Value &a, const Value &b) {
    Channel &aChannel = Channel::get(a.getInt());
    Channel &bChannel = Channel::get(b.getInt());
    return aChannel.channelIndex == bChannel.channelIndex && aChannel.flags.trackingEnabled == bChannel.flags.trackingEnabled;
}

void CHANNEL_TITLE_value_to_text(const Value &value, char *text, int count) {
    Channel &channel = Channel::get(value.getInt());
    if (channel.flags.trackingEnabled) {
        snprintf(text, count - 1, "\xA2 #%d", channel.channelIndex + 1);
    } else {
        snprintf(text, count - 1, "%s #%d", g_slots[channel.slotIndex].moduleInfo->moduleName, channel.channelIndex + 1);
    }
}

bool compare_CHANNEL_SHORT_TITLE_value(const Value &a, const Value &b) {
    Channel &aChannel = Channel::get(a.getInt());
    Channel &bChannel = Channel::get(b.getInt());
    return aChannel.channelIndex == bChannel.channelIndex && aChannel.flags.trackingEnabled == bChannel.flags.trackingEnabled;
}

void CHANNEL_SHORT_TITLE_value_to_text(const Value &value, char *text, int count) {
    Channel &channel = Channel::get(value.getInt());
    if (channel.flags.trackingEnabled) {
        snprintf(text, count - 1, "\xA2");
    } else {
        snprintf(text, count - 1, "#%d", channel.channelIndex + 1);
    }
}

bool compare_CHANNEL_SHORT_TITLE_WITHOUT_TRACKING_ICON_value(const Value &a, const Value &b) {
    return a.getInt() == b.getInt();
}

void CHANNEL_SHORT_TITLE_WITHOUT_TRACKING_ICON_value_to_text(const Value &value, char *text, int count) {
    Channel &channel = Channel::get(value.getInt());
    snprintf(text, count - 1, "#%d", channel.channelIndex + 1);
}

bool compare_CHANNEL_SHORT_TITLE_WITH_COLON_value(const Value &a, const Value &b) {
    return a.getInt() == b.getInt();
}

void CHANNEL_SHORT_TITLE_WITH_COLON_value_to_text(const Value &value, char *text, int count) {
    Channel &channel = Channel::get(value.getInt());
    snprintf(text, count - 1, "#%d:", channel.channelIndex + 1);
}

bool compare_CHANNEL_LONG_TITLE_value(const Value &a, const Value &b) {
    Channel &aChannel = Channel::get(a.getInt());
    Channel &bChannel = Channel::get(b.getInt());
    return aChannel.channelIndex == bChannel.channelIndex && aChannel.flags.trackingEnabled == bChannel.flags.trackingEnabled;
}

void CHANNEL_LONG_TITLE_value_to_text(const Value &value, char *text, int count) {
    auto &channel = Channel::get(value.getInt());
    auto &slot = g_slots[channel.slotIndex];
    if (channel.flags.trackingEnabled) {
        snprintf(text, count - 1, "\xA2 %s #%d: %dV/%dA, R%dB%d", slot.moduleInfo->moduleName, channel.channelIndex + 1, 
            (int)floor(channel.params.U_MAX), (int)floor(channel.params.I_MAX), 
            (int)(slot.moduleRevision >> 8), (int)(slot.moduleRevision & 0xFF));
    } else {
        snprintf(text, count - 1, "%s #%d: %dV/%dA, R%dB%d", g_slots[channel.slotIndex].moduleInfo->moduleName, channel.channelIndex + 1, 
            (int)floor(channel.params.U_MAX), (int)floor(channel.params.I_MAX), 
            (int)(slot.moduleRevision >> 8), (int)(slot.moduleRevision & 0xFF));
    }
}

bool compare_DLOG_VALUE_LABEL_value(const Value &a, const Value &b) {
    return a.getInt() == b.getInt();
}

void DLOG_VALUE_LABEL_value_to_text(const Value &value, char *text, int count) {
    static const char labels[] = { 'U', 'I', 'P' };
    int dlogValue = value.getInt();
    snprintf(text, count - 1, "%c%d", labels[dlogValue % 3], dlogValue / 3 + 1);
}

static double g_savedCurrentTime;

bool compare_DLOG_CURRENT_TIME_value(const Value &a, const Value &b) {
    bool result = g_savedCurrentTime == dlog_record::g_currentTime;
    g_savedCurrentTime = dlog_record::g_currentTime;
    return result;
}

void DLOG_CURRENT_TIME_value_to_text(const Value &value, char *text, int count) {
    printTime(g_savedCurrentTime, text, count);
}

bool compare_FILE_LENGTH_value(const Value &a, const Value &b) {
    return a.getUInt32() == b.getUInt32();
}

void FILE_LENGTH_value_to_text(const Value &value, char *text, int count) {
    formatBytes(value.getUInt32(), text, count);
}

bool compare_FILE_DATE_TIME_value(const Value &a, const Value &b) {
    return a.getUInt32() == b.getUInt32();
}

void FILE_DATE_TIME_value_to_text(const Value &value, char *text, int count) {
    int year, month, day, hour, minute, second;
    datetime::breakTime(value.getUInt32(), year, month, day, hour, minute, second);

    int yearNow, monthNow, dayNow, hourNow, minuteNow, secondNow;
    datetime::breakTime(datetime::now(), yearNow, monthNow, dayNow, hourNow, minuteNow, secondNow);

    if (yearNow == year && monthNow == month && dayNow == day) {
        snprintf(text, count - 1, "%02d:%02d:%02d", hour, minute, second);
    } else {
        snprintf(text, count - 1, "%02d-%02d-%02d", day, month, year % 100);
    }

    text[count - 1] = 0;
}

////////////////////////////////////////////////////////////////////////////////

} // namespace gui
} // namespace eez

namespace eez {
namespace gui {
namespace data {

CompareValueFunction g_compareUserValueFunctions[] = {
    compare_LESS_THEN_MIN_FLOAT_value,
    compare_GREATER_THEN_MAX_FLOAT_value,
    compare_CHANNEL_LABEL_value,
    compare_CHANNEL_SHORT_LABEL_value,
    compare_CHANNEL_BOARD_INFO_LABEL_value,
    compare_LESS_THEN_MIN_INT_value,
    compare_LESS_THEN_MIN_TIME_ZONE_value,
    compare_GREATER_THEN_MAX_INT_value,
    compare_GREATER_THEN_MAX_TIME_ZONE_value,
    compare_EVENT_value,
    compare_ON_TIME_COUNTER_value,
    compare_COUNTDOWN_value,
    compare_TIME_ZONE_value,
    compare_DATE_value,
    compare_YEAR_value,
    compare_MONTH_value,
    compare_DAY_value,
    compare_TIME_value,
    compare_HOUR_value,
    compare_MINUTE_value,
    compare_SECOND_value,
    compare_USER_PROFILE_LABEL_value,
    compare_USER_PROFILE_REMARK_value,
    compare_EDIT_INFO_value,
    compare_MAC_ADDRESS_value,
    compare_IP_ADDRESS_value,
    compare_PORT_value,
    compare_TEXT_MESSAGE_value,
    compare_SERIAL_BAUD_INDEX_value,
    compare_VALUE_LIST_value,
    compare_FLOAT_LIST_value,
    compare_CHANNEL_TITLE_value,
    compare_CHANNEL_SHORT_TITLE_value,
    compare_CHANNEL_SHORT_TITLE_WITHOUT_TRACKING_ICON_value,
    compare_CHANNEL_SHORT_TITLE_WITH_COLON_value,
    compare_CHANNEL_LONG_TITLE_value,
    compare_DLOG_VALUE_LABEL_value,
    compare_DLOG_CURRENT_TIME_value,
    compare_FILE_LENGTH_value,
    compare_FILE_DATE_TIME_value
};

ValueToTextFunction g_userValueToTextFunctions[] = { 
    LESS_THEN_MIN_FLOAT_value_to_text,
    GREATER_THEN_MAX_FLOAT_value_to_text,
    CHANNEL_LABEL_value_to_text,
    CHANNEL_SHORT_LABEL_value_to_text,
    CHANNEL_BOARD_INFO_LABEL_value_to_text,
    LESS_THEN_MIN_INT_value_to_text,
    LESS_THEN_MIN_TIME_ZONE_value_to_text,
    GREATER_THEN_MAX_INT_value_to_text,
    GREATER_THEN_MAX_TIME_ZONE_value_to_text,
    EVENT_value_to_text,
    ON_TIME_COUNTER_value_to_text,
    COUNTDOWN_value_to_text,
    TIME_ZONE_value_to_text,
    DATE_value_to_text,
    YEAR_value_to_text,
    MONTH_value_to_text,
    DAY_value_to_text,
    TIME_value_to_text,
    HOUR_value_to_text,
    MINUTE_value_to_text,
    SECOND_value_to_text,
    USER_PROFILE_LABEL_value_to_text,
    USER_PROFILE_REMARK_value_to_text,
    EDIT_INFO_value_to_text,
    MAC_ADDRESS_value_to_text,
    IP_ADDRESS_value_to_text,
    PORT_value_to_text,
    TEXT_MESSAGE_value_to_text,
    SERIAL_BAUD_INDEX_value_to_text,
    VALUE_LIST_value_to_text,
    FLOAT_LIST_value_to_text,
    CHANNEL_TITLE_value_to_text,
    CHANNEL_SHORT_TITLE_value_to_text,
    CHANNEL_SHORT_TITLE_WITHOUT_TRACKING_ICON_value_to_text,
    CHANNEL_SHORT_TITLE_WITH_COLON_value_to_text,
    CHANNEL_LONG_TITLE_value_to_text,
    DLOG_VALUE_LABEL_value_to_text,
    DLOG_CURRENT_TIME_value_to_text,
    FILE_LENGTH_value_to_text,
    FILE_DATE_TIME_value_to_text
};

} // namespace data
} // namespace gui
} // namespace eez

namespace eez {
namespace gui {

using namespace eez::psu;

Page *getUserProfilesPage() {
    Page *page = getPage(PAGE_ID_USER_PROFILE_SETTINGS);
    if (!page) {
        page = getPage(PAGE_ID_USER_PROFILE_0_SETTINGS);
    }
    if (!page) {
        page = getPage(PAGE_ID_USER_PROFILES);
    }
    return page;
}

static Cursor g_editValueCursor;
static uint16_t g_editValueDataId;

void onSetFloatValue(float value) {
    popPage();
    set(g_editValueCursor, g_editValueDataId, MakeValue(value, getUnit(g_editValueCursor, g_editValueDataId)), nullptr);
}

void onSetInfinityValue() {
    popPage();
    set(g_editValueCursor, g_editValueDataId, MakeValue(INFINITY, getUnit(g_editValueCursor, g_editValueDataId)), nullptr);
}

void onSetStringValue(char *value) {
    popPage();
    set(g_editValueCursor, g_editValueDataId, value, nullptr);
}

void editValue(uint16_t dataId) {
    g_editValueDataId = dataId;
    Value value = get(g_editValueCursor, g_editValueDataId);

    if (value.getType() == VALUE_TYPE_FLOAT) {
        NumericKeypadOptions options;

        options.editValueUnit = value.getUnit();

        options.min = getMin(g_editValueCursor, g_editValueDataId).getFloat();

        auto max = getMax(g_editValueCursor, g_editValueDataId);
        if (max.getType() != VALUE_TYPE_NONE) {
            if (isinf(max.getFloat())) {
                options.flags.option1ButtonEnabled = true;
                options.option1ButtonText = INF_TEXT;
                options.option1 = onSetInfinityValue;
            } else {
                options.max = max.getFloat();
                options.enableMaxButton();
            }
        }

        auto min = getDef(g_editValueCursor, g_editValueDataId);
        if (min.getType() != VALUE_TYPE_NONE) {
            options.min = min.getFloat();
            options.enableMinButton();
        }

        options.flags.signButtonEnabled = options.min < 0;
        options.flags.dotButtonEnabled = true;
        options.flags.option1ButtonEnabled = true;

        NumericKeypad::start(0, value, options, onSetFloatValue, 0, 0);
    } else {
        Keypad::startPush(0, value.getString(), getMax(g_editValueCursor, g_editValueDataId).getUInt32(), false, onSetStringValue, 0);
    }
}

void data_none(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    value = Value();
}

void data_edit_enabled(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        int iChannel = cursor.i >= 0 ? cursor.i : (g_channel ? g_channel->channelIndex : 0);
        Channel &channel = Channel::get(iChannel);

        if ((channel_dispatcher::getVoltageTriggerMode(channel) != TRIGGER_MODE_FIXED && !trigger::isIdle()) || isPageOnStack(PAGE_ID_CH_SETTINGS_LISTS)) {
            value = 0;
        }
        if (psu::calibration::isEnabled()) {
            value = 0;
        }
        value = 1;
    }
}

void data_channels(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_COUNT) {
        value = CH_NUM;
    } else if (operation == data::DATA_OPERATION_SELECT) {
        value = data::Value(g_channel, VALUE_TYPE_POINTER);
        selectChannel(&Channel::get(cursor.i));
    } else if (operation == data::DATA_OPERATION_DESELECT) {
        selectChannel((Channel * )value.getVoidPointer());
    }
}

void data_channel_status(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        int iChannel = cursor.i >= 0 ? cursor.i : (g_channel ? g_channel->channelIndex : 0);
        Channel &channel = Channel::get(iChannel);
        int channelStatus = channel.isInstalled() ? (channel.isOk() ? 1 : 2) : 0;
        value = channelStatus;
    }
}

void data_channel_output_state(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        int iChannel = cursor.i >= 0 ? cursor.i : (g_channel ? g_channel->channelIndex : 0);
        Channel &channel = Channel::get(iChannel);
        value = channel.isOutputEnabled();
    }
}

void data_channel_is_cc(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        int iChannel = cursor.i >= 0 ? cursor.i : (g_channel ? g_channel->channelIndex : 0);
        Channel &channel = Channel::get(iChannel);
        value = channel_dispatcher::getChannelMode(channel) == channel_dispatcher::CHANNEL_MODE_CC;
    }
}

void data_channel_is_cv(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        int iChannel = cursor.i >= 0 ? cursor.i : (g_channel ? g_channel->channelIndex : 0);
        Channel &channel = Channel::get(iChannel);
        value = channel_dispatcher::getChannelMode(channel) == channel_dispatcher::CHANNEL_MODE_CV;
    }
}

void data_channel_u_set(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    int iChannel = cursor.i >= 0 ? cursor.i : (g_channel ? g_channel->channelIndex : 0);
    Channel &channel = Channel::get(iChannel);
    if (operation == data::DATA_OPERATION_GET) {
        value = MakeValue(channel_dispatcher::getUSet(channel), UNIT_VOLT);
    } else if (operation == data::DATA_OPERATION_GET_EDIT_VALUE) {
        value = MakeValue(channel_dispatcher::getUSetUnbalanced(channel), UNIT_VOLT);
    } else if (operation == data::DATA_OPERATION_GET_MIN) {
        value = MakeValue(channel_dispatcher::getUMin(channel), UNIT_VOLT);
    } else if (operation == data::DATA_OPERATION_GET_MAX) {
        value = MakeValue(channel_dispatcher::getUMax(channel), UNIT_VOLT);
    } else if (operation == data::DATA_OPERATION_GET_LIMIT) {
        value = MakeValue(channel_dispatcher::getULimit(channel), UNIT_VOLT);
    } else if (operation == data::DATA_OPERATION_GET_UNIT) {
        value = UNIT_VOLT;
    } else if (operation == data::DATA_OPERATION_SET) {
        if (!between(value.getFloat(), channel_dispatcher::getUMin(channel), channel_dispatcher::getUMax(channel))) {
            value = MakeScpiErrorValue(SCPI_ERROR_DATA_OUT_OF_RANGE);
        } else if (value.getFloat() > channel_dispatcher::getULimit(channel)) {
            value = MakeScpiErrorValue(SCPI_ERROR_VOLTAGE_LIMIT_EXCEEDED);
        } else if (value.getFloat() * channel_dispatcher::getISetUnbalanced(channel) > channel_dispatcher::getPowerLimit(channel)) {
            value = MakeScpiErrorValue(SCPI_ERROR_POWER_LIMIT_EXCEEDED);
        } else {
            channel_dispatcher::setVoltage(channel, value.getFloat());
        }
    }
}

void data_channel_u_mon(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    int iChannel = cursor.i >= 0 ? cursor.i : (g_channel ? g_channel->channelIndex : 0);
    Channel &channel = Channel::get(iChannel);
    if (operation == data::DATA_OPERATION_GET) {
        value = MakeValue(channel_dispatcher::getUMon(channel), UNIT_VOLT);
    } else if (operation == data::DATA_OPERATION_GET_MIN) {
        value = MakeValue(channel_dispatcher::getUMin(channel), UNIT_VOLT);
    } else if (operation == data::DATA_OPERATION_GET_MAX) {
        value = MakeValue(channel_dispatcher::getUMax(channel), UNIT_VOLT);
    } else if (operation == data::DATA_OPERATION_GET_LIMIT) {
        value = MakeValue(channel_dispatcher::getULimit(channel), UNIT_VOLT);
    } else if (operation == data::DATA_OPERATION_GET_COLOR) {
        if (io_pins::isInhibited() || channel_dispatcher::getChannelMode(channel) == channel_dispatcher::CHANNEL_MODE_UR) {
            value = Value(COLOR_ID_STATUS_WARNING, VALUE_TYPE_UINT16);
        }
    } else if (operation == data::DATA_OPERATION_GET_BACKGROUND_COLOR) {
        if (!dlog_record::isIdle() && dlog_record::g_recording.parameters.logVoltage[iChannel]) {
            value = Value(COLOR_ID_DATA_LOGGING, VALUE_TYPE_UINT16);
        }
    } else if (operation == data::DATA_OPERATION_GET_ACTIVE_COLOR) {
        if (io_pins::isInhibited()) {
            value = Value(((const Style *)value.getVoidPointer())->background_color, VALUE_TYPE_UINT16);
        }
    } else if (operation == data::DATA_OPERATION_GET_ACTIVE_BACKGROUND_COLOR) {
        if (io_pins::isInhibited()) {
            value = Value(((const Style *)value.getVoidPointer())->background_color, VALUE_TYPE_UINT16);
        }
    } else if (operation == data::DATA_OPERATION_IS_BLINKING) {
        value = io_pins::isInhibited() ? 1 : 0;
    }
}

void data_channel_u_mon_snapshot(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (false && operation == data::DATA_OPERATION_GET) {
        int iChannel = cursor.i >= 0 ? cursor.i : (g_channel ? g_channel->channelIndex : 0);
        Channel &channel = Channel::get(iChannel);
        value = MakeValue(channel_dispatcher::getUMonSnapshot(channel), UNIT_VOLT);
    } else {
        data_channel_u_mon(operation, cursor, value);
    }
}

void data_channel_u_mon_dac(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    int iChannel = cursor.i >= 0 ? cursor.i : (g_channel ? g_channel->channelIndex : 0);
    Channel &channel = Channel::get(iChannel);
    if (operation == data::DATA_OPERATION_GET) {
        value = MakeValue(channel_dispatcher::getUMonDac(channel), UNIT_VOLT);
    } else if (operation == data::DATA_OPERATION_GET_BACKGROUND_COLOR) {
        if (!dlog_record::isIdle() && dlog_record::g_recording.parameters.logVoltage[iChannel]) {
            value = Value(COLOR_ID_DATA_LOGGING, VALUE_TYPE_UINT16);
        }
    }
}

void data_channel_u_limit(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    int iChannel = cursor.i >= 0 ? cursor.i : (g_channel ? g_channel->channelIndex : 0);
    Channel &channel = Channel::get(iChannel);
    if (operation == data::DATA_OPERATION_GET) {
        value = MakeValue(channel_dispatcher::getULimit(channel), UNIT_VOLT);
    }
}

void data_channel_u_edit(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    int iChannel = cursor.i >= 0 ? cursor.i : (g_channel ? g_channel->channelIndex : 0);
    Channel &channel = Channel::get(iChannel);
    if (operation == data::DATA_OPERATION_GET) {
        bool focused = g_focusCursor == cursor && g_focusDataId == DATA_ID_CHANNEL_U_EDIT;
        if (focused && g_focusEditValue.getType() != VALUE_TYPE_NONE) {
            value = g_focusEditValue;
        } else if (focused && getActivePageId() == PAGE_ID_EDIT_MODE_KEYPAD && edit_mode_keypad::g_keypad->isEditing()) {
            data_keypad_text(operation, cursor, value);
        } else {
            value = MakeValue(channel_dispatcher::getUSet(channel), UNIT_VOLT);
        }
    } else if (operation == data::DATA_OPERATION_GET_EDIT_VALUE) {
        value = MakeValue(channel_dispatcher::getUSetUnbalanced(channel), UNIT_VOLT);
    } else if (operation == data::DATA_OPERATION_GET_MIN) {
        value = MakeValue(channel_dispatcher::getUMin(channel), UNIT_VOLT);
    } else if (operation == data::DATA_OPERATION_GET_MAX) {
        value = MakeValue(channel_dispatcher::getUMax(channel), UNIT_VOLT);
    } else if (operation == data::DATA_OPERATION_GET_LIMIT) {
        value = MakeValue(channel_dispatcher::getULimit(channel), UNIT_VOLT);
    } else if (operation == data::DATA_OPERATION_GET_NAME) {
        value = "Voltage";
    } else if (operation == data::DATA_OPERATION_GET_UNIT) {
        value = UNIT_VOLT;
    } else if (operation == data::DATA_OPERATION_SET) {
        if (!between(value.getFloat(), channel_dispatcher::getUMin(channel), channel_dispatcher::getUMax(channel))) {
            value = MakeScpiErrorValue(SCPI_ERROR_DATA_OUT_OF_RANGE);
        } else if (value.getFloat() > channel_dispatcher::getULimit(channel)) {
            value = MakeScpiErrorValue(SCPI_ERROR_VOLTAGE_LIMIT_EXCEEDED);
        } else if (value.getFloat() * channel_dispatcher::getISetUnbalanced(channel) > channel_dispatcher::getPowerLimit(channel)) {
            value = MakeScpiErrorValue(SCPI_ERROR_POWER_LIMIT_EXCEEDED);
        } else {
            channel_dispatcher::setVoltage(channel, value.getFloat());
        }
    }
}

void data_channel_i_set(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    int iChannel = cursor.i >= 0 ? cursor.i : (g_channel ? g_channel->channelIndex : 0);
    Channel &channel = Channel::get(iChannel);
    if (operation == data::DATA_OPERATION_GET) {
        value = MakeValue(channel_dispatcher::getISet(channel), UNIT_AMPER);
    } else if (operation == data::DATA_OPERATION_GET_EDIT_VALUE) {
        value = MakeValue(channel_dispatcher::getISetUnbalanced(channel), UNIT_AMPER);
    } else if (operation == data::DATA_OPERATION_GET_MIN) {
        value = MakeValue(channel_dispatcher::getIMin(channel), UNIT_AMPER);
    } else if (operation == data::DATA_OPERATION_GET_MAX) {
        value = MakeValue(channel_dispatcher::getIMax(channel), UNIT_AMPER);
    } else if (operation == data::DATA_OPERATION_GET_LIMIT) {
        value = MakeValue(channel_dispatcher::getILimit(channel), UNIT_AMPER);
    } else if (operation == data::DATA_OPERATION_GET_UNIT) {
        value = UNIT_AMPER;
    } else if (operation == data::DATA_OPERATION_SET) {
        if (!between(value.getFloat(), channel_dispatcher::getIMin(channel), channel_dispatcher::getIMax(channel))) {
            value = MakeScpiErrorValue(SCPI_ERROR_DATA_OUT_OF_RANGE);
        } else if (value.getFloat() > channel_dispatcher::getILimit(channel)) {
            value = MakeScpiErrorValue(SCPI_ERROR_CURRENT_LIMIT_EXCEEDED);
        } else if (value.getFloat() * channel_dispatcher::getUSetUnbalanced(channel) > channel_dispatcher::getPowerLimit(channel)) {
            value = MakeScpiErrorValue(SCPI_ERROR_POWER_LIMIT_EXCEEDED);
        } else {
            channel_dispatcher::setCurrent(channel, value.getFloat());
        }
    }
}

void data_channel_i_mon(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    int iChannel = cursor.i >= 0 ? cursor.i : (g_channel ? g_channel->channelIndex : 0);
    Channel &channel = Channel::get(iChannel);
    if (operation == data::DATA_OPERATION_GET) {
        value = MakeValue(channel_dispatcher::getIMon(channel), UNIT_AMPER);
    } else if (operation == data::DATA_OPERATION_GET_MIN) {
        value = MakeValue(channel_dispatcher::getIMin(channel), UNIT_AMPER);
    } else if (operation == data::DATA_OPERATION_GET_MAX) {
        value = MakeValue(channel_dispatcher::getIMax(channel), UNIT_AMPER);
    } else if (operation == data::DATA_OPERATION_GET_LIMIT) {
        value = MakeValue(channel_dispatcher::getILimit(channel), UNIT_AMPER);
    } else if (operation == data::DATA_OPERATION_GET_COLOR) {
        if (io_pins::isInhibited() || channel_dispatcher::getChannelMode(channel) == channel_dispatcher::CHANNEL_MODE_UR) {
            value = Value(COLOR_ID_STATUS_WARNING, VALUE_TYPE_UINT16);
        }
    } else if (operation == data::DATA_OPERATION_GET_BACKGROUND_COLOR) {
        if (!dlog_record::isIdle() && dlog_record::g_recording.parameters.logCurrent[iChannel]) {
            value = Value(COLOR_ID_DATA_LOGGING, VALUE_TYPE_UINT16);
        }
    } else if (operation == data::DATA_OPERATION_GET_ACTIVE_COLOR) {
        if (io_pins::isInhibited()) {
            value = Value(((const Style *)value.getVoidPointer())->background_color, VALUE_TYPE_UINT16);
        }
    } else if (operation == data::DATA_OPERATION_GET_ACTIVE_BACKGROUND_COLOR) {
        if (io_pins::isInhibited()) {
            value = Value(((const Style *)value.getVoidPointer())->background_color, VALUE_TYPE_UINT16);
        }
    } else if (operation == data::DATA_OPERATION_IS_BLINKING) {
        value = io_pins::isInhibited() ? 1 : 0;
    }
}

void data_channel_i_mon_snapshot(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (false && operation == data::DATA_OPERATION_GET) {
        int iChannel = cursor.i >= 0 ? cursor.i : (g_channel ? g_channel->channelIndex : 0);
        Channel &channel = Channel::get(iChannel);
        value = MakeValue(channel_dispatcher::getIMonSnapshot(channel), UNIT_AMPER);
    } else {
        data_channel_i_mon(operation, cursor, value);
    }
}

void data_channel_i_mon_dac(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    int iChannel = cursor.i >= 0 ? cursor.i : (g_channel ? g_channel->channelIndex : 0);
    Channel &channel = Channel::get(iChannel);
    if (operation == data::DATA_OPERATION_GET) {
        value = MakeValue(channel_dispatcher::getIMonDac(channel), UNIT_AMPER);
    } else if (operation == data::DATA_OPERATION_GET_BACKGROUND_COLOR) {
        if (!dlog_record::isIdle() && dlog_record::g_recording.parameters.logCurrent[iChannel]) {
            value = Value(COLOR_ID_DATA_LOGGING, VALUE_TYPE_UINT16);
        }
    } 
}

void data_channel_i_limit(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    int iChannel = cursor.i >= 0 ? cursor.i : (g_channel ? g_channel->channelIndex : 0);
    Channel &channel = Channel::get(iChannel);
    if (operation == data::DATA_OPERATION_GET) {
        value = MakeValue(channel_dispatcher::getILimit(channel), UNIT_AMPER);
    }
}

void data_channel_i_edit(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    int iChannel = cursor.i >= 0 ? cursor.i : (g_channel ? g_channel->channelIndex : 0);
    Channel &channel = Channel::get(iChannel);
    if (operation == data::DATA_OPERATION_GET) {
        bool focused = g_focusCursor == cursor && g_focusDataId == DATA_ID_CHANNEL_I_EDIT;
        if (focused && g_focusEditValue.getType() != VALUE_TYPE_NONE) {
            value = g_focusEditValue;
        } else if (focused && getActivePageId() == PAGE_ID_EDIT_MODE_KEYPAD && edit_mode_keypad::g_keypad->isEditing()) {
            data_keypad_text(operation, cursor, value);
        } else {
            value = MakeValue(channel_dispatcher::getISet(channel), UNIT_AMPER);
        }
    } else if (operation == data::DATA_OPERATION_GET_EDIT_VALUE) {
        value = MakeValue(channel_dispatcher::getISetUnbalanced(channel), UNIT_AMPER);
    } else if (operation == data::DATA_OPERATION_GET_MIN) {
        value = MakeValue(channel_dispatcher::getIMin(channel), UNIT_AMPER);
    } else if (operation == data::DATA_OPERATION_GET_MAX) {
        value = MakeValue(channel_dispatcher::getIMax(channel), UNIT_AMPER);
    } else if (operation == data::DATA_OPERATION_GET_LIMIT) {
        value = MakeValue(channel_dispatcher::getILimit(channel), UNIT_AMPER);
    } else if (operation == data::DATA_OPERATION_GET_NAME) {
        value = "Current";
    } else if (operation == data::DATA_OPERATION_GET_UNIT) {
        value = UNIT_AMPER;
    } else if (operation == data::DATA_OPERATION_SET) {
        if (!between(value.getFloat(), channel_dispatcher::getIMin(channel), channel_dispatcher::getIMax(channel))) {
            value = MakeScpiErrorValue(SCPI_ERROR_DATA_OUT_OF_RANGE);
        } else if (value.getFloat() > channel_dispatcher::getILimit(channel)) {
            value = MakeScpiErrorValue(SCPI_ERROR_CURRENT_LIMIT_EXCEEDED);
        } else if (value.getFloat() * channel_dispatcher::getUSetUnbalanced(channel) > channel_dispatcher::getPowerLimit(channel)) {
            value = MakeScpiErrorValue(SCPI_ERROR_POWER_LIMIT_EXCEEDED);
        } else {
            channel_dispatcher::setCurrent(channel, value.getFloat());
        }
    }
}

void data_channel_p_mon(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    int iChannel = cursor.i >= 0 ? cursor.i : (g_channel ? g_channel->channelIndex : 0);
    Channel &channel = Channel::get(iChannel);
    if (operation == data::DATA_OPERATION_GET) {
        value = MakeValue(channel_dispatcher::getUMon(channel) * channel_dispatcher::getIMon(channel), UNIT_WATT);
    } else if (operation == data::DATA_OPERATION_GET_MIN) {
        value = MakeValue(channel_dispatcher::getPowerMinLimit(channel), UNIT_WATT);
    } else if (operation == data::DATA_OPERATION_GET_MAX) {
        value = MakeValue(channel_dispatcher::getPowerMaxLimit(channel), UNIT_WATT);
    } else if (operation == data::DATA_OPERATION_GET_LIMIT) {
        value = MakeValue(channel_dispatcher::getPowerLimit(channel), UNIT_WATT);
    } else if (operation == data::DATA_OPERATION_GET_UNIT) {
    } else if (operation == data::DATA_OPERATION_GET_COLOR) {
        if (io_pins::isInhibited() || channel_dispatcher::getChannelMode(channel) == channel_dispatcher::CHANNEL_MODE_UR) {
            value = Value(COLOR_ID_STATUS_WARNING, VALUE_TYPE_UINT16);
        }
    } else if (operation == data::DATA_OPERATION_GET_BACKGROUND_COLOR) {
        if (!dlog_record::isIdle() && dlog_record::g_recording.parameters.logPower[iChannel]) {
            value = Value(COLOR_ID_DATA_LOGGING, VALUE_TYPE_UINT16);
        }
    } else if (operation == data::DATA_OPERATION_GET_ACTIVE_COLOR) {
        if (io_pins::isInhibited()) {
            value = Value(((const Style *)value.getVoidPointer())->background_color, VALUE_TYPE_UINT16);
        }
    } else if (operation == data::DATA_OPERATION_GET_ACTIVE_BACKGROUND_COLOR) {
        if (io_pins::isInhibited()) {
            value = Value(((const Style *)value.getVoidPointer())->background_color, VALUE_TYPE_UINT16);
        }
    } else if (operation == data::DATA_OPERATION_IS_BLINKING) {
        value = io_pins::isInhibited() ? 1 : 0;
    }
}

void data_channel_p_mon_snapshot(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (false && operation == data::DATA_OPERATION_GET) {
        int iChannel = cursor.i >= 0 ? cursor.i : (g_channel ? g_channel->channelIndex : 0);
        Channel &channel = Channel::get(iChannel);
        value = MakeValue(channel_dispatcher::getUMonSnapshot(channel) * channel_dispatcher::getIMonSnapshot(channel), UNIT_WATT);
    } else {
        data_channel_p_mon(operation, cursor, value);
    }
}

void data_channel_other_value_mon(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (g_focusDataId == DATA_ID_CHANNEL_U_EDIT) {
        data_channel_i_mon(operation, cursor, value);
    } else if (g_focusDataId == DATA_ID_CHANNEL_I_EDIT) {
        data_channel_u_mon(operation, cursor, value);
    }
}

void data_channels_is_max_view(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        value = (int)persist_conf::isMaxChannelView();
    }
}

void data_channels_view_mode(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        value = (int)persist_conf::devConf.channelsViewMode;
    }
}

void data_channels_view_mode_in_default(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        value = (int)persist_conf::devConf.channelsViewMode;
    }
}

void data_channels_view_mode_in_max(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        value = (int)persist_conf::devConf.channelsViewModeInMax;
    }
}

int getDefaultView(int channelIndex) {
    int isVert = persist_conf::devConf.channelsViewMode == CHANNELS_VIEW_MODE_NUMERIC || persist_conf::devConf.channelsViewMode == CHANNELS_VIEW_MODE_VERT_BAR;

    Channel &channel = Channel::get(channelIndex);
    if (channel.isInstalled()) {
        if (channel.isOk()) {
            int numChannels = g_slots[channel.slotIndex].moduleInfo->numChannels;
            if (numChannels == 1) {
                if (channel_dispatcher::getCouplingType() == channel_dispatcher::COUPLING_TYPE_SERIES && channel.channelIndex == 1) {
                    if (persist_conf::devConf.channelsViewMode == CHANNELS_VIEW_MODE_NUMERIC || persist_conf::devConf.channelsViewMode == CHANNELS_VIEW_MODE_VERT_BAR) {
                        return PAGE_ID_SLOT_DEF_1CH_VERT_COUPLED_SERIES;
                    } else {
                        return PAGE_ID_SLOT_DEF_1CH_HORZ_COUPLED_SERIES;
                    }
                } else if (channel_dispatcher::getCouplingType() == channel_dispatcher::COUPLING_TYPE_PARALLEL && channel.channelIndex == 1) {
                    if (persist_conf::devConf.channelsViewMode == CHANNELS_VIEW_MODE_NUMERIC || persist_conf::devConf.channelsViewMode == CHANNELS_VIEW_MODE_VERT_BAR) {
                        return PAGE_ID_SLOT_DEF_1CH_VERT_COUPLED_PARALLEL;
                    } else {
                        return PAGE_ID_SLOT_DEF_1CH_HORZ_COUPLED_PARALLEL;
                    }
                } else if (persist_conf::devConf.channelsViewMode == CHANNELS_VIEW_MODE_NUMERIC) {
                    return channel.isOutputEnabled() ? PAGE_ID_SLOT_DEF_1CH_NUM_ON : PAGE_ID_SLOT_DEF_1CH_VERT_OFF;
                } else if (persist_conf::devConf.channelsViewMode == CHANNELS_VIEW_MODE_VERT_BAR) {
                    return channel.isOutputEnabled() ? PAGE_ID_SLOT_DEF_1CH_VBAR_ON : PAGE_ID_SLOT_DEF_1CH_VERT_OFF;
                } else if (persist_conf::devConf.channelsViewMode == CHANNELS_VIEW_MODE_HORZ_BAR) {
                    return channel.isOutputEnabled() ? PAGE_ID_SLOT_DEF_1CH_HBAR_ON : PAGE_ID_SLOT_DEF_1CH_HORZ_OFF;
                } else if (persist_conf::devConf.channelsViewMode == CHANNELS_VIEW_MODE_YT) {
                    return channel.isOutputEnabled() ? PAGE_ID_SLOT_DEF_1CH_YT_ON : PAGE_ID_SLOT_DEF_1CH_HORZ_OFF;
                } else {
                    return isVert ? PAGE_ID_SLOT_DEF_VERT_ERROR : PAGE_ID_SLOT_DEF_HORZ_ERROR;
                }
            } else if (numChannels == 2) {
                return isVert ? PAGE_ID_SLOT_DEF_2CH_VERT : PAGE_ID_SLOT_DEF_2CH_HORZ;
            } else {
                return isVert ? PAGE_ID_SLOT_DEF_VERT_ERROR : PAGE_ID_SLOT_DEF_HORZ_ERROR;
            }
        } else {
            return isVert ? PAGE_ID_SLOT_DEF_VERT_ERROR : PAGE_ID_SLOT_DEF_HORZ_ERROR;
        }
    } else {
        return isVert ? PAGE_ID_SLOT_DEF_VERT_NOT_INSTALLED : PAGE_ID_SLOT_DEF_HORZ_NOT_INSTALLED;
    }
}

void data_slot1_channel_index(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_SET_CONTEXT) {
        Channel &channel = Channel::getBySlotIndex(0);
        cursor.i = channel.channelIndex;
        value = data::Value(g_channel, VALUE_TYPE_POINTER);
        selectChannel(&channel);
    } else if (operation == data::DATA_OPERATION_GET_CONTEXT) {
        value = data::Value(g_channel, VALUE_TYPE_POINTER);
    } else if (operation == data::DATA_OPERATION_RESTORE_CONTEXT) {
        selectChannel((Channel *)value.getVoidPointer());
    }
}

void data_slot2_channel_index(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_SET_CONTEXT) {
        Channel &channel = Channel::getBySlotIndex(1);
        cursor.i = channel.channelIndex;
        value = data::Value(g_channel, VALUE_TYPE_POINTER);
        selectChannel(&channel);
    } else if (operation == data::DATA_OPERATION_GET_CONTEXT) {
        value = data::Value(g_channel, VALUE_TYPE_POINTER);
    } else if (operation == data::DATA_OPERATION_RESTORE_CONTEXT) {
        selectChannel((Channel *)value.getVoidPointer());
    }
}

void data_slot3_channel_index(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_SET_CONTEXT) {
        Channel &channel = Channel::getBySlotIndex(2);
        cursor.i = channel.channelIndex;
        value = data::Value(g_channel, VALUE_TYPE_POINTER);
        selectChannel(&channel);
    } else if (operation == data::DATA_OPERATION_GET_CONTEXT) {
        value = data::Value(g_channel, VALUE_TYPE_POINTER);
    } else if (operation == data::DATA_OPERATION_RESTORE_CONTEXT) {
        selectChannel((Channel *)value.getVoidPointer());
    }
}

void data_slot_default1_view(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        value = getDefaultView(cursor.i);
    }
}

void data_slot_default2_view(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        value = getDefaultView(cursor.i);
    }
}

void data_slot_default3_view(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        value = getDefaultView(cursor.i);
    }
}

void data_slot_max_channel_index(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_SET_CONTEXT) {
        Channel &channel = Channel::get(persist_conf::getMaxChannelIndex());
        cursor.i = channel.channelIndex;
        value = data::Value(g_channel, VALUE_TYPE_POINTER);
        selectChannel(&channel);
    } else if (operation == data::DATA_OPERATION_GET_CONTEXT) {
        value = data::Value(g_channel, VALUE_TYPE_POINTER);
    } else if (operation == data::DATA_OPERATION_RESTORE_CONTEXT) {
        selectChannel((Channel *)value.getVoidPointer());
    }
}

void data_slot_max_view(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        Channel &channel = Channel::get(cursor.i);
        if (channel.isInstalled()) {
            if (channel.isOk()) {
                int numChannels = g_slots[channel.slotIndex].moduleInfo->numChannels;
                if (numChannels == 1) {
                    if (persist_conf::devConf.channelsViewModeInMax == CHANNELS_VIEW_MODE_IN_MAX_NUMERIC) {
                        value = channel.isOutputEnabled() ? PAGE_ID_SLOT_MAX_1CH_NUM_ON : PAGE_ID_SLOT_MAX_1CH_NUM_OFF;
                    } else if (persist_conf::devConf.channelsViewModeInMax == CHANNELS_VIEW_MODE_IN_MAX_HORZ_BAR) {
                        value = channel.isOutputEnabled() ? PAGE_ID_SLOT_MAX_1CH_HBAR_ON : PAGE_ID_SLOT_MAX_1CH_HBAR_OFF;
                    } else if (persist_conf::devConf.channelsViewModeInMax == CHANNELS_VIEW_MODE_IN_MAX_YT) {
                        value = channel.isOutputEnabled() ? PAGE_ID_SLOT_MAX_1CH_YT_ON : PAGE_ID_SLOT_MAX_1CH_YT_OFF;
                    } else {
                        value = PAGE_ID_SLOT_MAX_ERROR;
                    }
                } else if (numChannels == 2) {
                    value = PAGE_ID_SLOT_MAX_2CH;
                } else {
                    value = PAGE_ID_SLOT_MAX_ERROR;
                }
            } else {
                value = PAGE_ID_SLOT_MAX_ERROR;
            }
        } else {
            value = PAGE_ID_SLOT_MAX_NOT_INSTALLED;
        }
    }
}

int getMinView(int channelIndex) {
    Channel &channel = Channel::get(channelIndex);
    if (channel.isInstalled()) {
        if (channel.isOk()) {
            int numChannels = g_slots[channel.slotIndex].moduleInfo->numChannels;
            if (numChannels == 1) {
                if (channel_dispatcher::getCouplingType() == channel_dispatcher::COUPLING_TYPE_SERIES && channel.channelIndex == 1) {
                    return PAGE_ID_SLOT_MIN_1CH_COUPLED_SERIES;
                } else if (channel_dispatcher::getCouplingType() == channel_dispatcher::COUPLING_TYPE_PARALLEL && channel.channelIndex == 1) {
                    return PAGE_ID_SLOT_MIN_1CH_COUPLED_PARALLEL;
                } else {
                    return channel.isOutputEnabled() ? PAGE_ID_SLOT_MIN_1CH_ON : PAGE_ID_SLOT_MIN_1CH_OFF;
                }
            } else if (numChannels == 2) {
                return PAGE_ID_SLOT_MIN_2CH;
            } else {
                return PAGE_ID_SLOT_MIN_ERROR;
            }
        } else {
            return PAGE_ID_SLOT_MIN_ERROR;
        }
    } else {
        return PAGE_ID_SLOT_MIN_NOT_INSTALLED;
    }
}

void data_slot_min1_channel_index(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_SET_CONTEXT) {
        Channel &channel = Channel::get(persist_conf::getMin1ChannelIndex());
        cursor.i = channel.channelIndex;
        value = data::Value(g_channel, VALUE_TYPE_POINTER);
        selectChannel(&channel);
    } else if (operation == data::DATA_OPERATION_GET_CONTEXT) {
        value = data::Value(g_channel, VALUE_TYPE_POINTER);
    } else if (operation == data::DATA_OPERATION_RESTORE_CONTEXT) {
        selectChannel((Channel *)value.getVoidPointer());
    }
}

void data_slot_min1_view(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        value = getMinView(cursor.i);
    }
}

void data_slot_min2_channel_index(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_SET_CONTEXT) {
        Channel &channel = Channel::get(persist_conf::getMin2ChannelIndex());
        cursor.i = channel.channelIndex;
        value = data::Value(g_channel, VALUE_TYPE_POINTER);
        selectChannel(&channel);
    } else if (operation == data::DATA_OPERATION_GET_CONTEXT) {
        value = data::Value(g_channel, VALUE_TYPE_POINTER);
    } else if (operation == data::DATA_OPERATION_RESTORE_CONTEXT) {
        selectChannel((Channel *)value.getVoidPointer());
    }
}

void data_slot_min2_view(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        value = getMinView(cursor.i);
    }
}

int getMicroView(int channelIndex) {
    Channel &channel = Channel::get(channelIndex);
    if (channel.isInstalled()) {
        if (channel.isOk()) {
            int numChannels = g_slots[channel.slotIndex].moduleInfo->numChannels;
            if (numChannels == 1) {
                if (channel_dispatcher::getCouplingType() == channel_dispatcher::COUPLING_TYPE_SERIES && channel.channelIndex == 1) {
                    return PAGE_ID_SLOT_MICRO_1CH_COUPLED_SERIES;
                } else if (channel_dispatcher::getCouplingType() == channel_dispatcher::COUPLING_TYPE_PARALLEL && channel.channelIndex == 1) {
                    return PAGE_ID_SLOT_MICRO_1CH_COUPLED_PARALLEL;
                } else {
                    return channel.isOutputEnabled() ? PAGE_ID_SLOT_MICRO_1CH_ON : PAGE_ID_SLOT_MICRO_1CH_OFF;
                }
            } else if (numChannels == 2) {
                return PAGE_ID_SLOT_MICRO_2CH;
            } else {
                return PAGE_ID_SLOT_MICRO_ERROR;
            }
        } else {
            return PAGE_ID_SLOT_MICRO_ERROR;
        }
    } else {
        return PAGE_ID_SLOT_MICRO_NOT_INSTALLED;
    }
}

void data_slot_micro1_view(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        value = getMicroView(cursor.i);
    }
}

void data_slot_micro2_view(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        value = getMicroView(cursor.i);
    }
}

void data_slot_micro3_view(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        value = getMicroView(cursor.i);
    }
}

void data_slot_2ch_ch1_index(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_SET_CONTEXT) {
        Channel &channel = Channel::get(cursor.i);
        value = data::Value(g_channel, VALUE_TYPE_POINTER);
        selectChannel(&channel);
    } else if (operation == data::DATA_OPERATION_GET_CONTEXT) {
        value = data::Value(g_channel, VALUE_TYPE_POINTER);
    } else if (operation == data::DATA_OPERATION_RESTORE_CONTEXT) {
        selectChannel((Channel *)value.getVoidPointer());
    }
}

void data_slot_2ch_ch2_index(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_SET_CONTEXT) {
        if (cursor.i == persist_conf::getMaxChannelIndex() && Channel::get(cursor.i).subchannelIndex == 1) {
            cursor.i = cursor.i - 1;
        } else {
            cursor.i = cursor.i + 1;
        }
        Channel &channel = Channel::get(cursor.i);
        value = data::Value(g_channel, VALUE_TYPE_POINTER);
        selectChannel(&channel);
    } else if (operation == data::DATA_OPERATION_GET_CONTEXT) {
        value = data::Value(g_channel, VALUE_TYPE_POINTER);
    } else if (operation == data::DATA_OPERATION_RESTORE_CONTEXT) {
        selectChannel((Channel *)value.getVoidPointer());
    }
}

void data_slot_def_2ch_view(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        Channel &channel = Channel::get(cursor.i);
        int isVert = persist_conf::devConf.channelsViewMode == CHANNELS_VIEW_MODE_NUMERIC || persist_conf::devConf.channelsViewMode == CHANNELS_VIEW_MODE_VERT_BAR;
        value = channel.isOutputEnabled() ? 
            (isVert ? PAGE_ID_SLOT_DEF_2CH_VERT_ON : PAGE_ID_SLOT_DEF_2CH_HORZ_ON) :
            (isVert ? PAGE_ID_SLOT_DEF_2CH_VERT_OFF : PAGE_ID_SLOT_DEF_2CH_HORZ_OFF);
    }
}

void data_slot_max_2ch_view(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        Channel &channel = Channel::get(cursor.i);

        if (persist_conf::devConf.channelsViewModeInMax == CHANNELS_VIEW_MODE_IN_MAX_NUMERIC) {
            value = channel.isOutputEnabled() ? PAGE_ID_SLOT_MAX_2CH_NUM_ON : PAGE_ID_SLOT_MAX_2CH_NUM_OFF;
        } else if (persist_conf::devConf.channelsViewModeInMax == CHANNELS_VIEW_MODE_IN_MAX_HORZ_BAR) {
            value = channel.isOutputEnabled() ? PAGE_ID_SLOT_MAX_2CH_HBAR_ON : PAGE_ID_SLOT_MAX_2CH_HBAR_OFF;
        } else {
            value = channel.isOutputEnabled() ? PAGE_ID_SLOT_MAX_2CH_YT_ON : PAGE_ID_SLOT_MAX_2CH_YT_OFF;
        }
    }
}

void data_slot_max_2ch_min_view(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        Channel &channel = Channel::get(cursor.i);
        value = channel.isOutputEnabled() ? PAGE_ID_SLOT_MAX_2CH_MIN_ON : PAGE_ID_SLOT_MAX_2CH_MIN_OFF;
    }
}


void data_slot_min_2ch_view(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        Channel &channel = Channel::get(cursor.i);
        value = channel.isOutputEnabled() ? PAGE_ID_SLOT_MIN_2CH_ON : PAGE_ID_SLOT_MIN_2CH_OFF;
    }
}

void data_slot_micro_2ch_view(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        Channel &channel = Channel::get(cursor.i);
        value = channel.isOutputEnabled() ? PAGE_ID_SLOT_MICRO_2CH_ON : PAGE_ID_SLOT_MICRO_2CH_OFF;
    }
}

void data_channel_display_value1(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    int iChannel = cursor.i >= 0 ? cursor.i : (g_channel ? g_channel->channelIndex : 0);
    Channel &channel = Channel::get(iChannel);
    if (channel.flags.displayValue1 == DISPLAY_VALUE_VOLTAGE) {
        data_channel_u_mon(operation, cursor, value);
    } else if (channel.flags.displayValue1 == DISPLAY_VALUE_CURRENT) {
        data_channel_i_mon(operation, cursor, value);
    } else if (channel.flags.displayValue1 == DISPLAY_VALUE_POWER) {
        data_channel_p_mon(operation, cursor, value);
    }
}

void data_channel_display_value2(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    int iChannel = cursor.i >= 0 ? cursor.i : (g_channel ? g_channel->channelIndex : 0);
    Channel &channel = Channel::get(iChannel);
    if (channel.flags.displayValue2 == DISPLAY_VALUE_VOLTAGE) {
        data_channel_u_mon(operation, cursor, value);
    } else if (channel.flags.displayValue2 == DISPLAY_VALUE_CURRENT) {
        data_channel_i_mon(operation, cursor, value);
    } else if (channel.flags.displayValue2 == DISPLAY_VALUE_POWER) {
        data_channel_p_mon(operation, cursor, value);
    }
}

void data_ovp(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    int iChannel = cursor.i >= 0 ? cursor.i : (g_channel ? g_channel->channelIndex : 0);
    Channel &channel = Channel::get(iChannel);
    if (operation == data::DATA_OPERATION_GET) {
        if (!channel.prot_conf.flags.u_state) {
            value = 0;
        } else if (!channel_dispatcher::isOvpTripped(channel)) {
            value = 1;
        } else {
            value = 2;
        }
    }
}

void data_ocp(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    int iChannel = cursor.i >= 0 ? cursor.i : (g_channel ? g_channel->channelIndex : 0);
    Channel &channel = Channel::get(iChannel);
    if (operation == data::DATA_OPERATION_GET) {
        if (!channel.prot_conf.flags.i_state) {
            value = 0;
        } else if (!channel_dispatcher::isOcpTripped(channel)) {
            value = 1;
        } else {
            value = 2;
        }
    }
}

void data_opp(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    int iChannel = cursor.i >= 0 ? cursor.i : (g_channel ? g_channel->channelIndex : 0);
    Channel &channel = Channel::get(iChannel);
    if (operation == data::DATA_OPERATION_GET) {
        if (!channel.prot_conf.flags.p_state) {
            value = 0;
        } else if (!channel_dispatcher::isOppTripped(channel)) {
            value = 1;
        } else {
            value = 2;
        }
    }
}

void data_otp_ch(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    int iChannel = cursor.i >= 0 ? cursor.i : (g_channel ? g_channel->channelIndex : 0);
    Channel &channel = Channel::get(iChannel);
    if (operation == data::DATA_OPERATION_GET) {
        temperature::TempSensorTemperature &tempSensor = temperature::sensors[temp_sensor::CH1 + iChannel];
        if (!tempSensor.isInstalled() || !tempSensor.isTestOK() || !tempSensor.prot_conf.state) {
            value = 0;
        } else if (!channel_dispatcher::isOtpTripped(channel)) {
            value = 1;
        } else {
            value = 2;
        }
    }
}

void data_otp_aux(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        temperature::TempSensorTemperature &tempSensor = temperature::sensors[temp_sensor::AUX];
        if (!tempSensor.prot_conf.state) {
            value = 0;
        } else if (!tempSensor.isTripped()) {
            value = 1;
        } else {
            value = 2;
        }
    }
}

void data_edit_value(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        value = edit_mode::getEditValue();
    } else if (operation == data::DATA_OPERATION_GET_MIN) {
        value = edit_mode::getMin();
    } else if (operation == data::DATA_OPERATION_GET_MAX) {
        value = edit_mode::getMax();
    } else if (operation == data::DATA_OPERATION_IS_BLINKING) {
        value = !edit_mode::isInteractiveMode() &&
                (edit_mode::getEditValue() != edit_mode::getCurrentValue());
    }
}

void data_edit_unit(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        Keypad *keypad = getActiveKeypad();
        if (keypad) {
            value = getUnitName(keypad->getSwitchToUnit());
        }
    }
}

void data_edit_info(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        value = data::Value(0, VALUE_TYPE_EDIT_INFO);
    }
}

void data_edit_mode_interactive_mode_selector(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        value = edit_mode::isInteractiveMode() ? 0 : 1;
    }
}

void data_edit_steps(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        value = edit_mode_step::getStepIndex();
    } else if (operation == data::DATA_OPERATION_GET_VALUE_LIST) {
        value = MakeValueListValue(edit_mode_step::getStepValues());
    } else if (operation == data::DATA_OPERATION_COUNT) {
        value = edit_mode_step::getStepValuesCount();
    } else if (operation == data::DATA_OPERATION_SET) {
        edit_mode_step::setStepIndex(value.getInt());
    }
}

void data_firmware_info(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        static const char FIRMWARE_LABEL[] = "Firmware: ";
        static char firmware_info[sizeof(FIRMWARE_LABEL) - 1 + sizeof(FIRMWARE) - 1 + 1];

        if (*firmware_info == 0) {
            strcat(firmware_info, FIRMWARE_LABEL);
            strcat(firmware_info, FIRMWARE);
        }

        value = firmware_info;
    }
}

void data_self_test_result(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        SelfTestResultPage *page = (SelfTestResultPage *)getPage(PAGE_ID_SELF_TEST_RESULT);
        if (page) {
            value = page->m_selfTestResult;
        }
    }
}

void data_keypad_text(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        Keypad *keypad = getActiveKeypad();
        if (keypad) {
            value = keypad->getKeypadTextValue();
        }
    }
}

void data_keypad_caps(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        Keypad *keypad = getActiveKeypad();
        if (keypad) {
            value = keypad->m_isUpperCase;
        }
    }
}

void data_keypad_option1_text(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        NumericKeypad *keypad = getActiveNumericKeypad();
        if (keypad) {
            value = data::Value(keypad->m_options.flags.option1ButtonEnabled ? keypad->m_options.option1ButtonText : "");
        }
    }
}

void data_keypad_option1_enabled(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        NumericKeypad *keypad = getActiveNumericKeypad();
        if (keypad) {
            value = (int)keypad->m_options.flags.option1ButtonEnabled;
        }
    }
}

void data_keypad_option2_text(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        NumericKeypad *keypad = getActiveNumericKeypad();
        if (keypad) {
            value = data::Value(keypad->m_options.flags.option2ButtonEnabled ? keypad->m_options.option2ButtonText : "");
        }
    }
}

void data_keypad_option2_enabled(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        NumericKeypad *keypad = getActiveNumericKeypad();
        if (keypad) {
            value = (int)keypad->m_options.flags.option2ButtonEnabled;
        }
    }
}

void data_keypad_sign_enabled(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        NumericKeypad *keypad = getActiveNumericKeypad();
        if (keypad) {
            value = (int)keypad->m_options.flags.signButtonEnabled;
        }
    }
}

void data_keypad_dot_enabled(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        NumericKeypad *keypad = getActiveNumericKeypad();
        if (keypad) {
            value = (int)keypad->m_options.flags.dotButtonEnabled;
        }
    }
}

void data_keypad_unit_enabled(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        NumericKeypad *keypad = getActiveNumericKeypad();
        if (keypad) {
            value = keypad->m_options.editValueUnit == UNIT_VOLT ||
                    keypad->m_options.editValueUnit == UNIT_MILLI_VOLT ||
                    keypad->m_options.editValueUnit == UNIT_AMPER ||
                    keypad->m_options.editValueUnit == UNIT_MILLI_AMPER ||
                    keypad->m_options.editValueUnit == UNIT_MICRO_AMPER ||
                    keypad->m_options.editValueUnit == UNIT_WATT ||
                    keypad->m_options.editValueUnit == UNIT_MILLI_WATT ||
                    keypad->m_options.editValueUnit == UNIT_SECOND ||
                    keypad->m_options.editValueUnit == UNIT_MILLI_SECOND;
        }
    }
}

void data_channel_off_label(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        value = io_pins::isInhibited() ? "INH" : "OFF";
    }
}

void data_channel_label(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        int iChannel = cursor.i >= 0 ? cursor.i : (g_channel ? g_channel->channelIndex : 0);
        value = data::Value(iChannel + 1, VALUE_TYPE_CHANNEL_LABEL);
    }
}

void data_channel_short_label(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        int iChannel = cursor.i >= 0 ? cursor.i : (g_channel ? g_channel->channelIndex : 0);
        value = data::Value(iChannel + 1, VALUE_TYPE_CHANNEL_SHORT_LABEL);
    }
}

void data_channel_title(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        int iChannel = cursor.i >= 0 ? cursor.i : (g_channel ? g_channel->channelIndex : 0);
        value = data::Value(iChannel, VALUE_TYPE_CHANNEL_TITLE);
    }
}

void data_channel_short_title(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        int iChannel = cursor.i >= 0 ? cursor.i : (g_channel ? g_channel->channelIndex : 0);
        value = data::Value(iChannel, VALUE_TYPE_CHANNEL_SHORT_TITLE);
    }
}

void data_channel_short_title_without_tracking_icon(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        int iChannel = cursor.i >= 0 ? cursor.i : (g_channel ? g_channel->channelIndex : 0);
        value = data::Value(iChannel, VALUE_TYPE_CHANNEL_SHORT_TITLE_WITHOUT_TRACKING_ICON);
    }
}

void data_channel_short_title_with_colon(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        int iChannel = cursor.i >= 0 ? cursor.i : (g_channel ? g_channel->channelIndex : 0);
        value = data::Value(iChannel, VALUE_TYPE_CHANNEL_SHORT_TITLE_WITH_COLON);
    }
}

void data_channel_long_title(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        int iChannel = cursor.i >= 0 ? cursor.i : (g_channel ? g_channel->channelIndex : 0);
        value = data::Value(iChannel, VALUE_TYPE_CHANNEL_LONG_TITLE);
    }
}

void data_channel_temp_status(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        int iChannel = cursor.i >= 0 ? cursor.i : (g_channel ? g_channel->channelIndex : 0);
        temperature::TempSensorTemperature &tempSensor = temperature::sensors[temp_sensor::CH1 + iChannel];
        if (tempSensor.isInstalled()) {
            if (tempSensor.isTestOK()) {
                value = 1;
            } else {
                value = 0;
            }
        } else {
            value = 2;
        }
    }
}

void data_channel_temp(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        float temperature = 0;

        int iChannel = cursor.i >= 0 ? cursor.i : (g_channel ? g_channel->channelIndex : 0);
        temperature::TempSensorTemperature &tempSensor = temperature::sensors[temp_sensor::CH1 + iChannel];
        if (tempSensor.isInstalled() && tempSensor.isTestOK()) {
            temperature = tempSensor.temperature;
        }

        value = MakeValue(temperature, UNIT_CELSIUS);
    }
}

void data_channel_on_time_total(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    int iChannel = cursor.i >= 0 ? cursor.i : (g_channel ? g_channel->channelIndex : 0);
    Channel &channel = Channel::get(iChannel);
    if (operation == data::DATA_OPERATION_GET) {
        value =
            data::Value((uint32_t)ontime::g_moduleCounters[channel.slotIndex].getTotalTime(), VALUE_TYPE_ON_TIME_COUNTER);
    }
}

void data_channel_on_time_last(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    int iChannel = cursor.i >= 0 ? cursor.i : (g_channel ? g_channel->channelIndex : 0);
    Channel &channel = Channel::get(iChannel);
    if (operation == data::DATA_OPERATION_GET) {
        value =
            data::Value((uint32_t)ontime::g_moduleCounters[channel.slotIndex].getLastTime(), VALUE_TYPE_ON_TIME_COUNTER);
    }
}

void data_channel_calibration_status(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        int iChannel = cursor.i >= 0 ? cursor.i : (g_channel ? g_channel->channelIndex : 0);
        Channel &channel = Channel::get(iChannel);
        value = channel.isCalibrationExists();
    }
}

void data_channel_calibration_state(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        int iChannel = cursor.i >= 0 ? cursor.i : (g_channel ? g_channel->channelIndex : 0);
        Channel &channel = Channel::get(iChannel);
        value = channel.isCalibrationEnabled();
    }
}

void data_channel_calibration_date(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        int iChannel = cursor.i >= 0 ? cursor.i : (g_channel ? g_channel->channelIndex : 0);
        Channel &channel = Channel::get(iChannel);
        value = data::Value(channel.cal_conf.calibration_date);
    }
}

void data_channel_calibration_remark(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        int iChannel = cursor.i >= 0 ? cursor.i : (g_channel ? g_channel->channelIndex : 0);
        Channel &channel = Channel::get(iChannel);
        value = data::Value(channel.cal_conf.calibration_remark);
    }
}

void data_channel_calibration_step_is_set_remark_step(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        value = calibration_wizard::g_stepNum == calibration_wizard::MAX_STEP_NUM - 1;
    }
}

void data_channel_calibration_step_num(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        value = data::Value(calibration_wizard::g_stepNum);
    }
}

void data_channel_calibration_step_status(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        switch (calibration_wizard::g_stepNum) {
        case 0:
            value = psu::calibration::getVoltage().min_set;
            break;
        case 1:
            value = psu::calibration::getVoltage().mid_set;
            break;
        case 2:
            value = psu::calibration::getVoltage().max_set;
            break;
        case 3:
        case 6:
            value = psu::calibration::getCurrent().min_set;
            break;
        case 4:
        case 7:
            value = psu::calibration::getCurrent().mid_set;
            break;
        case 5:
        case 8:
            value = psu::calibration::getCurrent().max_set;
            break;
        case 9:
            value = psu::calibration::isRemarkSet();
            break;
        }
    }
}

void data_channel_calibration_step_level_value(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        value = calibration_wizard::getLevelValue();
    }
}

void data_channel_calibration_step_value(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        switch (calibration_wizard::g_stepNum) {
        case 0:
            value = MakeValue(psu::calibration::getVoltage().min_val, UNIT_VOLT);
            break;
        case 1:
            value = MakeValue(psu::calibration::getVoltage().mid_val, UNIT_VOLT);
            break;
        case 2:
            value = MakeValue(psu::calibration::getVoltage().max_val, UNIT_VOLT);
            break;
        case 3:
            value = MakeValue(psu::calibration::getCurrent().min_val, UNIT_AMPER);
            break;
        case 4:
            value = MakeValue(psu::calibration::getCurrent().mid_val, UNIT_AMPER);
            break;
        case 5:
            value = MakeValue(psu::calibration::getCurrent().max_val, UNIT_AMPER);
            break;
        case 6:
            value = MakeValue(psu::calibration::getCurrent().min_val, UNIT_AMPER);
            break;
        case 7:
            value = MakeValue(psu::calibration::getCurrent().mid_val, UNIT_AMPER);
            break;
        case 8:
            value = MakeValue(psu::calibration::getCurrent().max_val, UNIT_AMPER);
            break;
        case 9:
            value = data::Value(psu::calibration::getRemark());
            break;
        }
    }
}

void data_channel_calibration_step_prev_enabled(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        value = calibration_wizard::g_stepNum > 0;
    }
}

void data_channel_calibration_step_next_enabled(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        value = calibration_wizard::g_stepNum < calibration_wizard::MAX_STEP_NUM;
    }
}

void data_cal_ch_u_min(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        int iChannel = cursor.i >= 0 ? cursor.i : (g_channel ? g_channel->channelIndex : 0);
        Channel &channel = Channel::get(iChannel);
        value = MakeValue(channel.cal_conf.u.min.val, UNIT_VOLT);
    }
}

void data_cal_ch_u_mid(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        int iChannel = cursor.i >= 0 ? cursor.i : (g_channel ? g_channel->channelIndex : 0);
        Channel &channel = Channel::get(iChannel);
        value = MakeValue(channel.cal_conf.u.mid.val, UNIT_VOLT);
    }
}

void data_cal_ch_u_max(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        int iChannel = cursor.i >= 0 ? cursor.i : (g_channel ? g_channel->channelIndex : 0);
        Channel &channel = Channel::get(iChannel);
        value = MakeValue(channel.cal_conf.u.max.val, UNIT_VOLT);
    }
}

void data_cal_ch_i0_min(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        int iChannel = cursor.i >= 0 ? cursor.i : (g_channel ? g_channel->channelIndex : 0);
        Channel &channel = Channel::get(iChannel);
        value = MakeValue(channel.cal_conf.i[0].min.val, UNIT_AMPER);
    }
}

void data_cal_ch_i0_mid(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        int iChannel = cursor.i >= 0 ? cursor.i : (g_channel ? g_channel->channelIndex : 0);
        Channel &channel = Channel::get(iChannel);
        value = MakeValue(channel.cal_conf.i[0].mid.val, UNIT_AMPER);
    }
}

void data_cal_ch_i0_max(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        int iChannel = cursor.i >= 0 ? cursor.i : (g_channel ? g_channel->channelIndex : 0);
        Channel &channel = Channel::get(iChannel);
        value = MakeValue(channel.cal_conf.i[0].max.val, UNIT_AMPER);
    }
}

void data_cal_ch_i1_min(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        int iChannel = cursor.i >= 0 ? cursor.i : (g_channel ? g_channel->channelIndex : 0);
        Channel &channel = Channel::get(iChannel);
        if (channel.hasSupportForCurrentDualRange()) {
            value = MakeValue(channel.cal_conf.i[1].min.val, UNIT_AMPER);
        } else {
            value = data::Value("");
        }
    }
}

void data_cal_ch_i1_mid(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        int iChannel = cursor.i >= 0 ? cursor.i : (g_channel ? g_channel->channelIndex : 0);
        Channel &channel = Channel::get(iChannel);
        if (channel.hasSupportForCurrentDualRange()) {
            value = MakeValue(channel.cal_conf.i[1].mid.val, UNIT_AMPER);
        } else {
            value = data::Value("");
        }
    }
}

void data_cal_ch_i1_max(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        int iChannel = cursor.i >= 0 ? cursor.i : (g_channel ? g_channel->channelIndex : 0);
        Channel &channel = Channel::get(iChannel);
        if (channel.hasSupportForCurrentDualRange()) {
            value = MakeValue(channel.cal_conf.i[1].max.val, UNIT_AMPER);
        } else {
            value = data::Value("");
        }
    }
}

void data_channel_protection_status(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        int iChannel = cursor.i >= 0 ? cursor.i : (g_channel ? g_channel->channelIndex : 0);
        Channel &channel = Channel::get(iChannel);

        bool ovp = channel_dispatcher::isOvpTripped(channel);
        bool ocp = channel_dispatcher::isOcpTripped(channel);
        bool opp = channel_dispatcher::isOppTripped(channel);
        bool otp = channel_dispatcher::isOtpTripped(channel);

        if (!ovp && !ocp && !opp && !otp) {
            value = 0;
        } else if (ovp && !ocp && !opp && !otp) {
            value = 1;
        } else if (!ovp && ocp && !opp && !otp) {
            value = 2;
        } else if (!ovp && !ocp && opp && !otp) {
            value = 3;
        } else if (!ovp && !ocp && !opp && otp) {
            value = 4;
        } else {
            value = 5;
        }
    }
}

void data_channel_protection_ovp_state(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        ChSettingsProtectionSetPage *page = (ChSettingsProtectionSetPage *)getPage(PAGE_ID_CH_SETTINGS_PROT_OVP);
        if (page) {
            value = page->state;
        }
    }
}

void data_channel_protection_ovp_type(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        int iChannel = cursor.i >= 0 ? cursor.i : (g_channel ? g_channel->channelIndex : 0);
        Channel &channel = Channel::get(iChannel);
        if (channel.params.features & CH_FEATURE_HW_OVP) {
            ChSettingsProtectionSetPage *page = (ChSettingsProtectionSetPage *)getPage(PAGE_ID_CH_SETTINGS_PROT_OVP);
            if (page) {
                value = page->type ? 0 : 1;
            } else {
                value = channel.prot_conf.flags.u_type ? 0 : 1;
            }
        } else {
            value = 2;
        }
    }
}

void data_channel_protection_ovp_level(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    int iChannel = cursor.i >= 0 ? cursor.i : (g_channel ? g_channel->channelIndex : 0);
    Channel &channel = Channel::get(iChannel);
    if (operation == data::DATA_OPERATION_GET) {
        ChSettingsProtectionSetPage *page = (ChSettingsProtectionSetPage *)getPage(PAGE_ID_CH_SETTINGS_PROT_OVP);
        if (page) {
            value = page->level;
        } else {
            bool focused = g_focusCursor == cursor && g_focusDataId == DATA_ID_CHANNEL_PROTECTION_OVP_LEVEL;
            if (focused && g_focusEditValue.getType() != VALUE_TYPE_NONE) {
                value = g_focusEditValue;
            } else {
                value = MakeValue(channel.prot_conf.u_level, UNIT_VOLT);
            }
        }
    } else if (operation == data::DATA_OPERATION_GET_MIN) {
        value = MakeValue(channel_dispatcher::getUMin(channel), UNIT_VOLT);
    } else if (operation == data::DATA_OPERATION_GET_MAX) {
        value = MakeValue(channel_dispatcher::getUMaxOvpLevel(channel), UNIT_VOLT);
    } else if (operation == data::DATA_OPERATION_SET) {
        channel_dispatcher::setOvpLevel(channel, value.getFloat());
    }
}

void data_channel_protection_ovp_delay(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    int iChannel = cursor.i >= 0 ? cursor.i : (g_channel ? g_channel->channelIndex : 0);
    Channel &channel = Channel::get(iChannel);
    if (operation == data::DATA_OPERATION_GET) {
        ChSettingsProtectionSetPage *page = (ChSettingsProtectionSetPage *)getPage(PAGE_ID_CH_SETTINGS_PROT_OVP);
        if (page) {
            value = page->delay;
        } else {
            bool focused = g_focusCursor == cursor && g_focusDataId == DATA_ID_CHANNEL_PROTECTION_OVP_DELAY;
            if (focused && g_focusEditValue.getType() != VALUE_TYPE_NONE) {
                value = g_focusEditValue;
            } else {
                value = MakeValue(channel.prot_conf.u_delay, UNIT_SECOND);
            }
        }
    } else if (operation == data::DATA_OPERATION_GET_MIN) {
        value = MakeValue(channel.params.OVP_MIN_DELAY, UNIT_SECOND);
    } else if (operation == data::DATA_OPERATION_GET_MAX) {
        value = MakeValue(channel.params.OVP_MAX_DELAY, UNIT_SECOND);
    } else if (operation == data::DATA_OPERATION_SET) {
        channel_dispatcher::setOvpDelay(channel, value.getFloat());
    } else if (operation == data::DATA_OPERATION_GET_NAME) {
        value = "OVP Delay";
    } else if (operation == data::DATA_OPERATION_GET_UNIT) {
        value = UNIT_SECOND;
    } 
}

void data_channel_protection_ovp_limit(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    int iChannel = cursor.i >= 0 ? cursor.i : (g_channel ? g_channel->channelIndex : 0);
    Channel &channel = Channel::get(iChannel);
    if (operation == data::DATA_OPERATION_GET) {
        ChSettingsProtectionSetPage *page = (ChSettingsProtectionSetPage *)getPage(PAGE_ID_CH_SETTINGS_PROT_OVP);
        if (page) {
            value = page->limit;
        } else {
            bool focused = g_focusCursor == cursor && g_focusDataId == DATA_ID_CHANNEL_PROTECTION_OVP_LIMIT;
            if (focused && g_focusEditValue.getType() != VALUE_TYPE_NONE) {
                value = g_focusEditValue;
            } else {
                value = MakeValue(channel_dispatcher::getULimit(channel), UNIT_VOLT);
            }
        }
    } else if (operation == data::DATA_OPERATION_GET_MIN) {
        value = MakeValue(channel_dispatcher::getUMin(channel), UNIT_VOLT);
    } else if (operation == data::DATA_OPERATION_GET_MAX) {
        value = MakeValue(channel_dispatcher::getUMax(channel), UNIT_VOLT);
    } else if (operation == data::DATA_OPERATION_SET) {
        channel_dispatcher::setVoltageLimit(channel, value.getFloat());
    } else if (operation == data::DATA_OPERATION_GET_NAME) {
        value = "OVP Limit";
    } else if (operation == data::DATA_OPERATION_GET_UNIT) {
        value = UNIT_VOLT;
    } 
}

void data_channel_protection_ocp_state(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        ChSettingsProtectionSetPage *page = (ChSettingsProtectionSetPage *)getPage(PAGE_ID_CH_SETTINGS_PROT_OCP);
        if (page) {
            value = page->state;
        }
    }
}

void data_channel_protection_ocp_delay(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    int iChannel = cursor.i >= 0 ? cursor.i : (g_channel ? g_channel->channelIndex : 0);
    Channel &channel = Channel::get(iChannel);
    if (operation == data::DATA_OPERATION_GET) {
        ChSettingsProtectionSetPage *page = (ChSettingsProtectionSetPage *)getPage(PAGE_ID_CH_SETTINGS_PROT_OCP);
        if (page) {
            value = page->delay;
        } else {
            bool focused = g_focusCursor == cursor && g_focusDataId == DATA_ID_CHANNEL_PROTECTION_OCP_DELAY;
            if (focused && g_focusEditValue.getType() != VALUE_TYPE_NONE) {
                value = g_focusEditValue;
            } else {
                value = MakeValue(channel.prot_conf.i_delay, UNIT_SECOND);
            }
        }
    } else if (operation == data::DATA_OPERATION_GET_MIN) {
        value = MakeValue(channel.params.OCP_MIN_DELAY, UNIT_SECOND);
    } else if (operation == data::DATA_OPERATION_GET_MAX) {
        value = MakeValue(channel.params.OCP_MAX_DELAY, UNIT_SECOND);
    } else if (operation == data::DATA_OPERATION_SET) {
        channel_dispatcher::setOcpParameters(channel, channel.prot_conf.flags.i_state ? 1 : 0, value.getFloat());
    } else if (operation == data::DATA_OPERATION_GET_NAME) {
        value = "OCP Delay";
    } else if (operation == data::DATA_OPERATION_GET_UNIT) {
        value = UNIT_SECOND;
    } 
}

void data_channel_protection_ocp_limit(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    int iChannel = cursor.i >= 0 ? cursor.i : (g_channel ? g_channel->channelIndex : 0);
    Channel &channel = Channel::get(iChannel);
    if (operation == data::DATA_OPERATION_GET) {
        ChSettingsProtectionSetPage *page = (ChSettingsProtectionSetPage *)getPage(PAGE_ID_CH_SETTINGS_PROT_OCP);
        if (page) {
            value = page->limit;
        } else {
            bool focused = g_focusCursor == cursor && g_focusDataId == DATA_ID_CHANNEL_PROTECTION_OCP_LIMIT;
            if (focused && g_focusEditValue.getType() != VALUE_TYPE_NONE) {
                value = g_focusEditValue;
            } else {
                value = MakeValue(channel_dispatcher::getILimit(channel), UNIT_AMPER);
            }
        }
    } else if (operation == data::DATA_OPERATION_GET_MIN) {
        value = MakeValue(channel_dispatcher::getIMin(channel), UNIT_AMPER);
    } else if (operation == data::DATA_OPERATION_GET_MAX) {
        value = MakeValue(channel_dispatcher::getIMax(channel), UNIT_AMPER);
    } else if (operation == data::DATA_OPERATION_SET) {
        channel_dispatcher::setCurrentLimit(channel, value.getFloat());
    } else if (operation == data::DATA_OPERATION_GET_NAME) {
        value = "OCP Limit";
    } else if (operation == data::DATA_OPERATION_GET_UNIT) {
        value = UNIT_AMPER;
    }
}

void data_channel_protection_ocp_max_current_limit_cause(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        value = data::Value(g_channel->getMaxCurrentLimitCause());
    }
}

void data_channel_protection_opp_state(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        ChSettingsProtectionSetPage *page = (ChSettingsProtectionSetPage *)getPage(PAGE_ID_CH_SETTINGS_PROT_OPP);
        if (page) {
            value = page->state;
        }
    }
}

void data_channel_protection_opp_level(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    int iChannel = cursor.i >= 0 ? cursor.i : (g_channel ? g_channel->channelIndex : 0);
    Channel &channel = Channel::get(iChannel);
    if (operation == data::DATA_OPERATION_GET) {
        ChSettingsProtectionSetPage *page = (ChSettingsProtectionSetPage *)getPage(PAGE_ID_CH_SETTINGS_PROT_OPP);
        if (page) {
            value = page->level;
        } else {
            bool focused = g_focusCursor == cursor && g_focusDataId == DATA_ID_CHANNEL_PROTECTION_OPP_LEVEL;
            if (focused && g_focusEditValue.getType() != VALUE_TYPE_NONE) {
                value = g_focusEditValue;
            } else {
                value = MakeValue(channel_dispatcher::getOppLevel(channel), UNIT_WATT);
            }
        }
    } else if (operation == data::DATA_OPERATION_GET_MIN) {
        value = MakeValue(channel_dispatcher::getOppMinLevel(channel), UNIT_WATT);
    } else if (operation == data::DATA_OPERATION_GET_MAX) {
        value = MakeValue(channel_dispatcher::getOppMaxLevel(channel), UNIT_WATT);
    } else if (operation == data::DATA_OPERATION_SET) {
        channel_dispatcher::setOppParameters(channel, channel.prot_conf.flags.p_state ? 1 : 0, value.getFloat(), channel.prot_conf.p_delay);
    } else if (operation == data::DATA_OPERATION_GET_NAME) {
        value = "OPP Level";
    } else if (operation == data::DATA_OPERATION_GET_UNIT) {
        value = UNIT_WATT;
    } 
}

void data_channel_protection_opp_delay(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    int iChannel = cursor.i >= 0 ? cursor.i : (g_channel ? g_channel->channelIndex : 0);
    Channel &channel = Channel::get(iChannel);
    if (operation == data::DATA_OPERATION_GET) {
        ChSettingsProtectionSetPage *page = (ChSettingsProtectionSetPage *)getPage(PAGE_ID_CH_SETTINGS_PROT_OPP);
        if (page) {
            value = page->delay;
        } else {
            bool focused = g_focusCursor == cursor && g_focusDataId == DATA_ID_CHANNEL_PROTECTION_OPP_DELAY;
            if (focused && g_focusEditValue.getType() != VALUE_TYPE_NONE) {
                value = g_focusEditValue;
            } else {
                value = MakeValue(channel.prot_conf.p_delay, UNIT_SECOND);
            }
        }
    } else if (operation == data::DATA_OPERATION_GET_MIN) {
        value = MakeValue(channel.params.OPP_MIN_DELAY, UNIT_SECOND);
    } else if (operation == data::DATA_OPERATION_GET_MAX) {
        value = MakeValue(channel.params.OPP_MAX_DELAY, UNIT_SECOND);
    } else if (operation == data::DATA_OPERATION_SET) {
        channel_dispatcher::setOppParameters(channel, channel.prot_conf.flags.p_state ? 1 : 0, channel_dispatcher::getPowerProtectionLevel(channel), value.getFloat());
    } else if (operation == data::DATA_OPERATION_GET_NAME) {
        value = "OPP Delay";
    } else if (operation == data::DATA_OPERATION_GET_UNIT) {
        value = UNIT_SECOND;
    } 
}

void data_channel_protection_opp_limit(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    int iChannel = cursor.i >= 0 ? cursor.i : (g_channel ? g_channel->channelIndex : 0);
    Channel &channel = Channel::get(iChannel);
    if (operation == data::DATA_OPERATION_GET) {
        ChSettingsProtectionSetPage *page = (ChSettingsProtectionSetPage *)getPage(PAGE_ID_CH_SETTINGS_PROT_OPP);
        if (page) {
            value = page->limit;
        } else {
            bool focused = g_focusCursor == cursor && g_focusDataId == DATA_ID_CHANNEL_PROTECTION_OPP_LIMIT;
            if (focused && g_focusEditValue.getType() != VALUE_TYPE_NONE) {
                value = g_focusEditValue;
            } else {
                value = MakeValue(channel_dispatcher::getPowerLimit(channel), UNIT_WATT);
            }
        }
    } else if (operation == data::DATA_OPERATION_GET_MIN) {
        value = MakeValue(channel_dispatcher::getPowerMinLimit(channel), UNIT_WATT);
    } else if (operation == data::DATA_OPERATION_GET_MAX) {
        value = MakeValue(channel_dispatcher::getPowerMaxLimit(channel), UNIT_WATT);
    } else if (operation == data::DATA_OPERATION_SET) {
        channel_dispatcher::setPowerLimit(channel, value.getFloat());
    } else if (operation == data::DATA_OPERATION_GET_NAME) {
        value = "OPP Limit";
    } else if (operation == data::DATA_OPERATION_GET_UNIT) {
        value = UNIT_WATT;
    } 
}

void data_channel_protection_otp_installed(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        value = temperature::isChannelSensorInstalled(g_channel);
    }
}

void data_channel_protection_otp_state(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        ChSettingsProtectionSetPage *page =
            (ChSettingsProtectionSetPage *)getPage(PAGE_ID_CH_SETTINGS_PROT_OTP);
        if (page) {
            value = page->state;
        }
    }
}

void data_channel_protection_otp_level(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    int iChannel = cursor.i >= 0 ? cursor.i : (g_channel ? g_channel->channelIndex : 0);
    Channel &channel = Channel::get(iChannel);
    if (operation == data::DATA_OPERATION_GET) {
        ChSettingsProtectionSetPage *page = (ChSettingsProtectionSetPage *)getPage(PAGE_ID_CH_SETTINGS_PROT_OTP);
        if (page) {
            value = page->level;
        } else {
            bool focused = g_focusCursor == cursor && g_focusDataId == DATA_ID_CHANNEL_PROTECTION_OTP_LEVEL;
            if (focused && g_focusEditValue.getType() != VALUE_TYPE_NONE) {
                value = g_focusEditValue;
            } else {
                value = MakeValue(temperature::getChannelSensorLevel(&channel), UNIT_CELSIUS);
            }
        }
    } else if (operation == data::DATA_OPERATION_GET_MIN) {
        value = MakeValue(OTP_AUX_MIN_LEVEL, UNIT_SECOND);
    } else if (operation == data::DATA_OPERATION_GET_MAX) {
        value = MakeValue(OTP_AUX_MAX_LEVEL, UNIT_SECOND);
    } else if (operation == data::DATA_OPERATION_SET) {
        channel_dispatcher::setOtpParameters(channel, temperature::getChannelSensorState(&channel) ? 1 : 0, value.getFloat(), temperature::getChannelSensorDelay(&channel));
    } else if (operation == data::DATA_OPERATION_GET_NAME) {
        value = "OTP Level";
    } else if (operation == data::DATA_OPERATION_GET_UNIT) {
        value = UNIT_CELSIUS;
    } 
}
    
void data_channel_protection_otp_delay(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    int iChannel = cursor.i >= 0 ? cursor.i : (g_channel ? g_channel->channelIndex : 0);
    Channel &channel = Channel::get(iChannel);
    if (operation == data::DATA_OPERATION_GET) {
        ChSettingsProtectionSetPage *page = (ChSettingsProtectionSetPage *)getPage(PAGE_ID_CH_SETTINGS_PROT_OTP);
        if (page) {
            value = page->delay;
        } else {
            bool focused = g_focusCursor == cursor && g_focusDataId == DATA_ID_CHANNEL_PROTECTION_OTP_DELAY;
            if (focused && g_focusEditValue.getType() != VALUE_TYPE_NONE) {
                value = g_focusEditValue;
            } else {
                value = MakeValue(temperature::getChannelSensorDelay(&channel), UNIT_SECOND);
            }
        }
    } else if (operation == data::DATA_OPERATION_GET_MIN) {
        value = MakeValue(OTP_AUX_MIN_DELAY, UNIT_SECOND);
    } else if (operation == data::DATA_OPERATION_GET_MAX) {
        value = MakeValue(OTP_AUX_MAX_DELAY, UNIT_SECOND);
    } else if (operation == data::DATA_OPERATION_SET) {
        channel_dispatcher::setOtpParameters(channel, temperature::getChannelSensorState(&channel) ? 1 : 0, temperature::getChannelSensorLevel(&channel), value.getFloat());
    } else if (operation == data::DATA_OPERATION_GET_NAME) {
        value = "OTP Delay";
    } else if (operation == data::DATA_OPERATION_GET_UNIT) {
        value = UNIT_SECOND;
    } 
}

void data_event_queue_last_event_type(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        auto *lastEvent = event_queue::getLastErrorEvent();
        value = data::Value(lastEvent ? event_queue::getEventType(lastEvent) : event_queue::EVENT_TYPE_NONE);
    }
}

void data_event_queue_last_event_message(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        auto lastEvent = event_queue::getLastErrorEvent();
        if (lastEvent && event_queue::getEventType(lastEvent) != event_queue::EVENT_TYPE_NONE) {
            value = MakeEventValue(lastEvent);
        }
    }
}

void data_event_queue_events(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_COUNT) {
        value = event_queue::EVENTS_PER_PAGE;
    }
}

void data_event_queue_events_type(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        if (cursor.i >= 0) {
            event_queue::Event *event = event_queue::getActivePageEvent(cursor.i);
            value = data::Value(event ? event_queue::getEventType(event) : event_queue::EVENT_TYPE_NONE);
        }
    }
}

void data_event_queue_events_message(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        if (cursor.i >= 0) {
            value = MakeEventValue(event_queue::getActivePageEvent(cursor.i));
        }
    }
}

void data_event_queue_multiple_pages(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        data::Value eventQueuePageInfo =
            MakePageInfoValue(event_queue::getActivePageIndex(), event_queue::getNumPages());
        value = getNumPagesFromValue(eventQueuePageInfo) > 1;
    }
}

void data_event_queue_previous_page_enabled(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        data::Value eventQueuePageInfo =
            MakePageInfoValue(event_queue::getActivePageIndex(), event_queue::getNumPages());
        value = getPageIndexFromValue(eventQueuePageInfo) > 0;
    }
}

void data_event_queue_next_page_enabled(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        data::Value eventQueuePageInfo =
            MakePageInfoValue(event_queue::getActivePageIndex(), event_queue::getNumPages());
        value = getPageIndexFromValue(eventQueuePageInfo) <
                getNumPagesFromValue(eventQueuePageInfo) - 1;
    }
}

void data_event_queue_page_info(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        value = MakePageInfoValue(event_queue::getActivePageIndex(), event_queue::getNumPages());
    }
}

void data_channel_has_advanced_options(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        int iChannel = cursor.i >= 0 ? cursor.i : (g_channel ? g_channel->channelIndex : 0);
        Channel &channel = Channel::get(iChannel);
        value = channel.params.features & (CH_FEATURE_DPROG | CH_FEATURE_RPROG | CH_FEATURE_RPOL) ? 1 : 0;
    }
}

void data_channel_rsense_status(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
		int iChannel = cursor.i >= 0 ? cursor.i : (g_channel ? g_channel->channelIndex : 0);
		Channel &channel = Channel::get(iChannel);
        value = channel.isRemoteSensingEnabled();
    }	
}

void data_channel_rprog_installed(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        value = g_channel->params.features & CH_FEATURE_RPROG ? 1 : 0;
    }
}

void data_channel_rprog_status(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
		int iChannel = cursor.i >= 0 ? cursor.i : (g_channel ? g_channel->channelIndex : 0);
		Channel &channel = Channel::get(iChannel);
		value = (int)channel.flags.rprogEnabled;
    }
}

void data_channel_dprog_installed(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        int iChannel = cursor.i >= 0 ? cursor.i : (g_channel ? g_channel->channelIndex : 0);
        Channel &channel = Channel::get(iChannel);
        value = channel.params.features & CH_FEATURE_DPROG ? 1 : 0;
    }
}

void data_channel_dprog(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        int iChannel = cursor.i >= 0 ? cursor.i : (g_channel ? g_channel->channelIndex : 0);
        Channel &channel = Channel::get(iChannel);
        value = (int)channel.getDprogState();
    }
}

void data_is_coupled_or_tracked(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        if (channel_dispatcher::getCouplingType() != channel_dispatcher::COUPLING_TYPE_NONE) {
            value = 1;
        } else {
            for (int i = 0; i < CH_NUM; i++) {
                if (Channel::get(i).flags.trackingEnabled) {
                    value = 1;
                    return;
                }
            }
            value = 0;
        }
    }
}

void data_channel_is_par_ser_coupled_or_tracked(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        int iChannel = cursor.i >= 0 ? cursor.i : (g_channel ? g_channel->channelIndex : 0);
        Channel &channel = Channel::get(iChannel);
        if (channel.flags.trackingEnabled || (iChannel < 2 && (channel_dispatcher::getCouplingType() == channel_dispatcher::COUPLING_TYPE_PARALLEL || channel_dispatcher::getCouplingType() == channel_dispatcher::COUPLING_TYPE_SERIES))) {
            value = 1;
        } else {
            value = 0;
        }
    }
}

void data_is_tracking_allowed(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        int n = 0;
        for (int i = 0; i < CH_NUM; i++) {
            if (Channel::get(i).isOk()) {
                ++n;
            }
        }
        value = n >= 2 ? 1 : 0;
    }
}

void data_channel_tracking_is_enabled(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        int iChannel = cursor.i >= 0 ? cursor.i : (g_channel ? g_channel->channelIndex : 0);
        Channel &channel = Channel::get(iChannel);
        auto page = (SysSettingsTrackingPage *)getPage(PAGE_ID_SYS_SETTINGS_TRACKING);
        if (page) {
            value = (page->m_trackingEnabled & (1 << channel.channelIndex)) ? 1 : 0;
        } else {
            value = channel.flags.trackingEnabled ? 1 : 0;
        }
    }
}

void data_channel_tracking_is_allowed(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        auto channel = Channel::get(cursor.i);
        value = channel_dispatcher::isTrackingAllowed(channel, nullptr) ? 1 : 0;
    }
}

void data_is_coupling_parallel_allowed(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        value = channel_dispatcher::isCouplingTypeAllowed(channel_dispatcher::COUPLING_TYPE_PARALLEL, nullptr) ? 1 : 0;
    }
}

void data_is_coupling_series_allowed(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        value = channel_dispatcher::isCouplingTypeAllowed(channel_dispatcher::COUPLING_TYPE_SERIES, nullptr) ? 1 : 0;
    }
}

channel_dispatcher::CouplingType getCouplingType() {
    auto page = (SysSettingsCouplingPage *)getPage(PAGE_ID_SYS_SETTINGS_COUPLING);
    if (page) {
        return page->m_couplingType;
    } else {
        return channel_dispatcher::getCouplingType();
    }
}

void data_coupling_type(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        value = (int)getCouplingType();
    }
}

void data_is_coupling_type_uncoupled(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        value = getCouplingType() == channel_dispatcher::COUPLING_TYPE_NONE ? 1 : 0;
    }
}

void data_is_coupling_type_series(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        value = getCouplingType() == channel_dispatcher::COUPLING_TYPE_SERIES ? 1 : 0;
    }
}

void data_is_coupling_type_parallel(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        value = getCouplingType() == channel_dispatcher::COUPLING_TYPE_PARALLEL ? 1 : 0;
    }
}

void data_is_coupling_type_common_gnd(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        value = getCouplingType() == channel_dispatcher::COUPLING_TYPE_COMMON_GND ? 1 : 0;
    }
}

void data_is_coupling_type_split_rails(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        value = getCouplingType() == channel_dispatcher::COUPLING_TYPE_SPLIT_RAILS ? 1 : 0;
    }
}

void data_channel_coupling_is_series(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
		int iChannel = cursor.i >= 0 ? cursor.i : (g_channel ? g_channel->channelIndex : 0);
        value = iChannel < 2 && getCouplingType() == channel_dispatcher::COUPLING_TYPE_SERIES ? 1 : 0;
    }
}

void data_channel_coupling_enable_tracking_mode(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        auto page = (SysSettingsCouplingPage *)getPage(PAGE_ID_SYS_SETTINGS_COUPLING);
        if (page) {
            value = page->m_enableTrackingMode ? 1 : 0;
        }
    }
}

void data_sys_on_time_total(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        value =
            data::Value((uint32_t)ontime::g_mcuCounter.getTotalTime(), VALUE_TYPE_ON_TIME_COUNTER);
    }
}

void data_sys_on_time_last(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        value =
            data::Value((uint32_t)ontime::g_mcuCounter.getLastTime(), VALUE_TYPE_ON_TIME_COUNTER);
    }
}

void data_sys_temp_aux_status(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        temperature::TempSensorTemperature &tempSensor = temperature::sensors[temp_sensor::AUX];
        if (tempSensor.isInstalled()) {
            if (tempSensor.isTestOK()) {
                value = 1;
            } else {
                value = 0;
            }
        } else {
            value = 2;
        }
    }
}

void data_sys_temp_aux_otp_state(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        SysSettingsTemperaturePage *page = (SysSettingsTemperaturePage *)getPage(PAGE_ID_SYS_SETTINGS_TEMPERATURE);
        if (page) {
            value = page->state;
        }
    }
}

void data_sys_temp_aux_otp_level(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        SysSettingsTemperaturePage *page = (SysSettingsTemperaturePage *)getPage(PAGE_ID_SYS_SETTINGS_TEMPERATURE);
        if (page) {
            value = page->level;
        }
    }
}

void data_sys_temp_aux_otp_delay(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        SysSettingsTemperaturePage *page = (SysSettingsTemperaturePage *)getPage(PAGE_ID_SYS_SETTINGS_TEMPERATURE);
        if (page) {
            value = page->delay;
        }
    }
}

void data_sys_temp_aux_otp_is_tripped(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        value = temperature::sensors[temp_sensor::AUX].isTripped();
    }
}

void data_sys_temp_aux(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        float auxTemperature = 0;
        temperature::TempSensorTemperature &tempSensor = temperature::sensors[temp_sensor::AUX];
        if (tempSensor.isInstalled() && tempSensor.isTestOK()) {
            auxTemperature = tempSensor.temperature;
        }
        value = MakeValue(auxTemperature, UNIT_CELSIUS);
    }
}

void data_sys_info_has_error(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        temperature::TempSensorTemperature &tempSensor = temperature::sensors[temp_sensor::AUX];
        int err;
        value = (
            // AUX temp.
            (tempSensor.isInstalled() && !tempSensor.isTestOK()) ||
            // FAN
            (aux_ps::fan::g_testResult == TEST_FAILED || aux_ps::fan::g_testResult == TEST_WARNING) ||
            // Battery
            mcu::battery::g_testResult == TEST_FAILED ||
            // SD card
            !eez::psu::sd_card::isMounted(&err)
        ) ? 1 : 0;
    }
}

void data_sys_info_firmware_ver(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        value = data::Value(FIRMWARE);
    }
}

void data_sys_info_serial_no(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        value = data::Value(persist_conf::devConf.serialNumber);
    }
}

void data_sys_info_scpi_ver(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        value = data::Value(SCPI_STD_VERSION_REVISION);
    }
}

void data_sys_info_cpu(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        value = data::Value(getCpuModel());
    }
}

void data_sys_info_battery_status(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        if (mcu::battery::g_testResult == TEST_FAILED) {
            value = 0;
        } else if (mcu::battery::g_testResult == TEST_OK) {
            value = 1;
        } else {
            value = 2;
        }
    }
}

void data_battery(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        value = MakeValue(mcu::battery::g_battery, UNIT_VOLT);
    }
}

void data_sys_info_sdcard_status(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
#if OPTION_SD_CARD
    	int err;
        if (eez::psu::sd_card::isMounted(&err)) {
            if (eez::psu::sd_card::isBusy()) {
                value = 5; // busy
            } else {
                value = 1; // present
            }
        } else if (err == SCPI_ERROR_MISSING_MASS_MEDIA) {
            value = 2; // not present
        } else if (err == SCPI_ERROR_MASS_MEDIA_NO_FILESYSTEM) {
        	value = 3; // no FAT
		} else {
			value = 4; // failed
		}
#else
        value = 0; // unsupported
#endif
    }
}


void data_sys_info_fan_status(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
#if OPTION_FAN
        if (aux_ps::fan::g_testResult == TEST_FAILED || aux_ps::fan::g_testResult == TEST_WARNING) {
            value = 0;
        } else if (aux_ps::fan::g_testResult == TEST_OK) {
#if FAN_OPTION_RPM_MEASUREMENT
            value = 1;
#else
            value = 2;
#endif
        } else if (aux_ps::fan::g_testResult == TEST_NONE) {
            value = 3;
        } else {
            value = 4;
        }
#else
        value = 3;
#endif
    }
}

void data_sys_info_fan_speed(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
#if OPTION_FAN && FAN_OPTION_RPM_MEASUREMENT
    if (operation == data::DATA_OPERATION_GET) {
        if (aux_ps::fan::g_testResult == TEST_OK) {
            value = MakeValue((float)aux_ps::fan::g_rpm, UNIT_RPM);
        }
    }
#endif
}

void data_sys_fan_mode(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        SysSettingsTemperaturePage *page = (SysSettingsTemperaturePage *)getPage(PAGE_ID_SYS_SETTINGS_TEMPERATURE);
        if (page) {
            value = page->fanMode;
        }
    }
}

void data_sys_fan_speed(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        SysSettingsTemperaturePage *page = (SysSettingsTemperaturePage *)getPage(PAGE_ID_SYS_SETTINGS_TEMPERATURE);
        if (page) {
            value = page->fanSpeed;
        }
    }
}

void data_channel_board_info_label(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        if (cursor.i >= 0 && cursor.i < CH_NUM) {
            value = data::Value(cursor.i, VALUE_TYPE_CHANNEL_BOARD_INFO_LABEL);
        }
    }
}

void data_channel_board_info_revision(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        if (cursor.i >= 0 && cursor.i < CH_NUM) {
            value = Value((int)Channel::get(cursor.i).slotIndex, VALUE_TYPE_SLOT_INFO);
        }
    }
}

void data_date_time_date(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        SysSettingsDateTimePage *page = (SysSettingsDateTimePage *)getPage(PAGE_ID_SYS_SETTINGS_DATE_TIME);
        if (page && page->ntpEnabled) {
            uint32_t nowUtc = datetime::nowUtc();
            uint32_t nowLocal = datetime::utcToLocal(nowUtc, page->timeZone, page->dstRule);
            value = data::Value(nowLocal, VALUE_TYPE_DATE);
        }
    }
}

void data_date_time_year(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        SysSettingsDateTimePage *page =
            (SysSettingsDateTimePage *)getPage(PAGE_ID_SYS_SETTINGS_DATE_TIME);
        if (page && !page->ntpEnabled) {
            if (!page->dateTimeModified) {
                page->dateTime = datetime::DateTime::now();
            }
            value = data::Value(page->dateTime.year, VALUE_TYPE_YEAR);
        }
    }
}

void data_date_time_month(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        SysSettingsDateTimePage *page = (SysSettingsDateTimePage *)getPage(PAGE_ID_SYS_SETTINGS_DATE_TIME);
        if (page && !page->ntpEnabled) {
            if (!page->dateTimeModified) {
                page->dateTime = datetime::DateTime::now();
            }
            value = data::Value(page->dateTime.month, VALUE_TYPE_MONTH);
        }
    }
}

void data_date_time_day(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        SysSettingsDateTimePage *page = (SysSettingsDateTimePage *)getPage(PAGE_ID_SYS_SETTINGS_DATE_TIME);
        if (page && !page->ntpEnabled) {
            if (!page->dateTimeModified) {
                page->dateTime = datetime::DateTime::now();
            }
            value = data::Value(page->dateTime.day, VALUE_TYPE_DAY);
        }
    }
}

void data_date_time_time(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        SysSettingsDateTimePage *page = (SysSettingsDateTimePage *)getPage(PAGE_ID_SYS_SETTINGS_DATE_TIME);
        if (page && page->ntpEnabled) {
            uint32_t nowUtc = datetime::nowUtc();
            uint32_t nowLocal = datetime::utcToLocal(nowUtc, page->timeZone, page->dstRule);
            value = data::Value(nowLocal, VALUE_TYPE_TIME);
        }
    }
}

void data_date_time_hour(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        SysSettingsDateTimePage *page = (SysSettingsDateTimePage *)getPage(PAGE_ID_SYS_SETTINGS_DATE_TIME);
        if (page && !page->ntpEnabled) {
            if (!page->dateTimeModified) {
                page->dateTime = datetime::DateTime::now();
            }
            value = data::Value(page->dateTime.hour, VALUE_TYPE_HOUR);
        }
    }
}

void data_date_time_minute(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        SysSettingsDateTimePage *page = (SysSettingsDateTimePage *)getPage(PAGE_ID_SYS_SETTINGS_DATE_TIME);
        if (page && !page->ntpEnabled) {
            if (!page->dateTimeModified) {
                page->dateTime = datetime::DateTime::now();
            }
            value = data::Value(page->dateTime.minute, VALUE_TYPE_MINUTE);
        }
    }
}

void data_date_time_second(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        SysSettingsDateTimePage *page = (SysSettingsDateTimePage *)getPage(PAGE_ID_SYS_SETTINGS_DATE_TIME);
        if (page && !page->ntpEnabled) {
            if (!page->dateTimeModified) {
                page->dateTime = datetime::DateTime::now();
            }
            value = data::Value(page->dateTime.second, VALUE_TYPE_SECOND);
        }
    }
}

void data_date_time_time_zone(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        SysSettingsDateTimePage *page = (SysSettingsDateTimePage *)getPage(PAGE_ID_SYS_SETTINGS_DATE_TIME);
        if (page) {
            value = data::Value(page->timeZone, VALUE_TYPE_TIME_ZONE);
        }
    }
}

void data_date_time_dst(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        SysSettingsDateTimePage *page = (SysSettingsDateTimePage *)getPage(PAGE_ID_SYS_SETTINGS_DATE_TIME);
        if (page) {
            value = MakeEnumDefinitionValue(page->dstRule, ENUM_DEFINITION_DST_RULE);
        }
    }
}

void data_set_page_dirty(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        SetPage *page = (SetPage *)getActivePage();
        if (page) {
            value = data::Value(page->getDirty());
        }
    }
}

void data_profiles_list(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_COUNT) {
        value = 10;
    }
}

void data_profiles_auto_recall_status(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        value = data::Value(persist_conf::isProfileAutoRecallEnabled());
    }
}

void data_profiles_auto_recall_location(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        value = data::Value(persist_conf::getProfileAutoRecallLocation());
    }
}

void data_profile_status(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        int selectedProfileLocation = UserProfilesPage::getSelectedProfileLocation();
        if (selectedProfileLocation != -1) {
            value = profile::isValid(selectedProfileLocation);
        }
    }
}

void data_profile_label(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        int selectedProfileLocation = UserProfilesPage::getSelectedProfileLocation();
        if (selectedProfileLocation != -1) {
            value = data::Value(selectedProfileLocation, VALUE_TYPE_USER_PROFILE_LABEL);
        } else if (cursor.i >= 0) {
            value = data::Value(cursor.i, VALUE_TYPE_USER_PROFILE_LABEL);
        }
    }
}

void data_profile_remark(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        int selectedProfileLocation = UserProfilesPage::getSelectedProfileLocation();
        if (selectedProfileLocation != -1) {
            profile::Parameters *profile = profile::load(selectedProfileLocation);
            if (profile) {
                value = data::Value(profile->name);
            }
        } else if (cursor.i >= 0) {
            value = data::Value(cursor.i, VALUE_TYPE_USER_PROFILE_REMARK);
        }
    }
}

void data_profile_is_auto_recall_location(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        int selectedProfileLocation = UserProfilesPage::getSelectedProfileLocation();
        if (selectedProfileLocation != -1) {
            value = persist_conf::getProfileAutoRecallLocation() == selectedProfileLocation;
        }
    }
}

void data_profile_channel_u_set(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        int selectedProfileLocation = UserProfilesPage::getSelectedProfileLocation();
        if (selectedProfileLocation != -1 && (cursor.i >= 0 && cursor.i < CH_MAX)) {
            profile::Parameters *profile = profile::load(selectedProfileLocation);
            if (profile) {
                value = MakeValue(profile->channels[cursor.i].u_set, UNIT_VOLT);
            }
        }
    }
}

void data_profile_channel_i_set(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        int selectedProfileLocation = UserProfilesPage::getSelectedProfileLocation();
        if (selectedProfileLocation != -1 && (cursor.i >= 0 && cursor.i < CH_MAX)) {
            profile::Parameters *profile = profile::load(selectedProfileLocation);
            if (profile) {
                value = MakeValue(profile->channels[cursor.i].i_set, UNIT_AMPER);
            }
        }
    }
}

void data_profile_channel_output_state(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        int selectedProfileLocation = UserProfilesPage::getSelectedProfileLocation();
        if (selectedProfileLocation != -1 && (cursor.i >= 0 && cursor.i < CH_MAX)) {
            profile::Parameters *profile = profile::load(selectedProfileLocation);
            if (profile) {
                value = (int)profile->channels[cursor.i].flags.output_enabled;
            }
        }
    }
}

void data_profile_dirty(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        value = profile::g_profileDirty ? 1 : 0;
    }
}

void data_ethernet_installed(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        value = data::Value(OPTION_ETHERNET);
    }
}

void data_ethernet_enabled(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
#if OPTION_ETHERNET
    if (operation == data::DATA_OPERATION_GET) {
        SysSettingsEthernetPage *page =
            (SysSettingsEthernetPage *)getPage(PAGE_ID_SYS_SETTINGS_ETHERNET);
        if (page) {
            value = page->m_enabled;
        }
    }
#endif
}

void data_ethernet_status(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
#if OPTION_ETHERNET
    if (operation == data::DATA_OPERATION_GET) {
        value = data::Value(ethernet::g_testResult);
    }
#endif
}

void data_ethernet_ip_address(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
#if OPTION_ETHERNET
    if (operation == data::DATA_OPERATION_GET) {
        SysSettingsEthernetStaticPage *page =
            (SysSettingsEthernetStaticPage *)getPage(PAGE_ID_SYS_SETTINGS_ETHERNET_STATIC);
        if (page) {
            value = data::Value(page->m_ipAddress, VALUE_TYPE_IP_ADDRESS);
        } else {
            SysSettingsEthernetPage *page =
                (SysSettingsEthernetPage *)getPage(PAGE_ID_SYS_SETTINGS_ETHERNET);
            if (page) {
                if (page->m_dhcpEnabled) {
                    value = data::Value(ethernet::getIpAddress(), VALUE_TYPE_IP_ADDRESS);
                }
            }
        }
    }
#endif
}

void data_ethernet_dns(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
#if OPTION_ETHERNET
    if (operation == data::DATA_OPERATION_GET) {
        SysSettingsEthernetStaticPage *page =
            (SysSettingsEthernetStaticPage *)getPage(PAGE_ID_SYS_SETTINGS_ETHERNET_STATIC);
        if (page) {
            value = data::Value(page->m_dns, VALUE_TYPE_IP_ADDRESS);
        }
    }
#endif
}

void data_ethernet_gateway(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
#if OPTION_ETHERNET
    if (operation == data::DATA_OPERATION_GET) {
        SysSettingsEthernetStaticPage *page =
            (SysSettingsEthernetStaticPage *)getPage(PAGE_ID_SYS_SETTINGS_ETHERNET_STATIC);
        if (page) {
            value = data::Value(page->m_gateway, VALUE_TYPE_IP_ADDRESS);
        }
    }
#endif
}

void data_ethernet_subnet_mask(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
#if OPTION_ETHERNET
    if (operation == data::DATA_OPERATION_GET) {
        SysSettingsEthernetStaticPage *page =
            (SysSettingsEthernetStaticPage *)getPage(PAGE_ID_SYS_SETTINGS_ETHERNET_STATIC);
        if (page) {
            value = data::Value(page->m_subnetMask, VALUE_TYPE_IP_ADDRESS);
        }
    }
#endif
}

void data_ethernet_scpi_port(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
#if OPTION_ETHERNET
    if (operation == data::DATA_OPERATION_GET) {
        SysSettingsEthernetPage *page =
            (SysSettingsEthernetPage *)getPage(PAGE_ID_SYS_SETTINGS_ETHERNET);
        if (page) {
            value = data::Value((uint16_t)page->m_scpiPort, VALUE_TYPE_PORT);
        }
    }
#endif
}

void data_ethernet_is_connected(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
#if OPTION_ETHERNET
    if (operation == data::DATA_OPERATION_GET) {
        value = data::Value(ethernet::isConnected());
    }
#endif
}

void data_ethernet_dhcp(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
#if OPTION_ETHERNET
    if (operation == data::DATA_OPERATION_GET) {
        SysSettingsEthernetPage *page =
            (SysSettingsEthernetPage *)getPage(PAGE_ID_SYS_SETTINGS_ETHERNET);
        if (page) {
            value = page->m_dhcpEnabled;
        }
    }
#endif
}

void data_ethernet_mac(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
#if OPTION_ETHERNET
    static uint8_t s_macAddressData[2][6];

    if (operation == data::DATA_OPERATION_GET) {
        SysSettingsEthernetPage *page =
            (SysSettingsEthernetPage *)getPage(PAGE_ID_SYS_SETTINGS_ETHERNET);
        if (page) {
            uint8_t *macAddress = &s_macAddressData[getCurrentStateBufferIndex()][0];
            memcpy(macAddress, page->m_macAddress, 6);
            value = MakeMacAddressValue(macAddress);
        }
    }
#endif
}

void data_channel_is_voltage_balanced(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        if (channel_dispatcher::getCouplingType() == channel_dispatcher::COUPLING_TYPE_SERIES) {
            value = Channel::get(0).isVoltageBalanced() || Channel::get(1).isVoltageBalanced();
        } else {
            value = 0;
        }
    }
}

void data_channel_is_current_balanced(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        if (channel_dispatcher::getCouplingType() == channel_dispatcher::COUPLING_TYPE_PARALLEL) {
            value = Channel::get(0).isCurrentBalanced() || Channel::get(1).isCurrentBalanced();
        } else {
            value = 0;
        }
    }
}

void data_sys_output_protection_coupled(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        value = persist_conf::isOutputProtectionCoupleEnabled();
    }
}

void data_sys_shutdown_when_protection_tripped(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        value = persist_conf::isShutdownWhenProtectionTrippedEnabled();
    }
}

void data_sys_force_disabling_all_outputs_on_power_up(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        value = persist_conf::isForceDisablingAllOutputsOnPowerUpEnabled();
    }
}

void data_sys_password_is_set(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        value = strlen(persist_conf::devConf.systemPassword) > 0;
    }
}

void data_sys_rl_state(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        value = data::Value(g_rlState);
    }
}

void data_sys_sound_enabled(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        if (!persist_conf::isSoundEnabled() && !persist_conf::isClickSoundEnabled()) {
            // both disabled
            value = 0;
        } else if (persist_conf::isSoundEnabled() && persist_conf::isClickSoundEnabled()) {
            // both enabled
            value = 1;
        } else {
            // mixed
            value = 2;
        }
    }
}

void data_sys_sound_is_enabled(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        value = persist_conf::isSoundEnabled();
    }
}

void data_sys_sound_is_click_enabled(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        value = persist_conf::isClickSoundEnabled();
    }
}

void data_channel_display_view_settings_display_value1(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        ChSettingsAdvViewPage *page =
            (ChSettingsAdvViewPage *)getPage(PAGE_ID_CH_SETTINGS_ADV_VIEW);
        if (page) {
            value =
                MakeEnumDefinitionValue(page->displayValue1, ENUM_DEFINITION_CHANNEL_DISPLAY_VALUE);
        }
    }
}

void data_channel_display_view_settings_display_value2(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        ChSettingsAdvViewPage *page =
            (ChSettingsAdvViewPage *)getPage(PAGE_ID_CH_SETTINGS_ADV_VIEW);
        if (page) {
            value =
                MakeEnumDefinitionValue(page->displayValue2, ENUM_DEFINITION_CHANNEL_DISPLAY_VALUE);
        }
    }
}

void data_channel_display_view_settings_yt_view_rate(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        ChSettingsAdvViewPage *page =
            (ChSettingsAdvViewPage *)getPage(PAGE_ID_CH_SETTINGS_ADV_VIEW);
        if (page) {
            value = MakeValue(page->ytViewRate, UNIT_SECOND);
        }
    }
}

void data_sys_encoder_confirmation_mode(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
#if OPTION_ENCODER
    if (operation == data::DATA_OPERATION_GET) {
        SysSettingsEncoderPage *page =
            (SysSettingsEncoderPage *)getPage(PAGE_ID_SYS_SETTINGS_ENCODER);
        if (page) {
            value = data::Value((int)page->confirmationMode);
        }
    }
#endif
}

void data_sys_encoder_moving_up_speed(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
#if OPTION_ENCODER
    if (operation == data::DATA_OPERATION_GET) {
        SysSettingsEncoderPage *page =
            (SysSettingsEncoderPage *)getPage(PAGE_ID_SYS_SETTINGS_ENCODER);
        if (page) {
            value = data::Value((int)page->movingSpeedUp);
        }
    } else if (operation == data::DATA_OPERATION_GET_MIN) {
        value = mcu::encoder::MIN_MOVING_SPEED;
    } else if (operation == data::DATA_OPERATION_GET_MAX) {
        value = mcu::encoder::MAX_MOVING_SPEED;
    } else if (operation == data::DATA_OPERATION_SET) {
        SysSettingsEncoderPage *page =
            (SysSettingsEncoderPage *)getPage(PAGE_ID_SYS_SETTINGS_ENCODER);
        if (page) {
            page->movingSpeedUp = (uint8_t)value.getInt();
        }
    }
#endif
}

void data_sys_encoder_moving_down_speed(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
#if OPTION_ENCODER
    if (operation == data::DATA_OPERATION_GET) {
        SysSettingsEncoderPage *page =
            (SysSettingsEncoderPage *)getPage(PAGE_ID_SYS_SETTINGS_ENCODER);
        if (page) {
            value = data::Value((int)page->movingSpeedDown);
        }
    } else if (operation == data::DATA_OPERATION_GET_MIN) {
        value = mcu::encoder::MIN_MOVING_SPEED;
    } else if (operation == data::DATA_OPERATION_GET_MAX) {
        value = mcu::encoder::MAX_MOVING_SPEED;
    } else if (operation == data::DATA_OPERATION_SET) {
        SysSettingsEncoderPage *page =
            (SysSettingsEncoderPage *)getPage(PAGE_ID_SYS_SETTINGS_ENCODER);
        if (page) {
            page->movingSpeedDown = (uint8_t)value.getInt();
        }
    }
#endif
}

void data_sys_encoder_installed(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        value = data::Value(OPTION_ENCODER);
    }
}

void data_sys_display_state(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        value = (int)persist_conf::devConf.displayState;
    }
}

void data_sys_display_brightness(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        value = data::Value(persist_conf::devConf.displayBrightness);
    } else if (operation == data::DATA_OPERATION_GET_MIN) {
        value = DISPLAY_BRIGHTNESS_MIN;
    } else if (operation == data::DATA_OPERATION_GET_MAX) {
        value = DISPLAY_BRIGHTNESS_MAX;
    } else if (operation == data::DATA_OPERATION_SET) {
        persist_conf::setDisplayBrightness((uint8_t)value.getInt());
    }
}

void data_channel_trigger_mode(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        value = MakeEnumDefinitionValue(channel_dispatcher::getVoltageTriggerMode(*g_channel),
                                        ENUM_DEFINITION_CHANNEL_TRIGGER_MODE);
    }
}

void data_channel_trigger_output_state(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        value = channel_dispatcher::getTriggerOutputState(*g_channel);
    }
}

void data_channel_trigger_on_list_stop(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        value = MakeEnumDefinitionValue(channel_dispatcher::getTriggerOnListStop(*g_channel),
                                        ENUM_DEFINITION_CHANNEL_TRIGGER_ON_LIST_STOP);
    }
}

void data_channel_u_trigger_value(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        value = MakeValue(channel_dispatcher::getTriggerVoltage(*g_channel), UNIT_VOLT);
    }
}

void data_channel_i_trigger_value(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        value = MakeValue(channel_dispatcher::getTriggerCurrent(*g_channel), UNIT_AMPER);
    }
}

void data_channel_list_count(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        uint16_t count = list::getListCount(*g_channel);
        if (count > 0) {
            value = data::Value(count);
        } else {
            value = INF_TEXT;
        }
    }
}

void data_channel_lists(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_COUNT) {
        value = LIST_ITEMS_PER_PAGE;
    } else if (operation == data::DATA_OPERATION_GET_FLOAT_LIST_LENGTH) {
        ChSettingsListsPage *page = (ChSettingsListsPage *)getPage(PAGE_ID_CH_SETTINGS_LISTS);
        if (page) {
            value = page->getMaxListLength();
        }
    }
}

void data_channel_list_index(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        ChSettingsListsPage *page = (ChSettingsListsPage *)getPage(PAGE_ID_CH_SETTINGS_LISTS);
        if (page) {
            int iPage = page->getPageIndex();
            int iRow = iPage * LIST_ITEMS_PER_PAGE + cursor.i;
            value = iRow + 1;
        }
    }
}

void data_channel_list_dwell(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        ChSettingsListsPage *page = (ChSettingsListsPage *)getPage(PAGE_ID_CH_SETTINGS_LISTS);
        if (page) {
            int iPage = page->getPageIndex();
            int iRow = iPage * LIST_ITEMS_PER_PAGE + cursor.i;
            if (iRow < page->m_dwellListLength) {
                value = MakeValue(page->m_dwellList[iRow], UNIT_SECOND);
            } else {
                value = data::Value(EMPTY_VALUE);
            }
        }
    } else if (operation == data::DATA_OPERATION_GET_MIN) {
        value = MakeValue(LIST_DWELL_MIN, UNIT_SECOND);
    } else if (operation == data::DATA_OPERATION_GET_MAX) {
        value = MakeValue(LIST_DWELL_MAX, UNIT_SECOND);
    } else if (operation == data::DATA_OPERATION_GET_DEF) {
        value = MakeValue(LIST_DWELL_DEF, UNIT_SECOND);
    } else if (operation == data::DATA_OPERATION_GET_FLOAT_LIST_LENGTH) {
        ChSettingsListsPage *page = (ChSettingsListsPage *)getPage(PAGE_ID_CH_SETTINGS_LISTS);
        if (page) {
            value = page->m_dwellListLength;
        }
    } else if (operation == data::DATA_OPERATION_GET_FLOAT_LIST) {
        ChSettingsListsPage *page = (ChSettingsListsPage *)getPage(PAGE_ID_CH_SETTINGS_LISTS);
        if (page) {
            value = MakeFloatListValue(page->m_dwellList);
        }
    }
}

void data_channel_list_dwell_enabled(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        ChSettingsListsPage *page = (ChSettingsListsPage *)getPage(PAGE_ID_CH_SETTINGS_LISTS);
        if (page) {
            int iPage = page->getPageIndex();
            int iRow = iPage * LIST_ITEMS_PER_PAGE + cursor.i;
            value = iRow <= page->m_dwellListLength;
        }
    }
}

void data_channel_list_voltage(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        ChSettingsListsPage *page = (ChSettingsListsPage *)getPage(PAGE_ID_CH_SETTINGS_LISTS);
        if (page) {
            int iPage = page->getPageIndex();
            int iRow = iPage * LIST_ITEMS_PER_PAGE + cursor.i;
            if (iRow < page->m_voltageListLength) {
                value = MakeValue(page->m_voltageList[iRow], UNIT_VOLT);
            } else {
                value = data::Value(EMPTY_VALUE);
            }
        }
    } else if (operation == data::DATA_OPERATION_GET_MIN) {
        value = MakeValue(channel_dispatcher::getUMin(*g_channel), UNIT_VOLT);
    } else if (operation == data::DATA_OPERATION_GET_MAX) {
        value = MakeValue(channel_dispatcher::getUMax(*g_channel), UNIT_VOLT);
    } else if (operation == data::DATA_OPERATION_GET_DEF) {
        value = MakeValue(g_channel->u.def, UNIT_VOLT);
    } else if (operation == data::DATA_OPERATION_GET_FLOAT_LIST_LENGTH) {
        ChSettingsListsPage *page = (ChSettingsListsPage *)getPage(PAGE_ID_CH_SETTINGS_LISTS);
        if (page) {
            value = page->m_voltageListLength;
        }
    } else if (operation == data::DATA_OPERATION_GET_FLOAT_LIST) {
        ChSettingsListsPage *page = (ChSettingsListsPage *)getPage(PAGE_ID_CH_SETTINGS_LISTS);
        if (page) {
            value = MakeFloatListValue(page->m_voltageList);
        }
    }
}

void data_channel_list_voltage_enabled(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        ChSettingsListsPage *page = (ChSettingsListsPage *)getPage(PAGE_ID_CH_SETTINGS_LISTS);
        if (page) {
            int iPage = page->getPageIndex();
            int iRow = iPage * LIST_ITEMS_PER_PAGE + cursor.i;
            value = iRow <= page->m_voltageListLength;
        }
    }
}

void data_channel_list_current(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        ChSettingsListsPage *page = (ChSettingsListsPage *)getPage(PAGE_ID_CH_SETTINGS_LISTS);
        if (page) {
            int iPage = page->getPageIndex();
            int iRow = iPage * LIST_ITEMS_PER_PAGE + cursor.i;
            if (iRow < page->m_currentListLength) {
                value = MakeValue(page->m_currentList[iRow], UNIT_AMPER);
            } else {
                value = data::Value(EMPTY_VALUE);
            }
        }
    } else if (operation == data::DATA_OPERATION_GET_MIN) {
        value =
            MakeValue(channel_dispatcher::getIMin(*g_channel), UNIT_AMPER);
    } else if (operation == data::DATA_OPERATION_GET_MAX) {
        value =
            MakeValue(channel_dispatcher::getIMax(*g_channel), UNIT_AMPER);
    } else if (operation == data::DATA_OPERATION_GET_DEF) {
        value = MakeValue(g_channel->i.def, UNIT_AMPER);
    } else if (operation == data::DATA_OPERATION_GET_FLOAT_LIST_LENGTH) {
        ChSettingsListsPage *page = (ChSettingsListsPage *)getPage(PAGE_ID_CH_SETTINGS_LISTS);
        if (page) {
            value = page->m_currentListLength;
        }
    } else if (operation == data::DATA_OPERATION_GET_FLOAT_LIST) {
        ChSettingsListsPage *page = (ChSettingsListsPage *)getPage(PAGE_ID_CH_SETTINGS_LISTS);
        if (page) {
            ChSettingsListsPage *page = (ChSettingsListsPage *)getActivePage();
            value = MakeFloatListValue(page->m_currentList);
        }
    }
}

void data_channel_list_current_enabled(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        ChSettingsListsPage *page = (ChSettingsListsPage *)getPage(PAGE_ID_CH_SETTINGS_LISTS);
        if (page) {
            int iPage = page->getPageIndex();
            int iRow = iPage * LIST_ITEMS_PER_PAGE + cursor.i;
            value = iRow <= page->m_currentListLength;
        }
    }
}

void data_channel_lists_previous_page_enabled(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        ChSettingsListsPage *page = (ChSettingsListsPage *)getPage(PAGE_ID_CH_SETTINGS_LISTS);
        if (page) {
            int iPage = page->getPageIndex();
            value = iPage > 0;
        }
    }
}

void data_channel_lists_next_page_enabled(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        ChSettingsListsPage *page = (ChSettingsListsPage *)getPage(PAGE_ID_CH_SETTINGS_LISTS);
        if (page) {
            int iPage = page->getPageIndex();
            value = (iPage < page->getNumPages() - 1);
        }
    }
}

void data_channel_lists_cursor(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    ChSettingsListsPage *page = (ChSettingsListsPage *)getPage(PAGE_ID_CH_SETTINGS_LISTS);
    if (page) {
        if (operation == data::DATA_OPERATION_GET) {
            value = data::Value(page->m_iCursor);
        } else if (operation == data::DATA_OPERATION_SET) {
            page->m_iCursor = value.getInt();
            page->moveCursorToFirstAvailableCell();
        }
    }
}

void data_channel_lists_insert_menu_enabled(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        ChSettingsListsPage *page = (ChSettingsListsPage *)getPage(PAGE_ID_CH_SETTINGS_LISTS);
        if (page) {
            value = page->getRowIndex() < page->getMaxListLength();
        }
    }
}

void data_channel_lists_delete_menu_enabled(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        ChSettingsListsPage *page = (ChSettingsListsPage *)getPage(PAGE_ID_CH_SETTINGS_LISTS);
        if (page) {
            value = page->getMaxListLength();
        }
    }
}

void data_channel_lists_delete_row_enabled(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        ChSettingsListsPage *page = (ChSettingsListsPage *)getPage(PAGE_ID_CH_SETTINGS_LISTS);
        if (page) {
            value = page->getRowIndex() < page->getMaxListLength();
        }
    }
}

void data_channel_lists_clear_column_enabled(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        ChSettingsListsPage *page = (ChSettingsListsPage *)getPage(PAGE_ID_CH_SETTINGS_LISTS);
        if (page) {
            value = page->getRowIndex() < page->getMaxListLength();
        }
    }
}

void data_channel_lists_delete_rows_enabled(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        ChSettingsListsPage *page = (ChSettingsListsPage *)getPage(PAGE_ID_CH_SETTINGS_LISTS);
        if (page) {
            value = page->getRowIndex() < page->getMaxListLength();
        }
    }
}

void data_trigger_source(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        SysSettingsTriggerPage *page =
            (SysSettingsTriggerPage *)getPage(PAGE_ID_SYS_SETTINGS_TRIGGER);
        if (page) {
            value = MakeEnumDefinitionValue(page->m_source, ENUM_DEFINITION_TRIGGER_SOURCE);
        }
    }
}

void data_trigger_delay(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        SysSettingsTriggerPage *page =
            (SysSettingsTriggerPage *)getPage(PAGE_ID_SYS_SETTINGS_TRIGGER);
        if (page) {
            value = MakeValue(page->m_delay, UNIT_SECOND);
        }
    }
}

void data_trigger_initiate_continuously(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        SysSettingsTriggerPage *page =
            (SysSettingsTriggerPage *)getPage(PAGE_ID_SYS_SETTINGS_TRIGGER);
        if (page) {
            value = page->m_initiateContinuously;
        }
    }
}

void data_trigger_is_initiated(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        int iChannel = cursor.i >= 0 ? cursor.i : (g_channel ? g_channel->channelIndex : 0);
        Channel &channel = Channel::get(iChannel);
        value = (trigger::isInitiated() || trigger::isTriggered()) && channel_dispatcher::getVoltageTriggerMode(channel) != TRIGGER_MODE_FIXED;
    } else if (operation == data::DATA_OPERATION_IS_BLINKING) {
        value = trigger::isInitiated();
    }
}

void data_trigger_is_manual(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        value = trigger::getSource() == trigger::SOURCE_MANUAL && !trigger::isTriggered();
    }
}

void data_channel_has_support_for_current_dual_range(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        int iChannel = cursor.i >= 0 ? cursor.i : (g_channel ? g_channel->channelIndex : 0);
        Channel &channel = Channel::get(iChannel);
        value = channel.hasSupportForCurrentDualRange();
    }
}

void data_channel_ranges_supported(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        value = g_channel->hasSupportForCurrentDualRange();
    }
}

void data_channel_ranges_mode(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        value = MakeEnumDefinitionValue(g_channel->getCurrentRangeSelectionMode(),
                                        ENUM_DEFINITION_CHANNEL_CURRENT_RANGE_SELECTION_MODE);
    }
}

void data_channel_ranges_auto_ranging(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        value = g_channel->isAutoSelectCurrentRangeEnabled();
    }
}

void data_channel_ranges_currently_selected(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        int iChannel = cursor.i >= 0 ? cursor.i : (g_channel ? g_channel->channelIndex : 0);
        Channel &channel = Channel::get(iChannel);
        value = MakeEnumDefinitionValue(channel.flags.currentCurrentRange, ENUM_DEFINITION_CHANNEL_CURRENT_RANGE);
    }
}

void data_text_message(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        value = data::Value(g_psuAppContext.getTextMessageVersion(), VALUE_TYPE_TEXT_MESSAGE);
    }
}

void data_serial_status(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        value = data::Value(serial::g_testResult);
    }
}

void data_serial_enabled(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        SysSettingsSerialPage *page = (SysSettingsSerialPage *)getPage(PAGE_ID_SYS_SETTINGS_SERIAL);
        if (page) {
            value = page->m_enabled;
        }
    }
}

void data_serial_is_connected(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        value = data::Value(serial::isConnected());
    }
}

void data_serial_baud(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        SysSettingsSerialPage *page = (SysSettingsSerialPage *)getPage(PAGE_ID_SYS_SETTINGS_SERIAL);
        if (page) {
            value = data::Value((int)page->m_baudIndex, VALUE_TYPE_SERIAL_BAUD_INDEX);
        }
    } else if (operation == data::DATA_OPERATION_GET_MIN) {
        value = 1;
    } else if (operation == data::DATA_OPERATION_GET_MAX) {
        value = (int)serial::g_baudsSize;
    } else if (operation == data::DATA_OPERATION_SET) {
        SysSettingsSerialPage *page = (SysSettingsSerialPage *)getPage(PAGE_ID_SYS_SETTINGS_SERIAL);
        if (page) {
            page->m_baudIndex = (uint8_t)value.getInt();
        }
    }
}

void data_serial_parity(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        SysSettingsSerialPage *page = (SysSettingsSerialPage *)getPage(PAGE_ID_SYS_SETTINGS_SERIAL);
        if (page) {
            value = MakeEnumDefinitionValue(page->m_parity, ENUM_DEFINITION_SERIAL_PARITY);
        }
    }
}

void data_io_pins(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_COUNT) {
        value = 4;
    }
}

void data_io_pins_inhibit_state(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        if (io_pins::isInhibited()) {
            value = 1;
        } else {
            const persist_conf::IOPin &inputPin1 = persist_conf::devConf.ioPins[0];
            const persist_conf::IOPin &inputPin2 = persist_conf::devConf.ioPins[1];
            if (inputPin1.function == io_pins::FUNCTION_INHIBIT || inputPin2.function == io_pins::FUNCTION_INHIBIT) {
                value = 0;
            } else {
                value = 2;
            }
        }
    } else if (operation == data::DATA_OPERATION_IS_BLINKING) {
        value = io_pins::isInhibited();
    }
}

void data_io_pin_number(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        value = data::Value(cursor.i + 1);
    }
}

void data_io_pin_polarity(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        SysSettingsIOPinsPage *page = (SysSettingsIOPinsPage *)getPage(PAGE_ID_SYS_SETTINGS_IO);
        if (page) {
            value = MakeEnumDefinitionValue(page->m_polarity[cursor.i], ENUM_DEFINITION_IO_PINS_POLARITY);
        }
    }
}

void data_io_pin_function(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        SysSettingsIOPinsPage *page = (SysSettingsIOPinsPage *)getPage(PAGE_ID_SYS_SETTINGS_IO);
        if (page) {
            if (cursor.i < 2) {
                value = MakeEnumDefinitionValue(page->m_function[cursor.i], ENUM_DEFINITION_IO_PINS_INPUT_FUNCTION);
            } else {
                value = MakeEnumDefinitionValue(page->m_function[cursor.i], ENUM_DEFINITION_IO_PINS_OUTPUT_FUNCTION);
            }
        }
    }
}

void data_io_pin_state(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        SysSettingsIOPinsPage *page = (SysSettingsIOPinsPage *)getPage(PAGE_ID_SYS_SETTINGS_IO);
        if (page) {
            int pin = cursor.i;
            if (page->m_function[pin] == io_pins::FUNCTION_NONE) {
                value = 4; // Unassigned
            } else {
                int state = io_pins::getPinState(cursor.i);

                if (page->m_polarity[pin] != persist_conf::devConf.ioPins[pin].polarity) {
                    state = state ? 0 : 1;
                }

                if (pin >= 2 && page->m_function[pin] == io_pins::FUNCTION_OUTPUT && persist_conf::devConf.ioPins[pin].function == io_pins::FUNCTION_OUTPUT) {
                    if (state) {
                        value = 3; // Active_Changeable
                    } else {
                        value = 2; // Inactive_Changeable
                    }
                } else {
                    value = state; // Active or Inactive
                }
            }
        }
    }
}

void data_ntp_enabled(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        SysSettingsDateTimePage *page =
            (SysSettingsDateTimePage *)getPage(PAGE_ID_SYS_SETTINGS_DATE_TIME);
        if (page) {
            value = page->ntpEnabled;
        }
    }
}

void data_ntp_server(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        SysSettingsDateTimePage *page =
            (SysSettingsDateTimePage *)getPage(PAGE_ID_SYS_SETTINGS_DATE_TIME);
        if (page) {
            value = page->ntpServer[0] ? page->ntpServer : "<not specified>";
        }
    }
}

void data_sys_display_background_luminosity_step(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        value = data::Value(persist_conf::devConf.displayBackgroundLuminosityStep);
    } else if (operation == data::DATA_OPERATION_GET_MIN) {
        value = DISPLAY_BACKGROUND_LUMINOSITY_STEP_MIN;
    } else if (operation == data::DATA_OPERATION_GET_MAX) {
        value = DISPLAY_BACKGROUND_LUMINOSITY_STEP_MAX;
    } else if (operation == data::DATA_OPERATION_SET) {
        persist_conf::setDisplayBackgroundLuminosityStep((uint8_t)value.getInt());
    }
}

#if defined(EEZ_PLATFORM_SIMULATOR)

void data_simulator_load_state(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
	if (operation == data::DATA_OPERATION_GET) {
        Channel &channel = Channel::get(cursor.i);
		value = channel.simulator.getLoadEnabled() ? 1 : 0;
	}
}

void data_simulator_load(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
	if (operation == data::DATA_OPERATION_GET) {
        Channel &channel = Channel::get(cursor.i);
		value = MakeValue(channel.simulator.getLoad(), UNIT_OHM);
	}
}

void data_simulator_load_state2(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        Channel &channel = Channel::get(cursor.i + 1);
        value = channel.simulator.getLoadEnabled() ? 1 : 0;
    }
}

void data_simulator_load2(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        Channel &channel = Channel::get(cursor.i + 1);
        value = MakeValue(channel.simulator.getLoad(), UNIT_OHM);
    }
}

#endif

void data_channel_active_coupled_led(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        Channel &channel = Channel::get(cursor.i);
        if (channel.isOutputEnabled()) {
            if (channel.channelIndex == 0 && channel_dispatcher::getCouplingType() == channel_dispatcher::COUPLING_TYPE_PARALLEL) {
                value = 2;
            } else if (channel.channelIndex == 1 && channel_dispatcher::getCouplingType() == channel_dispatcher::COUPLING_TYPE_PARALLEL) {
                value = 0;
            } else if (channel.channelIndex == 0 && channel_dispatcher::getCouplingType() == channel_dispatcher::COUPLING_TYPE_SERIES) {
                value = 2;
            } else if (channel.channelIndex == 1 && channel_dispatcher::getCouplingType() == channel_dispatcher::COUPLING_TYPE_SERIES) {
                value = 0;
            } else if (channel.channelIndex < 2 && channel_dispatcher::getCouplingType() == channel_dispatcher::COUPLING_TYPE_SPLIT_RAILS) {
                value = 2;
            } else if (channel_dispatcher::getCouplingType() == channel_dispatcher::COUPLING_TYPE_COMMON_GND) {
                value = 2;
            } else {
                value = 1;
            }
        } else {
            value = 0;
        }
    }
}

void data_channels_with_list_counter_visible(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_COUNT) {
        value = list::g_numChannelsWithVisibleCounters;
    }
}

void data_channel_list_countdown(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        if (cursor.i >= 0 && cursor.i < list::g_numChannelsWithVisibleCounters) {
            int iChannel = list::g_channelsWithVisibleCounters[cursor.i];
            Channel &channel = Channel::get(iChannel);
            int32_t remaining;
            uint32_t total;
            if (list::getCurrentDwellTime(channel, remaining, total) && total >= CONF_LIST_COUNDOWN_DISPLAY_THRESHOLD) {
                value = data::Value((uint32_t)remaining, VALUE_TYPE_COUNTDOWN);
            }
        }
    }
}

void data_overlay(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    static const int NUM_WIDGETS = 4;

    static const int LIST_ICON_WIDGET = 0;
    static const int LIST_GRID_WIDGET = 1;
    static const int DLOG_ICON_WIDGET = 2;
    static const int DLOG_STATUS_WIDGET = 3;

    static Overlay overlay;
    static WidgetOverride widgetOverrides[NUM_WIDGETS];

    if (operation == data::DATA_OPERATION_GET_OVERLAY_DATA) {
        value = data::Value(&overlay, VALUE_TYPE_POINTER);
    } else if (operation == data::DATA_OPERATION_UPDATE_OVERLAY_DATA) {
        overlay.widgetOverrides = widgetOverrides;

        bool areListCountersVisible = list::g_numChannelsWithVisibleCounters > 0;
        bool isDlogVisible = !dlog_record::isIdle();

        int state = 0;
        if (areListCountersVisible || isDlogVisible) {
            if (list::g_numChannelsWithVisibleCounters > 0) {
                if (list::g_numChannelsWithVisibleCounters > 4) {
                    state = 1;
                } else if (list::g_numChannelsWithVisibleCounters > 2) {
                    state = 2;
                } else if (list::g_numChannelsWithVisibleCounters == 2) {
                    state = 3;
                } else {
                    state = 4;
                }
            }
            if (isDlogVisible) {
                state += 5;
            }
        }

        if (overlay.state != state) {
            overlay.state = state;
            if (state > 0) {
                WidgetCursor &widgetCursor = *(WidgetCursor *)value.getVoidPointer();

                const ContainerWidget *containerWidget = GET_WIDGET_PROPERTY(widgetCursor.widget, specific, const ContainerWidget *);

                const Widget *listIconWidget = GET_WIDGET_LIST_ELEMENT(containerWidget->widgets, LIST_ICON_WIDGET);
                const Widget *listGridWidget = GET_WIDGET_LIST_ELEMENT(containerWidget->widgets, LIST_GRID_WIDGET);
                const Widget *dlogIconWidget = GET_WIDGET_LIST_ELEMENT(containerWidget->widgets, DLOG_ICON_WIDGET);
                const Widget *dlogStatusWidget = GET_WIDGET_LIST_ELEMENT(containerWidget->widgets, DLOG_STATUS_WIDGET);

                overlay.width = widgetCursor.widget->w;
                overlay.height = widgetCursor.widget->h;

                if (list::g_numChannelsWithVisibleCounters > 0) {
                    widgetOverrides[LIST_ICON_WIDGET].isVisible = true;
                    widgetOverrides[LIST_ICON_WIDGET].x = listIconWidget->x;
                    widgetOverrides[LIST_ICON_WIDGET].y = listIconWidget->y;
                    widgetOverrides[LIST_ICON_WIDGET].w = listIconWidget->w;
                    widgetOverrides[LIST_ICON_WIDGET].h = listIconWidget->h;

                    widgetOverrides[LIST_GRID_WIDGET].isVisible = true;
                    widgetOverrides[LIST_GRID_WIDGET].x = listGridWidget->x;
                    widgetOverrides[LIST_GRID_WIDGET].y = listGridWidget->y;
                    widgetOverrides[LIST_GRID_WIDGET].w = listGridWidget->w;

                    if (list::g_numChannelsWithVisibleCounters > 4) {
                        widgetOverrides[LIST_GRID_WIDGET].h = 3 * listGridWidget->h;
                        overlay.height += 2 * listGridWidget->h;
                    } else if (list::g_numChannelsWithVisibleCounters > 2) {
                        widgetOverrides[LIST_GRID_WIDGET].h = 2 * listGridWidget->h;
                        overlay.height += listGridWidget->h;
                    } else {
                        widgetOverrides[LIST_GRID_WIDGET].h = listGridWidget->h;
                        if (list::g_numChannelsWithVisibleCounters == 1 && !isDlogVisible) {
                            widgetOverrides[LIST_GRID_WIDGET].w = listGridWidget->w / 2;
                            overlay.width -= listGridWidget->w / 2;
                        }
                    }
                } else {
                    widgetOverrides[LIST_ICON_WIDGET].isVisible = false;
                    widgetOverrides[LIST_GRID_WIDGET].isVisible = false;
                    overlay.height -= listGridWidget->h;
                }

                if (isDlogVisible) {
                    widgetOverrides[DLOG_ICON_WIDGET].isVisible = true;
                    widgetOverrides[DLOG_ICON_WIDGET].x = dlogIconWidget->x;
                    widgetOverrides[DLOG_ICON_WIDGET].y = overlay.height - (widgetCursor.widget->h - dlogIconWidget->y);
                    widgetOverrides[DLOG_ICON_WIDGET].w = listIconWidget->w;
                    widgetOverrides[DLOG_ICON_WIDGET].h = listIconWidget->h;

                    widgetOverrides[DLOG_STATUS_WIDGET].isVisible = true;
                    widgetOverrides[DLOG_STATUS_WIDGET].x = dlogStatusWidget->x;
                    widgetOverrides[DLOG_STATUS_WIDGET].y = overlay.height - (widgetCursor.widget->h - dlogStatusWidget->y);
                    widgetOverrides[DLOG_STATUS_WIDGET].w = dlogStatusWidget->w;
                    widgetOverrides[DLOG_STATUS_WIDGET].h = dlogStatusWidget->h;
                } else {
                    widgetOverrides[DLOG_ICON_WIDGET].isVisible = false;
                    widgetOverrides[DLOG_STATUS_WIDGET].isVisible = false;
                    overlay.height -= dlogStatusWidget->h;
                }
            }
        }


        value = data::Value(&overlay, VALUE_TYPE_POINTER);
    }
}

void data_nondrag_overlay(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
}

void data_dlog_state(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
#if OPTION_SD_CARD
    if (operation == data::DATA_OPERATION_GET) {
        value = dlog_record::getState();
    }
#endif
}

void data_dlog_toggle_state(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
#if OPTION_SD_CARD
    if (operation == data::DATA_OPERATION_GET) {
        if (dlog_record::isIdle()) {
            value = 0;
        } else if (dlog_record::isExecuting()) {
            value = 1;
        } else if (dlog_record::isInitiated() && dlog_record::g_parameters.triggerSource == trigger::SOURCE_MANUAL) {
            value = 2;
        } else {
            value = 3;
        }
    }
#endif
}

void data_is_show_live_recording(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
#if OPTION_SD_CARD
    if (operation == data::DATA_OPERATION_GET) {
        dlog_view::Recording &recording = dlog_view::getRecording();
        value = &recording == &dlog_record::g_recording;
    }
#endif
}

void data_channel_history_values(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == DATA_OPERATION_YT_DATA_GET_GET_VALUE_FUNC) {
        value = Channel::getChannelHistoryValueFuncs(cursor.i);
    } else if (operation == DATA_OPERATION_YT_DATA_GET_REFRESH_COUNTER) {
        value = Value(0, VALUE_TYPE_UINT32);
    } else if (operation == DATA_OPERATION_YT_DATA_GET_SIZE) {
        value = Value(g_appContext->getNumHistoryValues(DATA_ID_CHANNEL_DISPLAY_VALUE1), VALUE_TYPE_UINT32);
    } else if (operation == DATA_OPERATION_YT_DATA_GET_POSITION) {
        value = Value(g_appContext->getCurrentHistoryValuePosition(cursor, DATA_ID_CHANNEL_DISPLAY_VALUE1) - 1, VALUE_TYPE_UINT32);
    } else if (operation == DATA_OPERATION_YT_DATA_GET_STYLE) {
        int iChannel = cursor.i >= 0 ? cursor.i : (g_channel ? g_channel->channelIndex : 0);
        Channel &channel = Channel::get(iChannel);
        if (channel_dispatcher::getChannelMode(channel) == channel_dispatcher::CHANNEL_MODE_UR) {
            value = Value(STYLE_ID_YT_GRAPH_UNREGULATED, VALUE_TYPE_UINT16);
        } else {
            if (value.getUInt8() == 0) {
                value = Value(
                    channel.flags.displayValue1 == DISPLAY_VALUE_VOLTAGE ? STYLE_ID_YT_GRAPH_U_DEFAULT : 
                    channel.flags.displayValue1 == DISPLAY_VALUE_CURRENT ? STYLE_ID_YT_GRAPH_I_DEFAULT :
                    STYLE_ID_YT_GRAPH_P_DEFAULT, 
                    VALUE_TYPE_UINT16);
            } else {
                value = Value(
                    channel.flags.displayValue2 == DISPLAY_VALUE_VOLTAGE ? STYLE_ID_YT_GRAPH_U_DEFAULT :
                    channel.flags.displayValue2 == DISPLAY_VALUE_CURRENT ? STYLE_ID_YT_GRAPH_I_DEFAULT :
                    STYLE_ID_YT_GRAPH_P_DEFAULT,
                    VALUE_TYPE_UINT16);
            }
            
        }
    } else if (operation == DATA_OPERATION_YT_DATA_GET_MIN) {
        value = getMin(cursor, value.getUInt8() == 0 ? DATA_ID_CHANNEL_DISPLAY_VALUE1 : DATA_ID_CHANNEL_DISPLAY_VALUE2);
    } else if (operation == DATA_OPERATION_YT_DATA_GET_MAX) {
        value = getMax(cursor, value.getUInt8() == 0 ? DATA_ID_CHANNEL_DISPLAY_VALUE1 : DATA_ID_CHANNEL_DISPLAY_VALUE2);
    } else if (operation == DATA_OPERATION_YT_DATA_GET_GRAPH_UPDATE_METHOD) {
        value = Value(psu::persist_conf::devConf.ytGraphUpdateMethod, VALUE_TYPE_UINT8);
    }
}

void data_dlog_voltage_enabled(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
#if OPTION_SD_CARD
    if (operation == data::DATA_OPERATION_GET) {
        value = dlog_record::g_guiParameters.logVoltage[cursor.i];
    }
#endif
}

void data_dlog_current_enabled(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
#if OPTION_SD_CARD
    if (operation == data::DATA_OPERATION_GET) {
        value = dlog_record::g_guiParameters.logCurrent[cursor.i];
    }
#endif
}

void data_dlog_power_enabled(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
#if OPTION_SD_CARD
    if (operation == data::DATA_OPERATION_GET) {
        value = dlog_record::g_guiParameters.logPower[cursor.i];
    }
#endif
}

void data_dlog_period(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
#if OPTION_SD_CARD
    if (operation == data::DATA_OPERATION_GET) {
        value = MakeValue(dlog_record::g_guiParameters.period, UNIT_SECOND);
    } else if (operation == data::DATA_OPERATION_GET_UNIT) {
        value = UNIT_SECOND;
    } else if (operation == data::DATA_OPERATION_GET_MIN) {
        value = MakeValue(dlog_record::PERIOD_MIN, UNIT_SECOND);
    } else if (operation == data::DATA_OPERATION_GET_MAX) {
        value = MakeValue(dlog_record::PERIOD_MAX, UNIT_SECOND);
    } else if (operation == data::DATA_OPERATION_SET) {
        dlog_record::g_guiParameters.period = value.getFloat();
    }
#endif
}

void data_dlog_duration(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
#if OPTION_SD_CARD
    if (operation == data::DATA_OPERATION_GET) {
        value = MakeValue(dlog_record::g_guiParameters.time, UNIT_SECOND);
    } else if (operation == data::DATA_OPERATION_GET_UNIT) {
        value = UNIT_SECOND;
    } else if (operation == data::DATA_OPERATION_GET_MIN) {
        value = MakeValue(dlog_record::TIME_MIN, UNIT_SECOND);
    } else if (operation == data::DATA_OPERATION_GET_MAX) {
        value = MakeValue(INFINITY, UNIT_SECOND);
    } else if (operation == data::DATA_OPERATION_SET) {
        dlog_record::g_guiParameters.time = value.getFloat();
    }
#endif
}

void data_dlog_file_name(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
#if OPTION_SD_CARD
    if (operation == data::DATA_OPERATION_GET) {
        value = dlog_record::g_guiParameters.filePath;
    } else if (operation == data::DATA_OPERATION_SET) {
        strcpy(dlog_record::g_guiParameters.filePath, value.getString());
    } else if (operation == data::DATA_OPERATION_GET_MAX) {
        value = Value(MAX_PATH_LENGTH, VALUE_TYPE_UINT32);
    }
#endif
}

void data_dlog_start_enabled(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
#if OPTION_SD_CARD
    if (operation == data::DATA_OPERATION_GET) {
        value = dlog_record::checkDlogParameters(dlog_record::g_guiParameters, true) == SCPI_RES_OK ? 1 : 0;
    }
#endif
}

void data_dlog_view_state(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
#if OPTION_SD_CARD
    if (operation == DATA_OPERATION_GET) {
        value = dlog_view::getState();
    }
#endif
}

void data_recording_ready(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
#if OPTION_SD_CARD
    if (operation == DATA_OPERATION_GET) {
        value = dlog_record::isExecuting() || dlog_record::getLatestFilePath() ? 1 : 0;
    }
#endif
}

void data_is_show_latest_recording(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = dlog_view::g_showLatest;
    }
}

void data_recording(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
#if OPTION_SD_CARD
    dlog_view::Recording &recording = dlog_view::getRecording();

    if (operation == DATA_OPERATION_YT_DATA_GET_REFRESH_COUNTER) {
        value = Value(recording.refreshCounter, VALUE_TYPE_UINT32);
    } else if (operation == DATA_OPERATION_YT_DATA_GET_GET_VALUE_FUNC) {
        value = recording.getValue;
    } else if (operation == DATA_OPERATION_YT_DATA_VALUE_IS_VISIBLE) {
        value = Value(recording.dlogValues[value.getUInt8()].isVisible);
    } else if (operation == DATA_OPERATION_YT_DATA_GET_SIZE) {
        value = Value(recording.size, VALUE_TYPE_UINT32);
    } else if (operation == DATA_OPERATION_YT_DATA_GET_POSITION) {
        float position;
        if (&recording == &dlog_record::g_recording) {
            value = Value(recording.size - recording.pageSize, VALUE_TYPE_UINT32);
        } else {
            position = recording.timeOffset / recording.parameters.period;
            if (position < 0) {
                value = Value(0, VALUE_TYPE_UINT32);
            } else if (position > recording.size - recording.pageSize) {
                value = Value(recording.size - recording.pageSize, VALUE_TYPE_UINT32);
            } else {
                value = Value((uint32_t)roundf(position), VALUE_TYPE_UINT32);
            }
        }
    } else if (operation == DATA_OPERATION_YT_DATA_SET_POSITION) {
        int32_t newPosition = value.getUInt32();
        if (newPosition < 0) {
            newPosition = 0;
        } else {
            if (newPosition + recording.pageSize > recording.size) {
                newPosition = recording.size - recording.pageSize;
            }
        }
        dlog_view::changeTimeOffset(recording, newPosition * recording.parameters.period);
    } else if (operation == DATA_OPERATION_YT_DATA_GET_PAGE_SIZE) {
        value = Value(recording.pageSize, VALUE_TYPE_UINT32);
    } else if (operation == DATA_OPERATION_YT_DATA_GET_STYLE) {
        value = Value(g_ytGraphStyles[value.getUInt8() % (sizeof(g_ytGraphStyles) / sizeof(uint16_t))], VALUE_TYPE_UINT16);
    } else if (operation == DATA_OPERATION_YT_DATA_GET_HORZ_DIVISIONS) {
        value = dlog_view::NUM_HORZ_DIVISIONS;
    } else if (operation == DATA_OPERATION_YT_DATA_GET_VERT_DIVISIONS) {
        value = dlog_view::NUM_VERT_DIVISIONS;
    } else if (operation == DATA_OPERATION_YT_DATA_GET_DIV) {
        value = recording.dlogValues[value.getUInt8()].div;
    } else if (operation == DATA_OPERATION_YT_DATA_GET_OFFSET) {
        value = recording.dlogValues[value.getUInt8()].offset;
    } else if (operation == DATA_OPERATION_YT_DATA_GET_GRAPH_UPDATE_METHOD) {
        value = YT_GRAPH_UPDATE_METHOD_STATIC;
    } else if (operation == DATA_OPERATION_YT_DATA_GET_PERIOD) {
        value = Value(recording.parameters.period, UNIT_SECOND);
    } else if (operation == DATA_OPERATION_YT_DATA_IS_CURSOR_VISIBLE) {
        value = &recording != &dlog_record::g_recording;
    } else if (operation == DATA_OPERATION_YT_DATA_GET_CURSOR_OFFSET) {
        value = Value(recording.cursorOffset, VALUE_TYPE_UINT32);
    } else if (operation == DATA_OPERATION_YT_DATA_TOUCH_DRAG) {
        TouchDrag *touchDrag = (TouchDrag *)value.getVoidPointer();
        
        static float valueAtTouchDown;
        static int xAtTouchDown;
        static int yAtTouchDown;
        
        if (g_focusDataId == DATA_ID_DLOG_VISIBLE_VALUE_OFFSET) {
            dlog_view::DlogValueParams *dlogValueParams = dlog_view::getVisibleDlogValueParams(recording, g_focusCursor.i);

            if (touchDrag->type == EVENT_TYPE_TOUCH_DOWN) {
                valueAtTouchDown = dlogValueParams->offset.getFloat();
                yAtTouchDown = touchDrag->y;
            } else {
                float newOffset = valueAtTouchDown + dlogValueParams->div.getFloat() * (yAtTouchDown - touchDrag->y) * dlog_view::NUM_VERT_DIVISIONS / dlog_view::VIEW_HEIGHT;

                float min = getMin(g_focusCursor, DATA_ID_DLOG_VISIBLE_VALUE_OFFSET).getFloat();
                float max = getMax(g_focusCursor, DATA_ID_DLOG_VISIBLE_VALUE_OFFSET).getFloat();

                newOffset = roundPrec(clamp(newOffset, min, max), dlog_view::VALUE_PREC);
                
                dlogValueParams->offset = Value(newOffset, dlogValueParams->offset.getUnit());
            }
        } else if (g_focusDataId == DATA_ID_DLOG_VISIBLE_VALUE_DIV) {
            dlog_view::DlogValueParams *dlogValueParams = dlog_view::getVisibleDlogValueParams(recording, g_focusCursor.i);

            if (touchDrag->type == EVENT_TYPE_TOUCH_DOWN) {
                valueAtTouchDown = dlogValueParams->div.getFloat();

                //uint32_t position = ytDataGetPosition(cursor, DATA_ID_RECORDING);
                //Value::YtDataGetValueFunctionPointer ytDataGetValue = data::ytDataGetGetValueFunc(cursor, DATA_ID_RECORDING);
                //for (int i = position + touchDrag->x - 10; i < position + touchDrag->x + 10; )

                yAtTouchDown = touchDrag->y;
            } else {
                float scale = 1.0f * (dlog_view::VIEW_HEIGHT - yAtTouchDown) / (dlog_view::VIEW_HEIGHT - touchDrag->y);

                float newDiv = valueAtTouchDown * scale;

                float min = getMin(g_focusCursor, DATA_ID_DLOG_VISIBLE_VALUE_DIV).getFloat();
                float max = getMax(g_focusCursor, DATA_ID_DLOG_VISIBLE_VALUE_DIV).getFloat();

                newDiv = roundPrec(clamp(newDiv, min, max), dlog_view::VALUE_PREC);

                dlogValueParams->div = Value(newDiv, dlogValueParams->div.getUnit());
            }
        } else if (g_focusDataId == DATA_ID_DLOG_TIME_DIV) {
            if (touchDrag->type == EVENT_TYPE_TOUCH_DOWN) {
                valueAtTouchDown = recording.timeDiv;
                xAtTouchDown = touchDrag->x;
            } else {
                float scale = 1.0f * (dlog_view::VIEW_WIDTH - touchDrag->x) / (dlog_view::VIEW_WIDTH - xAtTouchDown);

                float newDiv = valueAtTouchDown * scale;

                float min = getMin(g_focusCursor, DATA_ID_DLOG_TIME_DIV).getFloat();
                float max = getMax(g_focusCursor, DATA_ID_DLOG_TIME_DIV).getFloat();

                newDiv = roundPrec(clamp(newDiv, min, max), dlog_view::TIME_PREC);

                dlog_view::changeTimeDiv(recording, newDiv);
            }
        } else {
            recording.cursorOffset = MIN(dlog_view::VIEW_WIDTH - 1, MAX(touchDrag->x, 0));
        }
    } else if (operation == DATA_OPERATION_YT_DATA_GET_CURSOR_TIME) {
        value = Value((ytDataGetPosition(cursor, DATA_ID_RECORDING) + recording.cursorOffset) * recording.parameters.period, UNIT_SECOND);
    }
#endif
}

void data_dlog_overlay(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
#if OPTION_SD_CARD
    static const int NUM_WIDGETS = 3;

    static const int SHOW_OPTIONS_BUTTON = 0;
    static const int LABELS_CONTAINER_WIDGET = 1;
    static const int DLOG_VALUES_LIST_WIDGET = 2;

    static Overlay overlay;
    static WidgetOverride widgetOverrides[NUM_WIDGETS];

    if (operation == data::DATA_OPERATION_GET_OVERLAY_DATA) {
        value = data::Value(&overlay, VALUE_TYPE_POINTER);
    } else if (operation == data::DATA_OPERATION_UPDATE_OVERLAY_DATA) {
        dlog_view::Recording &recording = dlog_view::getRecording();
        
        int state = 0;
        int numVisibleDlogValues = 0;
        
        if (&recording != &dlog_record::g_recording) {
            state = numVisibleDlogValues = dlog_view::getNumVisibleDlogValues(recording);

            if (dlog_view::g_overlayMinimized) {
                state |= 0x80;
            }

            state |= 0x100;
        }

        if (overlay.state != state) {
            overlay.state = state;

            if (state > 0) {
                overlay.widgetOverrides = widgetOverrides;

                WidgetCursor &widgetCursor = *(WidgetCursor *)value.getVoidPointer();

                const ContainerWidget *containerWidget = GET_WIDGET_PROPERTY(widgetCursor.widget, specific, const ContainerWidget *);

                const Widget *showOptionsButtonWidget = GET_WIDGET_LIST_ELEMENT(containerWidget->widgets, SHOW_OPTIONS_BUTTON);
                const Widget *labelsContainerWidget = GET_WIDGET_LIST_ELEMENT(containerWidget->widgets, LABELS_CONTAINER_WIDGET);
                const Widget *dlogValuesListWidget = GET_WIDGET_LIST_ELEMENT(containerWidget->widgets, DLOG_VALUES_LIST_WIDGET);

                overlay.width = widgetCursor.widget->w;

                widgetOverrides[SHOW_OPTIONS_BUTTON].isVisible = true;
                widgetOverrides[SHOW_OPTIONS_BUTTON].x = showOptionsButtonWidget->x;
                widgetOverrides[SHOW_OPTIONS_BUTTON].y = showOptionsButtonWidget->y;
                widgetOverrides[SHOW_OPTIONS_BUTTON].w = showOptionsButtonWidget->w;
                widgetOverrides[SHOW_OPTIONS_BUTTON].h = showOptionsButtonWidget->h;

                if (state & 0x80) {
                    // minimized
                    widgetOverrides[LABELS_CONTAINER_WIDGET].isVisible = false;
                    widgetOverrides[DLOG_VALUES_LIST_WIDGET].isVisible = false;

                    overlay.width = showOptionsButtonWidget->w;
                    overlay.height = showOptionsButtonWidget->h;
                } else {
                    widgetOverrides[LABELS_CONTAINER_WIDGET].isVisible = true;
                    widgetOverrides[LABELS_CONTAINER_WIDGET].x = labelsContainerWidget->x;
                    widgetOverrides[LABELS_CONTAINER_WIDGET].y = labelsContainerWidget->y;
                    widgetOverrides[LABELS_CONTAINER_WIDGET].w = labelsContainerWidget->w;
                    widgetOverrides[LABELS_CONTAINER_WIDGET].h = labelsContainerWidget->h;

                    widgetOverrides[DLOG_VALUES_LIST_WIDGET].isVisible = true;
                    widgetOverrides[DLOG_VALUES_LIST_WIDGET].x = dlogValuesListWidget->x;
                    widgetOverrides[DLOG_VALUES_LIST_WIDGET].y = dlogValuesListWidget->y;
                    widgetOverrides[DLOG_VALUES_LIST_WIDGET].w = dlogValuesListWidget->w;
                    widgetOverrides[DLOG_VALUES_LIST_WIDGET].h = numVisibleDlogValues * dlogValuesListWidget->h;

                    overlay.width = widgetCursor.widget->w;
                    overlay.height = widgetOverrides[LABELS_CONTAINER_WIDGET].h + widgetOverrides[DLOG_VALUES_LIST_WIDGET].h;
                }
            }
        }

        value = data::Value(&overlay, VALUE_TYPE_POINTER);
    }
#endif
}

void data_dlog_visible_values(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
#if OPTION_SD_CARD
    if (operation == data::DATA_OPERATION_COUNT) {
        dlog_view::Recording &recording = dlog_view::getRecording();
        value = getNumVisibleDlogValues(recording);
    }
#endif
}

void data_dlog_visible_value_label(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        dlog_view::Recording &recording = dlog_view::getRecording();
        dlog_view::DlogValueParams *dlogValueParams = dlog_view::getVisibleDlogValueParams(recording, cursor.i);
        value = Value(dlogValueParams->dlogValueType, VALUE_TYPE_DLOG_VALUE_LABEL);
    }
}

void data_dlog_visible_value_div(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
#if OPTION_SD_CARD
    dlog_view::Recording &recording = dlog_view::getRecording();
    dlog_view::DlogValueParams *dlogValueParams = dlog_view::getVisibleDlogValueParams(recording, cursor.i);

    if (operation == data::DATA_OPERATION_GET) {
        bool focused = g_focusCursor == cursor && g_focusDataId == DATA_ID_DLOG_VISIBLE_VALUE_DIV;
        if (focused && g_focusEditValue.getType() != VALUE_TYPE_NONE) {
            value = g_focusEditValue;
        } else {
            value = dlogValueParams->div;
        }
    } else if (operation == data::DATA_OPERATION_GET_MIN) {
        value = Value(0.1f, dlogValueParams->div.getUnit());
    } else if (operation == data::DATA_OPERATION_GET_MAX) {
        value = Value(100.0f, dlogValueParams->div.getUnit());
    } else if (operation == data::DATA_OPERATION_SET) {
        dlogValueParams->div = Value(roundPrec(value.getFloat(), dlog_view::VALUE_PREC), value.getUnit());
    } else if (operation == data::DATA_OPERATION_GET_NAME) {
        value = "Div";
    } else if (operation == data::DATA_OPERATION_GET_UNIT) {
        value = dlogValueParams->div.getUnit();
    } 
#endif
}

void data_dlog_visible_value_offset(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
#if OPTION_SD_CARD
    dlog_view::Recording &recording = dlog_view::getRecording();
    dlog_view::DlogValueParams *dlogValueParams = dlog_view::getVisibleDlogValueParams(recording, cursor.i);

    if (operation == data::DATA_OPERATION_GET) {
        bool focused = g_focusCursor == cursor && g_focusDataId == DATA_ID_DLOG_VISIBLE_VALUE_OFFSET;
        if (focused && g_focusEditValue.getType() != VALUE_TYPE_NONE) {
            value = g_focusEditValue;
        } else {
            value = dlogValueParams->offset;
        }
    } else if (operation == data::DATA_OPERATION_GET_MIN) {
        value = Value(-100.0f, dlogValueParams->offset.getUnit());
    } else if (operation == data::DATA_OPERATION_GET_MAX) {
        value = Value(100.0f, dlogValueParams->offset.getUnit());
    } else if (operation == data::DATA_OPERATION_SET) {
        dlogValueParams->offset = Value(roundPrec(value.getFloat(), dlog_view::VALUE_PREC), value.getUnit());
    } else if (operation == data::DATA_OPERATION_GET_NAME) {
        value = "Offset";
    } else if (operation == data::DATA_OPERATION_GET_UNIT) {
        value = dlogValueParams->offset.getUnit();
    } 
#endif
}

void data_dlog_time_offset(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
#if OPTION_SD_CARD
    dlog_view::Recording &recording = dlog_view::getRecording();

    if (operation == data::DATA_OPERATION_GET) {
        bool focused = g_focusCursor == cursor && g_focusDataId == DATA_ID_DLOG_TIME_OFFSET;
        if (focused && g_focusEditValue.getType() != VALUE_TYPE_NONE) {
            value = g_focusEditValue;
        } else {
            value = Value(recording.timeOffset, UNIT_SECOND);
        }
    } else if (operation == data::DATA_OPERATION_GET_MIN) {
        value = Value(0.0f, UNIT_SECOND);
    } else if (operation == data::DATA_OPERATION_GET_MAX) {
        value = Value(dlog_view::getMaxTimeOffset(recording), UNIT_SECOND);
    } else if (operation == data::DATA_OPERATION_SET) {
        dlog_view::changeTimeOffset(recording, value.getFloat());
    } else if (operation == data::DATA_OPERATION_GET_NAME) {
        value = "Time Offset";
    } else if (operation == data::DATA_OPERATION_GET_UNIT) {
        value = UNIT_SECOND;
    } 
#endif
}

void data_dlog_time_div(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
#if OPTION_SD_CARD
    dlog_view::Recording &recording = dlog_view::getRecording();

    if (operation == data::DATA_OPERATION_GET) {
        bool focused = g_focusCursor == cursor && g_focusDataId == DATA_ID_DLOG_TIME_DIV;
        if (focused && g_focusEditValue.getType() != VALUE_TYPE_NONE) {
            value = g_focusEditValue;
        } else {
            value = Value(recording.timeDiv, UNIT_SECOND);
        }
    } else if (operation == data::DATA_OPERATION_GET_MIN || operation == data::DATA_OPERATION_GET_DEF) {
        value = Value(recording.timeDivMin, UNIT_SECOND);
    } else if (operation == data::DATA_OPERATION_GET_MAX) {
        value = Value(recording.timeDivMax, UNIT_SECOND);
    } else if (operation == data::DATA_OPERATION_SET) {
        dlog_view::changeTimeDiv(recording, value.getFloat());
    } else if (operation == data::DATA_OPERATION_GET_NAME) {
        value = "Time Div";
    } else if (operation == data::DATA_OPERATION_GET_UNIT) {
        value = UNIT_SECOND;
    }
#endif
}

void data_dlog_visible_value_cursor(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
#if OPTION_SD_CARD
    if (operation == data::DATA_OPERATION_GET) {
        dlog_view::Recording &recording = dlog_view::getRecording();
        int dlogValueIndex = dlog_view::getVisibleDlogValueIndex(recording, cursor.i);
        auto ytDataGetValue = data::ytDataGetGetValueFunc(cursor, DATA_ID_RECORDING);
        float max;
        float min = ytDataGetValue(ytDataGetPosition(cursor, DATA_ID_RECORDING) + recording.cursorOffset, dlogValueIndex, &max);
        value = Value(
            (min + max) / 2,
            recording.dlogValues[dlogValueIndex].div.getUnit()
        );

    }
#endif
}

void data_dlog_current_time(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
#if OPTION_SD_CARD
    if (operation == data::DATA_OPERATION_GET) {
        value = Value(0, VALUE_TYPE_DLOG_CURRENT_TIME);
    }
#endif
}

void data_dlog_time_duration(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
#if OPTION_SD_CARD
    if (operation == data::DATA_OPERATION_GET) {
        value = Value(dlog_view::getDuration(dlog_view::getRecording()), UNIT_SECOND);
    }
#endif
}
void data_dlog_file_length(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
#if OPTION_SD_CARD
    if (operation == data::DATA_OPERATION_GET) {
        value = Value(dlog_record::g_fileLength, VALUE_TYPE_FILE_LENGTH);
    }
#endif
}

void data_dlog_all_values(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
#if OPTION_SD_CARD
    if (operation == data::DATA_OPERATION_COUNT) {
        value = MAX_NUM_OF_Y_VALUES;
    }
#endif
}

void data_dlog_value_label(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
#if OPTION_SD_CARD
    if (operation == data::DATA_OPERATION_GET) {
        dlog_view::Recording &recording = dlog_view::getRecording();
        value = Value(recording.dlogValues[cursor.i].dlogValueType, VALUE_TYPE_DLOG_VALUE_LABEL);
    }
#endif
}

void data_dlog_value_state(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
#if OPTION_SD_CARD
    if (operation == data::DATA_OPERATION_GET) {
        dlog_view::Recording &recording = dlog_view::getRecording();
        value = cursor.i < recording.totalDlogValues ? recording.dlogValues[cursor.i].isVisible : 2;
    }
#endif
}

void data_dlog_view_overlay_minimized(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
#if OPTION_SD_CARD
    if (operation == data::DATA_OPERATION_GET) {
        value = dlog_view::g_overlayMinimized;
    }
#endif
}

void data_file_manager_current_directory(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
#if OPTION_SD_CARD
    if (operation == data::DATA_OPERATION_GET) {
        value = file_manager::getCurrentDirectory();
    }
#endif
}

void data_file_manager_state(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
#if OPTION_SD_CARD
    if (operation == data::DATA_OPERATION_GET) {
        value = file_manager::getState();
    }
#endif
}

void data_file_manager_is_root_directory(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
#if OPTION_SD_CARD
    if (operation == data::DATA_OPERATION_GET) {
        value = file_manager::isRootDirectory() ? 1 : 0;
    }
#endif
}

void data_file_manager_files(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
#if OPTION_SD_CARD
    if (operation == data::DATA_OPERATION_COUNT) {
        value = (int)file_manager::getFilesCount();
    } else if (operation == DATA_OPERATION_YT_DATA_GET_SIZE) {
        value = Value(file_manager::getFilesCount(), VALUE_TYPE_UINT32);
    } else if (operation == DATA_OPERATION_YT_DATA_GET_POSITION) {
        value = Value(file_manager::getFilesStartPosition(), VALUE_TYPE_UINT32);
    } else if (operation == DATA_OPERATION_YT_DATA_SET_POSITION) {
        file_manager::setFilesStartPosition(value.getUInt32());
    } else if (operation == DATA_OPERATION_YT_DATA_GET_PAGE_SIZE) {
        value = Value(file_manager::FILES_PAGE_SIZE, VALUE_TYPE_UINT32);
    }
#endif
}

void data_file_manager_is_directory(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
#if OPTION_SD_CARD
    if (operation == data::DATA_OPERATION_GET) {
        value = file_manager::isDirectory(cursor.i) ? 1 : 0;
    }
#endif
}

void data_file_manager_file_type(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
#if OPTION_SD_CARD
    if (operation == data::DATA_OPERATION_GET) {
        value = file_manager::getFileType(cursor.i);
    }
#endif
}

void data_file_manager_file_name(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
#if OPTION_SD_CARD
    if (operation == data::DATA_OPERATION_GET) {
        value = Value(file_manager::getFileName(cursor.i), VALUE_TYPE_STR, STRING_OPTIONS_FILE_ELLIPSIS);
    }
#endif
}

void data_file_manager_file_size(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
#if OPTION_SD_CARD
    if (operation == data::DATA_OPERATION_GET) {
        value = Value(file_manager::getFileSize(cursor.i), VALUE_TYPE_FILE_LENGTH);
    }
#endif
}

void data_file_manager_file_date_time(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
#if OPTION_SD_CARD
    if (operation == data::DATA_OPERATION_GET) {
        value = Value(file_manager::getFileDataTime(cursor.i), VALUE_TYPE_FILE_DATE_TIME);
    }
#endif
}

void data_file_manager_open_file_enabled(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
#if OPTION_SD_CARD
    if (operation == data::DATA_OPERATION_GET) {
        value = file_manager::isOpenFileEnabled();
    }
#endif
}

void data_file_manager_upload_file_enabled(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
#if OPTION_SD_CARD
    if (operation == data::DATA_OPERATION_GET) {
        value = file_manager::isUploadFileEnabled();
    }
#endif
}

void data_file_manager_rename_file_enabled(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
#if OPTION_SD_CARD
    if (operation == data::DATA_OPERATION_GET) {
        value = file_manager::isRenameFileEnabled();
    }
#endif
}

void data_file_manager_delete_file_enabled(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
#if OPTION_SD_CARD
    if (operation == data::DATA_OPERATION_GET) {
        value = file_manager::isDeleteFileEnabled();
    }
#endif
}

void data_file_manager_opened_image(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
#if OPTION_SD_CARD
    if (operation == data::DATA_OPERATION_GET_BITMAP_PIXELS) {
        value = Value(file_manager::getOpenedImagePixels(), VALUE_TYPE_POINTER);
    } else if (operation == data::DATA_OPERATION_GET_BITMAP_WIDTH) {
        value = Value(480, VALUE_TYPE_UINT16);
    } else if (operation == data::DATA_OPERATION_GET_BITMAP_HEIGHT) {
        value = Value(272, VALUE_TYPE_UINT16);
    }
#endif
}

} // namespace gui
} // namespace eez

#endif
