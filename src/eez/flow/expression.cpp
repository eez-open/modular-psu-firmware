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

#include <eez/flow/private.h>
#include <eez/flow/operations.h>

#include <eez/gui/gui.h>

using namespace eez::gui;

namespace eez {
namespace flow {

EvalStack g_stack;

static bool evalExpression(FlowState *flowState, const uint8_t *instructions, int *numInstructionBytes, const char *errorMessage) {
	auto flowDefinition = flowState->flowDefinition;
	auto flow = flowState->flow;

	int i = 0;
	while (true) {
		uint16_t instruction = instructions[i] + (instructions[i + 1] << 8);
		auto instructionType = instruction & EXPR_EVAL_INSTRUCTION_TYPE_MASK;
		auto instructionArg = instruction & EXPR_EVAL_INSTRUCTION_PARAM_MASK;
		if (instructionType == EXPR_EVAL_INSTRUCTION_TYPE_PUSH_CONSTANT) {
			if (!g_stack.push(*flowDefinition->constants[instructionArg])) {
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
				if (!g_stack.push(flowDefinition->globalVariables[instructionArg])) {
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
			auto elementIndexValue = g_stack.pop().getValue();
			auto arrayValue = g_stack.pop().getValue();

            if (arrayValue.getType() == VALUE_TYPE_UNDEFINED || arrayValue.getType() == VALUE_TYPE_NULL) {
                if (!g_stack.push(Value(0, VALUE_TYPE_UNDEFINED))) {
				    return false;
			    }
            } else {
                if (arrayValue.type != VALUE_TYPE_ARRAY && arrayValue.type != VALUE_TYPE_ARRAY_REF) {
                    throwError(flowState, g_stack.componentIndex, errorMessage, "Array value expected\n");
                    return false;
                }

                auto array = arrayValue.getArray();

                int err;
                auto elementIndex = elementIndexValue.toInt32(&err);
                if (err) {
                    throwError(flowState, g_stack.componentIndex, errorMessage, "Integer value expected for array element index\n");
                    return false;
                }

                if (elementIndex < 0 || elementIndex >= (int)array->arraySize) {
                    throwError(flowState, g_stack.componentIndex, errorMessage, "Array element index out of bounds\n");
                    return false;
                }

                if (!g_stack.push(Value::makeArrayElementRef(arrayValue, elementIndex, 0x132e0e2f))) {
                    return false;
                }
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

bool evalExpression(FlowState *flowState, int componentIndex, const uint8_t *instructions, Value &result, const char *errorMessage, int *numInstructionBytes, const int32_t *iterators, DataOperationEnum operation) {
	g_stack.sp = 0;
	g_stack.flowState = flowState;
	g_stack.componentIndex = componentIndex;
	g_stack.iterators = iterators;

	if (evalExpression(flowState, instructions, numInstructionBytes, errorMessage)) {
		if (g_stack.sp == 1) {
			result = g_stack.pop().getValue();
			return true;
		}
	}

	result = Value();
	return false;
}

bool evalAssignableExpression(FlowState *flowState, int componentIndex, const uint8_t *instructions, Value &result, const char *errorMessage, int *numInstructionBytes, const int32_t *iterators) {
	g_stack.sp = 0;
	g_stack.flowState = flowState;
	g_stack.componentIndex = componentIndex;
	g_stack.iterators = iterators;

	if (evalExpression(flowState, instructions, numInstructionBytes, errorMessage)) {
		if (g_stack.sp == 1) {
			auto finalResult = g_stack.pop();
			if (finalResult.getType() == VALUE_TYPE_VALUE_PTR || finalResult.getType() == VALUE_TYPE_NATIVE_VARIABLE || finalResult.getType() == VALUE_TYPE_FLOW_OUTPUT || finalResult.getType() == VALUE_TYPE_ARRAY_ELEMENT_VALUE) {
				result = finalResult;
				return true;
			}
		}
	}

	return false;
}

bool evalProperty(FlowState *flowState, int componentIndex, int propertyIndex, Value &result, const char *errorMessage, int *numInstructionBytes, const int32_t *iterators, DataOperationEnum operation) {
    if (componentIndex < 0 || componentIndex >= (int)flowState->flow->components.count) {
        char message[256];
        snprintf(message, sizeof(message), "invalid component index %d in flow at index %d", componentIndex, flowState->flowIndex);
        throwError(flowState, componentIndex, errorMessage, message);
        return false;
    }
    auto component = flowState->flow->components[componentIndex];
    if (propertyIndex < 0 || propertyIndex >= (int)component->properties.count) {
        char message[256];
        snprintf(message, sizeof(message), "invalid property index %d at component index %d in flow at index %d", propertyIndex, componentIndex, flowState->flowIndex);
        throwError(flowState, componentIndex, errorMessage, message);
        return false;
    }
    return evalExpression(flowState, componentIndex, component->properties[propertyIndex]->evalInstructions, result, errorMessage, numInstructionBytes, iterators, operation);
}

bool evalAssignableProperty(FlowState *flowState, int componentIndex, int propertyIndex, Value &result, const char *errorMessage, int *numInstructionBytes, const int32_t *iterators) {
    if (componentIndex < 0 || componentIndex >= (int)flowState->flow->components.count) {
        char message[256];
        snprintf(message, sizeof(message), "invalid component index %d in flow at index %d", componentIndex, flowState->flowIndex);
        throwError(flowState, componentIndex, errorMessage, message);
        return false;
    }
    auto component = flowState->flow->components[componentIndex];
    if (propertyIndex < 0 || propertyIndex >= (int)component->properties.count) {
        char message[256];
        snprintf(message, sizeof(message), "invalid property index %d (max: %d) in component at index %d in flow at index %d", propertyIndex, (int)component->properties.count, componentIndex, flowState->flowIndex);
        throwError(flowState, componentIndex, errorMessage, message);
        return false;
    }
    return evalAssignableExpression(flowState, componentIndex, component->properties[propertyIndex]->evalInstructions, result, errorMessage, numInstructionBytes, iterators);
}

int16_t getNativeVariableId(const WidgetCursor &widgetCursor) {
	if (widgetCursor.flowState) {
		FlowState *flowState = widgetCursor.flowState;
		auto flow = flowState->flow;

		WidgetDataItem *widgetDataItem = flow->widgetDataItems[-(widgetCursor.widget->data + 1)];
		if (widgetDataItem && widgetDataItem->componentIndex != -1 && widgetDataItem->propertyValueIndex != -1) {
			auto component = flow->components[widgetDataItem->componentIndex];
			auto property = component->properties[widgetDataItem->propertyValueIndex];

			g_stack.sp = 0;
			g_stack.flowState = flowState;
			g_stack.componentIndex = widgetDataItem->componentIndex;
			g_stack.iterators = widgetCursor.iterators;

			if (evalExpression(flowState, property->evalInstructions, nullptr, nullptr)) {
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
