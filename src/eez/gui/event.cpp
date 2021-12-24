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

#include <stdio.h>

#include <eez/conf.h>
#include <eez/os.h>
#include <eez/hmi.h>

#if OPTION_MOUSE
#include <eez/mouse.h>
#endif

#include <eez/util.h>

#include <eez/gui/gui.h>
#include <eez/gui/thread.h>

#define CONF_GUI_LONG_TOUCH_TIMEOUT_MS 1000
#define CONF_GUI_KEYPAD_FIRST_AUTO_REPEAT_DELAY_MS 300
#define CONF_GUI_KEYPAD_NEXT_AUTO_REPEAT_DELAY_MS 50
#define CONF_GUI_EXTRA_LONG_TOUCH_TIMEOUT_MS 15000

namespace eez {
namespace gui {

////////////////////////////////////////////////////////////////////////////////

WidgetCursor g_activeWidget;
bool g_isLongTouch;

static WidgetCursor g_foundWidgetAtDown;

static bool g_touchActionExecuted;
static bool g_touchActionExecutedAtDown;

static OnTouchFunctionType g_onTouchFunction;

static bool g_longTouchGenerated;
static bool g_extraLongTouchGenerated;

////////////////////////////////////////////////////////////////////////////////

static void processTouchEvent(EventType type, Event &touchEvent);
static void onPageTouch(const WidgetCursor &foundWidget, Event &touchEvent);
static void onWidgetDefaultTouch(const WidgetCursor &widgetCursor, Event &touchEvent);
static void onWidgetTouch(const WidgetCursor &widgetCursor, Event &touchEvent);

////////////////////////////////////////////////////////////////////////////////

void onTouchEvent(bool pressed, int x, int y, Event &lastEvent) {
	auto eventType = lastEvent.type;
	if (pressed) {
		if (lastEvent.type == EVENT_TYPE_TOUCH_NONE || lastEvent.type == EVENT_TYPE_TOUCH_UP) {
			eventType = EVENT_TYPE_TOUCH_DOWN;
		} else {
			if (lastEvent.type == EVENT_TYPE_TOUCH_DOWN) {
				eventType = EVENT_TYPE_TOUCH_MOVE;
			}
		}
	} else {
		if (lastEvent.type == EVENT_TYPE_TOUCH_DOWN || lastEvent.type == EVENT_TYPE_TOUCH_MOVE) {
			eventType = EVENT_TYPE_TOUCH_UP;
		} else if (lastEvent.type == EVENT_TYPE_TOUCH_UP) {
			eventType = EVENT_TYPE_TOUCH_NONE;
		}
	}

	if (eventType != lastEvent.type || x != lastEvent.x || y != lastEvent.y) {
		lastEvent.type = eventType;
		lastEvent.x = x;
		lastEvent.y = y;
		if (lastEvent.type != EVENT_TYPE_TOUCH_NONE) {
			sendTouchEventToGuiThread(lastEvent);
		}
	} else {
		static const uint32_t SEND_TOUC_MOVE_EVENT_EVERY_MS = 20;
		if (lastEvent.type == EVENT_TYPE_TOUCH_MOVE) {
			auto time = millis();
			if (time - lastEvent.time > SEND_TOUC_MOVE_EVENT_EVERY_MS) {
				sendTouchEventToGuiThread(lastEvent);
			}
		}
	}
}

void processTouchEvent(Event &touchEvent) {
	if (isEventHandlingDisabledHook()) {
		return;
	}

    static uint32_t m_lastAutoRepeatEventTimeMs;
    static uint32_t m_touchDownTimeMs;

    if (touchEvent.type != EVENT_TYPE_TOUCH_NONE) {
        uint32_t tickCountMs = millis();

        eez::hmi::noteActivity();

        if (touchEvent.type == EVENT_TYPE_TOUCH_DOWN) {
            m_touchDownTimeMs = tickCountMs;
            m_lastAutoRepeatEventTimeMs = tickCountMs;
            g_longTouchGenerated = false;
            g_extraLongTouchGenerated = false;
            processTouchEvent(EVENT_TYPE_TOUCH_DOWN, touchEvent);
        } else if (touchEvent.type == EVENT_TYPE_TOUCH_MOVE) {
            processTouchEvent(EVENT_TYPE_TOUCH_MOVE, touchEvent);

            if (!g_longTouchGenerated && int32_t(tickCountMs - m_touchDownTimeMs) >= CONF_GUI_LONG_TOUCH_TIMEOUT_MS) {
                g_longTouchGenerated = true;
                processTouchEvent(EVENT_TYPE_LONG_TOUCH, touchEvent);
            }

            if (g_longTouchGenerated && !g_extraLongTouchGenerated && int32_t(tickCountMs - m_touchDownTimeMs) >= CONF_GUI_EXTRA_LONG_TOUCH_TIMEOUT_MS) {
                g_extraLongTouchGenerated = true;
                processTouchEvent(EVENT_TYPE_EXTRA_LONG_TOUCH, touchEvent);
            }

            if (int32_t(tickCountMs - m_lastAutoRepeatEventTimeMs) >= (m_lastAutoRepeatEventTimeMs == m_touchDownTimeMs ? CONF_GUI_KEYPAD_FIRST_AUTO_REPEAT_DELAY_MS : CONF_GUI_KEYPAD_NEXT_AUTO_REPEAT_DELAY_MS)) {
                processTouchEvent(EVENT_TYPE_AUTO_REPEAT, touchEvent);
                m_lastAutoRepeatEventTimeMs = tickCountMs;
            }
        } else if (touchEvent.type == EVENT_TYPE_TOUCH_UP) {
            processTouchEvent(EVENT_TYPE_TOUCH_UP, touchEvent);
        }
    }
}

static void processTouchEvent(EventType type, Event &touchEvent) {
    if (type == EVENT_TYPE_TOUCH_DOWN) {
        g_foundWidgetAtDown = findWidget(touchEvent.x, touchEvent.y);
        g_onTouchFunction = getWidgetTouchFunction(g_foundWidgetAtDown);
        if (!g_onTouchFunction) {
            g_onTouchFunction = onPageTouch;
        }
    } else if (type == EVENT_TYPE_TOUCH_UP) {
        g_activeWidget = 0;
    }

    if (g_onTouchFunction) {
        Event event = touchEvent;
        event.type = type;
        g_onTouchFunction(g_foundWidgetAtDown, event);
    }
}

OnTouchFunctionType getWidgetTouchFunction(const WidgetCursor &widgetCursor) {
    if (widgetCursor) {
        auto action = getWidgetAction(widgetCursor);
        if (action && !widgetCursor.appContext->isWidgetActionEnabled(widgetCursor)) {
            return nullptr;
        }

        if (widgetCursor.currentState->hasOnTouch()) {
			return onWidgetTouch;
		}

        if (widgetCursor.appContext->isWidgetActionEnabled(widgetCursor)) {
            return onWidgetDefaultTouch;
        }

        return getWidgetTouchFunctionHook(widgetCursor);
    }

    return nullptr;
}

static void onPageTouch(const WidgetCursor &foundWidget, Event &touchEvent) {
	if (foundWidget.appContext) {
		foundWidget.appContext->onPageTouch(foundWidget, touchEvent);
	} else {
		getRootAppContext().onPageTouch(foundWidget, touchEvent);
	}
}

static void onWidgetDefaultTouch(const WidgetCursor &widgetCursor, Event &touchEvent) {
    if (!widgetCursor.widget) {
        return;
    }

    auto action = getWidgetAction(widgetCursor);        

    if (touchEvent.type == EVENT_TYPE_TOUCH_DOWN) {
        g_touchActionExecuted = false;
        g_touchActionExecutedAtDown = false;

        if (action == EEZ_CONF_ACTION_ID_DRAG_OVERLAY) {
            dragOverlay(touchEvent);
            g_activeWidget = widgetCursor;
        } else if (widgetCursor.appContext->testExecuteActionOnTouchDown(action)) {
            executeAction(widgetCursor, action);
            g_touchActionExecutedAtDown = true;
            if (widgetCursor.appContext->isAutoRepeatAction(action)) {
                g_activeWidget = widgetCursor;
            }
        } else {
            g_activeWidget = widgetCursor;
        }
    } else if (touchEvent.type == EVENT_TYPE_TOUCH_MOVE) {
        if (action == EEZ_CONF_ACTION_ID_DRAG_OVERLAY) {
            dragOverlay(touchEvent);
        }
    } else if (touchEvent.type == EVENT_TYPE_AUTO_REPEAT) {
        if (widgetCursor.appContext->isWidgetActionEnabled(widgetCursor) && widgetCursor.appContext->isAutoRepeatAction(action)) {
            g_touchActionExecuted = true;
            executeAction(widgetCursor, action);
        }
    } else if (touchEvent.type == EVENT_TYPE_LONG_TOUCH) {
        g_touchActionExecuted = true;
        int action = widgetCursor.appContext->getLongTouchActionHook(widgetCursor);
        if (action != ACTION_ID_NONE) {
            g_isLongTouch = true;
            executeAction(widgetCursor, action);
            g_isLongTouch = false;
        }
    } else if (touchEvent.type == EVENT_TYPE_EXTRA_LONG_TOUCH) {
        g_touchActionExecuted = true;
        int action = widgetCursor.appContext->getExtraLongTouchActionHook(widgetCursor);
        if (action != ACTION_ID_NONE) {
            executeAction(widgetCursor, action);
        }
    } else if (touchEvent.type == EVENT_TYPE_TOUCH_UP) {
        if (!g_touchActionExecutedAtDown) {
            if (!g_touchActionExecuted) {
                if (action == EEZ_CONF_ACTION_ID_DRAG_OVERLAY) {
                    dragOverlay(touchEvent);
                } else {
                    executeAction(widgetCursor, action);
                }
            }
        }
    }
}

static void onWidgetTouch(const WidgetCursor &widgetCursor, Event &touchEvent) {
	if (widgetCursor) {
        if (touchEvent.type == EVENT_TYPE_TOUCH_DOWN) {
			g_activeWidget = g_foundWidgetAtDown;
        }
		widgetCursor.currentState->onTouch(widgetCursor, touchEvent);
	}
}


////////////////////////////////////////////////////////////////////////////////

WidgetCursor &getFoundWidgetAtDown() {
    return g_foundWidgetAtDown;
}

void setFoundWidgetAtDown(WidgetCursor &widgetCursor) {
    g_foundWidgetAtDown = widgetCursor;
}

void clearFoundWidgetAtDown() {
    g_foundWidgetAtDown = 0;
}

} // namespace gui
} // namespace eez
