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

#include <eez/gui/touch.h>

#include <eez/gui/app_context.h>
#include <eez/gui/touch_filter.h>
#include <eez/modules/mcu/touch.h>

////////////////////////////////////////////////////////////////////////////////

namespace eez {
namespace gui {
namespace touch {

EventType g_eventType = EVENT_TYPE_TOUCH_NONE;
int g_x = -1;
int g_y = -1;

////////////////////////////////////////////////////////////////////////////////

void tick() {
    bool touchIsPressed = false;

    mcu::touch::read(touchIsPressed, g_x, g_y);

#ifndef EEZ_PLATFORM_SIMULATOR
    //touchIsPressed = filter(touchIsPressed, g_x, g_y);
    if (touchIsPressed) {
        transform(g_x, g_y);
    }
#endif

    if (touchIsPressed) {
        if (g_eventType == EVENT_TYPE_TOUCH_NONE || g_eventType == EVENT_TYPE_TOUCH_UP) {
            g_eventType = EVENT_TYPE_TOUCH_DOWN;
        } else {
            if (g_eventType == EVENT_TYPE_TOUCH_DOWN) {
                g_eventType = EVENT_TYPE_TOUCH_MOVE;
            }
        }
    } else {
        if (g_eventType == EVENT_TYPE_TOUCH_DOWN || g_eventType == EVENT_TYPE_TOUCH_MOVE) {
            g_eventType = EVENT_TYPE_TOUCH_UP;
        } else if (g_eventType == EVENT_TYPE_TOUCH_UP) {
            g_eventType = EVENT_TYPE_TOUCH_NONE;
        }
    }
}

int getX() {
    return g_x;
}

int getY() {
    return g_y;
}

} // namespace touch
} // namespace gui
} // namespace eez
