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

#include <eez/system.h>
#include <eez/scripting/scripting.h>

#include <eez/flow/components.h>
#include <eez/flow/flow_defs_v3.h>
#include <eez/flow/queue.h>

using namespace eez::gui;

namespace eez {
namespace flow {

bool executeStartComponent(FlowState *flowState, Component *component, ComponenentExecutionState *&componentExecutionState);
bool executeEndComponent(FlowState *flowState, Component *component, ComponenentExecutionState *&componentExecutionState);
bool executeDelayComponent(FlowState *flowState, Component *component, ComponenentExecutionState *&componentExecutionState);
bool executeConstantComponent(FlowState *flowState, Component *component, ComponenentExecutionState *&componentExecutionState);
bool executeSetVariableComponent(FlowState *flowState, Component *component, ComponenentExecutionState *&componentExecutionState);
bool executeSwitchComponent(FlowState *flowState, Component *component, ComponenentExecutionState *&componentExecutionState);
bool executeLogComponent(FlowState *flowState, Component *component, ComponenentExecutionState *&componentExecutionState);
bool executeScpiComponent(FlowState *flowState, Component *component, ComponenentExecutionState *&componentExecutionState);
bool executeCallActionComponent(FlowState *flowState, Component *component, ComponenentExecutionState *&componentExecutionState);

bool executeComponent(FlowState *flowState, unsigned componentIndex, ComponenentExecutionState *componentExecutionState) {
	auto assets = flowState->assets;
	auto flowDefinition = assets->flowDefinition.ptr(assets);
	auto flow = flowDefinition->flows.item(assets, flowState->flowIndex);
	auto component = flow->components.item(assets, componentIndex);

	bool result;
    
	if (component->type == defs_v3::COMPONENT_TYPE_START_ACTION) {
		result = executeStartComponent(flowState, component, componentExecutionState);
    } else if (component->type == defs_v3::COMPONENT_TYPE_END_ACTION) {
		result = executeEndComponent(flowState, component, componentExecutionState);
	} else if (component->type == defs_v3::COMPONENT_TYPE_DELAY_ACTION) {
		result = executeDelayComponent(flowState, component, componentExecutionState);
    } else if (component->type == defs_v3::COMPONENT_TYPE_CONSTANT_ACTION) {
		result = executeConstantComponent(flowState, component, componentExecutionState);
	} else if (component->type == defs_v3::COMPONENT_TYPE_SET_VARIABLE_ACTION) {
		result = executeSetVariableComponent(flowState, component, componentExecutionState);
    } else if (component->type == defs_v3::COMPONENT_TYPE_SWITCH_ACTION) {
		result = executeSwitchComponent(flowState, component, componentExecutionState);
	} else if (component->type == defs_v3::COMPONENT_TYPE_LOG_ACTION) {
		result = executeLogComponent(flowState, component, componentExecutionState);
	} else if (component->type == defs_v3::COMPONENT_TYPE_SCPI_ACTION) {
		result = executeScpiComponent(flowState, component, componentExecutionState);
	} else if (component->type == defs_v3::COMPONENT_TYPE_CALL_ACTION_ACTION) {
		result = executeCallActionComponent(flowState, component, componentExecutionState);
	} else {
		char errorMessage[100];
		snprintf(errorMessage, sizeof(errorMessage), "Unknown component at index = %d, type = %d\n", componentIndex, component->type);
		throwError(flowState, component, errorMessage);
		return false;
    }

	if (!result) {
		return false;
	}

	if (componentExecutionState) {
		addToQueue(flowState, componentIndex, componentExecutionState);
	} else {
		auto &nullValue = *flowDefinition->constants.item(assets, NULL_VALUE_INDEX);
		propagateValue(flowState, *component->outputs.item(assets, 0), nullValue);
	}
	
	return true;
}

} // namespace flow
} // namespace eez
