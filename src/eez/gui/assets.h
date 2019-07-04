/*
 * EEZ Generic Firmware
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

#include <eez/gui/document.h>

namespace eez {
namespace gui {

extern Document *g_document;
extern StyleList *g_styles;
extern uint8_t *g_fontsData;
extern uint8_t *g_bitmapsData;
extern Colors *g_colorsData;

void decompressAssets();

inline const Style *getStyle(int styleID) {
    return g_styles->first + styleID - 1;
}

const Style *getWidgetStyle(const Widget *widget);

////////////////////////////////////////////////////////////////////////////////

} // namespace gui
} // namespace eez
