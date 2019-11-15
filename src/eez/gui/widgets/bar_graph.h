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

#define BAR_GRAPH_ORIENTATION_LEFT_RIGHT 1
#define BAR_GRAPH_ORIENTATION_RIGHT_LEFT 2
#define BAR_GRAPH_ORIENTATION_TOP_BOTTOM 3
#define BAR_GRAPH_ORIENTATION_BOTTOM_TOP 4

namespace eez {
namespace gui {

struct BarGraphWidget {
    uint8_t orientation; // BAR_GRAPH_ORIENTATION_...
    uint16_t textStyle;
    uint16_t line1Data;
    uint16_t line1Style;
    uint16_t line2Data;
    uint16_t line2Style;
};

struct BarGraphWidgetState {
    WidgetState genericState;
    uint16_t color;
    uint16_t backgroundColor;
    uint16_t activeColor;
    uint16_t activeBackgroundColor;
    data::Value line1Data;
    data::Value line2Data;
};

void BarGraphWidget_draw(const WidgetCursor &widgetCursor);

} // namespace gui
} // namespace eez