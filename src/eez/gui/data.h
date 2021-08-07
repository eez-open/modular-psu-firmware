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
#include <stdint.h>
#include <math.h>
#include <memory.h>

#include <eez/unit.h>
#include <eez/index.h>
#include <eez/alloc.h>
#include <eez/util.h>
#include <eez/gui/event.h>
#include <eez/value_types.h>

namespace eez {
namespace gui {

struct Assets;

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

struct EnumValue {
    uint16_t enumValue;
    uint16_t enumDefinition;
};

const char *getWidgetLabel(EnumItem *enumDefinition, uint16_t value);

////////////////////////////////////////////////////////////////////////////////

#define VALUE_OPTIONS_REF (1 << 0)

#define STRING_OPTIONS_FILE_ELLIPSIS (1 << 1)

#define FLOAT_OPTIONS_LESS_THEN (1 << 1)
#define FLOAT_OPTIONS_FIXED_DECIMALS (1 << 2)
#define FLOAT_OPTIONS_GET_NUM_FIXED_DECIMALS(options) (((options) >> 3) & 0b111)
#define FLOAT_OPTIONS_SET_NUM_FIXED_DECIMALS(n) (FLOAT_OPTIONS_FIXED_DECIMALS | ((n & 0b111) << 3))

////////////////////////////////////////////////////////////////////////////////

struct PairOfUint8Value {
    uint8_t first;
    uint8_t second;
};

struct PairOfUint16Value {
    uint16_t first;
    uint16_t second;
};

struct PairOfInt16Value {
    int16_t first;
    int16_t second;
};

struct Ref {
	uint32_t refCounter;
};

////////////////////////////////////////////////////////////////////////////////

struct Value {
  public:
    Value()
        : type(VALUE_TYPE_UNDEFINED), unit(UNIT_UNKNOWN), options(0), pVoidValue(0)
    {
    }

	Value(int value)
        : type(VALUE_TYPE_INT32), unit(UNIT_UNKNOWN), options(0), int32Value(value)
    {
    }

	Value(const char *str)
        : type(VALUE_TYPE_STRING), unit(UNIT_UNKNOWN), options(0), strValue(str)
    {
    }

	Value(int version, const char *str)
        : type(VALUE_TYPE_VERSIONED_STRING), unit(version), options(0), strValue(str)
    {
    }

	Value(Value *pValue)
		: type(VALUE_TYPE_VALUE_PTR), unit(UNIT_UNKNOWN), options(0), pValueValue(pValue) {
	}

    Value(const char *str, ValueType type)
        : type(type), unit(UNIT_UNKNOWN), options(0), strValue(str)
    {
    }

    Value(int value, ValueType type)
        : type(type), unit(UNIT_UNKNOWN), options(0), int32Value(value)
    {
    }

    Value(int value, ValueType type, uint16_t options)
        : type(type), unit(UNIT_UNKNOWN), options(options), int32Value(value)
    {
    }

    Value(int8_t value, ValueType type)
        : type(type), unit(UNIT_UNKNOWN), options(0), int8Value(value)
    {
    }

    Value(uint8_t value, ValueType type)
        : type(type), unit(UNIT_UNKNOWN), options(0), uint8Value(value)
    {
    }

    Value(int16_t value, ValueType type)
        : type(type), unit(UNIT_UNKNOWN), options(0), int16Value(value)
    {
    }

    Value(uint16_t value, ValueType type)
        : type(type), unit(UNIT_UNKNOWN), options(0), uint16Value(value)
    {
    }

    Value(uint32_t value, ValueType type)
        : type(type), unit(UNIT_UNKNOWN), options(0), uint32Value(value)
    {
    }

    Value(int64_t value, ValueType type)
        : type(type), unit(UNIT_UNKNOWN), options(0), int64Value(value)
    {
    }

    Value(uint64_t value, ValueType type)
        : type(type), unit(UNIT_UNKNOWN), options(0), uint64Value(value)
    {
    }

    Value(float value, Unit unit)
        : type(VALUE_TYPE_FLOAT), unit(unit), options(0), floatValue(value)
    {
    }

    Value(float value, Unit unit, uint16_t options)
        : type(VALUE_TYPE_FLOAT), unit(unit), options(options), floatValue(value)
    {
    }

    Value(float value, ValueType type)
        : type(type), unit(UNIT_UNKNOWN), options(0), floatValue(value)
    {
    }

	Value(double value, ValueType type)
		: type(type), unit(UNIT_UNKNOWN), options(0), doubleValue(value) {
	}
	
	Value(const char *value, ValueType type, int16_t options)
        : type(type), unit(UNIT_UNKNOWN), options(options), strValue(value)
    {
    }

    Value(void *value, ValueType type)
        : type(type), unit(UNIT_UNKNOWN), options(0), pVoidValue(value)
    {
    }

    Value(AppContext *appContext)
        : type(VALUE_TYPE_POINTER), unit(UNIT_UNKNOWN), options(0), pVoidValue(appContext)
    {
    }

    typedef float (*YtDataGetValueFunctionPointer)(uint32_t rowIndex, uint8_t columnIndex, float *max);

    Value(YtDataGetValueFunctionPointer ytDataGetValueFunctionPointer)
        : type(VALUE_TYPE_YT_DATA_GET_VALUE_FUNCTION_POINTER), unit(UNIT_UNKNOWN), options(0), pVoidValue((void *)ytDataGetValueFunctionPointer)
    {
    }

	Value(const Value& value)
		: type(VALUE_TYPE_UNDEFINED), unit(UNIT_UNKNOWN), options(0), pVoidValue(0) 
	{
		*this = value;
	}

	~Value() {
		if (options & VALUE_OPTIONS_REF) {
			if (--refValue->refCounter == 0) {
				free(refValue);
			}
		}
	}

	void clear() {
		type = VALUE_TYPE_UNDEFINED;
		unit = UNIT_UNKNOWN;
		options = 0;
		pVoidValue = 0;
	}

    const Value& operator = (const Value &value) {
		if (options & VALUE_OPTIONS_REF) {
			if (--refValue->refCounter == 0) {
				free(refValue);
			}
		}

		type = value.type;
		unit = value.unit;
		options = value.options;
		int64Value = value.int64Value;
		if (options & VALUE_OPTIONS_REF) {
			refValue->refCounter++;
		}

        return *this;
    }

    bool operator==(const Value &other) const;

    bool operator!=(const Value &other) const {
        return !(*this == other);
    }

    ValueType getType() const {
        return (ValueType)type;
    }

	bool isInt32OrLess() const {
		return (type >= VALUE_TYPE_INT8 && type <= VALUE_TYPE_UINT32) || type == VALUE_TYPE_BOOLEAN;
	}

	bool isInt64() const {
		return type == VALUE_TYPE_INT64 || type == VALUE_TYPE_UINT64;
	}

	bool isInt32() const {
		return type == VALUE_TYPE_INT32 || type == VALUE_TYPE_UINT32;
	}

	bool isInt16() const {
		return type == VALUE_TYPE_INT16 || type == VALUE_TYPE_UINT16;
	}

	bool isInt8() const {
		return type == VALUE_TYPE_INT8 || type == VALUE_TYPE_UINT8;
	}

	bool isFloat() const {
        return type == VALUE_TYPE_FLOAT;
    }
    
	bool isDouble() const {
		return type == VALUE_TYPE_DOUBLE;
	}

	bool isBoolean() const {
		return type == VALUE_TYPE_BOOLEAN;
	}

	bool isString() const {
        return type == VALUE_TYPE_STRING;
    }

	bool isAnyStringType() const {
        return type == VALUE_TYPE_STRING || type == VALUE_TYPE_STRING_REF || type == VALUE_TYPE_ASSETS_STRING;
    }

    Unit getUnit() const {
        return (Unit)unit;
    }

	bool getBoolean() const {
		return int32Value;
	}

	int8_t getInt8() const {
		return int8Value;
	}

	uint8_t getUInt8() const {
        return uint8Value;
    }

	int16_t getInt16() const {
		return int16Value;
	}

	uint16_t getUInt16() const {
        return uint16Value;
    }
	
	int32_t getInt32() const {
		return int32Value;
	}
	
	uint32_t getUInt32() const {
        return uint32Value;
    }

	int32_t getInt64() const {
		return int64Value;
	}

	uint32_t getUInt64() const {
        return uint64Value;
    }

	float getFloat() const {
		return floatValue;
	}

	double getDouble() const {
		return doubleValue;
	}

	const char *getString() const;

    //////////

	int getInt() const {
		if (type == VALUE_TYPE_ENUM) {
			return enumValue.enumValue;
		}
		return int32Value;
	}

    const EnumValue &getEnum() const {
        return enumValue;
    }

    int16_t getScpiError() const {
        return int16Value;
    }

    uint8_t *getPUint8() const {
        return puint8Value;
    }
    
    StepValues *getStepValues() const {
        return (StepValues *)pVoidValue;
    }

    float *getFloatList() const {
        return pFloatValue;
    }

    void *getVoidPointer() const {
        return pVoidValue;
    }

    AppContext *getAppContext() const {
        return (AppContext *)pVoidValue;
    }

    YtDataGetValueFunctionPointer getYtDataGetValueFunctionPointer() const {
        return (YtDataGetValueFunctionPointer)pVoidValue;
    }

    uint8_t getFirstUInt8() const {
        return pairOfUint8Value.first;
    }
    uint8_t getSecondUInt8() const {
        return pairOfUint8Value.second;
    }

    uint16_t getFirstUInt16() const {
        return pairOfUint16Value.first;
    }
    uint16_t getSecondUInt16() const {
        return pairOfUint16Value.second;
    }

    int16_t getFirstInt16() const {
        return pairOfInt16Value.first;
    }
    int16_t getSecondInt16() const {
        return pairOfInt16Value.second;
    }

    void toText(char *text, int count) const;

    uint16_t getOptions() const {
        return options;
    }

    uint16_t getRangeFrom() {
        return pairOfUint16Value.first;
    }

    uint16_t getRangeTo() {
        return pairOfUint16Value.second;
    }

	double toDouble(int *err = nullptr) const;
	float toFloat(int *err = nullptr) const;
	int32_t toInt32(int *err = nullptr) const;
	int64_t toInt64(int *err = nullptr) const;
    bool toBool(Assets *assets, int *err = nullptr) const;

	Value toString(Assets *assets, uint32_t id) const;

	static Value makeStringRef(const char *str, size_t len, uint32_t id);
	static Value concatenateString(const char *str1, const char *str2);

	//////////

    bool isPico() const;
    bool isNano() const;
    bool isMicro() const;
    bool isMilli() const;
    bool isKilo() const;
    bool isMega() const;

  public:
	uint8_t type;
	uint8_t unit;
	uint16_t options;
	uint32_t reserved;

    union {
		uint8_t int8Value;
		uint8_t uint8Value;

		PairOfUint8Value pairOfUint8Value;

		int16_t int16Value;
		uint16_t uint16Value;

		PairOfUint16Value pairOfUint16Value;
		PairOfInt16Value pairOfInt16Value;

		EnumValue enumValue;

		uint32_t int32Value;
		uint32_t uint32Value;

		float floatValue;

		uint32_t assetsOffsetValue;

		const char *strValue;

		Ref *refValue;

		uint8_t *puint8Value;
		float *pFloatValue;
		void *pVoidValue;
		Value *pValueValue;

		uint64_t int64Value;
		uint64_t uint64Value;

		double doubleValue;
    };
};

typedef const char AssetsString;

struct AssetsArray {
	uint32_t arraySize;
    uint32_t reserved;
	Value values[10];
};

struct StringRef : public Ref {
	char str[4];
};

struct ArrayRef : public Ref {
	uint32_t arraySize;
	Value values[1];
};

////////////////////////////////////////////////////////////////////////////////

typedef bool (*CompareValueFunction)(const Value &a, const Value &b);
typedef void (*ValueToTextFunction)(const Value &value, char *text, int count);

extern CompareValueFunction g_valueTypeCompareFunctions[];
extern ValueToTextFunction g_valueTypeToTextFunctions[];

////////////////////////////////////////////////////////////////////////////////

uint16_t getPageIndexFromValue(const Value &value);
uint16_t getNumPagesFromValue(const Value &value);

////////////////////////////////////////////////////////////////////////////////

Value MakeRangeValue(uint16_t from, uint16_t to);
Value MakeEnumDefinitionValue(uint8_t enumValue, uint8_t enumDefinition);

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
    DATA_OPERATION_GET_CANVAS_DRAW_FUNCTION,
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

} // namespace gui
} // namespace eez
