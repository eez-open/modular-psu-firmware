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

#include <assert.h>
#include <math.h>
#include <stdio.h>

#if defined(EEZ_PLATFORM_STM32)
#include <main.h>
#include <iwdg.h>
#endif

#include <eez/conf.h>
#include <bb3/system.h>
#include <eez/util.h>
#include <bb3/index.h>
#include <bb3/file_type.h>
#include <bb3/scripting/scripting.h>
#include <eez/memory.h>
#include <bb3/hmi.h>
#include <bb3/uart.h>
#include <bb3/usb.h>
#include <bb3/function_generator.h>

#include <bb3/fs_driver.h>

#include <eez/gui/gui.h>
#include <eez/gui/widgets/container.h>
#include <eez/gui/widgets/yt_graph.h>

#if OPTION_FAN
#include <bb3/aux_ps/fan.h>
#endif

#include <bb3/bp3c/flash_slave.h>

#include <bb3/psu/psu.h>
#include <bb3/psu/calibration.h>
#include <bb3/psu/channel_dispatcher.h>
#include <bb3/psu/dlog_record.h>
#include <bb3/psu/dlog_view.h>
#if OPTION_ENCODER
#include <bb3/mcu/encoder.h>
#endif
#include <bb3/mcu/battery.h>
#if OPTION_ETHERNET
#include <bb3/psu/ethernet_scpi.h>
#include <bb3/mqtt.h>
#endif
#include <bb3/psu/event_queue.h>
#include <bb3/psu/list_program.h>
#include <bb3/psu/ramp.h>
#include <bb3/psu/serial_psu.h>
#include <bb3/psu/temperature.h>
#include <bb3/psu/trigger.h>
#include <bb3/psu/ontime.h>
#include <bb3/psu/sd_card.h>
#include <bb3/psu/ntp.h>

#include <bb3/psu/gui/psu.h>
#include <bb3/psu/gui/edit_mode.h>
#include <bb3/psu/gui/file_manager.h>
#include <bb3/psu/gui/keypad.h>
#include <bb3/psu/gui/page_ch_settings.h>
#include <bb3/psu/gui/page_sys_settings.h>
#include <bb3/psu/gui/page_user_profiles.h>
#include <bb3/psu/gui/channel_calibration.h>

using namespace eez::psu;
using namespace eez::psu::gui;

#if defined(EEZ_PLATFORM_STM32)
extern uint32_t g_RCC_CSR;
#endif

static const char *RPROG_LABEL = "RP";

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

EnumItem g_enumDefinition_CHANNEL_DISPLAY_VALUE_SCALE[] = {
    { DISPLAY_VALUE_SCALE_MAXIMUM, "Maximum" },
    { DISPLAY_VALUE_SCALE_LIMIT, "Limit" },
    { DISPLAY_VALUE_SCALE_AUTO, "Auto" },
    { DISPLAY_VALUE_SCALE_CUSTOM, "Custom" },
    { 0, 0 }
};

EnumItem g_enumDefinition_CHANNEL_TRIGGER_MODE[] = {
    { TRIGGER_MODE_FIXED, "Fixed" },
    { TRIGGER_MODE_LIST, "List" },
    { TRIGGER_MODE_STEP, "Step" },
    { TRIGGER_MODE_FUNCTION_GENERATOR, "Function generator" },
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
    { CURRENT_RANGE_HIGH + 2, "High*" },
    { CURRENT_RANGE_LOW + 2, "Low*" },
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

EnumItem g_enumDefinition_IO_PINS_INPUT1_FUNCTION[] = {
    { io_pins::FUNCTION_NONE, "None" },
    { io_pins::FUNCTION_INPUT, "Input" },
    { io_pins::FUNCTION_INHIBIT, "Inhibit" },
    { io_pins::FUNCTION_SYSTRIG, "System trigger", "SysTrig" },
    { io_pins::FUNCTION_UART, "UART RX" },
    { 0, 0 }
};

EnumItem g_enumDefinition_IO_PINS_INPUT1_FUNCTION_WITH_DLOG_TRIGGER[] = {
    { io_pins::FUNCTION_NONE, "None" },
    { io_pins::FUNCTION_INPUT, "Input" },
    { io_pins::FUNCTION_INHIBIT, "Inhibit" },
    { io_pins::FUNCTION_SYSTRIG, "System trigger", "SysTrig" },
    { io_pins::FUNCTION_UART, "UART RX" },
    { io_pins::FUNCTION_DLOGTRIG, "DLOG trigger", "DlogTrig" },
    { 0, 0 }
};

EnumItem g_enumDefinition_IO_PINS_INPUT2_FUNCTION[] = {
    { io_pins::FUNCTION_NONE, "None" },
    { io_pins::FUNCTION_INPUT, "Input" },
    { io_pins::FUNCTION_INHIBIT, "Inhibit" },
    { io_pins::FUNCTION_SYSTRIG, "System trigger", "SysTrig" },
    { 0, 0 }
};

EnumItem g_enumDefinition_IO_PINS_INPUT2_FUNCTION_WITH_DLOG_TRIGGER[] = {
    { io_pins::FUNCTION_NONE, "None" },
    { io_pins::FUNCTION_INPUT, "Input" },
    { io_pins::FUNCTION_INHIBIT, "Inhibit" },
    { io_pins::FUNCTION_SYSTRIG, "System trigger", "SysTrig" },
    { io_pins::FUNCTION_DLOGTRIG, "DLOG trigger", "DlogTrig" },
    { 0, 0 }
};

EnumItem g_enumDefinition_IO_PINS_OUTPUT1_FUNCTION[] = {
    { io_pins::FUNCTION_NONE, "None" },
    { io_pins::FUNCTION_OUTPUT, "Output" },
    { io_pins::FUNCTION_FAULT, "Fault" },
    { io_pins::FUNCTION_ON_COUPLE, "Channel ON couple", "ONcoup" },
    { io_pins::FUNCTION_TOUTPUT, "Trigger output", "Toutput" },
    { io_pins::FUNCTION_UART, "UART TX" },
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
    { MODULE_TYPE_DCM220, "DCM220" },
    { MODULE_TYPE_DCM224, "DCM224" },
    { MODULE_TYPE_DIB_MIO168, "MIO168" },
    { MODULE_TYPE_DIB_PREL6, "PREL6" },
    { MODULE_TYPE_DIB_SMX46, "SMX46" },
    { MODULE_TYPE_DIB_MUX14D, "MUX14D" },
    { 0, 0 }
};

EnumItem g_enumDefinition_DLOG_VIEW_LEGEND_VIEW_OPTION[] = {
    { persist_conf::DLOG_VIEW_LEGEND_VIEW_OPTION_HIDDEN, "Hidden" },
    { persist_conf::DLOG_VIEW_LEGEND_VIEW_OPTION_FLOAT, "Float" },
    { persist_conf::DLOG_VIEW_LEGEND_VIEW_OPTION_DOCK, "Dock" },
    { 0, 0 }
};

EnumItem g_enumDefinition_CALIBRATION_VALUE_TYPE_DUAL_RANGE[] = {
    { CALIBRATION_VALUE_U, "Voltage" },
    { CALIBRATION_VALUE_I_LOW_RANGE, "Current [0 - 50 mA]" },
    { CALIBRATION_VALUE_I_HI_RANGE, "Current [0 - 5 A]" },
    { 0, 0 }
};

EnumItem g_enumDefinition_CALIBRATION_VALUE_TYPE[] = {
    { CALIBRATION_VALUE_U, "Voltage" },
    { CALIBRATION_VALUE_I_HI_RANGE, "Current" },
    { 0, 0 }
};

EnumItem g_enumDefinition_USB_MODE[] = {
    { USB_MODE_DISABLED, "Disabled" },
    { USB_MODE_DEVICE, "Device" },
    { USB_MODE_HOST, "Host" },
    { USB_MODE_OTG, "OTG" },
    { 0, 0 }
};

EnumItem g_enumDefinition_USB_DEVICE_CLASS[] = {
    { USB_DEVICE_CLASS_VIRTUAL_COM_PORT, "Virtual COM Port" },
    { USB_DEVICE_CLASS_MASS_STORAGE_CLIENT, "Mass Storage Device" },
    { 0, 0 }
};

EnumItem g_enumDefinition_UART_MODE[] = {
	{ uart::UART_MODE_BUFFER, "Buffer" },
	{ uart::UART_MODE_SCPI, "SCPI" },
	{ uart::UART_MODE_BOOKMARK, "Bookmark" },
	{ 0, 0 }
};

EnumItem g_enumDefinition_UART_BAUD_RATE[] = {
	{ 0, "9600" },
	{ 1, "14400" },
	{ 2, "19200" },
    { 3, "38400" },
    { 4, "57600" },
    { 5, "115200" },
	{ 0, 0 }
};

EnumItem g_enumDefinition_UART_DATA_BITS[] = {
	{ 6, "6" },
	{ 7, "7" },
	{ 8, "8" },
	{ 0, 0 }
};

EnumItem g_enumDefinition_UART_STOP_BITS[] = {
	{ 1, "1" },
	{ 2, "2" },
	{ 0, 0 }
};

EnumItem g_enumDefinition_UART_PARITY[] = {
	{ 0, "None" },
	{ 1, "Even" },
	{ 2, "Odd" },
	{ 0, 0 }
};

////////////////////////////////////////////////////////////////////////////////

Value MakeValue(float value, Unit unit) {
    return Value(value, unit);
}

Value MakeValue(float value, Unit unit, uint16_t options) {
    return Value(value, unit, options);
}

Value MakeStepValuesValue(const StepValues *stepValues) {
    Value value;
    value.type = VALUE_TYPE_STEP_VALUES;
    value.options = 0;
    value.unit = UNIT_UNKNOWN;
    value.pVoidValue = (void *)stepValues;
    return value;
}

Value MakeFloatListValue(float *pFloat) {
    Value value;
    value.type = VALUE_TYPE_FLOAT_LIST;
    value.options = 0;
    value.unit = UNIT_UNKNOWN;
    value.pFloatValue = pFloat;
    return value;
}

Value MakeLessThenMinMessageValue(float float_, const Value &value_) {
    Value value;
    if (value_.getType() == VALUE_TYPE_INT32) {
        value.int32Value = int(float_);
        value.type = VALUE_TYPE_LESS_THEN_MIN_INT;
    } else if (value_.getType() == VALUE_TYPE_TIME_ZONE) {
        value.type = VALUE_TYPE_LESS_THEN_MIN_TIME_ZONE;
    } else {
        value.floatValue = float_;
        value.unit = value_.getUnit();
        value.type = VALUE_TYPE_LESS_THEN_MIN_FLOAT;
    }
    return value;
}

Value MakeGreaterThenMaxMessageValue(float float_, const Value &value_) {
    Value value;
    if (value_.getType() == VALUE_TYPE_INT32) {
        value.int32Value = int(float_);
        value.type = VALUE_TYPE_GREATER_THEN_MAX_INT;
    } else if (value_.getType() == VALUE_TYPE_TIME_ZONE) {
        value.type = VALUE_TYPE_GREATER_THEN_MAX_TIME_ZONE;
    } else {
        value.floatValue = float_;
        value.unit = value_.getUnit();
        value.type = VALUE_TYPE_GREATER_THEN_MAX_FLOAT;
    }
    return value;
}

Value MakeMacAddressValue(uint8_t *macAddress) {
    Value value;
    value.type = VALUE_TYPE_MAC_ADDRESS;
    value.puint8Value = macAddress;
    return value;
}

Value MakeFirmwareVersionValue(uint8_t majorVersion, uint8_t minorVersion) {
    Value value;
    value.type = VALUE_TYPE_FIRMWARE_VERSION;
    value.uint16Value = (majorVersion << 8) | minorVersion;
    return value;
}

Value MakeScpiErrorValue(int16_t errorCode) {
    Value value;
    value.pairOfInt16Value.first = errorCode;
    value.pairOfInt16Value.second = g_errorChannelIndex;
    g_errorChannelIndex = -1;
    value.type = VALUE_TYPE_SCPI_ERROR;
    return value;
}

Value MakeEventMessageValue(int16_t eventId, int channelIndex) {
    Value value;
    value.pairOfInt16Value.first = eventId;
    value.pairOfInt16Value.second = channelIndex;
    value.type = VALUE_TYPE_EVENT_MESSAGE;
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
            snprintf(text, count, "%ud %uh", d, h);
        } else if (m > 0) {
            snprintf(text, count, "%ud %um", d, m);
        } else {
            snprintf(text, count, "%ud %ds", d, (unsigned int)floor(s));
        }
    } else if (h > 0) {
        if (m > 0) {
            snprintf(text, count, "%uh %um", h, m);
        } else {
            snprintf(text, count, "%uh %ds", h, (unsigned int)floor(s));
        }
    } else if (m > 0) {
        snprintf(text, count, "%um %us", m, (unsigned int)floor(s));
    } else {
        snprintf(text, count, "%gs", s);
    }
}

void printTime(uint32_t time, char *text, int count) {
    printTime((double)time, text, count);
}

////////////////////////////////////////////////////////////////////////////////

bool compare_PASSWORD_value(const Value &a, const Value &b) {
    return strlen(a.getString()) == strlen(b.getString());
}

void PASSWORD_value_to_text(const Value &value, char *text, int count) {
    size_t i;
    size_t end = MIN(strlen(value.getString()), (size_t)count);
    for (i = 0; i < end; i++) {
        text[i] = '*';
    }
    text[i] = 0;
}

const char *PASSWORD_value_type_name(const Value &value) {
    return "internal";
}

bool compare_PERCENTAGE_value(const Value &a, const Value &b) {
    return a.getInt() == b.getInt();
}

void PERCENTAGE_value_to_text(const Value &value, char *text, int count) {
    snprintf(text, count - 1, "%d%%", value.getInt());
    text[count - 1] = 0;
}

const char *PERCENTAGE_value_type_name(const Value &value) {
    return "internal";
}

bool compare_SIZE_value(const Value &a, const Value &b) {
    return a.getUInt32() == b.getUInt32();
}

void SIZE_value_to_text(const Value &value, char *text, int count) {
    snprintf(text, count - 1, "%u", (unsigned int)value.getUInt32());
    text[count - 1] = 0;
}

const char *SIZE_value_type_name(const Value &value) {
    return "internal";
}

bool compare_TIME_SECONDS_value(const Value &a, const Value &b) {
    return a.getUInt32() == b.getUInt32();
}

void TIME_SECONDS_value_to_text(const Value &value, char *text, int count) {
    uint32_t totalSeconds = (uint32_t)value.getFloat();
    uint32_t hours = totalSeconds / 3600;
    uint32_t totalMinutes = totalSeconds - hours * 3600;
    uint32_t minutes = totalMinutes / 60;
    uint32_t seconds = totalMinutes % 60;
    snprintf(text, count - 1, "%02d:%02d:%02d", (int)hours, (int)minutes, (int)seconds);
    text[count - 1] = 0;
}

const char *TIME_SECONDS_value_type_name(const Value &value) {
    return "internal";
}

bool compare_LESS_THEN_MIN_FLOAT_value(const Value &a, const Value &b) {
    return a.getUnit() == b.getUnit() && a.getFloat() == b.getFloat();
}

void LESS_THEN_MIN_FLOAT_value_to_text(const Value &value, char *text, int count) {
    char valueText[64];
    MakeValue(value.getFloat(), (Unit)value.getUnit()).toText(valueText, sizeof(valueText));
    snprintf(text, count, "Value is less then %s", valueText);
}

const char *LESS_THEN_MIN_FLOAT_value_type_name(const Value &value) {
    return "internal";
}

bool compare_GREATER_THEN_MAX_FLOAT_value(const Value &a, const Value &b) {
    return a.getUnit() == b.getUnit() && a.getFloat() == b.getFloat();
}

void GREATER_THEN_MAX_FLOAT_value_to_text(const Value &value, char *text, int count) {
    char valueText[64];
    MakeValue(value.getFloat(), (Unit)value.getUnit()).toText(valueText, sizeof(valueText));
    snprintf(text, count, "Value is greater then %s", valueText);
}

const char *GREATER_THEN_MAX_FLOAT_value_type_name(const Value &value) {
    return "internal";
}

bool compare_CHANNEL_LABEL_value(const Value &a, const Value &b) {
    return a.getUInt8() == b.getUInt8();
}

void CHANNEL_LABEL_value_to_text(const Value &value, char *text, int count) {
    snprintf(text, count, "Channel %d:", value.getUInt8());
}

const char *CHANNEL_LABEL_value_type_name(const Value &value) {
    return "internal";
}

bool compare_CHANNEL_SHORT_LABEL_value(const Value &a, const Value &b) {
    return a.getUInt8() == b.getUInt8();
}

void CHANNEL_SHORT_LABEL_value_to_text(const Value &value, char *text, int count) {
    if (value.getUInt8() < CH_NUM) {
        snprintf(text, count, "Ch%d:", value.getUInt8() + 1);
    } else {
        text[0] = 0;
    }
}

const char *CHANNEL_SHORT_LABEL_value_type_name(const Value &value) {
    return "internal";
}

bool compare_CHANNEL_SHORT_LABEL_WITHOUT_COLUMN_value(const Value &a, const Value &b) {
    return a.getUInt8() == b.getUInt8();
}

void CHANNEL_SHORT_LABEL_WITHOUT_COLUMN_value_to_text(const Value &value, char *text, int count) {
    snprintf(text, count, "Ch. %d", value.getUInt8() + 1);
}

const char *CHANNEL_SHORT_LABEL_WITHOUT_COLUMN_value_type_name(const Value &value) {
    return "internal";
}

bool compare_CHANNEL_BOARD_INFO_LABEL_value(const Value &a, const Value &b) {
    return a.getInt() == b.getInt();
}

void CHANNEL_BOARD_INFO_LABEL_value_to_text(const Value &value, char *text, int count) {
    snprintf(text, count, "CH%d board:", value.getInt() + 1);
}

const char *CHANNEL_BOARD_INFO_LABEL_value_type_name(const Value &value) {
    return "internal";
}

bool compare_LESS_THEN_MIN_INT_value(const Value &a, const Value &b) {
    return a.getInt() == b.getInt();
}

void LESS_THEN_MIN_INT_value_to_text(const Value &value, char *text, int count) {
    snprintf(text, count, "Value is less then %d", value.getInt());
}

const char *LESS_THEN_MIN_INT_value_type_name(const Value &value) {
    return "internal";
}

bool compare_LESS_THEN_MIN_TIME_ZONE_value(const Value &a, const Value &b) {
    return true;
}

void LESS_THEN_MIN_TIME_ZONE_value_to_text(const Value &value, char *text, int count) {
    stringCopy(text, count, "Value is less then -12:00");
}

const char *LESS_THEN_MIN_TIME_ZONE_value_type_name(const Value &value) {
    return "internal";
}

bool compare_GREATER_THEN_MAX_INT_value(const Value &a, const Value &b) {
    return a.getInt() == b.getInt();
}

void GREATER_THEN_MAX_INT_value_to_text(const Value &value, char *text, int count) {
    snprintf(text, count, "Value is greater then %d", value.getInt());
}

const char *GREATER_THEN_MAX_INT_value_type_name(const Value &value) {
    return "internal";
}

bool compare_GREATER_THEN_MAX_TIME_ZONE_value(const Value &a, const Value &b) {
    return true;
}

void GREATER_THEN_MAX_TIME_ZONE_value_to_text(const Value &value, char *text, int count) {
    stringCopy(text, count, "Value is greater then +14:00");
}

const char *GREATER_THEN_MAX_TIME_ZONE_value_type_name(const Value &value) {
    return "internal";
}

bool compare_EVENT_value(const Value &a, const Value &b) {
    return compareEventValues(a, b);
}

void EVENT_value_to_text(const Value &value, char *text, int count) {
    eventValueToText(value, text, count);
}

const char *EVENT_value_type_name(const Value &value) {
    return "internal";
}

bool compare_EVENT_MESSAGE_value(const Value &a, const Value &b) {
    return a.getUInt32() == b.getUInt32();
}

void EVENT_MESSAGE_value_to_text(const Value &value, char *text, int count) {
    snprintf(text, count, event_queue::getEventMessage(value.getFirstInt16()), value.getSecondInt16() + 1);
}

const char *EVENT_MESSAGE_value_type_name(const Value &value) {
    return "internal";
}

bool compare_ON_TIME_COUNTER_value(const Value &a, const Value &b) {
    return a.getUInt32() == b.getUInt32();
}

void ON_TIME_COUNTER_value_to_text(const Value &value, char *text, int count) {
    ontime::counterToString(text, count, value.getUInt32());
}

const char *ON_TIME_COUNTER_value_type_name(const Value &value) {
    return "internal";
}

bool compare_COUNTDOWN_value(const Value &a, const Value &b) {
    return a.getUInt32() == b.getUInt32();
}

void COUNTDOWN_value_to_text(const Value &value, char *text, int count) {
    printTime(value.getUInt32(), text, count);
}

const char *COUNTDOWN_value_type_name(const Value &value) {
    return "internal";
}

bool compare_TIME_ZONE_value(const Value &a, const Value &b) {
    return a.getInt16() == b.getInt16();
}

void TIME_ZONE_value_to_text(const Value &value, char *text, int count) {
    formatTimeZone(value.getInt16(), text, count);
}

const char *TIME_ZONE_value_type_name(const Value &value) {
    return "internal";
}

bool compare_DATE_DMY_value(const Value &a, const Value &b) {
    return a.getUInt32() == b.getUInt32();
}

void DATE_DMY_value_to_text(const Value &value, char *text, int count) {
    int year, month, day, hour, minute, second;
    datetime::breakTime(value.getUInt32(), year, month, day, hour, minute, second);
    snprintf(text, count, "%02d - %02d - %d", day, month, year);
}

const char *DATE_DMY_value_type_name(const Value &value) {
    return "internal";
}

bool compare_DATE_MDY_value(const Value &a, const Value &b) {
    return a.getUInt32() == b.getUInt32();
}

void DATE_MDY_value_to_text(const Value &value, char *text, int count) {
    int year, month, day, hour, minute, second;
    datetime::breakTime(value.getUInt32(), year, month, day, hour, minute, second);
    snprintf(text, count, "%02d - %02d - %d", month, day, year);
}

const char *DATE_MDY_value_type_name(const Value &value) {
    return "internal";
}

bool compare_YEAR_value(const Value &a, const Value &b) {
    return a.getUInt16() == b.getUInt16();
}

void YEAR_value_to_text(const Value &value, char *text, int count) {
    snprintf(text, count, "%d", value.getUInt16());
}

const char *YEAR_value_type_name(const Value &value) {
    return "internal";
}

bool compare_MONTH_value(const Value &a, const Value &b) {
    return a.getUInt8() == b.getUInt8();
}

void MONTH_value_to_text(const Value &value, char *text, int count) {
    snprintf(text, count, "%02d", value.getUInt8());
}

const char *MONTH_value_type_name(const Value &value) {
    return "internal";
}

bool compare_DAY_value(const Value &a, const Value &b) {
    return a.getUInt8() == b.getUInt8();
}

void DAY_value_to_text(const Value &value, char *text, int count) {
    snprintf(text, count, "%02d", value.getUInt8());
}

const char *DAY_value_type_name(const Value &value) {
    return "internal";
}

bool compare_TIME_value(const Value &a, const Value &b) {
    return a.getUInt32() == b.getUInt32();
}

void TIME_value_to_text(const Value &value, char *text, int count) {
    int year, month, day, hour, minute, second;
    datetime::breakTime(value.getUInt32(), year, month, day, hour, minute, second);
    snprintf(text, count, "%02d : %02d : %02d", hour, minute, second);
}

const char *TIME_value_type_name(const Value &value) {
    return "internal";
}

bool compare_TIME12_value(const Value &a, const Value &b) {
    return a.getUInt32() == b.getUInt32();
}

void TIME12_value_to_text(const Value &value, char *text, int count) {
    int year, month, day, hour, minute, second;
    datetime::breakTime(value.getUInt32(), year, month, day, hour, minute, second);
    bool am;
    datetime::convertTime24to12(hour, am);
    snprintf(text, count, "%02d : %02d : %02d %s", hour, minute, second, am ? "AM" : "PM");
}

const char *TIME12_value_type_name(const Value &value) {
    return "internal";
}

bool compare_HOUR_value(const Value &a, const Value &b) {
    return a.getUInt8() == b.getUInt8();
}

void HOUR_value_to_text(const Value &value, char *text, int count) {
    snprintf(text, count, "%02d", value.getUInt8());
}

const char *HOUR_value_type_name(const Value &value) {
    return "internal";
}

bool compare_MINUTE_value(const Value &a, const Value &b) {
    return a.getUInt8() == b.getUInt8();
}

void MINUTE_value_to_text(const Value &value, char *text, int count) {
    snprintf(text, count, "%02d", value.getUInt8());
}

const char *MINUTE_value_type_name(const Value &value) {
    return "internal";
}

bool compare_SECOND_value(const Value &a, const Value &b) {
    return a.getUInt8() == b.getUInt8();
}

void SECOND_value_to_text(const Value &value, char *text, int count) {
    snprintf(text, count, "%02d", value.getUInt8());
}

const char *SECOND_value_type_name(const Value &value) {
    return "internal";
}

bool compare_USER_PROFILE_LABEL_value(const Value &a, const Value &b) {
    return a.getInt() == b.getInt();
}

void USER_PROFILE_LABEL_value_to_text(const Value &value, char *text, int count) {
    snprintf(text, count, "[ %d ]", value.getInt());
}

const char *USER_PROFILE_LABEL_value_type_name(const Value &value) {
    return "internal";
}

bool compare_USER_PROFILE_REMARK_value(const Value &a, const Value &b) {
    return a.getInt() == b.getInt();
}

void USER_PROFILE_REMARK_value_to_text(const Value &value, char *text, int count) {
    profile::getName(value.getInt(), text, count);
}

const char *USER_PROFILE_REMARK_value_type_name(const Value &value) {
    return "internal";
}

bool compare_EDIT_INFO_value(const Value &a, const Value &b) {
    return a.getInt() == b.getInt();
}

void EDIT_INFO_value_to_text(const Value &value, char *text, int count) {
    edit_mode::getInfoText(text, count);
}

const char *EDIT_INFO_value_type_name(const Value &value) {
    return "internal";
}

bool compare_MAC_ADDRESS_value(const Value &a, const Value &b) {
    return memcmp(a.getPUint8(), b.getPUint8(), 6) == 0;
}

void MAC_ADDRESS_value_to_text(const Value &value, char *text, int count) {
    macAddressToString(value.getPUint8(), text);
}

const char *MAC_ADDRESS_value_type_name(const Value &value) {
    return "internal";
}

bool compare_IP_ADDRESS_value(const Value &a, const Value &b) {
    return a.getUInt32() == b.getUInt32();
}

void IP_ADDRESS_value_to_text(const Value &value, char *text, int count) {
    ipAddressToString(value.getUInt32(), text, count);
}

const char *IP_ADDRESS_value_type_name(const Value &value) {
    return "internal";
}

bool compare_PORT_value(const Value &a, const Value &b) {
    return a.getUInt16() == b.getUInt16();
}

void PORT_value_to_text(const Value &value, char *text, int count) {
    snprintf(text, count, "%d", value.getUInt16());
}

const char *PORT_value_type_name(const Value &value) {
    return "internal";
}

bool compare_TEXT_MESSAGE_value(const Value &a, const Value &b) {
    return a.getUInt8() == b.getUInt8();
}

void TEXT_MESSAGE_value_to_text(const Value &value, char *text, int count) {
    stringCopy(text, count, getTextMessage());
}

const char *TEXT_MESSAGE_value_type_name(const Value &value) {
    return "internal";
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

const char *STEP_VALUES_value_type_name(const Value &value) {
    return "internal";
}

bool compare_FLOAT_LIST_value(const Value &a, const Value &b) {
    return a.getFloatList() == b.getFloatList();
}

void FLOAT_LIST_value_to_text(const Value &value, char *text, int count) {
}

const char *FLOAT_LIST_value_type_name(const Value &value) {
    return "internal";
}

bool compare_CHANNEL_TITLE_value(const Value &a, const Value &b) {
    Channel &aChannel = Channel::get(a.getInt());
    Channel &bChannel = Channel::get(b.getInt());
    return aChannel.channelIndex == bChannel.channelIndex && a.getOptions() == b.getOptions();
}

void CHANNEL_TITLE_value_to_text(const Value &value, char *text, int count) {
    Channel &channel = Channel::get(value.getInt());
    if (channel.flags.trackingEnabled) {
        snprintf(text, count, "\xA2 %s", channel.getLabelOrDefault());
    } else {
        snprintf(text, count, "%s", channel.getLabelOrDefault());
    }
}

const char *CHANNEL_TITLE_value_type_name(const Value &value) {
    return "internal";
}

bool compare_CHANNEL_SHORT_TITLE_value(const Value &a, const Value &b) {
    Channel &aChannel = Channel::get(a.getInt());
    Channel &bChannel = Channel::get(b.getInt());
    return aChannel.channelIndex == bChannel.channelIndex && a.getOptions() == b.getOptions();
}

void CHANNEL_SHORT_TITLE_value_to_text(const Value &value, char *text, int count) {
    Channel &channel = Channel::get(value.getInt());
    if (channel.flags.trackingEnabled) {
        snprintf(text, count, "\xA2");
    } else {
        snprintf(text, count, "#%d", channel.channelIndex + 1);
    }
}

const char *CHANNEL_SHORT_TITLE_value_type_name(const Value &value) {
    return "internal";
}

bool compare_CHANNEL_SHORT_TITLE_WITHOUT_TRACKING_ICON_value(const Value &a, const Value &b) {
    return a.getInt() == b.getInt();
}

void CHANNEL_SHORT_TITLE_WITHOUT_TRACKING_ICON_value_to_text(const Value &value, char *text, int count) {
    Channel &channel = Channel::get(value.getInt());
    snprintf(text, count, "#%d", channel.channelIndex + 1);
}

const char *CHANNEL_SHORT_TITLE_WITHOUT_TRACKING_ICON_value_type_name(const Value &value) {
    return "internal";
}

bool compare_CHANNEL_LONG_TITLE_value(const Value &a, const Value &b) {
    Channel &aChannel = Channel::get(a.getInt());
    Channel &bChannel = Channel::get(b.getInt());
    return aChannel.channelIndex == bChannel.channelIndex && aChannel.flags.trackingEnabled == bChannel.flags.trackingEnabled;
}

void CHANNEL_LONG_TITLE_value_to_text(const Value &value, char *text, int count) {
    auto &channel = Channel::get(value.getInt());
    auto &slot = *g_slots[channel.slotIndex];
    if (channel.flags.trackingEnabled) {
        snprintf(text, count, "\xA2 %s: %dV/%dA, R%dB%d", channel.getLabelOrDefault(), 
            (int)floor(channel.params.U_MAX), (int)floor(channel.params.I_MAX), 
            (int)(slot.moduleRevision >> 8), (int)(slot.moduleRevision & 0xFF));
    } else {
        snprintf(text, count, "%s: %dV/%dA, R%dB%d", channel.getLabelOrDefault(), 
            (int)floor(channel.params.U_MAX), (int)floor(channel.params.I_MAX), 
            (int)(slot.moduleRevision >> 8), (int)(slot.moduleRevision & 0xFF));
    }
}

const char *CHANNEL_LONG_TITLE_value_type_name(const Value &value) {
    return "internal";
}

bool compare_CHANNEL_ID_value(const Value &a, const Value &b) {
    return a.getUInt32() == b.getUInt32();
}

void CHANNEL_ID_value_to_text(const Value &value, char *text, int count) {
    snprintf(text, count, "%d%02d", (int)value.getFirstUInt16() + 1, (int)value.getSecondUInt16() + 1);
}

const char *CHANNEL_ID_value_type_name(const Value &value) {
    return "internal";
}

bool compare_DLOG_VALUE_LABEL_value(const Value &a, const Value &b) {
    return a.getInt() == b.getInt();
}

void DLOG_VALUE_LABEL_value_to_text(const Value &value, char *text, int count) {
    dlog_view::getLabel(dlog_view::getRecording(), value.getInt(), text, count);
}

const char *DLOG_VALUE_LABEL_value_type_name(const Value &value) {
    return "internal";
}

bool compare_FIRMWARE_VERSION_value(const Value &a, const Value &b) {
    return a.getUInt16() == b.getUInt16();
}

void FIRMWARE_VERSION_value_to_text(const Value &value, char *text, int count) {
    uint8_t majorVersion = value.getUInt16() >> 8;
    uint8_t minorVersion = value.getUInt16() & 0xFF;
    snprintf(text, count, "%d.%d", (int)majorVersion, (int)minorVersion);
}

const char *FIRMWARE_VERSION_value_type_name(const Value &value) {
    return "internal";
}

static double g_savedCurrentTime;

bool compare_DLOG_CURRENT_TIME_value(const Value &a, const Value &b) {
    double currentTime = dlog_record::getCurrentTime();
    bool result = g_savedCurrentTime == currentTime;
    g_savedCurrentTime = currentTime;
    return result;
}

void DLOG_CURRENT_TIME_value_to_text(const Value &value, char *text, int count) {
    printTime(g_savedCurrentTime, text, count);
}

const char *DLOG_CURRENT_TIME_value_type_name(const Value &value) {
    return "internal";
}

bool compare_FILE_LENGTH_value(const Value &a, const Value &b) {
    return a.getUInt32() == b.getUInt32();
}

void FILE_LENGTH_value_to_text(const Value &value, char *text, int count) {
    formatBytes(value.getUInt32(), text, count);
}

const char *FILE_LENGTH_value_type_name(const Value &value) {
    return "internal";
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
            snprintf(text, count, "%02d:%02d:%02d", hour, minute, second);
        } else {
            bool am;
            datetime::convertTime24to12(hour, am);
            snprintf(text, count, "%02d:%02d:%02d %s", hour, minute, second, am ? "AM" : "PM");
        }
    } else {
        if (persist_conf::devConf.dateTimeFormat == datetime::FORMAT_DMY_24 || persist_conf::devConf.dateTimeFormat == datetime::FORMAT_DMY_12) {
            snprintf(text, count, "%02d-%02d-%02d", day, month, year % 100);
        } else {
            snprintf(text, count, "%02d-%02d-%02d", month, day, year % 100);
        }
    }
}

const char *FILE_DATE_TIME_value_type_name(const Value &value) {
    return "internal";
}

bool compare_SLOT_INDEX_value(const Value &a, const Value &b) {
    return a.getInt() == b.getInt();
}

void SLOT_INDEX_value_to_text(const Value &value, char *text, int count) {
    snprintf(text, count, "Slot #%d:", value.getInt() + 1);
}

const char *SLOT_INDEX_value_type_name(const Value &value) {
    return "internal";
}

bool compare_SLOT_INFO_value(const Value &a, const Value &b) {
    return a.getInt() == b.getInt();
}

void SLOT_INFO_value_to_text(const Value &value, char *text, int count) {
    int slotIndex = value.getInt();
    auto &slot = *g_slots[slotIndex];
    if (slot.moduleType != MODULE_TYPE_NONE) {
        snprintf(text, count, "%s R%dB%d", slot.moduleName, (int)(slot.moduleRevision >> 8), (int)(slot.moduleRevision & 0xFF));
    } else {
        stringCopy(text, count, "Not installed");
    }
}

const char *SLOT_INFO_value_type_name(const Value &value) {
    return "internal";
}

bool compare_SLOT_INFO_WITH_FW_VER_value(const Value &a, const Value &b) {
    return a.getInt() == b.getInt();
}

void SLOT_INFO_WITH_FW_VER_value_to_text(const Value &value, char *text, int count) {
    int slotIndex = value.getInt() & 0xFF;
    auto &slot = *g_slots[slotIndex];
    if (slot.moduleType != MODULE_TYPE_NONE) {
        if (slot.firmwareVersionAcquired && (slot.firmwareMajorVersion != 0 || slot.firmwareMinorVersion != 0)) {
            snprintf(text, count, "%s R%dB%d v%d.%d", slot.moduleName, (int)(slot.moduleRevision >> 8), (int)(slot.moduleRevision & 0xFF), slot.firmwareMajorVersion, slot.firmwareMinorVersion);
        } else if (slot.firmareBasedModule && slot.firmwareVersionAcquired) {
            snprintf(text, count, "%s R%dB%d ---", slot.moduleName, (int)(slot.moduleRevision >> 8), (int)(slot.moduleRevision & 0xFF));
        } else {
            snprintf(text, count, "%s R%dB%d", slot.moduleName, (int)(slot.moduleRevision >> 8), (int)(slot.moduleRevision & 0xFF));
        }
    } else {
        stringCopy(text, count, "Not installed");
    }
}

const char *SLOT_INFO_WITH_FW_VER_value_type_name(const Value &value) {
    return "internal";
}

bool compare_SLOT_TITLE_value(const Value &a, const Value &b) {
    return a.getInt() == b.getInt();
}

void SLOT_TITLE_value_to_text(const Value &value, char *text, int count) {
    int slotIndex = value.getInt() & 0xFF;
    auto &slot = *g_slots[slotIndex];
    snprintf(text, count, "%s #%d", slot.getLabelOrDefault(), slotIndex + 1);
}

const char *SLOT_TITLE_value_type_name(const Value &value) {
    return "internal";
}

bool compare_SLOT_TITLE_DEF_value(const Value &a, const Value &b) {
    return a.getInt() == b.getInt();
}

void SLOT_TITLE_DEF_value_to_text(const Value &value, char *text, int count) {
    int slotIndex = value.getInt();
    auto &slot = *g_slots[slotIndex];
    snprintf(text, count, "%s", slot.getLabelOrDefault());
}

const char *SLOT_TITLE_DEF_value_type_name(const Value &value) {
    return "internal";
}

bool compare_SLOT_TITLE_MAX_value(const Value &a, const Value &b) {
    return a.getInt() == b.getInt();
}

void SLOT_TITLE_MAX_value_to_text(const Value &value, char *text, int count) {
    int slotIndex = value.getInt();
    auto &slot = *g_slots[slotIndex];
    snprintf(text, count, "%s #%d", slot.getLabelOrDefault(), slotIndex + 1);
}

const char *SLOT_TITLE_MAX_value_type_name(const Value &value) {
    return "internal";
}

bool compare_SLOT_TITLE_SETTINGS_value(const Value &a, const Value &b) {
    return a.getInt() == b.getInt();
}

void SLOT_TITLE_SETTINGS_value_to_text(const Value &value, char *text, int count) {
    int slotIndex = value.getInt() & 0xFF;
    int channelIndex = value.getInt() >> 8;
    if (channelIndex != -1) {
        Channel &channel = Channel::get(channelIndex);
        snprintf(text, count, "%s:", channel.getLabelOrDefault());
    } else {
        auto &slot = *g_slots[slotIndex];
        snprintf(text, count, "%s #%d:", slot.getLabelOrDefault(), slotIndex + 1);
    }
}

const char *SLOT_TITLE_SETTINGS_value_type_name(const Value &value) {
    return "internal";
}

bool compare_SLOT_SHORT_TITLE_value(const Value &a, const Value &b) {
    return a.getInt() == b.getInt();
}

void SLOT_SHORT_TITLE_value_to_text(const Value &value, char *text, int count) {
    int slotIndex = value.getInt() & 0xFF;
    snprintf(text, count, "#%d", slotIndex + 1);
}

const char *SLOT_SHORT_TITLE_value_type_name(const Value &value) {
    return "internal";
}

bool compare_MASTER_INFO_value(const Value &a, const Value &b) {
    return true;
}

void MASTER_INFO_value_to_text(const Value &value, char *text, int count) {
    snprintf(text, count, "%s R%dB%d", MCU_NAME, g_mcuRevision >> 8, g_mcuRevision & 0xFF);
}

const char *MASTER_INFO_value_type_name(const Value &value) {
    return "internal";
}

bool compare_MASTER_INFO_WITH_FW_VER_value(const Value &a, const Value &b) {
    return true;
}

void MASTER_INFO_WITH_FW_VER_value_to_text(const Value &value, char *text, int count) {
    snprintf(text, count, "%s R%dB%d v%s", MCU_NAME, g_mcuRevision >> 8, g_mcuRevision & 0xFF, MCU_FIRMWARE);
}

const char *MASTER_INFO_WITH_FW_VER_value_type_name(const Value &value) {
    return "internal";
}

bool compare_TEST_RESULT_value(const Value &a, const Value &b) {
    return a.getInt() == b.getInt();
}

void TEST_RESULT_value_to_text(const Value &value, char *text, int count) {
    TestResult testResult = (TestResult)value.getInt();
    if (testResult == TEST_FAILED) {
        stringCopy(text, count, "Failed");
    } else if (testResult == TEST_OK) {
        stringCopy(text, count, "OK");
    } else if (testResult == TEST_CONNECTING) {
        stringCopy(text, count, "Connecting");
    } else if (testResult == TEST_SKIPPED) {
        stringCopy(text, count, "Skipped");
    } else if (testResult == TEST_WARNING) {
        stringCopy(text, count, "Warning");
    } else {
        stringCopy(text, count, "");
    }
}

const char *TEST_RESULT_value_type_name(const Value &value) {
    return "internal";
}

bool compare_SCPI_ERROR_value(const Value &a, const Value &b) {
    return a.getUInt32() == b.getUInt32();
}

void SCPI_ERROR_value_to_text(const Value &value, char *text, int count) {
    const char *scpiErrorMessage = SCPI_ErrorTranslate(value.getFirstInt16());
    if (value.getSecondInt16() != -1) {
        snprintf(text, count, "Ch%d: %s", value.getSecondInt16() + 1, scpiErrorMessage);
    } else {
        snprintf(text, count, "%s", scpiErrorMessage);
    }
}

const char *SCPI_ERROR_value_type_name(const Value &value) {
    return "internal";
}

bool compare_STORAGE_INFO_value(const Value &a, const Value &b) {
    return a.getUInt32() == b.getUInt32();
}

void STORAGE_INFO_value_to_text(const Value &value, char *text, int count) {
    int diskDriveIndex = value.getUInt32() & 0xFFFF;

    uint64_t usedSpace;
    uint64_t freeSpace;
    if (sd_card::getInfo(diskDriveIndex, usedSpace, freeSpace, true)) { // "true" means get storage info from cache
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
        stringCopy(text, count, "Calculating storage info...");
    }
}

const char *STORAGE_INFO_value_type_name(const Value &value) {
    return "internal";
}

bool compare_FOLDER_INFO_value(const Value &a, const Value &b) {
    return a.getUInt32() == b.getUInt32();
}

void FOLDER_INFO_value_to_text(const Value &value, char *text, int count) {
    if (value.getUInt32() == 1) {
        stringCopy(text, count, "1 item");
    } else {
        snprintf(text, count, "%u items ", (unsigned int)value.getUInt32());
    }
}

const char *FOLDER_INFO_value_type_name(const Value &value) {
    return "internal";
}

bool compare_MODULE_SERIAL_INFO_value(const Value &a, const Value &b) {
    return a.getInt() == b.getInt();
}

void MODULE_SERIAL_INFO_value_to_text(const Value &value, char *text, int count) {
    getModuleSerialInfo(value.getInt(), text);
}

const char *MODULE_SERIAL_INFO_value_type_name(const Value &value) {
    return "internal";
}

bool compare_CALIBRATION_VALUE_TYPE_INFO_value(const Value &a, const Value &b) {
    return true;
}

void CALIBRATION_VALUE_TYPE_INFO_value_to_text(const Value &value, char *text, int count) {
    getCalibrationValueTypeInfo(text, count);
}

const char *CALIBRATION_VALUE_TYPE_INFO_value_type_name(const Value &value) {
    return "internal";
}

bool compare_CALIBRATION_POINT_INFO_value(const Value &a, const Value &b) {
    return a.getUInt32() == b.getUInt32();
}

void CALIBRATION_POINT_INFO_value_to_text(const Value &value, char *text, int count) {
    int currentPointIndex = value.getFirstInt16();
    int numPoints = value.getSecondInt16();

    if (currentPointIndex != -1) {
        snprintf(text, count, "%d of %d", currentPointIndex + 1, numPoints);
    } else {
        if (numPoints == 1) {
            stringCopy(text, count, "1 point");
        } else {
            snprintf(text, count, "%d points", numPoints);
        }
    }
}

const char *CALIBRATION_POINT_INFO_value_type_name(const Value &value) {
    return "internal";
}

bool compare_ZOOM_value(const Value &a, const Value &b) {
    return a.getInt() == b.getInt();
}

void ZOOM_value_to_text(const Value &value, char *text, int count) {
    snprintf(text, count, "\xb8 x%d", value.getInt());
}

const char *ZOOM_value_type_name(const Value &value) {
    return "internal";
}

bool compare_NUM_SELECTED_value(const Value &a, const Value &b) {
	return a.getUInt32() == b.getUInt32();
}

void NUM_SELECTED_value_to_text(const Value &value, char *text, int count) {
	snprintf(text, count, "%d of %d selected", (int)value.getFirstUInt16(), (int)value.getSecondUInt16());
}

const char *NUM_SELECTED_value_type_name(const Value &value) {
    return "internal";
}

bool compare_CURRENT_DIRECTORY_TITLE_value(const Value &a, const Value &b) {
    return a.getUInt32() == b.getUInt32();
}

void CURRENT_DIRECTORY_TITLE_value_to_text(const Value &value, char *text, int count) {
	file_manager::getCurrentDirectoryTitle(text, count);
}

const char *CURRENT_DIRECTORY_TITLE_value_type_name(const Value &value) {
    return "internal";
}

bool compare_MASS_STORAGE_DEVICE_LABEL_value(const Value &a, const Value &b) {
    return a.getInt() == b.getInt();
}

void MASS_STORAGE_DEVICE_LABEL_value_to_text(const Value &value, char *text, int count) {
	int massStorageDevice = value.getInt();
	if (massStorageDevice == 0) {
		stringCopy(text, count, "Master (0:)");
	} else {
		int slotIndex = massStorageDevice - 1;
		snprintf(text, count, "%s (%d:)", g_slots[slotIndex]->getLabelOrDefault(), massStorageDevice);
	}
}

const char *MASS_STORAGE_DEVICE_LABEL_value_type_name(const Value &value) {
    return "internal";
}

bool compare_OCP_TRIP_LEVEL_INFO_value(const Value &a, const Value &b) {
    return a.getFloat() == b.getFloat();
}

void OCP_TRIP_LEVEL_INFO_value_to_text(const Value &value, char *text, int count) {
    snprintf(text, count, "Trip level = %g%% of Iset", value.getFloat());
}

const char *OCP_TRIP_LEVEL_INFO_value_type_name(const Value &value) {
    return "internal";
}

bool compare_AUTO_START_SCRIPT_CONFIRMATION_MESSAGE_value(const Value &a, const Value &b) {
	return a.getInt() == b.getInt();
}

void AUTO_START_SCRIPT_CONFIRMATION_MESSAGE_value_to_text(const Value &value, char *text, int count) {
	scripting::getAutoStartConfirmationMessage(text, count);
}

const char *AUTO_START_SCRIPT_CONFIRMATION_MESSAGE_value_type_name(const Value &value) {
    return "internal";
}

static Cursor g_editValueCursor(-1);
static int16_t g_editValueDataId;

void onSetFloatValue(float value) {
    popPage();
	WidgetCursor widgetCursor;
	widgetCursor = g_editValueCursor;
    set(widgetCursor, g_editValueDataId, MakeValue(value, getUnit(widgetCursor, g_editValueDataId)));
}

void onSetInfinityValue() {
    popPage();
	WidgetCursor widgetCursor;
	widgetCursor = g_editValueCursor;
	set(widgetCursor, g_editValueDataId, MakeValue(INFINITY, getUnit(widgetCursor, g_editValueDataId)));
}

void onSetUInt16Value(float value) {
    popPage();
	WidgetCursor widgetCursor;
	widgetCursor = g_editValueCursor;
	set(widgetCursor, g_editValueDataId, Value((uint16_t)value, VALUE_TYPE_UINT16));
}

void onSetStringValue(char *value) {
	WidgetCursor widgetCursor;
	widgetCursor = g_editValueCursor;
	const char *errMessage = isValidValue(widgetCursor, g_editValueDataId, value);
    if (!errMessage) {
        popPage();
        set(widgetCursor, g_editValueDataId, value);
    } else {
        errorMessage(errMessage);
    }
}

void editValue(int16_t dataId) {
    g_editValueDataId = dataId;
	WidgetCursor widgetCursor;
	widgetCursor = g_editValueCursor;
	Value value = get(widgetCursor, g_editValueDataId);

    if (value.getType() == VALUE_TYPE_FLOAT) {
        NumericKeypadOptions options;

        options.editValueUnit = value.getUnit();

        options.min = getMin(widgetCursor, g_editValueDataId).getFloat();
		options.enableMinButton();

        auto max = getMax(widgetCursor, g_editValueDataId);
        if (max.getType() != VALUE_TYPE_UNDEFINED) {
            if (isinf(max.getFloat())) {
                options.flags.option1ButtonEnabled = true;
                options.option1ButtonText = INFINITY_SYMBOL;
                options.option1 = onSetInfinityValue;
            } else {
                options.max = max.getFloat();
                options.enableMaxButton();
            }
        }

        auto def = getDef(widgetCursor, g_editValueDataId);
        if (def.getType() != VALUE_TYPE_UNDEFINED) {
            options.def = def.getFloat();
            options.enableDefButton();
        }

        options.flags.signButtonEnabled = options.min < 0;
        options.flags.dotButtonEnabled = true;
        options.flags.option1ButtonEnabled = true;

        NumericKeypad::start(0, value, options, onSetFloatValue, 0, 0);
    } else if (value.getType() == VALUE_TYPE_UINT16) {
        NumericKeypadOptions options;

        options.min = getMin(widgetCursor, g_editValueDataId).getUInt16();
        options.max = getMax(widgetCursor, g_editValueDataId).getUInt16();
        options.def = getDef(widgetCursor, g_editValueDataId).getUInt16();

        options.enableDefButton();

        NumericKeypad::start(0, (int)value.getUInt16(), options, onSetUInt16Value, 0, 0);
    } else {
        Keypad::startPush(0, value.getString(), 0, getMax(widgetCursor, g_editValueDataId).getUInt32(), value.getType() == VALUE_TYPE_PASSWORD, onSetStringValue, 0);
    }
}

int getSlotView(SlotViewType slotViewType, int slotIndex, Cursor cursor) {
    auto testResult = g_slots[slotIndex]->getTestResult();
    if ((g_slots[slotIndex]->enabled && (testResult == TEST_OK || testResult == TEST_SKIPPED || testResult == TEST_CONNECTING)) || g_slots[slotIndex]->moduleType == MODULE_TYPE_NONE) {
        return g_slots[slotIndex]->getSlotView(slotViewType, slotIndex, cursor);
    } else {
        if (slotViewType == SLOT_VIEW_TYPE_DEFAULT) {
            int isVert = persist_conf::devConf.channelsViewMode == CHANNELS_VIEW_MODE_NUMERIC || persist_conf::devConf.channelsViewMode == CHANNELS_VIEW_MODE_VERT_BAR;        
            return isVert ? PAGE_ID_SLOT_DEF_VERT_ERROR : PAGE_ID_SLOT_DEF_HORZ_ERROR;
        }

        if (slotViewType == SLOT_VIEW_TYPE_DEFAULT_2COL) {
            int isVert = persist_conf::devConf.channelsViewMode == CHANNELS_VIEW_MODE_NUMERIC || persist_conf::devConf.channelsViewMode == CHANNELS_VIEW_MODE_VERT_BAR;        
            return isVert ? PAGE_ID_SLOT_DEF_VERT_ERROR_2COL : PAGE_ID_SLOT_DEF_HORZ_ERROR_2COL;
        }

        assert(slotViewType == SLOT_VIEW_TYPE_MAX);
        return PAGE_ID_SLOT_MAX_ERROR;
    }
}


////////////////////////////////////////////////////////////////////////////////

void data_none(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    value = Value();
}

void data_channels(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    auto cursor = widgetCursor.cursor;
    if (operation == DATA_OPERATION_COUNT) {
        value = CH_NUM;
    } else if (operation == DATA_OPERATION_SELECT) {
        value = Value(g_channel, VALUE_TYPE_POINTER);
        selectChannel(&Channel::get(cursor));
    } else if (operation == DATA_OPERATION_DESELECT) {
        selectChannel((Channel * )value.getVoidPointer());
    }
}

void data_channel_status(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    auto cursor = widgetCursor.cursor;
    if (operation == DATA_OPERATION_GET) {
        int iChannel = cursor >= 0 ? cursor : (g_channel ? g_channel->channelIndex : 0);
        Channel &channel = Channel::get(iChannel);
        value = channel.isOk() ? 1 : 0;
    }
}

void data_channel_output_state(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    auto cursor = widgetCursor.cursor;
    if (operation == DATA_OPERATION_GET) {
        int iChannel = cursor >= 0 ? cursor : (g_channel ? g_channel->channelIndex : 0);
        Channel &channel = Channel::get(iChannel);
        value = channel.isOutputEnabled();
    }
}

void data_channel_is_cc(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    auto cursor = widgetCursor.cursor;
    if (operation == DATA_OPERATION_GET) {
        int iChannel = cursor >= 0 ? cursor : (g_channel ? g_channel->channelIndex : 0);
        Channel &channel = Channel::get(iChannel);
        value = channel.getMode() == CHANNEL_MODE_CC;
    }
}

void data_channel_is_cv(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    auto cursor = widgetCursor.cursor;
    if (operation == DATA_OPERATION_GET) {
        int iChannel = cursor >= 0 ? cursor : (g_channel ? g_channel->channelIndex : -1);
        if (iChannel != -1) {
            Channel &channel = Channel::get(iChannel);
            value = channel.getMode() == CHANNEL_MODE_CV;
        } else {
            value = g_slots[hmi::g_selectedSlotIndex]->isConstantVoltageMode(hmi::g_selectedSubchannelIndex);
        }
    }
}

void data_channel_u_set(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    auto cursor = widgetCursor.cursor;
    int iChannel = cursor >= 0 ? cursor : (g_channel ? g_channel->channelIndex : 0);
    Channel &channel = Channel::get(iChannel);
    if (operation == DATA_OPERATION_GET) {
        if (channel.isRemoteProgrammingEnabled()) {
            value = RPROG_LABEL;
        } else {
            value = MakeValue(channel_dispatcher::getUSet(channel), UNIT_VOLT);
        }
    } else {
        data_channel_u_edit(operation, widgetCursor, value);
    }
}

void data_channel_u_mon(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    auto cursor = widgetCursor.cursor;
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
        if (!dlog_record::isIdle() && dlog_record::g_activeRecording.parameters.isDlogItemEnabled(channel.slotIndex, channel.subchannelIndex, DLOG_RESOURCE_TYPE_U)) {
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

void data_channel_u_mon_dac(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    auto cursor = widgetCursor.cursor;
    int iChannel = cursor >= 0 ? cursor : (g_channel ? g_channel->channelIndex : 0);
    Channel &channel = Channel::get(iChannel);
    if (operation == DATA_OPERATION_GET) {
        value = MakeValue(channel_dispatcher::getUMonDac(channel), UNIT_VOLT);
    } else if (operation == DATA_OPERATION_GET_BACKGROUND_COLOR) {
        if (!dlog_record::isIdle() && dlog_record::g_activeRecording.parameters.isDlogItemEnabled(channel.slotIndex, channel.subchannelIndex, DLOG_RESOURCE_TYPE_U)) {
            value = Value(COLOR_ID_DATA_LOGGING, VALUE_TYPE_UINT16);
        }
    }
}

void data_channel_u_limit(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    auto cursor = widgetCursor.cursor;
    int iChannel = cursor >= 0 ? cursor : (g_channel ? g_channel->channelIndex : 0);
    Channel &channel = Channel::get(iChannel);
    if (operation == DATA_OPERATION_GET) {
        value = MakeValue(channel_dispatcher::getULimit(channel), UNIT_VOLT);
    }
}

void data_channel_u_edit(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    auto cursor = widgetCursor.cursor;
    int iChannel = cursor >= 0 ? cursor : (g_channel ? g_channel->channelIndex : 0);
    Channel &channel = Channel::get(iChannel);
    if (operation == DATA_OPERATION_GET) {
        bool focused = g_focusCursor == widgetCursor && g_focusDataId == DATA_ID_CHANNEL_U_EDIT;
        if (focused && g_focusEditValue.getType() != VALUE_TYPE_UNDEFINED) {
            value = g_focusEditValue;
        } else if (focused && getActivePageId() == PAGE_ID_EDIT_MODE_KEYPAD && edit_mode_keypad::g_keypad->isEditing()) {
            data_keypad_text(operation, widgetCursor, value);
        } else {
            if (channel.isRemoteProgrammingEnabled()) {
                value = RPROG_LABEL;
            } else {
                value = MakeValue(channel_dispatcher::getUSet(channel), UNIT_VOLT);
            }
        }
    } else if (operation == DATA_OPERATION_GET_EDIT_VALUE) {
        value = MakeValue(channel_dispatcher::getUSet(channel), UNIT_VOLT);
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
        channel.getVoltageStepValues(value.getStepValues(), false);
        value = 1;
    } else if (operation == DATA_OPERATION_SET_ENCODER_MODE) {
        channel.setVoltageEncoderMode((EncoderMode)value.getInt());
    } else if (operation == DATA_OPERATION_SET) {
        int err;
        if (!channel.isVoltageWithinRange(value.getFloat())) {
            value = MakeScpiErrorValue(SCPI_ERROR_DATA_OUT_OF_RANGE);
        } else if (channel.isVoltageLimitExceeded(value.getFloat())) {
            g_errorChannelIndex = channel.channelIndex;
            value = MakeScpiErrorValue(SCPI_ERROR_VOLTAGE_LIMIT_EXCEEDED);
        } else if (channel.isPowerLimitExceeded(value.getFloat(), channel_dispatcher::getISet(channel), &err)) {
            g_errorChannelIndex = channel.channelIndex;
            value = MakeScpiErrorValue(err);
        } else {
            channel_dispatcher::setVoltage(channel, value.getFloat());
        }
    }
}

void data_channel_i_set(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    auto cursor = widgetCursor.cursor;
    int iChannel = cursor >= 0 ? cursor : (g_channel ? g_channel->channelIndex : 0);
    Channel &channel = Channel::get(iChannel);
    if (operation == DATA_OPERATION_GET) {
        value = MakeValue(channel_dispatcher::getISet(channel), UNIT_AMPER);
    } else {
        data_channel_i_edit(operation, widgetCursor, value);
    }
}

void data_channel_i_mon(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    auto cursor = widgetCursor.cursor;
    int iChannel = cursor >= 0 ? cursor : (g_channel ? g_channel->channelIndex : 0);
    Channel &channel = Channel::get(iChannel);
    if (operation == DATA_OPERATION_GET) {
        float iMon = channel_dispatcher::getIMon(channel);
        if (iMon < channel.params.I_MON_MIN) {
            value = MakeValue(channel.params.I_MON_MIN, UNIT_AMPER, FLOAT_OPTIONS_LESS_THEN);
        } else {
            value = MakeValue(iMon, UNIT_AMPER);
        }
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
        if (!dlog_record::isIdle() && dlog_record::g_activeRecording.parameters.isDlogItemEnabled(channel.slotIndex, channel.subchannelIndex, DLOG_RESOURCE_TYPE_I)) {
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

void data_channel_i_mon_dac(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    auto cursor = widgetCursor.cursor;
    int iChannel = cursor >= 0 ? cursor : (g_channel ? g_channel->channelIndex : 0);
    Channel &channel = Channel::get(iChannel);
    if (operation == DATA_OPERATION_GET) {
        value = MakeValue(channel_dispatcher::getIMonDac(channel), UNIT_AMPER);
    } else if (operation == DATA_OPERATION_GET_BACKGROUND_COLOR) {
        if (!dlog_record::isIdle() && dlog_record::g_activeRecording.parameters.isDlogItemEnabled(channel.slotIndex, channel.subchannelIndex, DLOG_RESOURCE_TYPE_I)) {
            value = Value(COLOR_ID_DATA_LOGGING, VALUE_TYPE_UINT16);
        }
    } 
}

void data_channel_i_limit(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    auto cursor = widgetCursor.cursor;
    int iChannel = cursor >= 0 ? cursor : (g_channel ? g_channel->channelIndex : 0);
    Channel &channel = Channel::get(iChannel);
    if (operation == DATA_OPERATION_GET) {
        value = MakeValue(channel_dispatcher::getILimit(channel), UNIT_AMPER);
    }
}

void data_channel_i_edit(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    auto cursor = widgetCursor.cursor;
    int iChannel = cursor >= 0 ? cursor : (g_channel ? g_channel->channelIndex : 0);
    Channel &channel = Channel::get(iChannel);
    if (operation == DATA_OPERATION_GET) {
        bool focused = g_focusCursor == widgetCursor && g_focusDataId == DATA_ID_CHANNEL_I_EDIT;
        if (focused && g_focusEditValue.getType() != VALUE_TYPE_UNDEFINED) {
            value = g_focusEditValue;
        } else if (focused && getActivePageId() == PAGE_ID_EDIT_MODE_KEYPAD && edit_mode_keypad::g_keypad->isEditing()) {
            data_keypad_text(operation, widgetCursor, value);
        } else {
            value = MakeValue(channel_dispatcher::getISet(channel), UNIT_AMPER);
        }
    } else if (operation == DATA_OPERATION_GET_EDIT_VALUE) {
        value = MakeValue(channel_dispatcher::getISet(channel), UNIT_AMPER);
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
        channel.getCurrentStepValues(value.getStepValues(), false);
        value = 1;
    } else if (operation == DATA_OPERATION_SET_ENCODER_MODE) {
        channel.setCurrentEncoderMode((EncoderMode)value.getInt());
    } else if (operation == DATA_OPERATION_SET) {
        int err;
        if (!channel.isCurrentWithinRange(value.getFloat())) {
            value = MakeScpiErrorValue(SCPI_ERROR_DATA_OUT_OF_RANGE);
        } else if (channel.isCurrentLimitExceeded(value.getFloat())) {
            g_errorChannelIndex = channel.channelIndex;
            value = MakeScpiErrorValue(SCPI_ERROR_CURRENT_LIMIT_EXCEEDED);
        } else if (channel.isPowerLimitExceeded(channel_dispatcher::getUSet(channel), value.getFloat(), &err)) {
            g_errorChannelIndex = channel.channelIndex;
            value = MakeScpiErrorValue(err);
        } else {
            channel_dispatcher::setCurrent(channel, value.getFloat());
        }
    }
}

void data_channel_p_mon(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    auto cursor = widgetCursor.cursor;
    int iChannel = cursor >= 0 ? cursor : (g_channel ? g_channel->channelIndex : 0);
    Channel &channel = Channel::get(iChannel);
    if (operation == DATA_OPERATION_GET) {
        auto iMon = channel_dispatcher::getIMon(channel);
        if (iMon < channel.params.I_MON_MIN) {
            value = MakeValue(channel_dispatcher::getUMon(channel) * channel.params.I_MON_MIN, UNIT_WATT, FLOAT_OPTIONS_LESS_THEN);
        } else {
            value = MakeValue(channel_dispatcher::getUMon(channel) * iMon, UNIT_WATT);
        }
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
        if (!dlog_record::isIdle() && dlog_record::g_activeRecording.parameters.isDlogItemEnabled(channel.slotIndex, channel.subchannelIndex, DLOG_RESOURCE_TYPE_P)) {
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

void data_channel_other_value_mon(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (g_focusDataId == DATA_ID_CHANNEL_U_EDIT) {
        data_channel_i_mon(operation, widgetCursor, value);
    } else if (g_focusDataId == DATA_ID_CHANNEL_I_EDIT) {
        data_channel_u_mon(operation, widgetCursor, value);
    }
}

void data_slot_view_type(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = persist_conf::isMaxView() ? 1 : 0;
    }
}

void data_channels_is_2col_view(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = g_isCol2Mode;
    }
}

void data_slot_is_2ch_view(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
		int slotIndex = hmi::g_selectedSlotIndex;
        auto testResult = g_slots[slotIndex]->getTestResult();
        if (g_slots[slotIndex]->enabled && (testResult == TEST_OK || testResult == TEST_SKIPPED)) {
            value = g_slots[hmi::g_selectedSlotIndex]->numPowerChannels == 2;
        } else {
            value = 0;
        }
    }
}


void data_channels_view_mode(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = (int)persist_conf::devConf.channelsViewMode;
    }
}

void data_channels_view_mode_in_default(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = (int)persist_conf::devConf.channelsViewMode;
    }
}

void data_channels_view_mode_in_max(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = (int)persist_conf::devConf.channelsViewModeInMax;
    }
}

void data_slot_is_enabled(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = g_slots[hmi::g_selectedSlotIndex]->enabled;
    }
}

void data_slot_is_dcpsupply_module(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = g_slots[hmi::g_selectedSlotIndex]->numPowerChannels > 0;
    }
}

void data_channel_index(Channel &channel, DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_SET_CONTEXT) {
        // save currently selected channel and slot index
        value.pairOfInt16Value.first = g_channelIndex;
        value.pairOfInt16Value.second = hmi::g_selectedSlotIndex;
        value.type = VALUE_TYPE_UINT32;

        selectChannel(&channel);
    } else if (operation == DATA_OPERATION_GET_CONTEXT) {
        value = Value(g_channel, VALUE_TYPE_POINTER);
    } else if (operation == DATA_OPERATION_GET_CONTEXT_CURSOR) {
        value = channel.channelIndex;
    } else if (operation == DATA_OPERATION_RESTORE_CONTEXT) {
        // restore channel and slot index
        auto channelIndex = value.pairOfInt16Value.first;
        auto slotIndex = value.pairOfInt16Value.second;
        selectChannel(channelIndex != -1 ? &Channel::get(channelIndex) : nullptr);
        hmi::selectSlot(slotIndex);
    }
}

void data_no_channel_index(int slotIndex, DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_SET_CONTEXT) {
        // save currently selected channel and slot index
        value.pairOfInt16Value.first = g_channelIndex;
        value.pairOfInt16Value.second = hmi::g_selectedSlotIndex;
        value.type = VALUE_TYPE_UINT32;

        selectChannel(nullptr);
        hmi::g_selectedSlotIndex = slotIndex;
    } else if (operation == DATA_OPERATION_GET_CONTEXT) {
        value = Value(g_channel, VALUE_TYPE_POINTER);
    } else if (operation == DATA_OPERATION_GET_CONTEXT_CURSOR) {
        value = slotIndex;
    } else if (operation == DATA_OPERATION_RESTORE_CONTEXT) {
        // restore channel and slot index
        auto channelIndex = value.pairOfInt16Value.first;
        auto slotIndex = value.pairOfInt16Value.second;
        selectChannel(channelIndex != -1 ? &Channel::get(channelIndex) : nullptr);
        hmi::selectSlot(slotIndex);
    }
}

void data_slot_channel_index(int slotIndex, Channel *channel, DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    auto testResult = g_slots[slotIndex]->getTestResult();
    if (channel && g_slots[slotIndex]->enabled && (testResult == TEST_OK || testResult == TEST_SKIPPED)) {
        data_channel_index(*channel, operation, widgetCursor, value);
    } else {
        data_no_channel_index(slotIndex, operation, widgetCursor, value);
    }
}

void data_slot1_channel_index(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    data_slot_channel_index(g_slotIndexes[0], Channel::getBySlotIndex(g_slotIndexes[0]), operation, widgetCursor, value);
}

void data_slot2_channel_index(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    data_slot_channel_index(g_slotIndexes[1], Channel::getBySlotIndex(g_slotIndexes[1]), operation, widgetCursor, value);
}

void data_slot3_channel_index(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    data_slot_channel_index(g_slotIndexes[2], Channel::getBySlotIndex(g_slotIndexes[2]), operation, widgetCursor, value);
}

void data_slot1_non_mapped_channel_index(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
	data_slot_channel_index(0, Channel::getBySlotIndex(0), operation, widgetCursor, value);
}

void data_slot2_non_mapped_channel_index(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
	data_slot_channel_index(1, Channel::getBySlotIndex(1), operation, widgetCursor, value);
}

void data_slot3_non_mapped_channel_index(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
	data_slot_channel_index(2, Channel::getBySlotIndex(2), operation, widgetCursor, value);
}

void data_slot_max_channel_index(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    int channelIndex = persist_conf::getMaxChannelIndex();
    data_slot_channel_index(persist_conf::getMaxSlotIndex(), channelIndex == -1 ? nullptr : &Channel::get(channelIndex), operation, widgetCursor, value);
}

void data_slot_default_view(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    auto cursor = widgetCursor.cursor;
	if (operation == DATA_OPERATION_GET) {
		value = getSlotView(g_isCol2Mode ? SLOT_VIEW_TYPE_DEFAULT_2COL : SLOT_VIEW_TYPE_DEFAULT, hmi::g_selectedSlotIndex, cursor);
	}
}

void data_slot_max_view(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    auto cursor = widgetCursor.cursor;
    if (operation == DATA_OPERATION_GET) {
        value = getSlotView(SLOT_VIEW_TYPE_MAX, persist_conf::getMaxSlotIndex(), cursor);
    }
}

void data_channel_display_value1(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    auto cursor = widgetCursor.cursor;
    int iChannel = cursor >= 0 ? cursor : (g_channel ? g_channel->channelIndex : 0);
    Channel &channel = Channel::get(iChannel);
    if (operation == DATA_OPERATION_GET_DISPLAY_VALUE_RANGE) {
        value = Value(channel.displayValues[0].getRange(&channel), channel.displayValues[0].getUnit());
    } else {
        if (channel.displayValues[0].type == DISPLAY_VALUE_VOLTAGE) {
            data_channel_u_mon(operation, widgetCursor, value);
        } else if (channel.displayValues[0].type == DISPLAY_VALUE_CURRENT) {
            data_channel_i_mon(operation, widgetCursor, value);
        } else if (channel.displayValues[0].type == DISPLAY_VALUE_POWER) {
            data_channel_p_mon(operation, widgetCursor, value);
        }
    }
}

void data_channel_display_value1_set(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    auto cursor = widgetCursor.cursor;
    if (operation == DATA_OPERATION_GET) {
        int iChannel = cursor >= 0 ? cursor : (g_channel ? g_channel->channelIndex : 0);
        Channel &channel = Channel::get(iChannel);
        if (channel.displayValues[0].type == DISPLAY_VALUE_VOLTAGE) {
            data_channel_u_set(operation, widgetCursor, value);
        } else if (channel.displayValues[0].type == DISPLAY_VALUE_CURRENT) {
            data_channel_i_set(operation, widgetCursor, value);
        } else if (channel.displayValues[0].type == DISPLAY_VALUE_POWER) {
            if (operation == DATA_OPERATION_GET) {
                value = MakeValue(channel_dispatcher::getUSet(channel) * channel_dispatcher::getISet(channel), UNIT_WATT);
            }
        }
    }
}

void data_channel_display_value1_limit(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    auto cursor = widgetCursor.cursor;
    if (operation == DATA_OPERATION_GET) {
        int iChannel = cursor >= 0 ? cursor : (g_channel ? g_channel->channelIndex : 0);
        Channel &channel = Channel::get(iChannel);
        if (channel.displayValues[0].type == DISPLAY_VALUE_VOLTAGE) {
            data_channel_u_limit(operation, widgetCursor, value);
        } else if (channel.displayValues[0].type == DISPLAY_VALUE_CURRENT) {
            data_channel_i_limit(operation, widgetCursor, value);
        } else if (channel.displayValues[0].type == DISPLAY_VALUE_POWER) {
            if (operation == DATA_OPERATION_GET) {
                value = MakeValue(channel_dispatcher::getPowerLimit(channel), UNIT_WATT);
            }
        }
    }
}

void data_channel_display_value2(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    auto cursor = widgetCursor.cursor;
    int iChannel = cursor >= 0 ? cursor : (g_channel ? g_channel->channelIndex : 0);
    Channel &channel = Channel::get(iChannel);
    if (operation == DATA_OPERATION_GET_DISPLAY_VALUE_RANGE) {
        value = Value(channel.displayValues[1].getRange(&channel), channel.displayValues[1].getUnit());
    } else {
        if (channel.displayValues[1].type == DISPLAY_VALUE_VOLTAGE) {
            data_channel_u_mon(operation, widgetCursor, value);
        } else if (channel.displayValues[1].type == DISPLAY_VALUE_CURRENT) {
            data_channel_i_mon(operation, widgetCursor, value);
        } else if (channel.displayValues[1].type == DISPLAY_VALUE_POWER) {
            data_channel_p_mon(operation, widgetCursor, value);
        }
    }
}

void data_channel_display_value2_set(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    auto cursor = widgetCursor.cursor;
    if (operation == DATA_OPERATION_GET) {
        int iChannel = cursor >= 0 ? cursor : (g_channel ? g_channel->channelIndex : 0);
        Channel &channel = Channel::get(iChannel);
        if (channel.displayValues[1].type == DISPLAY_VALUE_VOLTAGE) {
            data_channel_u_set(operation, widgetCursor, value);
        } else if (channel.displayValues[1].type == DISPLAY_VALUE_CURRENT) {
            data_channel_i_set(operation, widgetCursor, value);
        } else if (channel.displayValues[1].type == DISPLAY_VALUE_POWER) {
            if (operation == DATA_OPERATION_GET) {
                value = MakeValue(channel_dispatcher::getUSet(channel) * channel_dispatcher::getISet(channel), UNIT_WATT);
            }
        }
    }
}

void data_channel_display_value2_limit(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    auto cursor = widgetCursor.cursor;
    if (operation == DATA_OPERATION_GET) {
        int iChannel = cursor >= 0 ? cursor : (g_channel ? g_channel->channelIndex : 0);
        Channel &channel = Channel::get(iChannel);
        if (channel.displayValues[1].type == DISPLAY_VALUE_VOLTAGE) {
            data_channel_u_limit(operation, widgetCursor, value);
        } else if (channel.displayValues[1].type == DISPLAY_VALUE_CURRENT) {
            data_channel_i_limit(operation, widgetCursor, value);
        } else if (channel.displayValues[1].type == DISPLAY_VALUE_POWER) {
            if (operation == DATA_OPERATION_GET) {
                value = MakeValue(channel_dispatcher::getPowerLimit(channel), UNIT_WATT);
            }
        }
    }
}

void data_channel_display_value3(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    auto cursor = widgetCursor.cursor;
    int iChannel = cursor >= 0 ? cursor : (g_channel ? g_channel->channelIndex : 0);
    Channel &channel = Channel::get(iChannel);
    if (channel.displayValues[0].type != DISPLAY_VALUE_VOLTAGE && channel.displayValues[1].type != DISPLAY_VALUE_VOLTAGE) {
        data_channel_u_mon(operation, widgetCursor, value);
    } else if (channel.displayValues[0].type != DISPLAY_VALUE_CURRENT && channel.displayValues[1].type != DISPLAY_VALUE_CURRENT) {
        data_channel_i_mon(operation, widgetCursor, value);
    } else {
        data_channel_p_mon(operation, widgetCursor, value);
    }
}

void data_ovp(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    auto cursor = widgetCursor.cursor;
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

void data_ocp(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    auto cursor = widgetCursor.cursor;
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

void data_opp(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    auto cursor = widgetCursor.cursor;
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

void data_otp_ch(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    auto cursor = widgetCursor.cursor;
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

void data_otp_aux(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
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

void data_edit_value(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
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

void data_edit_unit(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        Keypad *keypad = getActiveKeypad();
        if (keypad) {
            value = getUnitName(keypad->getSwitchToUnit());
        }
    }
}

void data_edit_info(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = Value(0, VALUE_TYPE_EDIT_INFO);
    }
}

void data_edit_mode_interactive_mode_selector(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = edit_mode::isInteractiveMode() ? 0 : 1;
    }
}

void data_edit_steps(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    auto cursor = widgetCursor.cursor;
    StepValues stepValues;
    edit_mode_step::getStepValues(stepValues);

    if (operation == DATA_OPERATION_GET) {
        value = edit_mode_step::NUM_STEPS - 1 - (edit_mode_step::getStepIndex() % stepValues.count);
    } else if (operation == DATA_OPERATION_COUNT) {
        value = edit_mode_step::NUM_STEPS;
    } else if (operation == DATA_OPERATION_GET_LABEL) {
        edit_mode_step::getStepValues(stepValues);
        value = Value(stepValues.values[edit_mode_step::NUM_STEPS - 1 - cursor], stepValues.unit);
    } else if (operation == DATA_OPERATION_SET) {
        edit_mode_step::setStepIndex(edit_mode_step::NUM_STEPS - 1 - value.getInt());
    }
}

void data_firmware_info(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        static const char FIRMWARE_LABEL[] = "Firmware: ";
        static char firmware_info[sizeof(FIRMWARE_LABEL) - 1 + sizeof(MCU_FIRMWARE) - 1 + 1];

        if (*firmware_info == 0) {
            stringAppendString(firmware_info, sizeof(firmware_info), FIRMWARE_LABEL);
            stringAppendString(firmware_info, sizeof(firmware_info), MCU_FIRMWARE);
        }

        value = firmware_info;
    }
}

void data_keypad_text(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        Keypad *keypad = getActiveKeypad();
        if (keypad) {
            value = keypad->getKeypadTextValue();
        }
    } else if (operation == DATA_OPERATION_GET_TEXT_CURSOR_POSITION) {
        Keypad *keypad = getActiveKeypad();
        if (keypad) {
            value = keypad->getCursorPostion();
        }
    } else if (operation == DATA_OPERATION_GET_X_SCROLL) {
        Keypad *keypad = getActiveKeypad();
        if (keypad) {
            value = keypad->getXScroll(*(WidgetCursor *)value.getVoidPointer());
        }
    }
}

void data_keypad_mode(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        Keypad *keypad = getActiveKeypad();
        if (keypad) {
            value = keypad->m_keypadMode;
        }
    }
}

void data_keypad_option1_text(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        NumericKeypad *keypad = getActiveNumericKeypad();
        if (keypad) {
            value = Value(keypad->m_options.flags.option1ButtonEnabled ? keypad->m_options.option1ButtonText : "");
        }
    }
}

void data_keypad_option1_enabled(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        NumericKeypad *keypad = getActiveNumericKeypad();
        if (keypad) {
            value = (int)keypad->m_options.flags.option1ButtonEnabled;
        }
    }
}

void data_keypad_option2_text(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        NumericKeypad *keypad = getActiveNumericKeypad();
        if (keypad) {
            value = Value(keypad->m_options.flags.option2ButtonEnabled ? keypad->m_options.option2ButtonText : "");
        }
    }
}

void data_keypad_option2_enabled(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        NumericKeypad *keypad = getActiveNumericKeypad();
        if (keypad) {
            value = (int)keypad->m_options.flags.option2ButtonEnabled;
        }
    }
}

void data_keypad_option3_text(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        NumericKeypad *keypad = getActiveNumericKeypad();
        if (keypad) {
            value = Value(keypad->m_options.flags.option3ButtonEnabled ? keypad->m_options.option3ButtonText : "");
        }
    }
}

void data_keypad_option3_enabled(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        NumericKeypad *keypad = getActiveNumericKeypad();
        if (keypad) {
            value = (int)keypad->m_options.flags.option3ButtonEnabled;
        }
    }
}

void data_keypad_sign_enabled(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        NumericKeypad *keypad = getActiveNumericKeypad();
        if (keypad) {
            value = (int)keypad->m_options.flags.signButtonEnabled;
        }
    }
}

void data_keypad_dot_enabled(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        NumericKeypad *keypad = getActiveNumericKeypad();
        if (keypad) {
            value = (int)keypad->m_options.flags.dotButtonEnabled;
        }
    }
}

void data_keypad_unit_enabled(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
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

void data_keypad_ok_enabled(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        Keypad *keypad = getActiveKeypad();
        if (keypad) {
            value = keypad->isOkEnabled() ? 1 : 0;
        }
    }
}

void data_keypad_can_set_default(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        Keypad *keypad = getActiveKeypad();
        if (keypad) {
            value = keypad->canSetDefault();
        }
    }
}

void data_channel_off_label(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = io_pins::isInhibited() ? "INH" : "OFF";
    }
}

void data_channel_label(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    auto cursor = widgetCursor.cursor;
    if (operation == DATA_OPERATION_GET) {
        int iChannel = cursor >= 0 ? cursor : (g_channel ? g_channel->channelIndex : 0);
        value = Value(iChannel + 1, VALUE_TYPE_CHANNEL_LABEL);
    }
}

void data_channel_short_label(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    auto cursor = widgetCursor.cursor;
    if (operation == DATA_OPERATION_GET) {
        int iChannel = cursor >= 0 ? cursor : (g_channel ? g_channel->channelIndex : -1);
        value = Value(iChannel, VALUE_TYPE_CHANNEL_SHORT_LABEL);
    }
}

void data_channel_title(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    auto cursor = widgetCursor.cursor;
    if (operation == DATA_OPERATION_GET) {
        int iChannel = cursor >= 0 ? cursor : (g_channel ? g_channel->channelIndex : 0);
        value = Value(iChannel, VALUE_TYPE_CHANNEL_TITLE, Channel::get(iChannel).flags.trackingEnabled);
    }
}

void data_channel_short_title(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    auto cursor = widgetCursor.cursor;
    if (operation == DATA_OPERATION_GET) {
        int iChannel = cursor >= 0 ? cursor : (g_channel ? g_channel->channelIndex : 0);
        value = Value(iChannel, VALUE_TYPE_CHANNEL_SHORT_TITLE, Channel::get(iChannel).flags.trackingEnabled);
    }
}

void data_channel_short_title_without_tracking_icon(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    auto cursor = widgetCursor.cursor;
    if (operation == DATA_OPERATION_GET) {
        int iChannel = cursor >= 0 ? cursor : (g_channel ? g_channel->channelIndex : 0);
        value = Value(iChannel, VALUE_TYPE_CHANNEL_SHORT_TITLE_WITHOUT_TRACKING_ICON);
    }
}

void data_channel_long_title(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    auto cursor = widgetCursor.cursor;
    if (operation == DATA_OPERATION_GET) {
        int iChannel = cursor >= 0 ? cursor : (g_channel ? g_channel->channelIndex : 0);
        value = Value(iChannel, VALUE_TYPE_CHANNEL_LONG_TITLE);
    }
}

void data_channel_info_brand(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        auto &slot = *g_slots[hmi::g_selectedSlotIndex];
        value = slot.moduleBrand;
    }
}

void data_slot_serial_info(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = Value(hmi::g_selectedSlotIndex, VALUE_TYPE_MODULE_SERIAL_INFO);
    }
}

void data_channel_info_serial(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    auto cursor = widgetCursor.cursor;
    if (operation == DATA_OPERATION_GET) {
        int iChannel = cursor >= 0 ? cursor : (g_channel ? g_channel->channelIndex : 0);
        value = Value(Channel::get(iChannel).slotIndex, VALUE_TYPE_MODULE_SERIAL_INFO);
    }
}

void data_channel_temp_status(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    auto cursor = widgetCursor.cursor;
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

void data_channel_temp(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    auto cursor = widgetCursor.cursor;
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

void data_channel_on_time_total(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = Value((uint32_t)ontime::g_moduleCounters[hmi::g_selectedSlotIndex].getTotalTime(), VALUE_TYPE_ON_TIME_COUNTER);
    }
}

void data_channel_on_time_last(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = Value((uint32_t)ontime::g_moduleCounters[hmi::g_selectedSlotIndex].getLastTime(), VALUE_TYPE_ON_TIME_COUNTER);
    }
}

void data_channel_protection_status(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    auto cursor = widgetCursor.cursor;
    if (operation == DATA_OPERATION_GET) {
        int iChannel = cursor >= 0 ? cursor : (g_channel ? g_channel->channelIndex : 0);
        Channel &channel = Channel::get(iChannel);

        bool ovp = channel_dispatcher::isOvpTripped(channel);
        bool ocp = channel_dispatcher::isOcpTripped(channel);
        bool opp = channel_dispatcher::isOppTripped(channel);
        bool otp = channel_dispatcher::isOtpTripped(channel);

        value = ovp || ocp || opp || otp;
    }
}

void data_channel_protection_status_ovp(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    auto cursor = widgetCursor.cursor;
    if (operation == DATA_OPERATION_GET) {
        int iChannel = cursor >= 0 ? cursor : (g_channel ? g_channel->channelIndex : 0);
        Channel &channel = Channel::get(iChannel);

        value = channel_dispatcher::isOvpTripped(channel);
    }
}

void data_channel_protection_status_ocp(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    auto cursor = widgetCursor.cursor;
    if (operation == DATA_OPERATION_GET) {
        int iChannel = cursor >= 0 ? cursor : (g_channel ? g_channel->channelIndex : 0);
        Channel &channel = Channel::get(iChannel);

        value = channel_dispatcher::isOcpTripped(channel);
    }
}

void data_channel_protection_status_opp(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    auto cursor = widgetCursor.cursor;
    if (operation == DATA_OPERATION_GET) {
        int iChannel = cursor >= 0 ? cursor : (g_channel ? g_channel->channelIndex : 0);
        Channel &channel = Channel::get(iChannel);

        value = channel_dispatcher::isOppTripped(channel);
    }
}

void data_channel_protection_status_otp(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    auto cursor = widgetCursor.cursor;
    if (operation == DATA_OPERATION_GET) {
        int iChannel = cursor >= 0 ? cursor : (g_channel ? g_channel->channelIndex : 0);
        Channel &channel = Channel::get(iChannel);

        value = channel_dispatcher::isOtpTripped(channel);
    }
}

void data_channel_protection_ovp_state(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        ChSettingsProtectionSetPage *page = (ChSettingsProtectionSetPage *)getPage(PAGE_ID_CH_SETTINGS_PROT_OVP);
        if (page) {
            value = page->state;
        }
    }
}

void data_channel_protection_ovp_type(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    auto cursor = widgetCursor.cursor;
    if (operation == DATA_OPERATION_GET) {
        int iChannel = cursor >= 0 ? cursor : (g_channel ? g_channel->channelIndex : 0);
        Channel &channel = Channel::get(iChannel);
        if ((channel.params.features & CH_FEATURE_HW_OVP) && !channel.flags.rprogEnabled) {
            ChSettingsProtectionSetPage *page = (ChSettingsProtectionSetPage *)getPage(PAGE_ID_CH_SETTINGS_PROT_OVP);
            if (page) {
                value = page->type ? 0 : 1;
            } else {
                if (channel.prot_conf.flags.u_type) {
                    if (channel.prot_conf.flags.u_state && channel.isOutputEnabled() && channel.prot_conf.flags.u_hwOvpDeactivated) {
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

void data_channel_protection_ovp_level(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    auto cursor = widgetCursor.cursor;
    int iChannel = cursor >= 0 ? cursor : (g_channel ? g_channel->channelIndex : 0);
    Channel &channel = Channel::get(iChannel);
    if (operation == DATA_OPERATION_GET) {
        ChSettingsProtectionSetPage *page = (ChSettingsProtectionSetPage *)getPage(PAGE_ID_CH_SETTINGS_PROT_OVP);
        if (page) {
            value = page->level;
        } else {
            bool focused = g_focusCursor == widgetCursor && g_focusDataId == DATA_ID_CHANNEL_PROTECTION_OVP_LEVEL;
            if (focused && g_focusEditValue.getType() != VALUE_TYPE_UNDEFINED) {
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
    static float values[] = { 1.0f, 5.0f, 10.0f, 20.0f };
 
    stepValues->values = values;
    stepValues->count = sizeof(values) / sizeof(float);
    stepValues->unit = UNIT_SECOND;

    stepValues->encoderSettings.accelerationEnabled = true;
    stepValues->encoderSettings.range = 10.0f * stepValues->values[0];
    stepValues->encoderSettings.step = stepValues->values[0];
    stepValues->encoderSettings.mode = edit_mode_step::g_protectionDelayEncoderMode;
}

void data_channel_protection_ovp_delay(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    auto cursor = widgetCursor.cursor;
    int iChannel = cursor >= 0 ? cursor : (g_channel ? g_channel->channelIndex : 0);
    Channel &channel = Channel::get(iChannel);
    if (operation == DATA_OPERATION_GET) {
        ChSettingsProtectionSetPage *page = (ChSettingsProtectionSetPage *)getPage(PAGE_ID_CH_SETTINGS_PROT_OVP);
        if (page) {
            value = page->delay;
        } else {
            bool focused = g_focusCursor == widgetCursor && g_focusDataId == DATA_ID_CHANNEL_PROTECTION_OVP_DELAY;
            if (focused && g_focusEditValue.getType() != VALUE_TYPE_UNDEFINED) {
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
    } else if (operation == DATA_OPERATION_SET_ENCODER_MODE) {
        edit_mode_step::g_protectionDelayEncoderMode = (EncoderMode)value.getInt();
    }
}

void data_channel_protection_ovp_limit(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    auto cursor = widgetCursor.cursor;
    int iChannel = cursor >= 0 ? cursor : (g_channel ? g_channel->channelIndex : 0);
    Channel &channel = Channel::get(iChannel);
    if (operation == DATA_OPERATION_GET) {
        if (channel.isRemoteProgrammingEnabled()) {
            value = RPROG_LABEL;
        } else {
            ChSettingsProtectionSetPage *page = (ChSettingsProtectionSetPage *)getPage(PAGE_ID_CH_SETTINGS_PROT_OVP);
            if (page) {
                value = page->limit;
            } else {
                bool focused = g_focusCursor == widgetCursor && g_focusDataId == DATA_ID_CHANNEL_PROTECTION_OVP_LIMIT;
                if (focused && g_focusEditValue.getType() != VALUE_TYPE_UNDEFINED) {
                    value = g_focusEditValue;
                } else {
                    value = MakeValue(channel_dispatcher::getULimit(channel), UNIT_VOLT);
                }
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
        channel.getVoltageStepValues(value.getStepValues(), false);
        value = 1;
    } else if (operation == DATA_OPERATION_SET_ENCODER_MODE) {
        channel.setVoltageEncoderMode((EncoderMode)value.getInt());
    } 
}

void data_channel_protection_ocp_state(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        ChSettingsProtectionSetPage *page = (ChSettingsProtectionSetPage *)getPage(PAGE_ID_CH_SETTINGS_PROT_OCP);
        if (page) {
            value = page->state;
        }
    }
}

void data_channel_ocp_trip_level_info(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
	if (operation == DATA_OPERATION_GET) {
        value.floatValue = g_channel->params.OCP_TRIP_LEVEL_PERCENT;
        value.type = VALUE_TYPE_OCP_TRIP_LEVEL_INFO;
	}
}

void data_channel_protection_ocp_delay(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    auto cursor = widgetCursor.cursor;
    int iChannel = cursor >= 0 ? cursor : (g_channel ? g_channel->channelIndex : 0);
    Channel &channel = Channel::get(iChannel);
    if (operation == DATA_OPERATION_GET) {
        ChSettingsProtectionSetPage *page = (ChSettingsProtectionSetPage *)getPage(PAGE_ID_CH_SETTINGS_PROT_OCP);
        if (page) {
            value = page->delay;
        } else {
            bool focused = g_focusCursor == widgetCursor && g_focusDataId == DATA_ID_CHANNEL_PROTECTION_OCP_DELAY;
            if (focused && g_focusEditValue.getType() != VALUE_TYPE_UNDEFINED) {
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
    } else if (operation == DATA_OPERATION_SET_ENCODER_MODE) {
        edit_mode_step::g_protectionDelayEncoderMode = (EncoderMode)value.getInt();
    }
}

void data_channel_protection_ocp_limit(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    auto cursor = widgetCursor.cursor;
    int iChannel = cursor >= 0 ? cursor : (g_channel ? g_channel->channelIndex : 0);
    Channel &channel = Channel::get(iChannel);
    if (operation == DATA_OPERATION_GET) {
        ChSettingsProtectionSetPage *page = (ChSettingsProtectionSetPage *)getPage(PAGE_ID_CH_SETTINGS_PROT_OCP);
        if (page) {
            value = page->limit;
        } else {
            bool focused = g_focusCursor == widgetCursor && g_focusDataId == DATA_ID_CHANNEL_PROTECTION_OCP_LIMIT;
            if (focused && g_focusEditValue.getType() != VALUE_TYPE_UNDEFINED) {
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
        channel.getCurrentStepValues(value.getStepValues(), false);
        value = 1;
    } else if (operation == DATA_OPERATION_SET_ENCODER_MODE) {
        channel.setCurrentEncoderMode((EncoderMode)value.getInt());
    } 
}

void data_channel_protection_ocp_max_current_limit_cause(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = Value(g_channel->getMaxCurrentLimitCause());
    }
}

void data_channel_protection_opp_state(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        ChSettingsProtectionSetPage *page = (ChSettingsProtectionSetPage *)getPage(PAGE_ID_CH_SETTINGS_PROT_OPP);
        if (page) {
            value = page->state;
        }
    }
}

void data_channel_protection_opp_level(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    auto cursor = widgetCursor.cursor;
    int iChannel = cursor >= 0 ? cursor : (g_channel ? g_channel->channelIndex : 0);
    Channel &channel = Channel::get(iChannel);
    if (operation == DATA_OPERATION_GET) {
        ChSettingsProtectionSetPage *page = (ChSettingsProtectionSetPage *)getPage(PAGE_ID_CH_SETTINGS_PROT_OPP);
        if (page) {
            value = page->level;
        } else {
            bool focused = g_focusCursor == widgetCursor && g_focusDataId == DATA_ID_CHANNEL_PROTECTION_OPP_LEVEL;
            if (focused && g_focusEditValue.getType() != VALUE_TYPE_UNDEFINED) {
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
    } else if (operation == DATA_OPERATION_SET_ENCODER_MODE) {
        channel.setPowerEncoderMode((EncoderMode)value.getInt());
    } 
}

void data_channel_protection_opp_delay(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    auto cursor = widgetCursor.cursor;
    int iChannel = cursor >= 0 ? cursor : (g_channel ? g_channel->channelIndex : 0);
    Channel &channel = Channel::get(iChannel);
    if (operation == DATA_OPERATION_GET) {
        ChSettingsProtectionSetPage *page = (ChSettingsProtectionSetPage *)getPage(PAGE_ID_CH_SETTINGS_PROT_OPP);
        if (page) {
            value = page->delay;
        } else {
            bool focused = g_focusCursor == widgetCursor && g_focusDataId == DATA_ID_CHANNEL_PROTECTION_OPP_DELAY;
            if (focused && g_focusEditValue.getType() != VALUE_TYPE_UNDEFINED) {
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
    } else if (operation == DATA_OPERATION_SET_ENCODER_MODE) {
        edit_mode_step::g_protectionDelayEncoderMode = (EncoderMode)value.getInt();
    }
}

void data_channel_protection_opp_limit(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    auto cursor = widgetCursor.cursor;
    int iChannel = cursor >= 0 ? cursor : (g_channel ? g_channel->channelIndex : 0);
    Channel &channel = Channel::get(iChannel);
    if (operation == DATA_OPERATION_GET) {
        if (channel.isRemoteProgrammingEnabled()) {
            value = RPROG_LABEL;
        } else {
            ChSettingsProtectionSetPage *page = (ChSettingsProtectionSetPage *)getPage(PAGE_ID_CH_SETTINGS_PROT_OPP);
            if (page) {
                value = page->limit;
            } else {
                bool focused = g_focusCursor == widgetCursor && g_focusDataId == DATA_ID_CHANNEL_PROTECTION_OPP_LIMIT;
                if (focused && g_focusEditValue.getType() != VALUE_TYPE_UNDEFINED) {
                    value = g_focusEditValue;
                } else {
                    value = MakeValue(channel_dispatcher::getPowerLimit(channel), UNIT_WATT);
                }
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
    } else if (operation == DATA_OPERATION_SET_ENCODER_MODE) {
        channel.setPowerEncoderMode((EncoderMode)value.getInt());
    }
}

void data_channel_protection_otp_installed(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = temperature::isChannelSensorInstalled(g_channel);
    }
}

void data_channel_protection_otp_state(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        ChSettingsProtectionSetPage *page =
            (ChSettingsProtectionSetPage *)getPage(PAGE_ID_CH_SETTINGS_PROT_OTP);
        if (page) {
            value = page->state;
        }
    }
}

void data_channel_protection_otp_level(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    auto cursor = widgetCursor.cursor;
    int iChannel = cursor >= 0 ? cursor : (g_channel ? g_channel->channelIndex : 0);
    Channel &channel = Channel::get(iChannel);
    if (operation == DATA_OPERATION_GET) {
        ChSettingsProtectionSetPage *page = (ChSettingsProtectionSetPage *)getPage(PAGE_ID_CH_SETTINGS_PROT_OTP);
        if (page) {
            value = page->level;
        } else {
            bool focused = g_focusCursor == widgetCursor && g_focusDataId == DATA_ID_CHANNEL_PROTECTION_OTP_LEVEL;
            if (focused && g_focusEditValue.getType() != VALUE_TYPE_UNDEFINED) {
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

        static float values[] = { 1.0f, 2.0f, 5.0f, 10.0f };
        stepValues->values = values;
        stepValues->count = sizeof(values) / sizeof(float);
        stepValues->unit = UNIT_CELSIUS;

        stepValues->encoderSettings.accelerationEnabled = true;
        stepValues->encoderSettings.range = 10.0f * stepValues->values[0];
        stepValues->encoderSettings.step = stepValues->values[0];
        stepValues->encoderSettings.mode = edit_mode_step::g_otpLevelEncoderMode;

        value = 1;
    } else if (operation == DATA_OPERATION_SET_ENCODER_MODE) {
        edit_mode_step::g_otpLevelEncoderMode = (EncoderMode)value.getInt();
    }
}
    
void data_channel_protection_otp_delay(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    auto cursor = widgetCursor.cursor;
    int iChannel = cursor >= 0 ? cursor : (g_channel ? g_channel->channelIndex : 0);
    Channel &channel = Channel::get(iChannel);
    if (operation == DATA_OPERATION_GET) {
        ChSettingsProtectionSetPage *page = (ChSettingsProtectionSetPage *)getPage(PAGE_ID_CH_SETTINGS_PROT_OTP);
        if (page) {
            value = page->delay;
        } else {
            bool focused = g_focusCursor == widgetCursor && g_focusDataId == DATA_ID_CHANNEL_PROTECTION_OTP_DELAY;
            if (focused && g_focusEditValue.getType() != VALUE_TYPE_UNDEFINED) {
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
    } else if (operation == DATA_OPERATION_SET_ENCODER_MODE) {
        edit_mode_step::g_protectionDelayEncoderMode = (EncoderMode)value.getInt();
    }
}

void data_channel_has_advanced_options(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    auto cursor = widgetCursor.cursor;
    if (operation == DATA_OPERATION_GET) {
		int iChannel = cursor >= 0 ? cursor : (g_channel ? g_channel->channelIndex : 0);
		Channel &channel = Channel::get(iChannel);
        value = channel.getAdvancedOptionsPageId() != PAGE_ID_NONE ? 1 : 0;
    }
}

void data_channel_has_firmware_update(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        auto modulType = g_slots[hmi::g_selectedSlotIndex]->moduleType;
        value = modulType != MODULE_TYPE_DCP405 ? 1 : 0;
    }
}

void data_channel_has_error_settings(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        auto modulType = g_slots[hmi::g_selectedSlotIndex]->moduleType;
        if (modulType != MODULE_TYPE_DCP405) {
            value = 1;
        } else {
            value = 0;
        }
    }
}

void data_channel_firmware_version(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        auto &slot = *g_slots[hmi::g_selectedSlotIndex];
        value = MakeFirmwareVersionValue(slot.firmwareMajorVersion, slot.firmwareMinorVersion);
    }
}

void data_channel_rsense_installed(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    auto cursor = widgetCursor.cursor;
    if (operation == DATA_OPERATION_GET) {
		int iChannel = cursor >= 0 ? cursor : (g_channel ? g_channel->channelIndex : 0);
		Channel &channel = Channel::get(iChannel);
        auto modulType = g_slots[channel.slotIndex]->moduleType;
        if (modulType == MODULE_TYPE_DCP405) {
            value = 1;
        } else {
            value = 0;
        }
    }
}

void data_channel_rsense_status(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    auto cursor = widgetCursor.cursor;
    if (operation == DATA_OPERATION_GET) {
		int iChannel = cursor >= 0 ? cursor : (g_channel ? g_channel->channelIndex : 0);
        Channel &channel = Channel::get(iChannel);
        value = channel.isRemoteSensingEnabled();
    }	
}

void data_channel_rprog_installed(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = g_channel->params.features & CH_FEATURE_RPROG ? 1 : 0;
    }
}

void data_channel_rprog_status(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    auto cursor = widgetCursor.cursor;
    if (operation == DATA_OPERATION_GET) {
		int iChannel = cursor >= 0 ? cursor : (g_channel ? g_channel->channelIndex : 0);
        Channel &channel = Channel::get(iChannel);
        value = channel.isRemoteProgrammingEnabled();
    }
}

void data_channel_dprog_installed(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    auto cursor = widgetCursor.cursor;
    if (operation == DATA_OPERATION_GET) {
        int iChannel = cursor >= 0 ? cursor : (g_channel ? g_channel->channelIndex : 0);
        Channel &channel = Channel::get(iChannel);
        value = channel.params.features & CH_FEATURE_DPROG ? 1 : 0;
    }
}

void data_channel_dprog(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    auto cursor = widgetCursor.cursor;
    if (operation == DATA_OPERATION_GET) {
        int iChannel = cursor >= 0 ? cursor : (g_channel ? g_channel->channelIndex : 0);
        Channel &channel = Channel::get(iChannel);
        value = (int)channel.flags.dprogState;
    }
}

void data_is_coupled_or_tracked(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
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

void data_channel_is_par_ser_coupled_or_tracked(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    auto cursor = widgetCursor.cursor;
    if (operation == DATA_OPERATION_GET) {
        int iChannel = cursor >= 0 ? cursor : (g_channel ? g_channel->channelIndex : -1);
        if (iChannel != -1) {
            Channel &channel = Channel::get(iChannel);
            if (channel.flags.trackingEnabled || (iChannel < 2 && (channel_dispatcher::getCouplingType() == channel_dispatcher::COUPLING_TYPE_PARALLEL || channel_dispatcher::getCouplingType() == channel_dispatcher::COUPLING_TYPE_SERIES))) {
                value = 1;
            } else {
                value = 0;
            }
        } else {
            value = 0;
        }
    }
}

void data_is_tracking_allowed(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
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

void data_channel_tracking_is_enabled(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    auto cursor = widgetCursor.cursor;
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

void data_channel_tracking_is_allowed(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    auto cursor = widgetCursor.cursor;
    if (operation == DATA_OPERATION_GET) {
        auto page = (SysSettingsTrackingPage *)getPage(PAGE_ID_SYS_SETTINGS_TRACKING);
        if (page) {
            auto &channel = Channel::get(cursor);
            value = channel_dispatcher::isTrackingAllowed(channel, page->m_couplingType, nullptr) ? 1 : 0;
        } else {
            value = 0;
        }
    }
}

void data_is_multi_tracking(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        auto page = (SysSettingsTrackingPage *)getPage(PAGE_ID_SYS_SETTINGS_TRACKING);
        if (page) {
            value = page->getNumTrackingChannels() >= 2 ? 1 : 0;
        }
    }
}

void data_is_any_coupling_allowed(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = channel_dispatcher::isCouplingTypeAllowed(channel_dispatcher::COUPLING_TYPE_PARALLEL, nullptr) ||
                channel_dispatcher::isCouplingTypeAllowed(channel_dispatcher::COUPLING_TYPE_SERIES, nullptr) ||
                channel_dispatcher::isCouplingTypeAllowed(channel_dispatcher::COUPLING_TYPE_COMMON_GND, nullptr) ||
                channel_dispatcher::isCouplingTypeAllowed(channel_dispatcher::COUPLING_TYPE_SPLIT_RAILS, nullptr) ? 1 : 0;
    }
}

void data_is_coupling_parallel_allowed(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = channel_dispatcher::isCouplingTypeAllowed(channel_dispatcher::COUPLING_TYPE_PARALLEL, nullptr) ? 1 : 0;
    }
}

void data_is_coupling_split_rails_allowed(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = channel_dispatcher::isCouplingTypeAllowed(channel_dispatcher::COUPLING_TYPE_SPLIT_RAILS, nullptr) ? 1 : 0;
    }
}


void data_is_coupling_series_allowed(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = channel_dispatcher::isCouplingTypeAllowed(channel_dispatcher::COUPLING_TYPE_SERIES, nullptr) ? 1 : 0;
    }
}

channel_dispatcher::CouplingType getCouplingType() {
    auto page = (SysSettingsCouplingPage *)getPage(PAGE_ID_SYS_SETTINGS_COUPLING);
    if (page) {
        return page->m_couplingType;
	} else {
		auto page = (SysSettingsTrackingPage *)getPage(PAGE_ID_SYS_SETTINGS_TRACKING);
		if (page) {
			return page->m_couplingType;
		}

		return channel_dispatcher::getCouplingType();
	}
}

void data_coupling_type(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = (int)getCouplingType();
    }
}

void data_is_coupling_type_uncoupled(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = getCouplingType() == channel_dispatcher::COUPLING_TYPE_NONE ? 1 : 0;
    }
}

void data_is_coupling_type_series(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = getCouplingType() == channel_dispatcher::COUPLING_TYPE_SERIES ? 1 : 0;
    }
}

void data_is_coupling_type_parallel(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = getCouplingType() == channel_dispatcher::COUPLING_TYPE_PARALLEL ? 1 : 0;
    }
}

void data_is_coupling_type_common_gnd(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = getCouplingType() == channel_dispatcher::COUPLING_TYPE_COMMON_GND ? 1 : 0;
    }
}

void data_is_coupling_type_split_rails(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = getCouplingType() == channel_dispatcher::COUPLING_TYPE_SPLIT_RAILS ? 1 : 0;
    }
}

void data_channel_copy_available(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        if (g_channel) {
            value = CH_NUM >= 2 ? 1 : 0;
        } else {
            value = g_slots[hmi::g_selectedSlotIndex]->isCopyToAvailable();
        }
    }
}

void data_channel_coupling_is_series(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    auto cursor = widgetCursor.cursor;
    if (operation == DATA_OPERATION_GET) {
		int iChannel = cursor >= 0 ? cursor : (g_channel ? g_channel->channelIndex : 0);
        value = iChannel < 2 && getCouplingType() == channel_dispatcher::COUPLING_TYPE_SERIES ? 1 : 0;
    }
}

void data_channel_coupling_enable_tracking_mode(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        auto page = (SysSettingsCouplingPage *)getPage(PAGE_ID_SYS_SETTINGS_COUPLING);
        if (page) {
            value = page->m_enableTrackingMode ? 1 : 0;
        }
    }
}

void data_sys_on_time_total(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value =
            Value((uint32_t)ontime::g_mcuCounter.getTotalTime(), VALUE_TYPE_ON_TIME_COUNTER);
    }
}

void data_sys_on_time_last(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value =
            Value((uint32_t)ontime::g_mcuCounter.getLastTime(), VALUE_TYPE_ON_TIME_COUNTER);
    }
}

void data_sys_temp_aux_status(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
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

void data_sys_temp_aux_otp_state(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        SysSettingsTemperaturePage *page = (SysSettingsTemperaturePage *)getPage(PAGE_ID_SYS_SETTINGS_TEMPERATURE);
        if (page) {
            value = page->state;
        }
    }
}

void data_sys_temp_aux_otp_level(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        SysSettingsTemperaturePage *page = (SysSettingsTemperaturePage *)getPage(PAGE_ID_SYS_SETTINGS_TEMPERATURE);
        if (page) {
            value = page->level;
        }
    }
}

void data_sys_temp_aux_otp_delay(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        SysSettingsTemperaturePage *page = (SysSettingsTemperaturePage *)getPage(PAGE_ID_SYS_SETTINGS_TEMPERATURE);
        if (page) {
            value = page->delay;
        }
    }
}

void data_sys_temp_aux_otp_is_tripped(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = temperature::sensors[temp_sensor::AUX].isTripped();
    }
}

void data_sys_temp_aux(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
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
        !eez::psu::sd_card::isMounted(nullptr, &err);
}

void data_sys_info_has_error(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = getSysInfoHasError();
    }
}

void data_sys_settings_has_error(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        temperature::TempSensorTemperature &tempSensor = temperature::sensors[temp_sensor::AUX];
        
        value = (getSysInfoHasError()
            // System temp & fan
            || tempSensor.isTripped()
#if OPTION_ETHERNET
            // Ethernet
            || ethernet::g_testResult == TEST_FAILED
            // Mqtt
            || (persist_conf::devConf.mqttEnabled && mqtt::g_connectionState == mqtt::CONNECTION_STATE_ERROR)
#endif
        ) ? 1 : 0;
    }
}

void data_sys_info_firmware_ver(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = Value(MCU_FIRMWARE);
    }
}

void data_sys_info_serial_no(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = Value(getSerialNumber());
    }
}

void data_sys_info_scpi_ver(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = Value(SCPI_STD_VERSION_REVISION);
    }
}

void data_sys_info_cpu(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = Value(0, VALUE_TYPE_MASTER_INFO);
    }
}

void data_sys_info_battery_status(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
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

void data_battery(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = MakeValue(mcu::battery::g_battery, UNIT_VOLT);
    }
}

void data_sys_info_sdcard_status(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
    	int err;
        if (eez::psu::sd_card::isMounted(nullptr, &err)) {
            if (eez::psu::sd_card::isBusy()) {
                value = 5; // busy
            } else {
                value = 1; // present
            }
        } else if (err == SCPI_ERROR_MISSING_MASS_MEDIA) {
            if (fs_driver::getDiskDrivesNum() > 0) {
                value = 6; // slot present
            } else {
                value = 2; // not present
            }
        } else if (err == SCPI_ERROR_MASS_MEDIA_NO_FILESYSTEM) {
        	value = 3; // no FAT
		} else {
			value = 4; // failed
		}
    }
}


void data_sys_info_fan_status(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = aux_ps::fan::getStatus();
    }
}

void data_sys_info_fan_speed(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
#if OPTION_FAN 
    if (operation == DATA_OPERATION_GET) {
        if (aux_ps::fan::g_testResult == TEST_OK) {
            value = MakeValue((float)aux_ps::fan::g_rpm, UNIT_RPM);
        }
    }
#endif
}

void data_sys_fan_mode(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        SysSettingsTemperaturePage *page = (SysSettingsTemperaturePage *)getPage(PAGE_ID_SYS_SETTINGS_TEMPERATURE);
        if (page) {
            value = page->fanMode;
        }
    }
}

void data_sys_fan_speed(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        SysSettingsTemperaturePage *page = (SysSettingsTemperaturePage *)getPage(PAGE_ID_SYS_SETTINGS_TEMPERATURE);
        if (page) {
            value = page->fanSpeedPercentage;
        }
    }
}

void data_channel_board_info_label(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    auto cursor = widgetCursor.cursor;
    if (operation == DATA_OPERATION_GET) {
        if (cursor >= 0 && cursor < CH_NUM) {
            value = Value(cursor, VALUE_TYPE_CHANNEL_BOARD_INFO_LABEL);
        }
    }
}

void data_channel_board_info_revision(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    auto cursor = widgetCursor.cursor;
    if (operation == DATA_OPERATION_GET) {
        if (cursor >= 0 && cursor < CH_NUM) {
            value = Value((int)Channel::get(cursor).slotIndex, VALUE_TYPE_SLOT_INFO);
        }
    }
}

void data_date_time_date(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        SysSettingsDateTimePage *page = (SysSettingsDateTimePage *)getPage(PAGE_ID_SYS_SETTINGS_DATE_TIME);
        if (page && page->ntpEnabled) {
            uint32_t nowUtc = datetime::nowUtc();
            uint32_t nowLocal = datetime::utcToLocal(nowUtc, page->timeZone, page->dstRule);
            value = Value(nowLocal, page->dateTimeFormat == datetime::FORMAT_DMY_24 || page->dateTimeFormat == datetime::FORMAT_DMY_12 ? VALUE_TYPE_DATE_DMY : VALUE_TYPE_DATE_MDY);
        }
    }
}

void data_date_time_year(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
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

void data_date_time_month(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
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

void data_date_time_day(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
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

void data_date_time_time(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        SysSettingsDateTimePage *page = (SysSettingsDateTimePage *)getPage(PAGE_ID_SYS_SETTINGS_DATE_TIME);
        if (page && page->ntpEnabled) {
            uint32_t nowUtc = datetime::nowUtc();
            uint32_t nowLocal = datetime::utcToLocal(nowUtc, page->timeZone, page->dstRule);
            value = Value(nowLocal, page->dateTimeFormat == datetime::FORMAT_DMY_24 || page->dateTimeFormat == datetime::FORMAT_MDY_24 ? VALUE_TYPE_TIME : VALUE_TYPE_TIME12);
        }
    }
}

void data_date_time_hour(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
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

void data_date_time_minute(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
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

void data_date_time_second(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
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

void data_date_time_time_zone(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        SysSettingsDateTimePage *page = (SysSettingsDateTimePage *)getPage(PAGE_ID_SYS_SETTINGS_DATE_TIME);
        if (page) {
            value = Value(page->timeZone, VALUE_TYPE_TIME_ZONE);
        }
    }
}

void data_date_time_dst(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        SysSettingsDateTimePage *page = (SysSettingsDateTimePage *)getPage(PAGE_ID_SYS_SETTINGS_DATE_TIME);
        if (page) {
            value = MakeEnumDefinitionValue(page->dstRule, ENUM_DEFINITION_DST_RULE);
        }
    }
}

void data_date_time_format(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        SysSettingsDateTimePage *page = (SysSettingsDateTimePage *)getPage(PAGE_ID_SYS_SETTINGS_DATE_TIME);
        if (page) {
            value = g_enumDefinitions[ENUM_DEFINITION_DATE_TIME_FORMAT][page->dateTimeFormat].menuLabel;
        }
    }
}

void data_date_time_format_is_dmy(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        SysSettingsDateTimePage *page = (SysSettingsDateTimePage *)getPage(PAGE_ID_SYS_SETTINGS_DATE_TIME);
        if (page) {
            value = page->dateTimeFormat == datetime::FORMAT_DMY_24 || page->dateTimeFormat == datetime::FORMAT_DMY_12 ? 1 : 0;
        }
    }
}

void data_date_time_format_is_24h(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        SysSettingsDateTimePage *page = (SysSettingsDateTimePage *)getPage(PAGE_ID_SYS_SETTINGS_DATE_TIME);
        if (page) {
            value = page->dateTimeFormat == datetime::FORMAT_DMY_24 || page->dateTimeFormat == datetime::FORMAT_MDY_24 ? 1 : 0;
        }
    }
}

void data_date_time_am_pm(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
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

void data_set_page_dirty(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        SetPage *page = (SetPage *)getActivePage();
        if (page) {
            value = Value(page->getDirty());
        }
    }
}

void data_profiles_list(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_COUNT) {
        value = NUM_PROFILE_LOCATIONS - 1; // do not show last location in GUI
    }
}

void data_profiles_auto_recall_status(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = Value(persist_conf::isProfileAutoRecallEnabled());
    }
}

void data_profiles_auto_recall_location(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = Value(persist_conf::getProfileAutoRecallLocation());
    }
}

void data_profile_status(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
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

void data_profile_label(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    auto cursor = widgetCursor.cursor;
    if (operation == DATA_OPERATION_GET) {
        int selectedProfileLocation = UserProfilesPage::getSelectedProfileLocation();
        if (selectedProfileLocation != -1) {
            value = Value(selectedProfileLocation, VALUE_TYPE_USER_PROFILE_LABEL);
        } else if (cursor >= 0) {
            value = Value(cursor, VALUE_TYPE_USER_PROFILE_LABEL);
        }
    }
}

void data_profile_remark(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    auto cursor = widgetCursor.cursor;
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

void data_profile_is_auto_recall_location(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        int selectedProfileLocation = UserProfilesPage::getSelectedProfileLocation();
        if (selectedProfileLocation != -1) {
            value = persist_conf::getProfileAutoRecallLocation() == selectedProfileLocation;
        }
    }
}

void data_profile_channel_u_set(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    auto cursor = widgetCursor.cursor;
    if (operation == DATA_OPERATION_GET) {
        int selectedProfileLocation = UserProfilesPage::getSelectedProfileLocation();
        if (selectedProfileLocation != -1 && (cursor >= 0 && cursor < CH_MAX)) {
            profile::Parameters *profile = profile::getProfileParameters(selectedProfileLocation);
            if (profile) {
                value = MakeValue(g_slots[Channel::get(cursor).slotIndex]->getProfileUSet((uint8_t *)profile->channels[cursor].parameters), UNIT_VOLT);
            }
        }
    }
}

void data_profile_channel_i_set(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    auto cursor = widgetCursor.cursor;
    if (operation == DATA_OPERATION_GET) {
        int selectedProfileLocation = UserProfilesPage::getSelectedProfileLocation();
        if (selectedProfileLocation != -1 && (cursor >= 0 && cursor < CH_MAX)) {
            profile::Parameters *profile = profile::getProfileParameters(selectedProfileLocation);
            if (profile) {
                value = MakeValue(g_slots[Channel::get(cursor).slotIndex]->getProfileISet((uint8_t *)profile->channels[cursor].parameters), UNIT_AMPER);
            }
        }
    }
}

void data_profile_channel_output_state(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    auto cursor = widgetCursor.cursor;
    if (operation == DATA_OPERATION_GET) {
        int selectedProfileLocation = UserProfilesPage::getSelectedProfileLocation();
        if (selectedProfileLocation != -1 && (cursor >= 0 && cursor < CH_MAX)) {
            profile::Parameters *profile = profile::getProfileParameters(selectedProfileLocation);
            if (profile) {
                value = (int)g_slots[Channel::get(cursor).slotIndex]->getProfileOutputEnable((uint8_t *)profile->channels[cursor].parameters);
            }
        }
    }
}

void data_ethernet_installed(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = Value(OPTION_ETHERNET);
    }
}

void data_ethernet_enabled(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
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

void data_ethernet_status(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
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

void data_ethernet_and_mqtt_status(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
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

void data_mqtt_in_error(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
#if OPTION_ETHERNET
    if (operation == DATA_OPERATION_GET) {
        value = persist_conf::devConf.mqttEnabled && mqtt::g_connectionState == mqtt::CONNECTION_STATE_ERROR;
    }
#endif
}

void data_ethernet_ip_address(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
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
                    value = Value(page->m_ipAddress, VALUE_TYPE_IP_ADDRESS);
                }
            } else {
                value = Value(psu::persist_conf::devConf.ethernetIpAddress, VALUE_TYPE_IP_ADDRESS);
            }
        }
    }
#endif
}

void data_ethernet_host_name(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
#if OPTION_ETHERNET
    if (operation == DATA_OPERATION_GET) {
        SysSettingsEthernetPage *page = (SysSettingsEthernetPage *)getPage(PAGE_ID_SYS_SETTINGS_ETHERNET);
        if (page) {
            value = page->m_hostName;
        } else {
            value = persist_conf::devConf.ethernetHostName;
        }
    } else if (operation == DATA_OPERATION_GET_MAX) {
        value = Value(ETHERNET_HOST_NAME_SIZE, VALUE_TYPE_UINT32);
    } else if (operation == DATA_OPERATION_IS_VALID_VALUE) {
        value = persist_conf::validateEthernetHostName(value.getString());
    } else if (operation == DATA_OPERATION_SET) {
        SysSettingsEthernetPage *page = (SysSettingsEthernetPage *)getPage(PAGE_ID_SYS_SETTINGS_ETHERNET);
        if (page) {
            stringCopy(page->m_hostName, sizeof(page->m_hostName), value.getString());
        }
    }
#endif
}

void data_ethernet_dns(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
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

void data_ethernet_gateway(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
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

void data_ethernet_subnet_mask(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
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

void data_ethernet_scpi_port(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
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

void data_ethernet_is_connected(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
#if OPTION_ETHERNET
    if (operation == DATA_OPERATION_GET) {
        value = Value(ethernet::isConnected());
    }
#endif
}

void data_ethernet_dhcp(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
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

void data_ethernet_mac(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
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

void data_sys_output_protection_coupled(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = persist_conf::isOutputProtectionCoupleEnabled();
    }
}

void data_sys_shutdown_when_protection_tripped(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = persist_conf::isShutdownWhenProtectionTrippedEnabled();
    }
}

void data_sys_force_disabling_all_outputs_on_power_up(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = persist_conf::isForceDisablingAllOutputsOnPowerUpEnabled();
    }
}

void data_sys_password_is_set(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = strlen(persist_conf::devConf.systemPassword) > 0;
    }
}

void data_sys_rl_state(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = Value(g_rlState);
    }
}

void data_sys_sound_enabled(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
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

void data_sys_sound_is_enabled(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = persist_conf::isSoundEnabled();
    }
}

void data_sys_sound_is_click_enabled(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = persist_conf::isClickSoundEnabled();
    }
}

void data_channel_display_view_settings_display_values(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_COUNT) {
        value = 2;
    }
}

void data_channel_display_view_settings_display_value_label(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    auto cursor = widgetCursor.cursor;
    if (operation == DATA_OPERATION_GET) {
        value = cursor == 0 ? "Display value #1:" : "Display value #2:";
    }
}

void data_channel_display_view_settings_display_value(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    auto cursor = widgetCursor.cursor;
    if (operation == DATA_OPERATION_GET) {
        auto page = (ChSettingsAdvViewPage *)getPage(PAGE_ID_CH_SETTINGS_ADV_VIEW);
        if (page) {
            value = MakeEnumDefinitionValue(page->displayValues[cursor].type, ENUM_DEFINITION_CHANNEL_DISPLAY_VALUE);
        }
    }
}

void data_channel_display_view_settings_scale(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    auto cursor = widgetCursor.cursor;
    if (operation == DATA_OPERATION_GET) {
        auto page = (ChSettingsAdvViewPage *)getPage(PAGE_ID_CH_SETTINGS_ADV_VIEW);
        if (page) {
            value = MakeEnumDefinitionValue(page->displayValues[cursor].scale, ENUM_DEFINITION_CHANNEL_DISPLAY_VALUE_SCALE);
        }
    }
}

void data_channel_display_view_settings_is_custom_scale(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    auto cursor = widgetCursor.cursor;
    if (operation == DATA_OPERATION_GET) {
        auto page = (ChSettingsAdvViewPage *)getPage(PAGE_ID_CH_SETTINGS_ADV_VIEW);
        if (page) {
            value = page->displayValues[cursor].scale == DISPLAY_VALUE_SCALE_CUSTOM;
        }
    }
}

void data_channel_display_view_settings_range(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    auto cursor = widgetCursor.cursor;
    if (operation == DATA_OPERATION_GET) {
        auto page = (ChSettingsAdvViewPage *)getPage(PAGE_ID_CH_SETTINGS_ADV_VIEW);
        if (page) {
            value = Value(page->displayValues[cursor].getRange(g_channel),
                page->displayValues[cursor].type == DISPLAY_VALUE_VOLTAGE ? UNIT_VOLT :
                page->displayValues[cursor].type == DISPLAY_VALUE_CURRENT ? UNIT_AMPER :
                UNIT_WATT);
        }
    }
}

void data_channel_display_view_settings_yt_view_rate(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        ChSettingsAdvViewPage *page = (ChSettingsAdvViewPage *)getPage(PAGE_ID_CH_SETTINGS_ADV_VIEW);
        if (page) {
            value = MakeValue(page->ytViewRate, UNIT_SECOND);
        }
    }
}

void data_sys_encoder_confirmation_mode(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
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

void data_sys_encoder_moving_up_speed(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
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

void data_sys_encoder_moving_down_speed(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
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

void data_sys_encoder_installed(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = Value(OPTION_ENCODER);
    }
}

void data_sys_display_state(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = (int)persist_conf::devConf.displayState;
    }
}

void data_sys_display_brightness(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
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

void data_channel_coupling_state(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    auto cursor = widgetCursor.cursor;
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

void data_channel_trigger_mode(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        auto page = (ChSettingsTriggerPage *)getPage(PAGE_ID_CH_SETTINGS_TRIGGER);
        value = MakeEnumDefinitionValue(page ? page->triggerMode : channel_dispatcher::getVoltageTriggerMode(*g_channel), ENUM_DEFINITION_CHANNEL_TRIGGER_MODE);
    }
}

void data_channel_u_trigger_value(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    auto cursor = widgetCursor.cursor;
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
        data_channel_u_edit(operation, widgetCursor, value);
    }
}

void data_channel_i_trigger_value(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    auto cursor = widgetCursor.cursor;
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
        data_channel_i_edit(operation, widgetCursor, value);
    }
}

void data_channel_list_count(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
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

void data_channel_trigger_on_list_stop(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
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

void data_channel_lists(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        ChSettingsListsPage *page = (ChSettingsListsPage *)getPage(PAGE_ID_CH_SETTINGS_LISTS);
        value = page->m_listVersion;
    } else if (operation == DATA_OPERATION_COUNT) {
        value = LIST_ITEMS_PER_PAGE;
    } else if (operation == DATA_OPERATION_GET_FLOAT_LIST_LENGTH) {
        ChSettingsListsPage *page = (ChSettingsListsPage *)getPage(PAGE_ID_CH_SETTINGS_LISTS);
        if (page) {
            value = page->getMaxListLength();
        }
    }
}

void data_channel_list_index(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    auto cursor = widgetCursor.cursor;
    if (operation == DATA_OPERATION_GET) {
        ChSettingsListsPage *page = (ChSettingsListsPage *)getPage(PAGE_ID_CH_SETTINGS_LISTS);
        if (page) {
            int iPage = page->getPageIndex();
            int iRow = iPage * LIST_ITEMS_PER_PAGE + cursor;
            value = iRow + 1;
        }
    }
}

void data_channel_list_dwell(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    auto cursor = widgetCursor.cursor;
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
    } else if (operation == DATA_OPERATION_GET_ENCODER_STEP_VALUES) {
        static float values[] = { 0.001f, 0.01f, 0.1f, 1.0f };

        StepValues *stepValues = value.getStepValues();

        stepValues->values = values;
        stepValues->count = sizeof(values) / sizeof(float);
        stepValues->unit = UNIT_SECOND;

        stepValues->encoderSettings.accelerationEnabled = true;
        stepValues->encoderSettings.range = 10.0f * stepValues->values[0];
        stepValues->encoderSettings.step = stepValues->values[0];
        stepValues->encoderSettings.mode = edit_mode_step::g_listDwellEncoderMode;

        value = 1;
    } else if (operation == DATA_OPERATION_SET_ENCODER_MODE) {
        edit_mode_step::g_listDwellEncoderMode = (EncoderMode)value.getInt();
    }
}

void data_channel_list_dwell_enabled(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    auto cursor = widgetCursor.cursor;
    if (operation == DATA_OPERATION_GET) {
        ChSettingsListsPage *page = (ChSettingsListsPage *)getPage(PAGE_ID_CH_SETTINGS_LISTS);
        if (page) {
            int iPage = page->getPageIndex();
            int iRow = iPage * LIST_ITEMS_PER_PAGE + cursor;
            value = iRow <= page->m_dwellListLength;
        }
    }
}

void data_channel_list_voltage(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    auto cursor = widgetCursor.cursor;
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
    } else if (operation == DATA_OPERATION_GET_ENCODER_STEP_VALUES) {
        StepValues *stepValues = value.getStepValues();

		WidgetCursor uEditWidgetCursor = widgetCursor;
		uEditWidgetCursor = g_channel->channelIndex;
        data_channel_u_edit(operation, uEditWidgetCursor, value);

        stepValues->encoderSettings.range = g_channel->params.U_MAX;
        stepValues->encoderSettings.step = g_channel->params.U_RESOLUTION;
        stepValues->encoderSettings.mode = edit_mode_step::g_listVoltageEncoderMode;

        value = 1;
    } else if (operation == DATA_OPERATION_SET_ENCODER_MODE) {
        edit_mode_step::g_listVoltageEncoderMode = (EncoderMode)value.getInt();
    }
}

void data_channel_list_voltage_enabled(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    auto cursor = widgetCursor.cursor;
    if (operation == DATA_OPERATION_GET) {
        ChSettingsListsPage *page = (ChSettingsListsPage *)getPage(PAGE_ID_CH_SETTINGS_LISTS);
        if (page) {
            int iPage = page->getPageIndex();
            int iRow = iPage * LIST_ITEMS_PER_PAGE + cursor;
            value = iRow <= page->m_voltageListLength;
        }
    }
}

void data_channel_list_current(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    auto cursor = widgetCursor.cursor;
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
    } else if (operation == DATA_OPERATION_GET_ENCODER_STEP_VALUES) {
        StepValues *stepValues = value.getStepValues();

        data_channel_i_edit(operation, widgetCursor, value);

        stepValues->encoderSettings.range = g_channel->params.I_MAX;
        stepValues->encoderSettings.step = g_channel->params.I_RESOLUTION;
        stepValues->encoderSettings.mode = edit_mode_step::g_listCurrentEncoderMode;

        value = 1;
    } else if (operation == DATA_OPERATION_SET_ENCODER_MODE) {
        edit_mode_step::g_listCurrentEncoderMode = (EncoderMode)value.getInt();
    }
}

void data_channel_list_current_enabled(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    auto cursor = widgetCursor.cursor;
    if (operation == DATA_OPERATION_GET) {
        ChSettingsListsPage *page = (ChSettingsListsPage *)getPage(PAGE_ID_CH_SETTINGS_LISTS);
        if (page) {
            int iPage = page->getPageIndex();
            int iRow = iPage * LIST_ITEMS_PER_PAGE + cursor;
            value = iRow <= page->m_currentListLength;
        }
    }
}

void data_channel_lists_previous_page_enabled(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        ChSettingsListsPage *page = (ChSettingsListsPage *)getPage(PAGE_ID_CH_SETTINGS_LISTS);
        if (page) {
            int iPage = page->getPageIndex();
            value = iPage > 0;
        }
    }
}

void data_channel_lists_next_page_enabled(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        ChSettingsListsPage *page = (ChSettingsListsPage *)getPage(PAGE_ID_CH_SETTINGS_LISTS);
        if (page) {
            int iPage = page->getPageIndex();
            value = (iPage < page->getNumPages() - 1);
        }
    }
}

void data_channel_lists_cursor(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
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

void data_channel_lists_insert_menu_enabled(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        ChSettingsListsPage *page = (ChSettingsListsPage *)getPage(PAGE_ID_CH_SETTINGS_LISTS);
        if (page) {
            value = page->getRowIndex() < page->getMaxListLength();
        }
    }
}

void data_channel_lists_delete_menu_enabled(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        ChSettingsListsPage *page = (ChSettingsListsPage *)getPage(PAGE_ID_CH_SETTINGS_LISTS);
        if (page) {
            value = page->getMaxListLength();
        }
    }
}

void data_channel_lists_delete_row_enabled(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        ChSettingsListsPage *page = (ChSettingsListsPage *)getPage(PAGE_ID_CH_SETTINGS_LISTS);
        if (page) {
            value = page->getRowIndex() < page->getMaxListLength();
        }
    }
}

void data_channel_lists_clear_column_enabled(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        ChSettingsListsPage *page = (ChSettingsListsPage *)getPage(PAGE_ID_CH_SETTINGS_LISTS);
        if (page) {
            value = page->getRowIndex() < page->getMaxListLength();
        }
    }
}

void data_channel_lists_delete_rows_enabled(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        ChSettingsListsPage *page = (ChSettingsListsPage *)getPage(PAGE_ID_CH_SETTINGS_LISTS);
        if (page) {
            value = page->getRowIndex() < page->getMaxListLength();
        }
    }
}

void data_trigger_source(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        SysSettingsTriggerPage *page =
            (SysSettingsTriggerPage *)getPage(PAGE_ID_SYS_SETTINGS_TRIGGER);
        if (page) {
            value = MakeEnumDefinitionValue(page->m_source, ENUM_DEFINITION_TRIGGER_SOURCE);
        }
    }
}

void data_trigger_delay(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        SysSettingsTriggerPage *page =
            (SysSettingsTriggerPage *)getPage(PAGE_ID_SYS_SETTINGS_TRIGGER);
        if (page) {
            value = MakeValue(page->m_delay, UNIT_SECOND);
        }
    }
}

void data_trigger_initiate_continuously(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        SysSettingsTriggerPage *page =
            (SysSettingsTriggerPage *)getPage(PAGE_ID_SYS_SETTINGS_TRIGGER);
        if (page) {
            value = page->m_initiateContinuously;
        }
    }
}

void data_trigger_initiate_all(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        SysSettingsTriggerPage *page =
            (SysSettingsTriggerPage *)getPage(PAGE_ID_SYS_SETTINGS_TRIGGER);
        if (page) {
            value = page->m_initiateAll;
        }
    }
}


void data_trigger_state(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = trigger::getState();
    }
}

void data_trigger_is_initiated(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    auto cursor = widgetCursor.cursor;
    if (operation == DATA_OPERATION_GET) {
        int iChannel = cursor >= 0 ? cursor : (g_channel ? g_channel->channelIndex : 0);
        Channel &channel = Channel::get(iChannel);
        value = (trigger::isInitiated() || trigger::isTriggered()) && channel_dispatcher::getVoltageTriggerMode(channel) != TRIGGER_MODE_FIXED;
    } else if (operation == DATA_OPERATION_IS_BLINKING) {
        value = trigger::isInitiated();
    }
}

void data_trigger_is_manual(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = trigger::g_triggerSource == trigger::SOURCE_MANUAL && !trigger::isTriggered();
    }
}

void data_channel_has_support_for_current_dual_range(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    auto cursor = widgetCursor.cursor;
    if (operation == DATA_OPERATION_GET) {
        int iChannel = cursor >= 0 ? cursor : (g_channel ? g_channel->channelIndex : 0);
        Channel &channel = Channel::get(iChannel);
        value = channel.hasSupportForCurrentDualRange();
    }
}

void data_channel_ranges_supported(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = g_channel->hasSupportForCurrentDualRange();
    }
}

void data_channel_ranges_mode(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = MakeEnumDefinitionValue(g_channel->getCurrentRangeSelectionMode(),
                                        ENUM_DEFINITION_CHANNEL_CURRENT_RANGE_SELECTION_MODE);
    }
}

void data_channel_ranges_auto_ranging(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = g_channel->isAutoSelectCurrentRangeEnabled();
    }
}

void data_channel_ranges_currently_selected(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    auto cursor = widgetCursor.cursor;
    if (operation == DATA_OPERATION_GET) {
        int iChannel = cursor >= 0 ? cursor : (g_channel ? g_channel->channelIndex : 0);
        Channel &channel = Channel::get(iChannel);
        value = MakeEnumDefinitionValue(
            (channel.flags.currentRangeSelectionMode == CURRENT_RANGE_SELECTION_USE_BOTH ? 2 : 0) +
                channel.flags.currentCurrentRange,
            ENUM_DEFINITION_CHANNEL_CURRENT_RANGE
        );
    }
}

void data_text_message(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = Value(getTextMessageVersion(), VALUE_TYPE_TEXT_MESSAGE);
    }
}

void data_serial_status(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = Value(serial::g_testResult);
    }
}

void data_serial_is_connected(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = Value(serial::isConnected());
    }
}

void data_io_pins(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_COUNT) {
        value = 4;
    }
}

void data_io_pins_inhibit_state(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        if (io_pins::isInhibited()) {
            value = 1;
        } else {
            const io_pins::IOPin &inputPin1 = io_pins::g_ioPins[0];
            const io_pins::IOPin &inputPin2 = io_pins::g_ioPins[1];
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

void data_io_pin_label(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    auto cursor = widgetCursor.cursor;
    if (operation == DATA_OPERATION_GET) {
		const char *g_pin_label[] = {
			"#1 Din1",
			"#2 Din2",
			"#3 Dout1",
			"#4 Dout2",
		};
        value = g_pin_label[cursor];
    }
}

void data_io_pin_polarity(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    auto cursor = widgetCursor.cursor;
    if (operation == DATA_OPERATION_GET) {
        SysSettingsIOPinsPage *page = (SysSettingsIOPinsPage *)getPage(PAGE_ID_SYS_SETTINGS_IO);
        if (page) {
            value = MakeEnumDefinitionValue(page->m_polarity[cursor], ENUM_DEFINITION_IO_PINS_POLARITY);
        }
    }
}

void data_io_pin_function(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    auto cursor = widgetCursor.cursor;
    if (operation == DATA_OPERATION_GET) {
        SysSettingsIOPinsPage *page = (SysSettingsIOPinsPage *)getPage(PAGE_ID_SYS_SETTINGS_IO);
        if (page) {
            if (page->m_function[cursor] == io_pins::FUNCTION_PWM) {
                value = 0;
            } else if (page->m_function[cursor] == io_pins::FUNCTION_UART) {
				value = 1;
            } else {
                value = 2;
            }
        }
    }
}

void data_io_pin_function_name(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    auto cursor = widgetCursor.cursor;
    if (operation == DATA_OPERATION_GET) {
        SysSettingsIOPinsPage *page = (SysSettingsIOPinsPage *)getPage(PAGE_ID_SYS_SETTINGS_IO);
        if (page) {
            if (cursor == DIN1) {
                value = MakeEnumDefinitionValue(page->m_function[cursor], ENUM_DEFINITION_IO_PINS_INPUT1_FUNCTION_WITH_DLOG_TRIGGER);
            } else if (cursor == DIN2) {
				value = MakeEnumDefinitionValue(page->m_function[cursor], ENUM_DEFINITION_IO_PINS_INPUT2_FUNCTION_WITH_DLOG_TRIGGER);
			} else if (cursor == DOUT1) {
				value = MakeEnumDefinitionValue(page->m_function[cursor], ENUM_DEFINITION_IO_PINS_OUTPUT1_FUNCTION);
			} else {
                value = MakeEnumDefinitionValue(page->m_function[cursor], ENUM_DEFINITION_IO_PINS_OUTPUT2_FUNCTION);
            }
        }
    }
}

void data_io_pin_state(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    auto cursor = widgetCursor.cursor;
    if (operation == DATA_OPERATION_GET) {
        SysSettingsIOPinsPage *page = (SysSettingsIOPinsPage *)getPage(PAGE_ID_SYS_SETTINGS_IO);
        if (page) {
            int pin = cursor;
            if (page->m_function[pin] == io_pins::FUNCTION_NONE) {
                value = 4; // Unassigned
            } else {
                int state = io_pins::getPinState(cursor);

                if (page->m_polarity[pin] != io_pins::g_ioPins[pin].polarity) {
                    state = state ? 0 : 1;
                }

                if (pin >= 2 && page->m_function[pin] == io_pins::FUNCTION_OUTPUT && io_pins::g_ioPins[pin].function == io_pins::FUNCTION_OUTPUT) {
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

void data_io_pin_pwm_frequency(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    auto cursor = widgetCursor.cursor;
    if (operation == DATA_OPERATION_GET) {
        bool focused = g_focusCursor == widgetCursor && g_focusDataId == DATA_ID_IO_PIN_PWM_FREQUENCY;
        if (focused && g_focusEditValue.getType() != VALUE_TYPE_UNDEFINED) {
            value = g_focusEditValue;
        } else if (focused && getActivePageId() == PAGE_ID_EDIT_MODE_KEYPAD && edit_mode_keypad::g_keypad->isEditing()) {
            data_keypad_text(operation, widgetCursor, value);
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
    } else if (operation == DATA_OPERATION_GET_ENCODER_STEP_VALUES) {
        static float values[] = { 1.0f, 100.0f, 1000.0f, 10000.0f };

        StepValues *stepValues = value.getStepValues();

        stepValues->values = values;
        stepValues->count = sizeof(values) / sizeof(float);
        stepValues->unit = UNIT_HERTZ;

        stepValues->encoderSettings.accelerationEnabled = true;

        SysSettingsIOPinsPage *page = (SysSettingsIOPinsPage *)getPage(PAGE_ID_SYS_SETTINGS_IO);
        float fvalue = page && page->getDirty() ? page->getPwmFrequency(cursor) : io_pins::getPwmFrequency(cursor);
        float step = MAX(powf(10.0f, floorf(log10f(fabsf(fvalue))) - 1), 0.001f);

        stepValues->encoderSettings.range = step * 5.0f;
        stepValues->encoderSettings.step = step;

        stepValues->encoderSettings.mode = eez::psu::gui::edit_mode_step::g_frequencyEncoderMode;

        value = 1;
    } else if (operation == DATA_OPERATION_SET_ENCODER_MODE) {
        eez::psu::gui::edit_mode_step::g_frequencyEncoderMode = (EncoderMode)value.getInt();
    }
}

void data_io_pin_pwm_duty(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    auto cursor = widgetCursor.cursor;
    if (operation == DATA_OPERATION_GET) {
        bool focused = g_focusCursor == widgetCursor && g_focusDataId == DATA_ID_IO_PIN_PWM_DUTY;
        if (focused && g_focusEditValue.getType() != VALUE_TYPE_UNDEFINED) {
            value = g_focusEditValue;
        } else if (focused && getActivePageId() == PAGE_ID_EDIT_MODE_KEYPAD && edit_mode_keypad::g_keypad->isEditing()) {
            data_keypad_text(operation, widgetCursor, value);
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
    } else if (operation == DATA_OPERATION_GET_ENCODER_STEP_VALUES) {
        static float values[] = { 0.1f, 0.5f, 1.0f, 5.0f };

        StepValues *stepValues = value.getStepValues();

        stepValues->values = values;
        stepValues->count = sizeof(values) / sizeof(float);
        stepValues->unit = UNIT_PERCENT;

        stepValues->encoderSettings.accelerationEnabled = false;
        stepValues->encoderSettings.range = 100.0f;
        stepValues->encoderSettings.step = 1.0f;

        stepValues->encoderSettings.mode = eez::psu::gui::edit_mode_step::g_dutyEncoderMode;

        value = 1;
    } else if (operation == DATA_OPERATION_SET_ENCODER_MODE) {
        eez::psu::gui::edit_mode_step::g_dutyEncoderMode = (EncoderMode)value.getInt();
    } 
}

void data_io_pin_uart_mode(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
	if (operation == DATA_OPERATION_GET) {
		SysSettingsIOPinsPage *page = (SysSettingsIOPinsPage *)getPage(PAGE_ID_SYS_SETTINGS_IO);
		if (page) {
			value = MakeEnumDefinitionValue(page->m_uartMode, ENUM_DEFINITION_UART_MODE);
		}
	}
}

void data_io_pin_is_uart_enabled(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
	if (operation == DATA_OPERATION_GET) {
		SysSettingsIOPinsPage *page = (SysSettingsIOPinsPage *)getPage(PAGE_ID_SYS_SETTINGS_IO);
		if (page) {
			value = page->m_function[0] == io_pins::FUNCTION_UART;
		}
	}
}

void data_io_pin_uart_baud(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
	if (operation == DATA_OPERATION_GET) {
		SysSettingsIOPinsPage *page = (SysSettingsIOPinsPage *)getPage(PAGE_ID_SYS_SETTINGS_IO);
		if (page) {
			value = (int)page->m_uartBaudRate;
		}
	}
}

void data_io_pin_uart_data_bits(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
	if (operation == DATA_OPERATION_GET) {
		SysSettingsIOPinsPage *page = (SysSettingsIOPinsPage *)getPage(PAGE_ID_SYS_SETTINGS_IO);
		if (page) {
			value = (int)page->m_uartDataBits;
		}
	}
}

void data_io_pin_uart_parity(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
	if (operation == DATA_OPERATION_GET) {
		SysSettingsIOPinsPage *page = (SysSettingsIOPinsPage *)getPage(PAGE_ID_SYS_SETTINGS_IO);
		if (page) {
			value = page->m_uartParity == 0 ? "None" : page->m_uartParity == 1 ? "Even" : "Odd";
		}
	}
}

void data_io_pin_uart_stop_bits(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
	if (operation == DATA_OPERATION_GET) {
		SysSettingsIOPinsPage *page = (SysSettingsIOPinsPage *)getPage(PAGE_ID_SYS_SETTINGS_IO);
		if (page) {
			value = (int)page->m_uartStopBits;
		}
	}
}

void data_ntp_enabled(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        SysSettingsDateTimePage *page = (SysSettingsDateTimePage *)getPage(PAGE_ID_SYS_SETTINGS_DATE_TIME);
        if (page) {
            value = page->ntpEnabled;
        } else {
            value = persist_conf::isNtpEnabled();
        }
    }
}

void data_ntp_server(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        SysSettingsDateTimePage *page =
            (SysSettingsDateTimePage *)getPage(PAGE_ID_SYS_SETTINGS_DATE_TIME);
        if (page) {
            value = page->ntpServer[0] ? page->ntpServer : "<not specified>";
        }
    }
}

void data_sys_display_background_luminosity_step(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
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

void data_simulator_load_state(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    auto cursor = widgetCursor.cursor;
	if (operation == DATA_OPERATION_GET) {
		if (cursor >= 0 && cursor < CH_NUM) {
			Channel &channel = Channel::get(cursor);
			value = channel.simulator.getLoadEnabled() ? 1 : 0;
		} else {
			value = 0;
		}
	}
}

void data_simulator_load(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    auto cursor = widgetCursor.cursor;
	if (operation == DATA_OPERATION_GET) {
        Channel &channel = Channel::get(cursor);
        value = MakeValue(channel.simulator.getLoad(), UNIT_OHM);
	}
}

void data_simulator_load_state2(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    auto cursor = widgetCursor.cursor;
    if (operation == DATA_OPERATION_GET) {
		if (cursor + 1 < CH_NUM) {
			Channel &channel = Channel::get(cursor + 1);
			value = channel.simulator.getLoadEnabled() ? 1 : 0;
		}
    }
}

void data_simulator_load2(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    auto cursor = widgetCursor.cursor;
    if (operation == DATA_OPERATION_GET) {
        Channel &channel = Channel::get(cursor + 1);
        value = MakeValue(channel.simulator.getLoad(), UNIT_OHM);
    }
}

#endif

void data_channel_active_coupled_led(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    auto cursor = widgetCursor.cursor;
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

void data_channels_with_list_counter_visible(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_COUNT) {
        value = list::g_numChannelsWithVisibleCounters;
    }
}

void data_list_counter_label(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    auto cursor = widgetCursor.cursor;
    if (operation == DATA_OPERATION_GET) {
        if (cursor >= 0 && cursor < list::g_numChannelsWithVisibleCounters) {
            int iChannel = list::g_channelsWithVisibleCounters[cursor];
            value = Value(iChannel, VALUE_TYPE_CHANNEL_SHORT_TITLE_WITHOUT_TRACKING_ICON);
        }
    }
}

void data_channel_list_countdown(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    auto cursor = widgetCursor.cursor;
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

void data_channels_with_ramp_counter_visible(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_COUNT) {
        value = ramp::g_numChannelsWithVisibleCounters;
    }
}

void data_ramp_counter_label(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    auto cursor = widgetCursor.cursor;
    if (operation == DATA_OPERATION_GET) {
        if (cursor >= 0 && cursor < ramp::g_numChannelsWithVisibleCounters) {
            int iChannel = ramp::g_channelsWithVisibleCounters[cursor];
            value = Value(iChannel, VALUE_TYPE_CHANNEL_SHORT_TITLE_WITHOUT_TRACKING_ICON);
        }
    }
}

void data_channel_ramp_countdown(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    auto cursor = widgetCursor.cursor;
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

void data_overlay_minimized(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
	if (operation == DATA_OPERATION_GET) {
		value = (persist_conf::devConf.overlayVisibility & OVERLAY_MINIMIZED) != 0;
	}
}

void data_overlay_hidden(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
	if (operation == DATA_OPERATION_GET) {
		value = (persist_conf::devConf.overlayVisibility & OVERLAY_HIDDEN) != 0;
	}
}

void data_overlay(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    enum {
		TITLE_LIST_ICON_WIDGET,
		TITLE_RAMP_ICON_WIDGET,
		TITLE_DLOG_ICON_WIDGET,
		TITLE_MP_ICON_WIDGET,
		TITLE_FUNCGEN_ICON_WIDGET,
		TITLE_MIN_ICON_WIDGET,
		TITLE_MAX_ICON_WIDGET,
		TITLE_HIDE_ICON_WIDGET,
        LIST_ICON_WIDGET,
        LIST_GRID_WIDGET,
        RAMP_ICON_WIDGET,
        RAMP_GRID_WIDGET,
        DLOG_INFO_WIDGET,
        SCRIPT_INFO_WIDGET,
        FUNCGEN_WIDGET,
        NUM_WIDGETS
    };

    static Overlay overlay;
    static WidgetOverride widgetOverrides[NUM_WIDGETS];

    if (operation == DATA_OPERATION_GET_OVERLAY_DATA) {
        value = Value(&overlay, VALUE_TYPE_POINTER);
    } else if (operation == DATA_OPERATION_UPDATE_OVERLAY_DATA) {
		overlay.visibility = persist_conf::devConf.overlayVisibility;

        if (!overlay.widgetOverrides) {
			overlay.moved = persist_conf::devConf.overlayMoved;
			overlay.xOffsetMinimized = persist_conf::devConf.overlayXOffsetMinimized;
            overlay.yOffsetMinimized = persist_conf::devConf.overlayYOffsetMinimized;
            overlay.xOffsetMaximized = persist_conf::devConf.overlayXOffsetMaximized;
            overlay.yOffsetMaximized = persist_conf::devConf.overlayYOffsetMaximized;
        }

        overlay.widgetOverrides = widgetOverrides;

        bool areListCountersVisible = list::g_numChannelsWithVisibleCounters > 0;
        bool areRampCountersVisible = ramp::g_numChannelsWithVisibleCounters > 0;
        bool isDlogVisible = !dlog_record::isIdle();
        bool isScriptVisible = !scripting::isIdle();
        bool isFunctionGeneratorVisible = function_generator::isActive();

        bool isHidden = (overlay.visibility & OVERLAY_HIDDEN) != 0;
        bool isMinimized = (overlay.visibility & OVERLAY_MINIMIZED) != 0;

		auto isDC = get(widgetCursor, DATA_ID_FUNCTION_GENERATOR_IS_DC).getInt();
		auto hasModeSelect = get(widgetCursor, DATA_ID_FUNCTION_GENERATOR_HAS_MODE_SELECT).getInt();
		auto hasDutyCycle = get(widgetCursor, DATA_ID_FUNCTION_GENERATOR_HAS_DUTY_CYCLE).getInt();

        int state = 0;
        if (!isHidden && (areListCountersVisible || areRampCountersVisible || isDlogVisible || isScriptVisible || isFunctionGeneratorVisible)) {
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
                state |= 1 << 8;
            }

            if (isScriptVisible) {
                state |= 1 << 9;
            }

            if (isFunctionGeneratorVisible) {
                state |= 1 << 10;
            }

			if (isMinimized) {
				state |= 1 << 11;
			}

			if (isDC) {
				state |= 1 << 12;
			}

			if (hasModeSelect) {
				state |= 1 << 13;
			}

			if (hasDutyCycle) {
				state |= 1 << 14;
			}
		}

        if (overlay.state != state) {
            overlay.state = state;
			if (state > 0) {
				WidgetCursor &widgetCursor = *(WidgetCursor *)value.getVoidPointer();

				auto containerWidget = (ContainerWidget *)widgetCursor.widget;

				const Widget *titleListIconWidget = containerWidget->widgets.item(widgetCursor.assets, TITLE_LIST_ICON_WIDGET);
				const Widget *titleRampIconWidget = containerWidget->widgets.item(widgetCursor.assets, TITLE_RAMP_ICON_WIDGET);
				const Widget *titleDlogIconWidget = containerWidget->widgets.item(widgetCursor.assets, TITLE_DLOG_ICON_WIDGET);
				const Widget *titleMpIconWidget = containerWidget->widgets.item(widgetCursor.assets, TITLE_MP_ICON_WIDGET);
				const Widget *titleFuncgenIconWidget = containerWidget->widgets.item(widgetCursor.assets, TITLE_FUNCGEN_ICON_WIDGET);
				const Widget *titleMinIconWidget = containerWidget->widgets.item(widgetCursor.assets, TITLE_MIN_ICON_WIDGET);
				const Widget *titleMaxIconWidget = containerWidget->widgets.item(widgetCursor.assets, TITLE_MAX_ICON_WIDGET);
				const Widget *titleHideIconWidget = containerWidget->widgets.item(widgetCursor.assets, TITLE_HIDE_ICON_WIDGET);

				const Widget *listIconWidget = containerWidget->widgets.item(widgetCursor.assets, LIST_ICON_WIDGET);
				const Widget *listGridWidget = containerWidget->widgets.item(widgetCursor.assets, LIST_GRID_WIDGET);
				const Widget *rampIconWidget = containerWidget->widgets.item(widgetCursor.assets, RAMP_ICON_WIDGET);
				const Widget *rampGridWidget = containerWidget->widgets.item(widgetCursor.assets, RAMP_GRID_WIDGET);
				const Widget *dlogInfoWidget = containerWidget->widgets.item(widgetCursor.assets, DLOG_INFO_WIDGET);
				const Widget *scriptInfoWidget = containerWidget->widgets.item(widgetCursor.assets, SCRIPT_INFO_WIDGET);
				const Widget *funcgenWidget = containerWidget->widgets.item(widgetCursor.assets, FUNCGEN_WIDGET);

				if (overlay.visibility == OVERLAY_MINIMIZED) {
					auto x = titleListIconWidget->x;

					if (list::g_numChannelsWithVisibleCounters > 0) {
						widgetOverrides[TITLE_LIST_ICON_WIDGET].isVisible = true;
						widgetOverrides[TITLE_LIST_ICON_WIDGET].x = x;
						x += titleListIconWidget->w;
						widgetOverrides[TITLE_LIST_ICON_WIDGET].y = titleListIconWidget->y;
						widgetOverrides[TITLE_LIST_ICON_WIDGET].w = titleListIconWidget->w;
						widgetOverrides[TITLE_LIST_ICON_WIDGET].h = titleListIconWidget->h;

						overlay.width += titleListIconWidget->w;
					} else {
						widgetOverrides[TITLE_LIST_ICON_WIDGET].isVisible = true;
					}

					if (ramp::g_numChannelsWithVisibleCounters > 0) {
						widgetOverrides[TITLE_RAMP_ICON_WIDGET].isVisible = true;
						widgetOverrides[TITLE_RAMP_ICON_WIDGET].x = x;
						x += titleRampIconWidget->w;
						widgetOverrides[TITLE_RAMP_ICON_WIDGET].y = titleRampIconWidget->y;
						widgetOverrides[TITLE_RAMP_ICON_WIDGET].w = titleRampIconWidget->w;
						widgetOverrides[TITLE_RAMP_ICON_WIDGET].h = titleRampIconWidget->h;

						overlay.width += titleRampIconWidget->w;
					} else {
						widgetOverrides[TITLE_RAMP_ICON_WIDGET].isVisible = false;
					}

					if (isDlogVisible) {
						widgetOverrides[TITLE_DLOG_ICON_WIDGET].isVisible = true;
						widgetOverrides[TITLE_DLOG_ICON_WIDGET].x = x;
						x += titleDlogIconWidget->w;
						widgetOverrides[TITLE_DLOG_ICON_WIDGET].y = titleDlogIconWidget->y;
						widgetOverrides[TITLE_DLOG_ICON_WIDGET].w = titleDlogIconWidget->w;
						widgetOverrides[TITLE_DLOG_ICON_WIDGET].h = titleDlogIconWidget->h;

						overlay.width += titleDlogIconWidget->w;
					} else {
						widgetOverrides[TITLE_DLOG_ICON_WIDGET].isVisible = false;
					}

					if (isScriptVisible) {
						widgetOverrides[TITLE_MP_ICON_WIDGET].isVisible = true;
						widgetOverrides[TITLE_MP_ICON_WIDGET].x = x;
						x += titleMpIconWidget->w;
						widgetOverrides[TITLE_MP_ICON_WIDGET].y = titleMpIconWidget->y;
						widgetOverrides[TITLE_MP_ICON_WIDGET].w = titleMpIconWidget->w;
						widgetOverrides[TITLE_MP_ICON_WIDGET].h = titleMpIconWidget->h;

						overlay.width += titleMpIconWidget->w;
					} else {
						widgetOverrides[TITLE_MP_ICON_WIDGET].isVisible = false;
					}

					if (isFunctionGeneratorVisible) {
						widgetOverrides[TITLE_FUNCGEN_ICON_WIDGET].isVisible = true;
						widgetOverrides[TITLE_FUNCGEN_ICON_WIDGET].x = x;
						x += titleFuncgenIconWidget->w;
						widgetOverrides[TITLE_FUNCGEN_ICON_WIDGET].y = titleFuncgenIconWidget->y;
						widgetOverrides[TITLE_FUNCGEN_ICON_WIDGET].w = titleFuncgenIconWidget->w;
						widgetOverrides[TITLE_FUNCGEN_ICON_WIDGET].h = titleFuncgenIconWidget->h;

						overlay.width += titleFuncgenIconWidget->w;
					} else {
						widgetOverrides[TITLE_FUNCGEN_ICON_WIDGET].isVisible = false;
					}

					widgetOverrides[TITLE_MIN_ICON_WIDGET].isVisible = false;

					x += 5;

					widgetOverrides[TITLE_MAX_ICON_WIDGET].isVisible = true;
					widgetOverrides[TITLE_MAX_ICON_WIDGET].x = x;
					x += titleMaxIconWidget->w;
					widgetOverrides[TITLE_MAX_ICON_WIDGET].y = titleMaxIconWidget->y;
					widgetOverrides[TITLE_MAX_ICON_WIDGET].w = titleMaxIconWidget->w;
					widgetOverrides[TITLE_MAX_ICON_WIDGET].h = titleMaxIconWidget->h;

					widgetOverrides[TITLE_HIDE_ICON_WIDGET].isVisible = true;
					widgetOverrides[TITLE_HIDE_ICON_WIDGET].x = x;
					x += titleHideIconWidget->w;
					widgetOverrides[TITLE_HIDE_ICON_WIDGET].y = titleHideIconWidget->y;
					widgetOverrides[TITLE_HIDE_ICON_WIDGET].w = titleHideIconWidget->w;
					widgetOverrides[TITLE_HIDE_ICON_WIDGET].h = titleHideIconWidget->h;

					overlay.width = x + widgetCursor.widget->w - (titleHideIconWidget->x + titleHideIconWidget->w);
					overlay.height = widgetCursor.widget->h - (widgetCursor.widget->h - (funcgenWidget->y + funcgenWidget->h));
				} else {
					widgetOverrides[TITLE_LIST_ICON_WIDGET].isVisible = false;
					widgetOverrides[TITLE_RAMP_ICON_WIDGET].isVisible = false;
					widgetOverrides[TITLE_DLOG_ICON_WIDGET].isVisible = false;
					widgetOverrides[TITLE_MP_ICON_WIDGET].isVisible = false;
					widgetOverrides[TITLE_FUNCGEN_ICON_WIDGET].isVisible = false;

					widgetOverrides[TITLE_MIN_ICON_WIDGET].isVisible = true;
					widgetOverrides[TITLE_MIN_ICON_WIDGET].y = titleMinIconWidget->y;
					widgetOverrides[TITLE_MIN_ICON_WIDGET].w = titleMinIconWidget->w;
					widgetOverrides[TITLE_MIN_ICON_WIDGET].h = titleMinIconWidget->h;

					widgetOverrides[TITLE_MAX_ICON_WIDGET].isVisible = false;

					widgetOverrides[TITLE_HIDE_ICON_WIDGET].isVisible = true;
					widgetOverrides[TITLE_HIDE_ICON_WIDGET].y = titleHideIconWidget->y;
					widgetOverrides[TITLE_HIDE_ICON_WIDGET].w = titleHideIconWidget->w;
					widgetOverrides[TITLE_HIDE_ICON_WIDGET].h = titleHideIconWidget->h;

					overlay.width = widgetCursor.widget->w;
					overlay.height = widgetCursor.widget->h;
				}

				if (!isMinimized && list::g_numChannelsWithVisibleCounters <= 1 && ramp::g_numChannelsWithVisibleCounters <= 1 && !isDlogVisible && !isScriptVisible && !isFunctionGeneratorVisible) {
					overlay.width -= listGridWidget->w / 2;
				}

				if (!isMinimized && list::g_numChannelsWithVisibleCounters > 0) {
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

				if (!isMinimized && ramp::g_numChannelsWithVisibleCounters > 0) {
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

				if (!isMinimized && isDlogVisible) {
					widgetOverrides[DLOG_INFO_WIDGET].isVisible = true;
					widgetOverrides[DLOG_INFO_WIDGET].x = dlogInfoWidget->x;
					widgetOverrides[DLOG_INFO_WIDGET].y = overlay.height - (widgetCursor.widget->h - dlogInfoWidget->y);
					widgetOverrides[DLOG_INFO_WIDGET].w = dlogInfoWidget->w;
					widgetOverrides[DLOG_INFO_WIDGET].h = dlogInfoWidget->h;
				} else {
					widgetOverrides[DLOG_INFO_WIDGET].isVisible = false;
					overlay.height -= dlogInfoWidget->h;
				}

				if (!isMinimized && isScriptVisible) {
					widgetOverrides[SCRIPT_INFO_WIDGET].isVisible = true;
					widgetOverrides[SCRIPT_INFO_WIDGET].x = scriptInfoWidget->x;
					widgetOverrides[SCRIPT_INFO_WIDGET].y = overlay.height - (widgetCursor.widget->h - scriptInfoWidget->y);
					widgetOverrides[SCRIPT_INFO_WIDGET].w = scriptInfoWidget->w;
					widgetOverrides[SCRIPT_INFO_WIDGET].h = scriptInfoWidget->h;
				} else {
					widgetOverrides[SCRIPT_INFO_WIDGET].isVisible = false;
					overlay.height -= scriptInfoWidget->h;
				}

				if (!isMinimized && isFunctionGeneratorVisible) {
					widgetOverrides[FUNCGEN_WIDGET].isVisible = true;
					widgetOverrides[FUNCGEN_WIDGET].x = funcgenWidget->x;
					widgetOverrides[FUNCGEN_WIDGET].y = overlay.height - (widgetCursor.widget->h - funcgenWidget->y);
					widgetOverrides[FUNCGEN_WIDGET].w = funcgenWidget->w;
					widgetOverrides[FUNCGEN_WIDGET].h = funcgenWidget->h;

					if (isDC) {
						overlay.height -= 24;
					}
					if (!hasModeSelect && !hasDutyCycle) {
						overlay.height -= 24;
					}
				} else {
					widgetOverrides[FUNCGEN_WIDGET].isVisible = false;
					overlay.height -= funcgenWidget->h;
				}

				overlay.x = 480 - overlay.width;
				overlay.y = 240 - overlay.height;

				if (!isMinimized) {
					widgetOverrides[TITLE_MIN_ICON_WIDGET].x = overlay.width - (widgetCursor.widget->w - titleMaxIconWidget->x);
					widgetOverrides[TITLE_HIDE_ICON_WIDGET].x = overlay.width - (widgetCursor.widget->w - titleHideIconWidget->x);
				}
			}

            persist_conf::setOverlayPositions(overlay.xOffsetMinimized, overlay.yOffsetMinimized, overlay.xOffsetMaximized, overlay.yOffsetMaximized);
        }

        value = Value(&overlay, VALUE_TYPE_POINTER);
    }
}

void data_nondrag_overlay(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
}

void data_is_show_live_recording(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        auto &recording = dlog_view::getRecording();
        value = &recording == &dlog_record::g_activeRecording;
    }
}

void data_channel_history_values(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    auto cursor = widgetCursor.cursor;
    if (operation == DATA_OPERATION_YT_DATA_GET_GET_VALUE_FUNC) {
        value = ChannelHistory::getChannelHistoryValueFuncs(cursor);
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
                    channel.displayValues[0].type == DISPLAY_VALUE_VOLTAGE ? STYLE_ID_YT_GRAPH_U_DEFAULT : 
                    channel.displayValues[0].type == DISPLAY_VALUE_CURRENT ? STYLE_ID_YT_GRAPH_I_DEFAULT :
                    STYLE_ID_YT_GRAPH_P_DEFAULT, 
                    VALUE_TYPE_UINT16);
            } else {
                value = Value(
                    channel.displayValues[1].type == DISPLAY_VALUE_VOLTAGE ? STYLE_ID_YT_GRAPH_U_DEFAULT :
                    channel.displayValues[1].type == DISPLAY_VALUE_CURRENT ? STYLE_ID_YT_GRAPH_I_DEFAULT :
                    STYLE_ID_YT_GRAPH_P_DEFAULT,
                    VALUE_TYPE_UINT16);
            }
        // }
    } else if (operation == DATA_OPERATION_YT_DATA_GET_MIN) {
        value = getMin(widgetCursor, value.getUInt8() == 0 ? DATA_ID_CHANNEL_DISPLAY_VALUE1 : DATA_ID_CHANNEL_DISPLAY_VALUE2);
    } else if (operation == DATA_OPERATION_YT_DATA_GET_MAX) {
        value = getDisplayValueRange(widgetCursor, value.getUInt8() == 0 ? DATA_ID_CHANNEL_DISPLAY_VALUE1 : DATA_ID_CHANNEL_DISPLAY_VALUE2);
    } else if (operation == DATA_OPERATION_YT_DATA_GET_GRAPH_UPDATE_METHOD) {
        value = Value(psu::persist_conf::devConf.ytGraphUpdateMethod, VALUE_TYPE_UINT8);
    }
}

void data_is_single_page_on_stack(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = getNumPagesOnStack() == 1 ? 1 : 0;
    }
}

void data_script_is_started(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = scripting::isIdle() ? 0 : 1;
    }
}

void data_script_info(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        // get script file name
        const char *p = scripting::g_scriptPath + strlen(scripting::g_scriptPath) - 1;
        while (p >= scripting::g_scriptPath && *p != '/' && *p != '\\') {
            p--;
        }
        value = p + 1;
    }
}

void data_mqtt_enabled(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
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

void data_mqtt_connection_state(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
#if OPTION_ETHERNET
    if (operation == DATA_OPERATION_GET) {
        value = mqtt::g_connectionState + 1;
    }
#endif
}

void data_mqtt_host(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
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
            stringCopy(page->m_host, sizeof(page->m_host), value.getString());
        }
    } else if (operation == DATA_OPERATION_GET_MAX) {
        value = Value(64, VALUE_TYPE_UINT32);
    }
#endif
}

void data_mqtt_port(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
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

void data_mqtt_username(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
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
            stringCopy(page->m_username, sizeof(page->m_username), value.getString());
        }
    } else if (operation == DATA_OPERATION_GET_MAX) {
        value = Value(32, VALUE_TYPE_UINT32);
    }
#endif
}

void data_mqtt_password(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
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
            stringCopy(page->m_password, sizeof(page->m_password), value.getString());
        }
    } else if (operation == DATA_OPERATION_GET_MAX) {
        value = Value(32, VALUE_TYPE_UINT32);
    }
#endif
}

void data_mqtt_period(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
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

void data_progress(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = g_progress;
    }
}

void data_selected_theme(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
	if (operation == DATA_OPERATION_GET) {
		value = getThemeName(psu::persist_conf::devConf.selectedThemeIndex);
	}
}

void data_animations_duration(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = Value(psu::persist_conf::devConf.animationsDuration, UNIT_SECOND);
    }
}

void data_master_info(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = Value(0, VALUE_TYPE_MASTER_INFO);
    }
}

void data_master_info_with_fw_ver(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = Value(0, VALUE_TYPE_MASTER_INFO_WITH_FW_VER);
    }
}

void data_master_test_result(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = Value((int)g_masterTestResult, VALUE_TYPE_TEST_RESULT);
    }
}

void data_master_error_message(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = g_masterErrorMessage;
    }
}

void data_slots(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_COUNT) {
        value = 3;
    }
}

void data_slot_index(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    auto cursor = widgetCursor.cursor;
    if (operation == DATA_OPERATION_GET) {
        value = Value(cursor, VALUE_TYPE_SLOT_INDEX);
    }
}

void data_slot_info(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    auto cursor = widgetCursor.cursor;
    if (operation == DATA_OPERATION_GET) {
        value = Value(cursor != -1 ? cursor : hmi::g_selectedSlotIndex, VALUE_TYPE_SLOT_INFO);
    }
}

void data_slot_info_with_fw_ver(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    auto cursor = widgetCursor.cursor;
    if (operation == DATA_OPERATION_GET) {
        value = Value(cursor | (g_slots[cursor]->firmwareMajorVersion << 8) | (g_slots[cursor]->firmwareMinorVersion << 16), VALUE_TYPE_SLOT_INFO_WITH_FW_VER);
    }
}

void data_slot_test_result(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    auto cursor = widgetCursor.cursor;
    if (operation == DATA_OPERATION_GET) {
        value = Value((int)g_slots[cursor]->getTestResult(), VALUE_TYPE_TEST_RESULT);
    }
}

void data_slot_title_def(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = Value(hmi::g_selectedSlotIndex, VALUE_TYPE_SLOT_TITLE_DEF);
    }
}

void data_slot_title_max(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = Value(hmi::g_selectedSlotIndex, VALUE_TYPE_SLOT_TITLE_MAX);
    }
}

void data_slot_title_settings(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = Value((g_channelIndex << 8) | hmi::g_selectedSlotIndex, VALUE_TYPE_SLOT_TITLE_SETTINGS);
    }
}

void data_slot_short_title(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = Value(hmi::g_selectedSlotIndex, VALUE_TYPE_SLOT_SHORT_TITLE);
    }
}

void data_is_reset_by_iwdg(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
#if defined(EEZ_PLATFORM_STM32)
            value = g_RCC_CSR & RCC_CSR_IWDGRSTF ? 1 : 0;
#else
            value = 0;
#endif
    }
}

void data_can_show_previous_page(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = getPreviousPageId() != PAGE_ID_NONE;
    }
}

void data_async_progress(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
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

void data_alert_message_is_set(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = g_alertMessage.getString() != nullptr;
    }
}

void data_alert_message(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = psu::gui::g_alertMessage;
    } else if (operation == DATA_OPERATION_SET) {
        g_alertMessage = value;
    }
}

void data_alert_message_2(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = g_alertMessage2;
    } else if (operation == DATA_OPERATION_SET) {
        g_alertMessage2 = value;
    }
}

void data_alert_message_3(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = g_alertMessage3;
    } else if (operation == DATA_OPERATION_SET) {
        g_alertMessage3 = value;
    }
}

void data_ramp_and_delay_list(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    auto cursor = widgetCursor.cursor;
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

void data_ramp_and_delay_list_scrollbar_enabled(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = (uint32_t)count(widgetCursor, DATA_ID_RAMP_AND_DELAY_LIST) > ytDataGetPageSize(widgetCursor, DATA_ID_RAMP_AND_DELAY_LIST);
    }
}

void data_channel_ramp_state(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    auto cursor = widgetCursor.cursor;
    if (operation == DATA_OPERATION_GET) {
        auto page = (SysSettingsRampAndDelayPage *)getPage(PAGE_ID_SYS_SETTINGS_RAMP_AND_DELAY);
        if (page) {
            value = page->rampState[cursor];
        }
    }
}

void getRampAndDelayDurationStepValues(Value &value) {
    auto stepValues = value.getStepValues();

    static float values[] = { 0.001f, 0.01f, 0.1f, 1.0f };
    stepValues->values = values;
    stepValues->count = sizeof(values) / sizeof(float);
    stepValues->unit = UNIT_SECOND;

    stepValues->encoderSettings.accelerationEnabled = true;
    stepValues->encoderSettings.range = stepValues->values[0] * 10.0f;
    stepValues->encoderSettings.step = stepValues->values[0];

    stepValues->encoderSettings.mode = edit_mode_step::g_rampAndDelayDurationEncoderMode;

    value = 1;
}

void data_channel_voltage_ramp_duration(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    auto cursor = widgetCursor.cursor;
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
        value = 0;
    } else if (operation == DATA_OPERATION_GET_MIN) {
		auto page = (SysSettingsRampAndDelayPage *)getPage(PAGE_ID_SYS_SETTINGS_RAMP_AND_DELAY);
		if (page) {
			value = MakeValue(Channel::get(cursor).params.U_RAMP_DURATION_MIN_VALUE, UNIT_SECOND);
		} else {
			value = MakeValue(g_channel->params.U_RAMP_DURATION_MIN_VALUE, UNIT_SECOND);
		}
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
        value = 1;
    } else if (operation == DATA_OPERATION_SET_ENCODER_MODE) {
        edit_mode_step::g_rampAndDelayDurationEncoderMode = (EncoderMode)value.getInt();
    }    
}

void data_channel_current_ramp_duration(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    auto cursor = widgetCursor.cursor;
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
        value = 0;
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
        value = 1;
    } else if (operation == DATA_OPERATION_SET_ENCODER_MODE) {
        edit_mode_step::g_rampAndDelayDurationEncoderMode = (EncoderMode)value.getInt();
    }
}

void data_channel_output_delay(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    auto cursor = widgetCursor.cursor;
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
        value = 1;
    } else if (operation == DATA_OPERATION_SET_ENCODER_MODE) {
        edit_mode_step::g_rampAndDelayDurationEncoderMode = (EncoderMode)value.getInt();
    }
}

void data_slot_error_message(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        auto slot = g_slots[hmi::g_selectedSlotIndex];
        if (slot->enabled) {
            if (slot->flashMethod == FLASH_METHOD_NONE || slot->firmwareInstalled) {
                if (bp3c::flash_slave::g_bootloaderMode) {
                    value = "";
                } else {
                    value = "Error";
                }
            } else {
                value = "No firmware";
            }
        } else {
            value = "Disabled";
        }
    }
}

void data_usb_mode(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = g_usbMode;
    }
}

void data_usb_current_mode(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = g_usbMode == USB_MODE_OTG ? g_otgMode : g_usbMode;
    }
}

void data_usb_device_class(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = g_usbDeviceClass;
    }
}

void data_display_test_color_index(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = psu::gui::g_displayTestColorIndex;
    }
}

void data_user_switch_action(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = MakeEnumDefinitionValue(persist_conf::devConf.userSwitchAction, ENUM_DEFINITION_USER_SWITCH_ACTION);
    }
}

void data_ntp_refresh_frequency(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        SysSettingsDateTimePage *page = (SysSettingsDateTimePage *)getPage(PAGE_ID_SYS_SETTINGS_DATE_TIME);
        value = MakeValue((float)page->ntpRefreshFrequency, UNIT_MINUTE);
    } 
}

void data_has_custom_bitmap(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = !!g_customLogo.pixels;
    }
}

void data_custom_bitmap(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET_BITMAP_IMAGE) {
        if (g_customLogo.pixels) {
            value = Value(&g_customLogo, VALUE_TYPE_POINTER);
        }
    }
}

void data_has_multiple_disk_drives(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = fs_driver::getDiskDrivesNum(true) > 1;
    }
}

void data_selected_mass_storage_device(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = Value(g_selectedMassStorageDevice, VALUE_TYPE_MASS_STORAGE_DEVICE_LABEL);
    }
}

void data_module_is_resync_supported(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = g_slots[hmi::g_selectedSlotIndex]->isResyncSupported;
    }
}

void data_ac_mains(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        SysSettingsDateTimePage *page = (SysSettingsDateTimePage *)getPage(PAGE_ID_SYS_SETTINGS_DATE_TIME);
        value = Value(1.0f * page->powerLineFrequency, UNIT_HERTZ);
    }
}

void data_prev_channel_in_max_view_button_enabled(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = CH_NUM > 1 || getNumModules() > 1;
    }
}

void data_next_channel_in_max_view_button_enabled(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = CH_NUM > 1 || getNumModules() > 1;
    }
}

void data_is_toggle_channels_view_mode_enabled(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = CH_NUM > 0 || !persist_conf::isMaxView();
    }
}

} // namespace gui
} // namespace eez

#endif
