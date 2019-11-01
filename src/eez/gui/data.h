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

namespace eez {

enum BuiltInValueType {
    VALUE_TYPE_NONE,
    VALUE_TYPE_INT,
    VALUE_TYPE_FLOAT,
    VALUE_TYPE_STR,
    VALUE_TYPE_ENUM,
    VALUE_TYPE_SCPI_ERROR,
    VALUE_TYPE_PERCENTAGE,
    VALUE_TYPE_SIZE,
    VALUE_TYPE_POINTER,
    VALUE_TYPE_PAGE_INFO,
    VALUE_TYPE_MASTER_INFO,
    VALUE_TYPE_SLOT_INFO,
    VALUE_TYPE_SLOT_INFO2,
    VALUE_TYPE_TEST_RESULT,
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

    Value(void *value, ValueType type) 
        : type_(type), pVoid_(value) 
    {
    }

    Value(AppContext *appContext) 
        : type_(VALUE_TYPE_POINTER), pVoid_(appContext) 
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

    uint8_t getFirstUInt8() const {
        return pairOfUint8_.first;
    }
    uint8_t getSecondUInt8() const {
        return pairOfUint8_.second;
    }

    void toText(char *text, int count) const;

  public:
    ValueType type_;
    uint8_t options_;
    uint16_t unit_;
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
Value MakeScpiErrorValue(int16_t errorCode);
Value MakePageInfoValue(uint8_t pageIndex, uint8_t numPages);

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
    DATA_OPERATION_GET_UNIT,
    DATA_OPERATION_GET_HISTORY_VALUE,
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
    DATA_OPERATION_IS_BLINKING,
    DATA_OPERATION_SET,
    DATA_OPERATION_YT_DATA_GET_SIZE,
    DATA_OPERATION_YT_DATA_GET_POSITION,
    DATA_OPERATION_YT_DATA_SET_POSITION,
    DATA_OPERATION_YT_DATA_GET_PAGE_SIZE
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
ValueType getUnit(const Cursor &cursor, uint16_t id);

void getList(const Cursor &cursor, uint16_t id, const Value **labels, int &count);

Value get(const Cursor &cursor, uint16_t id);
bool set(const Cursor &cursor, uint16_t id, Value value, int16_t *error);

bool isBlinking(const Cursor &cursor, uint16_t id);
Value getEditValue(const Cursor &cursor, uint16_t id);

int ytDataGetSize(const Cursor &cursor, uint16_t id);
int ytDataGetPosition(const Cursor &cursor, uint16_t id);
void ytDataSetPosition(const Cursor &cursor, uint16_t id, int newPosition);
int ytDataGetPageSize(const Cursor &cursor, uint16_t id);

} // namespace data
} // namespace gui
} // namespace eez
