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

#include <eez/util.h>

#include <eez/gui/gui.h>
#include <eez/gui/widgets/multiline_text.h>

namespace eez {
namespace gui {

bool MultilineTextWidgetState::updateState() {
    const WidgetCursor &widgetCursor = g_widgetCursor;

    bool hasPreviousState = widgetCursor.hasPreviousState;
    auto widget = (const MultilineTextWidget *)widgetCursor.widget;

    WIDGET_STATE(flags.active, g_isActiveWidget);
    WIDGET_STATE(data, widget->data ? get(widgetCursor, widget->data) : 0);

    return !hasPreviousState;
}

void MultilineTextWidgetState::render() {
    const WidgetCursor &widgetCursor = g_widgetCursor;

    auto widget = (const MultilineTextWidget *)widgetCursor.widget;
    const Style* style = getStyle(widget->style);

    if (widget->data) {
        if (data.isString()) {
            drawMultilineText(data.getString(), widgetCursor.x,
                widgetCursor.y, (int)widget->w, (int)widget->h, style,
                flags.active,
                widget->firstLineIndent, widget->hangingIndent);
        } else {
            char text[64];
            data.toText(text, sizeof(text));
            drawMultilineText(text, widgetCursor.x, widgetCursor.y, (int)widget->w,
                (int)widget->h, style,
                flags.active,
                widget->firstLineIndent, widget->hangingIndent);
        }
    } else if (widget->text) {
        drawMultilineText(
            widget->text.ptr(widgetCursor.assets), 
            widgetCursor.x, widgetCursor.y, (int)widget->w, (int)widget->h,
            style, flags.active,
            widget->firstLineIndent, widget->hangingIndent);
    }
};

} // namespace gui
} // namespace eez
