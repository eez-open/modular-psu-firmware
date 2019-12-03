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

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include <eez/gui/data.h>

#include <scpi/scpi.h>

#include <eez/gui/app_context.h>
#include <eez/gui/dialogs.h>
#include <eez/gui/document.h>
#include <eez/gui/gui.h>
#include <eez/system.h>
#include <eez/util.h>
#include <eez/index.h>

// TODO
#include <eez/modules/psu/psu.h>
#include <eez/modules/psu/persist_conf.h>

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

    if (isinf(floatValue)) {
        strcat(text, INF_TEXT);
        return;
    }

    Unit unit = value.getUnit();

    if (floatValue != 0) {
        if (unit == UNIT_VOLT) {
            if (fabs(floatValue) < 1) {
                unit = UNIT_MILLI_VOLT;
                floatValue *= 1000.0f;
            }
        } else if (unit == UNIT_AMPER) {
            if (fabs(floatValue) < 0.001f && fabs(floatValue) != 0.0005f) {
                unit = UNIT_MICRO_AMPER;
                floatValue *= 1000000.0f;
            } else if (fabs(floatValue) < 1) {
                unit = UNIT_MILLI_AMPER;
                floatValue *= 1000.0f;
            }
        } else if (unit == UNIT_WATT) {
            if (fabs(floatValue) < 1) {
                unit = UNIT_MILLI_WATT;
                floatValue *= 1000.0f;
            }
        } else if (unit == UNIT_SECOND) {
            if (fabs(floatValue) < 1) {
                unit = UNIT_MILLI_SECOND;
                floatValue *= 1000.0f;
            }
        }
    }

    if (!isNaN(floatValue)) {
        strcatFloat(text, floatValue);
        removeTrailingZerosFromFloat(text);
        strcat(text, getUnitName(unit));
    } else {
        text[0] = 0;
    }
}

bool compare_STR_value(const Value &a, const Value &b) {
    return strcmp(a.getString(), b.getString()) == 0;
}

void STR_value_to_text(const Value &value, char *text, int count) {
    strncpy(text, value.getString(), count - 1);
    text[count - 1] = 0;
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

bool compare_SCPI_ERROR_value(const Value &a, const Value &b) {
    return a.getInt16() == b.getInt16();
}

void SCPI_ERROR_value_to_text(const Value &value, char *text, int count) {
    strncpy(text, SCPI_ErrorTranslate(value.getInt16()), count - 1);
    text[count - 1] = 0;
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

bool compare_PAGE_INFO_value(const Value &a, const Value &b) {
    return getPageIndexFromValue(a) == getPageIndexFromValue(b) &&
           getNumPagesFromValue(a) == getNumPagesFromValue(b);
}

void PAGE_INFO_value_to_text(const Value &value, char *text, int count) {
    snprintf(text, count - 1, "Page #%d of %d", getPageIndexFromValue(value) + 1,
             getNumPagesFromValue(value));
    text[count - 1] = 0;
}

bool compare_MASTER_INFO_value(const Value &a, const Value &b) {
    return true;
}

void MASTER_INFO_value_to_text(const Value &value, char *text, int count) {
    snprintf(text, count - 1, "%s %s", MCU_NAME, MCU_REVISION);
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
    compare_STR_value,
    compare_ENUM_value,
    compare_SCPI_ERROR_value,
    compare_PERCENTAGE_value,
    compare_SIZE_value, compare_POINTER_value,
    compare_PAGE_INFO_value,
    compare_MASTER_INFO_value,
    compare_SLOT_INFO_value,
    compare_SLOT_INFO2_value,
    compare_TEST_RESULT_value,
    compare_TIME_SECONDS_value,
    compare_YT_DATA_GET_VALUE_FUNCTION_POINTER_value
};

static ValueToTextFunction g_builtInValueToTextFunctions[] = {
    NONE_value_to_text,
    INT_value_to_text,
    UINT8_value_to_text,
    UINT16_value_to_text,
    UINT32_value_to_text,
    FLOAT_value_to_text,
    STR_value_to_text,
    ENUM_value_to_text,
    SCPI_ERROR_value_to_text,
    PERCENTAGE_value_to_text,
    SIZE_value_to_text,
    POINTER_value_to_text,
    PAGE_INFO_value_to_text,
    MASTER_INFO_value_to_text,
    SLOT_INFO_value_to_text,
    SLOT_INFO2_value_to_text,
    TEST_RESULT_value_to_text,
    TIME_SECONDS_value_to_text,
    YT_DATA_GET_VALUE_FUNCTION_POINTER_value_to_text
};

////////////////////////////////////////////////////////////////////////////////

uint8_t getPageIndexFromValue(const Value &value) {
    return value.getFirstUInt8();
}

uint8_t getNumPagesFromValue(const Value &value) {
    return value.getSecondUInt8();
}

////////////////////////////////////////////////////////////////////////////////

Value MakeEnumDefinitionValue(uint8_t enumValue, uint8_t enumDefinition) {
    Value value;
    value.type_ = VALUE_TYPE_ENUM;
    value.enum_.enumValue = enumValue;
    value.enum_.enumDefinition = enumDefinition;
    return value;
}

Value MakeScpiErrorValue(int16_t errorCode) {
    Value value;
    value.int16_ = errorCode;
    value.type_ = VALUE_TYPE_SCPI_ERROR;
    return value;
}

Value MakePageInfoValue(uint8_t pageIndex, uint8_t numPages) {
    Value value;
    value.pairOfUint8_.first = pageIndex;
    value.pairOfUint8_.second = numPages;
    value.type_ = VALUE_TYPE_PAGE_INFO;
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
    if (type_ != other.type_) {
        return false;
    }
    if (type_ < VALUE_TYPE_USER) {
        return g_compareBuiltInValueFunctions[type_](*this, other);
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

bool Value::isMilli() const {
    float floatValue = getFloat();
    Unit unit = getUnit();

    if (floatValue != 0) {
        if (unit == UNIT_VOLT) {
            if (fabs(floatValue) < 1) {
                return true;
            }
        } else if (unit == UNIT_AMPER) {
            if (fabs(floatValue) < 1 && !(fabs(floatValue) < 0.001f && fabs(floatValue) != 0.0005f)) {
                return true;
            }
        } else if (unit == UNIT_WATT) {
            if (fabs(floatValue) < 1) {
                return true;
            }
        } else if (unit == UNIT_SECOND) {
            if (fabs(floatValue) < 1) {
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
            if (fabs(floatValue) < 0.001f && fabs(floatValue) != 0.0005f) {
                return true;
            }
        }
    }

    return false;
}

////////////////////////////////////////////////////////////////////////////////

int count(uint16_t id) {
    Cursor dummyCursor;
    Value countValue = 0;
    g_dataOperationsFunctions[id](data::DATA_OPERATION_COUNT, dummyCursor, countValue);
    return countValue.getInt();
}

void select(Cursor &cursor, uint16_t id, int index, Value &oldValue) {
    cursor.i = index;
    Value indexValue = index;
    g_dataOperationsFunctions[id](data::DATA_OPERATION_SELECT, cursor, indexValue);
    if (index == 0) {
        oldValue = indexValue;
    }
}

void deselect(Cursor &cursor, uint16_t id, Value &oldValue) {
    g_dataOperationsFunctions[id](data::DATA_OPERATION_DESELECT, cursor, oldValue);
}

void setContext(Cursor &cursor, uint16_t id, Value &oldContext, Value &newContext) {
    g_dataOperationsFunctions[id](data::DATA_OPERATION_SET_CONTEXT, cursor, oldContext);
    g_dataOperationsFunctions[id](data::DATA_OPERATION_GET_CONTEXT, cursor, newContext);
}

void restoreContext(Cursor &cursor, uint16_t id, Value &oldContext) {
    g_dataOperationsFunctions[id](data::DATA_OPERATION_RESTORE_CONTEXT, cursor, oldContext);
}

int getFloatListLength(uint16_t id) {
    Cursor dummyCursor;
    Value listLengthValue = 0;
    g_dataOperationsFunctions[id](data::DATA_OPERATION_GET_FLOAT_LIST_LENGTH, dummyCursor,
                                  listLengthValue);
    return listLengthValue.getInt();
}

float *getFloatList(uint16_t id) {
    Cursor dummyCursor;
    Value floatListValue((float *)0);
    g_dataOperationsFunctions[id](data::DATA_OPERATION_GET_FLOAT_LIST, dummyCursor, floatListValue);
    return floatListValue.getFloatList();
}

Value getMin(const Cursor &cursor, uint16_t id) {
    Value value;
    g_dataOperationsFunctions[id](data::DATA_OPERATION_GET_MIN, (Cursor &)cursor, value);
    return value;
}

Value getMax(const Cursor &cursor, uint16_t id) {
    Value value;
    g_dataOperationsFunctions[id](data::DATA_OPERATION_GET_MAX, (Cursor &)cursor, value);
    return value;
}

Value getDef(const Cursor &cursor, uint16_t id) {
    Value value;
    g_dataOperationsFunctions[id](data::DATA_OPERATION_GET_DEF, (Cursor &)cursor, value);
    return value;
}

Value getLimit(const Cursor &cursor, uint16_t id) {
    Value value;
    g_dataOperationsFunctions[id](data::DATA_OPERATION_GET_LIMIT, (Cursor &)cursor, value);
    return value;
}

const char *getName(const Cursor &cursor, uint16_t id) {
    Value value;
    g_dataOperationsFunctions[id](data::DATA_OPERATION_GET_NAME, (Cursor &)cursor, value);
    return value.getString();
}

Unit getUnit(const Cursor &cursor, uint16_t id) {
    Value value;
    g_dataOperationsFunctions[id](data::DATA_OPERATION_GET_UNIT, (Cursor &)cursor, value);
    return (Unit)value.getInt();
}

void getList(const Cursor &cursor, uint16_t id, const Value **values, int &count) {
    Value listValue;
    g_dataOperationsFunctions[id](data::DATA_OPERATION_GET_VALUE_LIST, (Cursor &)cursor, listValue);
    *values = listValue.getValueList();

    Value countValue;
    g_dataOperationsFunctions[id](data::DATA_OPERATION_COUNT, (Cursor &)cursor, countValue);
    count = countValue.getInt();
}

Value get(const Cursor &cursor, uint16_t id) {
    Value value;
    g_dataOperationsFunctions[id](data::DATA_OPERATION_GET, (Cursor &)cursor, value);
    return value;
}

bool set(const Cursor &cursor, uint16_t id, Value value, int16_t *error) {
    g_dataOperationsFunctions[id](data::DATA_OPERATION_SET, (Cursor &)cursor, value);
    if (value.getType() == VALUE_TYPE_SCPI_ERROR) {
        if (error)
            *error = value.getScpiError();
        return false;
    }
    return true;
}

uint16_t getColor(const Cursor &cursor, uint16_t id, const Style *style) {
    Value value((void *)style, VALUE_TYPE_POINTER);
    g_dataOperationsFunctions[id](data::DATA_OPERATION_GET_COLOR, (Cursor &)cursor, value);
    if (value.getType() == VALUE_TYPE_UINT16) {
        return value.getUInt16();
    }
    return style->color;
}

uint16_t getBackgroundColor(const Cursor &cursor, uint16_t id, const Style *style) {
    Value value((void *)style, VALUE_TYPE_POINTER);
    g_dataOperationsFunctions[id](data::DATA_OPERATION_GET_BACKGROUND_COLOR, (Cursor &)cursor, value);
    if (value.getType() == VALUE_TYPE_UINT16) {
        return value.getUInt16();
    }
    return style->background_color;
}

uint16_t getActiveColor(const Cursor &cursor, uint16_t id, const Style *style) {
    Value value((void *)style, VALUE_TYPE_POINTER);
    g_dataOperationsFunctions[id](data::DATA_OPERATION_GET_ACTIVE_COLOR, (Cursor &)cursor, value);
    if (value.getType() == VALUE_TYPE_UINT16) {
        return value.getUInt16();
    }
    return style->active_color;
}

uint16_t getActiveBackgroundColor(const Cursor &cursor, uint16_t id, const Style *style) {
    Value value((void *)style, VALUE_TYPE_POINTER);
    g_dataOperationsFunctions[id](data::DATA_OPERATION_GET_ACTIVE_BACKGROUND_COLOR, (Cursor &)cursor, value);
    if (value.getType() == VALUE_TYPE_UINT16) {
        return value.getUInt16();
    }
    return style->active_background_color;
}

bool isBlinking(const Cursor &cursor, uint16_t id) {
    if (id == DATA_ID_NONE) {
        return false;
    }

    if (g_appContext->isBlinking(cursor, id)) {
        return true;
    }

    Value value;
    g_dataOperationsFunctions[id](data::DATA_OPERATION_IS_BLINKING, (Cursor &)cursor, value);
    return value.getInt() ? true : false;
}

Value getEditValue(const Cursor &cursor, uint16_t id) {
    Value value;
    g_dataOperationsFunctions[id](data::DATA_OPERATION_GET, (Cursor &)cursor, value);
    g_dataOperationsFunctions[id](data::DATA_OPERATION_GET_EDIT_VALUE, (Cursor &)cursor, value);
    return value;
}

uint16_t getBitmapWidth(const Cursor &cursor, uint16_t id) {
    Value value;
    g_dataOperationsFunctions[id](data::DATA_OPERATION_GET_BITMAP_WIDTH, (Cursor &)cursor, value);
    return value.getUInt16();
}

uint16_t getBitmapHeight(const Cursor &cursor, uint16_t id) {
    Value value;
    g_dataOperationsFunctions[id](data::DATA_OPERATION_GET_BITMAP_HEIGHT, (Cursor &)cursor, value);
    return value.getUInt16();
}

Value getBitmapPixels(const Cursor &cursor, uint16_t id) {
    Value value;
    g_dataOperationsFunctions[id](data::DATA_OPERATION_GET_BITMAP_PIXELS, (Cursor &)cursor, value);
    return value;
}

uint32_t ytDataGetRefreshCounter(const Cursor &cursor, uint16_t id) {
    Value value;
    g_dataOperationsFunctions[id](data::DATA_OPERATION_YT_DATA_GET_REFRESH_COUNTER, (Cursor &)cursor, value);
    return value.getUInt32();
}

uint32_t ytDataGetSize(const Cursor &cursor, uint16_t id) {
    Value value;
    g_dataOperationsFunctions[id](data::DATA_OPERATION_YT_DATA_GET_SIZE, (Cursor &)cursor, value);
    return value.getUInt32();

}

uint32_t ytDataGetPosition(const Cursor &cursor, uint16_t id) {
    Value value;
    g_dataOperationsFunctions[id](data::DATA_OPERATION_YT_DATA_GET_POSITION, (Cursor &)cursor, value);
    return value.getUInt32();
}

void ytDataSetPosition(const Cursor &cursor, uint16_t id, uint32_t newPosition) {
	Value value(newPosition, VALUE_TYPE_UINT32);
    g_dataOperationsFunctions[id](data::DATA_OPERATION_YT_DATA_SET_POSITION, (Cursor &)cursor, value);
}

uint32_t ytDataGetPageSize(const Cursor &cursor, uint16_t id) {
    Value value;
    g_dataOperationsFunctions[id](data::DATA_OPERATION_YT_DATA_GET_PAGE_SIZE, (Cursor &)cursor, value);
    return value.getUInt32();
}

const Style *ytDataGetStyle(const Cursor &cursor, uint16_t id, uint8_t valueIndex) {
    Value value(valueIndex, VALUE_TYPE_UINT8);
    g_dataOperationsFunctions[id](data::DATA_OPERATION_YT_DATA_GET_STYLE, (Cursor &)cursor, value);
    return getStyle(value.getUInt16());
}

Value ytDataGetMin(const Cursor &cursor, uint16_t id, uint8_t valueIndex) {
    Value value(valueIndex, VALUE_TYPE_UINT8);
    g_dataOperationsFunctions[id](data::DATA_OPERATION_YT_DATA_GET_MIN, (Cursor &)cursor, value);
    return value;
}

Value ytDataGetMax(const Cursor &cursor, uint16_t id, uint8_t valueIndex) {
    Value value(valueIndex, VALUE_TYPE_UINT8);
    g_dataOperationsFunctions[id](data::DATA_OPERATION_YT_DATA_GET_MAX, (Cursor &)cursor, value);
    return value;
}

int ytDataGetVertDivisions(const Cursor &cursor, uint16_t id) {
    Value value;
    g_dataOperationsFunctions[id](data::DATA_OPERATION_YT_DATA_GET_VERT_DIVISIONS, (Cursor &)cursor, value);
    return value.getInt();
}

int ytDataGetHorzDivisions(const Cursor &cursor, uint16_t id) {
    Value value;
    g_dataOperationsFunctions[id](data::DATA_OPERATION_YT_DATA_GET_HORZ_DIVISIONS, (Cursor &)cursor, value);
    return value.getInt();
}

float ytDataGetDiv(const Cursor &cursor, uint16_t id, uint8_t valueIndex) {
    Value value(valueIndex, VALUE_TYPE_UINT8);
    g_dataOperationsFunctions[id](data::DATA_OPERATION_YT_DATA_GET_DIV, (Cursor &)cursor, value);
    return value.getFloat();
}

float ytDataGetOffset(const Cursor &cursor, uint16_t id, uint8_t valueIndex) {
    Value value(valueIndex, VALUE_TYPE_UINT8);
    g_dataOperationsFunctions[id](data::DATA_OPERATION_YT_DATA_GET_OFFSET, (Cursor &)cursor, value);
    return value.getFloat();
}

bool ytDataDataValueIsVisible(const Cursor &cursor, uint16_t id, uint8_t valueIndex) {
    Value value(valueIndex, VALUE_TYPE_UINT8);
    g_dataOperationsFunctions[id]((DataOperationEnum)(data::DATA_OPERATION_YT_DATA_VALUE_IS_VISIBLE), (Cursor &)cursor, value);
    return value.getInt();
}

Value::YtDataGetValueFunctionPointer ytDataGetGetValueFunc(const Cursor &cursor, uint16_t id) {
    Value value;
    g_dataOperationsFunctions[id](data::DATA_OPERATION_YT_DATA_GET_GET_VALUE_FUNC, (Cursor &)cursor, value);
    return value.getYtDataGetValueFunctionPointer();
}

uint8_t ytDataGetGraphUpdateMethod(const Cursor &cursor, uint16_t id) {
    Value value;
    g_dataOperationsFunctions[id](data::DATA_OPERATION_YT_DATA_GET_GRAPH_UPDATE_METHOD, (Cursor &)cursor, value);
    return value.getUInt8();
}

float ytDataGetPeriod(const Cursor &cursor, uint16_t id) {
    Value value;
    g_dataOperationsFunctions[id](data::DATA_OPERATION_YT_DATA_GET_PERIOD, (Cursor &)cursor, value);
    return value.getFloat();
}

bool ytDataIsCursorVisible(const Cursor &cursor, uint16_t id) {
    Value value;
    g_dataOperationsFunctions[id](data::DATA_OPERATION_YT_DATA_IS_CURSOR_VISIBLE, (Cursor &)cursor, value);
    return value.getInt() == 1;
}

uint32_t ytDataGetCursorOffset(const Cursor &cursor, uint16_t id) {
    Value value;
    g_dataOperationsFunctions[id](data::DATA_OPERATION_YT_DATA_GET_CURSOR_OFFSET, (Cursor &)cursor, value);
    return value.getUInt32();
}

Value ytDataGetCursorTime(const Cursor &cursor, uint16_t id) {
    Value value;
    g_dataOperationsFunctions[id](data::DATA_OPERATION_YT_DATA_GET_CURSOR_TIME, (Cursor &)cursor, value);
    return value;
}

void ytDataTouchDrag(const Cursor &cursor, uint16_t id, TouchDrag *touchDrag) {
    Value value = Value(touchDrag, VALUE_TYPE_POINTER);
    g_dataOperationsFunctions[id](data::DATA_OPERATION_YT_DATA_TOUCH_DRAG, (Cursor &)cursor, value);
}

} // namespace data
} // namespace gui
} // namespace eez

namespace eez {
namespace gui {

void data_alert_message(data::DataOperationEnum operation, data::Cursor &cursor,
                        data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        value = g_alertMessage;
    } else if (operation == data::DATA_OPERATION_SET) {
        g_alertMessage = value;
    }
}

void data_alert_message_2(data::DataOperationEnum operation, data::Cursor &cursor,
                          data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        value = g_alertMessage2;
    } else if (operation == data::DATA_OPERATION_SET) {
        g_alertMessage2 = value;
    }
}

void data_alert_message_3(data::DataOperationEnum operation, data::Cursor &cursor,
                          data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        value = g_alertMessage3;
    } else if (operation == data::DATA_OPERATION_SET) {
        g_alertMessage3 = value;
    }
}

void data_progress(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        value = g_progress;
    }
}

void data_async_operation_throbber(data::DataOperationEnum operation, data::Cursor &cursor,
                                   data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        static const char *g_throbber[8] = { "|", "/", "-", "\\", "|", "/", "-", "\\" };
        value = data::Value(g_throbber[(millis() % 1000) / 125]);
    }
}

#if defined(EEZ_PLATFORM_SIMULATOR)

void data_slots(DataOperationEnum operation, Cursor &cursor, Value &value) {
    if (operation == data::DATA_OPERATION_COUNT) {
        value = CH_MAX;
    }
}

#endif

void data_selected_theme(DataOperationEnum operation, Cursor &cursor, Value &value) {
	if (operation == data::DATA_OPERATION_GET) {
		value = getThemeName(psu::persist_conf::devConf.selectedThemeIndex);
	}
}

void data_animations_duration(DataOperationEnum operation, Cursor &cursor, Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        value = data::Value(psu::persist_conf::devConf.animationsDuration, UNIT_SECOND);
    }
}

void data_master_info(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        value = Value(0, VALUE_TYPE_MASTER_INFO);
    }
}

void data_master_test_result(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        value = Value((int)g_masterTestResult, VALUE_TYPE_TEST_RESULT);
    }
}

void data_slot1_info(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        value = Value(0, VALUE_TYPE_SLOT_INFO);
    }
}

void data_slot1_test_result (data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        value = Value((int)psu::Channel::get(0).getTestResult(), VALUE_TYPE_TEST_RESULT);
    }
}

void data_slot2_info(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        value = Value(1, VALUE_TYPE_SLOT_INFO);
    }
}

void data_slot2_test_result(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        value = Value((int)psu::Channel::get(1).getTestResult(), VALUE_TYPE_TEST_RESULT);
    }
}

void data_slot3_info(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        value = Value(2, VALUE_TYPE_SLOT_INFO);
    }
}

void data_slot3_test_result(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        value = Value((int)psu::Channel::get(2).getTestResult(), VALUE_TYPE_TEST_RESULT);
    }
}

} // namespace gui
} // namespace eez

#endif
