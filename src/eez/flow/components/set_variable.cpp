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

using namespace eez::gui;

namespace eez {
namespace flow {

struct SetVariableActionComponent : public Component {
	uint8_t assignableExpressionEvalInstructions[1];
};

void executeSetVariableComponent(FlowState *flowState, unsigned componentIndex) {
    auto component = (SetVariableActionComponent *)flowState->flow->components.item(componentIndex);

	Value dstValue;
	if (!evalAssignableExpression(flowState, componentIndex, component->assignableExpressionEvalInstructions, dstValue)) {
		throwError(flowState, componentIndex, "Failed to evaluate Variable in SetVariable\n");
		return;
	}

	auto propertyValue = component->propertyValues.item(defs_v3::SET_VARIABLE_ACTION_COMPONENT_PROPERTY_VALUE);
	Value srcValue;
	if (!evalExpression(flowState, componentIndex, propertyValue->evalInstructions, srcValue)) {
		throwError(flowState, componentIndex, "Failed to evaluate Value in SetVariable\n");
		return;
	}

	assignValue(flowState, componentIndex, dstValue, srcValue);
	
	propagateValueThroughSeqout(flowState, componentIndex);
}

} // namespace flow
} // namespace eez
