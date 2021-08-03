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

void executeSetVariableComponent(Assets *assets, FlowState *flowState, Component *component, ComponenentExecutionState *&componentExecutionState) {
	struct SetVariableActionComponent : public Component {
		uint8_t assignableExpressionEvalInstructions[1];
	};
	auto setVariableActionComponent = (SetVariableActionComponent *)component;

	Value dstValue;
	if (!evalAssignableExpression(assets, flowState, setVariableActionComponent->assignableExpressionEvalInstructions, dstValue)) {
		throwError("setvariable component eval dest assignable expression\n");
		return;
	}

	auto propertyValue = component->propertyValues.item(assets, defs_v3::SET_VARIABLE_ACTION_COMPONENT_PROPERTY_VALUE);
	Value srcValue;
	if (!evalExpression(assets, flowState, propertyValue->evalInstructions, srcValue)) {
		throwError("setvariable component eval src expression\n");
		return;
	}

	assignValue(assets, flowState, component, dstValue, srcValue);
}

} // namespace flow
} // namespace eez
