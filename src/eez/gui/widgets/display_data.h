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
namespace gui {

enum {
    DISPLAY_OPTION_ALL = 0,
    DISPLAY_OPTION_INTEGER = 1,
    DISPLAY_OPTION_FRACTION = 2,
    DISPLAY_OPTION_FRACTION_AND_UNIT = 3,
    DISPLAY_OPTION_UNIT = 4,
    DISPLAY_OPTION_INTEGER_AND_FRACTION = 5
};

struct DisplayDataWidget {
    uint16_t focusStyle;
    uint8_t displayOption;
};

struct DisplayDataState {
    WidgetState genericState;
    uint16_t backgroundColor;
};

void DisplayDataWidget_draw(const WidgetCursor &widgetCursor);

} // namespace gui
} // namespace eez