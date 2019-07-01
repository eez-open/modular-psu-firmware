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

#if defined(EEZ_PLATFORM_STM32)
uint16_t g_lastCounter;
uint32_t g_lastTick;

GPIO_PinState g_buttonPinState = GPIO_PIN_RESET;
uint32_t g_buttonPinStateTick = 0;
#define CONF_DEBOUNCE_THRESHOLD_TIME 10 // 10 ms
int g_btnIsDown = 0;

#define CONF_ENCODER_ACCELERATION_DECREMENT_PER_MS 1
#define CONF_ENCODER_ACCELERATION_INCREMENT_FACTOR 2

#define CONF_ENCODER_SWITCH_DURATION_MIN 5000
#define CONF_ENCODER_SWITCH_DURATION_MAX 1000000
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

#if defined(EEZ_PLATFORM_STM32)

int g_direction = 0;
float g_acc = 0;

int getCounter() {
    uint16_t currentCounter = __HAL_TIM_GET_COUNTER(&htim8);
    int16_t diffCounter = currentCounter - g_lastCounter;
    g_lastCounter = currentCounter;

	uint32_t previousTick = g_lastTick;
	g_lastTick = HAL_GetTick();

	if (diffCounter > 0) {
		if (g_direction != 1) {
			g_direction = 1;
			g_acc = 0;
		}
	} else {
		if (g_direction != -1) {
			g_direction = -1;
			g_acc = 0;
		}
		diffCounter = -diffCounter;
	}

	if (diffCounter > 0) {
		static const float A1 = 0.125;
		static const float B1 = 1.75;

		static const float A2 = 0.75;
		static const float B2 = 100;

		float speed = 1.0f * diffCounter / (g_lastTick - previousTick);
		if (speed < A1) speed = A1;
		if (speed > B1) speed = B1;

		float t = (speed - A1) / (B1 - A1);

		g_acc += A2 + (t * t) * (B2 - A2);

		int counter = (int)g_acc;

		g_acc -= counter;

		return counter * g_direction;
	}

	g_direction = 0;
    return 0;
}

bool isButtonClicked() {
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
}
#endif

void read(uint32_t tickCount, int &counter, bool &clicked) {
#if defined(EEZ_PLATFORM_SIMULATOR)		
    counter = g_counter;
    clicked = g_clicked;
#endif

#if defined(EEZ_PLATFORM_STM32)		
    counter = getCounter();
    clicked = isButtonClicked();
#endif

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
