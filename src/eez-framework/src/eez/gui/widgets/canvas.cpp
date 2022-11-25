/*
 * EEZ Modular Firmware
 * Copyright (C) 2020-present, Envox d.o.o.
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

#include <eez/core/util.h>

#include <eez/gui/gui.h>
#include <eez/gui/widgets/canvas.h>

namespace eez {
namespace gui {

bool CanvasWidgetState::updateState() {
    WIDGET_STATE_START(CanvasWidget);

    WIDGET_STATE(data, getCanvasRefreshState(widgetCursor, widget->data));

    WIDGET_STATE_END()
}

void CanvasWidgetState::render() {
    const WidgetCursor &widgetCursor = g_widgetCursor;

    auto widget = (const CanvasWidget *)widgetCursor.widget;

    auto drawFunction = getCanvasDrawFunction(widgetCursor, widget->data);
    if (drawFunction) {
        drawFunction(widgetCursor);
    }
}

} // namespace gui
} // namespace eez

#endif // EEZ_OPTION_GUI
