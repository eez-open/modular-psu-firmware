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

#include <bb3/mcu/touch.h>

#include <bb3/platform/simulator/events.h>

#include <eez/gui/gui.h>

#include <bb3/mouse.h>
#include <bb3/usb.h>

using namespace eez::platform::simulator;

namespace eez {
namespace mcu {
namespace touch {

void read(bool &isPressed, int &x, int &y) {
    readEvents();

    using namespace eez::usb;
    using namespace eez::gui;

    if (g_usbMode == USB_MODE_HOST || g_usbMode == USB_MODE_OTG) {
        mouse::onMouseEvent(g_mouseButton1IsPressed, g_mouseX, g_mouseY);

        isPressed = false;
        x = 0;
        y = 0;
    } else {
        isPressed = g_mouseButton1IsPressed;
        x = g_mouseX;
        y = g_mouseY;
    }
}

} // namespace touch
} // namespace mcu

namespace gui {

void data_touch_raw_x(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = g_mouseX;
    }
}

void data_touch_raw_y(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = g_mouseY;
    }
}

void data_touch_raw_z1(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = 0;
    }
}

void data_touch_raw_pressed(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = g_mouseButton1IsPressed;
    }
}

} // namespace gui

} // namespace eez
