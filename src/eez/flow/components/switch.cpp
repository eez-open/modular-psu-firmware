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

using namespace eez::gui;

namespace eez {
namespace flow {

bool executeSwitchComponent(Assets *assets, FlowState *flowState, Component *component, ComponenentExecutionState *&componentExecutionState) {
	struct SwitchTest {
        uint8_t outputIndex;
		uint8_t conditionInstructions[1];
	};

	struct SwitchActionComponent : public Component {
        ListOfAssetsPtr<SwitchTest> tests;
	};

	auto switchActionComponent = (SwitchActionComponent *)component;

    for (uint32_t testIndex = 0; testIndex < switchActionComponent->tests.count; testIndex++) {
        auto test = switchActionComponent->tests.item(assets, testIndex);

        Value conditionValue;
        if (!evalExpression(assets, flowState, test->conditionInstructions, conditionValue)) {
            throwError(assets, flowState, component, "switch component eval src expression\n");
            return false;
        }

        if (conditionValue.getBoolean()) {
	        auto flowDefinition = assets->flowDefinition.ptr(assets);
			auto &nullValue = *flowDefinition->constants.item(assets, NULL_VALUE_INDEX);
			auto &componentOutput = *component->outputs.item(assets, test->outputIndex);
            propagateValue(assets, flowState, componentOutput, nullValue);
			break;
        }
    }

    return true;
}

} // namespace flow
} // namespace eez
