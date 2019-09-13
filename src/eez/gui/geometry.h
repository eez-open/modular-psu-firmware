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

#include <stdint.h>

namespace eez {
namespace gui {

struct Rect {
    int16_t x;
    int16_t y;
    int16_t w;
    int16_t h;
};

inline bool operator != (const Rect& r1, const Rect& r2) {
    return r1.x != r2.x || r1.y != r2.y || r1.w != r2.w || r1.h != r2.h;
}

inline bool operator == (const Rect& r1, const Rect& r2) {
    return !(r1 != r2);
}

} // namespace gui
} // namespace eez
