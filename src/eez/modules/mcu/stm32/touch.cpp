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

#include <eez/modules/mcu/touch.h>
#include <stdint.h>
#include <i2c.h>
#include <eez/system.h>
#include <eez/debug.h>
#include <eez/gui/gui.h>

// TSC2007IPW - Touch Screen Controller

const uint16_t TOUCH_DEVICE_ADDRESS = 0x90;

namespace eez {
namespace mcu {
namespace touch {

static const int CONF_TOUCH_PRESSED_DEBOUNCE_TIMEOUT_MS = 10;
static const int CONF_TOUCH_NOT_PRESSED_DEBOUNCE_TIMEOUT_MS = 25;

static const int CONF_TOUCH_Z1_THRESHOLD = 50;

static const uint8_t  X_DATA_ID = 0b11000010;
static const uint8_t  Y_DATA_ID = 0b11010010;
static const uint8_t Z1_DATA_ID = 0b11100010;

enum State {
    STATE_NOT_PRESSED,
    STATE_PRESSED,
    STATE_DEBOUNCE_PRESSED,
    STATE_DEBOUNCE_NOT_PRESSED
};

enum Event {
    EVENT_PRESSED,
    EVENT_NOT_PRESSED,
    EVENT_DEBOUNCE_TIMEOUT
};

static State g_state;
static uint32_t g_debounceTimeout;

static int16_t g_lastXData = -1;
static int16_t g_lastYData = -1;
static int16_t g_lastZ1Data = 0;

void touchMeasure() {
    taskENTER_CRITICAL();
    
    uint8_t result[4];

    HAL_I2C_Master_Transmit(&hi2c1, TOUCH_DEVICE_ADDRESS, (uint8_t *)&Z1_DATA_ID, 1, 5);
    HAL_I2C_Master_Receive(&hi2c1, TOUCH_DEVICE_ADDRESS, result, 2, 5);

    g_lastZ1Data = (((int16_t)result[0]) << 3) | ((int16_t)result[1]);

    if (g_lastZ1Data > CONF_TOUCH_Z1_THRESHOLD) {
        HAL_I2C_Master_Transmit(&hi2c1, TOUCH_DEVICE_ADDRESS, (uint8_t *)&X_DATA_ID, 1, 5);
        HAL_I2C_Master_Receive(&hi2c1, TOUCH_DEVICE_ADDRESS, result, 2, 5);

        HAL_I2C_Master_Transmit(&hi2c1, TOUCH_DEVICE_ADDRESS, (uint8_t *)&Y_DATA_ID, 1, 5);
        HAL_I2C_Master_Receive(&hi2c1, TOUCH_DEVICE_ADDRESS, result + 2, 2, 5);
    }

    taskEXIT_CRITICAL();

    if (g_lastZ1Data > CONF_TOUCH_Z1_THRESHOLD) {
        g_lastXData  = (((int16_t)result[0]) << 3) | ((int16_t)result[1]);
        g_lastYData  = (((int16_t)result[2]) << 3) | ((int16_t)result[3]);
    }
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

static void setState(State state) {
    if (state != g_state) {
        g_state = state;
    }
}

static void stateTransition(Event event) {
    if (g_state == STATE_NOT_PRESSED) {
        if (event == EVENT_PRESSED) {
            setState(STATE_DEBOUNCE_PRESSED);
            setTimeout(g_debounceTimeout, CONF_TOUCH_PRESSED_DEBOUNCE_TIMEOUT_MS);
        }
    } else if (g_state == STATE_PRESSED) {
        if (event == EVENT_NOT_PRESSED) {
            setState(STATE_DEBOUNCE_NOT_PRESSED);
            setTimeout(g_debounceTimeout, CONF_TOUCH_NOT_PRESSED_DEBOUNCE_TIMEOUT_MS);
        }
    } else if (g_state == STATE_DEBOUNCE_PRESSED) {
        if (event == EVENT_NOT_PRESSED) {
            setState(STATE_NOT_PRESSED);
            clearTimeout(g_debounceTimeout);
        } else if (event == EVENT_DEBOUNCE_TIMEOUT) {
            setState(STATE_PRESSED);
        }
    } else if (g_state == STATE_DEBOUNCE_NOT_PRESSED) {
        if (event == EVENT_PRESSED) {
            setState(STATE_PRESSED);
            clearTimeout(g_debounceTimeout);
        } else if (event == EVENT_DEBOUNCE_TIMEOUT) {
            setState(STATE_NOT_PRESSED);
        }
    }
}

static void testTimeoutEvent(uint32_t &timeout, Event timeoutEvent) {
    if (timeout && (int32_t)(millis() - timeout) >= 0) {
        clearTimeout(timeout);
        stateTransition(timeoutEvent);
    }
}

void read(bool &isPressed, int &x, int &y) {
    touchMeasure();

    stateTransition(g_lastZ1Data > CONF_TOUCH_Z1_THRESHOLD ? EVENT_PRESSED : EVENT_NOT_PRESSED);

    testTimeoutEvent(g_debounceTimeout, EVENT_DEBOUNCE_TIMEOUT);

    isPressed = g_state == STATE_PRESSED;
    x = g_lastXData;
    y = g_lastYData;
}

} // namespace touch
} // namespace mcu

namespace gui {

using namespace mcu::touch;

void data_touch_raw_x(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = (int)g_lastXData;
    }
}

void data_touch_raw_y(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = (int)g_lastYData;
    }
}

void data_touch_raw_z1(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = (int)g_lastZ1Data;
    }
}

void data_touch_raw_pressed(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = g_state == STATE_PRESSED;
    }
}

} // namespace gui


} // namespace eez
