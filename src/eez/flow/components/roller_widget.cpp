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
#include <eez/flow/components/roller_widget.h>
#include <eez/gui/widgets/roller.h>

using namespace eez::gui;

namespace eez {
namespace flow {

void executeRollerWidgetComponent(FlowState *flowState, unsigned componentIndex) {
	auto component = flowState->flow->components[componentIndex];

	static const unsigned START_INPUT_INDEX = 0;
	auto startInputIndex = component->inputs[START_INPUT_INDEX];
	if (flowState->values[startInputIndex].type != VALUE_TYPE_UNDEFINED) {
        auto executionState = (RollerWidgetComponenentExecutionState *)flowState->componenentExecutionStates[componentIndex];
        if (!executionState) {
            executionState = ObjectAllocator<RollerWidgetComponenentExecutionState>::allocate(0x4c669d4b);
			flowState->componenentExecutionStates[componentIndex] = executionState;
		}
		executionState->clear = true;
	}
}

} // namespace flow
} // namespace eez
