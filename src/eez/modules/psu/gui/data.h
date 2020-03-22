#pragma once

#include <eez/gui/data.h>

#include <eez/modules/psu/event_queue.h>

namespace eez {
namespace gui {

#define ENUM_DEFINITIONS \
    ENUM_DEFINITION(CHANNEL_DISPLAY_VALUE) \
    ENUM_DEFINITION(CHANNEL_TRIGGER_MODE) \
    ENUM_DEFINITION(TRIGGER_SOURCE) \
    ENUM_DEFINITION(CHANNEL_CURRENT_RANGE_SELECTION_MODE) \
    ENUM_DEFINITION(CHANNEL_CURRENT_RANGE) \
    ENUM_DEFINITION(CHANNEL_TRIGGER_ON_LIST_STOP) \
    ENUM_DEFINITION(IO_PINS_POLARITY) \
    ENUM_DEFINITION(IO_PINS_INPUT_FUNCTION) \
    ENUM_DEFINITION(IO_PINS_OUTPUT_FUNCTION) \
    ENUM_DEFINITION(IO_PINS_OUTPUT2_FUNCTION) \
    ENUM_DEFINITION(DST_RULE) \
    ENUM_DEFINITION(DATE_TIME_FORMAT) \
    ENUM_DEFINITION(USER_SWITCH_ACTION) \
    ENUM_DEFINITION(FILE_MANAGER_SORT_BY) \
    ENUM_DEFINITION(QUEUE_FILTER) \
    ENUM_DEFINITION(MODULE_TYPE)

#define ENUM_DEFINITION(NAME) ENUM_DEFINITION_##NAME,
enum EnumDefinition {
	ENUM_DEFINITIONS
};
#undef ENUM_DEFINITION

#define ENUM_DEFINITION(NAME) extern EnumItem g_enumDefinition_##NAME[];
ENUM_DEFINITIONS
#undef ENUM_DEFINITION

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