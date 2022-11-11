/*
 * EEZ Modular Firmware
 * Copyright (C) 2021-present, Envox d.o.o.
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

#include <stdio.h>
#include <string.h>
#include <math.h>

#include <chrono>
#include <string>
#include <iostream>
#include <sstream>

// https://howardhinnant.github.io/date/date.html
#include <eez/libs/date.h>

#include <eez/core/os.h>
#include <eez/core/value.h>
#include <eez/core/util.h>

#include <eez/flow/flow.h>
#include <eez/flow/operations.h>
#include <eez/flow/flow_defs_v3.h>

#if OPTION_GUI || !defined(OPTION_GUI)
#include <eez/gui/gui.h>
using namespace eez::gui;
#endif

namespace eez {
namespace flow {

Value op_add(const Value& a1, const Value& b1) {
    if (a1.isError()) {
        return a1;
    }

    if (b1.isError()) {
        return b1;
    }

	auto a = a1.getValue();
	auto b = b1.getValue();

	if (a.isString() || b.isString()) {
		Value value1 = a.toString(0x84eafaa8);
		Value value2 = b.toString(0xd273cab6);
		auto res = Value::concatenateString(value1, value2);

		char str1[128];
		res.toText(str1, sizeof(str1));

		return res;
	}

	if (a.isDouble() || b.isDouble()) {
		return Value(a.toDouble() + b.toDouble(), VALUE_TYPE_DOUBLE);
	}

	if (a.isFloat() || b.isFloat()) {
		return Value(a.toFloat() + b.toFloat(), VALUE_TYPE_FLOAT);
	}

	if (a.isInt64() || b.isInt64()) {
		return Value(a.toInt64() + b.toInt64(), VALUE_TYPE_INT64);
	}

	if (a.isInt32OrLess() && b.isInt32OrLess()) {
		return Value((int)(a.int32Value + b.int32Value), VALUE_TYPE_INT32);
	}

	return Value();
}

Value op_sub(const Value& a1, const Value& b1) {
    if (a1.isError()) {
        return a1;
    }

    if (b1.isError()) {
        return b1;
    }

	auto a = a1.getValue();
	auto b = b1.getValue();

	if (a.isDouble() || b.isDouble()) {
		return Value(a.toDouble() - b.toDouble(), VALUE_TYPE_DOUBLE);
	}

	if (a.isFloat() || b.isFloat()) {
		return Value(a.toFloat() - b.toFloat(), VALUE_TYPE_FLOAT);
	}

	if (a.isInt64() || b.isInt64()) {
		return Value(a.toInt64() - b.toInt64(), VALUE_TYPE_INT64);
	}

	if (a.isInt32OrLess() && b.isInt32OrLess()) {
		return Value((int)(a.int32Value - b.int32Value), VALUE_TYPE_INT32);
	}

	return Value();
}

Value op_mul(const Value& a1, const Value& b1) {
    if (a1.isError()) {
        return a1;
    }

    if (b1.isError()) {
        return b1;
    }

	auto a = a1.getValue();
	auto b = b1.getValue();

	if (a.isDouble() || b.isDouble()) {
		return Value(a.toDouble() * b.toDouble(), VALUE_TYPE_DOUBLE);
	}

	if (a.isFloat() || b.isFloat()) {
		return Value(a.toFloat() * b.toFloat(), VALUE_TYPE_FLOAT);
	}

	if (a.isInt64() || b.isInt64()) {
		return Value(a.toInt64() * b.toInt64(), VALUE_TYPE_INT64);
	}

	if (a.isInt32OrLess() && b.isInt32OrLess()) {
		return Value((int)(a.int32Value * b.int32Value), VALUE_TYPE_INT32);
	}

	return Value();
}

Value op_div(const Value& a1, const Value& b1) {
    if (a1.isError()) {
        return a1;
    }

    if (b1.isError()) {
        return b1;
    }

	auto a = a1.getValue();
	auto b = b1.getValue();

	if (a.isDouble() || b.isDouble()) {
		return Value(a.toDouble() / b.toDouble(), VALUE_TYPE_DOUBLE);
	}

	if (a.isFloat() || b.isFloat()) {
		return Value(a.toFloat() / b.toFloat(), VALUE_TYPE_FLOAT);
	}

	if (a.isInt64() || b.isInt64()) {
		return Value(1.0 * a.toInt64() / b.toInt64(), VALUE_TYPE_DOUBLE);
	}

	if (a.isInt32OrLess() && b.isInt32OrLess()) {
		return Value(1.0 * a.int32Value / b.int32Value, VALUE_TYPE_DOUBLE);
	}

	return Value();
}

Value op_mod(const Value& a1, const Value& b1) {
    if (a1.isError()) {
        return a1;
    }

    if (b1.isError()) {
        return b1;
    }

	auto a = a1.getValue();
	auto b = b1.getValue();

	return Value(a.toDouble() - floor(a.toDouble() / b.toDouble()) * b.toDouble(), VALUE_TYPE_DOUBLE);
}

Value op_left_shift(const Value& a1, const Value& b1) {
    if (a1.isError()) {
        return a1;
    }

    if (b1.isError()) {
        return b1;
    }

	auto a = a1.getValue();
	auto b = b1.getValue();

	return Value((int)(a.toInt32() << b.toInt32()), VALUE_TYPE_INT32);
}

Value op_right_shift(const Value& a1, const Value& b1) {
    if (a1.isError()) {
        return a1;
    }

    if (b1.isError()) {
        return b1;
    }

	auto a = a1.getValue();
	auto b = b1.getValue();

	return Value((int)(a.toInt32() >> b.toInt32()), VALUE_TYPE_INT32);
}

Value op_binary_and(const Value& a1, const Value& b1) {
    if (a1.isError()) {
        return a1;
    }

    if (b1.isError()) {
        return b1;
    }

	auto a = a1.getValue();
	auto b = b1.getValue();

	return Value((int)(a.toBool() & b.toBool()), VALUE_TYPE_INT32);
}

Value op_binary_or(const Value& a1, const Value& b1) {
    if (a1.isError()) {
        return a1;
    }

    if (b1.isError()) {
        return b1;
    }

	auto a = a1.getValue();
	auto b = b1.getValue();

	return Value((int)(a.toBool() | b.toBool()), VALUE_TYPE_INT32);
}

Value op_binary_xor(const Value& a1, const Value& b1) {
    if (a1.isError()) {
        return a1;
    }

    if (b1.isError()) {
        return b1;
    }

	auto a = a1.getValue();
	auto b = b1.getValue();

	return Value((int)(a.toBool() ^ b.toBool()), VALUE_TYPE_INT32);
}

bool is_equal(const Value& a1, const Value& b1) {
	auto a = a1.getValue();
	auto b = b1.getValue();

    auto aIsUndefinedOrNull = a.getType() == VALUE_TYPE_UNDEFINED || a.getType() == VALUE_TYPE_NULL;
    auto bIsUndefinedOrNull = b.getType() == VALUE_TYPE_UNDEFINED || b.getType() == VALUE_TYPE_NULL;

    if (aIsUndefinedOrNull) {
        return bIsUndefinedOrNull;
    } else if (bIsUndefinedOrNull) {
        return false;
    }

	if (a.isString() && b.isString()) {
		const char *aStr = a.getString();
		const char *bStr = b.getString();
		if (!aStr && !aStr) {
			return true;
		}
		if (!aStr || !bStr) {
			return false;
		}
		return strcmp(aStr, bStr) == 0;
	}

	return a.toDouble() == b.toDouble();
}

bool is_less(const Value& a1, const Value& b1) {
	auto a = a1.getValue();
	auto b = b1.getValue();

	if (a.isString() && b.isString()) {
		const char *aStr = a.getString();
		const char *bStr = b.getString();
		if (!aStr || !bStr) {
			return false;
		}
		return strcmp(aStr, bStr) < 0;
	}

	return a.toDouble() < b.toDouble();
}

Value op_eq(const Value& a1, const Value& b1) {
    if (a1.isError()) {
        return a1;
    }

    if (b1.isError()) {
        return b1;
    }

	return Value(is_equal(a1, b1), VALUE_TYPE_BOOLEAN);
}

Value op_neq(const Value& a1, const Value& b1) {
    if (a1.isError()) {
        return a1;
    }

    if (b1.isError()) {
        return b1;
    }

	return Value(!is_equal(a1, b1), VALUE_TYPE_BOOLEAN);
}

Value op_less(const Value& a1, const Value& b1) {
    if (a1.isError()) {
        return a1;
    }

    if (b1.isError()) {
        return b1;
    }

	return Value(is_less(a1, b1), VALUE_TYPE_BOOLEAN);
}

Value op_great(const Value& a1, const Value& b1) {
    if (a1.isError()) {
        return a1;
    }

    if (b1.isError()) {
        return b1;
    }

	return Value(!is_less(a1, b1) && !is_equal(a1, b1), VALUE_TYPE_BOOLEAN);
}

Value op_less_eq(const Value& a1, const Value& b1) {
    if (a1.isError()) {
        return a1;
    }

    if (b1.isError()) {
        return b1;
    }

	return Value(is_less(a1, b1) || is_equal(a1, b1), VALUE_TYPE_BOOLEAN);
}

Value op_great_eq(const Value& a1, const Value& b1) {
    if (a1.isError()) {
        return a1;
    }

    if (b1.isError()) {
        return b1;
    }

	return Value(!is_less(a1, b1), VALUE_TYPE_BOOLEAN);
}

void do_OPERATION_TYPE_ADD(EvalStack &stack) {
	auto b = stack.pop();
	auto a = stack.pop();

	auto result = op_add(a, b);

	if (result.getType() == VALUE_TYPE_UNDEFINED) {
		result = Value::makeError();
	}

	stack.push(result);
}

void do_OPERATION_TYPE_SUB(EvalStack &stack) {
	auto b = stack.pop();
	auto a = stack.pop();

	auto result = op_sub(a, b);

	if (result.getType() == VALUE_TYPE_UNDEFINED) {
		result = Value::makeError();
	}

	stack.push(result);
}

void do_OPERATION_TYPE_MUL(EvalStack &stack) {
	auto b = stack.pop();
	auto a = stack.pop();

	auto result = op_mul(a, b);

	if (result.getType() == VALUE_TYPE_UNDEFINED) {
		result = Value::makeError();
	}

	stack.push(result);
}

void do_OPERATION_TYPE_DIV(EvalStack &stack) {
	auto b = stack.pop();
	auto a = stack.pop();

	auto result = op_div(a, b);

	if (result.getType() == VALUE_TYPE_UNDEFINED) {
		result = Value::makeError();
	}

	stack.push(result);
}

void do_OPERATION_TYPE_MOD(EvalStack &stack) {
	auto b = stack.pop();
	auto a = stack.pop();

	auto result = op_mod(a, b);

	if (result.getType() == VALUE_TYPE_UNDEFINED) {
		result = Value::makeError();
	}

	stack.push(result);
}

void do_OPERATION_TYPE_LEFT_SHIFT(EvalStack &stack) {
	auto b = stack.pop();
	auto a = stack.pop();

	auto result = op_left_shift(a, b);

	if (result.getType() == VALUE_TYPE_UNDEFINED) {
		result = Value::makeError();
	}

	stack.push(result);
}

void do_OPERATION_TYPE_RIGHT_SHIFT(EvalStack &stack) {
	auto b = stack.pop();
	auto a = stack.pop();

	auto result = op_right_shift(a, b);

	if (result.getType() == VALUE_TYPE_UNDEFINED) {
		result = Value::makeError();
	}

	stack.push(result);
}

void do_OPERATION_TYPE_BINARY_AND(EvalStack &stack) {
	auto b = stack.pop();
	auto a = stack.pop();

	auto result = op_binary_and(a, b);

	if (result.getType() == VALUE_TYPE_UNDEFINED) {
		result = Value::makeError();
	}

	stack.push(result);
}

void do_OPERATION_TYPE_BINARY_OR(EvalStack &stack) {
	auto b = stack.pop();
	auto a = stack.pop();

	auto result = op_binary_or(a, b);

	if (result.getType() == VALUE_TYPE_UNDEFINED) {
		result = Value::makeError();
	}

	stack.push(result);
}

void do_OPERATION_TYPE_BINARY_XOR(EvalStack &stack) {
	auto b = stack.pop();
	auto a = stack.pop();

	auto result = op_binary_xor(a, b);

	if (result.getType() == VALUE_TYPE_UNDEFINED) {
		result = Value::makeError();
	}

	stack.push(result);
}

void do_OPERATION_TYPE_EQUAL(EvalStack &stack) {
	auto b = stack.pop();
	auto a = stack.pop();
	stack.push(op_eq(a, b));
}

void do_OPERATION_TYPE_NOT_EQUAL(EvalStack &stack) {
	auto b = stack.pop();
	auto a = stack.pop();
	stack.push(op_neq(a, b));
}

void do_OPERATION_TYPE_LESS(EvalStack &stack) {
	auto b = stack.pop();
	auto a = stack.pop();
	stack.push(op_less(a, b));
}

void do_OPERATION_TYPE_GREATER(EvalStack &stack) {
	auto b = stack.pop();
	auto a = stack.pop();
	stack.push(op_great(a, b));
}

void do_OPERATION_TYPE_LESS_OR_EQUAL(EvalStack &stack) {
	auto b = stack.pop();
	auto a = stack.pop();
	stack.push(op_less_eq(a, b));
}

void do_OPERATION_TYPE_GREATER_OR_EQUAL(EvalStack &stack) {
	auto b = stack.pop();
	auto a = stack.pop();
	stack.push(op_great_eq(a, b));
}

void do_OPERATION_TYPE_LOGICAL_AND(EvalStack &stack) {
	auto bValue = stack.pop().getValue();
	auto aValue = stack.pop().getValue();

    if (aValue.isError()) {
        stack.push(aValue);
        return;
    }

    if (!aValue.toBool()) {
        stack.push(Value(false, VALUE_TYPE_BOOLEAN));
        return;
    }

    if (bValue.isError()) {
        stack.push(bValue);
        return;
    }

    stack.push(Value(bValue.toBool(), VALUE_TYPE_BOOLEAN));
}

void do_OPERATION_TYPE_LOGICAL_OR(EvalStack &stack) {
	auto bValue = stack.pop().getValue();
	auto aValue = stack.pop().getValue();

    if (aValue.isError()) {
        stack.push(aValue);
        return;
    }

    if (aValue.toBool()) {
        stack.push(Value(true, VALUE_TYPE_BOOLEAN));
        return;
    }

    if (bValue.isError()) {
        stack.push(bValue);
        return;
    }

    stack.push(Value(bValue.toBool(), VALUE_TYPE_BOOLEAN));
}

void do_OPERATION_TYPE_UNARY_PLUS(EvalStack &stack) {
	auto a = stack.pop().getValue();

	if (a.isDouble()) {
		stack.push(Value(a.getDouble(), VALUE_TYPE_DOUBLE));
		return;
	}

	if (a.isFloat()) {
		stack.push(Value(a.toFloat(), VALUE_TYPE_FLOAT));
		return;
	}

	if (a.isInt64()) {
		stack.push(Value((int64_t)a.getInt64(), VALUE_TYPE_INT64));
		return;
	}

	if (a.isInt32()) {
		stack.push(Value((int)a.getInt32(), VALUE_TYPE_INT32));
		return;
	}

	if (a.isInt16()) {
		stack.push(Value((int16_t)a.getInt16(), VALUE_TYPE_INT16));
        return;
	}

	if (a.isInt8()) {
		stack.push(Value((int8_t)a.getInt8(), VALUE_TYPE_INT8));
		return;
	}

	stack.push(Value::makeError());
}

void do_OPERATION_TYPE_UNARY_MINUS(EvalStack &stack) {
	auto a = stack.pop().getValue();

	if (a.isDouble()) {
		stack.push(Value(-a.getDouble(), VALUE_TYPE_DOUBLE));
		return;
	}

	if (a.isFloat()) {
		stack.push(Value(-a.toFloat(), VALUE_TYPE_FLOAT));
		return;
	}

	if (a.isInt64()) {
		stack.push(Value((int64_t)-a.getInt64(), VALUE_TYPE_INT64));
		return;
	}

	if (a.isInt32()) {
		stack.push(Value((int)-a.getInt32(), VALUE_TYPE_INT32));
		return;
	}

	if (a.isInt16()) {
		stack.push(Value((int16_t)-a.getInt16(), VALUE_TYPE_INT16));
		return;
	}

	if (a.isInt8()) {
		stack.push(Value((int8_t)-a.getInt8(), VALUE_TYPE_INT8));
		return;
	}

	stack.push(Value::makeError());
}

void do_OPERATION_TYPE_BINARY_ONE_COMPLEMENT(EvalStack &stack) {
	auto a = stack.pop().getValue();

	if (a.isInt64()) {
		stack.push(Value(~a.uint64Value, VALUE_TYPE_UINT64));
		return;
	}

	if (a.isInt32()) {
		stack.push(Value(~a.uint32Value, VALUE_TYPE_UINT32));
		return;
	}

	if (a.isInt16()) {
		stack.push(Value(~a.uint16Value, VALUE_TYPE_UINT16));
		return;
	}

	if (a.isInt8()) {
		stack.push(Value(~a.uint8Value, VALUE_TYPE_UINT8));
		return;
	}

	stack.push(Value::makeError());
}

void do_OPERATION_TYPE_NOT(EvalStack &stack) {
	auto aValue = stack.pop();

    if (aValue.isError()) {
        stack.push(aValue);
        return;
    }

	int err;
	auto a = aValue.toBool(&err);
	if (err != 0) {
        stack.push(Value::makeError());
		return;
	}

	stack.push(Value(!a, VALUE_TYPE_BOOLEAN));
}

void do_OPERATION_TYPE_CONDITIONAL(EvalStack &stack) {
    auto alternate = stack.pop();
	auto consequent = stack.pop();
	auto conditionValue = stack.pop();

    if (conditionValue.isError()) {
        stack.push(conditionValue);
        return;
    }

	int err;
	auto condition = conditionValue.toBool(&err);
	if (err != 0) {
        stack.push(Value::makeError());
		return;
	}

	stack.push(condition ? consequent : alternate);
}

void do_OPERATION_TYPE_SYSTEM_GET_TICK(EvalStack &stack) {
	stack.push(Value(millis(), VALUE_TYPE_UINT32));
}

void do_OPERATION_TYPE_FLOW_INDEX(EvalStack &stack) {
	if (!stack.iterators) {
        stack.push(Value::makeError());
		return;
	}

	auto a = stack.pop();

	int err;
	auto iteratorIndex = a.toInt32(&err);
	if (err != 0) {
		stack.push(Value::makeError());
        return;
	}

	iteratorIndex = iteratorIndex;
	if (iteratorIndex < 0 || iteratorIndex >= (int)MAX_ITERATORS) {
		stack.push(Value::makeError());
        return;
	}

	stack.push(stack.iterators[iteratorIndex]);
}

void do_OPERATION_TYPE_FLOW_IS_PAGE_ACTIVE(EvalStack &stack) {
#if OPTION_GUI || !defined(OPTION_GUI)
	bool isActive = false;

	auto pageIndex = getPageIndex(stack.flowState);
	if (pageIndex >= 0) {
		int16_t pageId = (int16_t)(pageIndex + 1);
		if (stack.flowState->assets == g_externalAssets) {
			pageId = -pageId;
		}

		for (int16_t appContextId = 0; ; appContextId++) {
			auto appContext = getAppContextFromId(appContextId);
			if (!appContext) {
				break;
			}

			if (appContext->isPageOnStack(pageId)) {
				isActive = true;
				break;
			}
		}
	}

	stack.push(Value(isActive, VALUE_TYPE_BOOLEAN));
#else
    stack.push(Value::makeError());
#endif // OPTION_GUI || !defined(OPTION_GUI)
}

void do_OPERATION_TYPE_FLOW_PAGE_TIMELINE_POSITION(EvalStack &stack) {
	stack.push(Value(stack.flowState->timelinePosition, VALUE_TYPE_FLOAT));
}

void do_OPERATION_TYPE_FLOW_MAKE_ARRAY_VALUE(EvalStack &stack) {
    auto arrayTypeValue = stack.pop();
    if (arrayTypeValue.isError()) {
        stack.push(arrayTypeValue);
        return;
    }

    auto arraySizeValue = stack.pop();
    if (arraySizeValue.isError()) {
        stack.push(arraySizeValue);
        return;
    }

    int arrayType = arrayTypeValue.getInt();
    int arraySize = arraySizeValue.getInt();

    auto arrayValue = Value::makeArrayRef(arraySize, arrayType, 0x837260d4);

    auto array = arrayValue.getArray();

    for (int i = 0; i < arraySize; i++) {
        array->values[i] = stack.pop().getValue();
    }

    stack.push(arrayValue);
}

void do_OPERATION_TYPE_FLOW_LANGUAGES(EvalStack &stack) {
    auto &languages = stack.flowState->assets->languages;

    auto arrayValue = Value::makeArrayRef(languages.count, VALUE_TYPE_STRING, 0xff4787fc);

    auto array = arrayValue.getArray();

    for (uint32_t i = 0; i < languages.count; i++) {
        array->values[i] = Value((const char *)(languages[i]->languageID));
    }

    stack.push(arrayValue);
}

void do_OPERATION_TYPE_FLOW_TRANSLATE(EvalStack &stack) {
    auto textResourceIndexValue = stack.pop();

    int textResourceIndex = textResourceIndexValue.getInt();

    int languageIndex = g_selectedLanguage;

    auto &languages = stack.flowState->assets->languages;
    if (languageIndex >= 0 && languageIndex < (int)languages.count) {
        auto &translations = languages[languageIndex]->translations;
        if (textResourceIndex >= 0 && textResourceIndex < (int)translations.count) {
            stack.push(translations[textResourceIndex]);
            return;
        }
    }

    stack.push("");
}

void do_OPERATION_TYPE_FLOW_PARSE_INTEGER(EvalStack &stack) {
    auto str = stack.pop();
    if (str.isError()) {
        stack.push(str);
        return;
    }

    int err;
    auto value = str.toInt32(&err);
    if (err) {
        stack.push(Value::makeError());
        return;
    }

    stack.push(Value((int)value, VALUE_TYPE_INT32));
}

void do_OPERATION_TYPE_FLOW_PARSE_FLOAT(EvalStack &stack) {
    auto str = stack.pop();
    if (str.isError()) {
        stack.push(str);
        return;
    }

    int err;
    auto value = str.toFloat(&err);
    if (err) {
        stack.push(Value::makeError());
        return;
    }

    stack.push(Value(value, VALUE_TYPE_FLOAT));
}

void do_OPERATION_TYPE_FLOW_PARSE_DOUBLE(EvalStack &stack) {
    auto str = stack.pop();
    if (str.isError()) {
        stack.push(str);
        return;
    }

    int err;
    auto value = str.toDouble(&err);
    if (err) {
        stack.push(Value::makeError());
        return;
    }

    stack.push(Value(value, VALUE_TYPE_DOUBLE));
}

void do_OPERATION_TYPE_DATE_NOW(EvalStack &stack) {
    using namespace std::chrono;
    milliseconds ms = duration_cast<milliseconds>(system_clock::now().time_since_epoch());

    stack.push(Value((double)ms.count(), VALUE_TYPE_DATE));
}

void do_OPERATION_TYPE_DATE_TO_STRING(EvalStack &stack) {
	auto a = stack.pop().getValue();
    if (a.isError()) {
        stack.push(a);
        return;
    }
    if (a.getType() != VALUE_TYPE_DATE) {
        stack.push(Value::makeError());
        return;
    }

    using namespace std;
    using namespace std::chrono;
    using namespace date;

    auto tp = system_clock::time_point(milliseconds((long long)a.getDouble()));

    stringstream out;
    out << tp << endl;

    stack.push(Value::makeStringRef(out.str().c_str(), -1, 0xbe440ec8));
}

void do_OPERATION_TYPE_DATE_FROM_STRING(EvalStack &stack) {
	auto a = stack.pop().getValue();
    if (a.isError()) {
        stack.push(a);
        return;
    }

    Value dateStrValue = a.toString(0x99cb1a93);

    using namespace std;
    using namespace std::chrono;
    using namespace date;

    istringstream in{dateStrValue.getString()};

    system_clock::time_point tp;
    in >> date::parse("%Y-%m-%d %T", tp);

    milliseconds ms = duration_cast<milliseconds>(tp.time_since_epoch());
    stack.push(Value((double)ms.count(), VALUE_TYPE_DATE));
}

void do_OPERATION_TYPE_MATH_SIN(EvalStack &stack) {
	auto a = stack.pop().getValue();
    if (a.isError()) {
        stack.push(a);
        return;
    }

	if (a.isDouble()) {
		stack.push(Value(sin(a.getDouble()), VALUE_TYPE_DOUBLE));
        return;
	}

	if (a.isFloat()) {
		stack.push(Value(sinf(a.toFloat()), VALUE_TYPE_FLOAT));
        return;
	}

	if (a.isInt64()) {
		stack.push(Value(sin(a.toInt64()), VALUE_TYPE_FLOAT));
		return;
	}

	if (a.isInt32OrLess()) {
		stack.push(Value(sinf(a.int32Value), VALUE_TYPE_FLOAT));
		return;
	}

	stack.push(Value::makeError());
}

void do_OPERATION_TYPE_MATH_COS(EvalStack &stack) {
	auto a = stack.pop().getValue();
    if (a.isError()) {
        stack.push(a);
        return;
    }

	if (a.isDouble()) {
		stack.push(Value(cos(a.getDouble()), VALUE_TYPE_DOUBLE));
		return;
	}

	if (a.isFloat()) {
		stack.push(Value(cosf(a.toFloat()), VALUE_TYPE_FLOAT));
		return;
	}

	if (a.isInt64()) {
		stack.push(Value(cos(a.toInt64()), VALUE_TYPE_FLOAT));
		return;
	}

	if (a.isInt32OrLess()) {
		stack.push(Value(cosf(a.int32Value), VALUE_TYPE_FLOAT));
		return;
	}

	stack.push(Value::makeError());
}

void do_OPERATION_TYPE_MATH_LOG(EvalStack &stack) {
	auto a = stack.pop().getValue();
    if (a.isError()) {
        stack.push(a);
        return;
    }

	if (a.isDouble()) {
		stack.push(Value(log(a.getDouble()), VALUE_TYPE_DOUBLE));
		return;
	}

	if (a.isFloat()) {
		stack.push(Value(logf(a.toFloat()), VALUE_TYPE_FLOAT));
		return;
	}

	if (a.isInt64()) {
		stack.push(Value(log(a.toInt64()), VALUE_TYPE_FLOAT));
		return;
	}

	if (a.isInt32OrLess()) {
		stack.push(Value(logf(a.int32Value), VALUE_TYPE_FLOAT));
		return;
	}

	stack.push(Value::makeError());
}

void do_OPERATION_TYPE_MATH_LOG10(EvalStack &stack) {
	auto a = stack.pop().getValue();
    if (a.isError()) {
        stack.push(a);
        return;
    }

	if (a.isDouble()) {
		stack.push(Value(log10(a.getDouble()), VALUE_TYPE_DOUBLE));
		return;
	}

	if (a.isFloat()) {
		stack.push(Value(log10f(a.toFloat()), VALUE_TYPE_FLOAT));
		return;
	}

	if (a.isInt64()) {
		stack.push(Value(log10(a.toInt64()), VALUE_TYPE_FLOAT));
		return;
	}

	if (a.isInt32OrLess()) {
		stack.push(Value(log10f(a.int32Value), VALUE_TYPE_FLOAT));
		return;
	}

	stack.push(Value::makeError());
}

void do_OPERATION_TYPE_MATH_ABS(EvalStack &stack) {
	auto a = stack.pop().getValue();
    if (a.isError()) {
        stack.push(a);
        return;
    }

	if (a.isDouble()) {
		stack.push(Value(abs(a.getDouble()), VALUE_TYPE_DOUBLE));
		return;
	}

	if (a.isFloat()) {
		stack.push(Value(abs(a.toFloat()), VALUE_TYPE_FLOAT));
		return;
	}

	if (a.isInt64()) {
		stack.push(Value((int64_t)abs(a.getInt64()), VALUE_TYPE_INT64));
		return;
	}

	if (a.isInt32()) {
		stack.push(Value((int)abs(a.getInt32()), VALUE_TYPE_INT32));
		return;
	}

	if (a.isInt16()) {
		stack.push(Value(abs(a.getInt16()), VALUE_TYPE_INT16));
		return;
	}

	if (a.isInt8()) {
		stack.push(Value(abs(a.getInt8()), VALUE_TYPE_INT8));
		return;
	}

	stack.push(Value::makeError());
}

void do_OPERATION_TYPE_MATH_FLOOR(EvalStack &stack) {
	auto a = stack.pop().getValue();
    if (a.isError()) {
        stack.push(a);
        return;
    }

	if (a.isDouble()) {
		stack.push(Value(floor(a.getDouble()), VALUE_TYPE_DOUBLE));
		return;
	}

	if (a.isFloat()) {
		stack.push(Value(floorf(a.toFloat()), VALUE_TYPE_FLOAT));
		return;
	}

	stack.push(Value::makeError());
}

void do_OPERATION_TYPE_MATH_CEIL(EvalStack &stack) {
	auto a = stack.pop().getValue();
    if (a.isError()) {
        stack.push(a);
        return;
    }

	if (a.isDouble()) {
		stack.push(Value(ceil(a.getDouble()), VALUE_TYPE_DOUBLE));
		return;
	}

	if (a.isFloat()) {
		stack.push(Value(ceilf(a.toFloat()), VALUE_TYPE_FLOAT));
		return;
	}

	stack.push(Value::makeError());
}

float roundN(float value, unsigned int numDigits) {
  float pow_10 = pow(10.0f, numDigits);
  return round(value * pow_10) / pow_10;
}

double roundN(double value, unsigned int numDigits) {
  float pow_10 = pow(10.0f, numDigits);
  return round(value * pow_10) / pow_10;
}

void do_OPERATION_TYPE_MATH_ROUND(EvalStack &stack) {
    auto numArgs = stack.pop().getInt();
	auto a = stack.pop().getValue();
    if (a.isError()) {
        stack.push(a);
        return;
    }

    unsigned int numDigits;
    if (numArgs > 1) {
        auto b = stack.pop().getValue();
        numDigits = b.toInt32();
    } else {
        numDigits = 0;
    }

	if (a.isDouble()) {
		stack.push(Value(roundN(a.getDouble(), numDigits), VALUE_TYPE_DOUBLE));
		return;
	}

	if (a.isFloat()) {
		stack.push(Value(roundN(a.toFloat(), numDigits), VALUE_TYPE_FLOAT));
		return;
	}

    if (a.isInt32OrLess()) {
		stack.push(a);
		return;
    }

	stack.push(Value::makeError());
}

void do_OPERATION_TYPE_MATH_MIN(EvalStack &stack) {
    auto numArgs = stack.pop().getInt();

    double minValue = INFINITY;

    for (int i = 0; i < numArgs; i++) {
        auto value = stack.pop().getValue();
        if (value.isError()) {
            stack.push(value);
            return;
        }
        auto valueDouble = value.toDouble();
        if (valueDouble < minValue) {
            minValue = valueDouble;
        }
    }

	stack.push(Value(minValue, VALUE_TYPE_DOUBLE));
}

void do_OPERATION_TYPE_MATH_MAX(EvalStack &stack) {
    auto numArgs = stack.pop().getInt();

    double maxValue = -INFINITY;

    for (int i = 0; i < numArgs; i++) {
        auto value = stack.pop().getValue();
        if (value.isError()) {
            stack.push(value);
            return;
        }
        auto valueDouble = value.toDouble();
        if (valueDouble > maxValue) {
            maxValue = valueDouble;
        }
    }

	stack.push(Value(maxValue, VALUE_TYPE_DOUBLE));
}

void do_OPERATION_TYPE_STRING_LENGTH(EvalStack &stack) {
	auto a = stack.pop().getValue();
    if (a.isError()) {
        stack.push(a);
        return;
    }

    const char *aStr = a.getString();
	if (!aStr) {
        stack.push(Value::makeError());
		return;
	}

    int aStrLen = strlen(aStr);

    stack.push(Value(aStrLen, VALUE_TYPE_INT32));
}

void do_OPERATION_TYPE_STRING_SUBSTRING(EvalStack &stack) {
    auto numArgs = stack.pop().getInt();

    Value strValue = stack.pop().getValue();
    if (strValue.isError()) {
        stack.push(strValue);
        return;
    }
    Value startValue = stack.pop().getValue();
    if (startValue.isError()) {
        stack.push(startValue);
        return;
    }
    Value endValue;
    if (numArgs == 3) {
        endValue = stack.pop().getValue();
        if (endValue.isError()) {
            stack.push(endValue);
            return;
        }
    }

    const char *str = strValue.getString();
    if (!str) {
        stack.push(Value::makeError());
        return;
    }

    int strLen = (int)strlen(str);

    int err = 0;

    int start = startValue.toInt32(&err);
    if (err != 0) {
        stack.push(Value::makeError());
        return;
    }

    int end;
    if (endValue.getType() == VALUE_TYPE_UNDEFINED) {
        end = strLen;
    } else {
        end = endValue.toInt32(&err);
        if (err != 0) {
            stack.push(Value::makeError());
            return;
        }
    }

    if (start < 0) {
        start = 0;
    } else if (start > strLen) {
        start = strLen;
    }

    if (end < 0) {
        end = 0;
    } else if (end > strLen) {
        end = strLen;
    }

    if (start < end) {
        Value resultValue = Value::makeStringRef(str + start, end - start, 0x203b08a2);
        stack.push(resultValue);
        return;
    }

    stack.push(Value("", VALUE_TYPE_STRING));
}

void do_OPERATION_TYPE_STRING_FIND(EvalStack &stack) {
	auto a = stack.pop().getValue();
    if (a.isError()) {
        stack.push(a);
        return;
    }
	auto b = stack.pop().getValue();
    if (b.isError()) {
        stack.push(b);
        return;
    }

	Value aStr = a.toString(0xf616bf4d);
	Value bStr = b.toString(0x81229133);
	if (!aStr.getString() || !bStr.getString()) {
		stack.push(Value(-1, VALUE_TYPE_INT32));
		return;
	}

    const char *pos = strstr(aStr.getString(), bStr.getString());
    if (pos) {
        stack.push(Value((int)(pos - aStr.getString()), VALUE_TYPE_INT32));
        return;
    }

    stack.push(Value(-1, VALUE_TYPE_INT32));
}

void do_OPERATION_TYPE_STRING_PAD_START(EvalStack &stack) {
    auto a = stack.pop().getValue();
    if (a.isError()) {
        stack.push(a);
        return;
    }
	auto b = stack.pop().getValue();
    if (b.isError()) {
        stack.push(b);
        return;
    }
    auto c = stack.pop().getValue();
    if (c.isError()) {
        stack.push(c);
        return;
    }

	auto str = a.toString(0xcf6aabe6);
	if (!str.getString()) {
        stack.push(Value::makeError());
		return;
	}
	int strLen = strlen(str.getString());

	int err;
	int targetLength = b.toInt32(&err);
	if (err) {
        stack.push(Value::makeError());
		return;
	}
	if (targetLength < strLen) {
		targetLength = strLen;
	}

	auto padStr = c.toString(0x81353bd7);
	if (!padStr.getString()) {
        stack.push(Value::makeError());
		return;
	}
	int padStrLen = strlen(padStr.getString());

	Value resultValue = Value::makeStringRef("", targetLength, 0xf43b14dd);
	if (resultValue.type == VALUE_TYPE_NULL) {
        stack.push(Value::makeError());
		return;
	}
	char *resultStr = (char *)resultValue.getString();

	auto n = targetLength - strLen;
	stringCopy(resultStr + (targetLength - strLen), strLen + 1, str.getString());

	for (int i = 0; i < n; i++) {
		resultStr[i] = padStr.getString()[i % padStrLen];
	}

	stack.push(resultValue);
}

void do_OPERATION_TYPE_STRING_SPLIT(EvalStack &stack) {
	auto strValue = stack.pop().getValue();
    if (strValue.isError()) {
        stack.push(strValue);
        return;
    }
    auto delimValue = stack.pop().getValue();
    if (delimValue.isError()) {
        stack.push(delimValue);
        return;
    }

    auto str = strValue.getString();
	if (!str) {
        stack.push(Value::makeError());
		return;
	}

    auto delim = delimValue.getString();
	if (!delim) {
        stack.push(Value::makeError());
		return;
	}

    auto strLen = strlen(str);

    char *strCopy = (char *)eez::alloc(strLen + 1, 0xea9d0bc0);
    stringCopy(strCopy, strLen + 1, str);

    // get num parts
    size_t arraySize = 0;
    char *token = strtok(strCopy, delim);
    while (token != NULL) {
        arraySize++;
        token = strtok(NULL, delim);
    }

    eez::free(strCopy);
    strCopy = (char *)eez::alloc(strLen + 1, 0xea9d0bc1);
    stringCopy(strCopy, strLen + 1, str);

    // make array
    auto arrayValue = Value::makeArrayRef(arraySize, VALUE_TYPE_STRING, 0xe82675d4);
    auto array = arrayValue.getArray();
    int i = 0;
    token = strtok(strCopy, delim);
    while (token != NULL) {
        array->values[i++] = Value::makeStringRef(token, -1, 0x45209ec0);
        token = strtok(NULL, delim);
    }

    eez::free(strCopy);

    stack.push(arrayValue);
}

void do_OPERATION_TYPE_ARRAY_LENGTH(EvalStack &stack) {
    auto a = stack.pop().getValue();
    if (a.isError()) {
        stack.push(a);
        return;
    }

    if (!a.isArray()) {
        stack.push(Value::makeError());
        return;
    }

    auto array = a.getArray();
    stack.push(Value(array->arraySize, VALUE_TYPE_UINT32));
}

void do_OPERATION_TYPE_ARRAY_SLICE(EvalStack &stack) {
    auto numArgs = stack.pop().getInt();
    auto arrayValue = stack.pop().getValue();
    if (arrayValue.isError()) {
        stack.push(arrayValue);
        return;
    }
    auto fromValue = stack.pop().getValue();
    if (fromValue.isError()) {
        stack.push(fromValue);
        return;
    }
    auto from = fromValue.getInt();

    int to = -1;
    if (numArgs > 2) {
        auto toValue = stack.pop().getValue();
        if (toValue.isError()) {
            stack.push(toValue);
            return;
        }
        to = toValue.getInt();
    }

    if (!arrayValue.isArray()) {
        stack.push(Value::makeError());
        return;
    }
    auto array = arrayValue.getArray();

    if (from < 0 || from >= (int)array->arraySize) {
        stack.push(Value::makeError());
        return;
    }

    if (numArgs <= 2) {
        to = array->arraySize;
    }
    if (to < 0 || to >= (int)array->arraySize) {
        stack.push(Value::makeError());
        return;
    }

    if (from > to) {
        stack.push(Value::makeError());
        return;
    }

    auto size = to - from;

    auto resultArrayValue = Value::makeArrayRef(size, array->arrayType, 0xe2d78c65);
    auto resultArray = array;

    for (int elementIndex = from; elementIndex < to; elementIndex++) {
        resultArray->values[elementIndex - from] = array->values[elementIndex];
    }

    stack.push(resultArrayValue);
}

void do_OPERATION_TYPE_ARRAY_ALLOCATE(EvalStack &stack) {
    auto sizeValue = stack.pop();
    if (sizeValue.isError()) {
        stack.push(sizeValue);
        return;
    }
    auto size = sizeValue.getInt();

    auto resultArrayValue = Value::makeArrayRef(size, defs_v3::ARRAY_TYPE_ANY, 0xe2d78c65);

    stack.push(resultArrayValue);
}

void do_OPERATION_TYPE_ARRAY_APPEND(EvalStack &stack) {
	auto arrayValue = stack.pop().getValue();
    if (arrayValue.isError()) {
        stack.push(arrayValue);
        return;
    }
    auto value = stack.pop().getValue();
    if (value.isError()) {
        stack.push(value);
        return;
    }

    if (!arrayValue.isArray()) {
        stack.push(Value::makeError());
        return;
    }

    auto array = arrayValue.getArray();
    auto resultArrayValue = Value::makeArrayRef(array->arraySize + 1, array->arrayType, 0x664c3199);
    auto resultArray = resultArrayValue.getArray();

    for (uint32_t elementIndex = 0; elementIndex < array->arraySize; elementIndex++) {
        resultArray->values[elementIndex] = array->values[elementIndex];
    }

    resultArray->values[array->arraySize] = value;

    stack.push(resultArrayValue);
}

void do_OPERATION_TYPE_ARRAY_INSERT(EvalStack &stack) {
	auto arrayValue = stack.pop().getValue();
    if (arrayValue.isError()) {
        stack.push(arrayValue);
        return;
    }
    auto positionValue = stack.pop().getValue();
    if (positionValue.isError()) {
        stack.push(positionValue);
        return;
    }
    auto value = stack.pop().getValue();
    if (value.isError()) {
        stack.push(value);
        return;
    }

    if (!arrayValue.isArray()) {
        stack.push(Value::makeError());
        return;
    }

    int err;
    auto position = positionValue.toInt32(&err);
    if (err != 0) {
        stack.push(Value::makeError());
        return;
    }

    auto array = arrayValue.getArray();
    auto resultArrayValue = Value::makeArrayRef(array->arraySize + 1, array->arrayType, 0xc4fa9cd9);
    auto resultArray = resultArrayValue.getArray();

    for (uint32_t elementIndex = 0; (int)elementIndex < position; elementIndex++) {
        resultArray->values[elementIndex] = array->values[elementIndex];
    }

    resultArray->values[position] = value;

    for (uint32_t elementIndex = position; elementIndex < array->arraySize; elementIndex++) {
        resultArray->values[elementIndex + 1] = array->values[elementIndex];
    }

    stack.push(resultArrayValue);
}

void do_OPERATION_TYPE_ARRAY_REMOVE(EvalStack &stack) {
	auto arrayValue = stack.pop().getValue();
    if (arrayValue.isError()) {
        stack.push(arrayValue);
        return;
    }
    auto positionValue = stack.pop().getValue();
    if (positionValue.isError()) {
        stack.push(positionValue);
        return;
    }

    if (!arrayValue.isArray()) {
        stack.push(Value::makeError());
        return;
    }

    int err;
    auto position = positionValue.toInt32(&err);
    if (err != 0) {
        stack.push(Value::makeError());
        return;
    }

    auto array = arrayValue.getArray();
    auto resultArrayValue = Value::makeArrayRef(array->arraySize - 1, array->arrayType, 0x40e9bb4b);
    auto resultArray = resultArrayValue.getArray();

    for (uint32_t elementIndex = 0; (int)elementIndex < position; elementIndex++) {
        resultArray->values[elementIndex] = array->values[elementIndex];
    }

    for (uint32_t elementIndex = position + 1; elementIndex < array->arraySize; elementIndex++) {
        resultArray->values[elementIndex - 1] = array->values[elementIndex];
    }

    stack.push(resultArrayValue);
}

void do_OPERATION_TYPE_ARRAY_CLONE(EvalStack &stack) {
	auto arrayValue = stack.pop().getValue();
    if (arrayValue.isError()) {
        stack.push(arrayValue);
        return;
    }

    auto resultArray = arrayValue.clone();

    stack.push(resultArray);
}

EvalOperation g_evalOperations[] = {
	do_OPERATION_TYPE_ADD,
	do_OPERATION_TYPE_SUB,
	do_OPERATION_TYPE_MUL,
	do_OPERATION_TYPE_DIV,
	do_OPERATION_TYPE_MOD,
	do_OPERATION_TYPE_LEFT_SHIFT,
	do_OPERATION_TYPE_RIGHT_SHIFT,
	do_OPERATION_TYPE_BINARY_AND,
	do_OPERATION_TYPE_BINARY_OR,
	do_OPERATION_TYPE_BINARY_XOR,
	do_OPERATION_TYPE_EQUAL,
	do_OPERATION_TYPE_NOT_EQUAL,
	do_OPERATION_TYPE_LESS,
	do_OPERATION_TYPE_GREATER,
	do_OPERATION_TYPE_LESS_OR_EQUAL,
	do_OPERATION_TYPE_GREATER_OR_EQUAL,
	do_OPERATION_TYPE_LOGICAL_AND,
	do_OPERATION_TYPE_LOGICAL_OR,
	do_OPERATION_TYPE_UNARY_PLUS,
	do_OPERATION_TYPE_UNARY_MINUS,
	do_OPERATION_TYPE_BINARY_ONE_COMPLEMENT,
	do_OPERATION_TYPE_NOT,
	do_OPERATION_TYPE_CONDITIONAL,
	do_OPERATION_TYPE_SYSTEM_GET_TICK,
	do_OPERATION_TYPE_FLOW_INDEX,
	do_OPERATION_TYPE_FLOW_IS_PAGE_ACTIVE,
    do_OPERATION_TYPE_FLOW_PAGE_TIMELINE_POSITION,
    do_OPERATION_TYPE_FLOW_MAKE_ARRAY_VALUE,
    do_OPERATION_TYPE_FLOW_MAKE_ARRAY_VALUE,
    do_OPERATION_TYPE_FLOW_LANGUAGES,
    do_OPERATION_TYPE_FLOW_TRANSLATE,
    do_OPERATION_TYPE_FLOW_PARSE_INTEGER,
    do_OPERATION_TYPE_FLOW_PARSE_FLOAT,
    do_OPERATION_TYPE_FLOW_PARSE_DOUBLE,
    do_OPERATION_TYPE_DATE_NOW,
    do_OPERATION_TYPE_DATE_TO_STRING,
    do_OPERATION_TYPE_DATE_FROM_STRING,
	do_OPERATION_TYPE_MATH_SIN,
	do_OPERATION_TYPE_MATH_COS,
	do_OPERATION_TYPE_MATH_LOG,
	do_OPERATION_TYPE_MATH_LOG10,
	do_OPERATION_TYPE_MATH_ABS,
	do_OPERATION_TYPE_MATH_FLOOR,
	do_OPERATION_TYPE_MATH_CEIL,
	do_OPERATION_TYPE_MATH_ROUND,
    do_OPERATION_TYPE_MATH_MIN,
    do_OPERATION_TYPE_MATH_MAX,
    do_OPERATION_TYPE_STRING_LENGTH,
    do_OPERATION_TYPE_STRING_SUBSTRING,
	do_OPERATION_TYPE_STRING_FIND,
	do_OPERATION_TYPE_STRING_PAD_START,
	do_OPERATION_TYPE_STRING_SPLIT,
	do_OPERATION_TYPE_ARRAY_LENGTH,
	do_OPERATION_TYPE_ARRAY_SLICE,
    do_OPERATION_TYPE_ARRAY_ALLOCATE,
	do_OPERATION_TYPE_ARRAY_APPEND,
	do_OPERATION_TYPE_ARRAY_INSERT,
	do_OPERATION_TYPE_ARRAY_REMOVE,
	do_OPERATION_TYPE_ARRAY_CLONE
};

} // namespace flow
} // namespace eez

