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

void executeEvalComponent(FlowState *flowState, unsigned componentIndex) {
    auto assets = flowState->assets;
	auto flowDefinition = assets->flowDefinition.ptr(assets);
	auto flow = flowDefinition->flows.item(assets, flowState->flowIndex);
	auto component = flow->components.item(assets, componentIndex);

	auto expressionPropertyValue = component->propertyValues.item(assets, defs_v3::EVAL_ACTION_COMPONENT_PROPERTY_EXPRESSION);
	Value value;
	if (!evalExpression(flowState, component, expressionPropertyValue->evalInstructions, value)) {
		throwError(flowState, component, "Eval component eval src expression\n");
		return;
	}

    auto &componentOutput = *component->outputs.item(assets, 1);
	propagateValue(flowState, componentOutput, value);
	
	propagateValue(flowState, componentIndex);
}

} // namespace flow
} // namespace eez
