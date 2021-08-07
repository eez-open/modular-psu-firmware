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

#include <eez/scripting/scripting.h>

#include <eez/flow/components.h>

using namespace eez::gui;

namespace eez {
namespace flow {

bool executeEndComponent(FlowState *flowState, Component *component, ComponenentExecutionState *&componentExecutionState) {
	if (flowState->parentFlowState) {
		flowState->parentFlowState->numActiveComponents--;

		auto assets = flowState->assets;
		auto flowDefinition = assets->flowDefinition.ptr(assets);
		auto &nullValue = *flowDefinition->constants.item(assets, NULL_VALUE_INDEX);
		propagateValue(flowState->parentFlowState, *flowState->parentComponent->outputs.item(assets, 0), nullValue);
		return true;
	} else {
		scripting::stopScript();
		return false;
	}
}

} // namespace flow
} // namespace eez
