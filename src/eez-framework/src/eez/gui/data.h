/*
* EEZ Generic Firmware
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

#pragma once

#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <math.h>
#include <string.h>

#include <eez/core/step_values.h>

#include <eez/core/action.h>
#include <eez/core/unit.h>
#include <eez/core/alloc.h>
#include <eez/core/util.h>
#include <eez/core/value.h>
#include <eez/core/value_types.h>

#include <eez/gui/event.h>

namespace eez {
namespace gui {

////////////////////////////////////////////////////////////////////////////////

class AppContext;
struct WidgetCursor;
struct Style;

////////////////////////////////////////////////////////////////////////////////

struct EnumItem {
    uint16_t value;
    const char *menuLabel;
    const char *widgetLabel;
};

extern const EnumItem *g_enumDefinitions[];

const char *getWidgetLabel(EnumItem *enumDefinition, uint16_t value);

extern WidgetCursor g_widgetCursor;

////////////////////////////////////////////////////////////////////////////////

Value get(const WidgetCursor &widgetCursor, int16_t id);

////////////////////////////////////////////////////////////////////////////////

enum DataOperationEnum {
    DATA_OPERATION_GET,
    DATA_OPERATION_GET_DISPLAY_VALUE_RANGE,
    DATA_OPERATION_GET_EDIT_VALUE,
    DATA_OPERATION_GET_ALLOW_ZERO,
    DATA_OPERATION_GET_MIN,
    DATA_OPERATION_GET_MAX,
    DATA_OPERATION_GET_DEF,
    DATA_OPERATION_GET_PRECISION,
    DATA_OPERATION_GET_LIMIT,
    DATA_OPERATION_GET_NAME,
    DATA_OPERATION_GET_UNIT,
    DATA_OPERATION_GET_IS_CHANNEL_DATA,
    DATA_OPERATION_GET_ENCODER_STEP_VALUES,
	DATA_OPERATION_SET_ENCODER_MODE,
    DATA_OPERATION_GET_ENCODER_RANGE_AND_STEP,
    DATA_OPERATION_GET_FLOAT_LIST_LENGTH,
    DATA_OPERATION_GET_FLOAT_LIST,
    DATA_OPERATION_GET_BITMAP_IMAGE,
	DATA_OPERATION_GET_VALUE,
	DATA_OPERATION_GET_LABEL,
    DATA_OPERATION_GET_OVERLAY_DATA,
    DATA_OPERATION_UPDATE_OVERLAY_DATA,
    DATA_OPERATION_COUNT,
    DATA_OPERATION_GET_CURSOR_VALUE,
    DATA_OPERATION_SELECT,
    DATA_OPERATION_DESELECT,
    DATA_OPERATION_GET_CONTEXT,
    DATA_OPERATION_SET_CONTEXT,
    DATA_OPERATION_GET_CONTEXT_CURSOR,
    DATA_OPERATION_RESTORE_CONTEXT,
    DATA_OPERATION_GET_TEXT_REFRESH_RATE,
    DATA_OPERATION_GET_COLOR,
    DATA_OPERATION_GET_BACKGROUND_COLOR,
    DATA_OPERATION_GET_ACTIVE_COLOR,
    DATA_OPERATION_GET_ACTIVE_BACKGROUND_COLOR,
    DATA_OPERATION_IS_BLINKING,
    DATA_OPERATION_IS_VALID_VALUE,
    DATA_OPERATION_SET,
    DATA_OPERATION_YT_DATA_GET_REFRESH_COUNTER,
    DATA_OPERATION_YT_DATA_GET_SIZE,
    DATA_OPERATION_YT_DATA_GET_POSITION,
    DATA_OPERATION_YT_DATA_SET_POSITION,
    DATA_OPERATION_YT_DATA_GET_POSITION_INCREMENT,
    DATA_OPERATION_YT_DATA_GET_PAGE_SIZE,
    DATA_OPERATION_YT_DATA_GET_STYLE,
    DATA_OPERATION_YT_DATA_GET_MIN,
    DATA_OPERATION_YT_DATA_GET_MAX,
    DATA_OPERATION_YT_DATA_GET_HORZ_DIVISIONS,
    DATA_OPERATION_YT_DATA_GET_VERT_DIVISIONS,
    DATA_OPERATION_YT_DATA_GET_DIV,
    DATA_OPERATION_YT_DATA_GET_OFFSET,
    DATA_OPERATION_YT_DATA_VALUE_IS_VISIBLE,
    DATA_OPERATION_YT_DATA_GET_SHOW_LABELS,
    DATA_OPERATION_YT_DATA_GET_SELECTED_VALUE_INDEX,
    DATA_OPERATION_YT_DATA_GET_LABEL,
    DATA_OPERATION_YT_DATA_GET_GET_VALUE_FUNC,
    DATA_OPERATION_YT_DATA_GET_GRAPH_UPDATE_METHOD,
    DATA_OPERATION_YT_DATA_GET_PERIOD,
	DATA_OPERATION_YT_DATA_GET_BOOKMARKS,
    DATA_OPERATION_YT_DATA_IS_CURSOR_VISIBLE,
    DATA_OPERATION_YT_DATA_GET_CURSOR_OFFSET,
    DATA_OPERATION_YT_DATA_GET_CURSOR_X_VALUE,
    DATA_OPERATION_YT_DATA_TOUCH_DRAG,
    DATA_OPERATION_GET_CANVAS_REFRESH_STATE,
    DATA_OPERATION_GET_TEXT_CURSOR_POSITION,
    DATA_OPERATION_GET_X_SCROLL,
	DATA_OPERATION_GET_SLOT_AND_SUBCHANNEL_INDEX,
	DATA_OPERATION_IS_MICRO_AMPER_ALLOWED,
	DATA_OPERATION_IS_AMPER_ALLOWED
};

int count(const WidgetCursor &widgetCursor, int16_t id);
void select(WidgetCursor &widgetCursor, int16_t id, int index, Value &oldValue);
void deselect(WidgetCursor &widgetCursor, int16_t id, Value &oldValue);

void setContext(WidgetCursor &widgetCursor, int16_t id, Value &oldContext, Value &newContext);
void restoreContext(WidgetCursor &widgetCursor, int16_t id, Value &oldContext);

int getFloatListLength(const WidgetCursor &widgetCursor, int16_t id);
float *getFloatList(const WidgetCursor &widgetCursor, int16_t id);

bool getAllowZero(const WidgetCursor &widgetCursor, int16_t id);
Value getMin(const WidgetCursor &widgetCursor, int16_t id);
Value getMax(const WidgetCursor &widgetCursor, int16_t id);
Value getDef(const WidgetCursor &widgetCursor, int16_t id);
Value getPrecision(const WidgetCursor &widgetCursor, int16_t id);
Value getLimit(const WidgetCursor &widgetCursor, int16_t id);
const char *getName(const WidgetCursor &widgetCursor, int16_t id);
Unit getUnit(const WidgetCursor &widgetCursor, int16_t id);
bool isChannelData(const WidgetCursor &widgetCursor, int16_t id);

void getLabel(const WidgetCursor &widgetCursor, int16_t id, char *text, int count);

void getEncoderStepValues(const WidgetCursor &widgetCursor, int16_t id, StepValues &stepValues);
void setEncoderMode(const WidgetCursor &widgetCursor, int16_t id, EncoderMode encoderMode);

Value get(const WidgetCursor &widgetCursor, int16_t id);
const char *isValidValue(const WidgetCursor &widgetCursor, int16_t id, Value value);
Value set(const WidgetCursor &widgetCursor, int16_t id, Value value);

Value getDisplayValueRange(const WidgetCursor &widgetCursor, int16_t id);

uint32_t getTextRefreshRate(const WidgetCursor &widgetCursor, int16_t id);

uint16_t getColor(const WidgetCursor &widgetCursor, int16_t id, const Style *style);
uint16_t getBackgroundColor(const WidgetCursor &widgetCursor, int16_t id, const Style *style);
uint16_t getActiveColor(const WidgetCursor &widgetCursor, int16_t id, const Style *style);
uint16_t getActiveBackgroundColor(const WidgetCursor &widgetCursor, int16_t id, const Style *style);

bool isBlinking(const WidgetCursor &widgetCursor, int16_t id);
Value getEditValue(const WidgetCursor &widgetCursor, int16_t id);

Value getBitmapImage(const WidgetCursor &widgetCursor, int16_t id);

uint32_t ytDataGetRefreshCounter(const WidgetCursor &widgetCursor, int16_t id);
uint32_t ytDataGetSize(const WidgetCursor &widgetCursor, int16_t id);
uint32_t ytDataGetPosition(const WidgetCursor &widgetCursor, int16_t id);
void ytDataSetPosition(const WidgetCursor &widgetCursor, int16_t id, uint32_t newPosition);
uint32_t ytDataGetPositionIncrement(const WidgetCursor &widgetCursor, int16_t id);
uint32_t ytDataGetPageSize(const WidgetCursor &widgetCursor, int16_t id);
const Style *ytDataGetStyle(const WidgetCursor &widgetCursor, int16_t id, uint8_t valueIndex);
Value ytDataGetMin(const WidgetCursor &widgetCursor, int16_t id, uint8_t valueIndex);
Value ytDataGetMax(const WidgetCursor &widgetCursor, int16_t id, uint8_t valueIndex);
int ytDataGetVertDivisions(const WidgetCursor &widgetCursor, int16_t id);
int ytDataGetHorzDivisions(const WidgetCursor &widgetCursor, int16_t id);
float ytDataGetDiv(const WidgetCursor &widgetCursor, int16_t id, uint8_t valueIndex);
float ytDataGetOffset(const WidgetCursor &widgetCursor, int16_t id, uint8_t valueIndex);
bool ytDataDataValueIsVisible(const WidgetCursor &widgetCursor, int16_t id, uint8_t valueIndex);
bool ytDataGetShowLabels(const WidgetCursor &widgetCursor, int16_t id);
int8_t ytDataGetSelectedValueIndex(const WidgetCursor &widgetCursor, int16_t id);
struct YtDataGetLabelParams {
    uint8_t valueIndex;
    char *text;
    int count;
};
void ytDataGetLabel(const WidgetCursor &widgetCursor, int16_t id, uint8_t valueIndex, char *text, int count);
Value::YtDataGetValueFunctionPointer ytDataGetGetValueFunc(const WidgetCursor &widgetCursor, int16_t id);
uint8_t ytDataGetGraphUpdateMethod(const WidgetCursor &widgetCursor, int16_t id);
float ytDataGetPeriod(const WidgetCursor &widgetCursor, int16_t id);
uint8_t *ytDataGetBookmarks(const WidgetCursor &widgetCursor, int16_t id);
bool ytDataIsCursorVisible(const WidgetCursor &widgetCursor, int16_t id);
uint32_t ytDataGetCursorOffset(const WidgetCursor &widgetCursor, int16_t id);
Value ytDataGetCursorXValue(const WidgetCursor &widgetCursor, int16_t id);

struct TouchDrag {
	const WidgetCursor &widgetCursor;
    EventType type;
    int x;
    int y;
};
void ytDataTouchDrag(const WidgetCursor &widgetCursor, int16_t id, TouchDrag *touchDrag);

int getTextCursorPosition(const WidgetCursor &widgetCursor, int16_t id);
int getXScroll(const WidgetCursor &widgetCursor);

void getSlotAndSubchannelIndex(const WidgetCursor &widgetCursor, int16_t id, int &slotIndex, int &subchannelIndex);

bool isMicroAmperAllowed(const WidgetCursor &widgetCursor, int16_t id);
bool isAmperAllowed(const WidgetCursor &widgetCursor, int16_t id);

typedef void (*CanvasDrawFunction)(const WidgetCursor &widgetCursor);
CanvasDrawFunction getCanvasDrawFunction(const WidgetCursor &widgetCursor, int16_t id);
Value getCanvasRefreshState(const WidgetCursor &widgetCursor, int16_t id);

typedef void(*DataOperationsFunction)(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value);
extern DataOperationsFunction g_dataOperationsFunctions[];

} // namespace gui
} // namespace eez
