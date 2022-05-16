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
#include <eez/flow/components/set_variable.h>

using namespace eez::gui;

namespace eez {
namespace flow {

void executeSetVariableComponent(FlowState *flowState, unsigned componentIndex) {
    auto component = (SetVariableActionComponent *)flowState->flow->components[componentIndex];

    for (uint32_t entryIndex = 0; entryIndex < component->entries.count; entryIndex++) {
        auto entry = component->entries[entryIndex];

        Value dstValue;
        if (!evalAssignableExpression(flowState, componentIndex, entry->variable, dstValue)) {
            char strMessage[256];
            snprintf(strMessage, sizeof(strMessage), "Failed to evaluate Variable no. %d in SetVariable\n", (int)(entryIndex + 1));
            throwError(flowState, componentIndex, strMessage);
            return;
        }

        Value srcValue;
        if (!evalExpression(flowState, componentIndex, entry->value, srcValue)) {
            char strMessage[256];
            snprintf(strMessage, sizeof(strMessage), "Failed to evaluate Value no. %d in SetVariable\n", (int)(entryIndex + 1));
            throwError(flowState, componentIndex, strMessage);
            return;
        }

        assignValue(flowState, componentIndex, dstValue, srcValue);
    }

	propagateValueThroughSeqout(flowState, componentIndex);
}

} // namespace flow
} // namespace eez
