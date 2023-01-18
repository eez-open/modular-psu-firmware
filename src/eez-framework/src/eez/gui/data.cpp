/*
 * EEZ Modular Firmware
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

#include <eez/conf-internal.h>

#if EEZ_OPTION_GUI

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>

#include <chrono>
#include <string>
#include <iostream>
#include <sstream>

#include <eez/core/util.h>

#include <eez/gui/gui.h>

namespace eez {
namespace gui {

const char *getWidgetLabel(EnumItem *enumDefinition, uint16_t value) {
    for (int i = 0; enumDefinition[i].menuLabel; i++) {
        if (enumDefinition[i].value == value) {
            if (enumDefinition[i].widgetLabel) {
                return enumDefinition[i].widgetLabel;
            }
            return enumDefinition[i].menuLabel;
        }
    }
    return nullptr;
}

////////////////////////////////////////////////////////////////////////////////

void data_none(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    value = Value();
}

////////////////////////////////////////////////////////////////////////////////

int count(const WidgetCursor &widgetCursor, int16_t id) {
    Value countValue = 0;
    DATA_OPERATION_FUNCTION(id, DATA_OPERATION_COUNT, widgetCursor, countValue);
    return countValue.getInt();
}

void select(WidgetCursor &widgetCursor, int16_t id, int index, Value &oldValue) {
    Value cursorValue = index;
    DATA_OPERATION_FUNCTION(id, DATA_OPERATION_GET_CURSOR_VALUE, widgetCursor, cursorValue);

	widgetCursor.cursor = cursorValue.getInt();

    Value indexValue = index;

    DATA_OPERATION_FUNCTION(id, DATA_OPERATION_SELECT, widgetCursor, indexValue);

    if (index == 0) {
        oldValue = indexValue;
    }
}

void deselect(WidgetCursor &widgetCursor, int16_t id, Value &oldValue) {
    DATA_OPERATION_FUNCTION(id, DATA_OPERATION_DESELECT, widgetCursor, oldValue);
}

void setContext(WidgetCursor &widgetCursor, int16_t id, Value &oldContext, Value &newContext) {
    DATA_OPERATION_FUNCTION(id, DATA_OPERATION_SET_CONTEXT, widgetCursor, oldContext);
    DATA_OPERATION_FUNCTION(id, DATA_OPERATION_GET_CONTEXT, widgetCursor, newContext);

    Value cursorValue;
    DATA_OPERATION_FUNCTION(id, DATA_OPERATION_GET_CONTEXT_CURSOR, widgetCursor, cursorValue);
    widgetCursor.cursor = cursorValue.getInt();
}

void restoreContext(WidgetCursor &widgetCursor, int16_t id, Value &oldContext) {
    DATA_OPERATION_FUNCTION(id, DATA_OPERATION_RESTORE_CONTEXT, widgetCursor, oldContext);
}

int getFloatListLength(const WidgetCursor &widgetCursor, int16_t id) {
    Value value;
    DATA_OPERATION_FUNCTION(id, DATA_OPERATION_GET_FLOAT_LIST_LENGTH, widgetCursor, value);
    return value.getInt();
}

float *getFloatList(const WidgetCursor &widgetCursor, int16_t id) {
    Value value;
    DATA_OPERATION_FUNCTION(id, DATA_OPERATION_GET_FLOAT_LIST, widgetCursor, value);
    return value.getFloatList();
}

bool getAllowZero(const WidgetCursor &widgetCursor, int16_t id) {
    Value value;
    DATA_OPERATION_FUNCTION(id, DATA_OPERATION_GET_ALLOW_ZERO, widgetCursor, value);
    return value.getInt() != 0;
}

Value getMin(const WidgetCursor &widgetCursor, int16_t id) {
    Value value;
    DATA_OPERATION_FUNCTION(id, DATA_OPERATION_GET_MIN, widgetCursor, value);
    return value;
}

Value getMax(const WidgetCursor &widgetCursor, int16_t id) {
    Value value;
    DATA_OPERATION_FUNCTION(id, DATA_OPERATION_GET_MAX, widgetCursor, value);
    return value;
}

Value getDef(const WidgetCursor &widgetCursor, int16_t id) {
    Value value;
    DATA_OPERATION_FUNCTION(id, DATA_OPERATION_GET_DEF, widgetCursor, value);
    return value;
}

Value getPrecision(const WidgetCursor &widgetCursor, int16_t id) {
    Value value;
    DATA_OPERATION_FUNCTION(id, DATA_OPERATION_GET_PRECISION, widgetCursor, value);
    return value;
}

Value getLimit(const WidgetCursor &widgetCursor, int16_t id) {
    Value value;
    DATA_OPERATION_FUNCTION(id, DATA_OPERATION_GET_LIMIT, widgetCursor, value);
    return value;
}

const char *getName(const WidgetCursor &widgetCursor, int16_t id) {
    Value value;
    DATA_OPERATION_FUNCTION(id, DATA_OPERATION_GET_NAME, widgetCursor, value);
    return value.getString();
}

Unit getUnit(const WidgetCursor &widgetCursor, int16_t id) {
    Value value;
    DATA_OPERATION_FUNCTION(id, DATA_OPERATION_GET_UNIT, widgetCursor, value);
    return (Unit)value.getInt();
}

bool isChannelData(const WidgetCursor &widgetCursor, int16_t id) {
    Value value;
    DATA_OPERATION_FUNCTION(id, DATA_OPERATION_GET_IS_CHANNEL_DATA, widgetCursor, value);
    return value.getInt() != 0;
}

void getLabel(const WidgetCursor &widgetCursor, int16_t id, char *text, int count) {
    Value value;
    DATA_OPERATION_FUNCTION(id, DATA_OPERATION_GET_LABEL, widgetCursor, value);
    value.toText(text, count);
}

void getEncoderStepValues(const WidgetCursor &widgetCursor, int16_t id, StepValues &stepValues) {
    Value value(&stepValues, VALUE_TYPE_POINTER);
    DATA_OPERATION_FUNCTION(id, DATA_OPERATION_GET_ENCODER_STEP_VALUES, widgetCursor, value);
    if (value.getType() == VALUE_TYPE_INT32 && value.getInt()) {
        if (stepValues.encoderSettings.mode != ENCODER_MODE_AUTO) {
            stepValues.encoderSettings.accelerationEnabled = false;
        }
    } else {
        stepValues.count = 0;
        stepValues.encoderSettings.accelerationEnabled = true;
        stepValues.encoderSettings.mode = ENCODER_MODE_AUTO;
        stepValues.encoderSettings.range = getMax(widgetCursor, id).toFloat() - getMin(widgetCursor, id).toFloat();
        Value precisionValue = getPrecision(widgetCursor, id);
        stepValues.encoderSettings.step = precisionValue.toFloat();
    }
}

void setEncoderMode(const WidgetCursor &widgetCursor, int16_t id, EncoderMode encoderMode) {
	Value value(encoderMode, VALUE_TYPE_INT32);
	DATA_OPERATION_FUNCTION(id, DATA_OPERATION_SET_ENCODER_MODE, widgetCursor, value);
}

Value get(const WidgetCursor &widgetCursor, int16_t id) {
    Value value;
    DATA_OPERATION_FUNCTION(id, DATA_OPERATION_GET, widgetCursor, value);
    return value;
}

const char *isValidValue(const WidgetCursor &widgetCursor, int16_t id, Value value) {
    Value savedValue = value;
    DATA_OPERATION_FUNCTION(id, DATA_OPERATION_IS_VALID_VALUE, widgetCursor, value);
    return value != savedValue ? value.getString() : nullptr;
}

Value set(const WidgetCursor &widgetCursor, int16_t id, Value value) {
    DATA_OPERATION_FUNCTION(id, DATA_OPERATION_SET, widgetCursor, value);
    return value;
}

Value getDisplayValueRange(const WidgetCursor &widgetCursor, int16_t id) {
    Value value;
    DATA_OPERATION_FUNCTION(id, DATA_OPERATION_GET_DISPLAY_VALUE_RANGE, widgetCursor, value);
    return value;
}

uint32_t getTextRefreshRate(const WidgetCursor &widgetCursor, int16_t id) {
    Value value;
    DATA_OPERATION_FUNCTION(id, DATA_OPERATION_GET_TEXT_REFRESH_RATE, widgetCursor, value);
    if (value.getType() == VALUE_TYPE_UINT32) {
        return value.getUInt32();
    }
    return 0;
}

uint16_t getColor(const WidgetCursor &widgetCursor, int16_t id, const Style *style) {
    Value value((void *)style, VALUE_TYPE_POINTER);
    DATA_OPERATION_FUNCTION(id, DATA_OPERATION_GET_COLOR, widgetCursor, value);
    if (value.getType() == VALUE_TYPE_UINT16) {
        return value.getUInt16();
    }
    return style->color;
}

uint16_t getBackgroundColor(const WidgetCursor &widgetCursor, int16_t id, const Style *style) {
    Value value((void *)style, VALUE_TYPE_POINTER);
    DATA_OPERATION_FUNCTION(id, DATA_OPERATION_GET_BACKGROUND_COLOR, widgetCursor, value);
    if (value.getType() == VALUE_TYPE_UINT16) {
        return value.getUInt16();
    }
    return style->backgroundColor;
}

uint16_t getActiveColor(const WidgetCursor &widgetCursor, int16_t id, const Style *style) {
    Value value((void *)style, VALUE_TYPE_POINTER);
    DATA_OPERATION_FUNCTION(id, DATA_OPERATION_GET_ACTIVE_COLOR, widgetCursor, value);
    if (value.getType() == VALUE_TYPE_UINT16) {
        return value.getUInt16();
    }
    return style->activeColor;
}

uint16_t getActiveBackgroundColor(const WidgetCursor &widgetCursor, int16_t id, const Style *style) {
    Value value((void *)style, VALUE_TYPE_POINTER);
    DATA_OPERATION_FUNCTION(id, DATA_OPERATION_GET_ACTIVE_BACKGROUND_COLOR, widgetCursor, value);
    if (value.getType() == VALUE_TYPE_UINT16) {
        return value.getUInt16();
    }
    return style->activeBackgroundColor;
}

bool isBlinking(const WidgetCursor &widgetCursor, int16_t id) {
    if (id == DATA_ID_NONE) {
        return false;
    }

    if (widgetCursor.appContext->isBlinking(widgetCursor, id)) {
        return true;
    }

    Value value;
    DATA_OPERATION_FUNCTION(id, DATA_OPERATION_IS_BLINKING, widgetCursor, value);
    return value.getInt() ? true : false;
}

Value getEditValue(const WidgetCursor &widgetCursor, int16_t id) {
    Value value;
    DATA_OPERATION_FUNCTION(id, DATA_OPERATION_GET, widgetCursor, value);
    DATA_OPERATION_FUNCTION(id, DATA_OPERATION_GET_EDIT_VALUE, widgetCursor, value);
    return value;
}

Value getBitmapImage(const WidgetCursor &widgetCursor, int16_t id) {
    Value value;
    DATA_OPERATION_FUNCTION(id, DATA_OPERATION_GET_BITMAP_IMAGE, widgetCursor, value);
    return value;
}

uint32_t ytDataGetRefreshCounter(const WidgetCursor &widgetCursor, int16_t id) {
    Value value;
    DATA_OPERATION_FUNCTION(id, DATA_OPERATION_YT_DATA_GET_REFRESH_COUNTER, widgetCursor, value);
    return value.getUInt32();
}

uint32_t ytDataGetSize(const WidgetCursor &widgetCursor, int16_t id) {
    Value value;
    DATA_OPERATION_FUNCTION(id, DATA_OPERATION_YT_DATA_GET_SIZE, widgetCursor, value);
    return value.getUInt32();

}

uint32_t ytDataGetPosition(const WidgetCursor &widgetCursor, int16_t id) {
    Value value;
    DATA_OPERATION_FUNCTION(id, DATA_OPERATION_YT_DATA_GET_POSITION, widgetCursor, value);
    return value.getUInt32();
}

void ytDataSetPosition(const WidgetCursor &widgetCursor, int16_t id, uint32_t newPosition) {
	Value value(newPosition, VALUE_TYPE_UINT32);
    DATA_OPERATION_FUNCTION(id, DATA_OPERATION_YT_DATA_SET_POSITION, widgetCursor, value);
}

uint32_t ytDataGetPositionIncrement(const WidgetCursor &widgetCursor, int16_t id) {
    Value value;
    DATA_OPERATION_FUNCTION(id, DATA_OPERATION_YT_DATA_GET_POSITION_INCREMENT, widgetCursor, value);
    if (value.getType() == VALUE_TYPE_UINT32) {
        return value.getUInt32();
    }
    return 1;
}

uint32_t ytDataGetPageSize(const WidgetCursor &widgetCursor, int16_t id) {
    Value value;
    DATA_OPERATION_FUNCTION(id, DATA_OPERATION_YT_DATA_GET_PAGE_SIZE, widgetCursor, value);
    return value.getUInt32();
}

const Style *ytDataGetStyle(const WidgetCursor &widgetCursor, int16_t id, uint8_t valueIndex) {
    Value value(valueIndex, VALUE_TYPE_UINT8);
    DATA_OPERATION_FUNCTION(id, DATA_OPERATION_YT_DATA_GET_STYLE, widgetCursor, value);
    return getStyle(value.getUInt16());
}

Value ytDataGetMin(const WidgetCursor &widgetCursor, int16_t id, uint8_t valueIndex) {
    Value value(valueIndex, VALUE_TYPE_UINT8);
    DATA_OPERATION_FUNCTION(id, DATA_OPERATION_YT_DATA_GET_MIN, widgetCursor, value);
    return value;
}

Value ytDataGetMax(const WidgetCursor &widgetCursor, int16_t id, uint8_t valueIndex) {
    Value value(valueIndex, VALUE_TYPE_UINT8);
    DATA_OPERATION_FUNCTION(id, DATA_OPERATION_YT_DATA_GET_MAX, widgetCursor, value);
    return value;
}

int ytDataGetVertDivisions(const WidgetCursor &widgetCursor, int16_t id) {
    Value value;
    DATA_OPERATION_FUNCTION(id, DATA_OPERATION_YT_DATA_GET_VERT_DIVISIONS, widgetCursor, value);
    return value.getInt();
}

int ytDataGetHorzDivisions(const WidgetCursor &widgetCursor, int16_t id) {
    Value value;
    DATA_OPERATION_FUNCTION(id, DATA_OPERATION_YT_DATA_GET_HORZ_DIVISIONS, widgetCursor, value);
    return value.getInt();
}

float ytDataGetDiv(const WidgetCursor &widgetCursor, int16_t id, uint8_t valueIndex) {
    Value value(valueIndex, VALUE_TYPE_UINT8);
    DATA_OPERATION_FUNCTION(id, DATA_OPERATION_YT_DATA_GET_DIV, widgetCursor, value);
    return value.getFloat();
}

float ytDataGetOffset(const WidgetCursor &widgetCursor, int16_t id, uint8_t valueIndex) {
    Value value(valueIndex, VALUE_TYPE_UINT8);
    DATA_OPERATION_FUNCTION(id, DATA_OPERATION_YT_DATA_GET_OFFSET, widgetCursor, value);
    return value.getFloat();
}

bool ytDataDataValueIsVisible(const WidgetCursor &widgetCursor, int16_t id, uint8_t valueIndex) {
    Value value(valueIndex, VALUE_TYPE_UINT8);
    DATA_OPERATION_FUNCTION(id, (DataOperationEnum)(DATA_OPERATION_YT_DATA_VALUE_IS_VISIBLE), widgetCursor, value);
    return value.getInt();
}

bool ytDataGetShowLabels(const WidgetCursor &widgetCursor, int16_t id) {
    Value value;
    DATA_OPERATION_FUNCTION(id, (DataOperationEnum)(DATA_OPERATION_YT_DATA_GET_SHOW_LABELS), widgetCursor, value);
    return value.getInt();
}

int8_t ytDataGetSelectedValueIndex(const WidgetCursor &widgetCursor, int16_t id) {
    Value value;
    DATA_OPERATION_FUNCTION(id, (DataOperationEnum)(DATA_OPERATION_YT_DATA_GET_SELECTED_VALUE_INDEX), widgetCursor, value);
    return (int8_t)value.getInt();
}

void ytDataGetLabel(const WidgetCursor &widgetCursor, int16_t id, uint8_t valueIndex, char *text, int count) {
    text[0] = 0;
    YtDataGetLabelParams params = {
        valueIndex,
        text,
        count
    };
    Value value(&params, VALUE_TYPE_POINTER);
    DATA_OPERATION_FUNCTION(id, (DataOperationEnum)(DATA_OPERATION_YT_DATA_GET_LABEL), widgetCursor, value);
}

Value::YtDataGetValueFunctionPointer ytDataGetGetValueFunc(const WidgetCursor &widgetCursor, int16_t id) {
    Value value;
    DATA_OPERATION_FUNCTION(id, DATA_OPERATION_YT_DATA_GET_GET_VALUE_FUNC, widgetCursor, value);
    return value.getYtDataGetValueFunctionPointer();
}

uint8_t ytDataGetGraphUpdateMethod(const WidgetCursor &widgetCursor, int16_t id) {
    Value value;
    DATA_OPERATION_FUNCTION(id, DATA_OPERATION_YT_DATA_GET_GRAPH_UPDATE_METHOD, widgetCursor, value);
    return value.getUInt8();
}

float ytDataGetPeriod(const WidgetCursor &widgetCursor, int16_t id) {
    Value value;
    DATA_OPERATION_FUNCTION(id, DATA_OPERATION_YT_DATA_GET_PERIOD, widgetCursor, value);
    return value.getFloat();
}

uint8_t *ytDataGetBookmarks(const WidgetCursor &widgetCursor, int16_t id) {
	Value value;
	DATA_OPERATION_FUNCTION(id, DATA_OPERATION_YT_DATA_GET_BOOKMARKS, widgetCursor, value);
    if (value.getType() == VALUE_TYPE_POINTER) {
        return (uint8_t *)value.getVoidPointer();
    }
    return nullptr;
}

bool ytDataIsCursorVisible(const WidgetCursor &widgetCursor, int16_t id) {
    Value value;
    DATA_OPERATION_FUNCTION(id, DATA_OPERATION_YT_DATA_IS_CURSOR_VISIBLE, widgetCursor, value);
    return value.getInt() == 1;
}

uint32_t ytDataGetCursorOffset(const WidgetCursor &widgetCursor, int16_t id) {
    Value value;
    DATA_OPERATION_FUNCTION(id, DATA_OPERATION_YT_DATA_GET_CURSOR_OFFSET, widgetCursor, value);
    return value.getUInt32();
}

Value ytDataGetCursorXValue(const WidgetCursor &widgetCursor, int16_t id) {
    Value value;
    DATA_OPERATION_FUNCTION(id, DATA_OPERATION_YT_DATA_GET_CURSOR_X_VALUE, widgetCursor, value);
    return value;
}

void ytDataTouchDrag(const WidgetCursor &widgetCursor, int16_t id, TouchDrag *touchDrag) {
    Value value = Value(touchDrag, VALUE_TYPE_POINTER);
    DATA_OPERATION_FUNCTION(id, DATA_OPERATION_YT_DATA_TOUCH_DRAG, widgetCursor, value);
}

int getTextCursorPosition(const WidgetCursor &widgetCursor, int16_t id) {
    Value value;
    DATA_OPERATION_FUNCTION(id, DATA_OPERATION_GET_TEXT_CURSOR_POSITION, widgetCursor, value);
    return value.getType() == VALUE_TYPE_INT32 ? value.getInt() : -1;
}

int getXScroll(const WidgetCursor &widgetCursor) {
    const Widget *widget = widgetCursor.widget;
    Value value((void *)&widgetCursor, VALUE_TYPE_POINTER);
    DATA_OPERATION_FUNCTION(widget->data, DATA_OPERATION_GET_X_SCROLL, widgetCursor, value);
    return value.getType() == VALUE_TYPE_INT32 ? value.getInt() : 0;
}

void getSlotAndSubchannelIndex(const WidgetCursor &widgetCursor, int16_t id, int &slotIndex, int &subchannelIndex) {
	Value value;
	DATA_OPERATION_FUNCTION(id, DATA_OPERATION_GET_SLOT_AND_SUBCHANNEL_INDEX, widgetCursor, value);
	if (value.getType() == VALUE_TYPE_UINT32) {
		slotIndex = value.getFirstInt16();
		subchannelIndex = value.getSecondInt16();
	}
}

bool isMicroAmperAllowed(const WidgetCursor &widgetCursor, int16_t id) {
	Value value;
	DATA_OPERATION_FUNCTION(id, DATA_OPERATION_IS_MICRO_AMPER_ALLOWED, widgetCursor, value);
	return value.getInt() == 1;
}

bool isAmperAllowed(const WidgetCursor &widgetCursor, int16_t id) {
	Value value;
	DATA_OPERATION_FUNCTION(id, DATA_OPERATION_IS_AMPER_ALLOWED, widgetCursor, value);
	return value.getInt() == 1;
}

CanvasDrawFunction getCanvasDrawFunction(const WidgetCursor &widgetCursor, int16_t id) {
    Value value;
    DATA_OPERATION_FUNCTION(id, DATA_OPERATION_GET, widgetCursor, value);
    if (value.getType() == VALUE_TYPE_POINTER) {
        return (CanvasDrawFunction)value.getVoidPointer();
    }
    return nullptr;
}

Value getCanvasRefreshState(const WidgetCursor &widgetCursor, int16_t id) {
    Value value;
    DATA_OPERATION_FUNCTION(id, DATA_OPERATION_GET_CANVAS_REFRESH_STATE, widgetCursor, value);
    return value;
}

} // namespace gui
} // namespace eez

#endif // EEZ_OPTION_GUI
