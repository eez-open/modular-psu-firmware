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
#include <eez/flow/flow_defs_v3.h>
#include <eez/flow/expression.h>
#include <eez/flow/operations.h>

using namespace eez::gui;

namespace eez {
namespace flow {

struct LoopActionComponent : public Component {
    uint8_t assignableExpressionEvalInstructions[1];
};

struct LoopComponenentExecutionState : public ComponenentExecutionState {
    Value dstValue;
    Value toValue;
    Value stepValue;
};

void executeLoopComponent(FlowState *flowState, unsigned componentIndex) {
    auto assets = flowState->assets;
    auto component = (LoopActionComponent *)flowState->flow->components.item(assets, componentIndex);

    auto loopComponentExecutionState = (LoopComponenentExecutionState *)flowState->componenentExecutionStates[componentIndex];

    if (!loopComponentExecutionState) {
        Value dstValue;
        if (!evalAssignableExpression(flowState, componentIndex, component->assignableExpressionEvalInstructions, dstValue)) {
            throwError(flowState, componentIndex, "loop component eval dest assignable expression\n");
            return;
        }

        auto fromPropertyValue = component->propertyValues.item(assets, defs_v3::LOOP_ACTION_COMPONENT_PROPERTY_FROM);
        Value fromValue;
        if (!evalExpression(flowState, componentIndex, fromPropertyValue->evalInstructions, fromValue)) {
            throwError(flowState, componentIndex, "loop component eval 'from' expression\n");
            return;
        }

        auto toPropertyValue = component->propertyValues.item(assets, defs_v3::LOOP_ACTION_COMPONENT_PROPERTY_TO);
        Value toValue;
        if (!evalExpression(flowState, componentIndex, toPropertyValue->evalInstructions, toValue)) {
            throwError(flowState, componentIndex, "loop component eval 'to' expression\n");
            return;
        }

        auto stepPropertyValue = component->propertyValues.item(assets, defs_v3::LOOP_ACTION_COMPONENT_PROPERTY_STEP);
        Value stepValue;
        if (!evalExpression(flowState, componentIndex, stepPropertyValue->evalInstructions, stepValue)) {
            throwError(flowState, componentIndex, "loop component eval 'step' expression\n");
            return;
        }

        assignValue(flowState, componentIndex, dstValue, fromValue);

        loopComponentExecutionState = ObjectAllocator<LoopComponenentExecutionState>::allocate(0xd33c9288);
        loopComponentExecutionState->dstValue = dstValue;
        loopComponentExecutionState->toValue = toValue;
        loopComponentExecutionState->stepValue = stepValue;
        flowState->componenentExecutionStates[componentIndex] = loopComponentExecutionState;

        propagateValue(flowState, componentIndex);
    } else {
        auto value = op_add(loopComponentExecutionState->dstValue, loopComponentExecutionState->stepValue);
        if (op_great_eq(value, loopComponentExecutionState->toValue).toBool()) {
			ObjectAllocator<LoopComponenentExecutionState>::deallocate(loopComponentExecutionState);
			flowState->componenentExecutionStates[componentIndex] = nullptr;

		    propagateValue(flowState, componentIndex, 1);
		} else {
			assignValue(flowState, componentIndex, loopComponentExecutionState->dstValue, value);
            propagateValue(flowState, componentIndex);
		}
    }
}

} // namespace flow
} // namespace eez
