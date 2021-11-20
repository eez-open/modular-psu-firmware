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

#include <eez/gui/gui.h>

namespace eez {
namespace gui {

#define GRID_FLOW_ROW 1
#define GRID_FLOW_COLUMN 2

struct GridWidget : public Widget {
    AssetsPtr<Widget> itemWidget;
    uint8_t gridFlow; // GRID_FLOW_ROW or GRID_FLOW_COLUMN
};

EnumFunctionType GRID_enum = [](WidgetCursor &widgetCursor, EnumWidgetsCallback callback) {
	auto savedWidget = widgetCursor.widget;

    auto parentWidget = savedWidget;

    auto gridWidget = (const GridWidget *)widgetCursor.widget;

    int startPosition = ytDataGetPosition(widgetCursor, widgetCursor.widget->data);

	const Widget *childWidget = gridWidget->itemWidget.ptr(widgetCursor.assets);
    widgetCursor.widget = childWidget;

	auto savedX = widgetCursor.x;
	auto savedY = widgetCursor.y;

    auto savedCursor = widgetCursor.cursor;

    int xOffset = 0;
    int yOffset = 0;
    int count = eez::gui::count(widgetCursor, parentWidget->data);

    Value oldValue;

    for (int index = startPosition; index < count; ++index) {
        select(widgetCursor, parentWidget->data, index, oldValue);

		widgetCursor.x = savedX + xOffset;
		widgetCursor.y = savedY + yOffset;

		widgetCursor.pushIterator(index);
		enumWidget(widgetCursor, callback);
		widgetCursor.popIterator();

        if (gridWidget->gridFlow == GRID_FLOW_ROW) {
            if (xOffset + childWidget->w < parentWidget->w) {
                xOffset += childWidget->w;
            } else {
                if (yOffset + childWidget->h < parentWidget->h) {
                    yOffset += childWidget->h;
                    xOffset = 0;
                } else {
                    break;
                }
            }
        } else {
            if (yOffset + childWidget->h < parentWidget->h) {
                yOffset += childWidget->h;
            } else {
                if (xOffset + childWidget->w < parentWidget->w) {
                    yOffset = 0;
                    xOffset += childWidget->w;
                } else {
                    break;
                }
            }
        }
    }

	widgetCursor.x = savedX;
	widgetCursor.y = savedY;

    widgetCursor.cursor = savedCursor;

    widgetCursor.widget = savedWidget;

    if (count > 0) {
        deselect(widgetCursor, widgetCursor.widget->data, oldValue);
    }
};

DrawFunctionType GRID_draw = nullptr;

OnTouchFunctionType GRID_onTouch = nullptr;

OnKeyboardFunctionType GRID_onKeyboard = nullptr;

} // namespace gui
} // namespace eez

#endif
