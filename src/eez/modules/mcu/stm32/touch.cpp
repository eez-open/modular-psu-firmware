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

const uint16_t TOUCH_DEVICE_ADDRESS = 0x90;

namespace eez {
namespace mcu {
namespace touch {

static const int CONF_TOUCH_DEBOUNCE_THRESHOLD = 25;

static const int CONF_TOUCH_Z1_THRESHOLD = 100;

static const uint8_t X_DATA_ID = 0b11000010;
static const uint8_t Y_DATA_ID = 0b11010010;
static const uint8_t Z1_DATA_ID = 0b11100010;

static bool g_wasPressed;
static uint32_t g_time;

static int16_t g_lastZ1Data = 0;
static int16_t g_lastYData = -1;
static int16_t g_lastXData = -1;

int16_t touchMeasure(uint8_t data) {
    taskENTER_CRITICAL();
    HAL_StatusTypeDef returnValue = HAL_I2C_Master_Transmit(&hi2c1, TOUCH_DEVICE_ADDRESS, &data, 1, 5);
    if (returnValue == HAL_OK) {
        uint8_t result[2];
        result[0] = 0;
        result[1] = 0;
        returnValue = HAL_I2C_Master_Receive(&hi2c1, TOUCH_DEVICE_ADDRESS, result, 2, 5);
        if (returnValue == HAL_OK) {
            taskEXIT_CRITICAL();
            int16_t value = (((int16_t)result[0]) << 3) | ((int16_t)result[1]);

            if (data == X_DATA_ID) {
                g_lastXData = value;
            } else if (data == Y_DATA_ID) {
                g_lastYData = value;
            } else {
                g_lastZ1Data = value;
            }

            return value;
        }
    }
    taskEXIT_CRITICAL();

    if (data == X_DATA_ID) {
        return g_lastXData;
    } else if (data == Y_DATA_ID) {
        return g_lastYData;
    } else {
        return g_lastZ1Data;
    }
}

void read(bool &isPressed, int &x, int &y) {
    bool isPressedNow = touchMeasure(Z1_DATA_ID) > CONF_TOUCH_Z1_THRESHOLD;
    uint32_t now = millis();

    if (g_wasPressed != isPressedNow) {
        if (now - g_time > CONF_TOUCH_DEBOUNCE_THRESHOLD) {
            g_wasPressed = isPressedNow;
            g_time = now;
        }
    } else {
        g_time = now;
    }

    if (g_wasPressed) {
        isPressed = true;
        if (isPressedNow) {
        	x = touchMeasure(X_DATA_ID);
        	y = touchMeasure(Y_DATA_ID);
        }
    } else {
        isPressed = false;
    }
}

} // namespace touch
} // namespace mcu
} // namespace eez
