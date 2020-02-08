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
#include <eez/gui/event.h>

#define INFINITY_TEXT "\x91"

namespace eez {

enum BuiltInValueType {
    VALUE_TYPE_NONE,
    VALUE_TYPE_INT,
    VALUE_TYPE_UINT8,
    VALUE_TYPE_UINT16,
    VALUE_TYPE_UINT32,
    VALUE_TYPE_FLOAT,
    VALUE_TYPE_STR,
    VALUE_TYPE_PASSWORD,
    VALUE_TYPE_ENUM,
    VALUE_TYPE_PERCENTAGE,
    VALUE_TYPE_SIZE,
    VALUE_TYPE_POINTER,
    VALUE_TYPE_TIME_SECONDS,
    VALUE_TYPE_YT_DATA_GET_VALUE_FUNCTION_POINTER,
    VALUE_TYPE_USER,
};

namespace gui {

class AppContext;
struct WidgetCursor;
struct Style;

namespace data {

struct EnumItem {
    uint16_t value;
    const char *menuLabel;
    const char *widgetLabel;
};

extern const data::EnumItem *g_enumDefinitions[];

struct EnumValue {
    uint16_t enumValue;
    uint16_t enumDefinition;
};

struct PairOfUint8Value {
    uint8_t first;
    uint8_t second;
};

typedef uint8_t ValueType;

#define STRING_OPTIONS_FILE_ELLIPSIS 1

struct Value {
  public:
    Value() 
        : type_(VALUE_TYPE_NONE), options_(0), unit_(UNIT_UNKNOWN) 
    {
        uint32_ = 0;
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

    Value(const char *str, ValueType type)
        : type_(type), options_(0), unit_(UNIT_UNKNOWN), str_(str)
    {
    }

    Value(int value, ValueType type)
        : type_(type), options_(0), unit_(UNIT_UNKNOWN), int_(value) 
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

    bool isMilli() const;

    bool isMicro() const;

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
    const Value *getValueList() const {
        return pValue_;
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

    void toText(char *text, int count) const;

    uint16_t getOptions() const {
        return options_;
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
        const Value *pValue_;
        float *pFloat_;
        void *pVoid_;
        PairOfUint8Value pairOfUint8_;
    };
};

Value MakeEnumDefinitionValue(uint8_t enumValue, uint8_t enumDefinition);

typedef bool (*CompareValueFunction)(const Value &a, const Value &b);
typedef void (*ValueToTextFunction)(const Value &value, char *text, int count);

extern CompareValueFunction g_compareUserValueFunctions[];
extern ValueToTextFunction g_userValueToTextFunctions[];

uint8_t getPageIndexFromValue(const Value &value);
uint8_t getNumPagesFromValue(const Value &value);

////////////////////////////////////////////////////////////////////////////////

struct Cursor {
    int i;

    Cursor() {
        i = -1;
    }

    Cursor(int i) {
        this->i = i;
    }

    operator bool() {
        return i != -1;
    }

    bool operator!=(const Cursor &rhs) const {
        return !(*this == rhs);
    }

    bool operator==(const Cursor &rhs) const {
        return i == rhs.i;
    }

    void reset() {
        i = -1;
    }
};

////////////////////////////////////////////////////////////////////////////////

enum DataOperationEnum {
    DATA_OPERATION_GET,
    DATA_OPERATION_GET_EDIT_VALUE,
    DATA_OPERATION_GET_MIN,
    DATA_OPERATION_GET_MAX,
    DATA_OPERATION_GET_DEF,
    DATA_OPERATION_GET_LIMIT,
    DATA_OPERATION_GET_NAME,
    DATA_OPERATION_GET_UNIT,
    DATA_OPERATION_GET_IS_CHANNEL_DATA,
    DATA_OPERATION_GET_ENCODER_STEP,
    DATA_OPERATION_GET_ENCODER_STEP_VALUES,
    DATA_OPERATION_GET_VALUE_LIST,
    DATA_OPERATION_GET_FLOAT_LIST_LENGTH,
    DATA_OPERATION_GET_FLOAT_LIST,
    DATA_OPERATION_GET_BITMAP_WIDTH,
    DATA_OPERATION_GET_BITMAP_HEIGHT,
    DATA_OPERATION_GET_BITMAP_PIXELS,
	DATA_OPERATION_GET_VALUE,
	DATA_OPERATION_GET_LABEL,
    DATA_OPERATION_GET_OVERLAY_DATA,
    DATA_OPERATION_UPDATE_OVERLAY_DATA,
    DATA_OPERATION_COUNT,
    DATA_OPERATION_SELECT,
    DATA_OPERATION_DESELECT,
    DATA_OPERATION_GET_CONTEXT,
    DATA_OPERATION_SET_CONTEXT,
    DATA_OPERATION_RESTORE_CONTEXT,
    DATA_OPERATION_GET_COLOR,
    DATA_OPERATION_GET_BACKGROUND_COLOR,
    DATA_OPERATION_GET_ACTIVE_COLOR,
    DATA_OPERATION_GET_ACTIVE_BACKGROUND_COLOR,
    DATA_OPERATION_IS_BLINKING,
    DATA_OPERATION_SET,
    DATA_OPERATION_YT_DATA_GET_REFRESH_COUNTER,
    DATA_OPERATION_YT_DATA_GET_SIZE,
    DATA_OPERATION_YT_DATA_GET_POSITION,
    DATA_OPERATION_YT_DATA_SET_POSITION,
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
    DATA_OPERATION_YT_DATA_IS_CURSOR_VISIBLE,
    DATA_OPERATION_YT_DATA_GET_CURSOR_OFFSET,
    DATA_OPERATION_YT_DATA_GET_CURSOR_X_VALUE,
    DATA_OPERATION_YT_DATA_TOUCH_DRAG
};

int count(uint16_t id);
void select(Cursor &cursor, uint16_t id, int index, Value &oldValue);
void deselect(Cursor &cursor, uint16_t id, Value &oldValue);

void setContext(Cursor &cursor, uint16_t id, Value &oldContext, Value &newContext);
void restoreContext(Cursor &cursor, uint16_t id, Value &oldContext);

int getFloatListLength(uint16_t id);
float *getFloatList(uint16_t id);

Value getMin(const Cursor &cursor, uint16_t id);
Value getMax(const Cursor &cursor, uint16_t id);
Value getDef(const Cursor &cursor, uint16_t id);
Value getLimit(const Cursor &cursor, uint16_t id);
const char *getName(const Cursor &cursor, uint16_t id);
Unit getUnit(const Cursor &cursor, uint16_t id);
bool isChannelData(const Cursor &cursor, uint16_t id);

Value getEncoderStep(const Cursor &cursor, uint16_t id);
struct StepValues {
    int count;
    const eez::gui::data::Value *values;
};
bool getEncoderStepValues(const Cursor &cursor, uint16_t id, StepValues &stepValues);

void getList(const Cursor &cursor, uint16_t id, const Value **labels, int &count);

Value get(const Cursor &cursor, uint16_t id);
Value set(const Cursor &cursor, uint16_t id, Value value);

uint16_t getColor(const Cursor &cursor, uint16_t id, const Style *style);
uint16_t getBackgroundColor(const Cursor &cursor, uint16_t id, const Style *style);
uint16_t getActiveColor(const Cursor &cursor, uint16_t id, const Style *style);
uint16_t getActiveBackgroundColor(const Cursor &cursor, uint16_t id, const Style *style);

bool isBlinking(const Cursor &cursor, uint16_t id);
Value getEditValue(const Cursor &cursor, uint16_t id);

uint16_t getBitmapWidth(const Cursor &cursor, uint16_t id);
uint16_t getBitmapHeight(const Cursor &cursor, uint16_t id);
Value getBitmapPixels(const Cursor &cursor, uint16_t id);

uint32_t ytDataGetRefreshCounter(const Cursor &cursor, uint16_t id);
uint32_t ytDataGetSize(const Cursor &cursor, uint16_t id);
uint32_t ytDataGetPosition(const Cursor &cursor, uint16_t id);
void ytDataSetPosition(const Cursor &cursor, uint16_t id, uint32_t newPosition);
uint32_t ytDataGetPageSize(const Cursor &cursor, uint16_t id);
const Style *ytDataGetStyle(const Cursor &cursor, uint16_t id, uint8_t valueIndex);
Value ytDataGetMin(const Cursor &cursor, uint16_t id, uint8_t valueIndex);
Value ytDataGetMax(const Cursor &cursor, uint16_t id, uint8_t valueIndex);
int ytDataGetVertDivisions(const Cursor &cursor, uint16_t id);
int ytDataGetHorzDivisions(const Cursor &cursor, uint16_t id);
float ytDataGetDiv(const Cursor &cursor, uint16_t id, uint8_t valueIndex);
float ytDataGetOffset(const Cursor &cursor, uint16_t id, uint8_t valueIndex);
bool ytDataDataValueIsVisible(const Cursor &cursor, uint16_t id, uint8_t valueIndex);
bool ytDataGetShowLabels(const Cursor &cursor, uint16_t id);
int8_t ytDataGetSelectedValueIndex(const Cursor &cursor, uint16_t id);
struct YtDataGetLabelParams {
    uint8_t valueIndex;
    char *text;
    int count;
};
void ytDataGetLabel(const Cursor &cursor, uint16_t id, uint8_t valueIndex, char *text, int count);
Value::YtDataGetValueFunctionPointer ytDataGetGetValueFunc(const Cursor &cursor, uint16_t id);
uint8_t ytDataGetGraphUpdateMethod(const Cursor &cursor, uint16_t id);
float ytDataGetPeriod(const Cursor &cursor, uint16_t id);
bool ytDataIsCursorVisible(const Cursor &cursor, uint16_t id);
uint32_t ytDataGetCursorOffset(const Cursor &cursor, uint16_t id);
Value ytDataGetCursorXValue(const Cursor &cursor, uint16_t id);

struct TouchDrag {
    EventType type;
    int x;
    int y;
};
void ytDataTouchDrag(const Cursor &cursor, uint16_t id, TouchDrag *touchDrag);

} // namespace data
} // namespace gui
} // namespace eez
