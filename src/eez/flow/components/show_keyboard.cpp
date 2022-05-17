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
    if (!evalProperty(flowState, componentIndex, defs_v3::SHOW_KEYBOARD_ACTION_COMPONENT_PROPERTY_LABEL, labelValue, "Failed to evaluate Label in ShowKeyboard")) {
        return;
    }

    Value initialTextValue;
    if (!evalProperty(flowState, componentIndex, defs_v3::SHOW_KEYBOARD_ACTION_COMPONENT_PROPERTY_INITAL_TEXT, initialTextValue, "Failed to evaluate InitialText in ShowKeyboard")) {
        return;
    }

    Value minCharsValue;
    if (!evalProperty(flowState, componentIndex, defs_v3::SHOW_KEYBOARD_ACTION_COMPONENT_PROPERTY_MIN_CHARS, minCharsValue, "Failed to evaluate MinChars in ShowKeyboard")) {
        return;
    }

    Value maxCharsValue;
    if (!evalProperty(flowState, componentIndex, defs_v3::SHOW_KEYBOARD_ACTION_COMPONENT_PROPERTY_MAX_CHARS, maxCharsValue, "Failed to evaluate MaxChars in ShowKeyboard")) {
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
