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

#include <eez/flow/components/input.h>
#include <eez/flow/components/call_action.h>
#include <eez/flow/flow_defs_v3.h>

using namespace eez::gui;

namespace eez {
namespace flow {

bool getCallActionValue(FlowState *flowState, unsigned componentIndex, Value &value) {
	auto component = (InputActionComponent *)flowState->flow->components[componentIndex];

	if (!flowState->parentFlowState) {
		throwError(flowState, componentIndex, "No parentFlowState in Input\n");
		return false;
	}

	if (!flowState->parentComponent) {
		throwError(flowState, componentIndex, "No parentComponent in Input\n");
		return false;
	}

    auto callActionComponent = (CallActionActionComponent *)flowState->parentComponent;

    uint8_t parentComponentInputIndex = callActionComponent->inputsStartIndex + component->inputIndex;
    if (component->type == defs_v3::COMPONENT_TYPE_INPUT_ACTION) {
        parentComponentInputIndex = callActionComponent->inputsStartIndex + component->inputIndex;
    } else {
        parentComponentInputIndex = 0;
    }

    if (parentComponentInputIndex >= flowState->parentComponent->inputs.count) {
        throwError(flowState, componentIndex, "Invalid input index in Input\n");
        return false;
    }

    auto parentComponentInputs = flowState->parentComponent->inputs;
    auto flowInputIndex = parentComponentInputs[parentComponentInputIndex];

    auto parentFlow = flowState->flowDefinition->flows[flowState->parentFlowState->flowIndex];
    if (flowInputIndex >= parentFlow->componentInputs.count) {
        throwError(flowState, componentIndex, "Invalid input index of parent component in Input\n");
        return false;
    }

    value = flowState->parentFlowState->values[flowInputIndex];
    return true;
}

void executeInputComponent(FlowState *flowState, unsigned componentIndex) {
	Value value;
    if (getCallActionValue(flowState, componentIndex, value)) {
        auto inputActionComponentExecutionState = (InputActionComponentExecutionState *)flowState->componenentExecutionStates[componentIndex];
        if (!inputActionComponentExecutionState) {
            inputActionComponentExecutionState = allocateComponentExecutionState<InputActionComponentExecutionState>(flowState, componentIndex);
        }

        propagateValue(flowState, componentIndex, 0, value);
        inputActionComponentExecutionState->value = value;
    }
}

} // namespace flow
} // namespace eez
