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
#include <eez/gui/widgets/containers/grid.h>

namespace eez {
namespace gui {

#define GRID_FLOW_ROW 1
#define GRID_FLOW_COLUMN 2

bool GridWidgetState::updateState() {
    const WidgetCursor &widgetCursor = g_widgetCursor;

    auto widget = (const GridWidget *)widgetCursor.widget;
    startPosition = ytDataGetPosition(widgetCursor, widget->data);
    count = eez::gui::count(widgetCursor, widget->data);
    return false;
}

void GridWidgetState::enumChildren() {
    WidgetCursor &widgetCursor = g_widgetCursor;

    if (count > 0) {
        auto widget = (const GridWidget *)widgetCursor.widget;

        const Widget *childWidget = static_cast<const Widget *>(widget->itemWidget);
		auto savedWidget = widgetCursor.widget;
		widgetCursor.widget = childWidget;

        auto savedX = widgetCursor.x;
        auto savedY = widgetCursor.y;

        auto savedCursor = widgetCursor.cursor;

        int xOffset = 0;
        int yOffset = 0;

        Value oldValue;

        auto width = widgetCursor.w;
        auto height = widgetCursor.h;

        for (int index = startPosition; index < count; ++index) {
            select(widgetCursor, widget->data, index, oldValue);

			widgetCursor.x = savedX + xOffset;
			widgetCursor.y = savedY + yOffset;
            widgetCursor.w = childWidget->width;
            widgetCursor.h = childWidget->height;

			widgetCursor.pushIterator(index);
            enumWidget();
			widgetCursor.popIterator();

            if (widget->gridFlow == GRID_FLOW_ROW) {
                if (xOffset + childWidget->width < width) {
                    xOffset += childWidget->width;
                } else {
                    if (yOffset + childWidget->height < height) {
                        yOffset += childWidget->height;
                        xOffset = 0;
                    } else {
                        break;
                    }
                }
            } else {
                if (yOffset + childWidget->height < height) {
                    yOffset += childWidget->height;
                } else {
                    if (xOffset + childWidget->width < width) {
                        yOffset = 0;
                        xOffset += childWidget->width;
                    } else {
                        break;
                    }
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
