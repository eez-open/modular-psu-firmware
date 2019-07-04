/*
 * EEZ Generic Firmware
 * Copyright (C) 2018-present, Envox d.o.o.
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

#include <eez/gui/widgets/display_data.h>

#include <eez/gui/app_context.h>
#include <eez/gui/draw.h>
#include <eez/gui/gui.h>
#include <eez/gui/widget.h>
#include <eez/util.h>

namespace eez {
namespace gui {

void DisplayDataWidget_draw(const WidgetCursor &widgetCursor) {
    const Widget *widget = widgetCursor.widget;
    const DisplayDataWidget *display_data_widget = (const DisplayDataWidget *)widget->specific;

    widgetCursor.currentState->size = sizeof(WidgetState);
    widgetCursor.currentState->flags.focused = g_appContext->isFocusWidget(widgetCursor);

	const Style *style = getStyle(widgetCursor.currentState->flags.focused ? display_data_widget->focusStyle : widget->style);

    widgetCursor.currentState->flags.blinking =
        isBlinkTime() && data::isBlinking(widgetCursor.cursor, widget->data);
    widgetCursor.currentState->data = data::get(widgetCursor.cursor, widget->data);
    widgetCursor.currentState->backgroundColor =
        g_appContext->getWidgetBackgroundColor(widgetCursor, style);

    bool refresh =
        !widgetCursor.previousState ||
        widgetCursor.previousState->flags.focused != widgetCursor.currentState->flags.focused ||
        widgetCursor.previousState->flags.active != widgetCursor.currentState->flags.active ||
        widgetCursor.previousState->flags.blinking != widgetCursor.currentState->flags.blinking ||
        widgetCursor.previousState->data != widgetCursor.currentState->data ||
        widgetCursor.previousState->backgroundColor != widgetCursor.currentState->backgroundColor;

    if (refresh) {
        char text[64];
        widgetCursor.currentState->data.toText(text, sizeof(text));

        drawText(text, -1, widgetCursor.x, widgetCursor.y, (int)widget->w, (int)widget->h, style,
                 nullptr, widgetCursor.currentState->flags.active,
                 widgetCursor.currentState->flags.blinking, false,
                 &widgetCursor.currentState->backgroundColor);

        g_painted = true;
    }
}

} // namespace gui
} // namespace eez
