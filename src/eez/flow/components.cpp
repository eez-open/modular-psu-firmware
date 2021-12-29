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

#include <eez/os.h>

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
void executeEvalExprComponent(FlowState *flowState, unsigned componentIndex);
void executeSetVariableComponent(FlowState *flowState, unsigned componentIndex);
void executeSwitchComponent(FlowState *flowState, unsigned componentIndex);
void executeCompareComponent(FlowState *flowState, unsigned componentIndex);
void executeIsTrueComponent(FlowState *flowState, unsigned componentIndex);
void executeConstantComponent(FlowState *flowState, unsigned componentIndex);
void executeLogComponent(FlowState *flowState, unsigned componentIndex);
void executeCallActionComponent(FlowState *flowState, unsigned componentIndex);
void executeDelayComponent(FlowState *flowState, unsigned componentIndex);
void executeErrorComponent(FlowState *flowState, unsigned componentIndex);
void executeCatchErrorComponent(FlowState *flowState, unsigned componentIndex);
void executeLoopComponent(FlowState *flowState, unsigned componentIndex);
void executeScpiComponent(FlowState *flowState, unsigned componentIndex);
void executeShowPageComponent(FlowState *flowState, unsigned componentIndex);
void executeRollerWidgetComponent(FlowState *flowState, unsigned componentIndex);

void executeComponent(FlowState *flowState, unsigned componentIndex) {
	auto component = flowState->flow->components.item(flowState->assets, componentIndex);

	if (component->type == defs_v3::COMPONENT_TYPE_START_ACTION) {
		executeStartComponent(flowState, componentIndex);
    } else if (component->type == defs_v3::COMPONENT_TYPE_END_ACTION) {
		executeEndComponent(flowState, componentIndex);
	} else if (component->type == defs_v3::COMPONENT_TYPE_INPUT_ACTION) {
		executeInputComponent(flowState, componentIndex);
	} else if (component->type == defs_v3::COMPONENT_TYPE_OUTPUT_ACTION) {
		executeOutputComponent(flowState, componentIndex);
    } else if (component->type == defs_v3::COMPONENT_TYPE_EVAL_EXPR_ACTION) {
		executeEvalExprComponent(flowState, componentIndex);
    } else if (component->type == defs_v3::COMPONENT_TYPE_SET_VARIABLE_ACTION) {
		executeSetVariableComponent(flowState, componentIndex);
    } else if (component->type == defs_v3::COMPONENT_TYPE_SWITCH_ACTION) {
		executeSwitchComponent(flowState, componentIndex);
	} else if (component->type == defs_v3::COMPONENT_TYPE_COMPARE_ACTION) {
		executeCompareComponent(flowState, componentIndex);
	} else if (component->type == defs_v3::COMPONENT_TYPE_IS_TRUE_ACTION) {
		executeIsTrueComponent(flowState, componentIndex);
	} else if (component->type == defs_v3::COMPONENT_TYPE_CONSTANT_ACTION) {
		executeConstantComponent(flowState, componentIndex);
	} else if (component->type == defs_v3::COMPONENT_TYPE_LOG_ACTION) {
		executeLogComponent(flowState, componentIndex);
	} else if (component->type == defs_v3::COMPONENT_TYPE_CALL_ACTION_ACTION) {
		executeCallActionComponent(flowState, componentIndex);
	} else if (component->type == defs_v3::COMPONENT_TYPE_DELAY_ACTION) {
		executeDelayComponent(flowState, componentIndex);
    } else if (component->type == defs_v3::COMPONENT_TYPE_ERROR_ACTION) {
		executeErrorComponent(flowState, componentIndex);
    } else if (component->type == defs_v3::COMPONENT_TYPE_CATCH_ERROR_ACTION) {
		executeCatchErrorComponent(flowState, componentIndex);
    } else if (component->type == defs_v3::COMPONENT_TYPE_LOOP_ACTION) {
		executeLoopComponent(flowState, componentIndex);
    } else if (component->type == defs_v3::COMPONENT_TYPE_SCPIACTION) {
		executeScpiComponent(flowState, componentIndex);
	}  else if (component->type == defs_v3::COMPONENT_TYPE_SHOW_PAGE_ACTION) {
		executeShowPageComponent(flowState, componentIndex);
	} else if (component->type < 1000) {
		if (component->type == defs_v3::COMPONENT_TYPE_ROLLER_WIDGET) {
			executeRollerWidgetComponent(flowState, componentIndex);
		}
	} else {
		char errorMessage[100];
		snprintf(errorMessage, sizeof(errorMessage), "Unknown component at index = %d, type = %d\n", componentIndex, component->type);
		throwError(flowState, componentIndex, errorMessage);
    }
}

} // namespace flow
} // namespace eez
