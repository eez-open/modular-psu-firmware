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

#if EEZ_OPTION_GUI

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

void executeShowKeypadComponent(FlowState *flowState, unsigned componentIndex) {
    Value labelValue;
    if (!evalProperty(flowState, componentIndex, defs_v3::SHOW_KEYBOARD_ACTION_COMPONENT_PROPERTY_LABEL, labelValue, "Failed to evaluate Label in ShowKeypad")) {
        return;
    }

    Value initialValue;
    if (!evalProperty(flowState, componentIndex, defs_v3::SHOW_KEYPAD_ACTION_COMPONENT_PROPERTY_INITAL_VALUE, initialValue, "Failed to evaluate InitialValue in ShowKeypad")) {
        return;
    }

    Value minValue;
    if (!evalProperty(flowState, componentIndex, defs_v3::SHOW_KEYPAD_ACTION_COMPONENT_PROPERTY_MIN, minValue, "Failed to evaluate Min in ShowKeypad")) {
        return;
    }

    Value maxValue;
    if (!evalProperty(flowState, componentIndex, defs_v3::SHOW_KEYPAD_ACTION_COMPONENT_PROPERTY_MAX, maxValue, "Failed to evaluate Max in ShowKeypad")) {
        return;
    }

    Value unitValue;
    if (!evalProperty(flowState, componentIndex, defs_v3::SHOW_KEYPAD_ACTION_COMPONENT_PROPERTY_UNIT, unitValue, "Failed to evaluate Unit in ShowKeypad")) {
        return;
    }

	static FlowState *g_showKeyboardFlowState;
	static unsigned g_showKeyboardComponentIndex;

	g_showKeyboardFlowState = flowState;
	g_showKeyboardComponentIndex = componentIndex;

	startAsyncExecution(flowState, componentIndex);

	auto onOk = [](float value) {
        Value precisionValue;
        if (!evalProperty(g_showKeyboardFlowState, g_showKeyboardComponentIndex, defs_v3::SHOW_KEYPAD_ACTION_COMPONENT_PROPERTY_PRECISION, precisionValue, "Failed to evaluate Precision in ShowKeypad")) {
            return;
        }

        float precision = precisionValue.toFloat();

        Value unitValue;
        if (!evalProperty(g_showKeyboardFlowState, g_showKeyboardComponentIndex, defs_v3::SHOW_KEYPAD_ACTION_COMPONENT_PROPERTY_UNIT, unitValue, "Failed to evaluate Unit in ShowKeypad")) {
            return;
        }

		Unit unit = getUnitFromName(unitValue.getString());

        value = roundPrec(value, precision) / getUnitFactor(unit);

		propagateValue(g_showKeyboardFlowState, g_showKeyboardComponentIndex, 0, Value(value, VALUE_TYPE_FLOAT));
		getAppContextFromId(APP_CONTEXT_ID_DEVICE)->popPage();
        endAsyncExecution(g_showKeyboardFlowState, g_showKeyboardComponentIndex);
	};

	auto onCancel = []() {
		propagateValue(g_showKeyboardFlowState, g_showKeyboardComponentIndex, 1);
		getAppContextFromId(APP_CONTEXT_ID_DEVICE)->popPage();
		endAsyncExecution(g_showKeyboardFlowState, g_showKeyboardComponentIndex);
	};


	Unit unit = getUnitFromName(unitValue.getString());

    initialValue.unit = unit;

	showKeypadHook(labelValue, initialValue, minValue, maxValue, unit, onOk, onCancel);
}

} // namespace flow
} // namespace eez

#endif // EEZ_OPTION_GUI
