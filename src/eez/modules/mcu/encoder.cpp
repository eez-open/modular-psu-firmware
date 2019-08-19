/*
* EEZ Middleware
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

#include <stdint.h>
#include <math.h>

#include <eez/system.h>
#include <eez/debug.h>
#include <eez/util.h>

#if OPTION_ENCODER

#include <eez/modules/mcu/encoder.h>

#if defined(EEZ_PLATFORM_STM32)	
#include <tim.h>
#include <gpio.h>
#endif

namespace eez {
namespace mcu {
namespace encoder {

#if defined(EEZ_PLATFORM_SIMULATOR)
int g_counter;
bool g_clicked;
#endif	

uint32_t g_lastTick;

#if defined(EEZ_PLATFORM_STM32)
uint16_t g_lastCounter;
GPIO_PinState g_buttonPinState = GPIO_PIN_RESET;
uint32_t g_buttonPinStateTick = 0;
#define CONF_DEBOUNCE_THRESHOLD_TIME 10 // 10 ms
int g_btnIsDown = 0;
#endif	

// ignore counter change when direction is changed if tick difference is less then this number
#ifdef EEZ_PLATFORM_SIMULATOR
#define DIRECTION_CHANGE_DIFF_TICK_THRESHOLD 100
#else
#define DIRECTION_CHANGE_DIFF_TICK_THRESHOLD 50
#endif

// acceleration is enabled only if tick difference is less then this number
#define ACCELERATION_DIFF_TICK_THRESHOLD 50

#ifdef EEZ_PLATFORM_SIMULATOR
#define SPEED_FACTOR 10.0f
#else
#define SPEED_FACTOR 2.0f
#endif

bool g_accelerationEnabled = true;
int g_speedDown;
int g_speedUp;

void init() {
#if defined(EEZ_PLATFORM_STM32)	
	HAL_TIM_Encoder_Start(&htim8, TIM_CHANNEL_ALL);
	g_lastCounter = __HAL_TIM_GET_COUNTER(&htim8);
	g_lastTick = HAL_GetTick();
#endif
}

int g_direction = 0;

int getCounter() {
#if defined(EEZ_PLATFORM_SIMULATOR)
    int16_t diffCounter = g_counter;
#endif

#if defined(EEZ_PLATFORM_STM32)
    uint16_t previousCounter = g_lastCounter;
    g_lastCounter = __HAL_TIM_GET_COUNTER(&htim8);
    int16_t diffCounter = g_lastCounter - previousCounter;
#endif

    if (diffCounter != 0) {
		uint32_t previousTick = g_lastTick;
		g_lastTick = millis();
		float diffTick = 1.0f * (g_lastTick - previousTick) / diffCounter;

		if (diffCounter > 0) {
			if (g_direction != 1) {
				g_direction = 1;
				if (diffTick < DIRECTION_CHANGE_DIFF_TICK_THRESHOLD) {
					diffCounter = 0;
				}
			}
		} else {
			diffCounter = -diffCounter;
			diffTick = -diffTick;

			if (g_direction != -1) {
				g_direction = -1;
				if (diffTick < DIRECTION_CHANGE_DIFF_TICK_THRESHOLD) {
					diffCounter = 0;
				}
			}
		}

		if (diffCounter > 0) {
			// DebugTrace("C=%d, T=%g\n", (int)diffCounter, diffTick);

			int counter;

			if (g_accelerationEnabled && diffTick < ACCELERATION_DIFF_TICK_THRESHOLD) {
				uint8_t speed = g_direction == 1 ? g_speedUp : g_speedDown;
				float speedFactor = SPEED_FACTOR * (speed - MIN_MOVING_SPEED) / (MAX_MOVING_SPEED - MIN_MOVING_SPEED);
                counter = (int)roundf(diffCounter * speedFactor * ACCELERATION_DIFF_TICK_THRESHOLD / diffTick);
			} else {
				counter = diffCounter;
			}

			return counter * g_direction;
		}
    }

    return 0;
}

bool isButtonClicked() {
#if defined(EEZ_PLATFORM_SIMULATOR)
    return g_clicked;
#endif

#if defined(EEZ_PLATFORM_STM32)
    if (HAL_GPIO_ReadPin(ENC_SW_GPIO_Port, ENC_SW_Pin) == GPIO_PIN_RESET) {
    	// button is DOWN
    	if (!g_btnIsDown) {
			if (g_buttonPinState != GPIO_PIN_RESET) {
				g_buttonPinState = GPIO_PIN_RESET;
				g_buttonPinStateTick = HAL_GetTick();
			} else {
				int32_t diff = HAL_GetTick() - g_buttonPinStateTick;
				if (diff > CONF_DEBOUNCE_THRESHOLD_TIME) {
					g_btnIsDown = true;
					return true;
				}
			}
    	}
    } else {
    	// button is UP
    	if (g_btnIsDown) {
			if (g_buttonPinState) {
				g_buttonPinState = GPIO_PIN_RESET;
				g_buttonPinStateTick = HAL_GetTick();
			} else {
				int32_t diff = HAL_GetTick() - g_buttonPinStateTick;
				if (diff > CONF_DEBOUNCE_THRESHOLD_TIME) {
					g_btnIsDown = false;
				}
			}
    	}
    }

    return false;
#endif
}

void read(int &counter, bool &clicked) {
    counter = getCounter();
    clicked = isButtonClicked();
}

void enableAcceleration(bool enable) {
    if (g_accelerationEnabled != enable) {
        g_accelerationEnabled = enable;
    }
}

void setMovingSpeed(uint8_t down, uint8_t up) {
    g_speedDown = down;
    g_speedUp = up;
}

#if defined(EEZ_PLATFORM_SIMULATOR)
void write(int counter, bool clicked) {
	g_counter = counter;
	g_clicked = clicked;
}
#endif


} // namespace encoder
} // namespace mcu
} // namespace eez

#endif
