/*
* EEZ Generic Firmware
* Copyright (C) 2020-present, Envox d.o.o.
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
#include <eez/gui/thread.h>

#include <bb3/keyboard.h>
#include <bb3/mouse.h>
#include <bb3/system.h>
#include <bb3/usb.h>

#include <bb3/gui/document.h>

using namespace eez::gui;
using namespace eez::gui::display;

namespace eez {
namespace mouse {

static bool g_mouseCursorVisible;
MouseInfo g_mouseInfo;

static uint32_t g_lastEventTime;
static EventType g_lastEventType = EVENT_TYPE_TOUCH_NONE;
static int g_lastEventX = -1;
static int g_lastEventY = -1;

static WidgetCursor g_foundWidgetAtMouse;

static bool g_lastMouseCursorVisible;
static int g_lastMouseCursorX;
static int g_lastMouseCursorY;

void onMouseEvent() {
    g_mouseCursorVisible = true;

    auto pressed = g_mouseInfo.button1 || g_mouseInfo.button2 || g_mouseInfo.button3;
    int x = g_mouseInfo.x;
    int y = g_mouseInfo.y;

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

bool isDisplayDirty() {
    if (
        g_lastMouseCursorVisible != g_mouseCursorVisible ||
        g_lastMouseCursorX != g_mouseInfo.x ||
        g_lastMouseCursorY != g_mouseInfo.y
    ) {
    	g_lastMouseCursorVisible = g_mouseCursorVisible;
    	g_lastMouseCursorX = g_mouseInfo.x;
    	g_lastMouseCursorY = g_mouseInfo.y;

        auto foundWidgetAtMouse = findWidget(g_mouseInfo.x, g_mouseInfo.y, false);
        if (getWidgetTouchFunction(foundWidgetAtMouse)) {
            g_foundWidgetAtMouse = foundWidgetAtMouse;
        } else {
            g_foundWidgetAtMouse = 0;
        }

    	return true;
    }

    return false;
}

void updateDisplay() {
    using namespace gui;

    if (g_lastMouseCursorVisible) {
        if (g_lastMouseCursorX < getDisplayWidth() && g_lastMouseCursorY < getDisplayHeight()) {
            auto bitmap = getBitmap(BITMAP_ID_MOUSE_CURSOR);

            Image image;

            image.width = bitmap->w;
            image.height = bitmap->h;
            image.bpp = bitmap->bpp;
            image.lineOffset = 0;
            image.pixels = (uint8_t *)bitmap->pixels;

            if (g_lastMouseCursorX + (int)image.width > getDisplayWidth()) {
                image.width = getDisplayWidth() - g_lastMouseCursorX;
                image.lineOffset = bitmap->w - image.width;
            }

            if (g_lastMouseCursorY + (int)image.height > getDisplayHeight()) {
                image.height = getDisplayHeight() - g_lastMouseCursorY;
            }

            if (g_foundWidgetAtMouse) {
                int16_t w;
                int16_t h;

                auto overlay = getOverlay(g_foundWidgetAtMouse);
                if (overlay && overlay->widgetOverrides) {
                    w = overlay->width;
                    h = overlay->height;
                } else {
                    w = g_foundWidgetAtMouse.widget->w;
                    h = g_foundWidgetAtMouse.widget->h;
                }

                drawFocusFrame(g_foundWidgetAtMouse.x, g_foundWidgetAtMouse.y, w, h);
            }

            display::drawBitmap(&image, g_lastMouseCursorX, g_lastMouseCursorY);
        }
    }
}

void onPageChanged() {
    g_foundWidgetAtMouse = 0;
}

void onMouseDisconnected() {
    g_mouseCursorVisible = false;

    g_lastEventType = EVENT_TYPE_TOUCH_NONE;
    g_lastEventX = -1;
    g_lastEventY = -1;
}

#if defined(EEZ_PLATFORM_STM32)
static volatile int16_t g_xMouse = -1;
static volatile int16_t g_yMouse = -1;

void onMouseEvent(USBH_HandleTypeDef *phost) {
    HID_MOUSE_Info_TypeDef *info = USBH_HID_GetMouseInfo(phost);

    g_mouseInfo.dx = info->x;
    g_mouseInfo.dy = info->y;

    if (info->x != 0) {
        if (g_xMouse == -1) {
            g_xMouse = getDisplayWidth() / 2;
        }
        g_xMouse += (int8_t)info->x;
        if (g_xMouse < 0) {
            g_xMouse = 0;
        }
        if (g_xMouse >= getDisplayWidth()) {
            g_xMouse = getDisplayWidth() - 1;
        }
    }

    if (info->y != 0) {
        if (g_yMouse == -1) {
            g_yMouse = getDisplayHeight() / 2;
        }
        g_yMouse += (int8_t)info->y;
        if (g_yMouse < 0) {
            g_yMouse = 0;
        }
        if (g_yMouse >= getDisplayHeight()) {
            g_yMouse = getDisplayHeight() - 1;
        }
    }

    g_mouseInfo.x = g_xMouse;
    g_mouseInfo.y = g_yMouse;
    
    g_mouseInfo.button1 = info->buttons[0];
    g_mouseInfo.button2 = info->buttons[1];
    g_mouseInfo.button3 = info->buttons[2];

    memset(&keyboard::g_keyboardInfo, 0, sizeof(keyboard::KeyboardInfo));

    onMouseEvent();
}
#endif

#if defined(EEZ_PLATFORM_SIMULATOR)
void onMouseEvent(bool mouseButton1IsPressed, int mouseX, int mouseY) {
    g_mouseInfo.x = mouseX;
    g_mouseInfo.y = mouseY;
    g_mouseInfo.button1 = mouseButton1IsPressed;
    g_mouseInfo.button2 = 0;
    g_mouseInfo.button3 = 0;

    onMouseEvent();
}
#endif

bool isMouseEnabled() {
    using namespace eez::usb;
    return g_usbMode == USB_MODE_HOST || g_usbMode == USB_MODE_OTG;
}

} // mouse
} // eez
