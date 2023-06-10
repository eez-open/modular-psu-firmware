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

#include <eez/conf-internal.h>

#include <eez/core/alloc.h>
#include <eez/core/os.h>

#include <eez/flow/components.h>
#include <eez/flow/flow.h>
#include <eez/flow/flow_defs_v3.h>
#include <eez/flow/queue.h>
#include <eez/flow/components/input.h>
#include <eez/flow/components/lvgl_user_widget.h>

#if defined(EEZ_FOR_LVGL)

namespace eez {
namespace flow {

struct LVGLUserWidgetComponent : public Component {
	int16_t flowIndex;
	uint8_t inputsStartIndex;
	uint8_t outputsStartIndex;
    int32_t widgetStartIndex;
};

LVGLUserWidgetExecutionState *createUserWidgetFlowState(FlowState *flowState, unsigned userWidgetWidgetComponentIndex) {
    auto component = (LVGLUserWidgetComponent *)flowState->flow->components[userWidgetWidgetComponentIndex];
    auto userWidgetFlowState = initPageFlowState(flowState->assets, component->flowIndex, flowState, userWidgetWidgetComponentIndex);
    userWidgetFlowState->lvglWidgetStartIndex = component->widgetStartIndex;
    auto userWidgetWidgetExecutionState = allocateComponentExecutionState<LVGLUserWidgetExecutionState>(flowState, userWidgetWidgetComponentIndex);
    userWidgetWidgetExecutionState->flowState = userWidgetFlowState;
    return userWidgetWidgetExecutionState;
}

void executeLVGLUserWidgetComponent(FlowState *flowState, unsigned componentIndex) {
    auto component = (LVGLUserWidgetComponent *)flowState->flow->components[componentIndex];

    auto userWidgetWidgetExecutionState = (LVGLUserWidgetExecutionState *)flowState->componenentExecutionStates[componentIndex];
    if (!userWidgetWidgetExecutionState) {
        userWidgetWidgetExecutionState = createUserWidgetFlowState(flowState, componentIndex);
    }
    auto userWidgetFlowState = userWidgetWidgetExecutionState->flowState;

    for (
        unsigned userWidgetComponentIndex = 0;
        userWidgetComponentIndex < userWidgetFlowState->flow->components.count;
        userWidgetComponentIndex++
    ) {
        auto userWidgetComponent = userWidgetFlowState->flow->components[userWidgetComponentIndex];
        if (userWidgetComponent->type == defs_v3::COMPONENT_TYPE_INPUT_ACTION) {
            auto inputActionComponentExecutionState = (InputActionComponentExecutionState *)userWidgetFlowState->componenentExecutionStates[userWidgetComponentIndex];
            if (inputActionComponentExecutionState) {
                Value value;
                if (getCallActionValue(userWidgetFlowState, userWidgetComponentIndex, value)) {
                    if (inputActionComponentExecutionState->value != value) {
                        addToQueue(userWidgetWidgetExecutionState->flowState, userWidgetComponentIndex, -1, -1, -1, false);
                        inputActionComponentExecutionState->value = value;
                    }
                } else {
                    return;
                }
            }
        } else if (userWidgetComponent->type == defs_v3::COMPONENT_TYPE_START_ACTION) {
            Value value;
            if (getCallActionValue(userWidgetFlowState, userWidgetComponentIndex, value)) {
                if (value.getType() != VALUE_TYPE_UNDEFINED) {
                    addToQueue(userWidgetWidgetExecutionState->flowState, userWidgetComponentIndex, -1, -1, -1, false);
                }
            } else {
                return;
            }
        }
    }
}

} // namespace flow
} // namespace eez


#else

namespace eez {
namespace flow {

void executeLVGLUserWidgetComponent(FlowState *flowState, unsigned componentIndex) {
    throwError(flowState, componentIndex, "Not implemented");
}

} // namespace flow
} // namespace eez

#endif
