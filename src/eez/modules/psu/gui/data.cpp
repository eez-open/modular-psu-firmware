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

#include <math.h>
#include <stdio.h>

#include <eez/system.h>
#include <eez/util.h>
#include <eez/index.h>
#include <eez/file_type.h>
#include <eez/mp.h>
#include <eez/memory.h>

#include <eez/gui/gui.h>
#include <eez/gui/widgets/container.h>
#include <eez/gui/widgets/yt_graph.h>

#if OPTION_FAN
#include <eez/modules/aux_ps/fan.h>
#endif

#include <eez/modules/bp3c/flash_slave.h>

#include <eez/modules/psu/psu.h>
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
#include <eez/mqtt.h>
#endif
#include <eez/modules/psu/event_queue.h>
#include <eez/modules/psu/list_program.h>
#include <eez/modules/psu/ramp.h>
#include <eez/modules/psu/serial_psu.h>
#include <eez/modules/psu/temperature.h>
#include <eez/modules/psu/trigger.h>
#include <eez/modules/psu/ontime.h>
#include <eez/modules/psu/sd_card.h>

#include <eez/modules/psu/gui/psu.h>
#include <eez/modules/psu/gui/calibration.h>
#include <eez/modules/psu/gui/edit_mode.h>
#include <eez/modules/psu/gui/file_manager.h>
#include <eez/modules/psu/gui/keypad.h>
#include <eez/modules/psu/gui/page_ch_settings.h>
#include <eez/modules/psu/gui/page_sys_settings.h>
#include <eez/modules/psu/gui/page_user_profiles.h>

using namespace eez::psu;
using namespace eez::psu::gui;

#if defined(EEZ_PLATFORM_STM32)
extern bool g_isResetByIWDG;
#endif

namespace eez {
namespace gui {

#define ENUM_DEFINITION(NAME) g_enumDefinition_##NAME,
const EnumItem *g_enumDefinitions[] = { 
	ENUM_DEFINITIONS
};
#undef ENUM_DEFINITION

EnumItem g_enumDefinition_CHANNEL_DISPLAY_VALUE[] = {
    { DISPLAY_VALUE_VOLTAGE, "Voltage (V)" },
    { DISPLAY_VALUE_CURRENT, "Current (A)" },
    { DISPLAY_VALUE_POWER, "Power (W)" },
    { 0, 0 }
};

EnumItem g_enumDefinition_CHANNEL_TRIGGER_MODE[] = {
    { TRIGGER_MODE_FIXED, "Fixed" },
    { TRIGGER_MODE_LIST, "List" },
    { TRIGGER_MODE_STEP, "Step" },
    { 0, 0 } 
};

EnumItem g_enumDefinition_TRIGGER_SOURCE[] = {
    { trigger::SOURCE_BUS, "Bus" },
    { trigger::SOURCE_IMMEDIATE, "Immediate" },
    { trigger::SOURCE_MANUAL, "Manual" },
    { trigger::SOURCE_PIN1, "Pin1" },
    { trigger::SOURCE_PIN2, "Pin2" },
    { 0, 0 }
};

EnumItem g_enumDefinition_CHANNEL_CURRENT_RANGE_SELECTION_MODE[] = {
    { CURRENT_RANGE_SELECTION_USE_BOTH, "Best" },
    { CURRENT_RANGE_SELECTION_ALWAYS_HIGH, "High (5A)" },
    { CURRENT_RANGE_SELECTION_ALWAYS_LOW, "Low (50mA)" },
    { 0, 0 }
};

EnumItem g_enumDefinition_CHANNEL_CURRENT_RANGE[] = {
    { CURRENT_RANGE_HIGH, "High" },
    { CURRENT_RANGE_LOW, "Low" },
    { 0, 0 }
};

EnumItem g_enumDefinition_CHANNEL_TRIGGER_ON_LIST_STOP[] = {
    { TRIGGER_ON_LIST_STOP_OUTPUT_OFF, "Output OFF" },
    { TRIGGER_ON_LIST_STOP_SET_TO_FIRST_STEP, "Set to first step" },
    { TRIGGER_ON_LIST_STOP_SET_TO_LAST_STEP, "Set to last step" },
    { TRIGGER_ON_LIST_STOP_STANDBY, "Standby" },
    { 0, 0 }
};

EnumItem g_enumDefinition_IO_PINS_POLARITY[] = {
    { io_pins::POLARITY_NEGATIVE, "Negative" },
    { io_pins::POLARITY_POSITIVE, "Positive" },
    { 0, 0 }
};

EnumItem g_enumDefinition_IO_PINS_INPUT_FUNCTION[] = {
    { io_pins::FUNCTION_NONE, "None" },
    { io_pins::FUNCTION_INPUT, "Input" },
    { io_pins::FUNCTION_INHIBIT, "Inhibit" },
    { io_pins::FUNCTION_TINPUT, "Trigger input", "Tinput" },
    { 0, 0 }
};

EnumItem g_enumDefinition_IO_PINS_OUTPUT_FUNCTION[] = {
    { io_pins::FUNCTION_NONE, "None" },
    { io_pins::FUNCTION_OUTPUT, "Output" },
    { io_pins::FUNCTION_FAULT, "Fault" },
    { io_pins::FUNCTION_ON_COUPLE, "Channel ON couple", "ONcoup" },
    { io_pins::FUNCTION_TOUTPUT, "Trigger output", "Toutput" },
    { 0, 0 }
};

EnumItem g_enumDefinition_IO_PINS_OUTPUT2_FUNCTION[] = {
    { io_pins::FUNCTION_NONE, "None" },
    { io_pins::FUNCTION_OUTPUT, "Output" },
    { io_pins::FUNCTION_FAULT, "Fault" },
    { io_pins::FUNCTION_ON_COUPLE, "Channel ON couple", "ONcoup" },
    { io_pins::FUNCTION_TOUTPUT, "Trigger output", "Toutput" },
    { io_pins::FUNCTION_PWM, "PWM" },
    { 0, 0 }
};

EnumItem g_enumDefinition_DST_RULE[] = {
    { datetime::DST_RULE_OFF, "Off" },
    { datetime::DST_RULE_EUROPE, "Europe" },
    { datetime::DST_RULE_USA, "USA" },
    { datetime::DST_RULE_AUSTRALIA, "Australia" },
    { 0, 0 }
};

EnumItem g_enumDefinition_DATE_TIME_FORMAT[] = {
    { datetime::FORMAT_DMY_24, "DD-MM-YY 24H" },
    { datetime::FORMAT_MDY_24, "MM-DD-YY 24H" },
    { datetime::FORMAT_DMY_12, "DD-MM-YY 12H" },
    { datetime::FORMAT_MDY_12, "MM-DD-YY 12H" },
    { 0, 0 }
};

EnumItem g_enumDefinition_USER_SWITCH_ACTION[] = {
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

EnumItem g_enumDefinition_FILE_MANAGER_SORT_BY[] = {
	{ SORT_FILES_BY_NAME_ASC,  "\xaa Name" },
    { SORT_FILES_BY_NAME_DESC, "\xab Name" },
    { SORT_FILES_BY_SIZE_ASC,  "\xac Size" },
    { SORT_FILES_BY_SIZE_DESC, "\xad Size" },
    { SORT_FILES_BY_TIME_ASC,  "\xae Time" },
    { SORT_FILES_BY_TIME_DESC, "\xaf Time" },
    { 0, 0 }
};

EnumItem g_enumDefinition_QUEUE_FILTER[] = {
	{ event_queue::EVENT_TYPE_DEBUG, "Debug" },
    { event_queue::EVENT_TYPE_INFO, "Info" },
    { event_queue::EVENT_TYPE_WARNING, "Warning" },
    { event_queue::EVENT_TYPE_ERROR, "Error" },
    { 0, 0 }
};

EnumItem g_enumDefinition_MODULE_TYPE[] = {
    { MODULE_TYPE_NONE, "None" },
    { MODULE_TYPE_DCP405, "DCP405" },
    { MODULE_TYPE_DCP405B, "DCP405B" },
    { MODULE_TYPE_DCM220, "DCM220" },
    { MODULE_TYPE_DCM224, "DCM224" },
    { 0, 0 }
};

////////////////////////////////////////////////////////////////////////////////

Value MakeValue(float value, Unit unit) {
    return Value(value, unit);
}

Value MakeStepValuesValue(const StepValues *stepValues) {
    Value value;
    value.type_ = VALUE_TYPE_STEP_VALUES;
    value.options_ = 0;
    value.unit_ = UNIT_UNKNOWN;
    value.pVoid_ = (void *)stepValues;
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

Value MakeFirmwareVersionValue(uint8_t majorVersion, uint8_t minorVersion) {
    Value value;
    value.type_ = VALUE_TYPE_FIRMWARE_VERSION;
    value.uint16_ = (majorVersion << 8) | minorVersion;
    return value;
}

Value MakeScpiErrorValue(int16_t errorCode) {
    Value value;
    value.pairOfInt16_.first = errorCode;
    value.pairOfInt16_.second = g_errorChannelIndex;
    g_errorChannelIndex = -1;
    value.type_ = VALUE_TYPE_SCPI_ERROR;
    return value;
}

Value MakeEventMessageValue(int16_t eventId) {
    Value value;
    value.int16_ = eventId;
    value.type_ = VALUE_TYPE_EVENT_MESSAGE;
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

void printTime(uint32_t time, char *text, int count) {
    printTime((double)time, text, count);
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
    snprintf(text, count - 1, "Ch%d:", value.getUInt8() + 1);
    text[count - 1] = 0;
}

bool compare_CHANNEL_SHORT_LABEL_WITHOUT_COLUMN_value(const Value &a, const Value &b) {
    return a.getUInt8() == b.getUInt8();
}

void CHANNEL_SHORT_LABEL_WITHOUT_COLUMN_value_to_text(const Value &value, char *text, int count) {
    snprintf(text, count - 1, "Ch. %d", value.getUInt8() + 1);
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
    return compareEventValues(a, b);
}

void EVENT_value_to_text(const Value &value, char *text, int count) {
    eventValueToText(value, text, count);
}

bool compare_EVENT_MESSAGE_value(const Value &a, const Value &b) {
    return a.getInt16() == b.getInt16();
}

void EVENT_MESSAGE_value_to_text(const Value &value, char *text, int count) {
    strncpy(text, event_queue::getEventMessage(value.getInt16()), count - 1);
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

bool compare_DATE_DMY_value(const Value &a, const Value &b) {
    return a.getUInt32() == b.getUInt32();
}

void DATE_DMY_value_to_text(const Value &value, char *text, int count) {
    int year, month, day, hour, minute, second;
    datetime::breakTime(value.getUInt32(), year, month, day, hour, minute, second);
    snprintf(text, count - 1, "%02d - %02d - %d", day, month, year);
    text[count - 1] = 0;
}

bool compare_DATE_MDY_value(const Value &a, const Value &b) {
    return a.getUInt32() == b.getUInt32();
}

void DATE_MDY_value_to_text(const Value &value, char *text, int count) {
    int year, month, day, hour, minute, second;
    datetime::breakTime(value.getUInt32(), year, month, day, hour, minute, second);
    snprintf(text, count - 1, "%02d - %02d - %d", month, day, year);
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

bool compare_TIME12_value(const Value &a, const Value &b) {
    return a.getUInt32() == b.getUInt32();
}

void TIME12_value_to_text(const Value &value, char *text, int count) {
    int year, month, day, hour, minute, second;
    datetime::breakTime(value.getUInt32(), year, month, day, hour, minute, second);
    bool am;
    datetime::convertTime24to12(hour, am);
    snprintf(text, count - 1, "%02d : %02d : %02d %s", hour, minute, second, am ? "AM" : "PM");
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
    strncpy(text, getTextMessage(), count - 1);
    text[count - 1] = 0;
}

bool compare_STEP_VALUES_value(const Value &a, const Value &b) {
    const StepValues *aStepValues = a.getStepValues();
    const StepValues *bStepValues = b.getStepValues();

    if (aStepValues->unit != bStepValues->unit) {
        return false;
    }

    if (aStepValues->count != bStepValues->count) {
        return false;
    }

    for (int i = 0; i < aStepValues->count; i++) {
        if (aStepValues->values[i] != bStepValues->values[i]) {
            return false;
        }
    }

    return true;
}

void STEP_VALUES_value_to_text(const Value &value, char *text, int count) {
}

bool compare_FLOAT_LIST_value(const Value &a, const Value &b) {
    return a.getFloatList() == b.getFloatList();
}

void FLOAT_LIST_value_to_text(const Value &value, char *text, int count) {
}

bool compare_CHANNEL_TITLE_value(const Value &a, const Value &b) {
    Channel &aChannel = Channel::get(a.getInt());
    Channel &bChannel = Channel::get(b.getInt());
    return aChannel.channelIndex == bChannel.channelIndex && a.getOptions() == b.getOptions();
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
    return aChannel.channelIndex == bChannel.channelIndex && a.getOptions() == b.getOptions();
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
    dlog_view::getLabel(dlog_view::getRecording(), value.getInt(), text, count);
}

bool compare_FIRMWARE_VERSION_value(const Value &a, const Value &b) {
    return a.getUInt16() == b.getUInt16();
}

void FIRMWARE_VERSION_value_to_text(const Value &value, char *text, int count) {
    uint8_t majorVersion = value.getUInt16() >> 8;
    uint8_t minorVersion = value.getUInt16() & 0xFF;
    snprintf(text, count - 1, "%d.%d", (int)majorVersion, (int)minorVersion);
    text[count - 1] = 0;
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
        if (persist_conf::devConf.dateTimeFormat == datetime::FORMAT_DMY_24 || persist_conf::devConf.dateTimeFormat == datetime::FORMAT_MDY_24) {
            snprintf(text, count - 1, "%02d:%02d:%02d", hour, minute, second);
        } else {
            bool am;
            datetime::convertTime24to12(hour, am);
            snprintf(text, count - 1, "%02d:%02d:%02d %s", hour, minute, second, am ? "AM" : "PM");
        }
    } else {
        if (persist_conf::devConf.dateTimeFormat == datetime::FORMAT_DMY_24 || persist_conf::devConf.dateTimeFormat == datetime::FORMAT_DMY_12) {
            snprintf(text, count - 1, "%02d-%02d-%02d", day, month, year % 100);
        } else {
            snprintf(text, count - 1, "%02d-%02d-%02d", month, day, year % 100);
        }
    }

    text[count - 1] = 0;
}

bool compare_SLOT_INFO_value(const Value &a, const Value &b) {
    return a.getInt() == b.getInt();
}

void SLOT_INFO_value_to_text(const Value &value, char *text, int count) {
    int slotIndex = value.getInt();
    auto &slot = g_slots[slotIndex];
    psu::Channel &channel = psu::Channel::get(slot.channelIndex);
    if (channel.isInstalled()) {
        snprintf(text, count - 1, "%s R%dB%d", slot.moduleInfo->moduleName, (int)(slot.moduleRevision >> 8), (int)(slot.moduleRevision & 0xFF));
    } else {
        strncpy(text, "Not installed", count - 1);
    }
    text[count - 1] = 0;
}

bool compare_SLOT_INFO2_value(const Value &a, const Value &b) {
    return a.getInt() == b.getInt();
}

void SLOT_INFO2_value_to_text(const Value &value, char *text, int count) {
    int slotIndex = value.getInt();
    auto &slot = g_slots[slotIndex];
    psu::Channel &channel = psu::Channel::get(slot.channelIndex);
    if (channel.isInstalled()) {
        snprintf(text, count - 1, "%s_R%dB%d", slot.moduleInfo->moduleName, (int)(slot.moduleRevision >> 8), (int)(slot.moduleRevision & 0xFF));
    } else {
        strncpy(text, "None", count - 1);
    }
    text[count - 1] = 0;
}

bool compare_MASTER_INFO_value(const Value &a, const Value &b) {
    return true;
}

void MASTER_INFO_value_to_text(const Value &value, char *text, int count) {
    snprintf(text, count - 1, "%s %s", MCU_NAME, MCU_REVISION);
    text[count - 1] = 0;
}

bool compare_TEST_RESULT_value(const Value &a, const Value &b) {
    return a.getInt() == b.getInt();
}

void TEST_RESULT_value_to_text(const Value &value, char *text, int count) {
    TestResult testResult = (TestResult)value.getInt();
    if (testResult == TEST_FAILED) {
        strncpy(text, "Failed", count - 1);
    } else if (testResult == TEST_OK) {
        strncpy(text, "OK", count - 1);
    } else if (testResult == TEST_CONNECTING) {
        strncpy(text, "Connecting", count - 1);
    } else if (testResult == TEST_SKIPPED) {
        strncpy(text, "Skipped", count - 1);
    } else if (testResult == TEST_WARNING) {
        strncpy(text, "Warning", count - 1);
    } else {
        strncpy(text, "", count - 1);
    }
    text[count - 1] = 0;
}

bool compare_SCPI_ERROR_value(const Value &a, const Value &b) {
    return a.getUInt32() == b.getUInt32();
}

void SCPI_ERROR_value_to_text(const Value &value, char *text, int count) {
    if (value.getSecondInt16() != -1) {
        sprintf(text, "Ch%d: ", value.getSecondInt16() + 1);
    }
    strncpy(text + strlen(text), SCPI_ErrorTranslate(value.getFirstInt16()), count - 1 - strlen(text));
    text[count - 1] = 0;
}

bool compare_STORAGE_INFO_value(const Value &a, const Value &b) {
    return a.getInt() == b.getInt();
}

void STORAGE_INFO_value_to_text(const Value &value, char *text, int count) {
    uint64_t usedSpace;
    uint64_t freeSpace;
    if (sd_card::getInfo(usedSpace, freeSpace, true)) { // "true" means get storage info from cache
        auto totalSpace = usedSpace + freeSpace;

        formatBytes(freeSpace, text, count -1);
        auto n = strlen(text);
        auto countLeft = count - n - 1;
        if (countLeft > 0) {
            auto percent = (int)floor(100.0 * freeSpace / totalSpace);
            snprintf(text + n, countLeft, " (%d%%) free of ", percent);
            text[count - 1] = 0;
            n = strlen(text);
            countLeft = count - n - 1;
            if (countLeft > 0) {
                formatBytes(totalSpace, text + n, countLeft);
            }
        }
    } else {
        strncpy(text, "Calculating storage info...", count - 1);
        text[count - 1] = 0;
    }
}

bool compare_FOLDER_INFO_value(const Value &a, const Value &b) {
    return a.getUInt32() == b.getUInt32();
}

void FOLDER_INFO_value_to_text(const Value &value, char *text, int count) {
    if (value.getUInt32() == 1) {
        strncpy(text, "1 item", count - 1);
    } else {
        snprintf(text, count - 1, "%lu items ", value.getUInt32());
    }
    text[count - 1] = 0;
}

bool compare_CHANNEL_INFO_SERIAL_value(const Value &a, const Value &b) {
    return a.getInt() == b.getInt();
}

void CHANNEL_INFO_SERIAL_value_to_text(const Value &value, char *text, int count) {
    auto &channel = Channel::get(value.getInt());
    channel.getSerial(text);
}

bool compare_DEBUG_VARIABLE_value(const Value &a, const Value &b) {
    return false;
}

void DEBUG_VARIABLE_value_to_text(const Value &value, char *text, int count) {
    psu::debug::getVariableValue(value.getInt(), text);
}

static Cursor g_editValueCursor(-1);
static int16_t g_editValueDataId;

void onSetFloatValue(float value) {
    popPage();
    set(g_editValueCursor, g_editValueDataId, MakeValue(value, getUnit(g_editValueCursor, g_editValueDataId)));
}

void onSetInfinityValue() {
    popPage();
    set(g_editValueCursor, g_editValueDataId, MakeValue(INFINITY, getUnit(g_editValueCursor, g_editValueDataId)));
}

void onSetUInt16Value(float value) {
    popPage();
    set(g_editValueCursor, g_editValueDataId, Value((uint16_t)value, VALUE_TYPE_UINT16));
}

void onSetStringValue(char *value) {
    const char *errMessage = isValidValue(g_editValueCursor, g_editValueDataId, value);
    if (!errMessage) {
        popPage();
        set(g_editValueCursor, g_editValueDataId, value);
    } else {
        errorMessage(errMessage);
    }
}

void editValue(int16_t dataId) {
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
                options.option1ButtonText = INFINITY_SYMBOL;
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
    } else if (value.getType() == VALUE_TYPE_UINT16) {
        NumericKeypadOptions options;

        options.min = getMin(g_editValueCursor, g_editValueDataId).getUInt16();
        options.max = getMax(g_editValueCursor, g_editValueDataId).getUInt16();
        options.def = getDef(g_editValueCursor, g_editValueDataId).getUInt16();

        options.enableDefButton();

        NumericKeypad::start(0, (int)value.getUInt16(), options, onSetUInt16Value, 0, 0);
    } else {
        Keypad::startPush(0, value.getString(), 0, getMax(g_editValueCursor, g_editValueDataId).getUInt32(), value.getType() == VALUE_TYPE_PASSWORD, onSetStringValue, 0);
    }
}

////////////////////////////////////////////////////////////////////////////////

void data_none(DataOperationEnum operation, Cursor cursor, Value &value) {
    value = Value();
}

void data_channels(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_COUNT) {
        value = CH_NUM;
    } else if (operation == DATA_OPERATION_SELECT) {
        value = Value(g_channel, VALUE_TYPE_POINTER);
        selectChannel(&Channel::get(cursor));
    } else if (operation == DATA_OPERATION_DESELECT) {
        selectChannel((Channel * )value.getVoidPointer());
    }
}

void data_channel_status(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        int iChannel = cursor >= 0 ? cursor : (g_channel ? g_channel->channelIndex : 0);
        Channel &channel = Channel::get(iChannel);
        int channelStatus = channel.isInstalled() ? (channel.isOk() ? 1 : 2) : 0;
        value = channelStatus;
    }
}

void data_channel_output_state(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        int iChannel = cursor >= 0 ? cursor : (g_channel ? g_channel->channelIndex : 0);
        Channel &channel = Channel::get(iChannel);
        value = channel.isOutputEnabled();
    }
}

void data_channel_is_cc(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        int iChannel = cursor >= 0 ? cursor : (g_channel ? g_channel->channelIndex : 0);
        Channel &channel = Channel::get(iChannel);
        value = channel.getMode() == CHANNEL_MODE_CC;
    }
}

void data_channel_is_cv(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        int iChannel = cursor >= 0 ? cursor : (g_channel ? g_channel->channelIndex : 0);
        Channel &channel = Channel::get(iChannel);
        value = channel.getMode() == CHANNEL_MODE_CV;
    }
}

void data_channel_u_set(DataOperationEnum operation, Cursor cursor, Value &value) {
    int iChannel = cursor >= 0 ? cursor : (g_channel ? g_channel->channelIndex : 0);
    Channel &channel = Channel::get(iChannel);
    if (operation == DATA_OPERATION_GET) {
        value = MakeValue(channel_dispatcher::getUSet(channel), UNIT_VOLT);
    } else {
        data_channel_u_edit(operation, cursor, value);
    }
}

void data_channel_u_mon(DataOperationEnum operation, Cursor cursor, Value &value) {
    int iChannel = cursor >= 0 ? cursor : (g_channel ? g_channel->channelIndex : 0);
    Channel &channel = Channel::get(iChannel);
    if (operation == DATA_OPERATION_GET) {
        value = MakeValue(channel_dispatcher::getUMon(channel), UNIT_VOLT);
    } else if (operation == DATA_OPERATION_GET_MIN) {
        value = MakeValue(0, UNIT_VOLT);
    } else if (operation == DATA_OPERATION_GET_MAX) {
        value = MakeValue(channel_dispatcher::getUMax(channel), UNIT_VOLT);
    } else if (operation == DATA_OPERATION_GET_LIMIT) {
        value = MakeValue(channel_dispatcher::getULimit(channel), UNIT_VOLT);
    } else if (operation == DATA_OPERATION_GET_COLOR) {
        if (io_pins::isInhibited()/* || channel.getMode() == CHANNEL_MODE_UR*/) {
            value = Value(COLOR_ID_STATUS_WARNING, VALUE_TYPE_UINT16);
        }
    } else if (operation == DATA_OPERATION_GET_BACKGROUND_COLOR) {
        if (!dlog_record::isIdle() && dlog_record::g_recording.parameters.logVoltage[iChannel]) {
            value = Value(COLOR_ID_DATA_LOGGING, VALUE_TYPE_UINT16);
        }
    } else if (operation == DATA_OPERATION_GET_ACTIVE_COLOR) {
        if (io_pins::isInhibited()) {
            value = Value(((const Style *)value.getVoidPointer())->background_color, VALUE_TYPE_UINT16);
        }
    } else if (operation == DATA_OPERATION_GET_ACTIVE_BACKGROUND_COLOR) {
        if (io_pins::isInhibited()) {
            value = Value(((const Style *)value.getVoidPointer())->background_color, VALUE_TYPE_UINT16);
        }
    } else if (operation == DATA_OPERATION_IS_BLINKING) {
        value = io_pins::isInhibited() ? 1 : 0;
    } else if (operation == DATA_OPERATION_GET_TEXT_REFRESH_RATE) {
        value = Value(channel.params.MON_REFRESH_RATE_MS, VALUE_TYPE_UINT32);
    }
}

void data_channel_u_mon_dac(DataOperationEnum operation, Cursor cursor, Value &value) {
    int iChannel = cursor >= 0 ? cursor : (g_channel ? g_channel->channelIndex : 0);
    Channel &channel = Channel::get(iChannel);
    if (operation == DATA_OPERATION_GET) {
        value = MakeValue(channel_dispatcher::getUMonDac(channel), UNIT_VOLT);
    } else if (operation == DATA_OPERATION_GET_BACKGROUND_COLOR) {
        if (!dlog_record::isIdle() && dlog_record::g_recording.parameters.logVoltage[iChannel]) {
            value = Value(COLOR_ID_DATA_LOGGING, VALUE_TYPE_UINT16);
        }
    }
}

void data_channel_u_limit(DataOperationEnum operation, Cursor cursor, Value &value) {
    int iChannel = cursor >= 0 ? cursor : (g_channel ? g_channel->channelIndex : 0);
    Channel &channel = Channel::get(iChannel);
    if (operation == DATA_OPERATION_GET) {
        value = MakeValue(channel_dispatcher::getULimit(channel), UNIT_VOLT);
    }
}

void data_channel_u_edit(DataOperationEnum operation, Cursor cursor, Value &value) {
    int iChannel = cursor >= 0 ? cursor : (g_channel ? g_channel->channelIndex : 0);
    Channel &channel = Channel::get(iChannel);
    if (operation == DATA_OPERATION_GET) {
        bool focused = g_focusCursor == cursor && g_focusDataId == DATA_ID_CHANNEL_U_EDIT;
        if (focused && g_focusEditValue.getType() != VALUE_TYPE_NONE) {
            value = g_focusEditValue;
        } else if (focused && getActivePageId() == PAGE_ID_EDIT_MODE_KEYPAD && edit_mode_keypad::g_keypad->isEditing()) {
            data_keypad_text(operation, cursor, value);
        } else {
            value = MakeValue(channel_dispatcher::getUSet(channel), UNIT_VOLT);
        }
    } else if (operation == DATA_OPERATION_GET_EDIT_VALUE) {
        value = MakeValue(channel_dispatcher::getUSetUnbalanced(channel), UNIT_VOLT);
    } else if (operation == DATA_OPERATION_GET_MIN) {
        value = MakeValue(channel_dispatcher::getUMin(channel), UNIT_VOLT);
    } else if (operation == DATA_OPERATION_GET_MAX) {
        value = MakeValue(channel_dispatcher::getUMax(channel), UNIT_VOLT);
    } else if (operation == DATA_OPERATION_GET_LIMIT) {
        value = MakeValue(channel_dispatcher::getULimit(channel), UNIT_VOLT);
    } else if (operation == DATA_OPERATION_GET_NAME) {
        value = "Voltage";
    } else if (operation == DATA_OPERATION_GET_UNIT) {
        value = UNIT_VOLT;
    } else if (operation == DATA_OPERATION_GET_IS_CHANNEL_DATA) {
        value = 1;
    } else if (operation == DATA_OPERATION_GET_ENCODER_STEP_VALUES) {
        channel.getVoltageStepValues(value.getStepValues());
        value = 1;
    } else if (operation == DATA_OPERATION_SET) {
        if (!between(value.getFloat(), channel_dispatcher::getUMin(channel), channel_dispatcher::getUMax(channel))) {
            value = MakeScpiErrorValue(SCPI_ERROR_DATA_OUT_OF_RANGE);
        } else if (value.getFloat() > channel_dispatcher::getULimit(channel)) {
            g_errorChannelIndex = channel.channelIndex;
            value = MakeScpiErrorValue(SCPI_ERROR_VOLTAGE_LIMIT_EXCEEDED);
        } else if (channel.isPowerLimitExceeded(value.getFloat(), channel_dispatcher::getISetUnbalanced(channel))) {
            g_errorChannelIndex = channel.channelIndex;
            value = MakeScpiErrorValue(SCPI_ERROR_POWER_LIMIT_EXCEEDED);
        } else {
            channel_dispatcher::setVoltage(channel, value.getFloat());
        }
    }
}

void data_channel_i_set(DataOperationEnum operation, Cursor cursor, Value &value) {
    int iChannel = cursor >= 0 ? cursor : (g_channel ? g_channel->channelIndex : 0);
    Channel &channel = Channel::get(iChannel);
    if (operation == DATA_OPERATION_GET) {
        value = MakeValue(channel_dispatcher::getISet(channel), UNIT_AMPER);
    } else {
        data_channel_i_edit(operation, cursor, value);
    }
}

void data_channel_i_mon(DataOperationEnum operation, Cursor cursor, Value &value) {
    int iChannel = cursor >= 0 ? cursor : (g_channel ? g_channel->channelIndex : 0);
    Channel &channel = Channel::get(iChannel);
    if (operation == DATA_OPERATION_GET) {
        value = MakeValue(channel_dispatcher::getIMon(channel), UNIT_AMPER);
    } else if (operation == DATA_OPERATION_GET_MIN) {
        value = MakeValue(0, UNIT_AMPER);
    } else if (operation == DATA_OPERATION_GET_MAX) {
        value = MakeValue(channel_dispatcher::getIMax(channel), UNIT_AMPER);
    } else if (operation == DATA_OPERATION_GET_LIMIT) {
        value = MakeValue(channel_dispatcher::getILimit(channel), UNIT_AMPER);
    } else if (operation == DATA_OPERATION_GET_COLOR) {
        if (io_pins::isInhibited()/* || channel.getMode() == CHANNEL_MODE_UR*/) {
            value = Value(COLOR_ID_STATUS_WARNING, VALUE_TYPE_UINT16);
        }
    } else if (operation == DATA_OPERATION_GET_BACKGROUND_COLOR) {
        if (!dlog_record::isIdle() && dlog_record::g_recording.parameters.logCurrent[iChannel]) {
            value = Value(COLOR_ID_DATA_LOGGING, VALUE_TYPE_UINT16);
        }
    } else if (operation == DATA_OPERATION_GET_ACTIVE_COLOR) {
        if (io_pins::isInhibited()) {
            value = Value(((const Style *)value.getVoidPointer())->background_color, VALUE_TYPE_UINT16);
        }
    } else if (operation == DATA_OPERATION_GET_ACTIVE_BACKGROUND_COLOR) {
        if (io_pins::isInhibited()) {
            value = Value(((const Style *)value.getVoidPointer())->background_color, VALUE_TYPE_UINT16);
        }
    } else if (operation == DATA_OPERATION_IS_BLINKING) {
        value = io_pins::isInhibited() ? 1 : 0;
    } else if (operation == DATA_OPERATION_GET_TEXT_REFRESH_RATE) {
        value = Value(channel.params.MON_REFRESH_RATE_MS, VALUE_TYPE_UINT32);
    }
}

void data_channel_i_mon_dac(DataOperationEnum operation, Cursor cursor, Value &value) {
    int iChannel = cursor >= 0 ? cursor : (g_channel ? g_channel->channelIndex : 0);
    Channel &channel = Channel::get(iChannel);
    if (operation == DATA_OPERATION_GET) {
        value = MakeValue(channel_dispatcher::getIMonDac(channel), UNIT_AMPER);
    } else if (operation == DATA_OPERATION_GET_BACKGROUND_COLOR) {
        if (!dlog_record::isIdle() && dlog_record::g_recording.parameters.logCurrent[iChannel]) {
            value = Value(COLOR_ID_DATA_LOGGING, VALUE_TYPE_UINT16);
        }
    } 
}

void data_channel_i_limit(DataOperationEnum operation, Cursor cursor, Value &value) {
    int iChannel = cursor >= 0 ? cursor : (g_channel ? g_channel->channelIndex : 0);
    Channel &channel = Channel::get(iChannel);
    if (operation == DATA_OPERATION_GET) {
        value = MakeValue(channel_dispatcher::getILimit(channel), UNIT_AMPER);
    }
}

void data_channel_i_edit(DataOperationEnum operation, Cursor cursor, Value &value) {
    int iChannel = cursor >= 0 ? cursor : (g_channel ? g_channel->channelIndex : 0);
    Channel &channel = Channel::get(iChannel);
    if (operation == DATA_OPERATION_GET) {
        bool focused = g_focusCursor == cursor && g_focusDataId == DATA_ID_CHANNEL_I_EDIT;
        if (focused && g_focusEditValue.getType() != VALUE_TYPE_NONE) {
            value = g_focusEditValue;
        } else if (focused && getActivePageId() == PAGE_ID_EDIT_MODE_KEYPAD && edit_mode_keypad::g_keypad->isEditing()) {
            data_keypad_text(operation, cursor, value);
        } else {
            value = MakeValue(channel_dispatcher::getISet(channel), UNIT_AMPER);
        }
    } else if (operation == DATA_OPERATION_GET_EDIT_VALUE) {
        value = MakeValue(channel_dispatcher::getISetUnbalanced(channel), UNIT_AMPER);
    } else if (operation == DATA_OPERATION_GET_MIN) {
        value = MakeValue(channel_dispatcher::getIMin(channel), UNIT_AMPER);
    } else if (operation == DATA_OPERATION_GET_MAX) {
        value = MakeValue(channel_dispatcher::getIMax(channel), UNIT_AMPER);
    } else if (operation == DATA_OPERATION_GET_LIMIT) {
        value = MakeValue(channel_dispatcher::getILimit(channel), UNIT_AMPER);
    } else if (operation == DATA_OPERATION_GET_NAME) {
        value = "Current";
    } else if (operation == DATA_OPERATION_GET_UNIT) {
        value = UNIT_AMPER;
    } else if (operation == DATA_OPERATION_GET_IS_CHANNEL_DATA) {
        value = 1;
    } else if (operation == DATA_OPERATION_GET_ENCODER_STEP_VALUES) {
        channel.getCurrentStepValues(value.getStepValues());
        value = 1;
    } else if (operation == DATA_OPERATION_SET) {
        if (!between(value.getFloat(), channel_dispatcher::getIMin(channel), channel_dispatcher::getIMax(channel))) {
            value = MakeScpiErrorValue(SCPI_ERROR_DATA_OUT_OF_RANGE);
        } else if (value.getFloat() > channel_dispatcher::getILimit(channel)) {
            g_errorChannelIndex = channel.channelIndex;
            value = MakeScpiErrorValue(SCPI_ERROR_CURRENT_LIMIT_EXCEEDED);
        } else if (channel.isPowerLimitExceeded(channel_dispatcher::getUSetUnbalanced(channel), value.getFloat())) {
            g_errorChannelIndex = channel.channelIndex;
            value = MakeScpiErrorValue(SCPI_ERROR_POWER_LIMIT_EXCEEDED);
        } else {
            channel_dispatcher::setCurrent(channel, value.getFloat());
        }
    }
}

void data_channel_p_mon(DataOperationEnum operation, Cursor cursor, Value &value) {
    int iChannel = cursor >= 0 ? cursor : (g_channel ? g_channel->channelIndex : 0);
    Channel &channel = Channel::get(iChannel);
    if (operation == DATA_OPERATION_GET) {
        value = MakeValue(channel_dispatcher::getUMon(channel) * channel_dispatcher::getIMon(channel), UNIT_WATT);
    } else if (operation == DATA_OPERATION_GET_MIN) {
        value = MakeValue(0, UNIT_WATT);
    } else if (operation == DATA_OPERATION_GET_MAX) {
        value = MakeValue(channel_dispatcher::getPowerMaxLimit(channel), UNIT_WATT);
    } else if (operation == DATA_OPERATION_GET_LIMIT) {
        value = MakeValue(channel_dispatcher::getPowerLimit(channel), UNIT_WATT);
    } else if (operation == DATA_OPERATION_GET_UNIT) {
        value = UNIT_WATT;
    } else if (operation == DATA_OPERATION_GET_COLOR) {
        if (io_pins::isInhibited()/* || channel.getMode() == CHANNEL_MODE_UR*/) {
            value = Value(COLOR_ID_STATUS_WARNING, VALUE_TYPE_UINT16);
        }
    } else if (operation == DATA_OPERATION_GET_BACKGROUND_COLOR) {
        if (!dlog_record::isIdle() && dlog_record::g_recording.parameters.logPower[iChannel]) {
            value = Value(COLOR_ID_DATA_LOGGING, VALUE_TYPE_UINT16);
        }
    } else if (operation == DATA_OPERATION_GET_ACTIVE_COLOR) {
        if (io_pins::isInhibited()) {
            value = Value(((const Style *)value.getVoidPointer())->background_color, VALUE_TYPE_UINT16);
        }
    } else if (operation == DATA_OPERATION_GET_ACTIVE_BACKGROUND_COLOR) {
        if (io_pins::isInhibited()) {
            value = Value(((const Style *)value.getVoidPointer())->background_color, VALUE_TYPE_UINT16);
        }
    } else if (operation == DATA_OPERATION_IS_BLINKING) {
        value = io_pins::isInhibited() ? 1 : 0;
    } else if (operation == DATA_OPERATION_GET_TEXT_REFRESH_RATE) {
        value = Value(channel.params.MON_REFRESH_RATE_MS, VALUE_TYPE_UINT32);
    }
}

void data_channel_other_value_mon(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (g_focusDataId == DATA_ID_CHANNEL_U_EDIT) {
        data_channel_i_mon(operation, cursor, value);
    } else if (g_focusDataId == DATA_ID_CHANNEL_I_EDIT) {
        data_channel_u_mon(operation, cursor, value);
    }
}

void data_channels_is_max_view(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = (int)persist_conf::isMaxChannelView();
    }
}

void data_channels_view_mode(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = (int)persist_conf::devConf.channelsViewMode;
    }
}

void data_channels_view_mode_in_default(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = (int)persist_conf::devConf.channelsViewMode;
    }
}

void data_channels_view_mode_in_max(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
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

void data_slot1_channel_index(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_SET_CONTEXT) {
        Channel &channel = Channel::getBySlotIndex(0);
        value = Value(g_channel, VALUE_TYPE_POINTER);
        selectChannel(&channel);
    } else if (operation == DATA_OPERATION_GET_CONTEXT) {
        value = Value(g_channel, VALUE_TYPE_POINTER);
    } else if (operation == DATA_OPERATION_GET_CONTEXT_CURSOR) {
        Channel &channel = Channel::getBySlotIndex(0);
        value = channel.channelIndex;
    } else if (operation == DATA_OPERATION_RESTORE_CONTEXT) {
        selectChannel((Channel *)value.getVoidPointer());
    }
}

void data_slot2_channel_index(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_SET_CONTEXT) {
        Channel &channel = Channel::getBySlotIndex(1);
        value = Value(g_channel, VALUE_TYPE_POINTER);
        selectChannel(&channel);
    } else if (operation == DATA_OPERATION_GET_CONTEXT) {
        value = Value(g_channel, VALUE_TYPE_POINTER);
    } else if (operation == DATA_OPERATION_GET_CONTEXT_CURSOR) {
        Channel &channel = Channel::getBySlotIndex(1);
        value = channel.channelIndex;
    } else if (operation == DATA_OPERATION_RESTORE_CONTEXT) {
        selectChannel((Channel *)value.getVoidPointer());
    }
}

void data_slot3_channel_index(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_SET_CONTEXT) {
        Channel &channel = Channel::getBySlotIndex(2);
        value = Value(g_channel, VALUE_TYPE_POINTER);
        selectChannel(&channel);
    } else if (operation == DATA_OPERATION_GET_CONTEXT) {
        value = Value(g_channel, VALUE_TYPE_POINTER);
    } else if (operation == DATA_OPERATION_GET_CONTEXT_CURSOR) {
        Channel &channel = Channel::getBySlotIndex(2);
        value = channel.channelIndex;
    } else if (operation == DATA_OPERATION_RESTORE_CONTEXT) {
        selectChannel((Channel *)value.getVoidPointer());
    }
}

void data_slot_default1_view(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = getDefaultView(cursor);
    }
}

void data_slot_default2_view(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = getDefaultView(cursor);
    }
}

void data_slot_default3_view(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = getDefaultView(cursor);
    }
}

void data_slot_max_channel_index(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_SET_CONTEXT) {
        Channel &channel = Channel::get(persist_conf::getMaxChannelIndex());
        value = Value(g_channel, VALUE_TYPE_POINTER);
        selectChannel(&channel);
    } else if (operation == DATA_OPERATION_GET_CONTEXT) {
        value = Value(g_channel, VALUE_TYPE_POINTER);
    } else if (operation == DATA_OPERATION_GET_CONTEXT_CURSOR) {
        Channel &channel = Channel::get(persist_conf::getMaxChannelIndex());
        value = channel.channelIndex;
    } else if (operation == DATA_OPERATION_RESTORE_CONTEXT) {
        selectChannel((Channel *)value.getVoidPointer());
    }
}

void data_slot_max_view(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        Channel &channel = Channel::get(cursor);
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

void data_slot_min1_channel_index(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_SET_CONTEXT) {
        Channel &channel = Channel::get(persist_conf::getMin1ChannelIndex());
        cursor = channel.channelIndex;
        value = Value(g_channel, VALUE_TYPE_POINTER);
        selectChannel(&channel);
    } else if (operation == DATA_OPERATION_GET_CONTEXT) {
        value = Value(g_channel, VALUE_TYPE_POINTER);
    } else if (operation == DATA_OPERATION_GET_CONTEXT_CURSOR) {
        Channel &channel = Channel::get(persist_conf::getMin1ChannelIndex());
        value = channel.channelIndex;
    } else if (operation == DATA_OPERATION_RESTORE_CONTEXT) {
        selectChannel((Channel *)value.getVoidPointer());
    }
}

void data_slot_min1_view(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = getMinView(cursor);
    }
}

void data_slot_min2_channel_index(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_SET_CONTEXT) {
        Channel &channel = Channel::get(persist_conf::getMin2ChannelIndex());
        cursor = channel.channelIndex;
        value = Value(g_channel, VALUE_TYPE_POINTER);
        selectChannel(&channel);
    } else if (operation == DATA_OPERATION_GET_CONTEXT) {
        value = Value(g_channel, VALUE_TYPE_POINTER);
    } else if (operation == DATA_OPERATION_GET_CONTEXT_CURSOR) {
        Channel &channel = Channel::get(persist_conf::getMin2ChannelIndex());
        value = channel.channelIndex;
    } else if (operation == DATA_OPERATION_RESTORE_CONTEXT) {
        selectChannel((Channel *)value.getVoidPointer());
    }
}

void data_slot_min2_view(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = getMinView(cursor);
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

void data_slot_micro1_view(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = getMicroView(cursor);
    }
}

void data_slot_micro2_view(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = getMicroView(cursor);
    }
}

void data_slot_micro3_view(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = getMicroView(cursor);
    }
}

void data_slot_2ch_ch1_index(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_SET_CONTEXT) {
        Channel &channel = Channel::get(cursor);
        value = Value(g_channel, VALUE_TYPE_POINTER);
        selectChannel(&channel);
    } else if (operation == DATA_OPERATION_GET_CONTEXT) {
        value = Value(g_channel, VALUE_TYPE_POINTER);
    } else if (operation == DATA_OPERATION_GET_CONTEXT_CURSOR) {
        value = cursor;
    } else if (operation == DATA_OPERATION_RESTORE_CONTEXT) {
        selectChannel((Channel *)value.getVoidPointer());
    }
}

void data_slot_2ch_ch2_index(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_SET_CONTEXT) {
        if (cursor == persist_conf::getMaxChannelIndex() && Channel::get(cursor).subchannelIndex == 1) {
            cursor = cursor - 1;
        } else {
            cursor = cursor + 1;
        }
        Channel &channel = Channel::get(cursor);
        value = Value(g_channel, VALUE_TYPE_POINTER);
        selectChannel(&channel);
    } else if (operation == DATA_OPERATION_GET_CONTEXT) {
        value = Value(g_channel, VALUE_TYPE_POINTER);
    } else if (operation == DATA_OPERATION_GET_CONTEXT_CURSOR) {
        if (cursor == persist_conf::getMaxChannelIndex() && Channel::get(cursor).subchannelIndex == 1) {
            cursor = cursor - 1;
        } else {
            cursor = cursor + 1;
        }
        value = cursor;
    } else if (operation == DATA_OPERATION_RESTORE_CONTEXT) {
        selectChannel((Channel *)value.getVoidPointer());
    }
}

void data_slot_def_2ch_view(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        Channel &channel = Channel::get(cursor);
        int isVert = persist_conf::devConf.channelsViewMode == CHANNELS_VIEW_MODE_NUMERIC || persist_conf::devConf.channelsViewMode == CHANNELS_VIEW_MODE_VERT_BAR;
        value = channel.isOutputEnabled() ? 
            (isVert ? PAGE_ID_SLOT_DEF_2CH_VERT_ON : PAGE_ID_SLOT_DEF_2CH_HORZ_ON) :
            (isVert ? PAGE_ID_SLOT_DEF_2CH_VERT_OFF : PAGE_ID_SLOT_DEF_2CH_HORZ_OFF);
    }
}

void data_slot_max_2ch_view(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        Channel &channel = Channel::get(cursor);

        if (persist_conf::devConf.channelsViewModeInMax == CHANNELS_VIEW_MODE_IN_MAX_NUMERIC) {
            value = channel.isOutputEnabled() ? PAGE_ID_SLOT_MAX_2CH_NUM_ON : PAGE_ID_SLOT_MAX_2CH_NUM_OFF;
        } else if (persist_conf::devConf.channelsViewModeInMax == CHANNELS_VIEW_MODE_IN_MAX_HORZ_BAR) {
            value = channel.isOutputEnabled() ? PAGE_ID_SLOT_MAX_2CH_HBAR_ON : PAGE_ID_SLOT_MAX_2CH_HBAR_OFF;
        } else {
            value = channel.isOutputEnabled() ? PAGE_ID_SLOT_MAX_2CH_YT_ON : PAGE_ID_SLOT_MAX_2CH_YT_OFF;
        }
    }
}

void data_slot_max_2ch_min_view(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        Channel &channel = Channel::get(cursor);
        value = channel.isOutputEnabled() ? PAGE_ID_SLOT_MAX_2CH_MIN_ON : PAGE_ID_SLOT_MAX_2CH_MIN_OFF;
    }
}


void data_slot_min_2ch_view(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        Channel &channel = Channel::get(cursor);
        value = channel.isOutputEnabled() ? PAGE_ID_SLOT_MIN_2CH_ON : PAGE_ID_SLOT_MIN_2CH_OFF;
    }
}

void data_slot_micro_2ch_view(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        Channel &channel = Channel::get(cursor);
        value = channel.isOutputEnabled() ? PAGE_ID_SLOT_MICRO_2CH_ON : PAGE_ID_SLOT_MICRO_2CH_OFF;
    }
}

void data_channel_display_value1(DataOperationEnum operation, Cursor cursor, Value &value) {
    int iChannel = cursor >= 0 ? cursor : (g_channel ? g_channel->channelIndex : 0);
    Channel &channel = Channel::get(iChannel);
    if (channel.flags.displayValue1 == DISPLAY_VALUE_VOLTAGE) {
        data_channel_u_mon(operation, cursor, value);
    } else if (channel.flags.displayValue1 == DISPLAY_VALUE_CURRENT) {
        data_channel_i_mon(operation, cursor, value);
    } else if (channel.flags.displayValue1 == DISPLAY_VALUE_POWER) {
        data_channel_p_mon(operation, cursor, value);
    }
}

void data_channel_display_value1_set(DataOperationEnum operation, Cursor cursor, Value &value) {
    int iChannel = cursor >= 0 ? cursor : (g_channel ? g_channel->channelIndex : 0);
    Channel &channel = Channel::get(iChannel);
    if (channel.flags.displayValue1 == DISPLAY_VALUE_VOLTAGE) {
        data_channel_u_set(operation, cursor, value);
    } else if (channel.flags.displayValue1 == DISPLAY_VALUE_CURRENT) {
        data_channel_i_set(operation, cursor, value);
    } else if (channel.flags.displayValue1 == DISPLAY_VALUE_POWER) {
        if (operation == DATA_OPERATION_GET) {
            value = MakeValue(channel_dispatcher::getUSet(channel) * channel_dispatcher::getISet(channel), UNIT_WATT);
        }
    }
}

void data_channel_display_value1_limit(DataOperationEnum operation, Cursor cursor, Value &value) {
    int iChannel = cursor >= 0 ? cursor : (g_channel ? g_channel->channelIndex : 0);
    Channel &channel = Channel::get(iChannel);
    if (channel.flags.displayValue1 == DISPLAY_VALUE_VOLTAGE) {
        data_channel_u_limit(operation, cursor, value);
    } else if (channel.flags.displayValue1 == DISPLAY_VALUE_CURRENT) {
        data_channel_i_limit(operation, cursor, value);
    } else if (channel.flags.displayValue1 == DISPLAY_VALUE_POWER) {
        if (operation == DATA_OPERATION_GET) {
            value = MakeValue(channel_dispatcher::getPowerLimit(channel), UNIT_WATT);
        }
    }
}

void data_channel_display_value2(DataOperationEnum operation, Cursor cursor, Value &value) {
    int iChannel = cursor >= 0 ? cursor : (g_channel ? g_channel->channelIndex : 0);
    Channel &channel = Channel::get(iChannel);
    if (channel.flags.displayValue2 == DISPLAY_VALUE_VOLTAGE) {
        data_channel_u_mon(operation, cursor, value);
    } else if (channel.flags.displayValue2 == DISPLAY_VALUE_CURRENT) {
        data_channel_i_mon(operation, cursor, value);
    } else if (channel.flags.displayValue2 == DISPLAY_VALUE_POWER) {
        data_channel_p_mon(operation, cursor, value);
    }
}

void data_channel_display_value2_set(DataOperationEnum operation, Cursor cursor, Value &value) {
    int iChannel = cursor >= 0 ? cursor : (g_channel ? g_channel->channelIndex : 0);
    Channel &channel = Channel::get(iChannel);
    if (channel.flags.displayValue2 == DISPLAY_VALUE_VOLTAGE) {
        data_channel_u_set(operation, cursor, value);
    } else if (channel.flags.displayValue2 == DISPLAY_VALUE_CURRENT) {
        data_channel_i_set(operation, cursor, value);
    } else if (channel.flags.displayValue2 == DISPLAY_VALUE_POWER) {
        if (operation == DATA_OPERATION_GET) {
            value = MakeValue(channel_dispatcher::getUSet(channel) * channel_dispatcher::getISet(channel), UNIT_WATT);
        }
    }
}

void data_channel_display_value2_limit(DataOperationEnum operation, Cursor cursor, Value &value) {
    int iChannel = cursor >= 0 ? cursor : (g_channel ? g_channel->channelIndex : 0);
    Channel &channel = Channel::get(iChannel);
    if (channel.flags.displayValue2 == DISPLAY_VALUE_VOLTAGE) {
        data_channel_u_limit(operation, cursor, value);
    } else if (channel.flags.displayValue2 == DISPLAY_VALUE_CURRENT) {
        data_channel_i_limit(operation, cursor, value);
    } else if (channel.flags.displayValue2 == DISPLAY_VALUE_POWER) {
        if (operation == DATA_OPERATION_GET) {
            value = MakeValue(channel_dispatcher::getPowerLimit(channel), UNIT_WATT);
        }
    }
}

void data_ovp(DataOperationEnum operation, Cursor cursor, Value &value) {
    int iChannel = cursor >= 0 ? cursor : (g_channel ? g_channel->channelIndex : 0);
    Channel &channel = Channel::get(iChannel);
    if (operation == DATA_OPERATION_GET) {
        if (!channel.prot_conf.flags.u_state) {
            value = 0;
        } else if (!channel_dispatcher::isOvpTripped(channel)) {
            value = 1;
        } else {
            value = 2;
        }
    }
}

void data_ocp(DataOperationEnum operation, Cursor cursor, Value &value) {
    int iChannel = cursor >= 0 ? cursor : (g_channel ? g_channel->channelIndex : 0);
    Channel &channel = Channel::get(iChannel);
    if (operation == DATA_OPERATION_GET) {
        if (!channel.prot_conf.flags.i_state) {
            value = 0;
        } else if (!channel_dispatcher::isOcpTripped(channel)) {
            value = 1;
        } else {
            value = 2;
        }
    }
}

void data_opp(DataOperationEnum operation, Cursor cursor, Value &value) {
    int iChannel = cursor >= 0 ? cursor : (g_channel ? g_channel->channelIndex : 0);
    Channel &channel = Channel::get(iChannel);
    if (operation == DATA_OPERATION_GET) {
        if (!channel.prot_conf.flags.p_state) {
            value = 0;
        } else if (!channel_dispatcher::isOppTripped(channel)) {
            value = 1;
        } else {
            value = 2;
        }
    }
}

void data_otp_ch(DataOperationEnum operation, Cursor cursor, Value &value) {
    int iChannel = cursor >= 0 ? cursor : (g_channel ? g_channel->channelIndex : 0);
    Channel &channel = Channel::get(iChannel);
    if (operation == DATA_OPERATION_GET) {
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

void data_otp_aux(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
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

void data_edit_value(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = edit_mode::getEditValue();
    } else if (operation == DATA_OPERATION_GET_MIN) {
        value = edit_mode::getMin();
    } else if (operation == DATA_OPERATION_GET_MAX) {
        value = edit_mode::getMax();
    } else if (operation == DATA_OPERATION_IS_BLINKING) {
        value = !edit_mode::isInteractiveMode() && (edit_mode::getEditValue() != edit_mode::getCurrentValue());
    }
}

void data_edit_unit(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        Keypad *keypad = getActiveKeypad();
        if (keypad) {
            value = getUnitName(keypad->getSwitchToUnit());
        }
    }
}

void data_edit_info(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = Value(0, VALUE_TYPE_EDIT_INFO);
    }
}

void data_edit_mode_interactive_mode_selector(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = edit_mode::isInteractiveMode() ? 0 : 1;
    }
}

void data_edit_steps(DataOperationEnum operation, Cursor cursor, Value &value) {
    StepValues stepValues;
    edit_mode_step::getStepValues(stepValues);

    if (operation == DATA_OPERATION_GET) {
        value = edit_mode_step::getStepIndex() % stepValues.count;
    } else if (operation == DATA_OPERATION_COUNT) {
        value = stepValues.count;
    } else if (operation == DATA_OPERATION_GET_LABEL) {
        edit_mode_step::getStepValues(stepValues);
        value = Value(stepValues.values[cursor], stepValues.unit);
    } else if (operation == DATA_OPERATION_SET) {
        edit_mode_step::setStepIndex(value.getInt());
    }
}

void data_firmware_info(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        static const char FIRMWARE_LABEL[] = "Firmware: ";
        static char firmware_info[sizeof(FIRMWARE_LABEL) - 1 + sizeof(FIRMWARE) - 1 + 1];

        if (*firmware_info == 0) {
            strcat(firmware_info, FIRMWARE_LABEL);
            strcat(firmware_info, FIRMWARE);
        }

        value = firmware_info;
    }
}

void data_keypad_text(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        Keypad *keypad = getActiveKeypad();
        if (keypad) {
            value = keypad->getKeypadTextValue();
        }
    }
}

void data_keypad_mode(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        Keypad *keypad = getActiveKeypad();
        if (keypad) {
            value = keypad->m_keypadMode;
        }
    }
}

void data_keypad_option1_text(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        NumericKeypad *keypad = getActiveNumericKeypad();
        if (keypad) {
            value = Value(keypad->m_options.flags.option1ButtonEnabled ? keypad->m_options.option1ButtonText : "");
        }
    }
}

void data_keypad_option1_enabled(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        NumericKeypad *keypad = getActiveNumericKeypad();
        if (keypad) {
            value = (int)keypad->m_options.flags.option1ButtonEnabled;
        }
    }
}

void data_keypad_option2_text(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        NumericKeypad *keypad = getActiveNumericKeypad();
        if (keypad) {
            value = Value(keypad->m_options.flags.option2ButtonEnabled ? keypad->m_options.option2ButtonText : "");
        }
    }
}

void data_keypad_option2_enabled(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        NumericKeypad *keypad = getActiveNumericKeypad();
        if (keypad) {
            value = (int)keypad->m_options.flags.option2ButtonEnabled;
        }
    }
}

void data_keypad_sign_enabled(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        NumericKeypad *keypad = getActiveNumericKeypad();
        if (keypad) {
            value = (int)keypad->m_options.flags.signButtonEnabled;
        }
    }
}

void data_keypad_dot_enabled(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        NumericKeypad *keypad = getActiveNumericKeypad();
        if (keypad) {
            value = (int)keypad->m_options.flags.dotButtonEnabled;
        }
    }
}

void data_keypad_unit_enabled(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
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
                    keypad->m_options.editValueUnit == UNIT_MILLI_SECOND ||
                    keypad->m_options.editValueUnit == UNIT_OHM ||
                    keypad->m_options.editValueUnit == UNIT_KOHM ||
                    keypad->m_options.editValueUnit == UNIT_MOHM ||
                    keypad->m_options.editValueUnit == UNIT_HERTZ ||
                    keypad->m_options.editValueUnit == UNIT_MILLI_HERTZ ||
                    keypad->m_options.editValueUnit == UNIT_KHERTZ ||
                    keypad->m_options.editValueUnit == UNIT_MHERTZ ||
                    keypad->m_options.editValueUnit == UNIT_FARAD ||
                    keypad->m_options.editValueUnit == UNIT_MILLI_FARAD ||
                    keypad->m_options.editValueUnit == UNIT_MICRO_FARAD ||
                    keypad->m_options.editValueUnit == UNIT_NANO_FARAD ||
                    keypad->m_options.editValueUnit == UNIT_PICO_FARAD;
        }
    }
}

void data_keypad_ok_enabled(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        Keypad *keypad = getActiveKeypad();
        if (keypad) {
            value = keypad->isOkEnabled() ? 1 : 0;
        }
    }
}

void data_channel_off_label(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = io_pins::isInhibited() ? "INH" : "OFF";
    }
}

void data_channel_label(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        int iChannel = cursor >= 0 ? cursor : (g_channel ? g_channel->channelIndex : 0);
        value = Value(iChannel + 1, VALUE_TYPE_CHANNEL_LABEL);
    }
}

void data_channel_short_label(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        int iChannel = cursor >= 0 ? cursor : (g_channel ? g_channel->channelIndex : 0);
        value = Value(iChannel, VALUE_TYPE_CHANNEL_SHORT_LABEL);
    }
}

void data_channel_title(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        int iChannel = cursor >= 0 ? cursor : (g_channel ? g_channel->channelIndex : 0);
        value = Value(iChannel, VALUE_TYPE_CHANNEL_TITLE, Channel::get(iChannel).flags.trackingEnabled);
    }
}

void data_channel_short_title(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        int iChannel = cursor >= 0 ? cursor : (g_channel ? g_channel->channelIndex : 0);
        value = Value(iChannel, VALUE_TYPE_CHANNEL_SHORT_TITLE, Channel::get(iChannel).flags.trackingEnabled);
    }
}

void data_channel_short_title_without_tracking_icon(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        int iChannel = cursor >= 0 ? cursor : (g_channel ? g_channel->channelIndex : 0);
        value = Value(iChannel, VALUE_TYPE_CHANNEL_SHORT_TITLE_WITHOUT_TRACKING_ICON);
    }
}

void data_channel_short_title_with_colon(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        int iChannel = cursor >= 0 ? cursor : (g_channel ? g_channel->channelIndex : 0);
        value = Value(iChannel, VALUE_TYPE_CHANNEL_SHORT_TITLE_WITH_COLON);
    }
}

void data_channel_long_title(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        int iChannel = cursor >= 0 ? cursor : (g_channel ? g_channel->channelIndex : 0);
        value = Value(iChannel, VALUE_TYPE_CHANNEL_LONG_TITLE);
    }
}

void data_channel_info_brand(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        int iChannel = cursor >= 0 ? cursor : (g_channel ? g_channel->channelIndex : 0);
        auto &channel = Channel::get(iChannel);
        value = channel.getBrand();
    }
}

void data_channel_info_serial(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        int iChannel = cursor >= 0 ? cursor : (g_channel ? g_channel->channelIndex : 0);
        value = Value(iChannel, VALUE_TYPE_CHANNEL_INFO_SERIAL);
    }
}

void data_channel_temp_status(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        int iChannel = cursor >= 0 ? cursor : (g_channel ? g_channel->channelIndex : 0);
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

void data_channel_temp(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        float temperature = 0;

        int iChannel = cursor >= 0 ? cursor : (g_channel ? g_channel->channelIndex : 0);
        temperature::TempSensorTemperature &tempSensor = temperature::sensors[temp_sensor::CH1 + iChannel];
        if (tempSensor.isInstalled() && tempSensor.isTestOK()) {
            temperature = tempSensor.temperature;
        }

        value = MakeValue(temperature, UNIT_CELSIUS);
    }
}

void data_channel_on_time_total(DataOperationEnum operation, Cursor cursor, Value &value) {
    int iChannel = cursor >= 0 ? cursor : (g_channel ? g_channel->channelIndex : 0);
    Channel &channel = Channel::get(iChannel);
    if (operation == DATA_OPERATION_GET) {
        value =
            Value((uint32_t)ontime::g_moduleCounters[channel.slotIndex].getTotalTime(), VALUE_TYPE_ON_TIME_COUNTER);
    }
}

void data_channel_on_time_last(DataOperationEnum operation, Cursor cursor, Value &value) {
    int iChannel = cursor >= 0 ? cursor : (g_channel ? g_channel->channelIndex : 0);
    Channel &channel = Channel::get(iChannel);
    if (operation == DATA_OPERATION_GET) {
        value =
            Value((uint32_t)ontime::g_moduleCounters[channel.slotIndex].getLastTime(), VALUE_TYPE_ON_TIME_COUNTER);
    }
}

void data_channel_calibration_status(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        int iChannel = cursor >= 0 ? cursor : (g_channel ? g_channel->channelIndex : 0);
        Channel &channel = Channel::get(iChannel);
        value = channel.isCalibrationExists();
    }
}

void data_channel_calibration_state(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        int iChannel = cursor >= 0 ? cursor : (g_channel ? g_channel->channelIndex : 0);
        Channel &channel = Channel::get(iChannel);
        value = channel.isCalibrationEnabled();
    }
}

void data_channel_calibration_date(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        int iChannel = cursor >= 0 ? cursor : (g_channel ? g_channel->channelIndex : 0);
        Channel &channel = Channel::get(iChannel);

        char *p = channel.cal_conf.calibration_date;

        int year = (p[0] - '0') * 1000 + (p[1] - '0') * 100 + (p[2] - '0') * 10 + (p[3] - '0');
        int month = (p[4] - '0') * 10 + (p[5] - '0');
        int day = (p[6] - '0') * 10 + (p[7] - '0');

        uint32_t time = datetime::makeTime(year, month, day, 0, 0, 0);
        
        value = Value(time, persist_conf::devConf.dateTimeFormat == datetime::FORMAT_DMY_24 || persist_conf::devConf.dateTimeFormat == datetime::FORMAT_DMY_12 ? VALUE_TYPE_DATE_DMY : VALUE_TYPE_DATE_MDY);
    }
}

void data_channel_calibration_remark(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        int iChannel = cursor >= 0 ? cursor : (g_channel ? g_channel->channelIndex : 0);
        Channel &channel = Channel::get(iChannel);
        value = Value(channel.cal_conf.calibration_remark);
    }
}

void data_channel_calibration_step_is_set_remark_step(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = calibration_wizard::g_stepNum == calibration_wizard::MAX_STEP_NUM - 1;
    }
}

void data_channel_calibration_step_num(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = Value(calibration_wizard::g_stepNum);
    }
}

void data_channel_calibration_step_status(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
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

void data_channel_calibration_step_level_value(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = calibration_wizard::getLevelValue();
    }
}

void data_channel_calibration_step_value(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
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
            value = Value(psu::calibration::getRemark());
            break;
        }
    }
}

void data_channel_calibration_step_prev_enabled(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = calibration_wizard::g_stepNum > 0;
    }
}

void data_channel_calibration_step_next_enabled(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = calibration_wizard::g_stepNum < calibration_wizard::MAX_STEP_NUM;
    }
}

void data_channel_calibration_has_step_note(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = calibration_wizard::getStepNote() != nullptr;
    }
}

void data_channel_calibration_step_note(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = calibration_wizard::getStepNote();
    }
}

void data_cal_ch_u_min(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        int iChannel = cursor >= 0 ? cursor : (g_channel ? g_channel->channelIndex : 0);
        Channel &channel = Channel::get(iChannel);
        value = MakeValue(channel.cal_conf.u.min.val, UNIT_VOLT);
    }
}

void data_cal_ch_u_mid(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        int iChannel = cursor >= 0 ? cursor : (g_channel ? g_channel->channelIndex : 0);
        Channel &channel = Channel::get(iChannel);
        value = MakeValue(channel.cal_conf.u.mid.val, UNIT_VOLT);
    }
}

void data_cal_ch_u_max(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        int iChannel = cursor >= 0 ? cursor : (g_channel ? g_channel->channelIndex : 0);
        Channel &channel = Channel::get(iChannel);
        value = MakeValue(channel.cal_conf.u.max.val, UNIT_VOLT);
    }
}

void data_cal_ch_i0_min(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        int iChannel = cursor >= 0 ? cursor : (g_channel ? g_channel->channelIndex : 0);
        Channel &channel = Channel::get(iChannel);
        value = MakeValue(channel.cal_conf.i[0].min.val, UNIT_AMPER);
    }
}

void data_cal_ch_i0_mid(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        int iChannel = cursor >= 0 ? cursor : (g_channel ? g_channel->channelIndex : 0);
        Channel &channel = Channel::get(iChannel);
        value = MakeValue(channel.cal_conf.i[0].mid.val, UNIT_AMPER);
    }
}

void data_cal_ch_i0_max(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        int iChannel = cursor >= 0 ? cursor : (g_channel ? g_channel->channelIndex : 0);
        Channel &channel = Channel::get(iChannel);
        value = MakeValue(channel.cal_conf.i[0].max.val, UNIT_AMPER);
    }
}

void data_cal_ch_i1_min(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        int iChannel = cursor >= 0 ? cursor : (g_channel ? g_channel->channelIndex : 0);
        Channel &channel = Channel::get(iChannel);
        if (channel.hasSupportForCurrentDualRange()) {
            value = MakeValue(channel.cal_conf.i[1].min.val, UNIT_AMPER);
        } else {
            value = Value("");
        }
    }
}

void data_cal_ch_i1_mid(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        int iChannel = cursor >= 0 ? cursor : (g_channel ? g_channel->channelIndex : 0);
        Channel &channel = Channel::get(iChannel);
        if (channel.hasSupportForCurrentDualRange()) {
            value = MakeValue(channel.cal_conf.i[1].mid.val, UNIT_AMPER);
        } else {
            value = Value("");
        }
    }
}

void data_cal_ch_i1_max(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        int iChannel = cursor >= 0 ? cursor : (g_channel ? g_channel->channelIndex : 0);
        Channel &channel = Channel::get(iChannel);
        if (channel.hasSupportForCurrentDualRange()) {
            value = MakeValue(channel.cal_conf.i[1].max.val, UNIT_AMPER);
        } else {
            value = Value("");
        }
    }
}

void data_channel_protection_status(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        int iChannel = cursor >= 0 ? cursor : (g_channel ? g_channel->channelIndex : 0);
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

void data_channel_protection_ovp_state(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        ChSettingsProtectionSetPage *page = (ChSettingsProtectionSetPage *)getPage(PAGE_ID_CH_SETTINGS_PROT_OVP);
        if (page) {
            value = page->state;
        }
    }
}

void data_channel_protection_ovp_type(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        int iChannel = cursor >= 0 ? cursor : (g_channel ? g_channel->channelIndex : 0);
        Channel &channel = Channel::get(iChannel);
        if (channel.params.features & CH_FEATURE_HW_OVP) {
            ChSettingsProtectionSetPage *page = (ChSettingsProtectionSetPage *)getPage(PAGE_ID_CH_SETTINGS_PROT_OVP);
            if (page) {
                value = page->type ? 0 : 1;
            } else {
                if (channel.prot_conf.flags.u_type) {
                    if (channel.isOutputEnabled() && channel.prot_conf.flags.u_hwOvpDeactivated) {
                        value = 3;
                    } else {
                        value = 0;
                    }
                } else {
                    value = 1;
                }
            }
        } else {
            value = 2;
        }
    }
}

void data_channel_protection_ovp_level(DataOperationEnum operation, Cursor cursor, Value &value) {
    int iChannel = cursor >= 0 ? cursor : (g_channel ? g_channel->channelIndex : 0);
    Channel &channel = Channel::get(iChannel);
    if (operation == DATA_OPERATION_GET) {
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
    } else if (operation == DATA_OPERATION_GET_MIN) {
        value = MakeValue(channel_dispatcher::getUMin(channel), UNIT_VOLT);
    } else if (operation == DATA_OPERATION_GET_MAX) {
        value = MakeValue(channel_dispatcher::getUMaxOvpLevel(channel), UNIT_VOLT);
    } else if (operation == DATA_OPERATION_SET) {
        channel_dispatcher::setOvpLevel(channel, value.getFloat());
    }
}

void getProtectionDelayStepValues(StepValues *stepValues) {
    static float values[] = { 20.0f, 10.0f, 5.0f, 1.0f };
    stepValues->values = values;
    stepValues->count = sizeof(values) / sizeof(float);
    stepValues->unit = UNIT_SECOND;
}

void data_channel_protection_ovp_delay(DataOperationEnum operation, Cursor cursor, Value &value) {
    int iChannel = cursor >= 0 ? cursor : (g_channel ? g_channel->channelIndex : 0);
    Channel &channel = Channel::get(iChannel);
    if (operation == DATA_OPERATION_GET) {
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
    } else if (operation == DATA_OPERATION_GET_MIN) {
        value = MakeValue(channel.params.OVP_MIN_DELAY, UNIT_SECOND);
    } else if (operation == DATA_OPERATION_GET_MAX) {
        value = MakeValue(channel.params.OVP_MAX_DELAY, UNIT_SECOND);
    } else if (operation == DATA_OPERATION_SET) {
        channel_dispatcher::setOvpDelay(channel, value.getFloat());
    } else if (operation == DATA_OPERATION_GET_NAME) {
        value = "OVP Delay";
    } else if (operation == DATA_OPERATION_GET_UNIT) {
        value = UNIT_SECOND;
    } else if (operation == DATA_OPERATION_GET_IS_CHANNEL_DATA) {
        value = 1;
    } else if (operation == DATA_OPERATION_GET_ENCODER_STEP_VALUES) {
        getProtectionDelayStepValues(value.getStepValues());
        value = 1;
    }
}

void data_channel_protection_ovp_limit(DataOperationEnum operation, Cursor cursor, Value &value) {
    int iChannel = cursor >= 0 ? cursor : (g_channel ? g_channel->channelIndex : 0);
    Channel &channel = Channel::get(iChannel);
    if (operation == DATA_OPERATION_GET) {
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
    } else if (operation == DATA_OPERATION_GET_MIN) {
        value = MakeValue(channel_dispatcher::getUMin(channel), UNIT_VOLT);
    } else if (operation == DATA_OPERATION_GET_MAX) {
        value = MakeValue(channel_dispatcher::getUMaxOvpLimit(channel), UNIT_VOLT);
    } else if (operation == DATA_OPERATION_SET) {
        channel_dispatcher::setVoltageLimit(channel, value.getFloat());
    } else if (operation == DATA_OPERATION_GET_NAME) {
        value = "OVP Limit";
    } else if (operation == DATA_OPERATION_GET_UNIT) {
        value = UNIT_VOLT;
    } else if (operation == DATA_OPERATION_GET_IS_CHANNEL_DATA) {
        value = 1;
    } else if (operation == DATA_OPERATION_GET_ENCODER_STEP_VALUES) {
        channel.getVoltageStepValues(value.getStepValues());
        value = 1;
    }
}

void data_channel_protection_ocp_state(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        ChSettingsProtectionSetPage *page = (ChSettingsProtectionSetPage *)getPage(PAGE_ID_CH_SETTINGS_PROT_OCP);
        if (page) {
            value = page->state;
        }
    }
}

void data_channel_protection_ocp_delay(DataOperationEnum operation, Cursor cursor, Value &value) {
    int iChannel = cursor >= 0 ? cursor : (g_channel ? g_channel->channelIndex : 0);
    Channel &channel = Channel::get(iChannel);
    if (operation == DATA_OPERATION_GET) {
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
    } else if (operation == DATA_OPERATION_GET_MIN) {
        value = MakeValue(channel.params.OCP_MIN_DELAY, UNIT_SECOND);
    } else if (operation == DATA_OPERATION_GET_MAX) {
        value = MakeValue(channel.params.OCP_MAX_DELAY, UNIT_SECOND);
    } else if (operation == DATA_OPERATION_SET) {
        channel_dispatcher::setOcpParameters(channel, channel.prot_conf.flags.i_state ? 1 : 0, value.getFloat());
    } else if (operation == DATA_OPERATION_GET_NAME) {
        value = "OCP Delay";
    } else if (operation == DATA_OPERATION_GET_UNIT) {
        value = UNIT_SECOND;
    } else if (operation == DATA_OPERATION_GET_IS_CHANNEL_DATA) {
        value = 1;
    } else if (operation == DATA_OPERATION_GET_ENCODER_STEP_VALUES) {
        getProtectionDelayStepValues(value.getStepValues());
        value = 1;
    }
}

void data_channel_protection_ocp_limit(DataOperationEnum operation, Cursor cursor, Value &value) {
    int iChannel = cursor >= 0 ? cursor : (g_channel ? g_channel->channelIndex : 0);
    Channel &channel = Channel::get(iChannel);
    if (operation == DATA_OPERATION_GET) {
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
    } else if (operation == DATA_OPERATION_GET_MIN) {
        value = MakeValue(channel_dispatcher::getIMin(channel), UNIT_AMPER);
    } else if (operation == DATA_OPERATION_GET_MAX) {
        value = MakeValue(channel_dispatcher::getIMaxLimit(channel), UNIT_AMPER);
    } else if (operation == DATA_OPERATION_SET) {
        channel_dispatcher::setCurrentLimit(channel, value.getFloat());
    } else if (operation == DATA_OPERATION_GET_NAME) {
        value = "OCP Limit";
    } else if (operation == DATA_OPERATION_GET_UNIT) {
        value = UNIT_AMPER;
    } else if (operation == DATA_OPERATION_GET_IS_CHANNEL_DATA) {
        value = 1;
    } else if (operation == DATA_OPERATION_GET_ENCODER_STEP_VALUES) {
        channel.getCurrentStepValues(value.getStepValues());
        value = 1;
    }
}

void data_channel_protection_ocp_max_current_limit_cause(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = Value(g_channel->getMaxCurrentLimitCause());
    }
}

void data_channel_protection_opp_state(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        ChSettingsProtectionSetPage *page = (ChSettingsProtectionSetPage *)getPage(PAGE_ID_CH_SETTINGS_PROT_OPP);
        if (page) {
            value = page->state;
        }
    }
}

void data_channel_protection_opp_level(DataOperationEnum operation, Cursor cursor, Value &value) {
    int iChannel = cursor >= 0 ? cursor : (g_channel ? g_channel->channelIndex : 0);
    Channel &channel = Channel::get(iChannel);
    if (operation == DATA_OPERATION_GET) {
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
    } else if (operation == DATA_OPERATION_GET_MIN) {
        value = MakeValue(channel_dispatcher::getOppMinLevel(channel), UNIT_WATT);
    } else if (operation == DATA_OPERATION_GET_MAX) {
        value = MakeValue(channel_dispatcher::getOppMaxLevel(channel), UNIT_WATT);
    } else if (operation == DATA_OPERATION_SET) {
        channel_dispatcher::setOppParameters(channel, channel.prot_conf.flags.p_state ? 1 : 0, value.getFloat(), channel.prot_conf.p_delay);
    } else if (operation == DATA_OPERATION_GET_NAME) {
        value = "OPP Level";
    } else if (operation == DATA_OPERATION_GET_UNIT) {
        value = UNIT_WATT;
    } else if (operation == DATA_OPERATION_GET_IS_CHANNEL_DATA) {
        value = 1;
    } else if (operation == DATA_OPERATION_GET_ENCODER_STEP_VALUES) {
        channel.getPowerStepValues(value.getStepValues());
        value = 1;
    }
}

void data_channel_protection_opp_delay(DataOperationEnum operation, Cursor cursor, Value &value) {
    int iChannel = cursor >= 0 ? cursor : (g_channel ? g_channel->channelIndex : 0);
    Channel &channel = Channel::get(iChannel);
    if (operation == DATA_OPERATION_GET) {
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
    } else if (operation == DATA_OPERATION_GET_MIN) {
        value = MakeValue(channel.params.OPP_MIN_DELAY, UNIT_SECOND);
    } else if (operation == DATA_OPERATION_GET_MAX) {
        value = MakeValue(channel.params.OPP_MAX_DELAY, UNIT_SECOND);
    } else if (operation == DATA_OPERATION_SET) {
        channel_dispatcher::setOppParameters(channel, channel.prot_conf.flags.p_state ? 1 : 0, channel_dispatcher::getPowerProtectionLevel(channel), value.getFloat());
    } else if (operation == DATA_OPERATION_GET_NAME) {
        value = "OPP Delay";
    } else if (operation == DATA_OPERATION_GET_UNIT) {
        value = UNIT_SECOND;
    } else if (operation == DATA_OPERATION_GET_IS_CHANNEL_DATA) {
        value = 1;
    } else if (operation == DATA_OPERATION_GET_ENCODER_STEP_VALUES) {
        getProtectionDelayStepValues(value.getStepValues());
        value = 1;
    }
}

void data_channel_protection_opp_limit(DataOperationEnum operation, Cursor cursor, Value &value) {
    int iChannel = cursor >= 0 ? cursor : (g_channel ? g_channel->channelIndex : 0);
    Channel &channel = Channel::get(iChannel);
    if (operation == DATA_OPERATION_GET) {
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
    } else if (operation == DATA_OPERATION_GET_MIN) {
        value = MakeValue(channel_dispatcher::getPowerMinLimit(channel), UNIT_WATT);
    } else if (operation == DATA_OPERATION_GET_MAX) {
        value = MakeValue(channel_dispatcher::getPowerMaxLimit(channel), UNIT_WATT);
    } else if (operation == DATA_OPERATION_SET) {
        channel_dispatcher::setPowerLimit(channel, value.getFloat());
    } else if (operation == DATA_OPERATION_GET_NAME) {
        value = "OPP Limit";
    } else if (operation == DATA_OPERATION_GET_UNIT) {
        value = UNIT_WATT;
    } else if (operation == DATA_OPERATION_GET_IS_CHANNEL_DATA) {
        value = 1;
    } else if (operation == DATA_OPERATION_GET_ENCODER_STEP_VALUES) {
        channel.getPowerStepValues(value.getStepValues());
        value = 1;
    }
}

void data_channel_protection_otp_installed(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = temperature::isChannelSensorInstalled(g_channel);
    }
}

void data_channel_protection_otp_state(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        ChSettingsProtectionSetPage *page =
            (ChSettingsProtectionSetPage *)getPage(PAGE_ID_CH_SETTINGS_PROT_OTP);
        if (page) {
            value = page->state;
        }
    }
}

void data_channel_protection_otp_level(DataOperationEnum operation, Cursor cursor, Value &value) {
    int iChannel = cursor >= 0 ? cursor : (g_channel ? g_channel->channelIndex : 0);
    Channel &channel = Channel::get(iChannel);
    if (operation == DATA_OPERATION_GET) {
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
    } else if (operation == DATA_OPERATION_GET_MIN) {
        value = MakeValue(OTP_AUX_MIN_LEVEL, UNIT_CELSIUS);
    } else if (operation == DATA_OPERATION_GET_MAX) {
        value = MakeValue(OTP_AUX_MAX_LEVEL, UNIT_CELSIUS);
    } else if (operation == DATA_OPERATION_SET) {
        channel_dispatcher::setOtpParameters(channel, temperature::getChannelSensorState(&channel) ? 1 : 0, value.getFloat(), temperature::getChannelSensorDelay(&channel));
    } else if (operation == DATA_OPERATION_GET_NAME) {
        value = "OTP Level";
    } else if (operation == DATA_OPERATION_GET_UNIT) {
        value = UNIT_CELSIUS;
    } else if (operation == DATA_OPERATION_GET_IS_CHANNEL_DATA) {
        value = 1;
    } else if (operation == DATA_OPERATION_GET_ENCODER_STEP_VALUES) {
        auto stepValues = value.getStepValues();

        static float values[] = { 10.0f, 5.0f, 2.0f, 1.0f };
        stepValues->values = values;
        stepValues->count = sizeof(values) / sizeof(float);
        stepValues->unit = UNIT_CELSIUS;

        value = 1;
    }
}
    
void data_channel_protection_otp_delay(DataOperationEnum operation, Cursor cursor, Value &value) {
    int iChannel = cursor >= 0 ? cursor : (g_channel ? g_channel->channelIndex : 0);
    Channel &channel = Channel::get(iChannel);
    if (operation == DATA_OPERATION_GET) {
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
    } else if (operation == DATA_OPERATION_GET_MIN) {
        value = MakeValue(OTP_AUX_MIN_DELAY, UNIT_SECOND);
    } else if (operation == DATA_OPERATION_GET_MAX) {
        value = MakeValue(OTP_AUX_MAX_DELAY, UNIT_SECOND);
    } else if (operation == DATA_OPERATION_SET) {
        channel_dispatcher::setOtpParameters(channel, temperature::getChannelSensorState(&channel) ? 1 : 0, temperature::getChannelSensorLevel(&channel), value.getFloat());
    } else if (operation == DATA_OPERATION_GET_NAME) {
        value = "OTP Delay";
    } else if (operation == DATA_OPERATION_GET_UNIT) {
        value = UNIT_SECOND;
    } else if (operation == DATA_OPERATION_GET_IS_CHANNEL_DATA) {
        value = 1;
    } else if (operation == DATA_OPERATION_GET_ENCODER_STEP_VALUES) {
        getProtectionDelayStepValues(value.getStepValues());
        value = 1;
    }
}

void data_module_specific_ch_settings(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        int iChannel = cursor >= 0 ? cursor : (g_channel ? g_channel->channelIndex : 0);
        Channel &channel = Channel::get(iChannel);
        auto modulType = g_slots[channel.slotIndex].moduleInfo->moduleType;
        if (modulType == MODULE_TYPE_DCP405) {
            value = PAGE_ID_CH_SETTINGS_DCP405_SPECIFIC;
        } else if (modulType == MODULE_TYPE_DCP405B) {
            value = PAGE_ID_CH_SETTINGS_DCP405B_SPECIFIC;
        } else if (modulType == MODULE_TYPE_DCM220 || modulType == MODULE_TYPE_DCM224) {
            value = PAGE_ID_CH_SETTINGS_DCM220_SPECIFIC;
        } else {
            value = PAGE_ID_NONE;
        }
    }
}

void data_channel_has_error_settings(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        int iChannel = cursor >= 0 ? cursor : (g_channel ? g_channel->channelIndex : 0);
        Channel &channel = Channel::get(iChannel);
        auto modulType = g_slots[channel.slotIndex].moduleInfo->moduleType;
        if (modulType == MODULE_TYPE_DCM220 || modulType == MODULE_TYPE_DCM224) {
            value = 1;
        } else {
            value = 0;
        }
    }
}

void data_channel_settings_page(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        int iChannel = cursor >= 0 ? cursor : (g_channel ? g_channel->channelIndex : 0);
        Channel &channel = Channel::get(iChannel);
        if (channel.isOk()) {
            value = PAGE_ID_CH_SETTINGS_OK;
        } else {
            auto modulType = g_slots[channel.slotIndex].moduleInfo->moduleType;
            if (modulType == MODULE_TYPE_DCM220 || modulType == MODULE_TYPE_DCM224) {
            	uint8_t firmwareMajorVersion;
            	uint8_t firmwareMinorVersion;
            	channel.getFirmwareVersion(firmwareMajorVersion, firmwareMinorVersion);
            	if (!bp3c::flash_slave::g_bootloaderMode || (firmwareMajorVersion == 0 && firmwareMinorVersion == 0)) {
            		value = PAGE_ID_CH_SETTINGS_ERROR_DCM220;
            	} else {
            		value = PAGE_ID_CH_SETTINGS_OK;
            	}
            } else {
                value = PAGE_ID_CH_SETTINGS_ERROR;
            }
        }
    }
}

void data_channel_firmware_version(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        int iChannel = cursor >= 0 ? cursor : (g_channel ? g_channel->channelIndex : 0);
        Channel &channel = Channel::get(iChannel);
        uint8_t majorVersion;
        uint8_t minorVersion;
        channel.getFirmwareVersion(majorVersion, minorVersion);
        value = MakeFirmwareVersionValue(majorVersion, minorVersion);
    }
}

void data_channel_rsense_installed(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
		int iChannel = cursor >= 0 ? cursor : (g_channel ? g_channel->channelIndex : 0);
		Channel &channel = Channel::get(iChannel);
        auto modulType = g_slots[channel.slotIndex].moduleInfo->moduleType;
        if (modulType == MODULE_TYPE_DCP405) {
            value = 1;
        } else {
            value = 0;
        }
    }
}

void data_channel_rsense_status(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
		int iChannel = cursor >= 0 ? cursor : (g_channel ? g_channel->channelIndex : 0);
		Channel &channel = Channel::get(iChannel);
        value = channel.isRemoteSensingEnabled();
    }	
}

void data_channel_rprog_installed(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = g_channel->params.features & CH_FEATURE_RPROG ? 1 : 0;
    }
}

void data_channel_rprog_status(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
		int iChannel = cursor >= 0 ? cursor : (g_channel ? g_channel->channelIndex : 0);
		Channel &channel = Channel::get(iChannel);
		value = (int)channel.flags.rprogEnabled;
    }
}

void data_channel_dprog_installed(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        int iChannel = cursor >= 0 ? cursor : (g_channel ? g_channel->channelIndex : 0);
        Channel &channel = Channel::get(iChannel);
        value = channel.params.features & CH_FEATURE_DPROG ? 1 : 0;
    }
}

void data_channel_dprog(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        int iChannel = cursor >= 0 ? cursor : (g_channel ? g_channel->channelIndex : 0);
        Channel &channel = Channel::get(iChannel);
        value = (int)channel.flags.dprogState;
    }
}

void data_is_coupled_or_tracked(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
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

void data_channel_is_par_ser_coupled_or_tracked(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        int iChannel = cursor >= 0 ? cursor : (g_channel ? g_channel->channelIndex : 0);
        Channel &channel = Channel::get(iChannel);
        if (channel.flags.trackingEnabled || (iChannel < 2 && (channel_dispatcher::getCouplingType() == channel_dispatcher::COUPLING_TYPE_PARALLEL || channel_dispatcher::getCouplingType() == channel_dispatcher::COUPLING_TYPE_SERIES))) {
            value = 1;
        } else {
            value = 0;
        }
    }
}

void data_is_tracking_allowed(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        int n = 0;
        for (int i = 0; i < CH_NUM; i++) {
            if (Channel::get(i).isOk()) {
                ++n;
            }
        }
        value = n >= 2 ? 1 : 0;
    }
}

void data_channel_tracking_is_enabled(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        int iChannel = cursor >= 0 ? cursor : (g_channel ? g_channel->channelIndex : 0);
        Channel &channel = Channel::get(iChannel);
        auto page = (SysSettingsTrackingPage *)getPage(PAGE_ID_SYS_SETTINGS_TRACKING);
        if (page) {
            value = (page->m_trackingEnabled & (1 << channel.channelIndex)) ? 1 : 0;
        } else {
            value = channel.flags.trackingEnabled ? 1 : 0;
        }
    }
}

void data_channel_tracking_is_allowed(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        auto channel = Channel::get(cursor);
        value = channel_dispatcher::isTrackingAllowed(channel, nullptr) ? 1 : 0;
    }
}

void data_is_multi_tracking(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        auto page = (SysSettingsTrackingPage *)getPage(PAGE_ID_SYS_SETTINGS_TRACKING);
        if (page) {
            value = page->getNumTrackingChannels() >= 2 ? 1 : 0;
        }
    }
}

void data_is_any_coupling_allowed(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = channel_dispatcher::isCouplingTypeAllowed(channel_dispatcher::COUPLING_TYPE_PARALLEL, nullptr) ||
                channel_dispatcher::isCouplingTypeAllowed(channel_dispatcher::COUPLING_TYPE_SERIES, nullptr) ||
                channel_dispatcher::isCouplingTypeAllowed(channel_dispatcher::COUPLING_TYPE_COMMON_GND, nullptr) ||
                channel_dispatcher::isCouplingTypeAllowed(channel_dispatcher::COUPLING_TYPE_SPLIT_RAILS, nullptr) ? 1 : 0;
    }
}

void data_is_coupling_parallel_allowed(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = channel_dispatcher::isCouplingTypeAllowed(channel_dispatcher::COUPLING_TYPE_PARALLEL, nullptr) ? 1 : 0;
    }
}

void data_is_coupling_split_rails_allowed(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = channel_dispatcher::isCouplingTypeAllowed(channel_dispatcher::COUPLING_TYPE_SPLIT_RAILS, nullptr) ? 1 : 0;
    }
}


void data_is_coupling_series_allowed(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
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

void data_coupling_type(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = (int)getCouplingType();
    }
}

void data_is_coupling_type_uncoupled(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = getCouplingType() == channel_dispatcher::COUPLING_TYPE_NONE ? 1 : 0;
    }
}

void data_is_coupling_type_series(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = getCouplingType() == channel_dispatcher::COUPLING_TYPE_SERIES ? 1 : 0;
    }
}

void data_is_coupling_type_parallel(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = getCouplingType() == channel_dispatcher::COUPLING_TYPE_PARALLEL ? 1 : 0;
    }
}

void data_is_coupling_type_common_gnd(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = getCouplingType() == channel_dispatcher::COUPLING_TYPE_COMMON_GND ? 1 : 0;
    }
}

void data_is_coupling_type_split_rails(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = getCouplingType() == channel_dispatcher::COUPLING_TYPE_SPLIT_RAILS ? 1 : 0;
    }
}

void data_channel_copy_available(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = CH_NUM >= 2 ? 1 : 0;
    }
}

void data_channel_coupling_is_series(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
		int iChannel = cursor >= 0 ? cursor : (g_channel ? g_channel->channelIndex : 0);
        value = iChannel < 2 && getCouplingType() == channel_dispatcher::COUPLING_TYPE_SERIES ? 1 : 0;
    }
}

void data_channel_coupling_enable_tracking_mode(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        auto page = (SysSettingsCouplingPage *)getPage(PAGE_ID_SYS_SETTINGS_COUPLING);
        if (page) {
            value = page->m_enableTrackingMode ? 1 : 0;
        }
    }
}

void data_sys_on_time_total(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value =
            Value((uint32_t)ontime::g_mcuCounter.getTotalTime(), VALUE_TYPE_ON_TIME_COUNTER);
    }
}

void data_sys_on_time_last(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value =
            Value((uint32_t)ontime::g_mcuCounter.getLastTime(), VALUE_TYPE_ON_TIME_COUNTER);
    }
}

void data_sys_temp_aux_status(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
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

void data_sys_temp_aux_otp_state(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        SysSettingsTemperaturePage *page = (SysSettingsTemperaturePage *)getPage(PAGE_ID_SYS_SETTINGS_TEMPERATURE);
        if (page) {
            value = page->state;
        }
    }
}

void data_sys_temp_aux_otp_level(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        SysSettingsTemperaturePage *page = (SysSettingsTemperaturePage *)getPage(PAGE_ID_SYS_SETTINGS_TEMPERATURE);
        if (page) {
            value = page->level;
        }
    }
}

void data_sys_temp_aux_otp_delay(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        SysSettingsTemperaturePage *page = (SysSettingsTemperaturePage *)getPage(PAGE_ID_SYS_SETTINGS_TEMPERATURE);
        if (page) {
            value = page->delay;
        }
    }
}

void data_sys_temp_aux_otp_is_tripped(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = temperature::sensors[temp_sensor::AUX].isTripped();
    }
}

void data_sys_temp_aux(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        float auxTemperature = 0;
        temperature::TempSensorTemperature &tempSensor = temperature::sensors[temp_sensor::AUX];
        if (tempSensor.isInstalled() && tempSensor.isTestOK()) {
            auxTemperature = tempSensor.temperature;
        }
        value = MakeValue(auxTemperature, UNIT_CELSIUS);
    }
}

bool getSysInfoHasError() {
    temperature::TempSensorTemperature &tempSensor = temperature::sensors[temp_sensor::AUX];
    int err;
    return 
        // AUX temp.
        (tempSensor.isInstalled() && !tempSensor.isTestOK()) ||
        // FAN
        (aux_ps::fan::g_testResult == TEST_FAILED || aux_ps::fan::g_testResult == TEST_WARNING) ||
        // Battery
        mcu::battery::g_testResult == TEST_FAILED ||
        // SD card
        !eez::psu::sd_card::isMounted(&err);
}

void data_sys_info_has_error(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = getSysInfoHasError();
    }
}

void data_sys_settings_has_error(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = (getSysInfoHasError() ||
            // Ethernet
            ethernet::g_testResult == TEST_FAILED ||
            // Mqtt
            (persist_conf::devConf.mqttEnabled && mqtt::g_connectionState == mqtt::CONNECTION_STATE_ERROR)
        ) ? 1 : 0;
    }
}

void data_sys_info_firmware_ver(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = Value(FIRMWARE);
    }
}

void data_sys_info_serial_no(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = Value(getSerialNumber());
    }
}

void data_sys_info_scpi_ver(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = Value(SCPI_STD_VERSION_REVISION);
    }
}

void data_sys_info_cpu(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = Value(getCpuModelAndVersion());
    }
}

void data_sys_info_battery_status(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        if (mcu::battery::g_testResult == TEST_FAILED) {
            value = 0;
        } else if (mcu::battery::g_testResult == TEST_OK) {
            value = 1;
        } else {
            value = 2;
        }
    }
}

void data_battery(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = MakeValue(mcu::battery::g_battery, UNIT_VOLT);
    }
}

void data_sys_info_sdcard_status(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
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
    }
}


void data_sys_info_fan_status(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = aux_ps::fan::getStatus();
    }
}

void data_sys_info_fan_speed(DataOperationEnum operation, Cursor cursor, Value &value) {
#if OPTION_FAN 
    if (operation == DATA_OPERATION_GET) {
        if (aux_ps::fan::g_testResult == TEST_OK) {
            value = MakeValue((float)aux_ps::fan::g_rpm, UNIT_RPM);
        }
    }
#endif
}

void data_sys_fan_mode(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        SysSettingsTemperaturePage *page = (SysSettingsTemperaturePage *)getPage(PAGE_ID_SYS_SETTINGS_TEMPERATURE);
        if (page) {
            value = page->fanMode;
        }
    }
}

void data_sys_fan_speed(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        SysSettingsTemperaturePage *page = (SysSettingsTemperaturePage *)getPage(PAGE_ID_SYS_SETTINGS_TEMPERATURE);
        if (page) {
            value = page->fanSpeedPercentage;
        }
    }
}

void data_channel_board_info_label(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        if (cursor >= 0 && cursor < CH_NUM) {
            value = Value(cursor, VALUE_TYPE_CHANNEL_BOARD_INFO_LABEL);
        }
    }
}

void data_channel_board_info_revision(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        if (cursor >= 0 && cursor < CH_NUM) {
            value = Value((int)Channel::get(cursor).slotIndex, VALUE_TYPE_SLOT_INFO);
        }
    }
}

void data_date_time_date(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        SysSettingsDateTimePage *page = (SysSettingsDateTimePage *)getPage(PAGE_ID_SYS_SETTINGS_DATE_TIME);
        if (page && page->ntpEnabled) {
            uint32_t nowUtc = datetime::nowUtc();
            uint32_t nowLocal = datetime::utcToLocal(nowUtc, page->timeZone, page->dstRule);
            value = Value(nowLocal, page->dateTimeFormat == datetime::FORMAT_DMY_24 || page->dateTimeFormat == datetime::FORMAT_DMY_12 ? VALUE_TYPE_DATE_DMY : VALUE_TYPE_DATE_MDY);
        }
    }
}

void data_date_time_year(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        SysSettingsDateTimePage *page =
            (SysSettingsDateTimePage *)getPage(PAGE_ID_SYS_SETTINGS_DATE_TIME);
        if (page && !page->ntpEnabled) {
            if (!page->dateTimeModified) {
                page->dateTime = datetime::DateTime::now();
            }
            value = Value(page->dateTime.year, VALUE_TYPE_YEAR);
        }
    }
}

void data_date_time_month(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        SysSettingsDateTimePage *page = (SysSettingsDateTimePage *)getPage(PAGE_ID_SYS_SETTINGS_DATE_TIME);
        if (page && !page->ntpEnabled) {
            if (!page->dateTimeModified) {
                page->dateTime = datetime::DateTime::now();
            }
            value = Value(page->dateTime.month, VALUE_TYPE_MONTH);
        }
    }
}

void data_date_time_day(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        SysSettingsDateTimePage *page = (SysSettingsDateTimePage *)getPage(PAGE_ID_SYS_SETTINGS_DATE_TIME);
        if (page && !page->ntpEnabled) {
            if (!page->dateTimeModified) {
                page->dateTime = datetime::DateTime::now();
            }
            value = Value(page->dateTime.day, VALUE_TYPE_DAY);
        }
    }
}

void data_date_time_time(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        SysSettingsDateTimePage *page = (SysSettingsDateTimePage *)getPage(PAGE_ID_SYS_SETTINGS_DATE_TIME);
        if (page && page->ntpEnabled) {
            uint32_t nowUtc = datetime::nowUtc();
            uint32_t nowLocal = datetime::utcToLocal(nowUtc, page->timeZone, page->dstRule);
            value = Value(nowLocal, page->dateTimeFormat == datetime::FORMAT_DMY_24 || page->dateTimeFormat == datetime::FORMAT_MDY_24 ? VALUE_TYPE_TIME : VALUE_TYPE_TIME12);
        }
    }
}

void data_date_time_hour(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        SysSettingsDateTimePage *page = (SysSettingsDateTimePage *)getPage(PAGE_ID_SYS_SETTINGS_DATE_TIME);
        if (page && !page->ntpEnabled) {
            if (!page->dateTimeModified) {
                page->dateTime = datetime::DateTime::now();
            }
            if (page->dateTimeFormat == datetime::FORMAT_DMY_24 || page->dateTimeFormat == datetime::FORMAT_MDY_24) {
                value = Value(page->dateTime.hour, VALUE_TYPE_HOUR);
            } else {
                uint8_t hour = page->dateTime.hour;
                bool am;
                datetime::convertTime24to12(hour, am);
                value = Value(hour, VALUE_TYPE_HOUR);
            }
        }
    }
}

void data_date_time_minute(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        SysSettingsDateTimePage *page = (SysSettingsDateTimePage *)getPage(PAGE_ID_SYS_SETTINGS_DATE_TIME);
        if (page && !page->ntpEnabled) {
            if (!page->dateTimeModified) {
                page->dateTime = datetime::DateTime::now();
            }
            value = Value(page->dateTime.minute, VALUE_TYPE_MINUTE);
        }
    }
}

void data_date_time_second(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        SysSettingsDateTimePage *page = (SysSettingsDateTimePage *)getPage(PAGE_ID_SYS_SETTINGS_DATE_TIME);
        if (page && !page->ntpEnabled) {
            if (!page->dateTimeModified) {
                page->dateTime = datetime::DateTime::now();
            }
            value = Value(page->dateTime.second, VALUE_TYPE_SECOND);
        }
    }
}

void data_date_time_time_zone(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        SysSettingsDateTimePage *page = (SysSettingsDateTimePage *)getPage(PAGE_ID_SYS_SETTINGS_DATE_TIME);
        if (page) {
            value = Value(page->timeZone, VALUE_TYPE_TIME_ZONE);
        }
    }
}

void data_date_time_dst(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        SysSettingsDateTimePage *page = (SysSettingsDateTimePage *)getPage(PAGE_ID_SYS_SETTINGS_DATE_TIME);
        if (page) {
            value = MakeEnumDefinitionValue(page->dstRule, ENUM_DEFINITION_DST_RULE);
        }
    }
}

void data_date_time_format(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        SysSettingsDateTimePage *page = (SysSettingsDateTimePage *)getPage(PAGE_ID_SYS_SETTINGS_DATE_TIME);
        if (page) {
            value = g_enumDefinitions[ENUM_DEFINITION_DATE_TIME_FORMAT][page->dateTimeFormat].menuLabel;
        }
    }
}

void data_date_time_format_is_dmy(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        SysSettingsDateTimePage *page = (SysSettingsDateTimePage *)getPage(PAGE_ID_SYS_SETTINGS_DATE_TIME);
        if (page) {
            value = page->dateTimeFormat == datetime::FORMAT_DMY_24 || page->dateTimeFormat == datetime::FORMAT_DMY_12 ? 1 : 0;
        }
    }
}

void data_date_time_format_is_24h(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        SysSettingsDateTimePage *page = (SysSettingsDateTimePage *)getPage(PAGE_ID_SYS_SETTINGS_DATE_TIME);
        if (page) {
            value = page->dateTimeFormat == datetime::FORMAT_DMY_24 || page->dateTimeFormat == datetime::FORMAT_MDY_24 ? 1 : 0;
        }
    }
}

void data_date_time_am_pm(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        SysSettingsDateTimePage *page = (SysSettingsDateTimePage *)getPage(PAGE_ID_SYS_SETTINGS_DATE_TIME);
        if (page) {
            uint8_t hour = page->dateTime.hour;
            bool am;
            datetime::convertTime24to12(hour, am);
            value = am ? "AM" : "PM";
        }
    }
}

void data_set_page_dirty(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        SetPage *page = (SetPage *)getActivePage();
        if (page) {
            value = Value(page->getDirty());
        }
    }
}

void data_profiles_list(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_COUNT) {
        value = NUM_PROFILE_LOCATIONS - 1; // do not show last location in GUI
    }
}

void data_profiles_auto_recall_status(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = Value(persist_conf::isProfileAutoRecallEnabled());
    }
}

void data_profiles_auto_recall_location(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = Value(persist_conf::getProfileAutoRecallLocation());
    }
}

void data_profile_status(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        int selectedProfileLocation = UserProfilesPage::getSelectedProfileLocation();
        if (selectedProfileLocation != -1) {
            if (profile::isLoaded(selectedProfileLocation)) {
                value = profile::isValid(selectedProfileLocation);
            } else {
                value = 2;
            }
        }
    }
}

void data_profile_label(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        int selectedProfileLocation = UserProfilesPage::getSelectedProfileLocation();
        if (selectedProfileLocation != -1) {
            value = Value(selectedProfileLocation, VALUE_TYPE_USER_PROFILE_LABEL);
        } else if (cursor >= 0) {
            value = Value(cursor, VALUE_TYPE_USER_PROFILE_LABEL);
        }
    }
}

void data_profile_remark(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        int selectedProfileLocation = UserProfilesPage::getSelectedProfileLocation();
        if (selectedProfileLocation != -1) {
            profile::Parameters *profile = profile::getProfileParameters(selectedProfileLocation);
            if (profile) {
                value = Value(profile->name);
            }
        } else if (cursor >= 0) {
            value = Value(cursor, VALUE_TYPE_USER_PROFILE_REMARK);
        }
    }
}

void data_profile_is_auto_recall_location(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        int selectedProfileLocation = UserProfilesPage::getSelectedProfileLocation();
        if (selectedProfileLocation != -1) {
            value = persist_conf::getProfileAutoRecallLocation() == selectedProfileLocation;
        }
    }
}

void data_profile_channel_u_set(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        int selectedProfileLocation = UserProfilesPage::getSelectedProfileLocation();
        if (selectedProfileLocation != -1 && (cursor >= 0 && cursor < CH_MAX)) {
            profile::Parameters *profile = profile::getProfileParameters(selectedProfileLocation);
            if (profile) {
                value = MakeValue(profile->channels[cursor].u_set, UNIT_VOLT);
            }
        }
    }
}

void data_profile_channel_i_set(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        int selectedProfileLocation = UserProfilesPage::getSelectedProfileLocation();
        if (selectedProfileLocation != -1 && (cursor >= 0 && cursor < CH_MAX)) {
            profile::Parameters *profile = profile::getProfileParameters(selectedProfileLocation);
            if (profile) {
                value = MakeValue(profile->channels[cursor].i_set, UNIT_AMPER);
            }
        }
    }
}

void data_profile_channel_output_state(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        int selectedProfileLocation = UserProfilesPage::getSelectedProfileLocation();
        if (selectedProfileLocation != -1 && (cursor >= 0 && cursor < CH_MAX)) {
            profile::Parameters *profile = profile::getProfileParameters(selectedProfileLocation);
            if (profile) {
                value = (int)profile->channels[cursor].flags.output_enabled;
            }
        }
    }
}

void data_ethernet_installed(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = Value(OPTION_ETHERNET);
    }
}

void data_ethernet_enabled(DataOperationEnum operation, Cursor cursor, Value &value) {
#if OPTION_ETHERNET
    if (operation == DATA_OPERATION_GET) {
        SysSettingsEthernetPage *page = (SysSettingsEthernetPage *)getPage(PAGE_ID_SYS_SETTINGS_ETHERNET);
        if (page) {
            value = page->m_enabled;
        } else {
            value = persist_conf::isEthernetEnabled();
        }
    }
#endif
}

void data_ethernet_status(DataOperationEnum operation, Cursor cursor, Value &value) {
#if OPTION_ETHERNET
    if (operation == DATA_OPERATION_GET) {
        if (ethernet::g_testResult == TEST_CONNECTING) {
            value = Value(TEST_CONNECTING);
        } else if (ethernet::g_testResult == TEST_FAILED) {
            value = Value(TEST_FAILED);
        } else {
            value = Value(ethernet::g_testResult);
        }
    }
#endif
}

void data_ethernet_and_mqtt_status(DataOperationEnum operation, Cursor cursor, Value &value) {
#if OPTION_ETHERNET
    if (operation == DATA_OPERATION_GET) {
        if (
            ethernet::g_testResult == TEST_CONNECTING ||
            (
                persist_conf::devConf.mqttEnabled &&
                (
                    mqtt::g_connectionState == mqtt::CONNECTION_STATE_STARTING || 
                    mqtt::g_connectionState == mqtt::CONNECTION_STATE_DNS_IN_PROGRESS ||
                    mqtt::g_connectionState == mqtt::CONNECTION_STATE_DNS_FOUND ||
                    mqtt::g_connectionState == mqtt::CONNECTION_STATE_CONNECTING
                )
            )
        ) {
            value = Value(TEST_CONNECTING);
        } else if (ethernet::g_testResult == TEST_FAILED || (persist_conf::devConf.mqttEnabled && mqtt::g_connectionState == mqtt::CONNECTION_STATE_ERROR)) {
            value = Value(TEST_FAILED);
        } else {
            value = Value(ethernet::g_testResult);
        }
    }
#endif
}

void data_mqtt_in_error(DataOperationEnum operation, Cursor cursor, Value &value) {
#if OPTION_ETHERNET
    if (operation == DATA_OPERATION_GET) {
        value = persist_conf::devConf.mqttEnabled && mqtt::g_connectionState == mqtt::CONNECTION_STATE_ERROR;
    }
#endif
}

void data_ethernet_ip_address(DataOperationEnum operation, Cursor cursor, Value &value) {
#if OPTION_ETHERNET
    if (operation == DATA_OPERATION_GET) {
        SysSettingsEthernetStaticPage *page = (SysSettingsEthernetStaticPage *)getPage(PAGE_ID_SYS_SETTINGS_ETHERNET_STATIC);
        if (page) {
            value = Value(page->m_ipAddress, VALUE_TYPE_IP_ADDRESS);
        } else {
            SysSettingsEthernetPage *page =
                (SysSettingsEthernetPage *)getPage(PAGE_ID_SYS_SETTINGS_ETHERNET);
            if (page) {
                if (page->m_dhcpEnabled) {
                    value = Value(ethernet::getIpAddress(), VALUE_TYPE_IP_ADDRESS);
                } else {
                    value = Value(psu::persist_conf::devConf.ethernetIpAddress, VALUE_TYPE_IP_ADDRESS);
                }
            }
        }
    }
#endif
}

void data_ethernet_host_name(DataOperationEnum operation, Cursor cursor, Value &value) {
#if OPTION_ETHERNET
    if (operation == DATA_OPERATION_GET) {
        SysSettingsEthernetPage *page = (SysSettingsEthernetPage *)getPage(PAGE_ID_SYS_SETTINGS_ETHERNET);
        if (page) {
            value = page->m_hostName;
        }
    } else if (operation == DATA_OPERATION_GET_MAX) {
        value = Value(ETHERNET_HOST_NAME_SIZE, VALUE_TYPE_UINT32);
    } else if (operation == DATA_OPERATION_IS_VALID_VALUE) {
        value = persist_conf::validateEthernetHostName(value.getString());
    } else if (operation == DATA_OPERATION_SET) {
        SysSettingsEthernetPage *page = (SysSettingsEthernetPage *)getPage(PAGE_ID_SYS_SETTINGS_ETHERNET);
        if (page) {
            strcpy(page->m_hostName, value.getString());
        }
    }
#endif
}

void data_ethernet_dns(DataOperationEnum operation, Cursor cursor, Value &value) {
#if OPTION_ETHERNET
    if (operation == DATA_OPERATION_GET) {
        SysSettingsEthernetStaticPage *page =
            (SysSettingsEthernetStaticPage *)getPage(PAGE_ID_SYS_SETTINGS_ETHERNET_STATIC);
        if (page) {
            value = Value(page->m_dns, VALUE_TYPE_IP_ADDRESS);
        }
    }
#endif
}

void data_ethernet_gateway(DataOperationEnum operation, Cursor cursor, Value &value) {
#if OPTION_ETHERNET
    if (operation == DATA_OPERATION_GET) {
        SysSettingsEthernetStaticPage *page =
            (SysSettingsEthernetStaticPage *)getPage(PAGE_ID_SYS_SETTINGS_ETHERNET_STATIC);
        if (page) {
            value = Value(page->m_gateway, VALUE_TYPE_IP_ADDRESS);
        }
    }
#endif
}

void data_ethernet_subnet_mask(DataOperationEnum operation, Cursor cursor, Value &value) {
#if OPTION_ETHERNET
    if (operation == DATA_OPERATION_GET) {
        SysSettingsEthernetStaticPage *page =
            (SysSettingsEthernetStaticPage *)getPage(PAGE_ID_SYS_SETTINGS_ETHERNET_STATIC);
        if (page) {
            value = Value(page->m_subnetMask, VALUE_TYPE_IP_ADDRESS);
        }
    }
#endif
}

void data_ethernet_scpi_port(DataOperationEnum operation, Cursor cursor, Value &value) {
#if OPTION_ETHERNET
    if (operation == DATA_OPERATION_GET) {
        SysSettingsEthernetPage *page =
            (SysSettingsEthernetPage *)getPage(PAGE_ID_SYS_SETTINGS_ETHERNET);
        if (page) {
            value = Value((uint16_t)page->m_scpiPort, VALUE_TYPE_PORT);
        }
    }
#endif
}

void data_ethernet_is_connected(DataOperationEnum operation, Cursor cursor, Value &value) {
#if OPTION_ETHERNET
    if (operation == DATA_OPERATION_GET) {
        value = Value(ethernet::isConnected());
    }
#endif
}

void data_ethernet_dhcp(DataOperationEnum operation, Cursor cursor, Value &value) {
#if OPTION_ETHERNET
    if (operation == DATA_OPERATION_GET) {
        SysSettingsEthernetPage *page =
            (SysSettingsEthernetPage *)getPage(PAGE_ID_SYS_SETTINGS_ETHERNET);
        if (page) {
            value = page->m_dhcpEnabled;
        }
    }
#endif
}

void data_ethernet_mac(DataOperationEnum operation, Cursor cursor, Value &value) {
#if OPTION_ETHERNET
    static uint8_t s_macAddressData[2][6];

    if (operation == DATA_OPERATION_GET) {
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

void data_channel_is_voltage_balanced(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        if (channel_dispatcher::getCouplingType() == channel_dispatcher::COUPLING_TYPE_SERIES) {
            value = Channel::get(0).isVoltageBalanced() || Channel::get(1).isVoltageBalanced();
        } else {
            value = 0;
        }
    }
}

void data_channel_is_current_balanced(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        if (channel_dispatcher::getCouplingType() == channel_dispatcher::COUPLING_TYPE_PARALLEL) {
            value = Channel::get(0).isCurrentBalanced() || Channel::get(1).isCurrentBalanced();
        } else {
            value = 0;
        }
    }
}

void data_sys_output_protection_coupled(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = persist_conf::isOutputProtectionCoupleEnabled();
    }
}

void data_sys_shutdown_when_protection_tripped(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = persist_conf::isShutdownWhenProtectionTrippedEnabled();
    }
}

void data_sys_force_disabling_all_outputs_on_power_up(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = persist_conf::isForceDisablingAllOutputsOnPowerUpEnabled();
    }
}

void data_sys_password_is_set(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = strlen(persist_conf::devConf.systemPassword) > 0;
    }
}

void data_sys_rl_state(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = Value(g_rlState);
    }
}

void data_sys_sound_enabled(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
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

void data_sys_sound_is_enabled(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = persist_conf::isSoundEnabled();
    }
}

void data_sys_sound_is_click_enabled(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = persist_conf::isClickSoundEnabled();
    }
}

void data_channel_display_view_settings_display_value1(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        ChSettingsAdvViewPage *page =
            (ChSettingsAdvViewPage *)getPage(PAGE_ID_CH_SETTINGS_ADV_VIEW);
        if (page) {
            value =
                MakeEnumDefinitionValue(page->displayValue1, ENUM_DEFINITION_CHANNEL_DISPLAY_VALUE);
        }
    }
}

void data_channel_display_view_settings_display_value2(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        ChSettingsAdvViewPage *page =
            (ChSettingsAdvViewPage *)getPage(PAGE_ID_CH_SETTINGS_ADV_VIEW);
        if (page) {
            value =
                MakeEnumDefinitionValue(page->displayValue2, ENUM_DEFINITION_CHANNEL_DISPLAY_VALUE);
        }
    }
}

void data_channel_display_view_settings_yt_view_rate(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        ChSettingsAdvViewPage *page =
            (ChSettingsAdvViewPage *)getPage(PAGE_ID_CH_SETTINGS_ADV_VIEW);
        if (page) {
            value = MakeValue(page->ytViewRate, UNIT_SECOND);
        }
    }
}

void data_sys_encoder_confirmation_mode(DataOperationEnum operation, Cursor cursor, Value &value) {
#if OPTION_ENCODER
    if (operation == DATA_OPERATION_GET) {
        SysSettingsEncoderPage *page =
            (SysSettingsEncoderPage *)getPage(PAGE_ID_SYS_SETTINGS_ENCODER);
        if (page) {
            value = Value((int)page->confirmationMode);
        }
    }
#endif
}

void data_sys_encoder_moving_up_speed(DataOperationEnum operation, Cursor cursor, Value &value) {
#if OPTION_ENCODER
    if (operation == DATA_OPERATION_GET) {
        SysSettingsEncoderPage *page =
            (SysSettingsEncoderPage *)getPage(PAGE_ID_SYS_SETTINGS_ENCODER);
        if (page) {
            value = Value((int)page->movingSpeedUp);
        }
    } else if (operation == DATA_OPERATION_GET_MIN) {
        value = mcu::encoder::MIN_MOVING_SPEED;
    } else if (operation == DATA_OPERATION_GET_MAX) {
        value = mcu::encoder::MAX_MOVING_SPEED;
    } else if (operation == DATA_OPERATION_SET) {
        SysSettingsEncoderPage *page =
            (SysSettingsEncoderPage *)getPage(PAGE_ID_SYS_SETTINGS_ENCODER);
        if (page) {
            page->movingSpeedUp = (uint8_t)value.getInt();
        }
    }
#endif
}

void data_sys_encoder_moving_down_speed(DataOperationEnum operation, Cursor cursor, Value &value) {
#if OPTION_ENCODER
    if (operation == DATA_OPERATION_GET) {
        SysSettingsEncoderPage *page =
            (SysSettingsEncoderPage *)getPage(PAGE_ID_SYS_SETTINGS_ENCODER);
        if (page) {
            value = Value((int)page->movingSpeedDown);
        }
    } else if (operation == DATA_OPERATION_GET_MIN) {
        value = mcu::encoder::MIN_MOVING_SPEED;
    } else if (operation == DATA_OPERATION_GET_MAX) {
        value = mcu::encoder::MAX_MOVING_SPEED;
    } else if (operation == DATA_OPERATION_SET) {
        SysSettingsEncoderPage *page =
            (SysSettingsEncoderPage *)getPage(PAGE_ID_SYS_SETTINGS_ENCODER);
        if (page) {
            page->movingSpeedDown = (uint8_t)value.getInt();
        }
    }
#endif
}

void data_sys_encoder_installed(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = Value(OPTION_ENCODER);
    }
}

void data_sys_display_state(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = (int)persist_conf::devConf.displayState;
    }
}

void data_sys_display_brightness(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = Value(persist_conf::devConf.displayBrightness);
    } else if (operation == DATA_OPERATION_GET_MIN) {
        value = DISPLAY_BRIGHTNESS_MIN;
    } else if (operation == DATA_OPERATION_GET_MAX) {
        value = DISPLAY_BRIGHTNESS_MAX;
    } else if (operation == DATA_OPERATION_SET) {
        persist_conf::setDisplayBrightness((uint8_t)value.getInt());
    }
}

void data_channel_coupling_state(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        int iChannel = cursor >= 0 ? cursor : (g_channel ? g_channel->channelIndex : 0);
        if (iChannel == 1) {
            if (channel_dispatcher::getCouplingType() == channel_dispatcher::COUPLING_TYPE_PARALLEL) {
                value = 1;
            } else if (channel_dispatcher::getCouplingType() == channel_dispatcher::COUPLING_TYPE_SERIES) {
                value = 2;
            } else {
                value = 0;
            }
        } else {
            value = 0;
        }
    }
}

void data_channel_trigger_mode(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        auto page = (ChSettingsTriggerPage *)getPage(PAGE_ID_CH_SETTINGS_TRIGGER);
        value = MakeEnumDefinitionValue(page ? page->triggerMode : channel_dispatcher::getVoltageTriggerMode(*g_channel), ENUM_DEFINITION_CHANNEL_TRIGGER_MODE);
    }
}

void data_channel_u_trigger_value(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET || operation == DATA_OPERATION_GET_EDIT_VALUE) {
        auto page = (SysSettingsRampAndDelayPage *)getPage(PAGE_ID_SYS_SETTINGS_RAMP_AND_DELAY);
        if (page) {
            value = MakeValue(page->triggerVoltage[cursor], UNIT_VOLT);
        } else {
            auto page = (ChSettingsTriggerPage *)getPage(PAGE_ID_CH_SETTINGS_TRIGGER);
            if (page) {
                value = MakeValue(page->triggerVoltage, UNIT_VOLT);
            } else {
                value = MakeValue(channel_dispatcher::getTriggerVoltage(*g_channel), UNIT_VOLT);
            }
        }
    } else if (operation == DATA_OPERATION_SET) {
        auto page = (SysSettingsRampAndDelayPage *)getPage(PAGE_ID_SYS_SETTINGS_RAMP_AND_DELAY);
        if (page) {
            page->setTriggerVoltage(cursor, value.getFloat());
        } else {
            auto page = (ChSettingsTriggerPage *)getPage(PAGE_ID_CH_SETTINGS_TRIGGER);
            if (page) {
                page->triggerVoltage = value.getFloat();
            } else {
                channel_dispatcher::setTriggerVoltage(*g_channel, value.getFloat());
            }
        }
    } else if (operation == DATA_OPERATION_GET_NAME) {
        value = "Voltage step";
    } else {
        data_channel_u_edit(operation, cursor, value);
    }
}

void data_channel_i_trigger_value(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET || operation == DATA_OPERATION_GET_EDIT_VALUE) {
        auto page = (SysSettingsRampAndDelayPage *)getPage(PAGE_ID_SYS_SETTINGS_RAMP_AND_DELAY);
        if (page) {
            value = MakeValue(page->triggerCurrent[cursor], UNIT_AMPER);
        } else {
            auto page = (ChSettingsTriggerPage *)getPage(PAGE_ID_CH_SETTINGS_TRIGGER);
            if (page) {
                value = MakeValue(page->triggerCurrent, UNIT_AMPER);
            } else {
                value = MakeValue(channel_dispatcher::getTriggerCurrent(*g_channel), UNIT_AMPER);
            }
        }
    } else if (operation == DATA_OPERATION_SET) {
        auto page = (SysSettingsRampAndDelayPage *)getPage(PAGE_ID_SYS_SETTINGS_RAMP_AND_DELAY);
        if (page) {
            page->setTriggerCurrent(cursor, value.getFloat());
        } else {
            auto page = (ChSettingsTriggerPage *)getPage(PAGE_ID_CH_SETTINGS_TRIGGER);
            if (page) {
                page->triggerCurrent = value.getFloat();
            } else {
                channel_dispatcher::setTriggerCurrent(*g_channel, value.getFloat());
            }
        } 
    } else if (operation == DATA_OPERATION_GET_NAME) {
        value = "Current step";
    } else {
        data_channel_i_edit(operation, cursor, value);
    }
}

void data_channel_list_count(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        uint16_t listCount;

        ChSettingsListsPage *page = (ChSettingsListsPage *)getPage(PAGE_ID_CH_SETTINGS_LISTS);
        if (page) {
            listCount = page->m_listCount;
        } else {
            listCount = list::getListCount(*g_channel);
        }
        
        if (listCount > 0) {
            value = Value(listCount);
        } else {
            value = INFINITY_SYMBOL;
        }
    }
}

void data_channel_trigger_on_list_stop(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        TriggerOnListStop triggerOnListStop;

        ChSettingsListsPage *page = (ChSettingsListsPage *)getPage(PAGE_ID_CH_SETTINGS_LISTS);
        if (page) {
            triggerOnListStop = page->m_triggerOnListStop;
        } else {
            triggerOnListStop = channel_dispatcher::getTriggerOnListStop(*g_channel);
        }
        
        value = MakeEnumDefinitionValue(triggerOnListStop, ENUM_DEFINITION_CHANNEL_TRIGGER_ON_LIST_STOP);
    }
}

void data_channel_lists(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_COUNT) {
        value = LIST_ITEMS_PER_PAGE;
    } else if (operation == DATA_OPERATION_GET_FLOAT_LIST_LENGTH) {
        ChSettingsListsPage *page = (ChSettingsListsPage *)getPage(PAGE_ID_CH_SETTINGS_LISTS);
        if (page) {
            value = page->getMaxListLength();
        }
    }
}

void data_channel_list_index(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        ChSettingsListsPage *page = (ChSettingsListsPage *)getPage(PAGE_ID_CH_SETTINGS_LISTS);
        if (page) {
            int iPage = page->getPageIndex();
            int iRow = iPage * LIST_ITEMS_PER_PAGE + cursor;
            value = iRow + 1;
        }
    }
}

void data_channel_list_dwell(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        ChSettingsListsPage *page = (ChSettingsListsPage *)getPage(PAGE_ID_CH_SETTINGS_LISTS);
        if (page) {
            int iPage = page->getPageIndex();
            int iRow = iPage * LIST_ITEMS_PER_PAGE + cursor;
            if (iRow < page->m_dwellListLength) {
                value = MakeValue(page->m_dwellList[iRow], UNIT_SECOND);
            } else {
                value = Value(EMPTY_VALUE);
            }
        }
    } else if (operation == DATA_OPERATION_GET_MIN) {
        value = MakeValue(LIST_DWELL_MIN, UNIT_SECOND);
    } else if (operation == DATA_OPERATION_GET_MAX) {
        value = MakeValue(LIST_DWELL_MAX, UNIT_SECOND);
    } else if (operation == DATA_OPERATION_GET_DEF) {
        value = MakeValue(LIST_DWELL_DEF, UNIT_SECOND);
    } else if (operation == DATA_OPERATION_GET_FLOAT_LIST_LENGTH) {
        ChSettingsListsPage *page = (ChSettingsListsPage *)getPage(PAGE_ID_CH_SETTINGS_LISTS);
        if (page) {
            value = page->m_dwellListLength;
        }
    } else if (operation == DATA_OPERATION_GET_FLOAT_LIST) {
        ChSettingsListsPage *page = (ChSettingsListsPage *)getPage(PAGE_ID_CH_SETTINGS_LISTS);
        if (page) {
            value = MakeFloatListValue(page->m_dwellList);
        }
    }
}

void data_channel_list_dwell_enabled(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        ChSettingsListsPage *page = (ChSettingsListsPage *)getPage(PAGE_ID_CH_SETTINGS_LISTS);
        if (page) {
            int iPage = page->getPageIndex();
            int iRow = iPage * LIST_ITEMS_PER_PAGE + cursor;
            value = iRow <= page->m_dwellListLength;
        }
    }
}

void data_channel_list_voltage(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        ChSettingsListsPage *page = (ChSettingsListsPage *)getPage(PAGE_ID_CH_SETTINGS_LISTS);
        if (page) {
            int iPage = page->getPageIndex();
            int iRow = iPage * LIST_ITEMS_PER_PAGE + cursor;
            if (iRow < page->m_voltageListLength) {
                value = MakeValue(page->m_voltageList[iRow], UNIT_VOLT);
            } else {
                value = Value(EMPTY_VALUE);
            }
        }
    } else if (operation == DATA_OPERATION_GET_MIN) {
        value = MakeValue(channel_dispatcher::getUMin(*g_channel), UNIT_VOLT);
    } else if (operation == DATA_OPERATION_GET_MAX) {
        value = MakeValue(channel_dispatcher::getUMax(*g_channel), UNIT_VOLT);
    } else if (operation == DATA_OPERATION_GET_DEF) {
        value = MakeValue(g_channel->u.def, UNIT_VOLT);
    } else if (operation == DATA_OPERATION_GET_FLOAT_LIST_LENGTH) {
        ChSettingsListsPage *page = (ChSettingsListsPage *)getPage(PAGE_ID_CH_SETTINGS_LISTS);
        if (page) {
            value = page->m_voltageListLength;
        }
    } else if (operation == DATA_OPERATION_GET_FLOAT_LIST) {
        ChSettingsListsPage *page = (ChSettingsListsPage *)getPage(PAGE_ID_CH_SETTINGS_LISTS);
        if (page) {
            value = MakeFloatListValue(page->m_voltageList);
        }
    }
}

void data_channel_list_voltage_enabled(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        ChSettingsListsPage *page = (ChSettingsListsPage *)getPage(PAGE_ID_CH_SETTINGS_LISTS);
        if (page) {
            int iPage = page->getPageIndex();
            int iRow = iPage * LIST_ITEMS_PER_PAGE + cursor;
            value = iRow <= page->m_voltageListLength;
        }
    }
}

void data_channel_list_current(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        ChSettingsListsPage *page = (ChSettingsListsPage *)getPage(PAGE_ID_CH_SETTINGS_LISTS);
        if (page) {
            int iPage = page->getPageIndex();
            int iRow = iPage * LIST_ITEMS_PER_PAGE + cursor;
            if (iRow < page->m_currentListLength) {
                value = MakeValue(page->m_currentList[iRow], UNIT_AMPER);
            } else {
                value = Value(EMPTY_VALUE);
            }
        }
    } else if (operation == DATA_OPERATION_GET_MIN) {
        value =
            MakeValue(channel_dispatcher::getIMin(*g_channel), UNIT_AMPER);
    } else if (operation == DATA_OPERATION_GET_MAX) {
        value =
            MakeValue(channel_dispatcher::getIMax(*g_channel), UNIT_AMPER);
    } else if (operation == DATA_OPERATION_GET_DEF) {
        value = MakeValue(g_channel->i.def, UNIT_AMPER);
    } else if (operation == DATA_OPERATION_GET_FLOAT_LIST_LENGTH) {
        ChSettingsListsPage *page = (ChSettingsListsPage *)getPage(PAGE_ID_CH_SETTINGS_LISTS);
        if (page) {
            value = page->m_currentListLength;
        }
    } else if (operation == DATA_OPERATION_GET_FLOAT_LIST) {
        ChSettingsListsPage *page = (ChSettingsListsPage *)getPage(PAGE_ID_CH_SETTINGS_LISTS);
        if (page) {
            ChSettingsListsPage *page = (ChSettingsListsPage *)getActivePage();
            value = MakeFloatListValue(page->m_currentList);
        }
    }
}

void data_channel_list_current_enabled(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        ChSettingsListsPage *page = (ChSettingsListsPage *)getPage(PAGE_ID_CH_SETTINGS_LISTS);
        if (page) {
            int iPage = page->getPageIndex();
            int iRow = iPage * LIST_ITEMS_PER_PAGE + cursor;
            value = iRow <= page->m_currentListLength;
        }
    }
}

void data_channel_lists_previous_page_enabled(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        ChSettingsListsPage *page = (ChSettingsListsPage *)getPage(PAGE_ID_CH_SETTINGS_LISTS);
        if (page) {
            int iPage = page->getPageIndex();
            value = iPage > 0;
        }
    }
}

void data_channel_lists_next_page_enabled(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        ChSettingsListsPage *page = (ChSettingsListsPage *)getPage(PAGE_ID_CH_SETTINGS_LISTS);
        if (page) {
            int iPage = page->getPageIndex();
            value = (iPage < page->getNumPages() - 1);
        }
    }
}

void data_channel_lists_cursor(DataOperationEnum operation, Cursor cursor, Value &value) {
    ChSettingsListsPage *page = (ChSettingsListsPage *)getPage(PAGE_ID_CH_SETTINGS_LISTS);
    if (page) {
        if (operation == DATA_OPERATION_GET) {
            value = Value(page->m_iCursor);
        } else if (operation == DATA_OPERATION_SET) {
            page->m_iCursor = value.getInt();
            page->moveCursorToFirstAvailableCell();
        }
    }
}

void data_channel_lists_insert_menu_enabled(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        ChSettingsListsPage *page = (ChSettingsListsPage *)getPage(PAGE_ID_CH_SETTINGS_LISTS);
        if (page) {
            value = page->getRowIndex() < page->getMaxListLength();
        }
    }
}

void data_channel_lists_delete_menu_enabled(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        ChSettingsListsPage *page = (ChSettingsListsPage *)getPage(PAGE_ID_CH_SETTINGS_LISTS);
        if (page) {
            value = page->getMaxListLength();
        }
    }
}

void data_channel_lists_delete_row_enabled(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        ChSettingsListsPage *page = (ChSettingsListsPage *)getPage(PAGE_ID_CH_SETTINGS_LISTS);
        if (page) {
            value = page->getRowIndex() < page->getMaxListLength();
        }
    }
}

void data_channel_lists_clear_column_enabled(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        ChSettingsListsPage *page = (ChSettingsListsPage *)getPage(PAGE_ID_CH_SETTINGS_LISTS);
        if (page) {
            value = page->getRowIndex() < page->getMaxListLength();
        }
    }
}

void data_channel_lists_delete_rows_enabled(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        ChSettingsListsPage *page = (ChSettingsListsPage *)getPage(PAGE_ID_CH_SETTINGS_LISTS);
        if (page) {
            value = page->getRowIndex() < page->getMaxListLength();
        }
    }
}

void data_trigger_source(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        SysSettingsTriggerPage *page =
            (SysSettingsTriggerPage *)getPage(PAGE_ID_SYS_SETTINGS_TRIGGER);
        if (page) {
            value = MakeEnumDefinitionValue(page->m_source, ENUM_DEFINITION_TRIGGER_SOURCE);
        }
    }
}

void data_trigger_delay(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        SysSettingsTriggerPage *page =
            (SysSettingsTriggerPage *)getPage(PAGE_ID_SYS_SETTINGS_TRIGGER);
        if (page) {
            value = MakeValue(page->m_delay, UNIT_SECOND);
        }
    }
}

void data_trigger_initiate_continuously(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        SysSettingsTriggerPage *page =
            (SysSettingsTriggerPage *)getPage(PAGE_ID_SYS_SETTINGS_TRIGGER);
        if (page) {
            value = page->m_initiateContinuously;
        }
    }
}

void data_trigger_is_initiated(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        int iChannel = cursor >= 0 ? cursor : (g_channel ? g_channel->channelIndex : 0);
        Channel &channel = Channel::get(iChannel);
        value = (trigger::isInitiated() || trigger::isTriggered()) && channel_dispatcher::getVoltageTriggerMode(channel) != TRIGGER_MODE_FIXED;
    } else if (operation == DATA_OPERATION_IS_BLINKING) {
        value = trigger::isInitiated();
    }
}

void data_trigger_is_manual(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = trigger::getSource() == trigger::SOURCE_MANUAL && !trigger::isTriggered();
    }
}

void data_channel_has_support_for_current_dual_range(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        int iChannel = cursor >= 0 ? cursor : (g_channel ? g_channel->channelIndex : 0);
        Channel &channel = Channel::get(iChannel);
        value = channel.hasSupportForCurrentDualRange();
    }
}

void data_channel_ranges_supported(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = g_channel->hasSupportForCurrentDualRange();
    }
}

void data_channel_ranges_mode(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = MakeEnumDefinitionValue(g_channel->getCurrentRangeSelectionMode(),
                                        ENUM_DEFINITION_CHANNEL_CURRENT_RANGE_SELECTION_MODE);
    }
}

void data_channel_ranges_auto_ranging(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = g_channel->isAutoSelectCurrentRangeEnabled();
    }
}

void data_channel_ranges_currently_selected(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        int iChannel = cursor >= 0 ? cursor : (g_channel ? g_channel->channelIndex : 0);
        Channel &channel = Channel::get(iChannel);
        value = MakeEnumDefinitionValue(channel.flags.currentCurrentRange, ENUM_DEFINITION_CHANNEL_CURRENT_RANGE);
    }
}

void data_text_message(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = Value(getTextMessageVersion(), VALUE_TYPE_TEXT_MESSAGE);
    }
}

void data_serial_status(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = Value(serial::g_testResult);
    }
}

void data_serial_enabled(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = persist_conf::isSerialEnabled();
    }
}

void data_serial_is_connected(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = Value(serial::isConnected());
    }
}

void data_io_pins(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_COUNT) {
        value = 4;
    }
}

void data_io_pins_inhibit_state(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
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
    } else if (operation == DATA_OPERATION_IS_BLINKING) {
        value = io_pins::isInhibited();
    }
}

void data_io_pin_number(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = Value(cursor + 1);
    }
}

void data_io_pin_polarity(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        SysSettingsIOPinsPage *page = (SysSettingsIOPinsPage *)getPage(PAGE_ID_SYS_SETTINGS_IO);
        if (page) {
            value = MakeEnumDefinitionValue(page->m_polarity[cursor], ENUM_DEFINITION_IO_PINS_POLARITY);
        }
    }
}

void data_io_pin_function(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        SysSettingsIOPinsPage *page = (SysSettingsIOPinsPage *)getPage(PAGE_ID_SYS_SETTINGS_IO);
        if (page) {
            if (page->m_function[cursor] == io_pins::FUNCTION_PWM) {
                value = 0;
            } else {
                value = 1;
            }
        }
    }
}

void data_io_pin_function_name(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        SysSettingsIOPinsPage *page = (SysSettingsIOPinsPage *)getPage(PAGE_ID_SYS_SETTINGS_IO);
        if (page) {
            if (cursor < DOUT1) {
                value = MakeEnumDefinitionValue(page->m_function[cursor], ENUM_DEFINITION_IO_PINS_INPUT_FUNCTION);
            } else if (cursor == DOUT2) {
                value = MakeEnumDefinitionValue(page->m_function[cursor], ENUM_DEFINITION_IO_PINS_OUTPUT2_FUNCTION);
            } else {
                value = MakeEnumDefinitionValue(page->m_function[cursor], ENUM_DEFINITION_IO_PINS_OUTPUT_FUNCTION);
            }
        }
    }
}

void data_io_pin_state(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        SysSettingsIOPinsPage *page = (SysSettingsIOPinsPage *)getPage(PAGE_ID_SYS_SETTINGS_IO);
        if (page) {
            int pin = cursor;
            if (page->m_function[pin] == io_pins::FUNCTION_NONE) {
                value = 4; // Unassigned
            } else {
                int state = io_pins::getPinState(cursor);

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

void data_io_pin_pwm_frequency(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        bool focused = g_focusCursor == cursor && g_focusDataId == DATA_ID_IO_PIN_PWM_FREQUENCY;
        if (focused && g_focusEditValue.getType() != VALUE_TYPE_NONE) {
            value = g_focusEditValue;
        } else if (focused && getActivePageId() == PAGE_ID_EDIT_MODE_KEYPAD && edit_mode_keypad::g_keypad->isEditing()) {
            data_keypad_text(operation, cursor, value);
        } else {
            SysSettingsIOPinsPage *page = (SysSettingsIOPinsPage *)getPage(PAGE_ID_SYS_SETTINGS_IO);
            if (page && page->getDirty()) {
                value = MakeValue(page->getPwmFrequency(cursor), UNIT_HERTZ);
            } else {
                value = MakeValue(io_pins::getPwmFrequency(cursor), UNIT_HERTZ);
            }
        }
    } else if (operation == DATA_OPERATION_GET_ALLOW_ZERO) {
        value = 1;
    } else if (operation == DATA_OPERATION_GET_MIN) {
        value = MakeValue(io_pins::PWM_MIN_FREQUENCY, UNIT_HERTZ);
    } else if (operation == DATA_OPERATION_GET_MAX) {
        value = MakeValue(io_pins::PWM_MAX_FREQUENCY, UNIT_HERTZ);
    } else if (operation == DATA_OPERATION_GET_NAME) {
        value = "PWM frequency";
    } else if (operation == DATA_OPERATION_GET_UNIT) {
        value = UNIT_HERTZ;
    } else if (operation == DATA_OPERATION_SET) {
        SysSettingsIOPinsPage *page = (SysSettingsIOPinsPage *)getPage(PAGE_ID_SYS_SETTINGS_IO);
        if (page && page->getDirty()) {
            page->setPwmFrequency(cursor, value.getFloat());
        } else {
            io_pins::setPwmFrequency(cursor, value.getFloat());
        }
    } else if (operation == DATA_OPERATION_GET_ENCODER_STEP) {
        float fvalue;

        SysSettingsIOPinsPage *page = (SysSettingsIOPinsPage *)getPage(PAGE_ID_SYS_SETTINGS_IO);
        if (page && page->getDirty()) {
            fvalue = page->getPwmFrequency(cursor);
        } else {
            fvalue = io_pins::getPwmFrequency(cursor);
        }

        value = Value(MAX(powf(10.0f, floorf(log10f(fabsf(fvalue))) - 1), 0.001f), UNIT_HERTZ);
    } else if (operation == DATA_OPERATION_GET_ENCODER_STEP_VALUES) {
        static float values[] = { 10000.0f, 1000.0f, 100.0f, 1.0f };
        StepValues *stepValues = value.getStepValues();
        stepValues->values = values;
        stepValues->count = sizeof(values) / sizeof(float);
        stepValues->unit = UNIT_HERTZ;
        value = 1;
    }
}

void data_io_pin_pwm_duty(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        bool focused = g_focusCursor == cursor && g_focusDataId == DATA_ID_IO_PIN_PWM_DUTY;
        if (focused && g_focusEditValue.getType() != VALUE_TYPE_NONE) {
            value = g_focusEditValue;
        } else if (focused && getActivePageId() == PAGE_ID_EDIT_MODE_KEYPAD && edit_mode_keypad::g_keypad->isEditing()) {
            data_keypad_text(operation, cursor, value);
        } else {
            SysSettingsIOPinsPage *page = (SysSettingsIOPinsPage *)getPage(PAGE_ID_SYS_SETTINGS_IO);
            if (page && page->getDirty()) {
                value = MakeValue(page->getPwmDuty(cursor), UNIT_PERCENT);
            } else {
                value = MakeValue(io_pins::getPwmDuty(cursor), UNIT_PERCENT);
            }
        }
    } else if (operation == DATA_OPERATION_GET_MIN) {
        value = MakeValue(io_pins::PWM_MIN_DUTY, UNIT_PERCENT);
    } else if (operation == DATA_OPERATION_GET_MAX) {
        value = MakeValue(io_pins::PWM_MAX_DUTY, UNIT_PERCENT);
    } else if (operation == DATA_OPERATION_GET_NAME) {
        value = "PWM duty cycle";
    } else if (operation == DATA_OPERATION_GET_UNIT) {
        value = UNIT_PERCENT;
    } else if (operation == DATA_OPERATION_SET) {
        SysSettingsIOPinsPage *page = (SysSettingsIOPinsPage *)getPage(PAGE_ID_SYS_SETTINGS_IO);
        if (page && page->getDirty()) {
            page->setPwmDuty(cursor, value.getFloat());
        } else {
            io_pins::setPwmDuty(cursor, value.getFloat());
        }
    } else if (operation == DATA_OPERATION_GET_ENCODER_STEP) {
        value = Value(1.0f, UNIT_HERTZ);
    } else if (operation == DATA_OPERATION_GET_ENCODER_STEP_VALUES) {
        static float values[] = { 5.0f, 1.0f, 0.5f, 0.1f };
        StepValues *stepValues = value.getStepValues();
        stepValues->values = values;
        stepValues->count = sizeof(values) / sizeof(float);
        stepValues->unit = UNIT_PERCENT;
        value = 1;
    }
}

void data_ntp_enabled(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        SysSettingsDateTimePage *page = (SysSettingsDateTimePage *)getPage(PAGE_ID_SYS_SETTINGS_DATE_TIME);
        if (page) {
            value = page->ntpEnabled;
        } else {
            value = persist_conf::isNtpEnabled();
        }
    }
}

void data_ntp_server(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        SysSettingsDateTimePage *page =
            (SysSettingsDateTimePage *)getPage(PAGE_ID_SYS_SETTINGS_DATE_TIME);
        if (page) {
            value = page->ntpServer[0] ? page->ntpServer : "<not specified>";
        }
    }
}

void data_sys_display_background_luminosity_step(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = Value(persist_conf::devConf.displayBackgroundLuminosityStep);
    } else if (operation == DATA_OPERATION_GET_MIN) {
        value = DISPLAY_BACKGROUND_LUMINOSITY_STEP_MIN;
    } else if (operation == DATA_OPERATION_GET_MAX) {
        value = DISPLAY_BACKGROUND_LUMINOSITY_STEP_MAX;
    } else if (operation == DATA_OPERATION_SET) {
        persist_conf::setDisplayBackgroundLuminosityStep((uint8_t)value.getInt());
    }
}

#if defined(EEZ_PLATFORM_SIMULATOR)

void data_simulator_load_state(DataOperationEnum operation, Cursor cursor, Value &value) {
	if (operation == DATA_OPERATION_GET) {
        Channel &channel = Channel::get(cursor);
		value = channel.simulator.getLoadEnabled() ? 1 : 0;
	}
}

void data_simulator_load(DataOperationEnum operation, Cursor cursor, Value &value) {
	if (operation == DATA_OPERATION_GET) {
        Channel &channel = Channel::get(cursor);
		value = MakeValue(channel.simulator.getLoad(), UNIT_OHM);
	}
}

void data_simulator_load_state2(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        Channel &channel = Channel::get(cursor + 1);
        value = channel.simulator.getLoadEnabled() ? 1 : 0;
    }
}

void data_simulator_load2(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        Channel &channel = Channel::get(cursor + 1);
        value = MakeValue(channel.simulator.getLoad(), UNIT_OHM);
    }
}

#endif

void data_channel_active_coupled_led(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        Channel &channel = Channel::get(cursor);
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

void data_channels_with_list_counter_visible(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_COUNT) {
        value = list::g_numChannelsWithVisibleCounters;
    }
}

void data_list_counter_label(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        if (cursor >= 0 && cursor < list::g_numChannelsWithVisibleCounters) {
            int iChannel = list::g_channelsWithVisibleCounters[cursor];
            value = Value(iChannel, VALUE_TYPE_CHANNEL_SHORT_TITLE_WITHOUT_TRACKING_ICON);
        }
    }
}

void data_channel_list_countdown(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        if (cursor >= 0 && cursor < list::g_numChannelsWithVisibleCounters) {
            int iChannel = list::g_channelsWithVisibleCounters[cursor];
            Channel &channel = Channel::get(iChannel);
            int32_t remaining;
            uint32_t total;
            if (list::getCurrentDwellTime(channel, remaining, total) && total >= CONF_LIST_COUNDOWN_DISPLAY_THRESHOLD) {
                value = Value((uint32_t)remaining, VALUE_TYPE_COUNTDOWN);
            }
        }
    }
}

void data_channels_with_ramp_counter_visible(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_COUNT) {
        value = ramp::g_numChannelsWithVisibleCounters;
    }
}

void data_ramp_counter_label(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        if (cursor >= 0 && cursor < ramp::g_numChannelsWithVisibleCounters) {
            int iChannel = ramp::g_channelsWithVisibleCounters[cursor];
            value = Value(iChannel, VALUE_TYPE_CHANNEL_SHORT_TITLE_WITHOUT_TRACKING_ICON);
        }
    }
}

void data_channel_ramp_countdown(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        if (cursor >= 0 && cursor < ramp::g_numChannelsWithVisibleCounters) {
            int iChannel = ramp::g_channelsWithVisibleCounters[cursor];
            uint32_t remaining;
            uint32_t total;
            ramp::getCountdownTime(iChannel, remaining, total);
            if (total >= CONF_RAMP_COUNDOWN_DISPLAY_THRESHOLD) {
                value = Value(remaining, VALUE_TYPE_COUNTDOWN);
            }
        }
    }
}

void data_overlay(DataOperationEnum operation, Cursor cursor, Value &value) {
    enum {
        LIST_ICON_WIDGET,
        LIST_GRID_WIDGET,
        RAMP_ICON_WIDGET,
        RAMP_GRID_WIDGET,
        DLOG_INFO_WIDGET,
        SCRIPT_INFO_WIDGET,
        NUM_WIDGETS
    };

    static Overlay overlay;
    static WidgetOverride widgetOverrides[NUM_WIDGETS];

    if (operation == DATA_OPERATION_GET_OVERLAY_DATA) {
        value = Value(&overlay, VALUE_TYPE_POINTER);
    } else if (operation == DATA_OPERATION_UPDATE_OVERLAY_DATA) {
        overlay.widgetOverrides = widgetOverrides;

        bool areListCountersVisible = list::g_numChannelsWithVisibleCounters > 0;
        bool areRampCountersVisible = ramp::g_numChannelsWithVisibleCounters > 0;
        bool isDlogVisible = !dlog_record::isIdle();
        bool isScriptVisible = !mp::isIdle();

        int state = 0;
        if (areListCountersVisible || areRampCountersVisible || isDlogVisible || isScriptVisible) {
            state = 0;

            if (list::g_numChannelsWithVisibleCounters > 0) {
                if (list::g_numChannelsWithVisibleCounters > 4) {
                    state |= 1;
                } else if (list::g_numChannelsWithVisibleCounters > 2) {
                    state |= 2;
                } else if (list::g_numChannelsWithVisibleCounters == 2) {
                    state |= 3;
                } else {
                    state |= 4;
                }
            }

            if (ramp::g_numChannelsWithVisibleCounters > 0) {
                if (ramp::g_numChannelsWithVisibleCounters > 4) {
                    state |= 1 << 4;
                } else if (ramp::g_numChannelsWithVisibleCounters > 2) {
                    state |= 2 << 4;
                } else if (ramp::g_numChannelsWithVisibleCounters == 2) {
                    state |= 3 << 4;
                } else {
                    state |= 4 << 4;
                }
            }

            if (isDlogVisible) {
                state |= 0x8000;
            }

            if (isScriptVisible) {
                state |= 0x4000;
            }
        }

        if (overlay.state != state) {
            overlay.state = state;
            if (state > 0) {
                WidgetCursor &widgetCursor = *(WidgetCursor *)value.getVoidPointer();

                const ContainerWidget *containerWidget = GET_WIDGET_PROPERTY(widgetCursor.widget, specific, const ContainerWidget *);

                const Widget *listIconWidget = GET_WIDGET_LIST_ELEMENT(containerWidget->widgets, LIST_ICON_WIDGET);
                const Widget *listGridWidget = GET_WIDGET_LIST_ELEMENT(containerWidget->widgets, LIST_GRID_WIDGET);
                const Widget *rampIconWidget = GET_WIDGET_LIST_ELEMENT(containerWidget->widgets, RAMP_ICON_WIDGET);
                const Widget *rampGridWidget = GET_WIDGET_LIST_ELEMENT(containerWidget->widgets, RAMP_GRID_WIDGET);
                const Widget *dlogInfoWidget = GET_WIDGET_LIST_ELEMENT(containerWidget->widgets, DLOG_INFO_WIDGET);
                const Widget *scriptInfoWidget = GET_WIDGET_LIST_ELEMENT(containerWidget->widgets, SCRIPT_INFO_WIDGET);

                overlay.width = widgetCursor.widget->w;
                if (list::g_numChannelsWithVisibleCounters <= 1 && ramp::g_numChannelsWithVisibleCounters <= 1 && !isDlogVisible && !isScriptVisible) {
                    overlay.width -= listGridWidget->w / 2;
                }

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
                        if (ramp::g_numChannelsWithVisibleCounters == 1) {
                            widgetOverrides[LIST_GRID_WIDGET].w = listGridWidget->w / 2;
                        }
                    }
                } else {
                    widgetOverrides[LIST_ICON_WIDGET].isVisible = false;
                    widgetOverrides[LIST_GRID_WIDGET].isVisible = false;
                    overlay.height -= listGridWidget->h;
                }

                if (ramp::g_numChannelsWithVisibleCounters > 0) {
                    widgetOverrides[RAMP_ICON_WIDGET].isVisible = true;
                    widgetOverrides[RAMP_ICON_WIDGET].x = rampIconWidget->x;
                    widgetOverrides[RAMP_ICON_WIDGET].y = overlay.height - (widgetCursor.widget->h - rampIconWidget->y);
                    widgetOverrides[RAMP_ICON_WIDGET].w = rampIconWidget->w;
                    widgetOverrides[RAMP_ICON_WIDGET].h = rampIconWidget->h;

                    widgetOverrides[RAMP_GRID_WIDGET].isVisible = true;
                    widgetOverrides[RAMP_GRID_WIDGET].x = rampGridWidget->x;
                    widgetOverrides[RAMP_GRID_WIDGET].y = overlay.height - (widgetCursor.widget->h - rampGridWidget->y);
                    widgetOverrides[RAMP_GRID_WIDGET].w = rampGridWidget->w;

                    if (ramp::g_numChannelsWithVisibleCounters > 4) {
                        widgetOverrides[RAMP_GRID_WIDGET].h = 3 * rampGridWidget->h;
                        overlay.height += 2 * rampGridWidget->h;
                    } else if (ramp::g_numChannelsWithVisibleCounters > 2) {
                        widgetOverrides[RAMP_GRID_WIDGET].h = 2 * rampGridWidget->h;
                        overlay.height += rampGridWidget->h;
                    } else {
                        widgetOverrides[RAMP_GRID_WIDGET].h = rampGridWidget->h;
                        if (ramp::g_numChannelsWithVisibleCounters == 1) {
                            widgetOverrides[RAMP_GRID_WIDGET].w = rampGridWidget->w / 2;
                        }
                    }
                } else {
                    widgetOverrides[RAMP_ICON_WIDGET].isVisible = false;
                    widgetOverrides[RAMP_GRID_WIDGET].isVisible = false;
                    overlay.height -= rampGridWidget->h;
                }

                if (isDlogVisible) {
                    widgetOverrides[DLOG_INFO_WIDGET].isVisible = true;
                    widgetOverrides[DLOG_INFO_WIDGET].x = dlogInfoWidget->x;
                    widgetOverrides[DLOG_INFO_WIDGET].y = overlay.height - (widgetCursor.widget->h - dlogInfoWidget->y);
                    widgetOverrides[DLOG_INFO_WIDGET].w = dlogInfoWidget->w;
                    widgetOverrides[DLOG_INFO_WIDGET].h = dlogInfoWidget->h;
                } else {
                    widgetOverrides[DLOG_INFO_WIDGET].isVisible = false;
                    overlay.height -= dlogInfoWidget->h;
                }

                if (isScriptVisible) {
                    widgetOverrides[SCRIPT_INFO_WIDGET].isVisible = true;
                    widgetOverrides[SCRIPT_INFO_WIDGET].x = scriptInfoWidget->x;
                    widgetOverrides[SCRIPT_INFO_WIDGET].y = overlay.height - (widgetCursor.widget->h - scriptInfoWidget->y);
                    widgetOverrides[SCRIPT_INFO_WIDGET].w = scriptInfoWidget->w;
                    widgetOverrides[SCRIPT_INFO_WIDGET].h = scriptInfoWidget->h;
                } else {
                    widgetOverrides[SCRIPT_INFO_WIDGET].isVisible = false;
                    overlay.height -= scriptInfoWidget->h;
                }
            }
        }

        value = Value(&overlay, VALUE_TYPE_POINTER);
    }
}

void data_nondrag_overlay(DataOperationEnum operation, Cursor cursor, Value &value) {
}

void data_dlog_state(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = dlog_record::getState();
    }
}

void data_dlog_toggle_state(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
    	if (CH_NUM == 0) {
            value = 5;
        } else if (dlog_record::isInStateTransition()) {
    		value = 4;
    	} else if (dlog_record::isIdle()) {
            value = 0;
        } else if (dlog_record::isExecuting()) {
            value = 1;
        } else if (dlog_record::isInitiated() && dlog_record::g_parameters.triggerSource == trigger::SOURCE_MANUAL) {
            value = 2;
        } else {
            value = 3;
        }
    }
}

void data_is_show_live_recording(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        auto &recording = dlog_view::getRecording();
        value = &recording == &dlog_record::g_recording;
    }
}

void data_channel_history_values(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_YT_DATA_GET_GET_VALUE_FUNC) {
        value = Channel::getChannelHistoryValueFuncs(cursor);
    } else if (operation == DATA_OPERATION_YT_DATA_GET_REFRESH_COUNTER) {
        value = Value(0, VALUE_TYPE_UINT32);
    } else if (operation == DATA_OPERATION_YT_DATA_GET_SIZE) {
        value = Value(CHANNEL_HISTORY_SIZE, VALUE_TYPE_UINT32);
    } else if (operation == DATA_OPERATION_YT_DATA_GET_POSITION) {
        int iChannel = cursor >= 0 ? cursor : (g_channel ? g_channel->channelIndex : 0);
        value = Value(Channel::get(iChannel).getCurrentHistoryValuePosition() - 1, VALUE_TYPE_UINT32);
    } else if (operation == DATA_OPERATION_YT_DATA_GET_STYLE) {
        int iChannel = cursor >= 0 ? cursor : (g_channel ? g_channel->channelIndex : 0);
        Channel &channel = Channel::get(iChannel);
        // if (channel.getMode() == CHANNEL_MODE_UR) {
        //     value = Value(STYLE_ID_YT_GRAPH_UNREGULATED, VALUE_TYPE_UINT16);
        // } else {
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
        // }
    } else if (operation == DATA_OPERATION_YT_DATA_GET_MIN) {
        value = getMin(cursor, value.getUInt8() == 0 ? DATA_ID_CHANNEL_DISPLAY_VALUE1 : DATA_ID_CHANNEL_DISPLAY_VALUE2);
    } else if (operation == DATA_OPERATION_YT_DATA_GET_MAX) {
        value = getLimit(cursor, value.getUInt8() == 0 ? DATA_ID_CHANNEL_DISPLAY_VALUE1 : DATA_ID_CHANNEL_DISPLAY_VALUE2);
    } else if (operation == DATA_OPERATION_YT_DATA_GET_GRAPH_UPDATE_METHOD) {
        value = Value(psu::persist_conf::devConf.ytGraphUpdateMethod, VALUE_TYPE_UINT8);
    }
}

void data_dlog_enabled(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = (
                channel_dispatcher::getCouplingType() == channel_dispatcher::COUPLING_TYPE_PARALLEL || 
                channel_dispatcher::getCouplingType() == channel_dispatcher::COUPLING_TYPE_SERIES
            ) && cursor == 1 ? 0 : 1;
    }
}

void data_dlog_voltage_enabled(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = dlog_record::g_guiParameters.logVoltage[cursor];
    }
}

void data_dlog_current_enabled(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = dlog_record::g_guiParameters.logCurrent[cursor];
    }
}

void data_dlog_power_enabled(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = dlog_record::g_guiParameters.logPower[cursor];
    }
}

void data_dlog_period(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = MakeValue(dlog_record::g_guiParameters.period, UNIT_SECOND);
    } else if (operation == DATA_OPERATION_GET_UNIT) {
        value = UNIT_SECOND;
    } else if (operation == DATA_OPERATION_GET_MIN) {
        value = MakeValue(dlog_record::PERIOD_MIN, UNIT_SECOND);
    } else if (operation == DATA_OPERATION_GET_MAX) {
        value = MakeValue(dlog_record::PERIOD_MAX, UNIT_SECOND);
    } else if (operation == DATA_OPERATION_SET) {
        dlog_record::g_guiParameters.period = value.getFloat();
    }
}

void data_dlog_duration(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = MakeValue(dlog_record::g_guiParameters.time, UNIT_SECOND);
    } else if (operation == DATA_OPERATION_GET_UNIT) {
        value = UNIT_SECOND;
    } else if (operation == DATA_OPERATION_GET_MIN) {
        value = MakeValue(dlog_record::TIME_MIN, UNIT_SECOND);
    } else if (operation == DATA_OPERATION_GET_MAX) {
        value = MakeValue(INFINITY, UNIT_SECOND);
    } else if (operation == DATA_OPERATION_SET) {
        dlog_record::g_guiParameters.time = value.getFloat();
    }
}

void data_dlog_file_name(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = dlog_record::g_guiParameters.filePath;
    } else if (operation == DATA_OPERATION_SET) {
        strcpy(dlog_record::g_guiParameters.filePath, value.getString());
    } else if (operation == DATA_OPERATION_GET_MAX) {
        value = Value(MAX_PATH_LENGTH, VALUE_TYPE_UINT32);
    }
}

void data_dlog_start_enabled(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = dlog_record::checkDlogParameters(dlog_record::g_guiParameters, true, false) == SCPI_RES_OK ? 1 : 0;
    }
}

void data_dlog_view_state(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = dlog_view::getState();
    }
}

void data_recording_ready(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = dlog_record::isExecuting() || dlog_record::getLatestFilePath() ? 1 : 0;
    }
}

void data_is_single_page_on_stack(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = getNumPagesOnStack() == 1 ? 1 : 0;
    }
}

void data_recording(DataOperationEnum operation, Cursor cursor, Value &value) {
    auto &recording = dlog_view::getRecording();

    if (operation == DATA_OPERATION_YT_DATA_GET_REFRESH_COUNTER) {
        value = Value(recording.refreshCounter, VALUE_TYPE_UINT32);
    } else if (operation == DATA_OPERATION_YT_DATA_GET_GET_VALUE_FUNC) {
        value = recording.getValue;
    } else if (operation == DATA_OPERATION_YT_DATA_VALUE_IS_VISIBLE) {
        value = Value(recording.dlogValues[value.getUInt8()].isVisible);
    } else if (operation == DATA_OPERATION_YT_DATA_GET_SHOW_LABELS) {
        value = dlog_view::g_showLabels ? 1 : 0;
    } else if (operation == DATA_OPERATION_YT_DATA_GET_SELECTED_VALUE_INDEX) {
        value = dlog_view::g_showLegend ? dlog_view::getDlogValueIndex(recording, recording.selectedVisibleValueIndex) : -1;
    } else if (operation == DATA_OPERATION_YT_DATA_GET_LABEL) {
        YtDataGetLabelParams *params = (YtDataGetLabelParams *)value.getVoidPointer();
        dlog_view::getLabel(dlog_view::getRecording(), params->valueIndex, params->text, params->count);
    } else if (operation == DATA_OPERATION_YT_DATA_GET_SIZE) {
        value = Value(recording.size, VALUE_TYPE_UINT32);
    } else if (operation == DATA_OPERATION_YT_DATA_GET_POSITION) {
        value = Value(dlog_view::getPosition(recording), VALUE_TYPE_UINT32);
    } else if (operation == DATA_OPERATION_YT_DATA_SET_POSITION) {
        int32_t newPosition = value.getUInt32();
        if (newPosition < 0) {
            newPosition = 0;
        } else {
            if (newPosition + recording.pageSize > recording.size) {
                newPosition = recording.size - recording.pageSize;
            }
        }
        dlog_view::changeXAxisOffset(recording, newPosition * recording.parameters.period);
    } else if (operation == DATA_OPERATION_YT_DATA_GET_PAGE_SIZE) {
        value = Value(recording.pageSize, VALUE_TYPE_UINT32);
    } else if (operation == DATA_OPERATION_YT_DATA_GET_STYLE) {
        uint8_t dlogValueIndex = value.getUInt8();
        uint8_t visibleDlogValueIndex = dlog_view::getVisibleDlogValueIndex(recording, dlogValueIndex);
        uint8_t numYtGraphStyles = sizeof(g_ytGraphStyles) / sizeof(uint16_t);
        if (psu::dlog_view::isMulipleValuesOverlayHeuristic(recording)) {
            value = Value(g_ytGraphStyles[visibleDlogValueIndex % numYtGraphStyles], VALUE_TYPE_UINT16);
        } else {
            if (dlog_view::g_showLegend && visibleDlogValueIndex == recording.selectedVisibleValueIndex) {
                value = Value(STYLE_ID_YT_GRAPH_Y1, VALUE_TYPE_UINT16);
            } else {
                value = Value(STYLE_ID_YT_GRAPH_Y5, VALUE_TYPE_UINT16);
            }
        }
    } else if (operation == DATA_OPERATION_YT_DATA_GET_HORZ_DIVISIONS) {
        value = dlog_view::NUM_HORZ_DIVISIONS;
    } else if (operation == DATA_OPERATION_YT_DATA_GET_VERT_DIVISIONS) {
        value = dlog_view::NUM_VERT_DIVISIONS;
    } else if (operation == DATA_OPERATION_YT_DATA_GET_DIV) {
        value = Value(recording.dlogValues[value.getUInt8()].div, recording.parameters.yAxes[value.getUInt8()].unit);
    } else if (operation == DATA_OPERATION_YT_DATA_GET_OFFSET) {
        value = Value(recording.dlogValues[value.getUInt8()].offset, recording.parameters.yAxes[value.getUInt8()].unit);
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
            dlog_view::DlogValueParams *dlogValueParams = dlog_view::getVisibleDlogValueParams(recording, !dlog_view::isMulipleValuesOverlayHeuristic(recording) ? recording.selectedVisibleValueIndex : g_focusCursor);

            if (touchDrag->type == EVENT_TYPE_TOUCH_DOWN) {
                valueAtTouchDown = dlogValueParams->offset;
                yAtTouchDown = touchDrag->y;
            } else {
                float newOffset = valueAtTouchDown + dlogValueParams->div * (yAtTouchDown - touchDrag->y) * dlog_view::NUM_VERT_DIVISIONS / dlog_view::VIEW_HEIGHT;

                float min = getMin(g_focusCursor, DATA_ID_DLOG_VISIBLE_VALUE_OFFSET).getFloat();
                float max = getMax(g_focusCursor, DATA_ID_DLOG_VISIBLE_VALUE_OFFSET).getFloat();

                newOffset = clamp(newOffset, min, max);
                
                dlogValueParams->offset = newOffset;
            }
        } else if (g_focusDataId == DATA_ID_DLOG_VISIBLE_VALUE_DIV) {
            dlog_view::DlogValueParams *dlogValueParams = dlog_view::getVisibleDlogValueParams(recording, !dlog_view::isMulipleValuesOverlayHeuristic(recording) ? recording.selectedVisibleValueIndex : g_focusCursor);

            if (touchDrag->type == EVENT_TYPE_TOUCH_DOWN) {
                valueAtTouchDown = dlogValueParams->div;

                //uint32_t position = ytDataGetPosition(cursor, DATA_ID_RECORDING);
                //Value::YtDataGetValueFunctionPointer ytDataGetValue = ytDataGetGetValueFunc(cursor, DATA_ID_RECORDING);
                //for (int i = position + touchDrag->x - 10; i < position + touchDrag->x + 10; )

                yAtTouchDown = touchDrag->y;
            } else {
                float scale = 1.0f * (dlog_view::VIEW_HEIGHT - yAtTouchDown) / (dlog_view::VIEW_HEIGHT - touchDrag->y);

                float newDiv = valueAtTouchDown * scale;

                float min = getMin(g_focusCursor, DATA_ID_DLOG_VISIBLE_VALUE_DIV).getFloat();
                float max = getMax(g_focusCursor, DATA_ID_DLOG_VISIBLE_VALUE_DIV).getFloat();

                newDiv = clamp(newDiv, min, max);

                dlogValueParams->offset = dlogValueParams->offset * newDiv / dlogValueParams->div;
                dlogValueParams->div = newDiv;
            }
        } else if (g_focusDataId == DATA_ID_DLOG_X_AXIS_DIV) {
            if (touchDrag->type == EVENT_TYPE_TOUCH_DOWN) {
                valueAtTouchDown = recording.xAxisDiv;
                xAtTouchDown = touchDrag->x;
            } else {
                float scale = 1.0f * (dlog_view::VIEW_WIDTH - touchDrag->x) / (dlog_view::VIEW_WIDTH - xAtTouchDown);

                float newDiv = valueAtTouchDown * scale;

                float min = getMin(g_focusCursor, DATA_ID_DLOG_X_AXIS_DIV).getFloat();
                float max = getMax(g_focusCursor, DATA_ID_DLOG_X_AXIS_DIV).getFloat();

                newDiv = clamp(newDiv, min, max);

                dlog_view::changeXAxisDiv(recording, newDiv);
            }
        } else {
            recording.cursorOffset = MIN(dlog_view::VIEW_WIDTH - 1, MAX(touchDrag->x, 0));
        }
    } else if (operation == DATA_OPERATION_YT_DATA_GET_CURSOR_X_VALUE) {
        float xValue = (dlog_view::getPosition(recording) + recording.cursorOffset) * recording.parameters.period;
        if (recording.parameters.xAxis.scale == dlog_view::SCALE_LOGARITHMIC) {
            xValue = powf(10, recording.parameters.xAxis.range.min + xValue);
        }
        value = Value(roundPrec(xValue, 0.001f), recording.parameters.xAxis.unit);
    } else if (operation == DATA_OPERATION_GET) {
        value = Value((dlog_view::getPosition(recording) + recording.cursorOffset) * recording.parameters.period, dlog_view::getXAxisUnit(recording));
    } else if (operation == DATA_OPERATION_GET_MIN) {
        value = Value(0.0f, dlog_view::getXAxisUnit(recording));
    } else if (operation == DATA_OPERATION_GET_MAX) {
        value = Value(recording.parameters.xAxis.range.max - recording.parameters.xAxis.range.min, dlog_view::getXAxisUnit(recording));
    } else if (operation == DATA_OPERATION_GET_DEF) {
        value = Value((recording.parameters.xAxis.range.max - recording.parameters.xAxis.range.min) / 2, dlog_view::getXAxisUnit(recording));
    } else if (operation == DATA_OPERATION_SET) {
        float cursorOffset = value.getFloat() / recording.parameters.period - dlog_view::getPosition(recording);
        if (cursorOffset < 0.0f) {
            dlog_view::changeXAxisOffset(recording, recording.xAxisOffset + cursorOffset * recording.parameters.period);
            recording.cursorOffset = 0;
        } else if (cursorOffset > dlog_view::VIEW_WIDTH - 1) {
            dlog_view::changeXAxisOffset(recording, recording.xAxisOffset + (cursorOffset - (dlog_view::VIEW_WIDTH - 1)) * recording.parameters.period);
            recording.cursorOffset = dlog_view::VIEW_WIDTH - 1;
        } else {
            recording.cursorOffset = (uint32_t)roundf(cursorOffset);
        }
    } else if (operation == DATA_OPERATION_GET_ENCODER_STEP) {
        value = Value(recording.parameters.xAxis.step, dlog_view::getXAxisUnit(recording));
    } else if (operation == DATA_OPERATION_GET_ENCODER_STEP_VALUES) {
        static const int COUNT = 4;
        static float values[COUNT];
        values[0] = recording.parameters.xAxis.step * 1000;
        values[1] = recording.parameters.xAxis.step * 100;
        values[2] = recording.parameters.xAxis.step * 10;
        values[3] = recording.parameters.xAxis.step;
        StepValues *stepValues = value.getStepValues();
        stepValues->values = values;
        stepValues->count = COUNT;
        stepValues->unit = dlog_view::getXAxisUnit(recording);
        value = 1;
    }
}

void data_dlog_multiple_values_overlay(DataOperationEnum operation, Cursor cursor, Value &value) {
    static const int NUM_WIDGETS = 2;

    static const int LABELS_CONTAINER_WIDGET = 0;
    static const int DLOG_VALUES_LIST_WIDGET = 1;

    static Overlay overlay;
    static WidgetOverride widgetOverrides[NUM_WIDGETS];

    if (operation == DATA_OPERATION_GET_OVERLAY_DATA) {
        value = Value(&overlay, VALUE_TYPE_POINTER);
    } else if (operation == DATA_OPERATION_UPDATE_OVERLAY_DATA) {
        auto &recording = dlog_view::getRecording();
        
        int state = 0;
        int numVisibleDlogValues = dlog_view::getNumVisibleDlogValues(recording);
        
        if (dlog_view::g_showLegend && dlog_view::isMulipleValuesOverlayHeuristic(recording)) {
            state = numVisibleDlogValues;
        }

        if (overlay.state != state) {
            overlay.state = state;

            if (state != 0) {
                overlay.widgetOverrides = widgetOverrides;

                WidgetCursor &widgetCursor = *(WidgetCursor *)value.getVoidPointer();

                const ContainerWidget *containerWidget = GET_WIDGET_PROPERTY(widgetCursor.widget, specific, const ContainerWidget *);

                const Widget *labelsContainerWidget = GET_WIDGET_LIST_ELEMENT(containerWidget->widgets, LABELS_CONTAINER_WIDGET);
                const Widget *dlogValuesListWidget = GET_WIDGET_LIST_ELEMENT(containerWidget->widgets, DLOG_VALUES_LIST_WIDGET);

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

        value = Value(&overlay, VALUE_TYPE_POINTER);
    }
}

void data_dlog_single_value_overlay(DataOperationEnum operation, Cursor cursor, Value &value) {
    static Overlay overlay;

    if (operation == DATA_OPERATION_GET_OVERLAY_DATA) {
        value = Value(&overlay, VALUE_TYPE_POINTER);
    } else if (operation == DATA_OPERATION_UPDATE_OVERLAY_DATA) {
        auto &recording = dlog_view::getRecording();
        overlay.state = dlog_view::g_showLegend && !dlog_view::isMulipleValuesOverlayHeuristic(recording);
        
        WidgetCursor &widgetCursor = *(WidgetCursor *)value.getVoidPointer();
        overlay.width = widgetCursor.widget->w;
        overlay.height = widgetCursor.widget->h;
        
        value = Value(&overlay, VALUE_TYPE_POINTER);
    }
}

void data_dlog_visible_values(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_COUNT) {
        auto &recording = dlog_view::getRecording();
        value = getNumVisibleDlogValues(recording);
    }
}

void data_dlog_visible_value_label(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        auto &recording = dlog_view::getRecording();
        int dlogValueIndex = dlog_view::getDlogValueIndex(recording, !dlog_view::isMulipleValuesOverlayHeuristic(recording) ? recording.selectedVisibleValueIndex : cursor);
        value = Value(dlogValueIndex, VALUE_TYPE_DLOG_VALUE_LABEL);
    }
}

void guessStepValues(StepValues *stepValues, Unit unit) {
    if (unit == UNIT_VOLT) {
        static float values[] = { 2.0f, 1.0f, 0.5f, 0.1f };
        stepValues->values = values;
        stepValues->count = sizeof(values) / sizeof(float);
    } else if (unit == UNIT_AMPER) {
        static float values[] = { 0.25f, 0.1f, 0.05f, 0.01f };
        stepValues->values = values;
        stepValues->count = sizeof(values) / sizeof(float);
    } else if (unit == UNIT_WATT) {
        static float values[] = { 5.0f, 2.0f, 1.0f, 0.5f };
        stepValues->values = values;
        stepValues->count = sizeof(values) / sizeof(float);
    } else if (unit == UNIT_SECOND) {
        static float values[] = { 20.0f, 10.0f, 5.0f, 1.0f };
        stepValues->values = values;
        stepValues->count = sizeof(values) / sizeof(float);
    } else {
        static float values[] = { 10.0f, 1.0f, 0.1f, 0.01f };
        stepValues->values = values;
        stepValues->count = sizeof(values) / sizeof(float);
    }
    stepValues->unit = unit;
}

void data_dlog_visible_value_div(DataOperationEnum operation, Cursor cursor, Value &value) {
    auto &recording = dlog_view::getRecording();
    int dlogValueIndex = dlog_view::getDlogValueIndex(recording, !dlog_view::isMulipleValuesOverlayHeuristic(recording) ? recording.selectedVisibleValueIndex : cursor);

    if (operation == DATA_OPERATION_GET) {
        bool focused = g_focusCursor == cursor && g_focusDataId == DATA_ID_DLOG_VISIBLE_VALUE_DIV;
        if (focused && g_focusEditValue.getType() != VALUE_TYPE_NONE) {
            value = g_focusEditValue;
        } else {
            value = Value(dlog_view::roundValue(recording.dlogValues[dlogValueIndex].div), getYAxisUnit(recording, dlogValueIndex));
        }
    } else if (operation == DATA_OPERATION_GET_EDIT_VALUE) {
        bool focused = g_focusCursor == cursor && g_focusDataId == DATA_ID_DLOG_VISIBLE_VALUE_DIV;
        if (focused && g_focusEditValue.getType() != VALUE_TYPE_NONE) {
            value = g_focusEditValue;
        } else {
            value = Value(recording.dlogValues[dlogValueIndex].div, getYAxisUnit(recording, dlogValueIndex));
        }
    } else if (operation == DATA_OPERATION_GET_MIN) {
        value = Value(0.001f, getYAxisUnit(recording, dlogValueIndex));
    } else if (operation == DATA_OPERATION_GET_MAX) {
        value = Value(100.0f, getYAxisUnit(recording, dlogValueIndex));
    } else if (operation == DATA_OPERATION_SET) {
        recording.dlogValues[dlogValueIndex].offset = recording.dlogValues[dlogValueIndex].offset * value.getFloat() / recording.dlogValues[dlogValueIndex].div;
        recording.dlogValues[dlogValueIndex].div = value.getFloat();
    } else if (operation == DATA_OPERATION_GET_NAME) {
        value = "Div";
    } else if (operation == DATA_OPERATION_GET_UNIT) {
        value = getYAxisUnit(recording, dlogValueIndex);
    } else if (operation == DATA_OPERATION_GET_IS_CHANNEL_DATA) {
        value = 0;
    } else if (operation == DATA_OPERATION_GET_ENCODER_STEP_VALUES) {
        guessStepValues(value.getStepValues(), getYAxisUnit(recording, dlogValueIndex));
        value = 1;
    }
}

void data_dlog_visible_value_offset(DataOperationEnum operation, Cursor cursor, Value &value) {
    auto &recording = dlog_view::getRecording();
    int dlogValueIndex = dlog_view::getDlogValueIndex(recording, !dlog_view::isMulipleValuesOverlayHeuristic(recording) ? recording.selectedVisibleValueIndex : cursor);

    if (operation == DATA_OPERATION_GET) {
        bool focused = g_focusCursor == cursor && g_focusDataId == DATA_ID_DLOG_VISIBLE_VALUE_OFFSET;
        if (focused && g_focusEditValue.getType() != VALUE_TYPE_NONE) {
            value = g_focusEditValue;
        } else {
            value = Value(dlog_view::roundValue(recording.dlogValues[dlogValueIndex].offset), getYAxisUnit(recording, dlogValueIndex));
        }
    } else if (operation == DATA_OPERATION_GET_EDIT_VALUE) {
        bool focused = g_focusCursor == cursor && g_focusDataId == DATA_ID_DLOG_VISIBLE_VALUE_OFFSET;
        if (focused && g_focusEditValue.getType() != VALUE_TYPE_NONE) {
            value = g_focusEditValue;
        } else {
            value = Value(recording.dlogValues[dlogValueIndex].offset, getYAxisUnit(recording, dlogValueIndex));
        }
    } else if (operation == DATA_OPERATION_GET_MIN) {
        value = Value(-100.0f, getYAxisUnit(recording, dlogValueIndex));
    } else if (operation == DATA_OPERATION_GET_MAX) {
        value = Value(100.0f, getYAxisUnit(recording, dlogValueIndex));
    } else if (operation == DATA_OPERATION_SET) {
        recording.dlogValues[dlogValueIndex].offset = value.getFloat();
    } else if (operation == DATA_OPERATION_GET_NAME) {
        value = "Offset";
    } else if (operation == DATA_OPERATION_GET_UNIT) {
        value = getYAxisUnit(recording, dlogValueIndex);
    } else if (operation == DATA_OPERATION_GET_IS_CHANNEL_DATA) {
        value = 0;
    } else if (operation == DATA_OPERATION_GET_ENCODER_STEP_VALUES) {
        guessStepValues(value.getStepValues(), getYAxisUnit(recording, dlogValueIndex));
        value = 1;
    }
}

void data_dlog_x_axis_offset(DataOperationEnum operation, Cursor cursor, Value &value) {
    auto &recording = dlog_view::getRecording();

    if (operation == DATA_OPERATION_GET) {
        bool focused = g_focusCursor == cursor && g_focusDataId == DATA_ID_DLOG_X_AXIS_OFFSET;
        if (focused && g_focusEditValue.getType() != VALUE_TYPE_NONE) {
            value = g_focusEditValue;
        } else {
            value = Value(recording.xAxisOffset, dlog_view::getXAxisUnit(recording));
        }
    } else if (operation == DATA_OPERATION_GET_MIN) {
        value = Value(recording.parameters.xAxis.range.min, dlog_view::getXAxisUnit(recording));
    } else if (operation == DATA_OPERATION_GET_MAX) {
        value = Value(recording.parameters.xAxis.range.max - recording.pageSize * recording.parameters.period, dlog_view::getXAxisUnit(recording));
    } else if (operation == DATA_OPERATION_SET) {
        dlog_view::changeXAxisOffset(recording, value.getFloat());
    } else if (operation == DATA_OPERATION_GET_NAME) {
        value = "Offset";
    } else if (operation == DATA_OPERATION_GET_UNIT) {
        value = dlog_view::getXAxisUnit(recording);
    } else if (operation == DATA_OPERATION_GET_IS_CHANNEL_DATA) {
        value = 0;
    } else if (operation == DATA_OPERATION_GET_ENCODER_STEP_VALUES) {
        guessStepValues(value.getStepValues(), dlog_view::getXAxisUnit(recording));
        value = 1;
    }
}

void data_dlog_x_axis_div(DataOperationEnum operation, Cursor cursor, Value &value) {
    auto &recording = dlog_view::getRecording();

    if (operation == DATA_OPERATION_GET) {
        bool focused = g_focusCursor == cursor && g_focusDataId == DATA_ID_DLOG_X_AXIS_DIV;
        if (focused && g_focusEditValue.getType() != VALUE_TYPE_NONE) {
            value = g_focusEditValue;
        } else {
            value = Value(recording.xAxisDiv, dlog_view::getXAxisUnit(recording));
        }
    } else if (operation == DATA_OPERATION_GET_MIN || operation == DATA_OPERATION_GET_DEF) {
        value = Value(recording.xAxisDivMin, dlog_view::getXAxisUnit(recording));
    } else if (operation == DATA_OPERATION_GET_MAX) {
        value = Value(recording.xAxisDivMax, dlog_view::getXAxisUnit(recording));
    } else if (operation == DATA_OPERATION_SET) {
        dlog_view::changeXAxisDiv(recording, value.getFloat());
    } else if (operation == DATA_OPERATION_GET_NAME) {
        value = "Div";
    } else if (operation == DATA_OPERATION_GET_UNIT) {
        value = dlog_view::getXAxisUnit(recording);
    } else if (operation == DATA_OPERATION_GET_IS_CHANNEL_DATA) {
        value = 0;
    } else if (operation == DATA_OPERATION_GET_ENCODER_STEP_VALUES) {
        guessStepValues(value.getStepValues(), dlog_view::getXAxisUnit(recording));
        value = 1;
    }
}

void data_dlog_x_axis_max_value(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        auto &recording = dlog_view::getRecording();

        float maxValue = dlog_view::getDuration(recording);

        if (recording.parameters.xAxis.scale == dlog_view::SCALE_LOGARITHMIC) {
            maxValue = powf(10, recording.parameters.xAxis.range.min + maxValue);
        }

        value = Value(maxValue, recording.parameters.xAxis.unit);
    }
}

void data_dlog_x_axis_max_value_label(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        auto &recording = dlog_view::getRecording();
        if (recording.parameters.xAxis.unit == UNIT_SECOND) {
            value = "Total duration";
        } else {
            value = "Max.";
        }
    }
}

void data_dlog_visible_value_cursor(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        auto &recording = dlog_view::getRecording();
        int dlogValueIndex = dlog_view::getDlogValueIndex(recording, !dlog_view::isMulipleValuesOverlayHeuristic(recording) ? recording.selectedVisibleValueIndex : cursor);
        auto ytDataGetValue = ytDataGetGetValueFunc(cursor, DATA_ID_RECORDING);
        float max;
        float min = ytDataGetValue(ytDataGetPosition(cursor, DATA_ID_RECORDING) + recording.cursorOffset, dlogValueIndex, &max);
        float cursorValue = (min + max) / 2;
        if (recording.parameters.yAxisScale == dlog_view::SCALE_LOGARITHMIC) {
            float logOffset = 1 - recording.parameters.yAxes[dlogValueIndex].range.min;
            cursorValue = powf(10, cursorValue) - logOffset;
        }
        value = Value(cursorValue, recording.parameters.yAxes[dlogValueIndex].unit);

    }
}

void data_dlog_current_time(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = Value(0, VALUE_TYPE_DLOG_CURRENT_TIME);
    }
}

void data_dlog_file_length(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = Value(dlog_record::g_fileLength, VALUE_TYPE_FILE_LENGTH);
    }
}

void data_dlog_all_values(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_COUNT) {
        value = MAX_NUM_OF_Y_VALUES;
    }
}

void data_dlog_value_label(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = Value(cursor, VALUE_TYPE_DLOG_VALUE_LABEL);
    }
}

void data_dlog_value_state(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        auto &recording = dlog_view::getRecording();
        value = cursor < recording.parameters.numYAxes ? recording.dlogValues[cursor].isVisible : 2;
    }
}

void data_dlog_view_show_legend(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = dlog_view::g_showLegend ? 1 : 0;
    }
}

void data_dlog_view_show_labels(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = dlog_view::g_showLabels ? 1 : 0;
    }
}

void data_script_is_started(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = mp::isIdle() ? 0 : 1;
    }
}

void data_script_info(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        // get script file name
        const char *p = mp::g_scriptPath + strlen(mp::g_scriptPath) - 1;
        while (p >= mp::g_scriptPath && *p != '/' && *p != '\\') {
            p--;
        }
        value = p + 1;
    }
}

void data_mqtt_enabled(DataOperationEnum operation, Cursor cursor, Value &value) {
#if OPTION_ETHERNET
    if (operation == DATA_OPERATION_GET) {
        SysSettingsMqttPage *page = (SysSettingsMqttPage *)getPage(PAGE_ID_SYS_SETTINGS_MQTT);
        if (page) {
            value = page->m_enabled ? 1 : 0;
        } else {
            value = persist_conf::devConf.mqttEnabled ? 1 : 0;
        }
    }
#endif
}

void data_mqtt_connection_state(DataOperationEnum operation, Cursor cursor, Value &value) {
#if OPTION_ETHERNET
    if (operation == DATA_OPERATION_GET) {
        value = mqtt::g_connectionState + 1;
    }
#endif
}

void data_mqtt_host(DataOperationEnum operation, Cursor cursor, Value &value) {
#if OPTION_ETHERNET
    if (operation == DATA_OPERATION_GET) {
        SysSettingsMqttPage *page = (SysSettingsMqttPage *)getPage(PAGE_ID_SYS_SETTINGS_MQTT);
        if (page) {
            value = page->m_host;
        } else {
            value = persist_conf::devConf.mqttHost;
        }
    } else if (operation == DATA_OPERATION_SET) {
        SysSettingsMqttPage *page = (SysSettingsMqttPage *)getPage(PAGE_ID_SYS_SETTINGS_MQTT);
        if (page) {
            strcpy(page->m_host, value.getString());
        }
    } else if (operation == DATA_OPERATION_GET_MAX) {
        value = Value(64, VALUE_TYPE_UINT32);
    }
#endif
}

void data_mqtt_port(DataOperationEnum operation, Cursor cursor, Value &value) {
#if OPTION_ETHERNET
    if (operation == DATA_OPERATION_GET) {
        SysSettingsMqttPage *page = (SysSettingsMqttPage *)getPage(PAGE_ID_SYS_SETTINGS_MQTT);
        if (page) {
            value = Value(page->m_port, VALUE_TYPE_UINT16);
        } else {
            value = Value(persist_conf::devConf.mqttPort, VALUE_TYPE_UINT16);
        }
    } else if (operation == DATA_OPERATION_GET_UNIT) {
        value = UNIT_SECOND;
    } else if (operation == DATA_OPERATION_GET_MIN) {
        value = Value(0, VALUE_TYPE_UINT16);
    } else if (operation == DATA_OPERATION_GET_MAX) {
        value = Value(65535, VALUE_TYPE_UINT16);
    } else if (operation == DATA_OPERATION_GET_DEF) {
        value = Value(1883, VALUE_TYPE_UINT16);
    } else if (operation == DATA_OPERATION_SET) {
        SysSettingsMqttPage *page = (SysSettingsMqttPage *)getPage(PAGE_ID_SYS_SETTINGS_MQTT);
        if (page) {
            page->m_port = value.getUInt16();
        }
    }

#endif
}

void data_mqtt_username(DataOperationEnum operation, Cursor cursor, Value &value) {
#if OPTION_ETHERNET
    if (operation == DATA_OPERATION_GET) {
        SysSettingsMqttPage *page = (SysSettingsMqttPage *)getPage(PAGE_ID_SYS_SETTINGS_MQTT);
        if (page) {
            value = page->m_username;
        } else {
            value = persist_conf::devConf.mqttUsername;
        }
    } else if (operation == DATA_OPERATION_SET) {
        SysSettingsMqttPage *page = (SysSettingsMqttPage *)getPage(PAGE_ID_SYS_SETTINGS_MQTT);
        if (page) {
            strcpy(page->m_username, value.getString());
        }
    } else if (operation == DATA_OPERATION_GET_MAX) {
        value = Value(32, VALUE_TYPE_UINT32);
    }
#endif
}

void data_mqtt_password(DataOperationEnum operation, Cursor cursor, Value &value) {
#if OPTION_ETHERNET
    if (operation == DATA_OPERATION_GET) {
        SysSettingsMqttPage *page = (SysSettingsMqttPage *)getPage(PAGE_ID_SYS_SETTINGS_MQTT);
        if (page) {
            value = Value(page->m_password, VALUE_TYPE_PASSWORD);
        } else {
            value = Value(persist_conf::devConf.mqttPassword, VALUE_TYPE_PASSWORD);
        }
    } else if (operation == DATA_OPERATION_SET) {
        SysSettingsMqttPage *page = (SysSettingsMqttPage *)getPage(PAGE_ID_SYS_SETTINGS_MQTT);
        if (page) {
            strcpy(page->m_password, value.getString());
        }
    } else if (operation == DATA_OPERATION_GET_MAX) {
        value = Value(32, VALUE_TYPE_UINT32);
    }
#endif
}

void data_mqtt_period(DataOperationEnum operation, Cursor cursor, Value &value) {
#if OPTION_ETHERNET
    if (operation == DATA_OPERATION_GET) {
        SysSettingsMqttPage *page = (SysSettingsMqttPage *)getPage(PAGE_ID_SYS_SETTINGS_MQTT);
        if (page) {
            value = Value(page->m_period, UNIT_SECOND);
        } else {
            value = Value(persist_conf::devConf.mqttPeriod, UNIT_SECOND);
        }
    } else if (operation == DATA_OPERATION_GET_UNIT) {
        value = UNIT_SECOND;
    } else if (operation == DATA_OPERATION_GET_MIN) {
        value = MakeValue(mqtt::PERIOD_MIN, UNIT_SECOND);
    } else if (operation == DATA_OPERATION_GET_MAX) {
        value = MakeValue(mqtt::PERIOD_MAX, UNIT_SECOND);
    } else if (operation == DATA_OPERATION_SET) {
        SysSettingsMqttPage *page = (SysSettingsMqttPage *)getPage(PAGE_ID_SYS_SETTINGS_MQTT);
        if (page) {
            page->m_period = value.getFloat();
        }
    }
#endif
}

void data_progress(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = g_progress;
    }
}

void data_selected_theme(DataOperationEnum operation, Cursor cursor, Value &value) {
	if (operation == DATA_OPERATION_GET) {
		value = getThemeName(psu::persist_conf::devConf.selectedThemeIndex);
	}
}

void data_animations_duration(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = Value(psu::persist_conf::devConf.animationsDuration, UNIT_SECOND);
    }
}

void data_master_info(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = Value(0, VALUE_TYPE_MASTER_INFO);
    }
}

void data_master_test_result(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = Value((int)g_masterTestResult, VALUE_TYPE_TEST_RESULT);
    }
}

void data_slot1_info(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = Value(0, VALUE_TYPE_SLOT_INFO);
    }
}

void data_slot1_test_result (DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = Value((int)psu::Channel::getBySlotIndex(0).getTestResult(), VALUE_TYPE_TEST_RESULT);
    }
}

void data_slot2_info(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = Value(1, VALUE_TYPE_SLOT_INFO);
    }
}

void data_slot2_test_result(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = Value((int)psu::Channel::getBySlotIndex(1).getTestResult(), VALUE_TYPE_TEST_RESULT);
    }
}

void data_slot3_info(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = Value(2, VALUE_TYPE_SLOT_INFO);
    }
}

void data_slot3_test_result(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = Value((int)psu::Channel::getBySlotIndex(2).getTestResult(), VALUE_TYPE_TEST_RESULT);
    }
}

void data_is_reset_by_iwdg(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        #if defined(EEZ_PLATFORM_STM32)
            value = g_isResetByIWDG ? 1 : 0;
        #endif
    }
}

void data_can_show_previous_page(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = getPreviousPageId() != PAGE_ID_NONE;
    }
}

void data_async_progress(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        auto x = ((millis() / 40) * 40 - g_psuAppContext.getAsyncInProgressStartTime()) % 1000;
        if (x < 500) {
            x = x * 75 / 500;
        } else {
            x = (1000 - x) * 75 / 500;
        }
        value = MakeRangeValue(x, x + 25);
    }
}

void data_alert_message_is_set(DataOperationEnum operation, Cursor cursor,
                        Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = g_alertMessage.getString() != nullptr;
    }
}

void data_alert_message(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = psu::gui::g_alertMessage;
    } else if (operation == DATA_OPERATION_SET) {
        g_alertMessage = value;
    }
}

void data_alert_message_2(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = g_alertMessage2;
    } else if (operation == DATA_OPERATION_SET) {
        g_alertMessage2 = value;
    }
}

void data_alert_message_3(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = g_alertMessage3;
    } else if (operation == DATA_OPERATION_SET) {
        g_alertMessage3 = value;
    }
}

void data_ramp_and_delay_list(DataOperationEnum operation, Cursor cursor, Value &value) {
    auto page = (SysSettingsRampAndDelayPage *)getPage(PAGE_ID_SYS_SETTINGS_RAMP_AND_DELAY);
    if (!page) {
        return;
    }

    static const int PAGE_SIZE = 4;

    if (operation == DATA_OPERATION_COUNT) {
        value = CH_NUM;
    }  else if (operation == DATA_OPERATION_YT_DATA_GET_SIZE) {
        value = Value(CH_NUM, VALUE_TYPE_UINT32);
    } else if (operation == DATA_OPERATION_YT_DATA_GET_POSITION) {
        value = Value(page->startChannel, VALUE_TYPE_UINT32);
    } else if (operation == DATA_OPERATION_YT_DATA_SET_POSITION) {
        int32_t newPosition = value.getUInt32();
        if (newPosition < 0) {
            page->startChannel = 0;
        } else if (newPosition + PAGE_SIZE > CH_NUM) {
            page->startChannel = CH_NUM - PAGE_SIZE;
        } else {
            page->startChannel = newPosition;
        }
    } else if (operation == DATA_OPERATION_YT_DATA_GET_POSITION_INCREMENT) {
        value = Value(1, VALUE_TYPE_UINT32);
    } else if (operation == DATA_OPERATION_YT_DATA_GET_PAGE_SIZE) {
        value = Value(PAGE_SIZE, VALUE_TYPE_UINT32);
    } else if (operation == DATA_OPERATION_SELECT) {
        value = Value(g_channel, VALUE_TYPE_POINTER);
        selectChannel(&Channel::get(page->startChannel + cursor));
    } else if (operation == DATA_OPERATION_DESELECT) {
        selectChannel((Channel * )value.getVoidPointer());
    } else if (operation == DATA_OPERATION_GET) {
        value = page->getRefreshState();
    } else if (operation == DATA_OPERATION_GET_CANVAS_DRAW_FUNCTION) {
        value = Value((void *)SysSettingsRampAndDelayPage::draw, VALUE_TYPE_POINTER);
    }
}

void data_ramp_and_delay_list_scrollbar_enabled(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = (uint32_t)count(DATA_ID_RAMP_AND_DELAY_LIST) > ytDataGetPageSize(cursor, DATA_ID_RAMP_AND_DELAY_LIST);
    }
}

void data_channel_ramp_state(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        auto page = (SysSettingsRampAndDelayPage *)getPage(PAGE_ID_SYS_SETTINGS_RAMP_AND_DELAY);
        if (page) {
            value = page->rampState[cursor];
        }
    }
}

void getRampAndDelayDurationStepValues(Value &value) {
    auto stepValues = value.getStepValues();

    static float values[] = { 1.0f, 0.1f, 0.01f, 0.001f };
    stepValues->values = values;
    stepValues->count = sizeof(values) / sizeof(float);
    stepValues->unit = UNIT_SECOND;

    value = 1;
}

void data_channel_voltage_ramp_duration(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET || operation == DATA_OPERATION_GET_EDIT_VALUE) {
        auto page = (SysSettingsRampAndDelayPage *)getPage(PAGE_ID_SYS_SETTINGS_RAMP_AND_DELAY);
        if (page) {
            value = MakeValue(page->voltageRampDuration[cursor], UNIT_SECOND);
        } else {
            auto page = (ChSettingsTriggerPage *)getPage(PAGE_ID_CH_SETTINGS_TRIGGER);
            if (page) {
                value = MakeValue(page->voltageRampDuration, UNIT_SECOND);
            } else {
                value = MakeValue(g_channel->u.rampDuration, UNIT_SECOND);
            }
        }
    } else if (operation == DATA_OPERATION_SET) {
        auto page = (SysSettingsRampAndDelayPage *)getPage(PAGE_ID_SYS_SETTINGS_RAMP_AND_DELAY);
        if (page) {
            page->setVoltageRampDuration(cursor, value.getFloat());
        } else {
            auto page = (ChSettingsTriggerPage *)getPage(PAGE_ID_CH_SETTINGS_TRIGGER);
            if (page) {
                page->voltageRampDuration = value.getFloat();
            } else {
                channel_dispatcher::setVoltageRampDuration(*g_channel, value.getFloat());
            }
        }
    } if (operation == DATA_OPERATION_GET_NAME) {
        value = "U ramp dur.";
    } else if (operation == DATA_OPERATION_GET_ALLOW_ZERO) {
        value = 1;
    } else if (operation == DATA_OPERATION_GET_MIN) {
        value = MakeValue(RAMP_DURATION_MIN_VALUE, UNIT_SECOND);
    } else if (operation == DATA_OPERATION_GET_MAX) {
        value = MakeValue(RAMP_DURATION_MAX_VALUE, UNIT_SECOND);
    } else if (operation == DATA_OPERATION_GET_LIMIT) {
        value = MakeValue(RAMP_DURATION_MAX_VALUE, UNIT_SECOND);
    } else if (operation == DATA_OPERATION_GET_UNIT) {
        value = UNIT_SECOND;
    } else if (operation == DATA_OPERATION_GET_IS_CHANNEL_DATA) {
        value = 1;
    } else if (operation == DATA_OPERATION_GET_ENCODER_STEP_VALUES) {
        getRampAndDelayDurationStepValues(value);
    }    
}

void data_channel_current_ramp_duration(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET || operation == DATA_OPERATION_GET_EDIT_VALUE) {
        auto page = (SysSettingsRampAndDelayPage *)getPage(PAGE_ID_SYS_SETTINGS_RAMP_AND_DELAY);
        if (page) {
            value = MakeValue(page->currentRampDuration[cursor], UNIT_SECOND);
        } else {
            auto page = (ChSettingsTriggerPage *)getPage(PAGE_ID_CH_SETTINGS_TRIGGER);
            if (page) {
                value = MakeValue(page->currentRampDuration, UNIT_SECOND);
            } else {
                value = MakeValue(g_channel->i.rampDuration, UNIT_SECOND);
            }
        }
    } else if (operation == DATA_OPERATION_SET) {
        auto page = (SysSettingsRampAndDelayPage *)getPage(PAGE_ID_SYS_SETTINGS_RAMP_AND_DELAY);
        if (page) {
            page->setCurrentRampDuration(cursor, value.getFloat());
        } else {
            auto page = (ChSettingsTriggerPage *)getPage(PAGE_ID_CH_SETTINGS_TRIGGER);
            if (page) {
                page->currentRampDuration = value.getFloat();
            } else {
                channel_dispatcher::setCurrentRampDuration(*g_channel, value.getFloat());
            }
        }
    } if (operation == DATA_OPERATION_GET_NAME) {
        value = "I ramp dur.";
    } else if (operation == DATA_OPERATION_GET_ALLOW_ZERO) {
        value = 1;
    } else if (operation == DATA_OPERATION_GET_MIN) {
        value = MakeValue(RAMP_DURATION_MIN_VALUE, UNIT_SECOND);
    } else if (operation == DATA_OPERATION_GET_MAX) {
        value = MakeValue(RAMP_DURATION_MAX_VALUE, UNIT_SECOND);
    } else if (operation == DATA_OPERATION_GET_LIMIT) {
        value = MakeValue(RAMP_DURATION_MAX_VALUE, UNIT_SECOND);
    } else if (operation == DATA_OPERATION_GET_UNIT) {
        value = UNIT_SECOND;
    } else if (operation == DATA_OPERATION_GET_IS_CHANNEL_DATA) {
        value = 1;
    } else if (operation == DATA_OPERATION_GET_ENCODER_STEP_VALUES) {
        getRampAndDelayDurationStepValues(value);
    }    
}

void data_channel_output_delay(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET || operation == DATA_OPERATION_GET_EDIT_VALUE) {
        auto page = (SysSettingsRampAndDelayPage *)getPage(PAGE_ID_SYS_SETTINGS_RAMP_AND_DELAY);
        if (page) {
            value = MakeValue(page->outputDelayDuration[cursor], UNIT_SECOND);
        } else {
            auto page = (ChSettingsTriggerPage *)getPage(PAGE_ID_CH_SETTINGS_TRIGGER);
            if (page) {
                value = MakeValue(page->outputDelayDuration, UNIT_SECOND);
            } else {
                value = MakeValue(g_channel->outputDelayDuration, UNIT_SECOND);
            }
        }
    } else if (operation == DATA_OPERATION_SET) {
        auto page = (SysSettingsRampAndDelayPage *)getPage(PAGE_ID_SYS_SETTINGS_RAMP_AND_DELAY);
        if (page) {
            page->setOutputDelayDuration(cursor, value.getFloat());
        } else {
            auto page = (ChSettingsTriggerPage *)getPage(PAGE_ID_CH_SETTINGS_TRIGGER);
            if (page) {
                page->outputDelayDuration = value.getFloat();
            } else {
                channel_dispatcher::setOutputDelayDuration(*g_channel, value.getFloat());
            }
        }
    } if (operation == DATA_OPERATION_GET_NAME) {
        value = "Out. delay";
    } else if (operation == DATA_OPERATION_GET_ALLOW_ZERO) {
        value = 1;
    } else if (operation == DATA_OPERATION_GET_MIN) {
        value = MakeValue(RAMP_DURATION_MIN_VALUE, UNIT_SECOND);
    } else if (operation == DATA_OPERATION_GET_MAX) {
        value = MakeValue(RAMP_DURATION_MAX_VALUE, UNIT_SECOND);
    } else if (operation == DATA_OPERATION_GET_LIMIT) {
        value = MakeValue(RAMP_DURATION_MAX_VALUE, UNIT_SECOND);
    } else if (operation == DATA_OPERATION_GET_UNIT) {
        value = UNIT_SECOND;
    } else if (operation == DATA_OPERATION_GET_IS_CHANNEL_DATA) {
        value = 1;
    } else if (operation == DATA_OPERATION_GET_ENCODER_STEP_VALUES) {
        getRampAndDelayDurationStepValues(value);
    }    
}

void data_debug_variables(DataOperationEnum operation, Cursor cursor, Value &value) {
    static const uint32_t PAGE_SIZE = 20;
    static uint32_t g_position = 0;

    using namespace psu::debug;

    if (operation == DATA_OPERATION_COUNT) {
        value = (int)getNumVariables();
    }  else if (operation == DATA_OPERATION_YT_DATA_GET_SIZE) {
        value = Value(getNumVariables(), VALUE_TYPE_UINT32);
    } else if (operation == DATA_OPERATION_YT_DATA_GET_POSITION) {
        value = Value(g_position, VALUE_TYPE_UINT32);
    } else if (operation == DATA_OPERATION_YT_DATA_SET_POSITION) {
        int32_t newPosition = value.getUInt32();
        if (newPosition < 0) {
            g_position = 0;
        } else if (newPosition + PAGE_SIZE > getNumVariables()) {
            g_position = getNumVariables() - PAGE_SIZE;
        } else {
            g_position = newPosition;
        }
    } else if (operation == DATA_OPERATION_YT_DATA_GET_POSITION_INCREMENT) {
        value = Value(2, VALUE_TYPE_UINT32);
    } else if (operation == DATA_OPERATION_YT_DATA_GET_PAGE_SIZE) {
        value = Value(PAGE_SIZE, VALUE_TYPE_UINT32);
    }
}

void data_debug_variable_name(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = psu::debug::getVariableName(cursor);
    }
}

void data_debug_variable_value(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = Value(cursor, VALUE_TYPE_DEBUG_VARIABLE);
    } else if (operation == DATA_OPERATION_GET_TEXT_REFRESH_RATE) {
        value = Value(psu::debug::getVariableRefreshRateMs(cursor), VALUE_TYPE_UINT32);
    }
}


} // namespace gui
} // namespace eez

#endif
