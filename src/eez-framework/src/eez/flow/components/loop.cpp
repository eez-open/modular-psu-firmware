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

#include <eez/flow/components.h>
#include <eez/flow/flow_defs_v3.h>
#include <eez/flow/expression.h>
#include <eez/flow/operations.h>

namespace eez {
namespace flow {

struct LoopComponenentExecutionState : public ComponenentExecutionState {
    Value dstValue;
    Value toValue;
    Value currentValue;
};

void executeLoopComponent(FlowState *flowState, unsigned componentIndex) {
    auto component = flowState->flow->components[componentIndex];

    auto loopComponentExecutionState = (LoopComponenentExecutionState *)flowState->componenentExecutionStates[componentIndex];

    // restart loop if entered through "start" input
    static const unsigned START_INPUT_INDEX = 0;
    auto startInputIndex = component->inputs[START_INPUT_INDEX];
    if (flowState->values[startInputIndex].type != VALUE_TYPE_UNDEFINED) {
        if (loopComponentExecutionState) {
            deallocateComponentExecutionState(flowState, componentIndex);
            loopComponentExecutionState = nullptr;
        }
    } else {
        if (!loopComponentExecutionState) {
            return;
        }
    }

    Value stepValue;
    if (!evalProperty(flowState, componentIndex, defs_v3::LOOP_ACTION_COMPONENT_PROPERTY_STEP, stepValue, "Failed to evaluate Step in Loop")) {
        return;
    }

    if (!loopComponentExecutionState) {
        Value dstValue;
        if (!evalAssignableProperty(flowState, componentIndex, defs_v3::LOOP_ACTION_COMPONENT_PROPERTY_VARIABLE, dstValue, "Failed to evaluate Variable in Loop")) {
            return;
        }

        Value fromValue;
        if (!evalProperty(flowState, componentIndex, defs_v3::LOOP_ACTION_COMPONENT_PROPERTY_FROM, fromValue, "Failed to evaluate From in Loop")) {
            return;
        }

        Value toValue;
        if (!evalProperty(flowState, componentIndex, defs_v3::LOOP_ACTION_COMPONENT_PROPERTY_TO, toValue, "Failed to evaluate To in Loop")) {
            return;
        }

        loopComponentExecutionState = allocateComponentExecutionState<LoopComponenentExecutionState>(flowState, componentIndex);
        loopComponentExecutionState->dstValue = dstValue;
        loopComponentExecutionState->toValue = toValue;

		loopComponentExecutionState->currentValue = fromValue;
    } else {
        loopComponentExecutionState->currentValue = op_add(loopComponentExecutionState->currentValue, stepValue);
    }

    bool condition;
    if (stepValue.getInt() > 0) {
        condition = op_great(loopComponentExecutionState->currentValue, loopComponentExecutionState->toValue).toBool();
    } else {
        condition = op_less(loopComponentExecutionState->currentValue, loopComponentExecutionState->toValue).toBool();
    }

    if (condition) {
        // done
        deallocateComponentExecutionState(flowState, componentIndex);
        propagateValue(flowState, componentIndex, component->outputs.count - 1);
    } else {
        assignValue(flowState, componentIndex, loopComponentExecutionState->dstValue, loopComponentExecutionState->currentValue);
        propagateValueThroughSeqout(flowState, componentIndex);
    }
}

} // namespace flow
} // namespace eez
