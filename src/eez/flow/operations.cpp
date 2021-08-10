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

#include <eez/flow/operations.h>

namespace eez {
namespace flow {

Value op_add(Assets *assets, const Value& a1, const Value& b1) {
	const Value &a = a1.getType() == VALUE_TYPE_VALUE_PTR ? *a1.pValueValue : a1;
	const Value &b = b1.getType() == VALUE_TYPE_VALUE_PTR ? *b1.pValueValue : b1;

	if (a.isAnyStringType() || b.isAnyStringType()) {
		Value value1 = a.toString(assets, 0x84eafaa8);
		Value value2 = b.toString(assets, 0xd273cab6);
		return Value::concatenateString(value1.getString(), value2.getString());
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
		return Value(a.int32Value + b.int32Value, VALUE_TYPE_INT32);
	}

	return Value();
}

Value op_sub(Assets *assets, const Value& a1, const Value& b1) {
	const Value &a = a1.getType() == VALUE_TYPE_VALUE_PTR ? *a1.pValueValue : a1;
	const Value &b = b1.getType() == VALUE_TYPE_VALUE_PTR ? *b1.pValueValue : b1;

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
		return Value(a.int32Value - b.int32Value, VALUE_TYPE_INT32);
	}

	return Value();
}

Value op_mul(Assets *assets, const Value& a1, const Value& b1) {
	const Value &a = a1.getType() == VALUE_TYPE_VALUE_PTR ? *a1.pValueValue : a1;
	const Value &b = b1.getType() == VALUE_TYPE_VALUE_PTR ? *b1.pValueValue : b1;

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
		return Value(a.int32Value * b.int32Value, VALUE_TYPE_INT32);
	}

	return Value();
}

Value op_div(Assets *assets, const Value& a1, const Value& b1) {
	const Value &a = a1.getType() == VALUE_TYPE_VALUE_PTR ? *a1.pValueValue : a1;
	const Value &b = b1.getType() == VALUE_TYPE_VALUE_PTR ? *b1.pValueValue : b1;

	if (a.isDouble() || b.isDouble()) {
		return Value(a.toDouble() / b.toDouble(), VALUE_TYPE_DOUBLE);
	}

	if (a.isFloat() || b.isFloat()) {
		return Value(a.toFloat() / b.toFloat(), VALUE_TYPE_FLOAT);
	}

	if (a.isInt64() || b.isInt64()) {
		return Value(a.toInt64() / b.toInt64(), VALUE_TYPE_INT64);
	}

	if (a.isInt32OrLess() && b.isInt32OrLess()) {
		return Value(a.int32Value / b.int32Value, VALUE_TYPE_INT32);
	}

	return Value();
}

Value op_mod(Assets *assets, const Value& a1, const Value& b1) {
	const Value &a = a1.getType() == VALUE_TYPE_VALUE_PTR ? *a1.pValueValue : a1;
	const Value &b = b1.getType() == VALUE_TYPE_VALUE_PTR ? *b1.pValueValue : b1;

	if (a.isInt64() || b.isInt64()) {
		return Value(a.toInt64() % b.toInt64(), VALUE_TYPE_INT64);
	}

	if (a.isInt32OrLess() && b.isInt32OrLess()) {
		return Value(a.int32Value % b.int32Value, VALUE_TYPE_INT32);
	}

	return Value();
}

Value op_eq(const Value& a1, const Value& b1) {
	const Value &a = a1.getType() == VALUE_TYPE_VALUE_PTR ? *a1.pValueValue : a1;
	const Value &b = b1.getType() == VALUE_TYPE_VALUE_PTR ? *b1.pValueValue : b1;

	return Value(a == b, VALUE_TYPE_BOOLEAN);
}

Value op_neq(const Value& a1, const Value& b1) {
	const Value &a = a1.getType() == VALUE_TYPE_VALUE_PTR ? *a1.pValueValue : a1;
	const Value &b = b1.getType() == VALUE_TYPE_VALUE_PTR ? *b1.pValueValue : b1;

	return Value(a != b, VALUE_TYPE_BOOLEAN);
}

Value op_less(const Value& a1, const Value& b1) {
	const Value &a = a1.getType() == VALUE_TYPE_VALUE_PTR ? *a1.pValueValue : a1;
	const Value &b = b1.getType() == VALUE_TYPE_VALUE_PTR ? *b1.pValueValue : b1;

	return Value(a.toDouble() < b.toDouble(), VALUE_TYPE_BOOLEAN);
}

Value op_great(const Value& a1, const Value& b1) {
	const Value &a = a1.getType() == VALUE_TYPE_VALUE_PTR ? *a1.pValueValue : a1;
	const Value &b = b1.getType() == VALUE_TYPE_VALUE_PTR ? *b1.pValueValue : b1;

	return Value(a.toDouble() > b.toDouble(), VALUE_TYPE_BOOLEAN);
}

Value op_less_eq(const Value& a1, const Value& b1) {
	const Value &a = a1.getType() == VALUE_TYPE_VALUE_PTR ? *a1.pValueValue : a1;
	const Value &b = b1.getType() == VALUE_TYPE_VALUE_PTR ? *b1.pValueValue : b1;

	return Value(a.toDouble() <= b.toDouble(), VALUE_TYPE_BOOLEAN);
}

Value op_great_eq(const Value& a1, const Value& b1) {
	const Value &a = a1.getType() == VALUE_TYPE_VALUE_PTR ? *a1.pValueValue : a1;
	const Value &b = b1.getType() == VALUE_TYPE_VALUE_PTR ? *b1.pValueValue : b1;

	return Value(a.toDouble() >= b.toDouble(), VALUE_TYPE_BOOLEAN);
}

bool do_OPERATION_TYPE_ADD(EvalStack &stack) {
	auto b = stack.pop();
	auto a = stack.pop();

	auto result = op_add(stack.assets, a, b);

	if (result.getType() == VALUE_TYPE_UNDEFINED) {
		return false;
	}

	stack.push(result);

	return true;
}

bool do_OPERATION_TYPE_SUB(EvalStack &stack) {
	auto b = stack.pop();
	auto a = stack.pop();

	auto result = op_sub(stack.assets, a, b);

	if (result.getType() == VALUE_TYPE_UNDEFINED) {
		return false;
	}

	stack.push(result);

	return true;
}

bool do_OPERATION_TYPE_MUL(EvalStack &stack) {
	auto b = stack.pop();
	auto a = stack.pop();

	auto result = op_mul(stack.assets, a, b);

	if (result.getType() == VALUE_TYPE_UNDEFINED) {
		return false;
	}

	stack.push(result);

	return true;
}

bool do_OPERATION_TYPE_DIV(EvalStack &stack) {
	auto b = stack.pop();
	auto a = stack.pop();

	auto result = op_div(stack.assets, a, b);

	if (result.getType() == VALUE_TYPE_UNDEFINED) {
		return false;
	}

	stack.push(result);

	return true;
}

bool do_OPERATION_TYPE_MOD(EvalStack &stack) {
	auto b = stack.pop();
	auto a = stack.pop();

	auto result = op_mod(stack.assets, a, b);

	if (result.getType() == VALUE_TYPE_UNDEFINED) {
		return false;
	}

	stack.push(result);

	return true;
}

bool do_OPERATION_TYPE_LEFT_SHIFT(EvalStack &stack) {
	return false;
}

bool do_OPERATION_TYPE_RIGHT_SHIFT(EvalStack &stack) {
	return false;
}

bool do_OPERATION_TYPE_BINARY_AND(EvalStack &stack) {
	return false;
}

bool do_OPERATION_TYPE_BINARY_OR(EvalStack &stack) {
	return false;
}

bool do_OPERATION_TYPE_BINARY_XOR(EvalStack &stack) {
	return false;
}

bool do_OPERATION_TYPE_EQUAL(EvalStack &stack) {
	auto b = stack.pop();
	auto a = stack.pop();
	stack.push(op_eq(a, b));
	return true;
}

bool do_OPERATION_TYPE_NOT_EQUAL(EvalStack &stack) {
	auto b = stack.pop();
	auto a = stack.pop();
	stack.push(a != b);
	return true;}

bool do_OPERATION_TYPE_LESS(EvalStack &stack) {
	auto b = stack.pop();
	auto a = stack.pop();
	stack.push(op_less(a, b));
	return true;
}

bool do_OPERATION_TYPE_GREATER(EvalStack &stack) {
	auto b = stack.pop();
	auto a = stack.pop();
	stack.push(op_great(a, b));
	return true;
}

bool do_OPERATION_TYPE_LESS_OR_EQUAL(EvalStack &stack) {
	auto b = stack.pop();
	auto a = stack.pop();
	stack.push(op_less_eq(a, b));
	return true;
}

bool do_OPERATION_TYPE_GREATER_OR_EQUAL(EvalStack &stack) {
	auto b = stack.pop();
	auto a = stack.pop();
	stack.push(op_great_eq(a, b));
	return true;
}

bool do_OPERATION_TYPE_LOGICAL_AND(EvalStack &stack) {
	auto b = stack.pop();
	auto a = stack.pop();

	if (a.getType() == VALUE_TYPE_VALUE_PTR) {
		a = *a.pValueValue;
	}

	if (b.getType() == VALUE_TYPE_VALUE_PTR) {
		b = *b.pValueValue;
	}

	stack.push(Value(a.toBool(stack.assets) && b.toBool(stack.assets), VALUE_TYPE_BOOLEAN));

	return true;
}

bool do_OPERATION_TYPE_LOGICAL_OR(EvalStack &stack) {
	return false;
}

bool do_OPERATION_TYPE_UNARY_PLUS(EvalStack &stack) {
	auto a = stack.pop();

	if (a.getType() == VALUE_TYPE_VALUE_PTR) {
		a = *a.pValueValue;
	}
	if (a.isDouble()) {
		stack.push(Value(a.getDouble(), VALUE_TYPE_DOUBLE));
		return true;
	}

	if (a.isFloat()) {
		stack.push(Value(a.toFloat(), VALUE_TYPE_FLOAT));
		return true;
	}

	if (a.isInt64()) {
		stack.push(Value((int64_t)a.getInt64(), VALUE_TYPE_INT64));
		return true;
	}

	if (a.isInt32()) {
		stack.push(Value((int)a.getInt32(), VALUE_TYPE_INT32));
		return true;
	}

	if (a.isInt16()) {
		stack.push(Value((int16_t)a.getInt16(), VALUE_TYPE_INT16));
		return true;
	}


	if (a.isInt8()) {
		stack.push(Value((int8_t)a.getInt8(), VALUE_TYPE_INT8));
		return true;
	}

	return false;
}

bool do_OPERATION_TYPE_UNARY_MINUS(EvalStack &stack) {
	auto a = stack.pop();

	if (a.getType() == VALUE_TYPE_VALUE_PTR) {
		a = *a.pValueValue;
	}
	if (a.isDouble()) {
		stack.push(Value(-a.getDouble(), VALUE_TYPE_DOUBLE));
		return true;
	}

	if (a.isFloat()) {
		stack.push(Value(-a.toFloat(), VALUE_TYPE_FLOAT));
		return true;
	}

	if (a.isInt64()) {
		stack.push(Value((int64_t)-a.getInt64(), VALUE_TYPE_INT64));
		return true;
	}

	if (a.isInt32()) {
		stack.push(Value((int)-a.getInt32(), VALUE_TYPE_INT32));
		return true;
	}

	if (a.isInt16()) {
		stack.push(Value((int16_t)-a.getInt16(), VALUE_TYPE_INT16));
		return true;
	}


	if (a.isInt8()) {
		stack.push(Value((int8_t)-a.getInt8(), VALUE_TYPE_INT8));
		return true;
	}

	return false;
}

bool do_OPERATION_TYPE_BINARY_ONE_COMPLEMENT(EvalStack &stack) {
	auto a = stack.pop();

	if (a.getType() == VALUE_TYPE_VALUE_PTR) {
		a = *a.pValueValue;
	}
	if (a.isInt64()) {
		stack.push(Value(~a.uint64Value, VALUE_TYPE_UINT64));
		return true;
	}

	if (a.isInt32()) {
		stack.push(Value(~a.uint32Value, VALUE_TYPE_UINT32));
		return true;
	}

	if (a.isInt16()) {
		stack.push(Value(~a.uint16Value, VALUE_TYPE_UINT16));
		return true;
	}


	if (a.isInt8()) {
		stack.push(Value(~a.uint8Value, VALUE_TYPE_UINT8));
		return true;
	}

	return false;
}

bool do_OPERATION_TYPE_NOT(EvalStack &stack) {
	auto aValue = stack.pop();

	int err;
	auto a = aValue.toBool(stack.assets, &err);
	if (err != 0) {
		return false;
	}

	stack.push(Value(!a, VALUE_TYPE_BOOLEAN));

	return true;
}

bool do_OPERATION_TYPE_CONDITIONAL(EvalStack &stack) {
	auto alternate = stack.pop();
	auto consequent = stack.pop();
	auto conditionValue = stack.pop();

	int err;
	auto condition = conditionValue.toBool(stack.assets, &err);
	if (err != 0) {
		return false;
	}

	stack.push(condition ? consequent : alternate);
	return true;
}

bool do_OPERATION_TYPE_FLOW_IT(EvalStack &stack) {
	auto a = stack.pop();
	
	int err;
	auto iteratorIndex = a.toInt32(&err);
	if (err != 0) {
		return false;
	}
	
	iteratorIndex = -iteratorIndex;
	if (iteratorIndex < 0 || iteratorIndex >= (int)MAX_ITERATORS) {
		return false;
	}
	
	stack.push(stack.iterators[iteratorIndex]);
	
	return true;
}

bool do_OPERATION_TYPE_MATH_SIN(EvalStack &stack) {
	return false;
}

bool do_OPERATION_TYPE_MATH_COS(EvalStack &stack) {
	return false;
}

bool do_OPERATION_TYPE_MATH_LOG(EvalStack &stack) {
	return false;
}

bool do_OPERATION_TYPE_STRING_FIND(EvalStack &stack) {
	auto b = stack.pop();
	auto a = stack.pop();

	if (a.getType() == VALUE_TYPE_VALUE_PTR) {
		a = *a.pValueValue;
	}

	if (b.getType() == VALUE_TYPE_VALUE_PTR) {
		b = *b.pValueValue;
	}

	const char *aStr = a.toString(stack.assets, 0xf616bf4d).getString();
	const char *bStr = b.toString(stack.assets, 0x81229133).getString();
	if (!aStr || !bStr) {
		stack.push(Value(-1, VALUE_TYPE_INT32));
	} else {
		const char *pos = strstr(aStr, bStr);
		if (!pos) {
			stack.push(Value(pos - aStr, VALUE_TYPE_INT32));
		} else {
			stack.push(Value(-1, VALUE_TYPE_INT32));
		}
	}

	return true;
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
	do_OPERATION_TYPE_FLOW_IT,
	do_OPERATION_TYPE_MATH_SIN,
	do_OPERATION_TYPE_MATH_COS,
	do_OPERATION_TYPE_MATH_LOG,
	do_OPERATION_TYPE_STRING_FIND
};

} // namespace flow
} // namespace eez
