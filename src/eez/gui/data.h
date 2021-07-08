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

#include <stdint.h>
#include <eez/unit.h>
#include <eez/index.h>
#include <eez/gui/event.h>
#include <eez/value_types.h>

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

struct EnumValue {
    uint16_t enumValue;
    uint16_t enumDefinition;
};

const char *getWidgetLabel(EnumItem *enumDefinition, uint16_t value);

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

typedef uint8_t ValueType;

////////////////////////////////////////////////////////////////////////////////

#define STRING_OPTIONS_FILE_ELLIPSIS (1 << 0)

#define FLOAT_OPTIONS_LESS_THEN (1 << 0)
#define FLOAT_OPTIONS_FIXED_DECIMALS (1 << 1)
#define FLOAT_OPTIONS_GET_NUM_FIXED_DECIMALS(options) (((options) >> 2) & 0b111)
#define FLOAT_OPTIONS_SET_NUM_FIXED_DECIMALS(n) (FLOAT_OPTIONS_FIXED_DECIMALS | ((n & 0b111) << 2))

struct Value {
  public:
    Value() 
        : type_(VALUE_TYPE_NONE), options_(0), unit_(UNIT_UNKNOWN) 
    {
        pVoid_ = 0;
    }

    Value(int value) 
        : type_(VALUE_TYPE_INT), options_(0), unit_(UNIT_UNKNOWN), int_(value) 
    {
    }

    Value(bool value)
        : type_(VALUE_TYPE_INT), options_(0), unit_(UNIT_UNKNOWN), int_(value ? 1 : 0) 
    {
    }
    
    Value(const char *str) 
        : type_(VALUE_TYPE_STR), options_(0), unit_(UNIT_UNKNOWN), str_(str) 
    {
    }
    
    Value(int version, const char *str) 
        : type_(VALUE_TYPE_VERSIONED_STR), options_(0), unit_(version), str_(str) 
    {
    }

    Value(const char *str, ValueType type)
        : type_(type), options_(0), unit_(UNIT_UNKNOWN), str_(str)
    {
    }

    Value(int value, ValueType type)
        : type_(type), options_(0), unit_(UNIT_UNKNOWN), int_(value) 
    {
    }

    Value(int value, ValueType type, uint16_t options)
        : type_(type), options_(options), unit_(UNIT_UNKNOWN), int_(value) 
    {
    }

    Value(uint8_t value, ValueType type)
        : type_(type), options_(0), unit_(UNIT_UNKNOWN), uint8_(value) 
    {
    }

    Value(uint16_t value, ValueType type)
        : type_(type), options_(0), unit_(UNIT_UNKNOWN), uint16_(value) 
    {
    }

    Value(uint32_t value, ValueType type)
        : type_(type), options_(0), unit_(UNIT_UNKNOWN), uint32_(value) 
    {
    }

    Value(float value, Unit unit)
        : type_(VALUE_TYPE_FLOAT), options_(0), unit_(unit), float_(value) 
    {
    }

    Value(float value, Unit unit, uint16_t options)
        : type_(VALUE_TYPE_FLOAT), options_(options), unit_(unit), float_(value) 
    {
    }

    Value(float value, ValueType type)
        : type_(type), options_(0), unit_(UNIT_UNKNOWN), float_(value)
    {
    }

    Value(const char *value, ValueType type, int16_t options)
        : type_(type), options_(options), unit_(UNIT_UNKNOWN), str_(value)
    {
    }

    Value(void *value, ValueType type) 
        : type_(type), pVoid_(value) 
    {
    }

    Value(AppContext *appContext) 
        : type_(VALUE_TYPE_POINTER), pVoid_(appContext) 
    {
    }

    typedef float (*YtDataGetValueFunctionPointer)(uint32_t rowIndex, uint8_t columnIndex, float *max);

    Value(YtDataGetValueFunctionPointer ytDataGetValueFunctionPointer)
        : type_(VALUE_TYPE_YT_DATA_GET_VALUE_FUNCTION_POINTER), pVoid_((void *)ytDataGetValueFunctionPointer)
    {
    }

    bool operator==(const Value &other) const;

    bool operator!=(const Value &other) const {
        return !(*this == other);
    }

    ValueType getType() const {
        return (ValueType)type_;
    }
    bool isFloat() const {
        return type_ == VALUE_TYPE_FLOAT;
    }
    bool isString() {
        return type_ == VALUE_TYPE_STR;
    }

    Unit getUnit() const {
        return (Unit)unit_;
    }

    bool isPico() const;
    bool isNano() const;
    bool isMicro() const;
    bool isMilli() const;
    bool isKilo() const;
    bool isMega() const;

    float getFloat() const {
        return float_;
    }

    int getInt() const;
    int16_t getInt16() const {
        return int16_;
    }
    uint8_t getUInt8() const {
        return uint8_;
    }
    uint16_t getUInt16() const {
        return uint16_;
    }
    uint32_t getUInt32() const {
        return uint32_;
    }
    uint32_t getAssetObjOffset() const {
        return uint32_;
    }

    const char *getString() const {
        return str_;
    }

    const EnumValue &getEnum() const {
        return enum_;
    }

    int16_t getScpiError() const {
        return int16_;
    }

    uint8_t *getPUint8() const {
        return puint8_;
    }
    
    StepValues *getStepValues() const {
        return (StepValues *)pVoid_;
    }

    float *getFloatList() const {
        return pFloat_;
    }

    void *getVoidPointer() const {
        return pVoid_;
    }

    AppContext *getAppContext() const {
        return (AppContext *)pVoid_;
    }

    YtDataGetValueFunctionPointer getYtDataGetValueFunctionPointer() const {
        return (YtDataGetValueFunctionPointer)pVoid_;
    }

    uint8_t getFirstUInt8() const {
        return pairOfUint8_.first;
    }
    uint8_t getSecondUInt8() const {
        return pairOfUint8_.second;
    }

    uint16_t getFirstUInt16() const {
        return pairOfUint16_.first;
    }
    uint16_t getSecondUInt16() const {
        return pairOfUint16_.second;
    }

    int16_t getFirstInt16() const {
        return pairOfInt16_.first;
    }
    int16_t getSecondInt16() const {
        return pairOfInt16_.second;
    }

    void toText(char *text, int count) const;

    uint16_t getOptions() const {
        return options_;
    }

    uint16_t getRangeFrom() {
        return pairOfUint16_.first;
    }

    uint16_t getRangeTo() {
        return pairOfUint16_.second;
    }

  public:
    ValueType type_;
    uint16_t options_;
    uint8_t unit_;
    union {
        int int_;
        int16_t int16_;
        uint8_t uint8_;
        uint16_t uint16_;
        uint32_t uint32_;
        float float_;
        const char *str_;
        EnumValue enum_;
        uint8_t *puint8_;
        float *pFloat_;
        void *pVoid_;
        PairOfUint8Value pairOfUint8_;
        PairOfUint16Value pairOfUint16_;
        PairOfInt16Value pairOfInt16_;
    };
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

typedef int Cursor;

////////////////////////////////////////////////////////////////////////////////

enum DataOperationEnum {
    DATA_OPERATION_GET,
    DATA_OPERATION_GET_DISPLAY_VALUE_RANGE,
    DATA_OPERATION_GET_EDIT_VALUE,
    DATA_OPERATION_GET_ALLOW_ZERO,
    DATA_OPERATION_GET_MIN,
    DATA_OPERATION_GET_MAX,
    DATA_OPERATION_GET_DEF,
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

int count(int16_t id);
void select(Cursor &cursor, int16_t id, int index, Value &oldValue);
void deselect(Cursor &cursor, int16_t id, Value &oldValue);

void setContext(Cursor &cursor, int16_t id, Value &oldContext, Value &newContext);
void restoreContext(Cursor &cursor, int16_t id, Value &oldContext);

int getFloatListLength(int16_t id);
float *getFloatList(int16_t id);

bool getAllowZero(Cursor cursor, int16_t id);
Value getMin(Cursor cursor, int16_t id);
Value getMax(Cursor cursor, int16_t id);
Value getDef(Cursor cursor, int16_t id);
Value getLimit(Cursor cursor, int16_t id);
const char *getName(Cursor cursor, int16_t id);
Unit getUnit(Cursor cursor, int16_t id);
bool isChannelData(Cursor cursor, int16_t id);

void getLabel(Cursor cursor, int16_t id, char *text, int count);

bool getEncoderStepValues(Cursor cursor, int16_t id, StepValues &stepValues);
void setEncoderMode(Cursor cursor, int16_t id, EncoderMode encoderMode);

Value get(Cursor cursor, int16_t id);
const char *isValidValue(Cursor cursor, int16_t id, Value value);
Value set(Cursor cursor, int16_t id, Value value);

Value getDisplayValueRange(Cursor cursor, int16_t id);

uint32_t getTextRefreshRate(Cursor cursor, int16_t id);

uint16_t getColor(Cursor cursor, int16_t id, const Style *style);
uint16_t getBackgroundColor(Cursor cursor, int16_t id, const Style *style);
uint16_t getActiveColor(Cursor cursor, int16_t id, const Style *style);
uint16_t getActiveBackgroundColor(Cursor cursor, int16_t id, const Style *style);

bool isBlinking(const WidgetCursor &widgetCursor, int16_t id);
Value getEditValue(Cursor cursor, int16_t id);

Value getBitmapImage(Cursor cursor, int16_t id);

uint32_t ytDataGetRefreshCounter(Cursor cursor, int16_t id);
uint32_t ytDataGetSize(Cursor cursor, int16_t id);
uint32_t ytDataGetPosition(Cursor cursor, int16_t id);
void ytDataSetPosition(Cursor cursor, int16_t id, uint32_t newPosition);
uint32_t ytDataGetPositionIncrement(Cursor cursor, int16_t id);
uint32_t ytDataGetPageSize(Cursor cursor, int16_t id);
const Style *ytDataGetStyle(Cursor cursor, int16_t id, uint8_t valueIndex);
Value ytDataGetMin(Cursor cursor, int16_t id, uint8_t valueIndex);
Value ytDataGetMax(Cursor cursor, int16_t id, uint8_t valueIndex);
int ytDataGetVertDivisions(Cursor cursor, int16_t id);
int ytDataGetHorzDivisions(Cursor cursor, int16_t id);
float ytDataGetDiv(Cursor cursor, int16_t id, uint8_t valueIndex);
float ytDataGetOffset(Cursor cursor, int16_t id, uint8_t valueIndex);
bool ytDataDataValueIsVisible(Cursor cursor, int16_t id, uint8_t valueIndex);
bool ytDataGetShowLabels(Cursor cursor, int16_t id);
int8_t ytDataGetSelectedValueIndex(Cursor cursor, int16_t id);
struct YtDataGetLabelParams {
    uint8_t valueIndex;
    char *text;
    int count;
};
void ytDataGetLabel(Cursor cursor, int16_t id, uint8_t valueIndex, char *text, int count);
Value::YtDataGetValueFunctionPointer ytDataGetGetValueFunc(Cursor cursor, int16_t id);
uint8_t ytDataGetGraphUpdateMethod(Cursor cursor, int16_t id);
float ytDataGetPeriod(Cursor cursor, int16_t id);
uint8_t *ytDataGetBookmarks(Cursor cursor, int16_t id);
bool ytDataIsCursorVisible(Cursor cursor, int16_t id);
uint32_t ytDataGetCursorOffset(Cursor cursor, int16_t id);
Value ytDataGetCursorXValue(Cursor cursor, int16_t id);

struct TouchDrag {
	const WidgetCursor &widgetCursor;
    EventType type;
    int x;
    int y;
};
void ytDataTouchDrag(Cursor cursor, int16_t id, TouchDrag *touchDrag);

int getTextCursorPosition(Cursor cursor, int16_t id);
int getXScroll(const WidgetCursor &widgetCursor);

void getSlotAndSubchannelIndex(Cursor cursor, int16_t id, int &slotIndex, int &subchannelIndex);

bool isMicroAmperAllowed(Cursor cursor, int16_t id);
bool isAmperAllowed(Cursor cursor, int16_t id);

} // namespace gui
} // namespace eez
