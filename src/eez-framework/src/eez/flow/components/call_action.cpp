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

#include <eez/core/action.h>

#include <eez/flow/components.h>
#include <eez/flow/components/call_action.h>
#include <eez/flow/debugger.h>
#include <eez/flow/queue.h>

namespace eez {
namespace flow {

CallActionComponenentExecutionState::~CallActionComponenentExecutionState() {
    freeFlowState(flowState);
}

void executeCallAction(FlowState *flowState, unsigned componentIndex, int flowIndex) {
	if (flowIndex >= (int)flowState->flowDefinition->flows.count) {
		executeActionFunction(flowIndex - flowState->flowDefinition->flows.count);
		propagateValueThroughSeqout(flowState, componentIndex);
		return;
	}

    if ((int)componentIndex != -1) {
        auto callActionComponenentExecutionState = (CallActionComponenentExecutionState *)flowState->componenentExecutionStates[componentIndex];
        if (callActionComponenentExecutionState) {
            if (canFreeFlowState(callActionComponenentExecutionState->flowState)) {
                freeFlowState(callActionComponenentExecutionState->flowState);
            } else {
                if (!addToQueue(flowState, componentIndex, -1, -1, -1, true)) {
			        throwError(flowState, componentIndex, "Execution queue is full\n");
		        }
                return;
            }
        }
    }

	FlowState *actionFlowState = initActionFlowState(flowIndex, flowState, componentIndex);

	if (canFreeFlowState(actionFlowState)) {
		freeFlowState(actionFlowState);
        if ((int)componentIndex != -1) {
		    propagateValueThroughSeqout(flowState, componentIndex);
        }
	} else {
        if ((int)componentIndex != -1) {
		    auto callActionComponenentExecutionState = allocateComponentExecutionState<CallActionComponenentExecutionState>(flowState, componentIndex);
		    callActionComponenentExecutionState->flowState = actionFlowState;
        }
	}
}

void executeCallActionComponent(FlowState *flowState, unsigned componentIndex) {
	auto component = (CallActionActionComponent *)flowState->flow->components[componentIndex];

	auto flowIndex = component->flowIndex;
	if (flowIndex < 0) {
		throwError(flowState, componentIndex, "Invalid action flow index in CallAction\n");
		return;
	}

    executeCallAction(flowState, componentIndex, flowIndex);
}

} // namespace flow
} // namespace eez
