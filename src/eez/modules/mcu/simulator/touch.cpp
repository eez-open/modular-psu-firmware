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

#include <eez/modules/mcu/touch.h>

#include <eez/platform/simulator/events.h>

#include <eez/gui/gui.h>

#include <eez/usb.h>

using namespace eez::platform::simulator;

namespace eez {
namespace mcu {
namespace touch {

void read(bool &isPressed, int &x, int &y) {
    readEvents();

    using namespace eez::usb;
    using namespace eez::gui;

    if (g_usbMode == USB_MODE_HOST || g_usbMode == USB_MODE_OTG) {
        if (g_mouseX != g_mouseInfo.x) {
            sendMessageToGuiThread(GUI_QUEUE_MESSAGE_MOUSE_X_MOVE, (uint32_t)(int16_t)g_mouseX, 10);
        }

        if (g_mouseY != g_mouseInfo.y) {
            sendMessageToGuiThread(GUI_QUEUE_MESSAGE_MOUSE_Y_MOVE, (uint32_t)(int16_t)g_mouseY, 10);
        }

        if (!g_mouseInfo.button1 && g_mouseButton1IsPressed) {
            sendMessageToGuiThread(GUI_QUEUE_MESSAGE_MOUSE_BUTTON_DOWN, 1, 50);
        }

        if (g_mouseInfo.button1 && !g_mouseButton1IsPressed) {
            sendMessageToGuiThread(GUI_QUEUE_MESSAGE_MOUSE_BUTTON_UP, 1, 50);
        }

        g_mouseInfo.x = g_mouseX;
        g_mouseInfo.y = g_mouseY;
        g_mouseInfo.button1 = g_mouseButton1IsPressed;
        g_mouseInfo.button2 = 0;
        g_mouseInfo.button3 = 0;

        isPressed = false;
        x = 0;
        y = 0;
    } else {
        if (g_mouseButton1IsPressed) {
            isPressed = true;
        } else {
            isPressed = false;
        }

        x = g_mouseX;
        y = g_mouseY;
    }
}

} // namespace touch
} // namespace mcu

namespace gui {

void data_touch_raw_x(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = g_mouseX;
    }
}

void data_touch_raw_y(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = g_mouseY;
    }
}

void data_touch_raw_z1(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = 0;
    }
}

void data_touch_raw_pressed(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = g_mouseButton1IsPressed;
    }
}

} // namespace gui

} // namespace eez
