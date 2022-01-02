/*
* EEZ Generic Firmware
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

#pragma once

#include <eez/core/keyboard.h>

namespace eez {
namespace keyboard {

void onKeyDown(uint16_t param);

struct KeyboardInfo {
    uint8_t state;
    
    uint8_t lctrl: 1;
    uint8_t lshift: 1;
    uint8_t lalt: 1;
    uint8_t lgui: 1;

    uint8_t rctrl: 1;
    uint8_t rshift: 1;
    uint8_t ralt: 1;
    uint8_t rgui: 1;

    uint8_t keys[6];
};

extern KeyboardInfo g_keyboardInfo;

} // keyboard
} // eez
