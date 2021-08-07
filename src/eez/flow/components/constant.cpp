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

using namespace eez::gui;

namespace eez {
namespace flow {

struct ConstantActionComponent : public Component {
	uint16_t valueIndex;
};

void executeConstantComponent(FlowState *flowState, unsigned componentIndex) {
 	auto assets = flowState->assets;
	auto flowDefinition = assets->flowDefinition.ptr(assets);
	auto flow = flowDefinition->flows.item(assets, flowState->flowIndex);
	auto component = flow->components.item(assets, componentIndex);

	auto constantActionComponent = (ConstantActionComponent *)component;
	auto &sourceValue = *flowDefinition->constants.item(assets, constantActionComponent->valueIndex);

	// TODO ovdje treba poslati null value za 0-ti output
	for (unsigned outputIndex = 0; outputIndex < component->outputs.count; outputIndex++) {
		auto &componentOutput = *component->outputs.item(assets, outputIndex);
		propagateValue(flowState, componentOutput, sourceValue);
	}

	propagateValue(flowState, componentIndex);
}

} // namespace flow
} // namespace eez
