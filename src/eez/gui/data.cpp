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

#if OPTION_DISPLAY

#include <math.h>
#include <stdio.h>
#include <string.h>

#include <eez/util.h>

#include <eez/gui/gui.h>

namespace eez {
namespace gui {
namespace data {

////////////////////////////////////////////////////////////////////////////////

bool compare_NONE_value(const Value &a, const Value &b) {
    return true;
}

void NONE_value_to_text(const Value &value, char *text, int count) {
}

bool compare_INT_value(const Value &a, const Value &b) {
    return a.getInt() == b.getInt();
}

void INT_value_to_text(const Value &value, char *text, int count) {
    strcatInt(text, value.getInt());
}

bool compare_UINT8_value(const Value &a, const Value &b) {
    return a.getUInt8() == b.getUInt8();
}

void UINT8_value_to_text(const Value &value, char *text, int count) {
    strcatUInt32(text, value.getUInt8());
}

bool compare_UINT16_value(const Value &a, const Value &b) {
    return a.getUInt16() == b.getUInt16();
}

void UINT16_value_to_text(const Value &value, char *text, int count) {
    strcatUInt32(text, value.getUInt16());
}

bool compare_UINT32_value(const Value &a, const Value &b) {
    return a.getUInt32() == b.getUInt32();
}

void UINT32_value_to_text(const Value &value, char *text, int count) {
    strcatUInt32(text, value.getUInt32());
}

bool compare_FLOAT_value(const Value &a, const Value &b) {
    return a.getUnit() == b.getUnit() && a.getFloat() == b.getFloat();
}

void FLOAT_value_to_text(const Value &value, char *text, int count) {
    text[0] = 0;

    float floatValue = value.getFloat();

#if defined(INFINITY_SYMBOL)
    if (isinf(floatValue)) {
        strcat(text, INFINITY_SYMBOL);
        return;
    }
#endif

    Unit unit = value.getUnit();

    if (floatValue != 0) {
        if (unit == UNIT_VOLT) {
            if (fabs(floatValue) < 1) {
                unit = UNIT_MILLI_VOLT;
                floatValue *= 1E3f;
            }
        } else if (unit == UNIT_AMPER) {
            if (fabs(floatValue) < 0.001f && fabs(floatValue) != 0.0005f) {
                unit = UNIT_MICRO_AMPER;
                floatValue *= 1E6f;
            } else if (fabs(floatValue) < 1) {
                unit = UNIT_MILLI_AMPER;
                floatValue *= 1E3f;
            }
        } else if (unit == UNIT_WATT) {
            if (fabs(floatValue) < 1) {
                unit = UNIT_MILLI_WATT;
                floatValue *= 1E3f;
            }
        } else if (unit == UNIT_SECOND) {
            if (fabs(floatValue) < 1) {
                unit = UNIT_MILLI_SECOND;
                floatValue *= 1E3f;
            }
        } else if (unit == UNIT_OHM) {
            if (fabs(floatValue) >= 1000000) {
                unit = UNIT_MOHM;
                floatValue *= 1E-6F;
            } else if (fabs(floatValue) >= 1000) {
                unit = UNIT_KOHM;
                floatValue *= 1E-3f;
            }
        } else if (unit == UNIT_FARAD) {
            if (fabs(floatValue) < 1E-9f) {
                unit = UNIT_PICO_FARAD;
                floatValue *= 1E12F;
            } else if (fabs(floatValue) < 1E-6f) {
                unit = UNIT_NANO_FARAD;
                floatValue *= 1E9F;
            } else if (fabs(floatValue) < 1E-3f) {
                unit = UNIT_MICRO_FARAD;
                floatValue *= 1E6f;
            } else if (fabs(floatValue) < 1) {
                unit = UNIT_MILLI_FARAD;
                floatValue *= 1E3f;
            }
        } else if (unit == UNIT_HERTZ) {
            if (fabs(floatValue) >= 1000000) {
                unit = UNIT_MHERTZ;
                floatValue *= 1E-6F;
            } else if (fabs(floatValue) >= 1000) {
                unit = UNIT_KHERTZ;
                floatValue *= 1E-3f;
            } else if (fabs(floatValue) < 1) {
                unit = UNIT_MILLI_HERTZ;
                floatValue *= 1E3f;
            }
        } 
    }

    if (!isNaN(floatValue)) {
        if (unit == UNIT_WATT || unit == UNIT_MILLI_WATT) {
            strcatFloat(text, floatValue, 2);
        } else {
            strcatFloat(text, floatValue);
        }
        removeTrailingZerosFromFloat(text);
        strcat(text, " ");
        strcat(text, getUnitName(unit));
    } else {
        text[0] = 0;
    }
}

bool compare_RANGE_value(const Value &a, const Value &b) {
    return a.getUInt32() == b.getUInt32();
}

void RANGE_value_to_text(const Value &value, char *text, int count) {
    text[0] = 0;
}

bool compare_STR_value(const Value &a, const Value &b) {
    const char *astr = a.getString();
    const char *bstr = b.getString();
    if (!astr && !bstr) {
        return true;
    }
    if ((!astr && bstr) || (astr && !bstr)) {
        return false;
    }
    return strcmp(astr, bstr) == 0;
}

void STR_value_to_text(const Value &value, char *text, int count) {
    const char *str = value.getString();
    if (str) {
        strncpy(text, str, count - 1);
        text[count - 1] = 0;
    } else {
        text[0] = 0;
    }
}

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

bool compare_ENUM_value(const Value &a, const Value &b) {
    return a.getEnum().enumDefinition == b.getEnum().enumDefinition &&
           a.getEnum().enumValue == b.getEnum().enumValue;
}

void ENUM_value_to_text(const Value &value, char *text, int count) {
    const EnumItem *enumDefinition = g_enumDefinitions[value.getEnum().enumDefinition];
    for (int i = 0; enumDefinition[i].menuLabel; ++i) {
        if (value.getEnum().enumValue == enumDefinition[i].value) {
            if (enumDefinition[i].widgetLabel) {
                strncpy(text, enumDefinition[i].widgetLabel, count - 1);
            } else {
                strncpy(text, enumDefinition[i].menuLabel, count - 1);
            }
            break;
        }
    }
}

bool compare_PERCENTAGE_value(const Value &a, const Value &b) {
    return a.getInt() == b.getInt();
}

void PERCENTAGE_value_to_text(const Value &value, char *text, int count) {
    snprintf(text, count - 1, "%d%%", value.getInt());
    text[count - 1] = 0;
}

bool compare_SIZE_value(const Value &a, const Value &b) {
    return a.getUInt32() == b.getUInt32();
}

void SIZE_value_to_text(const Value &value, char *text, int count) {
    snprintf(text, count - 1, "%u", (unsigned int)value.getUInt32());
    text[count - 1] = 0;
}

bool compare_POINTER_value(const Value &a, const Value &b) {
    return a.getVoidPointer() == b.getVoidPointer();
}

void POINTER_value_to_text(const Value &value, char *text, int count) {
    text[0] = 0;
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

bool compare_YT_DATA_GET_VALUE_FUNCTION_POINTER_value(const Value &a, const Value &b) {
    return a.getUInt32() == b.getUInt32();
}

void YT_DATA_GET_VALUE_FUNCTION_POINTER_value_to_text(const Value &value, char *text, int count) {
    text[0] = 0;
}

////////////////////////////////////////////////////////////////////////////////

static CompareValueFunction g_compareBuiltInValueFunctions[] = {
    compare_NONE_value,
    compare_INT_value,
    compare_UINT8_value,
    compare_UINT16_value,
    compare_UINT32_value,
    compare_FLOAT_value,
    compare_RANGE_value,
    compare_STR_value,
    compare_PASSWORD_value,
    compare_ENUM_value,
    compare_PERCENTAGE_value,
    compare_SIZE_value,
    compare_POINTER_value,
    compare_TIME_SECONDS_value,
    compare_YT_DATA_GET_VALUE_FUNCTION_POINTER_value,
};

static ValueToTextFunction g_builtInValueToTextFunctions[] = {
    NONE_value_to_text,
    INT_value_to_text,
    UINT8_value_to_text,
    UINT16_value_to_text,
    UINT32_value_to_text,
    FLOAT_value_to_text,
    RANGE_value_to_text,
    STR_value_to_text,
    PASSWORD_value_to_text,
    ENUM_value_to_text,
    PERCENTAGE_value_to_text,
    SIZE_value_to_text,
    POINTER_value_to_text,
    TIME_SECONDS_value_to_text,
    YT_DATA_GET_VALUE_FUNCTION_POINTER_value_to_text,
};

////////////////////////////////////////////////////////////////////////////////

uint16_t getPageIndexFromValue(const Value &value) {
    return value.getFirstUInt16();
}

uint16_t getNumPagesFromValue(const Value &value) {
    return value.getSecondUInt16();
}

////////////////////////////////////////////////////////////////////////////////

Value MakeRangeValue(uint16_t from, uint16_t to) {
    Value value;
    value.type_ = VALUE_TYPE_RANGE;
    value.uint32_ = (from << 16) | to;
    return value;
}

Value MakeEnumDefinitionValue(uint8_t enumValue, uint8_t enumDefinition) {
    Value value;
    value.type_ = VALUE_TYPE_ENUM;
    value.enum_.enumValue = enumValue;
    value.enum_.enumDefinition = enumDefinition;
    return value;
}

void Value::toText(char *text, int count) const {
    *text = 0;
    if (type_ < VALUE_TYPE_USER) {
        g_builtInValueToTextFunctions[type_](*this, text, count);
    } else {
        g_userValueToTextFunctions[type_ - VALUE_TYPE_USER](*this, text, count);
    }
}

bool Value::operator==(const Value &other) const {
    auto a = this;
    auto b = this->type_;

    if (type_ != other.type_) {
        return false;
    }

    if (type_ < VALUE_TYPE_USER) {
        auto result = g_compareBuiltInValueFunctions[type_](*this, other);

        if ((void *)&a == (void *)&b) {
            osDelay(1);
        }

        return result;
    } else {
        return g_compareUserValueFunctions[type_ - VALUE_TYPE_USER](*this, other);
    }
}

int Value::getInt() const {
    if (type_ == VALUE_TYPE_ENUM) {
        return enum_.enumValue;
    }
    return int_;
}

bool Value::isPico() const {
    float floatValue = getFloat();
    Unit unit = getUnit();

    if (floatValue != 0) {
        if (unit == UNIT_FARAD) {
            if (fabs(floatValue) < 1E-9f) {
                return true;
            }
        }
    }

    return false;
}

bool Value::isNano() const {
    float floatValue = getFloat();
    Unit unit = getUnit();

    if (floatValue != 0) {
        if (unit == UNIT_FARAD) {
            if (fabs(floatValue) < 1E-6f) {
                return true;
            }
        }
    }

    return false;
}

bool Value::isMicro() const {
    float floatValue = getFloat();
    Unit unit = getUnit();

    if (floatValue != 0) {
        if (unit == UNIT_AMPER) {
            if (fabs(floatValue) < 1E-3f && fabs(floatValue) != 0.0005f) {
                return true;
            }
        } else if (unit == UNIT_FARAD) {
            if (fabs(floatValue) < 1E-3f) {
                return true;
            }
        }
    }

    return false;
}

bool Value::isMilli() const {
    float floatValue = getFloat();
    Unit unit = getUnit();

    if (floatValue != 0) {
        if (unit == UNIT_AMPER) {
            if (fabs(floatValue) < 1 && !(fabs(floatValue) < 1E-3f && fabs(floatValue) != 0.0005f)) {
                return true;
            }
        } else if (unit == UNIT_VOLT || unit == UNIT_WATT || unit == UNIT_SECOND || unit == UNIT_FARAD || unit == UNIT_HERTZ) {
            if (fabs(floatValue) < 1) {
                return true;
            }
        }
    }

    return false;
}

bool Value::isKilo() const {
    float floatValue = getFloat();
    Unit unit = getUnit();

    if (floatValue != 0) {
        if (unit == UNIT_OHM || unit == UNIT_HERTZ) {
            if (fabs(floatValue) >= 1E3f) {
                return true;
            }
        }
    }

    return false;
}

bool Value::isMega() const {
    float floatValue = getFloat();
    Unit unit = getUnit();

    if (floatValue != 0) {
        if (unit == UNIT_OHM || unit == UNIT_HERTZ) {
            if (fabs(floatValue) >= 1E6f) {
                return true;
            }
        }
    }

    return false;

}

////////////////////////////////////////////////////////////////////////////////

int count(int16_t id) {
    Cursor dummyCursor(-1);
    Value countValue = 0;
    DATA_OPERATION_FUNCTION(id, data::DATA_OPERATION_COUNT, dummyCursor, countValue);
    return countValue.getInt();
}

void select(Cursor &cursor, int16_t id, int index, Value &oldValue) {
    cursor = index;
    Value indexValue = index;
    DATA_OPERATION_FUNCTION(id, data::DATA_OPERATION_SELECT, cursor, indexValue);
    if (index == 0) {
        oldValue = indexValue;
    }
}

void deselect(Cursor &cursor, int16_t id, Value &oldValue) {
    DATA_OPERATION_FUNCTION(id, data::DATA_OPERATION_DESELECT, cursor, oldValue);
}

void setContext(Cursor &cursor, int16_t id, Value &oldContext, Value &newContext) {
    DATA_OPERATION_FUNCTION(id, data::DATA_OPERATION_SET_CONTEXT, cursor, oldContext);
    DATA_OPERATION_FUNCTION(id, data::DATA_OPERATION_GET_CONTEXT, cursor, newContext);

    Value cursorValue;
    DATA_OPERATION_FUNCTION(id, data::DATA_OPERATION_GET_CONTEXT_CURSOR, cursor, cursorValue);
    cursor = cursorValue.getInt();
}

void restoreContext(Cursor &cursor, int16_t id, Value &oldContext) {
    DATA_OPERATION_FUNCTION(id, data::DATA_OPERATION_RESTORE_CONTEXT, cursor, oldContext);
}

int getFloatListLength(int16_t id) {
    Cursor dummyCursor(-1);
    Value listLengthValue = 0;
    DATA_OPERATION_FUNCTION(id, data::DATA_OPERATION_GET_FLOAT_LIST_LENGTH, dummyCursor,
                                  listLengthValue);
    return listLengthValue.getInt();
}

float *getFloatList(int16_t id) {
    Cursor dummyCursor(-1);
    Value floatListValue((float *)0);
    DATA_OPERATION_FUNCTION(id, data::DATA_OPERATION_GET_FLOAT_LIST, dummyCursor, floatListValue);
    return floatListValue.getFloatList();
}

bool getAllowZero(Cursor cursor, int16_t id) {
    Value value;
    DATA_OPERATION_FUNCTION(id, data::DATA_OPERATION_GET_ALLOW_ZERO, cursor, value);
    return value.getInt() != 0;
}

Value getMin(Cursor cursor, int16_t id) {
    Value value;
    DATA_OPERATION_FUNCTION(id, data::DATA_OPERATION_GET_MIN, cursor, value);
    return value;
}

Value getMax(Cursor cursor, int16_t id) {
    Value value;
    DATA_OPERATION_FUNCTION(id, data::DATA_OPERATION_GET_MAX, cursor, value);
    return value;
}

Value getDef(Cursor cursor, int16_t id) {
    Value value;
    DATA_OPERATION_FUNCTION(id, data::DATA_OPERATION_GET_DEF, cursor, value);
    return value;
}

Value getLimit(Cursor cursor, int16_t id) {
    Value value;
    DATA_OPERATION_FUNCTION(id, data::DATA_OPERATION_GET_LIMIT, cursor, value);
    return value;
}

const char *getName(Cursor cursor, int16_t id) {
    Value value;
    DATA_OPERATION_FUNCTION(id, data::DATA_OPERATION_GET_NAME, cursor, value);
    return value.getString();
}

Unit getUnit(Cursor cursor, int16_t id) {
    Value value;
    DATA_OPERATION_FUNCTION(id, data::DATA_OPERATION_GET_UNIT, cursor, value);
    return (Unit)value.getInt();
}

bool isChannelData(Cursor cursor, int16_t id) {
    Value value;
    DATA_OPERATION_FUNCTION(id, data::DATA_OPERATION_GET_IS_CHANNEL_DATA, cursor, value);
    return value.getInt() != 0;
}

Value getEncoderStep(Cursor cursor, int16_t id) {
    Value value;
    DATA_OPERATION_FUNCTION(id, data::DATA_OPERATION_GET_ENCODER_STEP, cursor, value);
    return value;
}

bool getEncoderStepValues(Cursor cursor, int16_t id, StepValues &stepValues) {
    Value value(&stepValues, VALUE_TYPE_POINTER);
    DATA_OPERATION_FUNCTION(id, data::DATA_OPERATION_GET_ENCODER_STEP_VALUES, cursor, value);
    return value.getType() == VALUE_TYPE_INT && value.getInt();
}

void getList(Cursor cursor, int16_t id, const Value **values, int &count) {
    Value listValue;
    DATA_OPERATION_FUNCTION(id, data::DATA_OPERATION_GET_VALUE_LIST, cursor, listValue);
    *values = listValue.getValueList();

    Value countValue;
    DATA_OPERATION_FUNCTION(id, data::DATA_OPERATION_COUNT, cursor, countValue);
    count = countValue.getInt();
}

Value get(Cursor cursor, int16_t id) {
    Value value;
    DATA_OPERATION_FUNCTION(id, data::DATA_OPERATION_GET, cursor, value);
    return value;
}

Value set(Cursor cursor, int16_t id, Value value) {
    DATA_OPERATION_FUNCTION(id, data::DATA_OPERATION_SET, cursor, value);
    return value;
}

uint32_t getTextRefreshRate(Cursor cursor, int16_t id) {
    Value value;
    DATA_OPERATION_FUNCTION(id, DATA_OPERATION_GET_TEXT_REFRESH_RATE, cursor, value);
    if (value.getType() == VALUE_TYPE_UINT32) {
        return value.getUInt32();
    }
    return 0;
}

uint16_t getColor(Cursor cursor, int16_t id, const Style *style) {
    Value value((void *)style, VALUE_TYPE_POINTER);
    DATA_OPERATION_FUNCTION(id, data::DATA_OPERATION_GET_COLOR, cursor, value);
    if (value.getType() == VALUE_TYPE_UINT16) {
        return value.getUInt16();
    }
    return style->color;
}

uint16_t getBackgroundColor(Cursor cursor, int16_t id, const Style *style) {
    Value value((void *)style, VALUE_TYPE_POINTER);
    DATA_OPERATION_FUNCTION(id, data::DATA_OPERATION_GET_BACKGROUND_COLOR, cursor, value);
    if (value.getType() == VALUE_TYPE_UINT16) {
        return value.getUInt16();
    }
    return style->background_color;
}

uint16_t getActiveColor(Cursor cursor, int16_t id, const Style *style) {
    Value value((void *)style, VALUE_TYPE_POINTER);
    DATA_OPERATION_FUNCTION(id, data::DATA_OPERATION_GET_ACTIVE_COLOR, cursor, value);
    if (value.getType() == VALUE_TYPE_UINT16) {
        return value.getUInt16();
    }
    return style->active_color;
}

uint16_t getActiveBackgroundColor(Cursor cursor, int16_t id, const Style *style) {
    Value value((void *)style, VALUE_TYPE_POINTER);
    DATA_OPERATION_FUNCTION(id, data::DATA_OPERATION_GET_ACTIVE_BACKGROUND_COLOR, cursor, value);
    if (value.getType() == VALUE_TYPE_UINT16) {
        return value.getUInt16();
    }
    return style->active_background_color;
}

bool isBlinking(const WidgetCursor &widgetCursor, int16_t id) {
    if (id == DATA_ID_NONE) {
        return false;
    }

    if (widgetCursor.appContext->isBlinking(widgetCursor.cursor, id)) {
        return true;
    }

    Value value;
    DATA_OPERATION_FUNCTION(id, data::DATA_OPERATION_IS_BLINKING, widgetCursor.cursor, value);
    return value.getInt() ? true : false;
}

Value getEditValue(Cursor cursor, int16_t id) {
    Value value;
    DATA_OPERATION_FUNCTION(id, data::DATA_OPERATION_GET, cursor, value);
    DATA_OPERATION_FUNCTION(id, data::DATA_OPERATION_GET_EDIT_VALUE, cursor, value);
    return value;
}

Value getBitmapImage(Cursor cursor, int16_t id) {
    Value value;
    DATA_OPERATION_FUNCTION(id, data::DATA_OPERATION_GET_BITMAP_IMAGE, cursor, value);
    return value;
}

uint32_t ytDataGetRefreshCounter(Cursor cursor, int16_t id) {
    Value value;
    DATA_OPERATION_FUNCTION(id, data::DATA_OPERATION_YT_DATA_GET_REFRESH_COUNTER, cursor, value);
    return value.getUInt32();
}

uint32_t ytDataGetSize(Cursor cursor, int16_t id) {
    Value value;
    DATA_OPERATION_FUNCTION(id, data::DATA_OPERATION_YT_DATA_GET_SIZE, cursor, value);
    return value.getUInt32();

}

uint32_t ytDataGetPosition(Cursor cursor, int16_t id) {
    Value value;
    DATA_OPERATION_FUNCTION(id, data::DATA_OPERATION_YT_DATA_GET_POSITION, cursor, value);
    return value.getUInt32();
}

void ytDataSetPosition(Cursor cursor, int16_t id, uint32_t newPosition) {
	Value value(newPosition, VALUE_TYPE_UINT32);
    DATA_OPERATION_FUNCTION(id, data::DATA_OPERATION_YT_DATA_SET_POSITION, cursor, value);
}

uint32_t ytDataGetPositionIncrement(Cursor cursor, int16_t id) {
    Value value;
    DATA_OPERATION_FUNCTION(id, data::DATA_OPERATION_YT_DATA_GET_POSITION_INCREMENT, cursor, value);
    if (value.getType() == VALUE_TYPE_UINT32) {
        return value.getUInt32();
    }
    return 1;
}

uint32_t ytDataGetPageSize(Cursor cursor, int16_t id) {
    Value value;
    DATA_OPERATION_FUNCTION(id, data::DATA_OPERATION_YT_DATA_GET_PAGE_SIZE, cursor, value);
    return value.getUInt32();
}

const Style *ytDataGetStyle(Cursor cursor, int16_t id, uint8_t valueIndex) {
    Value value(valueIndex, VALUE_TYPE_UINT8);
    DATA_OPERATION_FUNCTION(id, data::DATA_OPERATION_YT_DATA_GET_STYLE, cursor, value);
    return getStyle(value.getUInt16());
}

Value ytDataGetMin(Cursor cursor, int16_t id, uint8_t valueIndex) {
    Value value(valueIndex, VALUE_TYPE_UINT8);
    DATA_OPERATION_FUNCTION(id, data::DATA_OPERATION_YT_DATA_GET_MIN, cursor, value);
    return value;
}

Value ytDataGetMax(Cursor cursor, int16_t id, uint8_t valueIndex) {
    Value value(valueIndex, VALUE_TYPE_UINT8);
    DATA_OPERATION_FUNCTION(id, data::DATA_OPERATION_YT_DATA_GET_MAX, cursor, value);
    return value;
}

int ytDataGetVertDivisions(Cursor cursor, int16_t id) {
    Value value;
    DATA_OPERATION_FUNCTION(id, data::DATA_OPERATION_YT_DATA_GET_VERT_DIVISIONS, cursor, value);
    return value.getInt();
}

int ytDataGetHorzDivisions(Cursor cursor, int16_t id) {
    Value value;
    DATA_OPERATION_FUNCTION(id, data::DATA_OPERATION_YT_DATA_GET_HORZ_DIVISIONS, cursor, value);
    return value.getInt();
}

float ytDataGetDiv(Cursor cursor, int16_t id, uint8_t valueIndex) {
    Value value(valueIndex, VALUE_TYPE_UINT8);
    DATA_OPERATION_FUNCTION(id, data::DATA_OPERATION_YT_DATA_GET_DIV, cursor, value);
    return value.getFloat();
}

float ytDataGetOffset(Cursor cursor, int16_t id, uint8_t valueIndex) {
    Value value(valueIndex, VALUE_TYPE_UINT8);
    DATA_OPERATION_FUNCTION(id, data::DATA_OPERATION_YT_DATA_GET_OFFSET, cursor, value);
    return value.getFloat();
}

bool ytDataDataValueIsVisible(Cursor cursor, int16_t id, uint8_t valueIndex) {
    Value value(valueIndex, VALUE_TYPE_UINT8);
    DATA_OPERATION_FUNCTION(id, (DataOperationEnum)(data::DATA_OPERATION_YT_DATA_VALUE_IS_VISIBLE), cursor, value);
    return value.getInt();
}

bool ytDataGetShowLabels(Cursor cursor, int16_t id) {
    Value value;
    DATA_OPERATION_FUNCTION(id, (DataOperationEnum)(data::DATA_OPERATION_YT_DATA_GET_SHOW_LABELS), cursor, value);
    return value.getInt();
}

int8_t ytDataGetSelectedValueIndex(Cursor cursor, int16_t id) {
    Value value;
    DATA_OPERATION_FUNCTION(id, (DataOperationEnum)(data::DATA_OPERATION_YT_DATA_GET_SELECTED_VALUE_INDEX), cursor, value);
    return (int8_t)value.getInt();
}

void ytDataGetLabel(Cursor cursor, int16_t id, uint8_t valueIndex, char *text, int count) {
    text[0] = 0;
    YtDataGetLabelParams params = {
        valueIndex,
        text,
        count
    };
    Value value(&params, VALUE_TYPE_POINTER);
    DATA_OPERATION_FUNCTION(id, (DataOperationEnum)(data::DATA_OPERATION_YT_DATA_GET_LABEL), cursor, value);
}

Value::YtDataGetValueFunctionPointer ytDataGetGetValueFunc(Cursor cursor, int16_t id) {
    Value value;
    DATA_OPERATION_FUNCTION(id, data::DATA_OPERATION_YT_DATA_GET_GET_VALUE_FUNC, cursor, value);
    return value.getYtDataGetValueFunctionPointer();
}

uint8_t ytDataGetGraphUpdateMethod(Cursor cursor, int16_t id) {
    Value value;
    DATA_OPERATION_FUNCTION(id, data::DATA_OPERATION_YT_DATA_GET_GRAPH_UPDATE_METHOD, cursor, value);
    return value.getUInt8();
}

float ytDataGetPeriod(Cursor cursor, int16_t id) {
    Value value;
    DATA_OPERATION_FUNCTION(id, data::DATA_OPERATION_YT_DATA_GET_PERIOD, cursor, value);
    return value.getFloat();
}

bool ytDataIsCursorVisible(Cursor cursor, int16_t id) {
    Value value;
    DATA_OPERATION_FUNCTION(id, data::DATA_OPERATION_YT_DATA_IS_CURSOR_VISIBLE, cursor, value);
    return value.getInt() == 1;
}

uint32_t ytDataGetCursorOffset(Cursor cursor, int16_t id) {
    Value value;
    DATA_OPERATION_FUNCTION(id, data::DATA_OPERATION_YT_DATA_GET_CURSOR_OFFSET, cursor, value);
    return value.getUInt32();
}

Value ytDataGetCursorXValue(Cursor cursor, int16_t id) {
    Value value;
    DATA_OPERATION_FUNCTION(id, data::DATA_OPERATION_YT_DATA_GET_CURSOR_X_VALUE, cursor, value);
    return value;
}

void ytDataTouchDrag(Cursor cursor, int16_t id, TouchDrag *touchDrag) {
    Value value = Value(touchDrag, VALUE_TYPE_POINTER);
    DATA_OPERATION_FUNCTION(id, data::DATA_OPERATION_YT_DATA_TOUCH_DRAG, cursor, value);
}

} // namespace data
} // namespace gui
} // namespace eez

#endif
