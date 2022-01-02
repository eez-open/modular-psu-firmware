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

struct SwitchTest {
    uint8_t outputIndex;
    uint8_t conditionInstructions[1];
};

struct SwitchActionComponent : public Component {
    ListOfAssetsPtr<SwitchTest> tests;
};

void executeSwitchComponent(FlowState *flowState, unsigned componentIndex) {
    auto assets = flowState->assets;
    auto component = (SwitchActionComponent *)flowState->flow->components.item(assets, componentIndex);

    for (uint32_t testIndex = 0; testIndex < component->tests.count; testIndex++) {
        auto test = component->tests.item(assets, testIndex);

        Value conditionValue;
        if (!evalExpression(flowState, componentIndex, test->conditionInstructions, conditionValue)) {
            throwError(flowState, componentIndex, "Failed to evaluate test condition in Switch\n");
            return;
        }

        if (conditionValue.getBoolean()) {
            propagateValue(flowState, componentIndex, test->outputIndex);
			break;
        }
    }

	propagateValueThroughSeqout(flowState, componentIndex);
}

} // namespace flow
} // namespace eez
