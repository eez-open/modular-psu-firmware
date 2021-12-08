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
#include <eez/gui/widgets/rectangle.h>

namespace eez {
namespace gui {

bool RectangleWidgetState::updateState() {
    const WidgetCursor &widgetCursor = g_widgetCursor;

    bool hasPreviousState = widgetCursor.hasPreviousState;
    WIDGET_STATE(flags.active, g_isActiveWidget);

    return !hasPreviousState;
}

void RectangleWidgetState::render() {
    const WidgetCursor &widgetCursor = g_widgetCursor;

    auto widget = (const RectangleWidget *)widgetCursor.widget;
    const Style* style = getStyle(widget->style);
    drawRectangle(
        widgetCursor.x, widgetCursor.y, (int)widget->w, (int)widget->h,
        style, 
        flags.active, 
        widget->flags.ignoreLuminosity
    );
}

} // namespace gui
} // namespace eez
