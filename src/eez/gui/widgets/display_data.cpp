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

#include <string.h>

#include <eez/gui/widgets/display_data.h>

#include <eez/gui/app_context.h>
#include <eez/gui/draw.h>
#include <eez/gui/gui.h>
#include <eez/gui/widget.h>
#include <eez/util.h>

namespace eez {
namespace gui {

int findStartOfFraction(char *text) {
    int i;
    for (i = 0; text[i] && (text[i] == '-' || (text[i] >= '0' && text[i] <= '9')); i++);
    return i;
}

int findStartOfUnit(char *text, int i) {
    for (i = 0; text[i] && (text[i] == '-' || (text[i] >= '0' && text[i] <= '9') || text[i] == '.'); i++);
    return i;
}

void DisplayDataWidget_draw(const WidgetCursor &widgetCursor) {
    const Widget *widget = widgetCursor.widget;
    const DisplayDataWidget *display_data_widget = (const DisplayDataWidget *)widget->specific;

    widgetCursor.currentState->size = sizeof(WidgetState);
    widgetCursor.currentState->flags.focused = g_appContext->isFocusWidget(widgetCursor);

	const Style *style = getStyle(widgetCursor.currentState->flags.focused ? display_data_widget->focusStyle : widget->style);
    const Style *activeStyle = getStyle(widgetCursor.currentState->flags.focused ? display_data_widget->focusStyle : widget->activeStyle);

    widgetCursor.currentState->flags.blinking = g_isBlinkTime && data::isBlinking(widgetCursor.cursor, widget->data);
    widgetCursor.currentState->data = data::get(widgetCursor.cursor, widget->data);
    widgetCursor.currentState->backgroundColor = g_appContext->getWidgetBackgroundColor(widgetCursor, style);

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

        char *start = text;

        if (display_data_widget->displayOption == DISPLAY_OPTION_INTEGER) {
            int i = findStartOfFraction(text);
            text[i] = 0;
        } else if (display_data_widget->displayOption == DISPLAY_OPTION_FRACTION) {
            int i = findStartOfFraction(text);
            start = text + i;
        } else if (display_data_widget->displayOption == DISPLAY_OPTION_FRACTION_AND_UNIT) {
            int i = findStartOfFraction(text);
            int k = findStartOfUnit(text, i);
            if (i < k) {
                start = text + i;
                text[k] = 0;
            } else {
                strcpy(text, ".00");
            }
        } else if (display_data_widget->displayOption == DISPLAY_OPTION_UNIT) {
            int i = findStartOfUnit(text, 0);
            start = text + i;
        }

        drawText(start, -1, widgetCursor.x, widgetCursor.y, (int)widget->w, (int)widget->h,
                 style, activeStyle, widgetCursor.currentState->flags.active,
                 widgetCursor.currentState->flags.blinking, false,
                 &widgetCursor.currentState->backgroundColor);
    }
}

} // namespace gui
} // namespace eez
