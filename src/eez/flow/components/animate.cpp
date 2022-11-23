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

#include <eez/core/os.h>

#include <eez/flow/components.h>
#include <eez/flow/flow_defs_v3.h>
#include <eez/flow/expression.h>
#include <eez/flow/private.h>
#include <eez/flow/queue.h>
#include <eez/flow/debugger.h>

#if EEZ_OPTION_GUI || !defined(EEZ_OPTION_GUI)
#include <eez/gui/gui.h>
using namespace eez::gui;
#endif

namespace eez {
namespace flow {

struct AnimateComponenentExecutionState : public ComponenentExecutionState {
    float startPosition;
    float endPosition;
    float speed;
    uint32_t startTimestamp;
};

void executeAnimateComponent(FlowState *flowState, unsigned componentIndex) {
	auto state = (AnimateComponenentExecutionState *)flowState->componenentExecutionStates[componentIndex];
	if (!state) {
        Value fromValue;
        if (!evalProperty(flowState, componentIndex, defs_v3::ANIMATE_ACTION_COMPONENT_PROPERTY_FROM, fromValue, "Failed to evaluate From in Animate")) {
            return;
        }

        Value toValue;
        if (!evalProperty(flowState, componentIndex, defs_v3::ANIMATE_ACTION_COMPONENT_PROPERTY_TO, toValue, "Failed to evaluate To in Animate")) {
            return;
        }

        Value speedValue;
        if (!evalProperty(flowState, componentIndex, defs_v3::ANIMATE_ACTION_COMPONENT_PROPERTY_SPEED, speedValue, "Failed to evaluate Speed in Animate")) {
            return;
        }

        float from = fromValue.toFloat();
        float to = toValue.toFloat();
        float speed = speedValue.toFloat();

        if (speed == 0) {
            flowState->timelinePosition = to;
            onFlowStateTimelineChanged(flowState);

            propagateValueThroughSeqout(flowState, componentIndex);
        } else {
		    state = allocateComponentExecutionState<AnimateComponenentExecutionState>(flowState, componentIndex);

            state->startPosition = from;
            state->endPosition = to;
            state->speed = speed;
            state->startTimestamp = millis();

            if (!addToQueue(flowState, componentIndex, -1, -1, -1, true)) {
                throwError(flowState, componentIndex, "Execution queue is full\n");
                return;
            }
        }
    } else {
        float currentTime;

        if (state->startPosition < state->endPosition) {
            currentTime = state->startPosition + state->speed * (millis() - state->startTimestamp) / 1000.0f;
            if (currentTime >= state->endPosition) {
                currentTime = state->endPosition;
            }
        } else {
            currentTime = state->startPosition - state->speed * (millis() - state->startTimestamp) / 1000.0f;
            if (currentTime <= state->endPosition) {
                currentTime = state->endPosition;
            }
        }

        flowState->timelinePosition = currentTime;
        onFlowStateTimelineChanged(flowState);

        if (currentTime == state->endPosition) {
            deallocateComponentExecutionState(flowState, componentIndex);
            propagateValueThroughSeqout(flowState, componentIndex);
        } else {
            if (!addToQueue(flowState, componentIndex, -1, -1, -1, true)) {
                throwError(flowState, componentIndex, "Execution queue is full\n");
                return;
            }
        }
    }
}

} // namespace flow
} // namespace eez
