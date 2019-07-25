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
#include <eez/apps/psu/psu.h>
#include <eez/apps/psu/conf.h> // CH_MAX
#include <eez/apps/psu/persist_conf.h>

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

bool compare_FLOAT_value(const Value &a, const Value &b) {
    if (a.getUnit() != b.getUnit()) {
        return false;
    }

    if (a.getFloat() == b.getFloat()) {
        return true;
    }

    if (a.getUnit() == UNIT_SECOND) {
        return equal(a.getFloat(), b.getFloat(), powf(10.0f, 4));
    }

    float aValue;
    Unit aUnit;
    int aNumSignificantDecimalDigits;
    a.formatFloatValue(aValue, aUnit, aNumSignificantDecimalDigits);

    float bValue;
    Unit bUnit;
    int bNumSignificantDecimalDigits;
    b.formatFloatValue(bValue, bUnit, bNumSignificantDecimalDigits);

    if (aUnit != bUnit) {
        return false;
    }

    if (aNumSignificantDecimalDigits != bNumSignificantDecimalDigits) {
        return false;
    }

    return equal(aValue, bValue,
                 getPrecisionFromNumSignificantDecimalDigits(aNumSignificantDecimalDigits));
}

void FLOAT_value_to_text(const Value &value, char *text, int count) {
    float fValue;
    Unit unit;
    int numSignificantDecimalDigits;

    value.formatFloatValue(fValue, unit, numSignificantDecimalDigits);

    text[0] = 0;

    strcatFloat(text, fValue, numSignificantDecimalDigits);

    if (value.getUnit() == UNIT_SECOND) {
        removeTrailingZerosFromFloat(text);
    }

    strcat(text, getUnitName(unit));
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

////////////////////////////////////////////////////////////////////////////////

static CompareValueFunction g_compareBuiltInValueFunctions[] = {
    compare_NONE_value,       compare_INT_value,  compare_FLOAT_value,
    compare_STR_value,        compare_ENUM_value, compare_SCPI_ERROR_value,
    compare_PERCENTAGE_value, compare_SIZE_value, compare_POINTER_value

};

static ValueToTextFunction g_builtInValueToTextFunctions[] = {
    NONE_value_to_text,       INT_value_to_text,  FLOAT_value_to_text,
    STR_value_to_text,        ENUM_value_to_text, SCPI_ERROR_value_to_text,
    PERCENTAGE_value_to_text, SIZE_value_to_text, POINTER_value_to_text
};

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

bool Value::isMilli() const {
    if (unit_ == UNIT_VOLT || unit_ == UNIT_AMPER || unit_ == UNIT_WATT || unit_ == UNIT_SECOND) {
        int numSignificantDecimalDigits =
            options_ & VALUE_OPTIONS_NUM_SIGNIFICANT_DECIMAL_DIGITS_MASK;

        float value = float_;
        float min = -1.0f;
        float max = 1.0f;
        float precision = getPrecisionFromNumSignificantDecimalDigits(numSignificantDecimalDigits);

        bool gt = greater(value, min, precision);
        if (!gt) {
            return false;
        }

        bool ls = less(value, max, precision);
        if (!ls) {
            return false;
        }

        return true;
    }
    return false;
}

bool Value::isMicro() const {
    if (unit_ == UNIT_AMPER) {
        int numSignificantDecimalDigits =
            options_ & VALUE_OPTIONS_NUM_SIGNIFICANT_DECIMAL_DIGITS_MASK;

        float value = float_;
        float min = -0.001f;
        float max = 0.001f;
        float precision = getPrecisionFromNumSignificantDecimalDigits(numSignificantDecimalDigits);

        bool gt = greater(value, min, precision);
        if (!gt) {
            return false;
        }

        bool ls = less(value, max, precision);
        if (!ls) {
            return false;
        }

        return true;
    }
    return false;
}


void Value::formatFloatValue(float &value, Unit &unit, int &numSignificantDecimalDigits) const {
    value = float_;
    unit = (Unit)unit_;
    numSignificantDecimalDigits = options_ & VALUE_OPTIONS_NUM_SIGNIFICANT_DECIMAL_DIGITS_MASK;

    if (isMicro()) {
        value *= 1000000.0f;

        numSignificantDecimalDigits -= 6;

        if (numSignificantDecimalDigits == -1) {
            value = roundf(value / 10) * 10;
            numSignificantDecimalDigits = 0;
        } else if (numSignificantDecimalDigits == -2) {
            value = roundf(value / 100) * 100;
            numSignificantDecimalDigits = 0;
        } else if (numSignificantDecimalDigits == -3) {
            value = roundf(value / 1000) * 1000;
            numSignificantDecimalDigits = 0;
        } else if (numSignificantDecimalDigits == -4) {
            value = roundf(value / 10000) * 10000;
            numSignificantDecimalDigits = 0;
        } else if (numSignificantDecimalDigits == -5) {
            value = roundf(value / 100000) * 100000;
            numSignificantDecimalDigits = 0;
        } else if (numSignificantDecimalDigits == -6) {
            value = roundf(value / 1000000) * 1000000;
            numSignificantDecimalDigits = 0;
        }

        unit = UNIT_MICRO_AMPER;
	} else if (isMilli()) {
        value *= 1000.0f;

        numSignificantDecimalDigits -= 3;

        if (numSignificantDecimalDigits == -1) {
            value = roundf(value / 10) * 10;
            numSignificantDecimalDigits = 0;
        } else if (numSignificantDecimalDigits == -2) {
            value = roundf(value / 100) * 100;
            numSignificantDecimalDigits = 0;
        } else if (numSignificantDecimalDigits == -3) {
            value = roundf(value / 1000) * 1000;
            numSignificantDecimalDigits = 0;
        }

        if (unit == UNIT_VOLT) {
            unit = UNIT_MILLI_VOLT;
        } else if (unit == UNIT_AMPER) {
            unit = UNIT_MILLI_AMPER;
        } else if (unit == UNIT_WATT) {
            unit = UNIT_MILLI_WATT;
        } else if (unit == UNIT_SECOND) {
            unit = UNIT_MILLI_SECOND;
        }
    }

    if (!(options_ & VALUE_OPTIONS_EXTENDED_PRECISION)) {
        if (numSignificantDecimalDigits > 3) {
            numSignificantDecimalDigits = 3;
        }

        if (numSignificantDecimalDigits > 2 &&
            greater(value, 9.999f, getPrecisionFromNumSignificantDecimalDigits(3))) {
            numSignificantDecimalDigits = 2;
        }

        if (numSignificantDecimalDigits > 1 &&
            greater(value, 99.99f, getPrecisionFromNumSignificantDecimalDigits(2))) {
            numSignificantDecimalDigits = 1;
        }
    }
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

////////////////////////////////////////////////////////////////////////////////

int count(uint16_t id) {
    Cursor dummyCursor;
    Value countValue = 0;
    g_dataOperationsFunctions[id](data::DATA_OPERATION_COUNT, dummyCursor, countValue);
    return countValue.getInt();
}

void select(Cursor &cursor, uint16_t id, int index) {
    cursor.i = index;
    Value indexValue = index;
    g_dataOperationsFunctions[id](data::DATA_OPERATION_SELECT, cursor, indexValue);
}

void setContext(Cursor &cursor, uint16_t id) {
	Value empty;
    g_dataOperationsFunctions[id](data::DATA_OPERATION_SET_CONTEXT, cursor, empty);
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

ValueType getUnit(const Cursor &cursor, uint16_t id) {
    Value value;
    g_dataOperationsFunctions[id](data::DATA_OPERATION_GET_UNIT, (Cursor &)cursor, value);
    return (ValueType)value.getInt();
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
        value = data::Value(g_throbber[(millis() % 1000) / 125]);
    }
}

#if defined(EEZ_PLATFORM_SIMULATOR)

void data_slots(DataOperationEnum operation, Cursor &cursor, Value &value) {
    if (operation == data::DATA_OPERATION_COUNT) {
        value = CH_MAX;
    }
}

void data_slot_module_type(DataOperationEnum operation, Cursor &cursor, Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        int slotIndex = cursor.i;
        assert(slotIndex >= 0 && slotIndex < CH_MAX);
        value = g_slots[slotIndex].moduleType;
    }
}

#endif

void data_selected_theme(DataOperationEnum operation, Cursor &cursor, Value &value) {
	if (operation == data::DATA_OPERATION_GET) {
		value = g_colorsData->themes.first[psu::persist_conf::devConf2.selectedThemeIndex].name;
	}
}

} // namespace gui
} // namespace eez
