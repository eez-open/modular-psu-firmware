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
#include <eez/modules/mcu/encoder.h>

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
osMutexId(g_eventQueueMutexId);
osMutexDef(g_eventQueueMutex);

void mainLoop(const void *);

#if defined(EEZ_PLATFORM_STM32)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wwrite-strings"
#endif

osThreadDef(g_touchTask, mainLoop, osPriorityNormal, 0, 1024);

#if defined(EEZ_PLATFORM_STM32)
#pragma GCC diagnostic pop
#endif

void oneIter();

void mainLoop(const void *) {
#ifdef __EMSCRIPTEN__
	oneIter();
#else
    while (1) {
        oneIter();
        mcu::encoder::tick();
        osDelay(10);
    }
#endif
}

#endif

void oneIter() {
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
        if (osMutexWait(g_eventQueueMutexId, 5) == osOK) {
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

            osMutexRelease(g_eventQueueMutexId);
        }
    }
#endif
}

////////////////////////////////////////////////////////////////////////////////

void init() {
#if defined(EEZ_PLATFORM_STM32)
    g_eventQueueMutexId = osMutexCreate(osMutex(g_eventQueueMutex));
    osThreadCreate(osThread(g_touchTask), nullptr);
#endif
}

void tick() {
#if defined(EEZ_PLATFORM_STM32)
    if (osMutexWait(g_eventQueueMutexId, 5) == osOK) {
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

        osMutexRelease(g_eventQueueMutexId);
    }
#endif

#if defined(EEZ_PLATFORM_SIMULATOR)
    oneIter();

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

void data_touch_calibrated_x(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = touch::g_calibratedX;
    }
}

void data_touch_calibrated_y(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = touch::g_calibratedY;
    }
}

void data_touch_calibrated_pressed(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = touch::g_calibratedPressed;
    }
}

void data_touch_filtered_x(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = touch::g_filteredX;
    }
}

void data_touch_filtered_y(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = touch::g_filteredY;
    }
}

void data_touch_filtered_pressed(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = touch::g_filteredPressed;
    }
}

} // namespace gui
} // namespace eez
