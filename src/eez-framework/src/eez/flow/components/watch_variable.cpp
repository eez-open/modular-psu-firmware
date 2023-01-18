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

#include <eez/conf-internal.h>

#include <stdio.h>

#include <eez/core/alloc.h>
#include <eez/core/os.h>

#include <eez/flow/components.h>
#include <eez/flow/flow_defs_v3.h>
#include <eez/flow/expression.h>
#include <eez/flow/queue.h>

namespace eez {
namespace flow {

struct WatchVariableComponenentExecutionState : public ComponenentExecutionState {
	Value value;
};

void executeWatchVariableComponent(FlowState *flowState, unsigned componentIndex) {
	auto watchVariableComponentExecutionState = (WatchVariableComponenentExecutionState *)flowState->componenentExecutionStates[componentIndex];

    Value value;
    if (!evalProperty(flowState, componentIndex, defs_v3::WATCH_VARIABLE_ACTION_COMPONENT_PROPERTY_VARIABLE, value, "Failed to evaluate Variable in WatchVariable")) {
        return;
    }

	if (!watchVariableComponentExecutionState) {
        watchVariableComponentExecutionState = allocateComponentExecutionState<WatchVariableComponenentExecutionState>(flowState, componentIndex);
        watchVariableComponentExecutionState->value = value;

        propagateValue(flowState, componentIndex, 1, value);

		if (!addToQueue(flowState, componentIndex, -1, -1, -1, true)) {
			return;
		}
	} else {
		if (value != watchVariableComponentExecutionState->value) {
            watchVariableComponentExecutionState->value = value;
			propagateValue(flowState, componentIndex, 1, value);
		}

        if (canFreeFlowState(flowState, false)) {
            deallocateComponentExecutionState(flowState, componentIndex);
        } else {
            if (!addToQueue(flowState, componentIndex, -1, -1, -1, true)) {
                return;
            }
        }
	}
}

} // namespace flow
} // namespace eez
