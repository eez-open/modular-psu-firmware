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

#pragma once

#include <eez/gui/widget.h>

namespace eez {

enum {
    YT_GRAPH_UPDATE_METHOD_SCROLL,
    YT_GRAPH_UPDATE_METHOD_SCAN_LINE,
    YT_GRAPH_UPDATE_METHOD_STATIC
};

namespace gui {

struct YTGraphWidgetState {
    WidgetState genericState;
    uint8_t iChannel;
    uint32_t historyValuePosition;
    uint8_t ytGraphUpdateMethod;
    uint32_t cursorPosition;
};

void YTGraphWidget_draw(const WidgetCursor &widgetCursor);
void YTGraphWidget_onTouch(const WidgetCursor &widgetCursor, Event &touchEvent);

} // namespace gui
} // namespace eez