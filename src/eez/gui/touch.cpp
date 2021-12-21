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

#include <eez/os.h>

#include <eez/gui/gui.h>
#include <eez/gui/touch_filter.h>
#include <eez/gui/thread.h>

#include <eez/platform/touch.h>

////////////////////////////////////////////////////////////////////////////////

namespace eez {
namespace gui {
namespace touch {

static Event g_lastEvent;

static int g_calibratedX = -1;
static int g_calibratedY = -1;
static bool g_calibratedPressed = false;

static int g_filteredX = -1;
static int g_filteredY = -1;
static bool g_filteredPressed = false;

////////////////////////////////////////////////////////////////////////////////

void tick() {
	bool pressed;
	int x;
	int y;
	mcu::touch::read(pressed, x, y);

	g_calibratedPressed = pressed;
	g_calibratedX = x;
	g_calibratedY = y;
	transform(g_calibratedX, g_calibratedY);

	g_filteredX = g_calibratedX;
	g_filteredY = g_calibratedY;
	g_filteredPressed = filter(g_calibratedPressed, g_filteredX, g_filteredY);

#ifdef EEZ_PLATFORM_STM32
	pressed = g_filteredPressed;
	x = g_filteredX;
	y = g_filteredY;
#endif

	onTouchEvent(pressed, x, y, g_lastEvent);
}

Event &getLastEvent() {
    return g_lastEvent;
}

} // namespace touch

void data_touch_calibrated_x(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = touch::g_calibratedX;
    }
}

void data_touch_calibrated_y(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = touch::g_calibratedY;
    }
}

void data_touch_calibrated_pressed(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = touch::g_calibratedPressed;
    }
}

void data_touch_filtered_x(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = touch::g_filteredX;
    }
}

void data_touch_filtered_y(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = touch::g_filteredY;
    }
}

void data_touch_filtered_pressed(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = touch::g_filteredPressed;
    }
}

} // namespace gui
} // namespace eez
