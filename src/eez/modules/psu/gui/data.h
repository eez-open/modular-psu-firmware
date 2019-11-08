#pragma once

#include <eez/gui/data.h>

#include <eez/modules/psu/event_queue.h>

using eez::gui::data::EnumItem;
using eez::gui::data::Value;

namespace eez {
namespace gui {

enum EnumDefinition {
    ENUM_DEFINITION_CHANNEL_DISPLAY_VALUE,
    ENUM_DEFINITION_CHANNEL_TRIGGER_MODE,
    ENUM_DEFINITION_TRIGGER_SOURCE,
    ENUM_DEFINITION_CHANNEL_CURRENT_RANGE_SELECTION_MODE,
    ENUM_DEFINITION_CHANNEL_CURRENT_RANGE,
    ENUM_DEFINITION_CHANNEL_TRIGGER_ON_LIST_STOP,
    ENUM_DEFINITION_IO_PINS_POLARITY,
    ENUM_DEFINITION_IO_PINS_INPUT_FUNCTION,
    ENUM_DEFINITION_IO_PINS_OUTPUT_FUNCTION,
    ENUM_DEFINITION_SERIAL_PARITY,
    ENUM_DEFINITION_DST_RULE
#if defined(EEZ_PLATFORM_SIMULATOR)
    , ENUM_DEFINITION_MODULE_TYPE
#endif
};

extern EnumItem g_channelDisplayValueEnumDefinition[];
extern EnumItem g_channelTriggerModeEnumDefinition[];
extern EnumItem g_triggerSourceEnumDefinition[];
extern EnumItem g_channelCurrentRangeSelectionModeEnumDefinition[];
extern EnumItem g_channelCurrentRangeEnumDefinition[];
extern EnumItem g_channelTriggerOnListStopEnumDefinition[];
extern EnumItem g_ioPinsPolarityEnumDefinition[];
extern EnumItem g_ioPinsInputFunctionEnumDefinition[];
extern EnumItem g_ioPinsOutputFunctionEnumDefinition[];
extern EnumItem g_serialParityEnumDefinition[];
extern EnumItem g_dstRuleEnumDefinition[];

#if defined(EEZ_PLATFORM_SIMULATOR)
extern EnumItem g_moduleTypeEnumDefinition[];
#endif

enum UserValueType {
    VALUE_TYPE_LESS_THEN_MIN_FLOAT = VALUE_TYPE_USER,
    VALUE_TYPE_GREATER_THEN_MAX_FLOAT,
    VALUE_TYPE_CHANNEL_LABEL,
    VALUE_TYPE_CHANNEL_SHORT_LABEL,
    VALUE_TYPE_CHANNEL_BOARD_INFO_LABEL,
    VALUE_TYPE_LESS_THEN_MIN_INT,
    VALUE_TYPE_LESS_THEN_MIN_TIME_ZONE,
    VALUE_TYPE_GREATER_THEN_MAX_INT,
    VALUE_TYPE_GREATER_THEN_MAX_TIME_ZONE,
    VALUE_TYPE_EVENT,
    VALUE_TYPE_ON_TIME_COUNTER,
    VALUE_TYPE_COUNTDOWN,
    VALUE_TYPE_TIME_ZONE,
    VALUE_TYPE_DATE,
    VALUE_TYPE_YEAR,
    VALUE_TYPE_MONTH,
    VALUE_TYPE_DAY,
    VALUE_TYPE_TIME,
    VALUE_TYPE_HOUR,
    VALUE_TYPE_MINUTE,
    VALUE_TYPE_SECOND,
    VALUE_TYPE_USER_PROFILE_LABEL,
    VALUE_TYPE_USER_PROFILE_REMARK,
    VALUE_TYPE_EDIT_INFO,
    VALUE_TYPE_MAC_ADDRESS,
    VALUE_TYPE_IP_ADDRESS,
    VALUE_TYPE_PORT,
    VALUE_TYPE_TEXT_MESSAGE,
    VALUE_TYPE_SERIAL_BAUD_INDEX,
    VALUE_TYPE_VALUE_LIST,
    VALUE_TYPE_FLOAT_LIST,
    VALUE_TYPE_CHANNEL_TITLE,
    VALUE_TYPE_CHANNEL_SHORT_TITLE,
    VALUE_TYPE_CHANNEL_SHORT_TITLE_WITHOUT_TRACKING_ICON,
    VALUE_TYPE_CHANNEL_SHORT_TITLE_WITH_COLON,
    VALUE_TYPE_CHANNEL_LONG_TITLE,
    VALUE_TYPE_DLOG_VALUE_LABEL,
    VALUE_TYPE_DLOG_CURRENT_TIME,
    VALUE_TYPE_DLOG_TIME_DURATION,
    VALUE_TYPE_FILE_LENGTH,
};

Value MakeValue(float value, Unit unit);
Value MakeValueListValue(const Value *values);
Value MakeFloatListValue(float *pFloat);
Value MakeEventValue(eez::psu::event_queue::Event *e);
Value MakeLessThenMinMessageValue(float float_, const Value &value_);
Value MakeGreaterThenMaxMessageValue(float float_, const Value &value_);
Value MakeMacAddressValue(uint8_t *macAddress);

} // namespace gui
} // namespace eez