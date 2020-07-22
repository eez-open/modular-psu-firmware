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

#if defined(EEZ_PLATFORM_STM32)
#include <gpio.h>
#include <usb_device.h>
#include <usb_host.h>
#include <usbh_hid.h>
#endif

#include <eez/keyboard.h>
#include <eez/mouse.h>
#include <eez/usb.h>

#include <eez/gui/gui.h>

#include <eez/modules/psu/psu.h>
#include <eez/modules/psu/persist_conf.h>
#include <eez/modules/psu/serial_psu.h>

#include <eez/modules/mcu/display.h>

namespace eez {
namespace usb {

int g_usbMode;

enum Event {
    EVENT_OTG_ID_HI,
    EVENT_OTG_ID_LOW,
    EVENT_OTG_OC_HI,
    EVENT_OTG_OC_LOW,
    EVENT_DEBOUNCE_TIMEOUT
};

enum State {
    STATE_UNKNOWN_MODE,
    STATE_DEVICE_MODE,
    STATE_HOST_MODE,
    STATE_DEBOUNCE_DEVICE_MODE,
    STATE_DEBOUNCE_HOST_MODE,
	STATE_FAULT_DETECTED
};

static const int CONF_OTG_ID_DEBOUNCE_TIMEOUT_MS = 100;

State g_state = STATE_UNKNOWN_MODE;
static uint32_t g_debounceTimeout;

static void testTimeoutEvent(uint32_t &timeout, Event timeoutEvent);
static void stateTransition(Event event);
static void setState(State state);

int g_otgMode = USB_MODE_DISABLED;

////////////////////////////////////////////////////////////////////////////////

void init() {
    selectUsbMode(psu::persist_conf::getUsbMode(), g_otgMode);
}

void tick(uint32_t tickCount) {
#if defined(EEZ_PLATFORM_STM32)
    stateTransition(HAL_GPIO_ReadPin(USB_OTG_FS_OC_GPIO_Port, USB_OTG_FS_OC_Pin) ? EVENT_OTG_OC_HI : EVENT_OTG_OC_LOW);
    stateTransition(HAL_GPIO_ReadPin(USB_OTG_FS_ID_GPIO_Port, USB_OTG_FS_ID_Pin) ? EVENT_OTG_ID_HI : EVENT_OTG_ID_LOW);

    testTimeoutEvent(g_debounceTimeout, EVENT_DEBOUNCE_TIMEOUT);
#endif

#if defined(EEZ_PLATFORM_SIMULATOR)
    stateTransition(EVENT_OTG_OC_LOW);
    stateTransition(EVENT_OTG_ID_HI);

    testTimeoutEvent(g_debounceTimeout, EVENT_DEBOUNCE_TIMEOUT);
#endif
}

static void setTimeout(uint32_t &timeout, uint32_t timeoutDuration) {
    timeout = millis() + timeoutDuration;
    if (timeout == 0) {
        timeout = 1;
    }
}

static void clearTimeout(uint32_t &timeout) {
    timeout = 0;
}

static void testTimeoutEvent(uint32_t &timeout, Event timeoutEvent) {
    if (timeout && (int32_t)(millis() - timeout) >= 0) {
        clearTimeout(timeout);
        stateTransition(timeoutEvent);
    }
}

static void setState(State state) {
    if (state != g_state) {
        g_state = state;

        if (g_state == STATE_DEVICE_MODE) {
            if (g_usbMode == USB_MODE_OTG) {
                selectUsbMode(g_usbMode, USB_MODE_DEVICE);
            } else {
                g_otgMode = USB_MODE_DEVICE;
            }
        } else if (g_state == STATE_HOST_MODE) {
            if (g_usbMode == USB_MODE_OTG) {
                selectUsbMode(g_usbMode, USB_MODE_HOST);
            } else {
                g_otgMode = USB_MODE_HOST;
            }
        }
    }
}

static void stateTransition(Event event) {
    if (g_state == STATE_UNKNOWN_MODE) {
        if (event == EVENT_OTG_ID_LOW) {
            setState(STATE_DEBOUNCE_HOST_MODE);
            setTimeout(g_debounceTimeout, 1500);
        } else if (event == EVENT_OTG_ID_HI) {
            setState(STATE_DEBOUNCE_DEVICE_MODE);
            setTimeout(g_debounceTimeout, 1500);
        } else if (event == EVENT_OTG_OC_LOW) {
            setTimeout(g_debounceTimeout, CONF_OTG_ID_DEBOUNCE_TIMEOUT_MS);
            setState(STATE_FAULT_DETECTED);
        }
    } else if (g_state == STATE_DEVICE_MODE) {
        if (event == EVENT_OTG_ID_LOW) {
            setState(STATE_DEBOUNCE_HOST_MODE);
            setTimeout(g_debounceTimeout, CONF_OTG_ID_DEBOUNCE_TIMEOUT_MS);
        } else if (event == EVENT_OTG_OC_LOW) {
            setTimeout(g_debounceTimeout, CONF_OTG_ID_DEBOUNCE_TIMEOUT_MS);
            setState(STATE_FAULT_DETECTED);
        }
    } else if (g_state == STATE_HOST_MODE) {
        if (event == EVENT_OTG_ID_HI) {
            setState(STATE_DEBOUNCE_DEVICE_MODE);
            setTimeout(g_debounceTimeout, CONF_OTG_ID_DEBOUNCE_TIMEOUT_MS);
        } else if (event == EVENT_OTG_OC_LOW) {
            setTimeout(g_debounceTimeout, CONF_OTG_ID_DEBOUNCE_TIMEOUT_MS);
            setState(STATE_FAULT_DETECTED);
        }
    } else if (g_state == STATE_DEBOUNCE_DEVICE_MODE) {
        if (event == EVENT_OTG_ID_LOW) {
            setState(STATE_HOST_MODE);
            clearTimeout(g_debounceTimeout);
        } else if (event == EVENT_DEBOUNCE_TIMEOUT) {
            setState(STATE_DEVICE_MODE);
        } else if (event == EVENT_OTG_OC_LOW) {
            setTimeout(g_debounceTimeout, CONF_OTG_ID_DEBOUNCE_TIMEOUT_MS);
            setState(STATE_FAULT_DETECTED);
        }
    } else if (g_state == STATE_DEBOUNCE_HOST_MODE) {
        if (event == EVENT_OTG_ID_HI) {
            setState(STATE_DEVICE_MODE);
            clearTimeout(g_debounceTimeout);
        } else if (event == EVENT_DEBOUNCE_TIMEOUT) {
            setState(STATE_HOST_MODE);
        } else if (event == EVENT_OTG_OC_LOW) {
            setTimeout(g_debounceTimeout, CONF_OTG_ID_DEBOUNCE_TIMEOUT_MS);
            setState(STATE_FAULT_DETECTED);
        }
    } else if (g_state == STATE_FAULT_DETECTED) {
        if (event == EVENT_OTG_OC_LOW) {
            setTimeout(g_debounceTimeout, CONF_OTG_ID_DEBOUNCE_TIMEOUT_MS);
        } else if (event == EVENT_DEBOUNCE_TIMEOUT) {
            setState(STATE_HOST_MODE);
        }
    }
}

void selectUsbMode(int usbMode, int otgMode) {
    if (g_usbMode == usbMode && g_otgMode == otgMode) {
        return;
    }

#if defined(EEZ_PLATFORM_STM32)
    taskENTER_CRITICAL();

    if (g_usbMode == USB_MODE_DEVICE || (g_usbMode == USB_MODE_OTG && g_otgMode == USB_MODE_DEVICE)) {
        MX_USB_DEVICE_DeInit();
    } else if (g_usbMode == USB_MODE_HOST || (g_usbMode == USB_MODE_OTG && g_otgMode == USB_MODE_HOST)) {
        MX_USB_HOST_DeInit();
    }
#endif

#if defined(EEZ_PLATFORM_SIMULATOR)
    if (g_usbMode == USB_MODE_HOST || g_usbMode == USB_MODE_OTG) {
        SDL_ShowCursor(SDL_ENABLE);
        SDL_CaptureMouse(SDL_FALSE);
    }
#endif

    g_usbMode = usbMode;
    g_otgMode = otgMode;

#if defined(EEZ_PLATFORM_STM32)
    if (g_usbMode == USB_MODE_DEVICE || (g_usbMode == USB_MODE_OTG && g_otgMode == USB_MODE_DEVICE)) {
        MX_USB_DEVICE_Init();
    } else if (g_usbMode == USB_MODE_HOST || (g_usbMode == USB_MODE_OTG && g_otgMode == USB_MODE_HOST)) {
        MX_USB_HOST_Init();
    }

    taskEXIT_CRITICAL();
#endif

#if defined(EEZ_PLATFORM_SIMULATOR)
    if (g_usbMode == USB_MODE_HOST || g_usbMode == USB_MODE_OTG) {
        SDL_ShowCursor(SDL_DISABLE);
        SDL_CaptureMouse(SDL_TRUE);
    }
#endif

    psu::persist_conf::setUsbMode(g_usbMode);

    if (isVirtualComPortActive()) {
        psu::serial::initScpi();
    }

    psu::serial::g_testResult = g_usbMode != USB_MODE_DISABLED ? TEST_OK : TEST_SKIPPED;

    using namespace eez::gui;
    sendMessageToGuiThread(GUI_QUEUE_MESSAGE_MOUSE_DISCONNECTED);
}

void selectUsbDeviceClass(int usbDeviceClass) {
    if (g_usbDeviceClass == usbDeviceClass) {
        return;
    }
    
    if (g_usbMode == USB_MODE_DEVICE || (g_usbMode == USB_MODE_OTG && g_otgMode == USB_MODE_DEVICE)) {
#if defined(EEZ_PLATFORM_STM32)
        taskENTER_CRITICAL();

        MX_USB_DEVICE_DeInit();

        g_usbDeviceClass = usbDeviceClass;

        MX_USB_DEVICE_Init();

        taskEXIT_CRITICAL();
#endif

        if (g_usbDeviceClass == USB_DEVICE_CLASS_VIRTUAL_COM_PORT) {
            psu::serial::initScpi();
        }
    } else {
        g_usbDeviceClass = usbDeviceClass;
    }
}

bool isVirtualComPortActive() {
    return (g_usbMode == USB_MODE_DEVICE || (g_usbMode == USB_MODE_OTG && g_otgMode == USB_MODE_DEVICE)) && g_usbDeviceClass == USB_DEVICE_CLASS_VIRTUAL_COM_PORT;
}

bool isMassStorageActive() {
    return (g_usbMode == USB_MODE_DEVICE || (g_usbMode == USB_MODE_OTG && g_otgMode == USB_MODE_DEVICE)) && g_usbDeviceClass == USB_DEVICE_CLASS_MASS_STORAGE_CLIENT;
}

bool isOtgHostModeDetected() {
    return !HAL_GPIO_ReadPin(USB_OTG_FS_ID_GPIO_Port, USB_OTG_FS_ID_Pin);
}

} // usb
} // eez

int g_usbDeviceClass = USB_DEVICE_CLASS_VIRTUAL_COM_PORT;

#if defined(EEZ_PLATFORM_STM32)
extern "C" void USBH_HID_EventCallback(USBH_HandleTypeDef *phost) {
    auto deviceType = USBH_HID_GetDeviceType(phost);
	if (deviceType == HID_KEYBOARD) {
        eez::keyboard::onKeyboardEvent(phost);
	} if (deviceType == HID_MOUSE) {
        eez::mouse::onMouseEvent(phost);
	}
}
#endif
