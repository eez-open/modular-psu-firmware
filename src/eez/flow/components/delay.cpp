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

#define FLOW_DEBUG 0

using namespace eez::gui;

namespace eez {
namespace flow {

void executeDelayComponent(Assets *assets, FlowState *flowState, Component *component, ComponenentExecutionState *&componentExecutionState) {
#if FLOW_DEBUG
	printf("Execute DELAY component at index = %d\n", componentIndex);
#endif

	struct DelayComponenentExecutionState : public ComponenentExecutionState {
		uint32_t waitUntil;
	};

	if (!componentExecutionState) {
		auto propertyValue = component->propertyValues.item(assets, defs_v3::DELAY_ACTION_COMPONENT_PROPERTY_MILLISECONDS);

		Value value;
		if (!evalExpression(assets, flowState, propertyValue->evalInstructions, value)) {
			throwError();
			return;
		}

		double milliseconds = value.toDouble();
		if (!isNaN(milliseconds)) {
			auto delayComponentExecutionState = ObjectAllocator<DelayComponenentExecutionState>::allocate();
			delayComponentExecutionState->waitUntil = millis() + (uint32_t)floor(milliseconds);
			componentExecutionState = delayComponentExecutionState;
		} else {
			throwError();
			return;
		}
	} else {
		auto delayComponentExecutionState = (DelayComponenentExecutionState *)componentExecutionState;
		if (millis() >= delayComponentExecutionState->waitUntil) {
			ObjectAllocator<DelayComponenentExecutionState>::deallocate(delayComponentExecutionState);
			componentExecutionState = nullptr;
		}
	}
}

} // namespace flow
} // namespace eez
