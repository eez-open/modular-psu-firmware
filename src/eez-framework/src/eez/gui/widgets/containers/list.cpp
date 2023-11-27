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

#include <eez/conf-internal.h>

#if EEZ_OPTION_GUI

#include <eez/gui/gui.h>
#include <eez/gui/widgets/containers/list.h>

namespace eez {
namespace gui {

#define LIST_TYPE_VERTICAL 1
#define LIST_TYPE_HORIZONTAL 2

bool ListWidgetState::updateState() {
    WIDGET_STATE_START(ListWidget);

    auto newStartPosition = ytDataGetPosition(widgetCursor, widget->data);
    if ((int)newStartPosition != startPosition) {
        startPosition = newStartPosition;
        hasPreviousState = false;
    }
    auto newCount = eez::gui::count(widgetCursor, widget->data);
    if (newCount != count) {
        count = newCount;
        hasPreviousState = false;
    }

    WIDGET_STATE_END()
}

void ListWidgetState::enumChildren() {
    WidgetCursor &widgetCursor = g_widgetCursor;

    auto widget = (const ListWidget *)widgetCursor.widget;
    const Style *style = getStyle(widget->style);

    auto childWidget = static_cast<const Widget *>(widget->itemWidget);
    widgetCursor.widget = childWidget;

    auto savedX = widgetCursor.x;
    auto savedY = widgetCursor.y;

    auto savedCursor = widgetCursor.cursor;

    int offset = 0;

    Value oldValue;

    auto width = widgetCursor.w;
    auto height = widgetCursor.h;

    for (int index = startPosition; ; ++index) {
        if (index >= 0 && index < count) {
            select(widgetCursor, widget->data, index, oldValue);

            widgetCursor.w = childWidget->width;
            widgetCursor.h = childWidget->height;

            if (widget->listType == LIST_TYPE_VERTICAL) {
                if (offset < height) {
                    widgetCursor.y = savedY + offset;
                    widgetCursor.pushIterator(index);
                    enumWidget();
                    widgetCursor.popIterator();
                    offset += childWidget->height + widget->gap;
                } else {
                    break;
                }
            } else {
                if (offset < width) {
                    widgetCursor.x = savedX + offset;
                    widgetCursor.pushIterator(index);
                    enumWidget();
                    widgetCursor.popIterator();
                    offset += childWidget->width + widget->gap;
                } else {
                    break;
                }
            }
        } else {
            widgetCursor.w = childWidget->width;
            widgetCursor.h = childWidget->height;

            if (widget->listType == LIST_TYPE_VERTICAL) {
                if (offset < height) {
                    widgetCursor.y = savedY + offset;
                    drawRectangle(widgetCursor.x, widgetCursor.y, widgetCursor.w, MIN(widgetCursor.h, height - offset), style, false, false, true);
                    offset += childWidget->height + widget->gap;
                } else {
                    break;
                }
            } else {
                if (offset < width) {
                    widgetCursor.x = savedX + offset;
                    drawRectangle(widgetCursor.x, widgetCursor.y, MIN(widgetCursor.w, width - offset), widgetCursor.h, style, false, false, true);
                    offset += childWidget->width + widget->gap;
                } else {
                    break;
                }
            }
        }
    }

    deselect(widgetCursor, widget->data, oldValue);

    widgetCursor.widget = widget;

    widgetCursor.x = savedX;
    widgetCursor.y = savedY;

    widgetCursor.cursor = savedCursor;
}

} // namespace gui
} // namespace eez

#endif // EEZ_OPTION_GUI
