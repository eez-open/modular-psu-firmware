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
#include <eez/modules/mcu/button.h>
#endif

namespace eez {
namespace mcu {
namespace encoder {

#if defined(EEZ_PLATFORM_STM32)
static Button g_encoderSwitch(ENC_SW_GPIO_Port, ENC_SW_Pin);
static uint16_t g_lastCounter;
#endif	

#if defined(EEZ_PLATFORM_SIMULATOR)
int g_simulatorCounter;
bool g_simulatorClicked;
#endif	

// ignore counter change when direction is changed if tick difference is less then this number
#ifdef EEZ_PLATFORM_SIMULATOR
#define DIRECTION_CHANGE_DIFF_TICK_THRESHOLD 100
#else
#define DIRECTION_CHANGE_DIFF_TICK_THRESHOLD 50
#endif

static bool g_accelerationEnabled = true;
static int g_speedDown;
static int g_speedUp;
static float g_acceleration;
static uint32_t g_lastTick;
static int g_direction;

EncoderMode g_encoderMode = ENCODER_MODE_AUTO;

void init() {
#if defined(EEZ_PLATFORM_STM32)	
	HAL_TIM_Encoder_Start(&htim8, TIM_CHANNEL_ALL);
	g_lastCounter = __HAL_TIM_GET_COUNTER(&htim8);
#endif

    g_lastTick = millis();
}

int getCounter() {
#if defined(EEZ_PLATFORM_SIMULATOR)
    int16_t deltaCounter = g_simulatorCounter;
    g_simulatorCounter = 0;
#endif

#if defined(EEZ_PLATFORM_STM32)
    uint16_t counter = __HAL_TIM_GET_COUNTER(&htim8);
    int16_t deltaCounter = counter - g_lastCounter;
    g_lastCounter = counter;
#endif

#define CONF_ENCODER_ACCELERATION_INCREMENT_FACTOR 1.5f
#define CONF_ENCODER_ACCELERATION_DECREMENT_PER_MS 1.0f

    float acceleratedCounter = 0;

    if (deltaCounter != 0) {
        uint32_t tick = millis();
        float diffTick = 1.0f * (tick - g_lastTick);
        g_lastTick = tick;

        if (deltaCounter > 0) {
			if (g_direction != 1) {
				g_direction = 1;
				if (diffTick < DIRECTION_CHANGE_DIFF_TICK_THRESHOLD) {
					deltaCounter = 0;
				}
			}
		} else {
			if (g_direction != -1) {
				g_direction = -1;
				if (diffTick < DIRECTION_CHANGE_DIFF_TICK_THRESHOLD) {
					deltaCounter = 0;
				}
			}
		}        

        if (deltaCounter != 0) {
            diffTick = diffTick / (g_direction * deltaCounter);
            //float maxDiffTick = CONF_ENCODER_ACCELERATION_INCREMENT_FACTOR * MAX(g_speedUp, g_speedDown) / CONF_ENCODER_ACCELERATION_DECREMENT_PER_MS;
            //diffTick = MIN(diffTick, maxDiffTick);

            int speed = g_direction ? g_speedUp : g_speedDown;

            float accelerationIncrement = -CONF_ENCODER_ACCELERATION_DECREMENT_PER_MS * diffTick + CONF_ENCODER_ACCELERATION_INCREMENT_FACTOR * speed;

            for (int i = 0; i < g_direction * deltaCounter; i++) {
                if (g_accelerationEnabled && g_encoderMode == ENCODER_MODE_AUTO) {
                    g_acceleration += accelerationIncrement;
                    if (g_acceleration < 0) g_acceleration = 0;
                    if (g_acceleration > 99) g_acceleration = 99;
                } else {
                    g_acceleration = 0;
                }

                acceleratedCounter += g_direction * (1 + g_acceleration);
            }
        }
    }

    return (int)roundf(acceleratedCounter);
}

bool isButtonClicked() {
#if defined(EEZ_PLATFORM_SIMULATOR)
    return g_simulatorClicked;
#endif

#if defined(EEZ_PLATFORM_STM32)
    return g_encoderSwitch.isClicked();
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

void switchEncoderMode() {
    if (g_encoderMode == ENCODER_MODE_STEP5) {
        g_encoderMode = ENCODER_MODE_AUTO;
    } else {
        g_encoderMode = EncoderMode(g_encoderMode + 1);
    }
}

#if defined(EEZ_PLATFORM_SIMULATOR)
void write(int counter, bool clicked) {
	g_simulatorCounter = counter;
	g_simulatorClicked = clicked;
}
#endif


} // namespace encoder
} // namespace mcu
} // namespace eez

#endif
