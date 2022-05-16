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
#include <math.h>
#include <chrono>
#include <string>
#include <iostream>
#include <sstream>

// https://howardhinnant.github.io/date/date.html
#include <eez/libs/date.h>

#include <eez/gui/gui.h>

#include <eez/flow/flow.h>
#include <eez/flow/operations.h>

namespace eez {
namespace flow {

using namespace gui;

Value op_add(const Value& a1, const Value& b1) {
	auto a = a1.getValue();
	auto b = b1.getValue();

	if (a.isAnyStringType() || b.isAnyStringType()) {
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
	auto a = a1.getValue();
	auto b = b1.getValue();

	return Value(a.toDouble() - floor(a.toDouble() / b.toDouble()) * b.toDouble(), VALUE_TYPE_DOUBLE);
}

Value op_left_shift(const Value& a1, const Value& b1) {
	auto a = a1.getValue();
	auto b = b1.getValue();

	return Value((int)(a.toInt32() << b.toInt32()), VALUE_TYPE_INT32);
}

Value op_right_shift(const Value& a1, const Value& b1) {
	auto a = a1.getValue();
	auto b = b1.getValue();

	return Value((int)(a.toInt32() >> b.toInt32()), VALUE_TYPE_INT32);
}

Value op_binary_and(const Value& a1, const Value& b1) {
	auto a = a1.getValue();
	auto b = b1.getValue();

	return Value((int)(a.toBool() & b.toBool()), VALUE_TYPE_INT32);
}

Value op_binary_or(const Value& a1, const Value& b1) {
	auto a = a1.getValue();
	auto b = b1.getValue();

	return Value((int)(a.toBool() | b.toBool()), VALUE_TYPE_INT32);
}

Value op_binary_xor(const Value& a1, const Value& b1) {
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

	if (a.isAnyStringType() && b.isAnyStringType()) {
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

	if (a.isAnyStringType() && b.isAnyStringType()) {
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
	return Value(is_equal(a1, b1), VALUE_TYPE_BOOLEAN);
}

Value op_neq(const Value& a1, const Value& b1) {
	return Value(!is_equal(a1, b1), VALUE_TYPE_BOOLEAN);
}

Value op_less(const Value& a1, const Value& b1) {
	return Value(is_less(a1, b1), VALUE_TYPE_BOOLEAN);
}

Value op_great(const Value& a1, const Value& b1) {
	return Value(!is_less(a1, b1) && !is_equal(a1, b1), VALUE_TYPE_BOOLEAN);
}

Value op_less_eq(const Value& a1, const Value& b1) {
	return Value(is_less(a1, b1) || is_equal(a1, b1), VALUE_TYPE_BOOLEAN);
}

Value op_great_eq(const Value& a1, const Value& b1) {
	return Value(!is_less(a1, b1), VALUE_TYPE_BOOLEAN);
}

bool do_OPERATION_TYPE_ADD(EvalStack &stack) {
	auto b = stack.pop();
	auto a = stack.pop();

	auto result = op_add(a, b);

	if (result.getType() == VALUE_TYPE_UNDEFINED) {
		return false;
	}

	if (!stack.push(result)) {
		return false;
	}

	return true;
}

bool do_OPERATION_TYPE_SUB(EvalStack &stack) {
	auto b = stack.pop();
	auto a = stack.pop();

	auto result = op_sub(a, b);

	if (result.getType() == VALUE_TYPE_UNDEFINED) {
		return false;
	}

	if (!stack.push(result)) {
		return false;
	}

	return true;
}

bool do_OPERATION_TYPE_MUL(EvalStack &stack) {
	auto b = stack.pop();
	auto a = stack.pop();

	auto result = op_mul(a, b);

	if (result.getType() == VALUE_TYPE_UNDEFINED) {
		return false;
	}

	if (!stack.push(result)) {
		return false;
	}

	return true;
}

bool do_OPERATION_TYPE_DIV(EvalStack &stack) {
	auto b = stack.pop();
	auto a = stack.pop();

	auto result = op_div(a, b);

	if (result.getType() == VALUE_TYPE_UNDEFINED) {
		return false;
	}

	if (!stack.push(result)) {
		return false;
	}

	return true;
}

bool do_OPERATION_TYPE_MOD(EvalStack &stack) {
	auto b = stack.pop();
	auto a = stack.pop();

	auto result = op_mod(a, b);

	if (result.getType() == VALUE_TYPE_UNDEFINED) {
		return false;
	}

	if (!stack.push(result)) {
		return false;
	}

	return true;
}

bool do_OPERATION_TYPE_LEFT_SHIFT(EvalStack &stack) {
	auto b = stack.pop();
	auto a = stack.pop();

	auto result = op_left_shift(a, b);

	if (result.getType() == VALUE_TYPE_UNDEFINED) {
		return false;
	}

	if (!stack.push(result)) {
		return false;
	}

	return true;
}

bool do_OPERATION_TYPE_RIGHT_SHIFT(EvalStack &stack) {
	auto b = stack.pop();
	auto a = stack.pop();

	auto result = op_right_shift(a, b);

	if (result.getType() == VALUE_TYPE_UNDEFINED) {
		return false;
	}

	if (!stack.push(result)) {
		return false;
	}

	return true;
}

bool do_OPERATION_TYPE_BINARY_AND(EvalStack &stack) {
	auto b = stack.pop();
	auto a = stack.pop();

	auto result = op_binary_and(a, b);

	if (result.getType() == VALUE_TYPE_UNDEFINED) {
		return false;
	}

	if (!stack.push(result)) {
		return false;
	}

	return true;
}

bool do_OPERATION_TYPE_BINARY_OR(EvalStack &stack) {
	auto b = stack.pop();
	auto a = stack.pop();

	auto result = op_binary_or(a, b);

	if (result.getType() == VALUE_TYPE_UNDEFINED) {
		return false;
	}

	if (!stack.push(result)) {
		return false;
	}

	return true;
}

bool do_OPERATION_TYPE_BINARY_XOR(EvalStack &stack) {
	auto b = stack.pop();
	auto a = stack.pop();

	auto result = op_binary_xor(a, b);

	if (result.getType() == VALUE_TYPE_UNDEFINED) {
		return false;
	}

	if (!stack.push(result)) {
		return false;
	}

	return true;
}

bool do_OPERATION_TYPE_EQUAL(EvalStack &stack) {
	auto b = stack.pop();
	auto a = stack.pop();
	if (!stack.push(op_eq(a, b))) {
		return false;
	}
	return true;
}

bool do_OPERATION_TYPE_NOT_EQUAL(EvalStack &stack) {
	auto b = stack.pop();
	auto a = stack.pop();
	if (!stack.push(op_neq(a, b))) {
		return false;
	}
	return true;}

bool do_OPERATION_TYPE_LESS(EvalStack &stack) {
	auto b = stack.pop();
	auto a = stack.pop();
	if (!stack.push(op_less(a, b))) {
		return false;
	}
	return true;
}

bool do_OPERATION_TYPE_GREATER(EvalStack &stack) {
	auto b = stack.pop();
	auto a = stack.pop();
	if (!stack.push(op_great(a, b))) {
		return false;
	}
	return true;
}

bool do_OPERATION_TYPE_LESS_OR_EQUAL(EvalStack &stack) {
	auto b = stack.pop();
	auto a = stack.pop();
	if (!stack.push(op_less_eq(a, b))) {
		return false;
	}
	return true;
}

bool do_OPERATION_TYPE_GREATER_OR_EQUAL(EvalStack &stack) {
	auto b = stack.pop();
	auto a = stack.pop();
	if (!stack.push(op_great_eq(a, b))) {
		return false;
	}
	return true;
}

bool do_OPERATION_TYPE_LOGICAL_AND(EvalStack &stack) {
	auto b = stack.pop().getValue();
	auto a = stack.pop().getValue();

	if (!stack.push(Value(a.toBool() && b.toBool(), VALUE_TYPE_BOOLEAN))) {
		return false;
	}

	return true;
}

bool do_OPERATION_TYPE_LOGICAL_OR(EvalStack &stack) {
	auto b = stack.pop().getValue();
	auto a = stack.pop().getValue();

	if (!stack.push(Value(a.toBool() || b.toBool(), VALUE_TYPE_BOOLEAN))) {
		return false;
	}

	return true;
}

bool do_OPERATION_TYPE_UNARY_PLUS(EvalStack &stack) {
	auto a = stack.pop().getValue();

	if (a.isDouble()) {
		if (!stack.push(Value(a.getDouble(), VALUE_TYPE_DOUBLE))) {
			return false;
		}
		return true;
	}

	if (a.isFloat()) {
		if (!stack.push(Value(a.toFloat(), VALUE_TYPE_FLOAT))) {
			return false;
		}
		return true;
	}

	if (a.isInt64()) {
		if (!stack.push(Value((int64_t)a.getInt64(), VALUE_TYPE_INT64))) {
			return false;
		}
		return true;
	}

	if (a.isInt32()) {
		if (!stack.push(Value((int)a.getInt32(), VALUE_TYPE_INT32))) {
			return false;
		}
		return true;
	}

	if (a.isInt16()) {
		if (!stack.push(Value((int16_t)a.getInt16(), VALUE_TYPE_INT16))) {
			return false;
		}
		return true;
	}


	if (a.isInt8()) {
		if (!stack.push(Value((int8_t)a.getInt8(), VALUE_TYPE_INT8))) {
			return false;
		}
		return true;
	}

	return false;
}

bool do_OPERATION_TYPE_UNARY_MINUS(EvalStack &stack) {
	auto a = stack.pop().getValue();

	if (a.isDouble()) {
		if (!stack.push(Value(-a.getDouble(), VALUE_TYPE_DOUBLE))) {
			return false;
		}
		return true;
	}

	if (a.isFloat()) {
		if (!stack.push(Value(-a.toFloat(), VALUE_TYPE_FLOAT))) {
			return false;
		}
		return true;
	}

	if (a.isInt64()) {
		if (!stack.push(Value((int64_t)-a.getInt64(), VALUE_TYPE_INT64))) {
			return false;
		}
		return true;
	}

	if (a.isInt32()) {
		if (!stack.push(Value((int)-a.getInt32(), VALUE_TYPE_INT32))) {
			return false;
		}
		return true;
	}

	if (a.isInt16()) {
		if (!stack.push(Value((int16_t)-a.getInt16(), VALUE_TYPE_INT16))) {
			return false;
		}
		return true;
	}


	if (a.isInt8()) {
		if (!stack.push(Value((int8_t)-a.getInt8(), VALUE_TYPE_INT8))) {
			return false;
		}
		return true;
	}

	return false;
}

bool do_OPERATION_TYPE_BINARY_ONE_COMPLEMENT(EvalStack &stack) {
	auto a = stack.pop().getValue();

	if (a.isInt64()) {
		if (!stack.push(Value(~a.uint64Value, VALUE_TYPE_UINT64))) {
			return false;
		}
		return true;
	}

	if (a.isInt32()) {
		if (!stack.push(Value(~a.uint32Value, VALUE_TYPE_UINT32))) {
			return false;
		}
		return true;
	}

	if (a.isInt16()) {
		if (!stack.push(Value(~a.uint16Value, VALUE_TYPE_UINT16))) {
			return false;
		}
		return true;
	}


	if (a.isInt8()) {
		if (!stack.push(Value(~a.uint8Value, VALUE_TYPE_UINT8))) {
			return false;
		}
		return true;
	}

	return false;
}

bool do_OPERATION_TYPE_NOT(EvalStack &stack) {
	auto aValue = stack.pop();

	int err;
	auto a = aValue.toBool(&err);
	if (err != 0) {
		return false;
	}

	if (!stack.push(Value(!a, VALUE_TYPE_BOOLEAN))) {
		return false;
	}

	return true;
}

bool do_OPERATION_TYPE_CONDITIONAL(EvalStack &stack) {
    auto alternate = stack.pop();
	auto consequent = stack.pop();
	auto conditionValue = stack.pop();

	int err;
	auto condition = conditionValue.toBool(&err);
	if (err != 0) {
		return false;
	}

	if (!stack.push(condition ? consequent : alternate)) {
		return false;
	}

	return true;
}

bool do_OPERATION_TYPE_SYSTEM_GET_TICK(EvalStack &stack) {
	if (!stack.push(Value(millis(), VALUE_TYPE_UINT32))) {
		return false;
	}
	return true;
}

bool do_OPERATION_TYPE_FLOW_INDEX(EvalStack &stack) {
	if (!stack.iterators) {
		return false;
	}

	auto a = stack.pop();

	int err;
	auto iteratorIndex = a.toInt32(&err);
	if (err != 0) {
		return false;
	}

	using eez::gui::MAX_ITERATORS;

	iteratorIndex = iteratorIndex;
	if (iteratorIndex < 0 || iteratorIndex >= (int)MAX_ITERATORS) {
		return false;
	}

	if (!stack.push(stack.iterators[iteratorIndex])) {
		return false;
	}

	return true;
}

bool do_OPERATION_TYPE_FLOW_IS_PAGE_ACTIVE(EvalStack &stack) {
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

	if (!stack.push(Value(isActive, VALUE_TYPE_BOOLEAN))) {
		return false;
	}

	return true;
}

bool do_OPERATION_TYPE_FLOW_MAKE_ARRAY_VALUE(EvalStack &stack) {
    auto arrayTypeValue = stack.pop();
    auto arraySizeValue = stack.pop();

    int arrayType = arrayTypeValue.getInt();
    int arraySize = arraySizeValue.getInt();

    auto arrayValue = Value::makeArrayRef(arraySize, arrayType, 0x837260d4);

    auto array = arrayValue.getArray();

    for (int i = 0; i < arraySize; i++) {
        array->values[i] = stack.pop().getValue();
    }

    if (!stack.push(arrayValue)) {
        return false;
    }

    return true;
}

bool do_OPERATION_TYPE_FLOW_LANGUAGES(EvalStack &stack) {
    auto languages = stack.flowState->assets->languages;

    auto arrayValue = Value::makeArrayRef(languages.count, VALUE_TYPE_STRING, 0xff4787fc);

    auto array = arrayValue.getArray();

    for (int i = 0; i < languages.count; i++) {
        array->values[i] = Value((const char *)(languages[i]->languageID));
    }

    if (!stack.push(arrayValue)) {
        return false;
    }

    return true;
}

bool do_OPERATION_TYPE_FLOW_TRANSLATE(EvalStack &stack) {
    auto textResourceIndexValue = stack.pop();

    int textResourceIndex = textResourceIndexValue.getInt();

    int languageIndex = g_selectedLanguage;

    auto languages = stack.flowState->assets->languages;
    if (languageIndex >= 0 && languageIndex < languages.count) {
        auto translations = languages[languageIndex]->translations;
        if (textResourceIndex >= 0 && textResourceIndex < translations.count) {
            if (!stack.push(translations[textResourceIndex])) {
                return false;
            }
            return true;
        }
    }

    if (!stack.push("")) {
        return false;
    }

    return true;
}

bool do_OPERATION_TYPE_DATE_NOW(EvalStack &stack) {
    using namespace std::chrono;
    milliseconds ms = duration_cast<milliseconds>(system_clock::now().time_since_epoch());

    if (!stack.push(Value((double)ms.count(), VALUE_TYPE_DATE))) {
        return false;
    }

	return true;
}

bool do_OPERATION_TYPE_DATE_TO_STRING(EvalStack &stack) {
	auto a = stack.pop().getValue();
    if (a.getType() != VALUE_TYPE_DATE) {
        return false;
    }

    using namespace std;
    using namespace std::chrono;
    using namespace date;

    auto tp = system_clock::time_point(milliseconds((long long)a.getDouble()));

    stringstream out;
    out << tp << endl;

    if (!stack.push(Value::makeStringRef(out.str().c_str(), -1, 0xbe440ec8))) {
        return false;
    }

	return true;
}

bool do_OPERATION_TYPE_DATE_FROM_STRING(EvalStack &stack) {
	auto a = stack.pop().getValue();

    Value dateStrValue = a.toString(0x99cb1a93);

    using namespace std;
    using namespace std::chrono;
    using namespace date;

    istringstream in{dateStrValue.getString()};

    system_clock::time_point tp;
    in >> date::parse("%Y-%m-%d %T", tp);

    milliseconds ms = duration_cast<milliseconds>(tp.time_since_epoch());
    if (!stack.push(Value((double)ms.count(), VALUE_TYPE_DATE))) {
        return false;
    }

	return true;
}

bool do_OPERATION_TYPE_MATH_SIN(EvalStack &stack) {
	auto a = stack.pop().getValue();

	if (a.isDouble()) {
		if (!stack.push(Value(sin(a.getDouble()), VALUE_TYPE_DOUBLE))) {
			return false;
		}
		return true;
	}

	if (a.isFloat()) {
		if (!stack.push(Value(sinf(a.toFloat()), VALUE_TYPE_FLOAT))) {
			return false;
		}
		return true;
	}

	if (a.isInt64()) {
		if (!stack.push(Value(sin(a.toInt64()), VALUE_TYPE_FLOAT))) {
			return false;
		}
		return true;
	}

	if (a.isInt32OrLess()) {
		if (!stack.push(Value(sinf(a.int32Value), VALUE_TYPE_FLOAT))) {
			return false;
		}
		return true;
	}

	return false;
}

bool do_OPERATION_TYPE_MATH_COS(EvalStack &stack) {
	auto a = stack.pop().getValue();

	if (a.isDouble()) {
		if (!stack.push(Value(cos(a.getDouble()), VALUE_TYPE_DOUBLE))) {
			return false;
		}
		return true;
	}

	if (a.isFloat()) {
		if (!stack.push(Value(cosf(a.toFloat()), VALUE_TYPE_FLOAT))) {
			return false;
		}
		return true;
	}

	if (a.isInt64()) {
		if (!stack.push(Value(cos(a.toInt64()), VALUE_TYPE_FLOAT))) {
			return false;
		}
		return true;
	}

	if (a.isInt32OrLess()) {
		if (!stack.push(Value(cosf(a.int32Value), VALUE_TYPE_FLOAT))) {
			return false;
		}
		return true;
	}

	return false;
}

bool do_OPERATION_TYPE_MATH_LOG(EvalStack &stack) {
	auto a = stack.pop().getValue();

	if (a.isDouble()) {
		if (!stack.push(Value(log(a.getDouble()), VALUE_TYPE_DOUBLE))) {
			return false;
		}
		return true;
	}

	if (a.isFloat()) {
		if (!stack.push(Value(logf(a.toFloat()), VALUE_TYPE_FLOAT))) {
			return false;
		}
		return true;
	}

	if (a.isInt64()) {
		if (!stack.push(Value(log(a.toInt64()), VALUE_TYPE_FLOAT))) {
			return false;
		}
		return true;
	}

	if (a.isInt32OrLess()) {
		if (!stack.push(Value(logf(a.int32Value), VALUE_TYPE_FLOAT))) {
			return false;
		}
		return true;
	}

	return false;
}

bool do_OPERATION_TYPE_MATH_LOG10(EvalStack &stack) {
	auto a = stack.pop().getValue();

	if (a.isDouble()) {
		if (!stack.push(Value(log10(a.getDouble()), VALUE_TYPE_DOUBLE))) {
			return false;
		}
		return true;
	}

	if (a.isFloat()) {
		if (!stack.push(Value(log10f(a.toFloat()), VALUE_TYPE_FLOAT))) {
			return false;
		}
		return true;
	}

	if (a.isInt64()) {
		if (!stack.push(Value(log10(a.toInt64()), VALUE_TYPE_FLOAT))) {
			return false;
		}
		return true;
	}

	if (a.isInt32OrLess()) {
		if (!stack.push(Value(log10f(a.int32Value), VALUE_TYPE_FLOAT))) {
			return false;
		}
		return true;
	}

	return false;
}

bool do_OPERATION_TYPE_MATH_ABS(EvalStack &stack) {
	auto a = stack.pop().getValue();

	if (a.isDouble()) {
		if (!stack.push(Value(abs(a.getDouble()), VALUE_TYPE_DOUBLE))) {
			return false;
		}
		return true;
	}

	if (a.isFloat()) {
		if (!stack.push(Value(abs(a.toFloat()), VALUE_TYPE_FLOAT))) {
			return false;
		}
		return true;
	}

	if (a.isInt64()) {
		if (!stack.push(Value((int64_t)abs(a.getInt64()), VALUE_TYPE_INT64))) {
			return false;
		}
		return true;
	}

	if (a.isInt32()) {
		if (!stack.push(Value((int)abs(a.getInt32()), VALUE_TYPE_INT32))) {
			return false;
		}
		return true;
	}

	if (a.isInt16()) {
		if (!stack.push(Value(abs(a.getInt16()), VALUE_TYPE_INT16))) {
			return false;
		}
		return true;
	}


	if (a.isInt8()) {
		if (!stack.push(Value(abs(a.getInt8()), VALUE_TYPE_INT8))) {
			return false;
		}
		return true;
	}

	return false;
}

bool do_OPERATION_TYPE_MATH_FLOOR(EvalStack &stack) {
	auto a = stack.pop().getValue();

	if (a.isDouble()) {
		if (!stack.push(Value(floor(a.getDouble()), VALUE_TYPE_DOUBLE))) {
			return false;
		}
		return true;
	}

	if (a.isFloat()) {
		if (!stack.push(Value(floorf(a.toFloat()), VALUE_TYPE_FLOAT))) {
			return false;
		}
		return true;
	}

	return false;
}

bool do_OPERATION_TYPE_MATH_CEIL(EvalStack &stack) {
	auto a = stack.pop().getValue();

	if (a.isDouble()) {
		if (!stack.push(Value(ceil(a.getDouble()), VALUE_TYPE_DOUBLE))) {
			return false;
		}
		return true;
	}

	if (a.isFloat()) {
		if (!stack.push(Value(ceilf(a.toFloat()), VALUE_TYPE_FLOAT))) {
			return false;
		}
		return true;
	}

	return false;
}

float roundN(float value, unsigned int numDigits) {
  float pow_10 = pow(10.0f, numDigits);
  return round(value * pow_10) / pow_10;
}

double roundN(double value, unsigned int numDigits) {
  float pow_10 = pow(10.0f, numDigits);
  return round(value * pow_10) / pow_10;
}

bool do_OPERATION_TYPE_MATH_ROUND(EvalStack &stack) {
    auto numArgs = stack.pop().getInt();
	auto a = stack.pop().getValue();

    unsigned int numDigits;
    if (numArgs > 1) {
        auto b = stack.pop().getValue();
        numDigits = b.toInt32();
    } else {
        numDigits = 0;
    }

	if (a.isDouble()) {
		if (!stack.push(Value(roundN(a.getDouble(), numDigits), VALUE_TYPE_DOUBLE))) {
			return false;
		}
		return true;
	}

	if (a.isFloat()) {
		if (!stack.push(Value(roundN(a.toFloat(), numDigits), VALUE_TYPE_FLOAT))) {
			return false;
		}
		return true;
	}

    if (a.isInt32OrLess()) {
		if (!stack.push(a)) {
			return false;
		}
		return true;
    }

	return false;
}

bool do_OPERATION_TYPE_MATH_MIN(EvalStack &stack) {
    auto numArgs = stack.pop().getInt();

    double minValue = INFINITY;

    for (int i = 0; i < numArgs; i++) {
        auto value = stack.pop().getValue().toDouble();
        if (value < minValue) {
            minValue = value;
        }
    }

	if (!stack.push(Value(minValue, VALUE_TYPE_DOUBLE))) {
		return false;
	}

	return true;
}

bool do_OPERATION_TYPE_MATH_MAX(EvalStack &stack) {
    auto numArgs = stack.pop().getInt();

    double maxValue = -INFINITY;

    for (int i = 0; i < numArgs; i++) {
        auto value = stack.pop().getValue().toDouble();
        if (value > maxValue) {
            maxValue = value;
        }
    }

	if (!stack.push(Value(maxValue, VALUE_TYPE_DOUBLE))) {
		return false;
	}

	return true;
}

bool do_OPERATION_TYPE_STRING_FIND(EvalStack &stack) {
	auto b = stack.pop().getValue();
	auto a = stack.pop().getValue();

	Value aStr = a.toString(0xf616bf4d);
	Value bStr = b.toString(0x81229133);
	if (!aStr.getString() || !bStr.getString()) {
		if (!stack.push(Value(-1, VALUE_TYPE_INT32))) {
			return false;
		}
	} else {
		const char *pos = strstr(aStr.getString(), bStr.getString());
		if (!pos) {
			if (!stack.push(Value((int)(pos - aStr.getString()), VALUE_TYPE_INT32))) {
				return false;
			}
		} else {
			if (!stack.push(Value(-1, VALUE_TYPE_INT32))) {
				return false;
			}
		}
	}

	return true;
}

bool do_OPERATION_TYPE_STRING_PAD_START(EvalStack &stack) {
	auto c = stack.pop().getValue();
	auto b = stack.pop().getValue();
	auto a = stack.pop().getValue();

	auto str = a.toString(0xcf6aabe6);
	if (!str.getString()) {
		return false;
	}
	int strLen = strlen(str.getString());

	int err;
	int targetLength = b.toInt32(&err);
	if (err) {
		return false;
	}
	if (targetLength < strLen) {
		targetLength = strLen;
	}

	auto padStr = c.toString(0x81353bd7);
	if (!padStr.getString()) {
		return false;
	}
	int padStrLen = strlen(padStr.getString());

	Value resultValue = eez::gui::Value::makeStringRef("", targetLength, 0xf43b14dd);
	if (resultValue.type == VALUE_TYPE_NULL) {
		return false;
	}
	char *resultStr = (char *)resultValue.getString();

	auto n = targetLength - strLen;
	stringCopy(resultStr + (targetLength - strLen), strLen + 1, str.getString());

	for (int i = 0; i < n; i++) {
		resultStr[i] = padStr.getString()[i % padStrLen];
	}

	if (!stack.push(resultValue)) {
		return false;
	}

	return true;
}

bool do_OPERATION_TYPE_STRING_SPLIT(EvalStack &stack) {
	// TODO
	return false;
}

bool do_OPERATION_TYPE_ARRAY_LENGTH(EvalStack &stack) {
    auto a = stack.pop().getValue();

    if (a.getType() == VALUE_TYPE_ARRAY || a.getType() == VALUE_TYPE_ARRAY_REF) {
        auto array = a.getArray();

        if (!stack.push(Value(array->arraySize, VALUE_TYPE_UINT32))) {
		    return false;
	    }

        return true;
    }

	return false;
}

bool do_OPERATION_TYPE_ARRAY_SLICE(EvalStack &stack) {
	// TODO
	return false;
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
    do_OPERATION_TYPE_FLOW_MAKE_ARRAY_VALUE,
    do_OPERATION_TYPE_FLOW_MAKE_ARRAY_VALUE,
    do_OPERATION_TYPE_FLOW_LANGUAGES,
    do_OPERATION_TYPE_FLOW_TRANSLATE,
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
	do_OPERATION_TYPE_STRING_FIND,
	do_OPERATION_TYPE_STRING_PAD_START,
	do_OPERATION_TYPE_STRING_SPLIT,
	do_OPERATION_TYPE_ARRAY_LENGTH,
	do_OPERATION_TYPE_ARRAY_SLICE
};

} // namespace flow
} // namespace eez

