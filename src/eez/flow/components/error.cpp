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

void executeErrorComponent(FlowState *flowState, unsigned componentIndex) {
    auto assets = flowState->assets;
    auto component = (Component *)flowState->flow->components.item(assets, componentIndex);

	if (flowState->parentFlowState && flowState->isAction) {
		flowState->parentFlowState->numActiveComponents--;
	}

	auto propertyValue = component->propertyValues.item(assets, defs_v3::EVAL_EXPR_ACTION_COMPONENT_PROPERTY_EXPRESSION);
	Value expressionValue;
	if (!evalExpression(flowState, componentIndex, propertyValue->evalInstructions, expressionValue)) {
		throwError(flowState, componentIndex, "Failed to evaluate Message in Error\n");
		return;
	}

	throwError(flowState, componentIndex, expressionValue.getString());
}

} // namespace flow
} // namespace eez