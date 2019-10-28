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

#include <eez/gui/widgets/multiline_text.h>

#include <eez/gui/draw.h>
#include <eez/gui/gui.h>
#include <eez/gui/widget.h>
#include <eez/util.h>

namespace eez {
namespace gui {

#if OPTION_SDRAM
void MultilineTextWidget_fixPointers(Widget *widget) {
    MultilineTextWidget *multilineTextWidget = (MultilineTextWidget *)widget->specific;
    multilineTextWidget->text = (const char *)((uint8_t *)g_document + (uint32_t)multilineTextWidget->text);
}
#endif

void MultilineTextWidget_draw(const WidgetCursor &widgetCursor) {
    const Widget *widget = widgetCursor.widget;

    widgetCursor.currentState->size = sizeof(WidgetState);
    widgetCursor.currentState->data =
        widget->data ? data::get(widgetCursor.cursor, widget->data) : 0;

    bool refresh =
        !widgetCursor.previousState ||
        widgetCursor.previousState->flags.active != widgetCursor.currentState->flags.active ||
        widgetCursor.previousState->data != widgetCursor.currentState->data;

    if (refresh) {
        const Style* style = getStyle(widget->style);

        const MultilineTextWidget *display_string_widget = GET_WIDGET_PROPERTY(widget, specific, const MultilineTextWidget *);

        if (widget->data) {
            if (widgetCursor.currentState->data.isString()) {
                drawMultilineText(widgetCursor.currentState->data.getString(), widgetCursor.x,
                    widgetCursor.y, (int)widget->w, (int)widget->h, style, nullptr,
                    widgetCursor.currentState->flags.active,
                    display_string_widget->firstLineIndent, display_string_widget->hangingIndent);
            } else {
                char text[64];
                widgetCursor.currentState->data.toText(text, sizeof(text));
                drawMultilineText(text, widgetCursor.x, widgetCursor.y, (int)widget->w,
                    (int)widget->h, style, nullptr,
                    widgetCursor.currentState->flags.active,
                    display_string_widget->firstLineIndent, display_string_widget->hangingIndent);
            }
        } else {
            drawMultilineText(
                GET_WIDGET_PROPERTY(display_string_widget, text, const char *), 
                widgetCursor.x, widgetCursor.y, (int)widget->w, (int)widget->h,
                style, nullptr, widgetCursor.currentState->flags.active,
                display_string_widget->firstLineIndent, display_string_widget->hangingIndent);
        }
    }
}

} // namespace gui
} // namespace eez

#endif