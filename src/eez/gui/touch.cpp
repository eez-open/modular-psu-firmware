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

#include <eez/gui/gui.h>
#include <eez/gui/touch_filter.h>

#include <eez/modules/mcu/touch.h>

////////////////////////////////////////////////////////////////////////////////

namespace eez {
namespace gui {
namespace touch {

EventType g_eventType = EVENT_TYPE_TOUCH_NONE;

int g_x;
int g_y;
bool g_pressed;

int g_filteredX = -1;
int g_filteredY = -1;
bool g_filteredPressed;

int g_calibratedX = -1;
int g_calibratedY = -1;
bool g_calibratedPressed;

////////////////////////////////////////////////////////////////////////////////

void tick() {
    bool pressed;
    int x;
    int y;
    mcu::touch::read(pressed, x, y);

    g_filteredX = x;
    g_filteredY = y;
    g_filteredPressed = filter(pressed, g_filteredX, g_filteredY);

    g_calibratedPressed = g_filteredPressed;
    g_calibratedX = g_filteredX;
    g_calibratedY = g_filteredY;
    transform(g_calibratedX, g_calibratedY);

    g_x = g_calibratedX;
    g_y = g_calibratedY;
    g_pressed = g_calibratedPressed;

#ifdef EEZ_PLATFORM_SIMULATOR
    g_x = x;
    g_y = y;
    g_pressed = pressed;
#endif

    if (g_pressed) {
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

void data_touch_filtered_x(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        value = touch::g_filteredX;
    }
}

void data_touch_filtered_y(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        value = touch::g_filteredY;
    }
}

void data_touch_filtered_pressed(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        value = touch::g_filteredPressed;
    }
}

void data_touch_calibrated_x(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        value = touch::g_x;
    }
}

void data_touch_calibrated_y(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        value = touch::g_y;
    }
}

void data_touch_calibrated_pressed(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        value = touch::g_pressed;
    }
}

} // namespace gui
} // namespace eez
