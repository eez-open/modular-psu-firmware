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
#include <assert.h>

#include <eez/alloc.h>
#include <eez/flow.h>
#include <eez/system.h>
#include <eez/flow_defs_v3.h>
#include <eez/scripting.h>

#define FLOW_DEBUG 0

using namespace eez::gui;

namespace eez {
namespace flow {

static Assets *g_assets;

static const int UNDEFINED_VALUE_INDEX = 0;
static const int NULL_VALUE_INDEX = 1;

struct FlowState {
	unsigned flowIndex;
	Value values[1];
};
FlowState *g_mainPageFlowState;

static const unsigned QUEUE_SIZE = 100;
static struct {
    FlowState *flowState;
	unsigned componentIndex;
} g_queue[QUEUE_SIZE];
static unsigned g_queueHead;
static unsigned g_queueTail;
static bool g_queueIsFull = false;

void addToQueue(FlowState *flowState, unsigned componentIndex) {
	g_queue[g_queueTail].flowState = flowState;
	g_queue[g_queueTail].componentIndex = componentIndex;

	g_queueTail = (g_queueTail + 1) % QUEUE_SIZE;

	if (g_queueHead == g_queueTail) {
		g_queueIsFull = true;
	}
}

bool removeFromQueue(FlowState *&flowState, unsigned &componentIndex) {
	if (g_queueHead == g_queueTail && !g_queueIsFull) {
		return false;
	}

	flowState = g_queue[g_queueHead].flowState;
	componentIndex = g_queue[g_queueHead].componentIndex;

	g_queueHead = (g_queueHead + 1) % QUEUE_SIZE;
	g_queueIsFull = false;

	return true;
}

bool isReadyToRun(Assets *assets, FlowState *flowState, unsigned componentIndex) {
	auto flowDefinition = assets->flowDefinition.ptr(assets);
	auto flow = flowDefinition->flows.item(assets, flowState->flowIndex);
	auto component = flow->components.item(assets, componentIndex);

	if (component->type < 1000) {
		return false;
	}

	// check if all inputs are defined
	for (unsigned inputIndex = 0; inputIndex < component->inputs.count; inputIndex++) {
		auto inputValueIndex = component->inputs.ptr(assets)[inputIndex];

		auto &value = flowState->values[inputValueIndex];
		if (value.type_ == VALUE_TYPE_UNDEFINED) {
			return false;
		}
	}

	return true;
}

void pingComponent(Assets *assets, FlowState *flowState, unsigned componentIndex) {
    if (isReadyToRun(assets, flowState, componentIndex)) {
        addToQueue(flowState, componentIndex);
    }
}

void propagateValue(Assets *assets, FlowState *flowState, ComponentOutput &componentOutput, const gui::Value &value) {
	for (unsigned connectionIndex = 0; connectionIndex < componentOutput.connections.count; connectionIndex++) {
		auto connection = componentOutput.connections.item(assets, connectionIndex);
		flowState->values[connection->targetInputIndex] = value;
		pingComponent(assets, flowState, connection->targetComponentIndex);
	}
}

////////////////////////////////////////////////////////////////////////////////

struct EvalStack {
	Value stack[100];
	int sp = 0;

	void push(const Value &value) {
		stack[sp++] = value;
	}

	void push(Value *pValue) {
		stack[sp++] = Value(pValue, VALUE_TYPE_VALUE_PTR);
	}

	Value pop() {
		return stack[--sp];
	}

};

bool do_OPERATION_TYPE_ADD(EvalStack &stack) {
	auto a = stack.pop();
	auto b = stack.pop();

	if (a.getType() == VALUE_TYPE_VALUE_PTR) {
		a = *a.pValue_;
	}

	if (b.getType() == VALUE_TYPE_VALUE_PTR) {
		b = *b.pValue_;
	}

	if (a.isDouble() || b.isDouble()) {
		stack.push(Value(a.toDouble() + b.toDouble(), VALUE_TYPE_DOUBLE));
		return true;
	}

	if (a.isFloat() || b.isFloat()) {
		stack.push(Value(a.toFloat() + b.toFloat(), VALUE_TYPE_FLOAT));
		return true;
	}

	if (a.isInt64() || b.isInt64()) {
		stack.push(Value(a.toInt64() + b.toInt64(), VALUE_TYPE_INT64));
		return true;
	}

	if (a.isInt32OrLess() && b.isInt32OrLess()) {
		stack.push(Value(a.int32_ + b.int32_, VALUE_TYPE_INT32));
		return true;
	}

	return false;
}

bool do_OPERATION_TYPE_SUB(EvalStack &stack) {
	return false;
}

bool do_OPERATION_TYPE_MUL(EvalStack &stack) {
	return false;
}

bool do_OPERATION_TYPE_DIV(EvalStack &stack) {
	return false;
}

bool do_OPERATION_TYPE_MOD(EvalStack &stack) {
	return false;
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
	return false;
}

bool do_OPERATION_TYPE_NOT_EQUAL(EvalStack &stack) {
	return false;
}

bool do_OPERATION_TYPE_LESS(EvalStack &stack) {
	return false;
}

bool do_OPERATION_TYPE_GREATER(EvalStack &stack) {
	return false;
}

bool do_OPERATION_TYPE_LESS_OR_EQUAL(EvalStack &stack) {
	return false;
}

bool do_OPERATION_TYPE_GREATER_OR_EQUAL(EvalStack &stack) {
	return false;
}

bool do_OPERATION_TYPE_LOGICAL_AND(EvalStack &stack) {
	return false;
}

bool do_OPERATION_TYPE_LOGICAL_OR(EvalStack &stack) {
	return false;
}

bool do_OPERATION_TYPE_UNARY_PLUS(EvalStack &stack) {
	return false;
}

bool do_OPERATION_TYPE_UNARY_MINUS(EvalStack &stack) {
	return false;
}

bool do_OPERATION_TYPE_BINARY_ONE_COMPLEMENT(EvalStack &stack) {
	return false;
}

bool do_OPERATION_TYPE_NOT(EvalStack &stack) {
	return false;
}

bool do_OPERATION_TYPE_CONDITIONAL(EvalStack &stack) {
	return false;
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

typedef bool (*EvalOperation)(EvalStack &);

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
	do_OPERATION_TYPE_MATH_SIN,
	do_OPERATION_TYPE_MATH_COS,
	do_OPERATION_TYPE_MATH_LOG,
};

static bool evalExpression(Assets *assets, FlowState *flowState, uint8_t *instructions, EvalStack &stack, int *numInstructionBytes = nullptr) {
	auto flowDefinition = assets->flowDefinition.ptr(assets);
	auto flow = flowDefinition->flows.item(assets, flowState->flowIndex);

	int i = 0;
	while (true) {
		uint16_t instruction = instructions[i] + (instructions[i + 1] << 8);
		auto instructionType = instruction & EXPR_EVAL_INSTRUCTION_TYPE_MASK;
		auto instructionArg = instruction & EXPR_EVAL_INSTRUCTION_PARAM_MASK;
		if (instructionType == EXPR_EVAL_INSTRUCTION_TYPE_PUSH_CONSTANT) {
			stack.push(*flowDefinition->constants.item(assets, instructionArg));
		} else if (instructionType == EXPR_EVAL_INSTRUCTION_TYPE_PUSH_INPUT) {
			stack.push(flowState->values[instructionArg]);
		} else if (instructionType == EXPR_EVAL_INSTRUCTION_TYPE_PUSH_LOCAL_VAR) {
			stack.push(&flowState->values[flow->nInputValues + instructionArg]);
		} else if (instructionType == EXPR_EVAL_INSTRUCTION_TYPE_PUSH_GLOBAL_VAR) {
			stack.push(flowDefinition->globalVariables.item(assets, instructionArg));
		} else if (instructionType == EXPR_EVAL_INSTRUCTION_TYPE_OPERATION) {
			if (!g_evalOperations[instructionArg](stack)) {
				return false;
			}
		} else {
			i += 2;
			break;
		}

		i += 2;
	}

	if (numInstructionBytes) {
		*numInstructionBytes = i;
	}

	return true;
}

static bool evalExpression(Assets *assets, FlowState *flowState, uint8_t *instructions, Value &result) {
	EvalStack stack;

	if (evalExpression(assets, flowState, instructions, stack)) {
		if (stack.sp == 1) {
			auto finalResult = stack.pop();

			if (finalResult.getType() == VALUE_TYPE_VALUE_PTR) {
				result = *finalResult.pValue_;
			} else {
				result = finalResult;
			}

			return true;
		}
	}

	result = Value();
	return false;
}

////////////////////////////////////////////////////////////////////////////////

static bool evalAssignableExpression(Assets *assets, FlowState *flowState, uint8_t *instructions, Value **result, int *numInstructionBytes = nullptr) {
	EvalStack stack;

	if (evalExpression(assets, flowState, instructions, stack, numInstructionBytes)) {
		if (stack.sp == 1) {
			auto finalResult = stack.pop();
			if (finalResult.getType() == VALUE_TYPE_VALUE_PTR) {
				*result = finalResult.pValue_;
				return true;
			}
		}
	}

	*result = nullptr;
	return false;
}

////////////////////////////////////////////////////////////////////////////////

void assignValue(Value &dstValue, const Value &srcValue) {
	if (dstValue.isInt32OrLess()) {
		dstValue.int32_ = srcValue.toInt32();
	} else if (dstValue.isFloat()) {
		dstValue.float_ = srcValue.toFloat();
	} else if (dstValue.isDouble()) {
		dstValue.float_ = srcValue.toDouble();
	}
	// TODO
}

////////////////////////////////////////////////////////////////////////////////

void throwError() {
	// TODO
}

void executeComponent(Assets *assets, FlowState *flowState, unsigned componentIndex) {
	auto flowDefinition = assets->flowDefinition.ptr(assets);
	auto flow = flowDefinition->flows.item(assets, flowState->flowIndex);
	auto component = flow->components.item(assets, componentIndex);
    if (component->type == defs_v3::COMPONENT_TYPE_START_ACTION) {
#if FLOW_DEBUG
	    printf("Execute START component at index = %d\n", componentIndex);
#endif
    } else if (component->type == defs_v3::COMPONENT_TYPE_DELAY_ACTION) {
#if FLOW_DEBUG
	    printf("Execute DELAY component at index = %d\n", componentIndex);
#endif
		auto propertyValue = component->propertyValues.item(assets, defs_v3::DELAY_ACTION_COMPONENT_PROPERTY_MILLISECONDS);

		Value value;
		if (!evalExpression(assets, flowState, propertyValue->evalInstructions, value)) {
			throwError();
			return;
		}

		double milliseconds = value.toDouble();
		if (!isNaN(milliseconds)) {
			osDelay((uint32_t)floor(milliseconds));
		} else {
			throwError();
			return;
		}
    } else if (component->type == defs_v3::COMPONENT_TYPE_END_ACTION) {
#if FLOW_DEBUG
		printf("Execute END component at index = %d\n", componentIndex);
#endif
		scripting::stopScript();
    } else if (component->type == defs_v3::COMPONENT_TYPE_CONSTANT_ACTION) {
#if FLOW_DEBUG
		printf("Execute CONSTANT component at index = %d\n", componentIndex);
#endif
        struct ConstantActionComponent : public Component {
            uint16_t valueIndex;
        };
        auto constantActionComponent = (ConstantActionComponent *)component;
		auto &sourceValue = *flowDefinition->constants.item(assets, constantActionComponent->valueIndex);

		// TODO ovdje treba poslati null value za 0-ti output
		for (unsigned outputIndex = 0; outputIndex < component->outputs.count; outputIndex++) {
			auto &componentOutput = *component->outputs.item(assets, outputIndex);
			propagateValue(assets, flowState, componentOutput, sourceValue);
		}
	} else if (component->type == defs_v3::COMPONENT_TYPE_SCPI_ACTION) {
#if FLOW_DEBUG
		printf("Execute SCPI component at index = %d\n", componentIndex);
#endif
		struct ScpiActionComponent : public Component {
			uint8_t instructions[1];
		};
		auto specific = (ScpiActionComponent *)component;
		auto instructions = specific->instructions;

		static const int SCPI_PART_STRING = 1;
		static const int SCPI_PART_EXPR = 2;
		static const int SCPI_PART_QUERY_WITH_ASSIGNMENT = 3;
		static const int SCPI_PART_QUERY = 4;
		static const int SCPI_PART_COMMAND = 5;
		static const int SCPI_PART_END = 6;

		char commandOrQueryText[256] = {0};
		const char *resultText;
		size_t resultTextLen;

		int i = 0;
		while (true) {
			uint8_t op = instructions[i++];

			if (op == SCPI_PART_STRING) {
#if FLOW_DEBUG
				printf("SCPI_PART_STRING\n");
#endif
				uint16_t sizeLowByte = instructions[i++];
				uint16_t sizeHighByte = instructions[i++];
				uint16_t stringLength = sizeLowByte | (sizeHighByte << 8);
				stringAppendStringLength(commandOrQueryText, sizeof(commandOrQueryText), (const char *)instructions + i, (size_t)stringLength);
				i += stringLength;
			} else if (op == SCPI_PART_EXPR) {
#if FLOW_DEBUG
				printf("SCPI_PART_EXPR\n");
#endif
				//uint8_t inputIndex = specific[i++];
				// TODO
			} else if (op == SCPI_PART_QUERY_WITH_ASSIGNMENT) {
#if FLOW_DEBUG
				printf("SCPI_PART_QUERY_WITH_ASSIGNMENT\n");
#endif
				stringAppendString(commandOrQueryText, sizeof(commandOrQueryText), "\n");
				if (!scripting::scpi(commandOrQueryText, &resultText, &resultTextLen)) {
					throwError();
					return;
				}
				commandOrQueryText[0] = 0;

				Value *pDstValue;
				int numInstructionBytes;
				if (!evalAssignableExpression(assets, flowState, instructions + i, &pDstValue, &numInstructionBytes)) {
					throwError();
					return;
				}
				i += numInstructionBytes;

				Value srcValue(resultText, resultTextLen);

				assignValue(*pDstValue, srcValue);
			} else if (op == SCPI_PART_QUERY) {
#if FLOW_DEBUG
				printf("SCPI_PART_QUERY\n");
#endif
				stringAppendString(commandOrQueryText, sizeof(commandOrQueryText), "\n");
				scripting::scpi(commandOrQueryText, &resultText, &resultTextLen);
				commandOrQueryText[0] = 0;
			} else if (op == SCPI_PART_COMMAND) {
#if FLOW_DEBUG
				printf("SCPI_PART_COMMAND\n");
#endif
				stringAppendString(commandOrQueryText, sizeof(commandOrQueryText), "\n");
				scripting::scpi(commandOrQueryText, &resultText, &resultTextLen);
				commandOrQueryText[0] = 0;
			} else if (op == SCPI_PART_END) {
#if FLOW_DEBUG
				printf("SCPI_PART_END\n");
#endif
				break;
			}
		}

	} else if (component->type == defs_v3::COMPONENT_TYPE_SET_VARIABLE_ACTION) {
#if FLOW_DEBUG
		printf("Execute SetVariable component at index = %d\n", componentIndex);
#endif

        struct SetVariableActionComponent : public Component {
            uint8_t assignableExpressionEvalInstructions[1];
        };
		auto setVariableActionComponent = (SetVariableActionComponent *)component;

		Value *pDstValue;
		if (!evalAssignableExpression(assets, flowState, setVariableActionComponent->assignableExpressionEvalInstructions, &pDstValue)) {
			throwError();
			return;
		}

		auto propertyValue = component->propertyValues.item(assets, defs_v3::SET_VARIABLE_ACTION_COMPONENT_PROPERTY_VALUE);
		Value srcValue;
		if (!evalExpression(assets, flowState, propertyValue->evalInstructions, srcValue)) {
			throwError();
			return;
		}

		assignValue(*pDstValue, srcValue);
    } else {
#if FLOW_DEBUG
		printf("Unknown component at index = %d, type = %d\n", componentIndex, component->type);
#endif
    }

	auto &nullValue = *flowDefinition->constants.item(assets, NULL_VALUE_INDEX);
	propagateValue(assets, flowState, *component->outputs.item(assets, 0), nullValue);
}

static FlowState *initFlowState(Assets *assets, int flowIndex) {
	auto flowDefinition = assets->flowDefinition.ptr(assets);
	auto flow = flowDefinition->flows.item(assets, flowIndex);

	FlowState *flowState = (FlowState *)alloc(sizeof(FlowState) + (flow->nInputValues + flow->localVariables.count - 1) * sizeof(Value));

	flowState->flowIndex = flowIndex;

	auto &undefinedValue = *flowDefinition->constants.item(assets, UNDEFINED_VALUE_INDEX);

	for (unsigned i = 0; i < flow->nInputValues; i++) {
		flowState->values[i] = undefinedValue;
	}

	for (unsigned i = 0; i < flow->localVariables.count; i++) {
		auto &value = *flow->localVariables.item(assets, i);
		flowState->values[flow->nInputValues + i] = value;
	}

	return flowState;
}

unsigned start(Assets *assets) {
	auto flowDefinition = assets->flowDefinition.ptr(assets);
	if (flowDefinition->flows.count == 0) {
		return 0;
	}

	g_assets = assets;

	g_queueHead = 0;
	g_queueTail = 0;
	g_queueIsFull = false;

	g_mainPageFlowState = initFlowState(assets, 0);

	auto flowState = g_mainPageFlowState;
	auto flow = flowDefinition->flows.item(assets, flowState->flowIndex);

	for (unsigned componentIndex = 0; componentIndex < flow->components.count; componentIndex++) {
		pingComponent(assets, flowState, componentIndex);
	}

	return 1;
}

void stop() {
	// TODO
}

void tick(unsigned flowHandle) {
	if (flowHandle != 1) {
		return;
	}

	Assets *assets = g_assets;

	FlowState *flowState;
	unsigned componentIndex;
	if (removeFromQueue(flowState, componentIndex)) {
		executeComponent(assets, flowState, componentIndex);
	}
}

FlowState *getFlowState(int16_t pageId) {
	pageId = -pageId - 1;
	return g_mainPageFlowState;
}

void executeFlowAction(unsigned flowHandle, const gui::WidgetCursor &widgetCursor, int16_t actionId) {
	auto flowState = (FlowState *)widgetCursor.pageState;
	actionId = -actionId - 1;

	Assets *assets = g_assets;
	auto flowDefinition = assets->flowDefinition.ptr(assets);

	auto flow = flowDefinition->flows.item(assets, flowState->flowIndex);

	if (actionId >= 0 && actionId < (int16_t)flow->widgetActions.count) {
		auto componentOutput = flow->widgetActions.item(assets, actionId);
		if (componentOutput) {
			auto &nullValue = *flowDefinition->constants.item(assets, NULL_VALUE_INDEX);
			propagateValue(assets, flowState, *componentOutput, nullValue);
		}
	}
}

void dataOperation(unsigned flowHandle, int16_t dataId, DataOperationEnum operation, const gui::WidgetCursor &widgetCursor, Value &value) {
	auto flowState = (FlowState *)widgetCursor.pageState;
	dataId = -dataId - 1;

	Assets *assets = g_assets;
	auto flowDefinition = assets->flowDefinition.ptr(assets);

	auto flow = flowDefinition->flows.item(assets, flowState->flowIndex);

	if (dataId >= 0 && dataId < (int16_t)flow->widgetDataItems.count) {
		auto propertyValue = flow->widgetDataItems.item(assets, dataId);
		if (propertyValue) {
			evalExpression(assets, flowState, propertyValue->evalInstructions, value);
		} else {
			value = Value();
		}
	}
}

} // namespace flow
} // namespace eez
