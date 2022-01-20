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

#include <eez/flow/private.h>
#include <eez/flow/operations.h>

#include <eez/gui/gui.h>

using namespace eez::gui;

namespace eez {
namespace flow {

EvalStack g_stack;

static bool evalExpression(FlowState *flowState, const uint8_t *instructions, int *numInstructionBytes) {
	auto flowDefinition = flowState->flowDefinition;
	auto flow = flowState->flow;

	int i = 0;
	while (true) {
		uint16_t instruction = instructions[i] + (instructions[i + 1] << 8);
		auto instructionType = instruction & EXPR_EVAL_INSTRUCTION_TYPE_MASK;
		auto instructionArg = instruction & EXPR_EVAL_INSTRUCTION_PARAM_MASK;
		if (instructionType == EXPR_EVAL_INSTRUCTION_TYPE_PUSH_CONSTANT) {
			if (!g_stack.push(*flowDefinition->constants.item(instructionArg))) {
				return false;
			}
		} else if (instructionType == EXPR_EVAL_INSTRUCTION_TYPE_PUSH_INPUT) {
			if (!g_stack.push(flowState->values[instructionArg])) {
				return false;
			}
		} else if (instructionType == EXPR_EVAL_INSTRUCTION_TYPE_PUSH_LOCAL_VAR) {
			if (!g_stack.push(&flowState->values[flow->componentInputs.count + instructionArg])) {
				return false;
			}
		} else if (instructionType == EXPR_EVAL_INSTRUCTION_TYPE_PUSH_GLOBAL_VAR) {
			if ((uint32_t)instructionArg < flowDefinition->globalVariables.count) {
				if (!g_stack.push(flowDefinition->globalVariables.item(instructionArg))) {
					return false;
				}
			} else {
				// native variable
				if (!g_stack.push(Value((int)(instructionArg - flowDefinition->globalVariables.count + 1), VALUE_TYPE_NATIVE_VARIABLE))) {
					return false;
				}
			}
		} else if (instructionType == EXPR_EVAL_INSTRUCTION_TYPE_PUSH_OUTPUT) {
			if (!g_stack.push(Value((uint16_t)instructionArg, VALUE_TYPE_FLOW_OUTPUT))) {
				return false;
			}
		} else if (instructionType == EXPR_EVAL_INSTRUCTION_ARRAY_ELEMENT) {
			auto elementIndexValue = g_stack.pop();
			auto arrayValue = g_stack.pop();

			if (arrayValue.getType() == VALUE_TYPE_VALUE_PTR) {
				arrayValue = *arrayValue.pValueValue;
			} else if (arrayValue.getType() == VALUE_TYPE_NATIVE_VARIABLE) {
				arrayValue = get(g_widgetCursor, arrayValue.getInt());
			}

			if (elementIndexValue.getType() == VALUE_TYPE_VALUE_PTR) {
				elementIndexValue = *elementIndexValue.pValueValue;
			} else if (arrayValue.getType() == VALUE_TYPE_NATIVE_VARIABLE) {
				elementIndexValue = get(g_widgetCursor, elementIndexValue.getInt());
			}

			if (arrayValue.type != VALUE_TYPE_ARRAY) {
				throwError(flowState, g_stack.componentIndex, "Array value expected\n");
				return false;
			}

			auto array = arrayValue.arrayValue;

			int err;
			auto elementIndex = elementIndexValue.toInt32(&err);
			if (err) {
				throwError(flowState, g_stack.componentIndex, "Integer value expected for array element index\n");
				return false;
			}
		
			if (elementIndex < 0 || elementIndex >= (int)array->arraySize) {
				throwError(flowState, g_stack.componentIndex, "Array element index out of bounds\n");
				return false;
			}

			if (!g_stack.push(&array->values[elementIndex])) {
				return false;
			}
		} else if (instructionType == EXPR_EVAL_INSTRUCTION_TYPE_OPERATION) {
			if (!g_evalOperations[instructionArg](g_stack)) {
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

bool evalExpression(FlowState *flowState, int componentIndex, const uint8_t *instructions, Value &result, int *numInstructionBytes, const int32_t *iterators, DataOperationEnum operation) {
	g_stack.sp = 0;
	g_stack.flowState = flowState;
	g_stack.componentIndex = componentIndex;
	g_stack.iterators = iterators;

	if (evalExpression(flowState, instructions, numInstructionBytes)) {
		if (g_stack.sp == 1) {
			auto finalResult = g_stack.pop();

			if (finalResult.getType() == VALUE_TYPE_VALUE_PTR) {
				result = *finalResult.pValueValue;
			} else if (finalResult.getType() == VALUE_TYPE_NATIVE_VARIABLE) {
				DATA_OPERATION_FUNCTION(finalResult.getInt(), operation, g_widgetCursor, result);
			} else {
				result = finalResult;
			}

			return true;
		}
	}

	result = Value();
	return false;
}

bool evalAssignableExpression(FlowState *flowState, int componentIndex, const uint8_t *instructions, Value &result, int *numInstructionBytes, const int32_t *iterators) {
	g_stack.sp = 0;
	g_stack.flowState = flowState;
	g_stack.componentIndex = componentIndex;
	g_stack.iterators = iterators;

	if (evalExpression(flowState, instructions, numInstructionBytes)) {
		if (g_stack.sp == 1) {
			auto finalResult = g_stack.pop();
			if (finalResult.getType() == VALUE_TYPE_VALUE_PTR || finalResult.getType() == VALUE_TYPE_NATIVE_VARIABLE || finalResult.getType() == VALUE_TYPE_FLOW_OUTPUT) {
				result = finalResult;
				return true;
			}
		}
	}

	return false;
}

int16_t getNativeVariableId(const WidgetCursor &widgetCursor) {
	if (widgetCursor.flowState) {
		FlowState *flowState = widgetCursor.flowState;
		auto flow = flowState->flow;

		WidgetDataItem *widgetDataItem = flow->widgetDataItems.item(-(widgetCursor.widget->data + 1));
		if (widgetDataItem && widgetDataItem->componentIndex != -1 && widgetDataItem->propertyValueIndex != -1) {
			auto component = flow->components.item(widgetDataItem->componentIndex);
			auto propertyValue = component->propertyValues.item(widgetDataItem->propertyValueIndex);

			g_stack.sp = 0;
			g_stack.flowState = flowState;
			g_stack.componentIndex = widgetDataItem->componentIndex;
			g_stack.iterators = widgetCursor.iterators;

			if (evalExpression(flowState, propertyValue->evalInstructions, nullptr)) {
				if (g_stack.sp == 1) {
					auto finalResult = g_stack.pop();

					if (finalResult.getType() == VALUE_TYPE_NATIVE_VARIABLE) {
						return finalResult.getInt();
					}
				}
			}
		}
	}

	return DATA_ID_NONE;
}

} // flow
} // eez
