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

#include <eez/gui/widgets/text.h>

#include <eez/gui/draw.h>
#include <eez/gui/gui.h>
#include <eez/util.h>

namespace eez {
namespace gui {

#if OPTION_SDRAM
void TextWidget_fixPointers(Widget *widget) {
    TextWidget *textWidget = (TextWidget *)widget->specific;
    textWidget->text = (const char *)((uint8_t *)g_document + (uint32_t)textWidget->text);
}
#endif

void TextWidget_draw(const WidgetCursor &widgetCursor) {
    const Widget *widget = widgetCursor.widget;
    const Style* style = getWidgetStyle(widget);

    widgetCursor.currentState->size = sizeof(WidgetState);
    widgetCursor.currentState->flags.blinking = g_isBlinkTime && styleIsBlink(style);
    widgetCursor.currentState->data =
        widget->data ? data::get(widgetCursor.cursor, widget->data) : 0;

    bool refresh =
        !widgetCursor.previousState ||
        widgetCursor.previousState->flags.active != widgetCursor.currentState->flags.active ||
        widgetCursor.previousState->flags.blinking != widgetCursor.currentState->flags.blinking ||
        widgetCursor.previousState->data != widgetCursor.currentState->data ||
        widgetCursor.previousState->backgroundColor != widgetCursor.currentState->backgroundColor;

    if (refresh) {
        const Style *activeStyle = getStyle(widget->activeStyle);
        const TextWidget *display_string_widget = GET_WIDGET_PROPERTY(widget, specific, const TextWidget *);

        if (widget->data) {
            if (widgetCursor.currentState->data.isString()) {
                drawText(widgetCursor.currentState->data.getString(), -1, widgetCursor.x,
                         widgetCursor.y, (int)widget->w, (int)widget->h, style, activeStyle,
                         widgetCursor.currentState->flags.active,
                         widgetCursor.currentState->flags.blinking,
                         display_string_widget->flags.ignoreLuminosity, nullptr);
            } else {
                char text[64];
                widgetCursor.currentState->data.toText(text, sizeof(text));
                drawText(text, -1, widgetCursor.x, widgetCursor.y, (int)widget->w, (int)widget->h,
                         style, activeStyle, widgetCursor.currentState->flags.active,
                         widgetCursor.currentState->flags.blinking,
                         display_string_widget->flags.ignoreLuminosity, nullptr);
            }
        } else {
            drawText(GET_WIDGET_PROPERTY(display_string_widget, text, const char *), -1, widgetCursor.x, widgetCursor.y, (int)widget->w, (int)widget->h,
                     style, activeStyle, widgetCursor.currentState->flags.active,
                     widgetCursor.currentState->flags.blinking,
                     display_string_widget->flags.ignoreLuminosity, nullptr);
        }
    }
}

} // namespace gui
} // namespace eez

#endif