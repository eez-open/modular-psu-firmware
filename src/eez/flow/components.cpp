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

void executeStartComponent(FlowState *flowState, unsigned componentIndex);
void executeEndComponent(FlowState *flowState, unsigned componentIndex);
void executeInputComponent(FlowState *flowState, unsigned componentIndex);
void executeOutputComponent(FlowState *flowState, unsigned componentIndex);
void executeDelayComponent(FlowState *flowState, unsigned componentIndex);
void executeConstantComponent(FlowState *flowState, unsigned componentIndex);
void executeSetVariableComponent(FlowState *flowState, unsigned componentIndex);
void executeEvalComponent(FlowState *flowState, unsigned componentIndex);
void executeLoopComponent(FlowState *flowState, unsigned componentIndex);
void executeSwitchComponent(FlowState *flowState, unsigned componentIndex);
void executeLogComponent(FlowState *flowState, unsigned componentIndex);
void executeScpiComponent(FlowState *flowState, unsigned componentIndex);
void executeCallActionComponent(FlowState *flowState, unsigned componentIndex);

void executeComponent(FlowState *flowState, unsigned componentIndex) {
	auto assets = flowState->assets;
	auto flowDefinition = assets->flowDefinition.ptr(assets);
	auto flow = flowDefinition->flows.item(assets, flowState->flowIndex);
	auto component = flow->components.item(assets, componentIndex);

	if (component->type == defs_v3::COMPONENT_TYPE_START_ACTION) {
		executeStartComponent(flowState, componentIndex);
    } else if (component->type == defs_v3::COMPONENT_TYPE_END_ACTION) {
		executeEndComponent(flowState, componentIndex);
	} else if (component->type == defs_v3::COMPONENT_TYPE_INPUT_ACTION) {
		executeInputComponent(flowState, componentIndex);
	} else if (component->type == defs_v3::COMPONENT_TYPE_OUTPUT_ACTION) {
		executeOutputComponent(flowState, componentIndex);
    } else if (component->type == defs_v3::COMPONENT_TYPE_DELAY_ACTION) {
		executeDelayComponent(flowState, componentIndex);
    } else if (component->type == defs_v3::COMPONENT_TYPE_CONSTANT_ACTION) {
		executeConstantComponent(flowState, componentIndex);
	} else if (component->type == defs_v3::COMPONENT_TYPE_SET_VARIABLE_ACTION) {
		executeSetVariableComponent(flowState, componentIndex);
    } else if (component->type == defs_v3::COMPONENT_TYPE_EVAL_ACTION) {
		executeEvalComponent(flowState, componentIndex);
    } else if (component->type == defs_v3::COMPONENT_TYPE_LOOP_ACTION) {
		executeLoopComponent(flowState, componentIndex);
    } else if (component->type == defs_v3::COMPONENT_TYPE_SWITCH_ACTION) {
		executeSwitchComponent(flowState, componentIndex);
	} else if (component->type == defs_v3::COMPONENT_TYPE_LOG_ACTION) {
		executeLogComponent(flowState, componentIndex);
	} else if (component->type == defs_v3::COMPONENT_TYPE_SCPI_ACTION) {
		executeScpiComponent(flowState, componentIndex);
	} else if (component->type == defs_v3::COMPONENT_TYPE_CALL_ACTION_ACTION) {
		executeCallActionComponent(flowState, componentIndex);
	} else {
		char errorMessage[100];
		snprintf(errorMessage, sizeof(errorMessage), "Unknown component at index = %d, type = %d\n", componentIndex, component->type);
		throwError(flowState, component, errorMessage);
    }
}

} // namespace flow
} // namespace eez
