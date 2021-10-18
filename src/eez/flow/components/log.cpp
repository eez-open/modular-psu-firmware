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

#include <eez/debug.h>

#include <eez/flow/components.h>
#include <eez/flow/flow_defs_v3.h>
#include <eez/flow/expression.h>
#include <eez/flow/debugger.h>

using namespace eez::gui;

namespace eez {
namespace flow {

void executeLogComponent(FlowState *flowState, unsigned componentIndex) {
    auto assets = flowState->assets;
    auto component = flowState->flow->components.item(assets, componentIndex);

    auto propertyValue = component->propertyValues.item(assets, defs_v3::LOG_ACTION_COMPONENT_PROPERTY_VALUE);

    Value value;
    if (!evalExpression(flowState, componentIndex, propertyValue->evalInstructions, value)) {
        throwError(flowState, componentIndex, "log component value eval error\n");
        return;
    }

    const char *valueStr = value.toString(0x0f9812ee).getString();
    if (valueStr && *valueStr) {
      logInfo(flowState, componentIndex, valueStr);
    }

	propagateValueThroughSeqout(flowState, componentIndex);
}

} // namespace flow
} // namespace eez
