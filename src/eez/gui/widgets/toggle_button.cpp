/*
 * EEZ Modular Firmware
 * Copyright (C) 2015-present, Envox d.o.o.
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

#include <eez/util.h>

#include <eez/gui/gui.h>

namespace eez {
namespace gui {

struct ToggleButtonWidget {
    AssetsPtr<const char> text1;
    AssetsPtr<const char> text2;
};

FixPointersFunctionType TOGGLE_BUTTON_fixPointers = [](Widget *widget, Assets *assets) {
    ToggleButtonWidget *toggleButtonWidget = (ToggleButtonWidget *)widget->specific;
    toggleButtonWidget->text1 = (const char *)((uint8_t *)(void *)assets->document + (uint32_t)toggleButtonWidget->text1);
    toggleButtonWidget->text2 = (const char *)((uint8_t *)(void *)assets->document + (uint32_t)toggleButtonWidget->text2);
};

EnumFunctionType TOGGLE_BUTTON_enum = nullptr;

DrawFunctionType TOGGLE_BUTTON_draw = [](const WidgetCursor &widgetCursor) {
    const Widget *widget = widgetCursor.widget;

    widgetCursor.currentState->size = sizeof(WidgetState);
    widgetCursor.currentState->flags.enabled =
        get(widgetCursor.cursor, widget->data).getInt() ? 1 : 0;

    bool refresh =
        !widgetCursor.previousState ||
        widgetCursor.previousState->flags.active != widgetCursor.currentState->flags.active ||
        widgetCursor.previousState->flags.enabled != widgetCursor.currentState->flags.enabled;

    if (refresh) {
        const ToggleButtonWidget *toggle_button_widget = GET_WIDGET_PROPERTY(widget, specific, const ToggleButtonWidget *);
        const Style* style = getStyle(widget->style);
        drawText(
            widgetCursor.currentState->flags.enabled ? 
                GET_WIDGET_PROPERTY(toggle_button_widget, text2, const char *) : 
                GET_WIDGET_PROPERTY(toggle_button_widget, text1, const char *),
            -1,
			widgetCursor.x, widgetCursor.y, (int)widget->w, (int)widget->h, style,
            widgetCursor.currentState->flags.active, false, false, nullptr, nullptr, nullptr, nullptr);
    }
};

OnTouchFunctionType TOGGLE_BUTTON_onTouch = nullptr;

OnKeyboardFunctionType TOGGLE_BUTTON_onKeyboard = nullptr;

} // namespace gui
} // namespace eez

#endif
