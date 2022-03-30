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

#include <eez/flow/flow.h>
#include <eez/flow/components.h>
#include <eez/flow/flow_defs_v3.h>
#include <eez/flow/expression.h>
#include <eez/flow/operations.h>
#include <eez/flow/private.h>
#include <eez/flow/hooks.h>

#include <eez/gui/gui.h>

namespace eez {
namespace flow {

struct ShowKeyboardActionComponent : public Component {
	uint8_t password;
};

void executeShowKeyboardComponent(FlowState *flowState, unsigned componentIndex) {
	auto component = (ShowKeyboardActionComponent *)flowState->flow->components[componentIndex];

    Value labelValue;
    if (!evalProperty(flowState, componentIndex, defs_v3::SHOW_KEYBOARD_ACTION_COMPONENT_PROPERTY_LABEL, labelValue)) {
        throwError(flowState, componentIndex, "Failed to evaluate Label in ShowKeyboard\n");
        return;
    }

    Value initialTextValue;
    if (!evalProperty(flowState, componentIndex, defs_v3::SHOW_KEYBOARD_ACTION_COMPONENT_PROPERTY_INITAL_TEXT, initialTextValue)) {
        throwError(flowState, componentIndex, "Failed to evaluate InitialText in ShowKeyboard\n");
        return;
    }

    Value minCharsValue;
    if (!evalProperty(flowState, componentIndex, defs_v3::SHOW_KEYBOARD_ACTION_COMPONENT_PROPERTY_MIN_CHARS, minCharsValue)) {
        throwError(flowState, componentIndex, "Failed to evaluate MinChars in ShowKeyboard\n");
        return;
    }

    Value maxCharsValue;
    if (!evalProperty(flowState, componentIndex, defs_v3::SHOW_KEYBOARD_ACTION_COMPONENT_PROPERTY_MAX_CHARS, maxCharsValue)) {
        throwError(flowState, componentIndex, "Failed to evaluate MaxChars in ShowKeyboard\n");
        return;
    }

	static FlowState *g_showKeyboardFlowState;
	static unsigned g_showKeyboardComponentIndex;

	g_showKeyboardFlowState = flowState;
	g_showKeyboardComponentIndex = componentIndex;

	startAsyncExecution(flowState, componentIndex);

	auto onOk = [](char *value) {
		propagateValue(g_showKeyboardFlowState, g_showKeyboardComponentIndex, 0, Value::makeStringRef(value, -1, 0x87d32fe2));
		getAppContextFromId(APP_CONTEXT_ID_DEVICE)->popPage();
		endAsyncExecution(g_showKeyboardFlowState, g_showKeyboardComponentIndex);
	};

	auto onCancel = []() {
		propagateValue(g_showKeyboardFlowState, g_showKeyboardComponentIndex, 1, Value());
		getAppContextFromId(APP_CONTEXT_ID_DEVICE)->popPage();
		endAsyncExecution(g_showKeyboardFlowState, g_showKeyboardComponentIndex);
	};

	const char *label = labelValue.getString();
	if (label && *label) {
		labelValue = op_add(labelValue, Value(": "));
	}

	showKeyboardHook(labelValue, initialTextValue, minCharsValue, maxCharsValue, component->password, onOk, onCancel);
}

} // namespace flow
} // namespace eez
