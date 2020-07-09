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

#if defined(EEZ_PLATFORM_STM32)
#include <gpio.h>
#include <usb_device.h>
#include <usb_host.h>
#include <usbh_hid.h>
#endif

#include <eez/firmware.h>
#include <eez/modules/psu/psu.h>
#include <eez/modules/psu/serial_psu.h>

namespace eez {

using namespace scpi;
    
namespace psu {

using namespace scpi;

namespace serial {

TestResult g_testResult = TEST_FAILED;

size_t SCPI_Write(scpi_t *context, const char *data, size_t len) {
    Serial.write(data, len);
    return len;
}

scpi_result_t SCPI_Flush(scpi_t *context) {
    return SCPI_RES_OK;
}

int SCPI_Error(scpi_t *context, int_fast16_t err) {
    if (err != 0) {
        scpi::printError(err);

        if (err == SCPI_ERROR_INPUT_BUFFER_OVERRUN) {
            scpi::onBufferOverrun(*context);
        }
    }

    return 0;
}

scpi_result_t SCPI_Control(scpi_t *context, scpi_ctrl_name_t ctrl, scpi_reg_val_t val) {
    if (serial::g_testResult == TEST_OK) {
        char errorOutputBuffer[256];
        if (SCPI_CTRL_SRQ == ctrl) {
            sprintf(errorOutputBuffer, "**SRQ: 0x%X (%d)\r\n", val, val);
        } else {
            sprintf(errorOutputBuffer, "**CTRL %02x: 0x%X (%d)\r\n", ctrl, val, val);
        }
        Serial.println(errorOutputBuffer);
    }

    return SCPI_RES_OK;
}

scpi_result_t SCPI_Reset(scpi_t *context) {
    if (serial::g_testResult == TEST_OK) {
        char errorOutputBuffer[256];
        strcpy(errorOutputBuffer, "**Reset\r\n");
        Serial.println(errorOutputBuffer);
    }

    return reset() ? SCPI_RES_OK : SCPI_RES_ERR;
}

////////////////////////////////////////////////////////////////////////////////

static scpi_reg_val_t g_scpiPsuRegs[SCPI_PSU_REG_COUNT];
static scpi_psu_t g_scpiPsuContext = { g_scpiPsuRegs };

static scpi_interface_t g_scpiInterface = {
    SCPI_Error, SCPI_Write, SCPI_Control, SCPI_Flush, SCPI_Reset,
};

static char g_scpiInputBuffer[SCPI_PARSER_INPUT_BUFFER_LENGTH];
static scpi_error_t g_errorQueueData[SCPI_PARSER_ERROR_QUEUE_SIZE + 1];

scpi_t g_scpiContext;

static bool g_isConnected;

static void initScpi();

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
    g_testResult = TEST_SKIPPED;
	initScpi();

    selectUsbMode(persist_conf::getUsbMode(), g_otgMode);

#ifdef EEZ_PLATFORM_SIMULATOR
    if (isVirtualComPortActive()) {
        Serial.print("EEZ BB3 software simulator ver. ");
        Serial.println(MCU_FIRMWARE);
    }
#endif
}

void tick(uint32_t tickCount) {
    stateTransition(HAL_GPIO_ReadPin(USB_OTG_FS_OC_GPIO_Port, USB_OTG_FS_OC_Pin) ? EVENT_OTG_OC_HI : EVENT_OTG_OC_LOW);
    stateTransition(HAL_GPIO_ReadPin(USB_OTG_FS_ID_GPIO_Port, USB_OTG_FS_ID_Pin) ? EVENT_OTG_ID_HI : EVENT_OTG_ID_LOW);

    testTimeoutEvent(g_debounceTimeout, EVENT_DEBOUNCE_TIMEOUT);
}

void onQueueMessage(uint32_t type, uint32_t param) {
    if (type == SERIAL_LINE_STATE_CHANGED) {
        g_isConnected = param ? true : false;
        if (isConnected()) {
            scpi::emptyBuffer(g_scpiContext);
        }
    } else if (type == SERIAL_INPUT_AVAILABLE) {
        uint8_t *buffer;
        uint32_t length;
        Serial.getInputBuffer(param, &buffer, &length);
        if (g_testResult == TEST_OK) {
            input(g_scpiContext, (const char *)buffer, length);
        }
        Serial.releaseInputBuffer();
    }
}

bool isConnected() {
    return g_testResult == TEST_OK && g_isConnected;
}

static void initScpi() {
    scpi::init(g_scpiContext, g_scpiPsuContext, &g_scpiInterface, g_scpiInputBuffer, SCPI_PARSER_INPUT_BUFFER_LENGTH, g_errorQueueData, SCPI_PARSER_ERROR_QUEUE_SIZE + 1);
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

    persist_conf::setUsbMode(g_usbMode);

    if ((g_usbMode == USB_MODE_DEVICE || (g_usbMode == USB_MODE_OTG && g_otgMode == USB_MODE_DEVICE)) && g_usbDeviceClass == USB_DEVICE_CLASS_VIRTUAL_COM_PORT) {
        initScpi();
    }

    g_testResult = g_usbMode != USB_MODE_DISABLED ? TEST_OK : TEST_SKIPPED;
}

void selectUsbDeviceClass(int usbDeviceClass) {
    if (g_usbDeviceClass == usbDeviceClass) {
        return;
    }
    
    if (g_usbMode == USB_MODE_DEVICE || (g_usbMode == USB_MODE_OTG && g_otgMode == USB_MODE_DEVICE)) {
        taskENTER_CRITICAL();

        MX_USB_DEVICE_DeInit();

        g_usbDeviceClass = usbDeviceClass;

        MX_USB_DEVICE_Init();

        taskEXIT_CRITICAL();

        if (g_usbDeviceClass == USB_DEVICE_CLASS_VIRTUAL_COM_PORT) {
            initScpi();
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

} // namespace serial
} // namespace psu
} // namespace eez

int g_usbDeviceClass = USB_DEVICE_CLASS_VIRTUAL_COM_PORT;

uint8_t g_keyboardState;
uint8_t g_keyboardLCtrl;
uint8_t g_keyboardLShift;
uint8_t g_keyboardLAlt;
uint8_t g_keyboardLGui;
uint8_t g_keyboardRCtrl;
uint8_t g_keyboardRShift;
uint8_t g_keyboardRAlt;
uint8_t g_keyboardRGui;
uint8_t g_keyboardKeys[6];

extern "C" void USBH_HID_EventCallback(USBH_HandleTypeDef *phost) {
	HID_HandleTypeDef *HID_Handle = (HID_HandleTypeDef *)phost->pActiveClass->pData;
	if (HID_Handle->Init == USBH_HID_KeybdInit) {
		HID_KEYBD_Info_TypeDef *info = USBH_HID_GetKeybdInfo(phost);

		g_keyboardState = info->state;

		g_keyboardLCtrl = info->lctrl;
		g_keyboardLShift = info->lshift;
		g_keyboardLAlt = info->lalt;
		g_keyboardLGui = info->lgui;

		g_keyboardRCtrl = info->rctrl;
		g_keyboardRShift = info->rshift;
		g_keyboardRAlt = info->ralt;
		g_keyboardRGui = info->rgui;

		memcpy(g_keyboardKeys, info->keys, 6);
	}
}
