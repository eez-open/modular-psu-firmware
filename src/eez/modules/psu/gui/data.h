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
    ENUM_DEFINITION_IO_PINS_OUTPUT2_FUNCTION,
    ENUM_DEFINITION_DST_RULE,
    ENUM_DEFINITION_DATE_TIME_FORMAT_RULE,
    ENUM_DEFINITION_USER_SWITCH_ACTION,
    ENUM_DEFINITION_FILE_MANAGER_SORT_BY,
#if defined(EEZ_PLATFORM_SIMULATOR)
    ENUM_DEFINITION_MODULE_TYPE,
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
extern EnumItem g_ioPinsOutput2FunctionEnumDefinition[];
extern EnumItem g_dstRuleEnumDefinition[];
extern EnumItem g_dateTimeFormatEnumDefinition[];
extern EnumItem g_userSwitchActionEnumDefinition[];
extern EnumItem g_fileManagerSortByEnumDefinition[];
extern EnumItem g_eventQueueFilterEnumDefinition[];
#if defined(EEZ_PLATFORM_SIMULATOR)
extern EnumItem g_moduleTypeEnumDefinition[];
#endif

Value MakeValue(float value, Unit unit);
Value MakeLessThenMinMessageValue(float float_, const Value &value_);
Value MakeGreaterThenMaxMessageValue(float float_, const Value &value_);
Value MakeScpiErrorValue(int16_t errorCode);

void editValue(int16_t dataId);

Value MakeEventValue(psu::event_queue::Event *e);
void eventValueToText(const Value &value, char *text, int count);
bool compareEventValues(const Value &a, const Value &b);
Value MakeEventMessageValue(int16_t eventId);

} // namespace gui
} // namespace eez