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

#if OPTION_DISPLAY

#include <math.h>

#include <eez/util.h>

#include <eez/flow/private.h>

#include <eez/gui/gui.h>
#include <eez/gui/data.h>

#include <eez/gui/widgets/input.h>

namespace eez {
namespace gui {

static const size_t MAX_TEXT_LEN = 128;

struct InputWidgetState : public WidgetState {
	Value unit;
};

EnumFunctionType INPUT_enum = nullptr;

DrawFunctionType INPUT_draw = [](const WidgetCursor &widgetCursor) {
	auto widget = (const InputWidget*)widgetCursor.widget;

    InputWidgetState *currentState = (InputWidgetState *)widgetCursor.currentState;
    InputWidgetState *previousState = (InputWidgetState *)widgetCursor.previousState;

    widgetCursor.currentState->size = sizeof(InputWidgetState);

	const Style *style = getStyle(overrideStyleHook(widgetCursor, widget->style));

	widgetCursor.currentState->flags.focused = isFocusWidget(widgetCursor);
    widgetCursor.currentState->flags.blinking = g_isBlinkTime && styleIsBlink(style);
	widgetCursor.currentState->data.clear();
    widgetCursor.currentState->data = get(widgetCursor, widget->data);
	
    bool refresh =
		!widgetCursor.previousState ||
        widgetCursor.previousState->flags.focused != widgetCursor.currentState->flags.focused ||
        widgetCursor.previousState->flags.active != widgetCursor.currentState->flags.active ||
        widgetCursor.previousState->flags.blinking != widgetCursor.currentState->flags.blinking ||
		widgetCursor.previousState->data != widgetCursor.currentState->data;

	currentState->unit.clear();
	if (widget->flags & INPUT_WIDGET_TYPE_NUMBER) {
		currentState->unit = get(widgetCursor, widget->unit);
		if (!refresh) {
			refresh |= 
				previousState->unit != currentState->unit;
		}
	}

    if (refresh) {
        uint16_t overrideColor = widgetCursor.currentState->flags.focused ? style->focus_color : overrideStyleColorHook(widgetCursor, style);
        uint16_t overrideBackgroundColor = widgetCursor.currentState->flags.focused ? style->focus_background_color : style->background_color;
        uint16_t overrideActiveColor =  widgetCursor.currentState->flags.focused ? style->focus_background_color : overrideActiveStyleColorHook(widgetCursor, style);
        uint16_t overrideActiveBackgroundColor = widgetCursor.currentState->flags.focused ? style->focus_color : style->active_background_color;

		char text[MAX_TEXT_LEN + 1];
		widgetCursor.currentState->data.toText(text, sizeof(text));

		Value unit = currentState->unit.toString(0x5f8feae3);
		if (unit.isString()) {
			stringAppendString(text, sizeof(text), " ");
			stringAppendString(text, sizeof(text), unit.getString());
		}

		drawText(text, -1, widgetCursor.x, widgetCursor.y, (int)widget->w, (int)widget->h,
			style, widgetCursor.currentState->flags.active,
			widgetCursor.currentState->flags.blinking,
			false, &overrideColor, &overrideBackgroundColor, &overrideActiveColor, &overrideActiveBackgroundColor,
			false);
    }

	widgetCursor.currentState->data.freeRef();
	currentState->unit.freeRef();
};

OnTouchFunctionType INPUT_onTouch = nullptr;

OnKeyboardFunctionType INPUT_onKeyboard = nullptr;

} // namespace gui
} // namespace eez

#endif
