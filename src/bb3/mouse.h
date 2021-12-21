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

#if defined(EEZ_PLATFORM_STM32)
#include <usbh_hid.h>
#endif

#include <eez/mouse.h>

namespace eez {
namespace mouse {

void onPageChanged();

void onMouseXMove(int x);
void onMouseYMove(int y);
void onMouseButtonDown(int button);
void onMouseButtonUp(int button);
void onMouseDisconnected();

#if defined(EEZ_PLATFORM_STM32)
void onMouseEvent(USBH_HandleTypeDef *phost);
#endif

struct MouseInfo {
    uint8_t dx;
    uint8_t dy;
    int x;
    int y;
    uint8_t button1: 1;
    uint8_t button2: 1;
    uint8_t button3: 1;
};

extern MouseInfo g_mouseInfo;

} // mouse
} // eez
