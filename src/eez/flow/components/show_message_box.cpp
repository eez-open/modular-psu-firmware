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

#if OPTION_GUI || !defined(OPTION_GUI)

#include <eez/flow/flow.h>
#include <eez/flow/components.h>
#include <eez/flow/flow_defs_v3.h>
#include <eez/flow/expression.h>

#include <eez/gui/gui.h>

namespace eez {
namespace flow {

const uint8_t MESSAGE_BOX_TYPE_INFO = 1;
const uint8_t MESSAGE_BOX_TYPE_ERROR = 2;
const uint8_t MESSAGE_BOX_TYPE_QUESTION = 3;

struct ShowMessagePageActionComponent : public Component {
	uint8_t type;
};

struct ShowMessagePageComponentExecutionState : public ComponenentExecutionState {
	FlowState *flowState;
    unsigned componentIndex;
};

void questionCallback(void *userParam, unsigned buttonIndex) {
    auto executionState = (ShowMessagePageComponentExecutionState *)userParam;

    auto flowState = executionState->flowState;
    auto componentIndex = executionState->componentIndex;

    deallocateComponentExecutionState(flowState, componentIndex);

    auto component = flowState->flow->components[componentIndex];

    if (buttonIndex < component->outputs.count - 1) {
        propagateValue(flowState, componentIndex, buttonIndex);
    }
}

void executeShowMessageBoxComponent(FlowState *flowState, unsigned componentIndex) {
	auto component = (ShowMessagePageActionComponent *)flowState->flow->components[componentIndex];

    Value messageValue;
    if (!evalProperty(flowState, componentIndex, defs_v3::SHOW_MESSAGE_BOX_ACTION_COMPONENT_PROPERTY_MESSAGE, messageValue, "Failed to evaluate Message in ShowMessageBox")) {
        return;
    }

	if (component->type == MESSAGE_BOX_TYPE_INFO) {
		getAppContextFromId(APP_CONTEXT_ID_DEVICE)->infoMessage(messageValue);
	} else if (component->type == MESSAGE_BOX_TYPE_ERROR) {
		getAppContextFromId(APP_CONTEXT_ID_DEVICE)->errorMessage(messageValue);
	} else if (component->type == MESSAGE_BOX_TYPE_QUESTION) {
        Value buttonsValue;
        if (!evalProperty(flowState, componentIndex, defs_v3::SHOW_MESSAGE_BOX_ACTION_COMPONENT_PROPERTY_BUTTONS, buttonsValue, "Failed to evaluate Buttons in ShowMessageBox")) {
            return;
        }

        if (!buttonsValue.isArray()) {
            throwError(flowState, componentIndex, "Buttons in ShowMessageBox is not an array");
            return;
        }

        auto buttonsArray = buttonsValue.getArray();
        for (uint32_t i = 0; i < buttonsArray->arraySize; i++) {
            if (!buttonsArray->values[i].isString()) {
                char errorMessage[256];
                snprintf(errorMessage, sizeof(errorMessage), "Element at index %d is not a string in Buttons array in ShowMessageBox", (int)i);
                throwError(flowState, componentIndex, errorMessage);
                return;
            }
        }

        auto executionState = allocateComponentExecutionState<ShowMessagePageComponentExecutionState>(flowState, componentIndex);
        executionState->flowState = flowState;
        executionState->componentIndex = componentIndex;

        getAppContextFromId(APP_CONTEXT_ID_DEVICE)->questionDialog(messageValue, buttonsValue, executionState, questionCallback);
    }
}

} // namespace flow
} // namespace eez

#endif // OPTION_GUI || !defined(OPTION_GUI)
