/*
 * EEZ Modular Firmware
 * Copyright (C) 2020-present, Envox d.o.o.
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

#include <math.h>

#include <eez/alloc.h>
#include <eez/util.h>

#include <eez/gui/gui.h>
#include <eez/gui/data.h>

#include <eez/gui/widgets/input.h>

namespace eez {
namespace gui {

static const size_t MAX_TEXT_LEN = 128;

void getInputWidgetParams(const gui::WidgetCursor &widgetCursor, Value &min, Value &max, Value &precision, Unit &unit) {
	auto widget = (const InputWidget*)widgetCursor.widget;

	auto unitValue = get(widgetCursor, widget->unit);
	unit = getUnitFromName(unitValue.toString(0x5049bd52).getString());

	auto minValue = get(widgetCursor, widget->min);
	auto maxValue = get(widgetCursor, widget->max);
	auto precisionValue = get(widgetCursor, widget->precision);

	float base10 = getUnitBase10(unit);
	auto baseUnit = getBaseUnit(unit);

	min = Value(minValue.toFloat() * base10, baseUnit);
	max = Value(maxValue.toFloat() * base10, baseUnit);
	precision = Value(precisionValue.toFloat() * base10, baseUnit);
}

Value getInputWidgetMin(const gui::WidgetCursor &widgetCursor) {
	auto widget = (const InputWidget*)widgetCursor.widget;

	auto flowState = widgetCursor.flowState;
	if (flowState) {
		auto inputWidgetExecutionState = (InputWidgetExecutionState *)flowState->componenentExecutionStates[widget->componentIndex];
		if (inputWidgetExecutionState) {
			return inputWidgetExecutionState->min;
		}
	}

	auto unitValue = get(widgetCursor, widget->unit);
	auto unit = getUnitFromName(unitValue.toString(0x5049bd52).getString());
	auto baseUnit = getBaseUnit(unit);

	auto value = get(widgetCursor, widget->min);
	float base10 = getUnitBase10(unit);
	return Value(value.toFloat() * base10, baseUnit);
}

Value getInputWidgetMax(const gui::WidgetCursor &widgetCursor) {
	auto widget = (const InputWidget*)widgetCursor.widget;

	auto flowState = widgetCursor.flowState;
	if (flowState) {
		auto inputWidgetExecutionState = (InputWidgetExecutionState *)flowState->componenentExecutionStates[widget->componentIndex];
		if (inputWidgetExecutionState) {
			return inputWidgetExecutionState->max;
		}
	}

	auto unitValue = get(widgetCursor, widget->unit);
	auto unit = getUnitFromName(unitValue.toString(0x5049bd52).getString());
	auto baseUnit = getBaseUnit(unit);

	auto value = get(widgetCursor, widget->max);
	float base10 = getUnitBase10(unit);
	return Value(value.toFloat() * base10, baseUnit);
}

Value getInputWidgetPrecision(const gui::WidgetCursor &widgetCursor) {
	auto widget = (const InputWidget*)widgetCursor.widget;

	auto flowState = widgetCursor.flowState;
	if (flowState) {
		auto inputWidgetExecutionState = (InputWidgetExecutionState *)flowState->componenentExecutionStates[widget->componentIndex];
		if (inputWidgetExecutionState) {
			return inputWidgetExecutionState->precision;
		}
	}

	auto unitValue = get(widgetCursor, widget->unit);
	auto unit = getUnitFromName(unitValue.toString(0x5049bd52).getString());
	auto baseUnit = getBaseUnit(unit);

	auto value = get(widgetCursor, widget->precision);
	float base10 = getUnitBase10(unit);
	return Value(value.toFloat() * base10, baseUnit);
}

Unit getInputWidgetUnit(const gui::WidgetCursor &widgetCursor) {
	auto widget = (const InputWidget*)widgetCursor.widget;

	auto flowState = widgetCursor.flowState;
	if (flowState) {
		auto inputWidgetExecutionState = (InputWidgetExecutionState *)flowState->componenentExecutionStates[widget->componentIndex];
		if (inputWidgetExecutionState) {
			return inputWidgetExecutionState->unit;
		}
	}

	auto unitValue = get(widgetCursor, widget->unit);
	if (unitValue.isString()) {
		return getUnitFromName(unitValue.toString(0x5049bd52).getString());
	} else {
		return UNIT_NONE;
	}
}

Value getInputWidgetData(const gui::WidgetCursor &widgetCursor, const Value &dataValue) {
	auto widget = (const InputWidget*)widgetCursor.widget;

	if (widget->flags & INPUT_WIDGET_TYPE_NUMBER) {
		auto unit = getInputWidgetUnit(widgetCursor);
		float base10 = getUnitBase10(unit);
		auto baseUnit = getBaseUnit(unit);
    	return Value(dataValue.toFloat() * base10, baseUnit);
	} else {
		return get(widgetCursor, widget->data);
	}
}

bool InputWidgetState::updateState() {
    const WidgetCursor &widgetCursor = g_widgetCursor;

	bool hasPreviousState = widgetCursor.hasPreviousState;
	auto widget = (const InputWidget*)widgetCursor.widget;
	const Style *style = getStyle(overrideStyleHook(widgetCursor, widget->style));

	WIDGET_STATE(flags.active, g_isActiveWidget);
	WIDGET_STATE(flags.focused, isFocusWidget(widgetCursor));
	WIDGET_STATE(flags.blinking, g_isBlinkTime && styleIsBlink(style));
	WIDGET_STATE(data, get(widgetCursor, widget->data));

	return !hasPreviousState;
}

void InputWidgetState::render() {
    const WidgetCursor &widgetCursor = g_widgetCursor;

	auto widget = (const InputWidget*)widgetCursor.widget;

	if (widgetCursor.flowState) {
		auto inputWidgetExecutionState = (InputWidgetExecutionState *)widgetCursor.flowState->componenentExecutionStates[widget->componentIndex];
		if (!inputWidgetExecutionState) {
			inputWidgetExecutionState = ObjectAllocator<InputWidgetExecutionState>::allocate(0xa570ccad);
			widgetCursor.flowState->componenentExecutionStates[widget->componentIndex] = inputWidgetExecutionState;
		}

		getInputWidgetParams(
			widgetCursor,
			inputWidgetExecutionState->min,
			inputWidgetExecutionState->max,
			inputWidgetExecutionState->precision,
			inputWidgetExecutionState->unit
		);
	}

	const Style *style = getStyle(overrideStyleHook(widgetCursor, widget->style));

	uint16_t overrideColor                 = flags.focused ? style->focusColor           : overrideStyleColorHook(widgetCursor, style);
	uint16_t overrideBackgroundColor       = flags.focused ? style->focusBackgroundColor : style->backgroundColor;
	uint16_t overrideActiveColor           = flags.focused ? style->focusBackgroundColor : overrideActiveStyleColorHook(widgetCursor, style);
	uint16_t overrideActiveBackgroundColor = flags.focused ? style->focusColor           : style->activeBackgroundColor;

	char text[MAX_TEXT_LEN + 1];
	data.toText(text, sizeof(text));

	drawText(
		text, -1,
		widgetCursor.x, widgetCursor.y, (int)widget->w, (int)widget->h,
		style,
		flags.active, flags.blinking, false,
		&overrideColor, &overrideBackgroundColor, &overrideActiveColor, &overrideActiveBackgroundColor,
		true // useSmallerFontIfDoesNotFit
	);
}

} // namespace gui
} // namespace eez
