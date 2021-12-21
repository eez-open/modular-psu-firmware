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

static uint32_t g_lastEventTime;
static EventType g_lastEventType = EVENT_TYPE_TOUCH_NONE;
static int g_lastEventX = -1;
static int g_lastEventY = -1;

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
	
	auto eventType = g_lastEventType;
	if (pressed) {
		if (g_lastEventType == EVENT_TYPE_TOUCH_NONE || g_lastEventType == EVENT_TYPE_TOUCH_UP) {
			eventType = EVENT_TYPE_TOUCH_DOWN;
		} else {
			if (g_lastEventType == EVENT_TYPE_TOUCH_DOWN) {
				eventType = EVENT_TYPE_TOUCH_MOVE;
			}
		}
	} else {
		if (g_lastEventType == EVENT_TYPE_TOUCH_DOWN || g_lastEventType == EVENT_TYPE_TOUCH_MOVE) {
			eventType = EVENT_TYPE_TOUCH_UP;
		} else if (g_lastEventType == EVENT_TYPE_TOUCH_UP) {
			eventType = EVENT_TYPE_TOUCH_NONE;
		}
	}

	if (eventType != g_lastEventType || x != g_lastEventX || y != g_lastEventY) {
		g_lastEventType = eventType;
		g_lastEventX = x;
		g_lastEventY = y;
		if (g_lastEventType != EVENT_TYPE_TOUCH_NONE) {
			g_lastEventTime = millis();
			sendPointerEventToGuiThread(g_lastEventType, x, y);
		}
	} else {
		static const uint32_t SEND_TOUC_MOVE_EVENT_EVERY_MS = 20;
		if (g_lastEventType == EVENT_TYPE_TOUCH_MOVE) {
			auto time = millis();
			if (time - g_lastEventTime > SEND_TOUC_MOVE_EVENT_EVERY_MS) {
				g_lastEventTime = time;
				sendPointerEventToGuiThread(g_lastEventType, g_lastEventX, g_lastEventY);
			}
		}
	}
}

EventType getLastEventType() {
    return g_lastEventType;
}

int getLastX() {
    return g_lastEventX;
}

int getLastY() {
    return g_lastEventY;
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
