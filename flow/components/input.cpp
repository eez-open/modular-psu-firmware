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

#include <eez/flow/components.h>
#include <eez/flow/components/call_action.h>

using namespace eez::gui;

namespace eez {
namespace flow {

struct InputActionComponent : public Component {
	uint8_t inputIndex;
};

void executeInputComponent(FlowState *flowState, unsigned componentIndex) {
 	auto assets = flowState->assets;
	auto component = (InputActionComponent *)flowState->flow->components.item(assets, componentIndex);

	if (!flowState->parentFlowState) {
		throwError(flowState, componentIndex, "No parentFlowState in Input\n");
		return;
	}

	if (!flowState->parentComponent) {
		throwError(flowState, componentIndex, "No parentComponent in Input\n");
		return;
	}

    auto callActionComponent = (CallActionActionComponent *)flowState->parentComponent;

    uint8_t parentComponentInputIndex = callActionComponent->inputsStartIndex + component->inputIndex;
    
    if (parentComponentInputIndex >= flowState->parentComponent->inputs.count) {
        throwError(flowState, componentIndex, "Invalid input index in Input\n");
        return;
    }

    auto parentComponentInputs = flowState->parentComponent->inputs.ptr(assets);
    auto flowInputIndex = parentComponentInputs[parentComponentInputIndex];

    auto parentFlow = flowState->flowDefinition->flows.item(assets, flowState->parentFlowState->flowIndex);
    if (flowInputIndex >= parentFlow->componentInputs.count) {
        throwError(flowState, componentIndex, "Invalid input index of parent component in Input\n");
        return;
    }

    auto value = flowState->parentFlowState->values[flowInputIndex];

    propagateValue(flowState, componentIndex, 0, value);
}

} // namespace flow
} // namespace eez
