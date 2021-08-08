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

struct OutputActionComponent : public Component {
	uint8_t outputIndex;
};

void executeOutputComponent(FlowState *flowState, unsigned componentIndex) {
 	auto assets = flowState->assets;
	auto flowDefinition = assets->flowDefinition.ptr(assets);
	auto flow = flowDefinition->flows.item(assets, flowState->flowIndex);
	auto component = flow->components.item(assets, componentIndex);

	if (!flowState->parentFlowState) {
		throwError(flowState, component, "Output action component, no parentFlowState\n");
		return;
	}

	if (!flowState->parentComponent) {
		throwError(flowState, component, "Output action component, no parentComponent\n");
		return;
	}

	auto outputActionComponent = (OutputActionComponent *)component;

    auto inputIndex = outputActionComponent->inputs.ptr(assets)[0];
    if (inputIndex >= flow->nInputValues) {
        throwError(flowState, component, "Output action component, invalid input index\n");
    }

    auto value = flowState->values[inputIndex];

    auto callActionComponent = (CallActionActionComponent *)flowState->parentComponent;

    uint8_t parentComponentOutputIndex = callActionComponent->outputsStartIndex + outputActionComponent->outputIndex;

    if (parentComponentOutputIndex >= flowState->parentComponent->outputs.count) {
        throwError(flowState, component, "Output action component, invalid output index\n");
    }

    auto &componentOutput = *flowState->parentComponent->outputs.item(flowState->assets, parentComponentOutputIndex);

    propagateValue(flowState->parentFlowState, componentOutput, value);
}

} // namespace flow
} // namespace eez
