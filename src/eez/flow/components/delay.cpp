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

#include <eez/alloc.h>
#include <eez/system.h>

#include <eez/flow/components.h>
#include <eez/flow/flow_defs_v3.h>
#include <eez/flow/expression.h>

using namespace eez::gui;

namespace eez {
namespace flow {

bool executeDelayComponent(FlowState *flowState, Component *component, ComponenentExecutionState *&componentExecutionState) {
	struct DelayComponenentExecutionState : public ComponenentExecutionState {
		uint32_t waitUntil;
	};

	if (!componentExecutionState) {
		auto assets = flowState->assets;
		auto propertyValue = component->propertyValues.item(assets, defs_v3::DELAY_ACTION_COMPONENT_PROPERTY_MILLISECONDS);

		Value value;
		if (!evalExpression(flowState, component, propertyValue->evalInstructions, value)) {
			throwError(flowState, component, "delay component milliseconds eval error\n");
			return false;
		}

		double milliseconds = value.toDouble();
		if (!isNaN(milliseconds)) {
			auto delayComponentExecutionState = ObjectAllocator<DelayComponenentExecutionState>::allocate(0x28969c75);
			delayComponentExecutionState->waitUntil = millis() + (uint32_t)floor(milliseconds);
			componentExecutionState = delayComponentExecutionState;
		} else {
			throwError(flowState, component, "delay component milliseconds invalid value\n");
			return false;
		}
	} else {
		auto delayComponentExecutionState = (DelayComponenentExecutionState *)componentExecutionState;
		if (millis() >= delayComponentExecutionState->waitUntil) {
			ObjectAllocator<DelayComponenentExecutionState>::deallocate(delayComponentExecutionState);
			componentExecutionState = nullptr;
		}
	}

	return true;
}

} // namespace flow
} // namespace eez
