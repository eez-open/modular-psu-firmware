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

#include <eez/alloc.h>
#include <eez/system.h>
#include <eez/scripting/scripting.h>

#include <eez/flow/components.h>
#include <eez/flow/flow_defs_v3.h>
#include <eez/flow/queue.h>

using namespace eez::gui;

namespace eez {
namespace flow {

void executeStartComponent(Assets *assets, FlowState *flowState, Component *component, ComponenentExecutionState *&componentExecutionState);
void executeEndComponent(Assets *assets, FlowState *flowState, Component *component, ComponenentExecutionState *&componentExecutionState);
void executeDelayComponent(Assets *assets, FlowState *flowState, Component *component, ComponenentExecutionState *&componentExecutionState);
void executeConstantComponent(Assets *assets, FlowState *flowState, Component *component, ComponenentExecutionState *&componentExecutionState);
void executeSetVariableComponent(Assets *assets, FlowState *flowState, Component *component, ComponenentExecutionState *&componentExecutionState);
void executeSwitchComponent(Assets *assets, FlowState *flowState, Component *component, ComponenentExecutionState *&componentExecutionState);
void executeLogComponent(Assets *assets, FlowState *flowState, Component *component, ComponenentExecutionState *&componentExecutionState);
void executeScpiComponent(Assets *assets, FlowState *flowState, Component *component, ComponenentExecutionState *&componentExecutionState);

void executeComponent(Assets *assets, FlowState *flowState, unsigned componentIndex, ComponenentExecutionState *componentExecutionState) {
	auto flowDefinition = assets->flowDefinition.ptr(assets);
	auto flow = flowDefinition->flows.item(assets, flowState->flowIndex);
	auto component = flow->components.item(assets, componentIndex);

    if (component->type == defs_v3::COMPONENT_TYPE_START_ACTION) {
		executeStartComponent(assets, flowState, component, componentExecutionState);
    } else if (component->type == defs_v3::COMPONENT_TYPE_END_ACTION) {
		executeEndComponent(assets, flowState, component, componentExecutionState);
	} else if (component->type == defs_v3::COMPONENT_TYPE_DELAY_ACTION) {
		executeDelayComponent(assets, flowState, component, componentExecutionState);
    } else if (component->type == defs_v3::COMPONENT_TYPE_CONSTANT_ACTION) {
		executeConstantComponent(assets, flowState, component, componentExecutionState);
	} else if (component->type == defs_v3::COMPONENT_TYPE_SET_VARIABLE_ACTION) {
		executeSetVariableComponent(assets, flowState, component, componentExecutionState);
    } else if (component->type == defs_v3::COMPONENT_TYPE_SWITCH_ACTION) {
		executeSwitchComponent(assets, flowState, component, componentExecutionState);
	} else if (component->type == defs_v3::COMPONENT_TYPE_LOG_ACTION) {
		executeLogComponent(assets, flowState, component, componentExecutionState);
	} else if (component->type == defs_v3::COMPONENT_TYPE_SCPI_ACTION) {
		executeScpiComponent(assets, flowState, component, componentExecutionState);
	} else {
		DebugTrace("Unknown component at index = %d, type = %d\n", componentIndex, component->type);
    }

	if (componentExecutionState) {
		addToQueue(flowState, componentIndex, componentExecutionState);
	} else {
		auto &nullValue = *flowDefinition->constants.item(assets, NULL_VALUE_INDEX);
		propagateValue(assets, flowState, *component->outputs.item(assets, 0), nullValue);
	}
}

} // namespace flow
} // namespace eez
