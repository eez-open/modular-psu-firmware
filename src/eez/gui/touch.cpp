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

static EventType g_lastEventType = EVENT_TYPE_TOUCH_NONE;

static int g_x = -1;
static int g_y = -1;
static bool g_pressed = false;

static int g_calibratedX = -1;
static int g_calibratedY = -1;
static bool g_calibratedPressed = false;

static int g_filteredX = -1;
static int g_filteredY = -1;
static bool g_filteredPressed = false;

////////////////////////////////////////////////////////////////////////////////

static EventType g_eventType = EVENT_TYPE_TOUCH_NONE;
static int g_eventX = -1;
static int g_eventY = -1;

////////////////////////////////////////////////////////////////////////////////

#if defined(EEZ_PLATFORM_STM32)
static const int EVENT_QUEUE_MAX_SIZE = 50;
struct Event {
    EventType type;
    int x;
    int y;
};
static Event g_eventQueue[EVENT_QUEUE_MAX_SIZE];
static uint8_t g_eventQueueHead = 0;
static uint8_t g_eventQueueTail = 0;
static bool g_eventQueueFull;
#endif

void tickHighPriority() {
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

    g_x = g_filteredX;
    g_y = g_filteredY;
    g_pressed = g_filteredPressed;

#ifdef EEZ_PLATFORM_SIMULATOR
    g_x = x;
    g_y = y;
    g_pressed = pressed;
#endif

    if (g_pressed) {
        if (g_lastEventType == EVENT_TYPE_TOUCH_NONE || g_lastEventType == EVENT_TYPE_TOUCH_UP) {
            g_lastEventType = EVENT_TYPE_TOUCH_DOWN;
        } else {
            if (g_lastEventType == EVENT_TYPE_TOUCH_DOWN) {
                g_lastEventType = EVENT_TYPE_TOUCH_MOVE;
            }
        }
    } else {
        if (g_lastEventType == EVENT_TYPE_TOUCH_DOWN || g_lastEventType == EVENT_TYPE_TOUCH_MOVE) {
            g_lastEventType = EVENT_TYPE_TOUCH_UP;
        } else if (g_lastEventType == EVENT_TYPE_TOUCH_UP) {
            g_lastEventType = EVENT_TYPE_TOUCH_NONE;
        }
    }

#if defined(EEZ_PLATFORM_STM32)
    if (g_lastEventType != EVENT_TYPE_TOUCH_NONE) {
        if (g_eventQueueFull || g_eventQueueTail != g_eventQueueHead) {
            uint32_t previousEventQueueHead = g_eventQueueHead == 0 ? EVENT_QUEUE_MAX_SIZE - 1 : g_eventQueueHead - 1;
            Event &event = g_eventQueue[previousEventQueueHead];
            if (event.type == g_lastEventType) {
                g_eventQueueHead = previousEventQueueHead;
            }
        }

        Event &event = g_eventQueue[g_eventQueueHead];

        event.type = g_lastEventType;
        event.x = g_x;
        event.y = g_y;

        if (g_eventQueueFull) {
            g_eventQueueTail = (g_eventQueueTail + 1) % EVENT_QUEUE_MAX_SIZE;
        }

        g_eventQueueHead = (g_eventQueueHead + 1) % EVENT_QUEUE_MAX_SIZE;

        if (g_eventQueueHead == g_eventQueueTail) {
            g_eventQueueFull = true;
        }
    }
#endif
}

////////////////////////////////////////////////////////////////////////////////

void tick() {
#if defined(EEZ_PLATFORM_STM32)
    taskENTER_CRITICAL();

    if (g_eventQueueFull || g_eventQueueTail != g_eventQueueHead) {
        Event &event = g_eventQueue[g_eventQueueTail];

        g_eventType = event.type;
        g_eventX = event.x;
        g_eventY = event.y;

        g_eventQueueTail = (g_eventQueueTail + 1) % EVENT_QUEUE_MAX_SIZE;
        g_eventQueueFull = false;
    } else {
        g_eventType = EVENT_TYPE_TOUCH_NONE;
        g_eventX = -1;
        g_eventY = -1;
    }

    taskEXIT_CRITICAL();
#endif

#if defined(EEZ_PLATFORM_SIMULATOR)
    tickHighPriority();

    g_eventType = g_lastEventType;
    g_eventX = g_x;
    g_eventY = g_y;
#endif
}

EventType getEventType() {
    return g_eventType;
}

int getX() {
    return g_eventX;
}

int getY() {
    return g_eventY;
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
