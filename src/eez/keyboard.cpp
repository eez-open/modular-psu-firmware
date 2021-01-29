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

#include <memory.h>
#include <string.h>
#include <stdio.h>

#include <eez/keyboard.h>
#include <eez/mouse.h>
#include <eez/gui/gui.h>
#include <eez/modules/mcu/display.h>
#include <eez/modules/psu/psu.h>
#include <eez/modules/psu/gui/psu.h>

using namespace eez::gui;
using namespace eez::psu;
using namespace eez::psu::gui;

namespace eez {
namespace keyboard {

static WidgetCursor g_focusWidgetCursor;
static WidgetCursor g_lastFocusWidgetCursor;

KeyboardInfo g_keyboardInfo;

static void moveToNextFocusCursor();
static void moveToPreviousFocusCursor();

////////////////////////////////////////////////////////////////////////////////

bool isDisplayDirty() {

    if (g_lastFocusWidgetCursor != g_focusWidgetCursor) {
        g_lastFocusWidgetCursor = g_focusWidgetCursor;
    	return true;
    }

    return false;
}

void updateDisplay() {
    using namespace gui;

    if (g_lastFocusWidgetCursor) {
        mcu::display::drawFocusFrame(
            g_lastFocusWidgetCursor.x, g_lastFocusWidgetCursor.y, 
            g_lastFocusWidgetCursor.widget->w, g_lastFocusWidgetCursor.widget->h
        );
    }
}

void onPageChanged() {
    g_focusWidgetCursor = 0;
}

void onKeyDown(uint16_t param) {
    if (getActivePageId() != PAGE_ID_SYS_SETTINGS_USB) {
        uint8_t key = param & 0xFF;
        uint8_t mod = param >> 8;

        bool handled = false;

        if (g_focusWidgetCursor) {
            if (*g_onKeyboardWidgetFunctions[g_focusWidgetCursor.widget->type]) {
                handled = (*g_onKeyboardWidgetFunctions[g_focusWidgetCursor.widget->type])(g_focusWidgetCursor, key, mod);
            }
        }

        if (!handled) {
            if (mod == 0) {
                if (key == KEY_TAB) {
                    moveToNextFocusCursor();
                } else if (key == KEY_PRINTSCREEN) {
                    takeScreenshot();
                } else {
                    if (key >= KEY_1_EXCLAMATION_MARK && key <= KEY_0_CPARENTHESIS) {
                        if (getActivePageId() == PAGE_ID_MAIN) {
                            int channelIndex = key - KEY_1_EXCLAMATION_MARK;
                            if (channelIndex < CH_MAX) {
                                using namespace psu;
                                auto &channel = Channel::get(channelIndex);
                                selectChannel(&channel);
                                clearFoundWidgetAtDown();
                                channelToggleOutput();
                            }
                        }
                    } else if (key == KEY_HOME) {
                        goBack();
                    } else if (key == KEY_ESCAPE) {
                        popPage();
                    } else if (key == KEY_SPACEBAR || key == KEY_ENTER) {
                        if (g_focusWidgetCursor) {
                            if (g_focusWidgetCursor.widget->action) {
                                setFoundWidgetAtDown(g_focusWidgetCursor);
                                executeAction(g_focusWidgetCursor.widget->action);
                            }
                        }
                    }
                }
            } else if (mod == KEY_MOD_LSHIFT || mod == KEY_MOD_RSHIFT) {
                if (key == KEY_TAB) {
                    moveToPreviousFocusCursor();
                }
            }
        }
    }    
}

#if defined(EEZ_PLATFORM_STM32)
void onKeyboardEvent(USBH_HandleTypeDef *phost) {
    HID_KEYBD_Info_TypeDef *info = USBH_HID_GetKeybdInfo(phost);

    if (g_keyboardInfo.keys[0] == 0 && info->keys[0] != 0) {
        sendMessageToGuiThread(GUI_QUEUE_MESSAGE_KEY_DOWN, 
            (
                (
                    (info->lctrl ? KEY_MOD_LCTRL : 0) |
                    (info->lshift ? KEY_MOD_LSHIFT : 0) |
                    (info->lalt ? KEY_MOD_LALT : 0) |
                    (info->lgui ? KEY_MOD_LGUI : 0) |
                    (info->rctrl ? KEY_MOD_RCTRL : 0) |
                    (info->rshift ? KEY_MOD_RSHIFT : 0) |
                    (info->ralt ? KEY_MOD_RALT : 0) |
                    (info->rgui ? KEY_MOD_RGUI : 0)
                ) << 8
            ) | 
            info->keys[0]
        );
    }

    g_keyboardInfo.state = info->state;

    g_keyboardInfo.lctrl = info->lctrl;
    g_keyboardInfo.lshift = info->lshift;
    g_keyboardInfo.lalt = info->lalt;
    g_keyboardInfo.lgui = info->lgui;

    g_keyboardInfo.rctrl = info->rctrl;
    g_keyboardInfo.rshift = info->rshift;
    g_keyboardInfo.ralt = info->ralt;
    g_keyboardInfo.rgui = info->rgui;

    memcpy(g_keyboardInfo.keys, info->keys, 6);

    memset(&mouse::g_mouseInfo, 0, sizeof(mouse::MouseInfo));
}
#endif

#if defined(EEZ_PLATFORM_SIMULATOR)
void onKeyboardEvent(SDL_KeyboardEvent *key) {
    uint8_t mod = 
        (key->keysym.mod & KMOD_LCTRL ? KEY_MOD_LCTRL : 0) |
        (key->keysym.mod & KMOD_LSHIFT ? KEY_MOD_LSHIFT : 0) |
        (key->keysym.mod & KMOD_LALT ? KEY_MOD_LALT : 0) |
        (key->keysym.mod & KMOD_RCTRL ? KEY_MOD_RCTRL : 0) |
        (key->keysym.mod & KMOD_RSHIFT ? KEY_MOD_RSHIFT : 0) |
        (key->keysym.mod & KMOD_RALT ? KEY_MOD_RALT : 0);

    uint8_t scancode = key->keysym.scancode;

    sendMessageToGuiThread(GUI_QUEUE_MESSAGE_KEY_DOWN, (mod << 8) | scancode);

    g_keyboardInfo.state = 0;

    g_keyboardInfo.lctrl = key->keysym.mod & KMOD_LCTRL;
    g_keyboardInfo.lshift = key->keysym.mod & KMOD_LSHIFT;
    g_keyboardInfo.lalt = key->keysym.mod & KMOD_LALT;
    g_keyboardInfo.lgui = 0;

    g_keyboardInfo.rctrl = key->keysym.mod & KMOD_RCTRL;
    g_keyboardInfo.rshift = key->keysym.mod & KMOD_RSHIFT;
    g_keyboardInfo.ralt = key->keysym.mod & KMOD_RALT;
    g_keyboardInfo.rgui = 0;

    g_keyboardInfo.keys[0] = scancode;
}
#endif

////////////////////////////////////////////////////////////////////////////////

static int g_findFocusCursorState;
static WidgetCursor g_focusWidgetCursorIter;

static bool isKeyboardEnabledForWidget(const WidgetCursor &widgetCursor) {
    Overlay *overlay = getOverlay(widgetCursor);
    if (overlay && !overlay->state) {
        return false;
    }

    if (widgetCursor.widget->action != ACTION_ID_NONE) {
        return true;
    }

    if (*g_onKeyboardWidgetFunctions[widgetCursor.widget->type]) {
        return true;
    }

    return false;
}

static void findNextFocusCursor(const WidgetCursor &widgetCursor) {
    if (isKeyboardEnabledForWidget(widgetCursor)) {
        if (g_findFocusCursorState == 0) {
            g_focusWidgetCursorIter = widgetCursor;
            g_findFocusCursorState = 1;
        }

        if (g_findFocusCursorState == 1) {
            if (g_focusWidgetCursor == widgetCursor) {
                g_findFocusCursorState = 2;
            }
        } else if (g_findFocusCursorState == 2) {
            g_focusWidgetCursorIter = widgetCursor;
            g_findFocusCursorState = 3;
        }
    }
}

static void moveToNextFocusCursor() {
    g_findFocusCursorState = 0;
    g_focusWidgetCursorIter = 0;
    
    enumWidgets(&getRootAppContext(), findNextFocusCursor);
    
    if (g_findFocusCursorState > 0) {
        g_focusWidgetCursor = g_focusWidgetCursorIter;
    } else {
        g_focusWidgetCursor = 0;
    }
}

static void findPreviousFocusCursor(const WidgetCursor &widgetCursor) {
    if (isKeyboardEnabledForWidget(widgetCursor)) {
        if (g_findFocusCursorState == 0) {
            g_focusWidgetCursorIter = widgetCursor;
            g_findFocusCursorState = 1;
        } else if (g_findFocusCursorState == 1) {
            if (g_focusWidgetCursor == widgetCursor) {
                g_findFocusCursorState = 2;
            } else {
                g_focusWidgetCursorIter = widgetCursor;
            }
        }
    }
}

static void moveToPreviousFocusCursor() {
    g_findFocusCursorState = 0;
    g_focusWidgetCursorIter = 0;
    
    enumWidgets(&getRootAppContext(), findPreviousFocusCursor);
    
    if (g_findFocusCursorState > 0) {
        g_focusWidgetCursor = g_focusWidgetCursorIter;
    } else {
        g_focusWidgetCursor = 0;
    }
}

////////////////////////////////////////////////////////////////////////////////

} // keyboard

using namespace keyboard;
using namespace mouse;

namespace gui {

void data_usb_keyboard_state(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        static const size_t KEYBOARD_INFO_STRING_SIZE = 256;
        static char g_keyboardInfoStr[2][KEYBOARD_INFO_STRING_SIZE] = { 0 };
        static KeyboardInfo g_latestKeyboardInfo;
        static MouseInfo g_latestMouseInfo;
        static int g_keyboardInfoStrIndex = 0;

        char *str = &g_keyboardInfoStr[g_keyboardInfoStrIndex][0];

        if (
        	memcmp(&g_latestKeyboardInfo, &g_keyboardInfo, sizeof(KeyboardInfo)) != 0 ||
			memcmp(&g_latestMouseInfo, &g_mouseInfo, sizeof(MouseInfo)) != 0
		) {
            memcpy(&g_latestKeyboardInfo, &g_keyboardInfo, sizeof(KeyboardInfo));
            memcpy(&g_latestMouseInfo, &g_mouseInfo, sizeof(MouseInfo));

            g_keyboardInfoStrIndex = (g_keyboardInfoStrIndex + 1) % 2;
            str = &g_keyboardInfoStr[g_keyboardInfoStrIndex][0];
            str[0] = 0;

            if (g_keyboardInfo.state != 0) {
                snprintf(str, KEYBOARD_INFO_STRING_SIZE, "State=0x%02X", g_keyboardInfo.state);
            }

            if (g_keyboardInfo.lctrl) {
                strcat(str, " LCTRL");
            }

            if (g_keyboardInfo.lshift) {
                strcat(str, " LSHIFT");
            }

            if (g_keyboardInfo.lalt) {
                strcat(str, " LALT");
            }

            if (g_keyboardInfo.lgui) {
                strcat(str, " LGUI");
            }

            if (g_keyboardInfo.rctrl) {
                strcat(str, " RCTRL");
            }

            if (g_keyboardInfo.rshift) {
                strcat(str, " RSHIFT");
            }

            if (g_keyboardInfo.ralt) {
                strcat(str, " RALT");
            }

            if (g_keyboardInfo.rgui) {
                strcat(str, " RGUI");
            }

            for (int i = 0; i < 6; i++) {
                if (g_keyboardInfo.keys[0]) {
                    auto n = strlen(str);
                    snprintf(str + n, KEYBOARD_INFO_STRING_SIZE - n, " 0x%02X", g_keyboardInfo.keys[0]);
                }
            }

            if (g_mouseInfo.x != 0) {
                auto n = strlen(str);
                snprintf(str + n, KEYBOARD_INFO_STRING_SIZE - n, " / X=%d", (int8_t)g_mouseInfo.x);
            }

            if (g_mouseInfo.y != 0) {
                auto n = strlen(str);
                snprintf(str + n, KEYBOARD_INFO_STRING_SIZE - n, " Y=%d", (int8_t)g_mouseInfo.y);
            }

            if (g_mouseInfo.button1) {
                strcat(str, " BTN1");
            }

            if (g_mouseInfo.button2) {
                strcat(str, " BTN2");
            }

            if (g_mouseInfo.button3) {
                strcat(str, " BTN3");
            }
        }

        value = (const char *)str;
    }
}

}

} // eez
