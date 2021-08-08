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

#include <eez/flow/components.h>
#include <eez/flow/components/call_action.h>

using namespace eez::gui;

namespace eez {
namespace flow {

void executeCallActionComponent(FlowState *flowState, unsigned componentIndex) {
 	auto assets = flowState->assets;
	auto flowDefinition = assets->flowDefinition.ptr(assets);
	auto flow = flowDefinition->flows.item(assets, flowState->flowIndex);
	auto component = flow->components.item(assets, componentIndex);

	auto callActionActionComponent = (CallActionActionComponent *)component;

	auto flowIndex = callActionActionComponent->flowIndex;

	if (flowIndex < 0 || flowIndex >= (int)flowDefinition->flows.count) {
		throwError(flowState, component, "Invalid action flow index in CallAction component\n");
	}

	FlowState *actionFlowState = initFlowState(assets, flowIndex);

	actionFlowState->parentFlowState = flowState;
	actionFlowState->parentComponent = component;

	if (actionFlowState->numActiveComponents == 0) {
		freeFlowState(actionFlowState);
		propagateValue(flowState, componentIndex);
	} else {
		flowState->numActiveComponents++;
	}
}

} // namespace flow
} // namespace eez
