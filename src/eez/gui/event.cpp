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

#if OPTION_DISPLAY

#include <eez/firmware.h>
#include <eez/system.h>

#include <eez/gui/gui.h>
#include <eez/gui/widgets/container.h>

#include <eez/modules/psu/psu.h>
#include <eez/modules/psu/idle.h>

#define CONF_GUI_LONG_TOUCH_TIMEOUT              1000000L // 1s
#define CONF_GUI_KEYPAD_FIRST_AUTO_REPEAT_DELAY   300000L // 300ms
#define CONF_GUI_KEYPAD_NEXT_AUTO_REPEAT_DELAY     50000L // 50ms
#define CONF_GUI_EXTRA_LONG_TOUCH_TIMEOUT       30000000L // 30s

namespace eez {
namespace gui {

static WidgetCursor m_foundWidgetAtDown;
static WidgetCursor m_activeWidget;
static bool m_touchActionExecuted;
static bool m_touchActionExecutedAtDown;
static OnTouchFunctionType m_onTouchFunction;
static uint32_t m_touchDownTime;
static uint32_t m_lastAutoRepeatEventTime;
static bool m_longTouchGenerated;
static bool m_extraLongTouchGenerated;
static int m_lastTouchMoveX = -1;
static int m_lastTouchMoveY = -1;

void processTouchEvent(EventType type);
void onPageTouch(const WidgetCursor &foundWidget, Event &touchEvent);
void onInternalPageTouch(const WidgetCursor &widgetCursor, Event &touchEvent);
void onWidgetDefaultTouch(const WidgetCursor &widgetCursor, Event &touchEvent);

void eventHandling() {
	if (g_shutdownInProgress) {
		return;
	}

    using namespace eez::gui::touch;

    if (g_eventType != EVENT_TYPE_TOUCH_NONE) {
        psu::idle::noteHmiActivity();
    }

    uint32_t tickCount = micros();

    if (g_eventType == EVENT_TYPE_TOUCH_DOWN) {
        m_touchDownTime = tickCount;
        m_lastAutoRepeatEventTime = tickCount;
        m_longTouchGenerated = false;
        m_extraLongTouchGenerated = false;
        processTouchEvent(EVENT_TYPE_TOUCH_DOWN);
    } else if (g_eventType == EVENT_TYPE_TOUCH_MOVE) {
        processTouchEvent(EVENT_TYPE_TOUCH_MOVE);

        if (!m_longTouchGenerated &&
            int32_t(tickCount - m_touchDownTime) >= CONF_GUI_LONG_TOUCH_TIMEOUT) {
            m_longTouchGenerated = true;
            processTouchEvent(EVENT_TYPE_LONG_TOUCH);
        }

        if (m_longTouchGenerated && !m_extraLongTouchGenerated &&
            int32_t(tickCount - m_touchDownTime) >= CONF_GUI_EXTRA_LONG_TOUCH_TIMEOUT) {
            m_extraLongTouchGenerated = true;
            processTouchEvent(EVENT_TYPE_EXTRA_LONG_TOUCH);
        }

        if (int32_t(tickCount - m_lastAutoRepeatEventTime) >= (m_lastAutoRepeatEventTime == m_touchDownTime ? CONF_GUI_KEYPAD_FIRST_AUTO_REPEAT_DELAY : CONF_GUI_KEYPAD_NEXT_AUTO_REPEAT_DELAY)) {
            processTouchEvent(EVENT_TYPE_AUTO_REPEAT);
            m_lastAutoRepeatEventTime = tickCount;
        }

    } else if (g_eventType == EVENT_TYPE_TOUCH_UP) {
        processTouchEvent(EVENT_TYPE_TOUCH_UP);
    }
}

void processTouchEvent(EventType type) {
    int x = touch::getX();
    int y = touch::getY();

    if (type == EVENT_TYPE_TOUCH_DOWN) {
        m_foundWidgetAtDown = findWidget(x, y);
        m_onTouchFunction = getTouchFunction(m_foundWidgetAtDown);
        if (!m_onTouchFunction) {
            m_onTouchFunction = onPageTouch;
        }
    } 
#if defined(EEZ_PLATFORM_STM32)    
    else if (type == EVENT_TYPE_TOUCH_MOVE) {
        // ignore EVENT_TYPE_TOUCH_MOVE if it is the same as the last event
        // or change is too big
    	int dx = (m_lastTouchMoveX - x);
    	int dy = (m_lastTouchMoveY - y);
    	int d = dx * dx + dy * dy;
        if (d == 0 || d > 1000) {
            return;
        }
    }
#endif
    else if (type == EVENT_TYPE_TOUCH_UP) {
        m_activeWidget = 0;
    }

    if (m_onTouchFunction) {
        AppContext *saved = g_appContext;
        if (m_foundWidgetAtDown) {
            g_appContext = m_foundWidgetAtDown.appContext;
        }

        Event event;
        event.type = type;
        event.x = x;
        event.y = y;

        m_onTouchFunction(m_foundWidgetAtDown, event);

        g_appContext = saved;
    }

    m_lastTouchMoveX = x;
    m_lastTouchMoveY = y;
}

OnTouchFunctionType getTouchFunction(const WidgetCursor &widgetCursor) {
    if (widgetCursor.appContext && widgetCursor.appContext->isActivePageInternal()) {
        return onInternalPageTouch;
    }

    if (widgetCursor) {
		if (!widgetCursor.widget->action || widgetCursor.appContext->isWidgetActionEnabled(widgetCursor)) {
            if (g_onTouchFunctions[widgetCursor.widget->type]) {
                return g_onTouchFunctions[widgetCursor.widget->type];
            }
        }

		if (widgetCursor.widget->action) {
			if (widgetCursor.appContext->isWidgetActionEnabled(widgetCursor)) {
				return onWidgetDefaultTouch;
			}
		}
    }

    return nullptr;
}

int getAction(const WidgetCursor &widgetCursor) {
    return widgetCursor.widget->action;
}

void onPageTouch(const WidgetCursor &foundWidget, Event &touchEvent) {
    g_appContext->onPageTouch(foundWidget, touchEvent);
}

void onInternalPageTouch(const WidgetCursor &widgetCursor, Event &touchEvent) {
    if (touchEvent.type == EVENT_TYPE_TOUCH_DOWN) {
        if (m_foundWidgetAtDown) {
            m_activeWidget = m_foundWidgetAtDown;
        }
    } else if (touchEvent.type == EVENT_TYPE_TOUCH_UP) {
        if (m_foundWidgetAtDown) {
            m_activeWidget = 0;
            int action = getAction(m_foundWidgetAtDown);
            executeAction(action);
        } else {
            InternalPage *page = (InternalPage *)widgetCursor.appContext->getActivePage();
            if (!pointInsideRect(touchEvent.x, touchEvent.y, page->x, page->y, page->width,
                page->height)) {
                widgetCursor.appContext->popPage();
            }
        }
    }
}

void onWidgetDefaultTouch(const WidgetCursor &widgetCursor, Event &touchEvent) {
    if (!widgetCursor.widget) {
        return;
    }

    if (touchEvent.type == EVENT_TYPE_TOUCH_DOWN) {
        m_touchActionExecuted = false;
        m_touchActionExecutedAtDown = false;

        int action = getAction(widgetCursor);
        if (action == ACTION_ID_DRAG_OVERLAY) {
            dragOverlay(touchEvent);
            m_activeWidget = widgetCursor;    
        } else if (widgetCursor.appContext->testExecuteActionOnTouchDown(action)) {
            executeAction(action);
            m_touchActionExecutedAtDown = true;
            if (widgetCursor.appContext->isAutoRepeatAction(action)) {
                m_activeWidget = widgetCursor;    
            }
        } else {
            m_activeWidget = widgetCursor;
        }
    } else if (touchEvent.type == EVENT_TYPE_TOUCH_MOVE) {
        int action = getAction(widgetCursor);
        if (action == ACTION_ID_DRAG_OVERLAY) {
            dragOverlay(touchEvent);
        }
    } else if (touchEvent.type == EVENT_TYPE_AUTO_REPEAT) {
        int action = getAction(widgetCursor);
        if (widgetCursor.appContext->isWidgetActionEnabled(widgetCursor) && widgetCursor.appContext->isAutoRepeatAction(action)) {
            m_touchActionExecuted = true;
            executeAction(action);
        }
    } else if (touchEvent.type == EVENT_TYPE_LONG_TOUCH) {
        m_touchActionExecuted = true;
        int action = widgetCursor.appContext->getLongTouchActionHook(widgetCursor);
        if (action != ACTION_ID_NONE) {
            executeAction(action);
        }
    } else if (touchEvent.type == EVENT_TYPE_TOUCH_UP) {
        if (!m_touchActionExecutedAtDown) {
            m_activeWidget = 0;
            if (!m_touchActionExecuted) {
                int action = getAction(widgetCursor);
                if (action == ACTION_ID_DRAG_OVERLAY) {
                    dragOverlay(touchEvent);
                } else {
                    executeAction(action);
                }
            }
        }
    }
}

WidgetCursor &getFoundWidgetAtDown() {
    return m_foundWidgetAtDown;
}

void clearFoundWidgetAtDown() {
    m_foundWidgetAtDown = WidgetCursor();
}

bool isActiveWidget(const WidgetCursor &widgetCursor) {
    if (widgetCursor.appContext && widgetCursor.appContext->isActiveWidget(widgetCursor)) {
        return true;
    }
    return widgetCursor == m_activeWidget;
}

bool isFocusWidget(const WidgetCursor &widgetCursor) {
    return g_appContext->isFocusWidget(widgetCursor);
}

} // namespace gui
} // namespace eez

#endif
