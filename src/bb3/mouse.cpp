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
#include <eez/gui/display.h>

#include <bb3/keyboard.h>
#include <bb3/mouse.h>
#include <bb3/system.h>
#include <bb3/usb.h>

#include <bb3/gui/document.h>
#include <bb3/gui/thread.h>

using namespace eez::gui;
using namespace eez::gui::display;

#define CONF_GUI_MOUSE_TIMEOUT_MS 10 * 1000

namespace eez {
namespace mouse {

static bool g_mouseCursorVisible;
static int g_mouseCursorX;
static int g_mouseCursorY;

static bool g_mouseDown;
static uint32_t g_mouseCursorLastActivityTimeMs;

static bool g_mouseWasDown;
static int g_mouseWasCursorX;
static int g_mouseWasCursorY;

static WidgetCursor g_foundWidgetAtMouse;
static OnTouchFunctionType g_onTouchFunctionAtMouse;

static bool g_lastMouseCursorVisible;
static int g_lastMouseCursorX;
static int g_lastMouseCursorY;
static WidgetCursor g_lastFoundWidgetAtMouse;
static OnTouchFunctionType g_lastOnTouchFunctionAtMouse;

MouseInfo g_mouseInfo;

void init() {
    g_mouseCursorX = getDisplayWidth() / 2;
    g_mouseCursorY = getDisplayHeight() / 2;
}

void getEvent(bool &mouseCursorVisible, EventType &mouseEventType, int &mouseX, int &mouseY) {
    uint32_t tickCount = millis();

    if (g_mouseCursorVisible && !g_mouseDown) {
        int32_t diff = tickCount - g_mouseCursorLastActivityTimeMs;
        if (diff >= CONF_GUI_MOUSE_TIMEOUT_MS) {
            g_mouseCursorVisible = false;
        }
    }

    mouseEventType = EVENT_TYPE_TOUCH_NONE;

    if (g_mouseCursorVisible) {
		if (!g_mouseWasDown && g_mouseDown) {
			mouseEventType = EVENT_TYPE_TOUCH_DOWN;
		} else if (g_mouseWasDown && !g_mouseDown) {
			mouseEventType = EVENT_TYPE_TOUCH_UP;
		} else {
			if (g_mouseDown && (g_mouseWasCursorX != g_mouseCursorX || g_mouseWasCursorY != g_mouseCursorY)) {
				mouseEventType = EVENT_TYPE_TOUCH_MOVE;
			}
		}

		g_mouseWasDown = g_mouseDown;
		g_mouseWasCursorX = g_mouseCursorX;
		g_mouseWasCursorY = g_mouseCursorY;

		g_foundWidgetAtMouse = findWidget(&getRootAppContext(), g_mouseCursorX, g_mouseCursorY, false);
		g_onTouchFunctionAtMouse = getWidgetTouchFunction(g_foundWidgetAtMouse);

	    mouseCursorVisible = true;
	    mouseX = g_mouseCursorX;
	    mouseY = g_mouseCursorY;
    } else {
		g_mouseWasDown = false;
		g_mouseWasCursorX = 0;
		g_mouseWasCursorY = 0;

		g_foundWidgetAtMouse = 0;
		g_onTouchFunctionAtMouse = 0;

	    mouseCursorVisible = false;
	    mouseX = 0;
	    mouseY = 0;
    }
}

bool isDisplayDirty() {
    if (
        g_lastMouseCursorVisible != g_mouseCursorVisible ||
        g_lastMouseCursorX != g_mouseCursorX ||
        g_lastMouseCursorY != g_mouseCursorY ||
        g_lastFoundWidgetAtMouse != g_foundWidgetAtMouse ||
        g_lastOnTouchFunctionAtMouse != g_onTouchFunctionAtMouse
    ) {
    	g_lastMouseCursorVisible = g_mouseCursorVisible;
    	g_lastMouseCursorX = g_mouseCursorX;
    	g_lastMouseCursorY = g_mouseCursorY;
        g_lastFoundWidgetAtMouse = g_foundWidgetAtMouse;
        g_lastOnTouchFunctionAtMouse = g_onTouchFunctionAtMouse;

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

            if (g_foundWidgetAtMouse && g_onTouchFunctionAtMouse) {
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
    g_onTouchFunctionAtMouse = 0;
}

void onMouseXMove(int x) {
    g_mouseCursorX = x;
    g_mouseCursorVisible = true;
    g_mouseCursorLastActivityTimeMs = millis();
}

void onMouseYMove(int y) {
    g_mouseCursorY = y;
    g_mouseCursorVisible = true;
    g_mouseCursorLastActivityTimeMs = millis();
}

void onMouseButtonDown(int button) {
    g_mouseCursorVisible = true;
    g_mouseCursorLastActivityTimeMs = millis();
    g_mouseDown = true;
}

void onMouseButtonUp(int button) {
    g_mouseCursorVisible = true;
    g_mouseCursorLastActivityTimeMs = millis();
    g_mouseDown = false;
}

void onMouseDisconnected() {
    g_mouseCursorVisible = false;
    g_mouseDown = false;
}

#if defined(EEZ_PLATFORM_STM32)
static int16_t g_xMouse = -1;
static int16_t g_yMouse = -1;

void onMouseEvent(USBH_HandleTypeDef *phost) {
    HID_MOUSE_Info_TypeDef *info = USBH_HID_GetMouseInfo(phost);

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
        sendMessageToGuiThread(GUI_QUEUE_MESSAGE_MOUSE_X_MOVE, g_xMouse, 10);
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
        sendMessageToGuiThread(GUI_QUEUE_MESSAGE_MOUSE_Y_MOVE, g_yMouse, 10);
    }

    if (!g_mouseInfo.button1 && info->buttons[0]) {
        sendMessageToGuiThread(GUI_QUEUE_MESSAGE_MOUSE_BUTTON_DOWN, 1, 50);
    }
    if (g_mouseInfo.button1 && !info->buttons[0]) {
        sendMessageToGuiThread(GUI_QUEUE_MESSAGE_MOUSE_BUTTON_UP, 1, 50);
    }

    if (!g_mouseInfo.button2 && info->buttons[1]) {
        sendMessageToGuiThread(GUI_QUEUE_MESSAGE_MOUSE_BUTTON_DOWN, 2, 50);
    }
    if (g_mouseInfo.button2 && !info->buttons[1]) {
        sendMessageToGuiThread(GUI_QUEUE_MESSAGE_MOUSE_BUTTON_UP, 2, 50);
    }

    if (!g_mouseInfo.button3 && info->buttons[2]) {
        sendMessageToGuiThread(GUI_QUEUE_MESSAGE_MOUSE_BUTTON_DOWN, 3, 50);
    }
    if (g_mouseInfo.button3 && !info->buttons[2]) {
        sendMessageToGuiThread(GUI_QUEUE_MESSAGE_MOUSE_BUTTON_UP, 3, 50);
    }

    g_mouseInfo.x = info->x;
    g_mouseInfo.y = info->y;
    
    g_mouseInfo.button1 = info->buttons[0];
    g_mouseInfo.button2 = info->buttons[1];
    g_mouseInfo.button3 = info->buttons[2];

    memset(&keyboard::g_keyboardInfo, 0, sizeof(keyboard::KeyboardInfo));
}
#endif

#if defined(EEZ_PLATFORM_SIMULATOR)
void onMouseEvent(bool mouseButton1IsPressed, int mouseX, int mouseY) {
    if (mouseX != g_mouseInfo.x) {
        sendMessageToGuiThread(GUI_QUEUE_MESSAGE_MOUSE_X_MOVE, (uint32_t)(int16_t)mouseX, 10);
    }

    if (mouseY != g_mouseInfo.y) {
        sendMessageToGuiThread(GUI_QUEUE_MESSAGE_MOUSE_Y_MOVE, (uint32_t)(int16_t)mouseY, 10);
    }

    if (!g_mouseInfo.button1 && mouseButton1IsPressed) {
        sendMessageToGuiThread(GUI_QUEUE_MESSAGE_MOUSE_BUTTON_DOWN, 1, 50);
    }

    if (g_mouseInfo.button1 && !mouseButton1IsPressed) {
        sendMessageToGuiThread(GUI_QUEUE_MESSAGE_MOUSE_BUTTON_UP, 1, 50);
    }

    g_mouseInfo.x = mouseX;
    g_mouseInfo.y = mouseY;
    g_mouseInfo.button1 = mouseButton1IsPressed;
    g_mouseInfo.button2 = 0;
    g_mouseInfo.button3 = 0;
}
#endif

bool isMouseEnabled() {
    using namespace eez::usb;
    return g_usbMode == USB_MODE_HOST || g_usbMode == USB_MODE_OTG;
}

} // mouse
} // eez
