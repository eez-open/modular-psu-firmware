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

#include <eez/gui/gui.h>
#include <eez/gui/widgets/containers/list.h>

namespace eez {
namespace gui {

#define LIST_TYPE_VERTICAL 1
#define LIST_TYPE_HORIZONTAL 2

bool ListWidgetState::updateState() {
    const WidgetCursor &widgetCursor = g_widgetCursor;

    auto widget = (const ListWidget *)widgetCursor.widget;
    startPosition = ytDataGetPosition(widgetCursor, widget->data);
    count = eez::gui::count(widgetCursor, widget->data);
    return false;
}

void ListWidgetState::enumChildren() {
    WidgetCursor &widgetCursor = g_widgetCursor;

    if (count > 0) {
        auto widget = (const ListWidget *)widgetCursor.widget;

		const Widget *childWidget = widget->itemWidget.ptr();
		auto savedWidget = widgetCursor.widget;
		widgetCursor.widget = childWidget;

        auto savedX = widgetCursor.x;
        auto savedY = widgetCursor.y;

        auto savedCursor = widgetCursor.cursor;

        int offset = 0;

        Value oldValue;

        for (int index = startPosition; index < count; ++index) {
            select(widgetCursor, widget->data, index, oldValue);

            if (widget->listType == LIST_TYPE_VERTICAL) {
                if (offset < widget->h) {
					widgetCursor.y = savedY + offset;
					widgetCursor.pushIterator(index);
                    enumWidget();
					widgetCursor.popIterator();
                    offset += childWidget->h + widget->gap;
                } else {
                    break;
                }
            } else {
                if (offset < widget->w) {
					widgetCursor.x = savedX + offset;
					widgetCursor.pushIterator(index);
                    enumWidget();
					widgetCursor.popIterator();
                    offset += childWidget->w + widget->gap;
                } else {
                    break;
                }
            }
        }

        deselect(widgetCursor, widget->data, oldValue);

		widgetCursor.widget = savedWidget;

		widgetCursor.x = savedX;
		widgetCursor.y = savedY;

        widgetCursor.cursor = savedCursor;
	}
}

} // namespace gui
} // namespace eez
