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

void executeShowKeypadComponent(FlowState *flowState, unsigned componentIndex) {
	auto component = (ShowKeyboardActionComponent *)flowState->flow->components.item(componentIndex);

    auto labelPropertyValue = component->propertyValues.item(defs_v3::SHOW_KEYBOARD_ACTION_COMPONENT_PROPERTY_LABEL);
    Value labelValue;
    if (!evalExpression(flowState, componentIndex, labelPropertyValue->evalInstructions, labelValue)) {
        throwError(flowState, componentIndex, "Failed to evaluate Label in ShowKeypad\n");
        return;
    }

    auto initialValuePropertyValue = component->propertyValues.item(defs_v3::SHOW_KEYPAD_ACTION_COMPONENT_PROPERTY_INITAL_VALUE);
    Value initialValue;
    if (!evalExpression(flowState, componentIndex, initialValuePropertyValue->evalInstructions, initialValue)) {
        throwError(flowState, componentIndex, "Failed to evaluate InitialValue in ShowKeypad\n");
        return;
    }

    auto minPropertyValue = component->propertyValues.item(defs_v3::SHOW_KEYPAD_ACTION_COMPONENT_PROPERTY_MIN);
    Value minValue;
    if (!evalExpression(flowState, componentIndex, minPropertyValue->evalInstructions, minValue)) {
        throwError(flowState, componentIndex, "Failed to evaluate Min in ShowKeypad\n");
        return;
    }

    auto maxPropertyValue = component->propertyValues.item(defs_v3::SHOW_KEYPAD_ACTION_COMPONENT_PROPERTY_MAX);
    Value maxValue;
    if (!evalExpression(flowState, componentIndex, maxPropertyValue->evalInstructions, maxValue)) {
        throwError(flowState, componentIndex, "Failed to evaluate Max in ShowKeypad\n");
        return;
    }

    auto unitPropertyValue = component->propertyValues.item(defs_v3::SHOW_KEYPAD_ACTION_COMPONENT_PROPERTY_UNIT);
    Value unitValue;
    if (!evalExpression(flowState, componentIndex, unitPropertyValue->evalInstructions, unitValue)) {
        throwError(flowState, componentIndex, "Failed to evaluate Unit in ShowKeypad\n");
        return;
    }

	static FlowState *g_showKeyboardFlowState;
	static unsigned g_showKeyboardComponentIndex;

	g_showKeyboardFlowState = flowState;
	g_showKeyboardComponentIndex = componentIndex;

	startAsyncExecution(flowState, componentIndex);

	auto onOk = [](float value) {
        auto component = (ShowKeyboardActionComponent *)g_showKeyboardFlowState->flow->components.item(g_showKeyboardComponentIndex);

		auto precisionPropertyValue = component->propertyValues.item(defs_v3::SHOW_KEYPAD_ACTION_COMPONENT_PROPERTY_PRECISION);
        Value precisionValue;
        if (!evalExpression(g_showKeyboardFlowState, g_showKeyboardComponentIndex, precisionPropertyValue->evalInstructions, precisionValue)) {
            throwError(g_showKeyboardFlowState, g_showKeyboardComponentIndex, "Failed to evaluate Precision in ShowKeypad\n");
            return;
        }

        float precision = precisionValue.toFloat();
        
        auto unitPropertyValue = component->propertyValues.item(defs_v3::SHOW_KEYPAD_ACTION_COMPONENT_PROPERTY_UNIT);
        Value unitValue;
        if (!evalExpression(g_showKeyboardFlowState, g_showKeyboardComponentIndex, unitPropertyValue->evalInstructions, unitValue)) {
            throwError(g_showKeyboardFlowState, g_showKeyboardComponentIndex, "Failed to evaluate Unit in ShowKeypad\n");
            return;
        }

		Unit unit = getUnitFromName(unitValue.getString());

        value = roundPrec(value, precision) / getUnitFactor(unit);

		propagateValue(g_showKeyboardFlowState, g_showKeyboardComponentIndex, 0, Value(value, VALUE_TYPE_FLOAT));
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

	Unit unit = getUnitFromName(unitValue.getString());

	showKeypadHook(labelValue, initialValue, minValue, maxValue, unit, onOk, onCancel);
}

} // namespace flow
} // namespace eez
