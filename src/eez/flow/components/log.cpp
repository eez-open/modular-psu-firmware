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

#include <eez/core/debug.h>

#include <eez/flow/components.h>
#include <eez/flow/flow_defs_v3.h>
#include <eez/flow/expression.h>
#include <eez/flow/debugger.h>

namespace eez {
namespace flow {

void executeLogComponent(FlowState *flowState, unsigned componentIndex) {
    Value value;
    if (!evalProperty(flowState, componentIndex, defs_v3::LOG_ACTION_COMPONENT_PROPERTY_VALUE, value, "Failed to evaluate Message in Log")) {
        return;
    }

    Value strValue = value.toString(0x0f9812ee);
    const char *valueStr = strValue.getString();
    if (valueStr && *valueStr) {
      logInfo(flowState, componentIndex, valueStr);
    }

	propagateValueThroughSeqout(flowState, componentIndex);
}

} // namespace flow
} // namespace eez
