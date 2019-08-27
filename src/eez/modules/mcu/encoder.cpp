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

#include <eez/apps/psu/psu.h>
#include <eez/apps/psu/channel.h>
#include <eez/apps/psu/persist_conf.h>
#include <eez/apps/psu/gui/edit_mode_step.h>

namespace eez {
namespace mcu {
namespace encoder {

#if defined(EEZ_PLATFORM_STM32)
static Button g_encoderSwitch(ENC_SW_GPIO_Port, ENC_SW_Pin, true);
static uint16_t g_lastCounter;
#endif	

#if defined(EEZ_PLATFORM_SIMULATOR)
int g_simulatorCounter;
bool g_simulatorClicked;
#endif	

#define CONF_INTERVAL 50

static uint32_t g_lastTick;
static uint32_t g_lastIntervalTick;
static int g_direction;
static float g_deltaCounter;
static float g_nextDeltaCounter;

EncoderMode g_encoderMode = ENCODER_MODE_AUTO;

static float g_acceleration;

void init() {
#if defined(EEZ_PLATFORM_STM32)	
	HAL_TIM_Encoder_Start(&htim8, TIM_CHANNEL_ALL);
	g_lastCounter = __HAL_TIM_GET_COUNTER(&htim8);
#endif

    g_lastTick = millis();
    g_lastIntervalTick = millis();
}

int getCounter() {
#if defined(EEZ_PLATFORM_SIMULATOR)
    int16_t diffCounter = g_simulatorCounter;
    g_simulatorCounter = 0;
#endif

#if defined(EEZ_PLATFORM_STM32)
    uint16_t counter = __HAL_TIM_GET_COUNTER(&htim8);
    int16_t diffCounter = counter - g_lastCounter;
    g_lastCounter = counter;
#endif

    g_nextDeltaCounter += diffCounter;

#define CONF_ENCODER_ACCELERATION_INCREMENT_FACTOR 1.5f
#define CONF_ENCODER_ACCELERATION_DECREMENT_PER_MS 1.0f

    uint32_t tick = millis();
    int32_t diffTick = tick - g_lastTick;
    g_lastTick = tick;

    g_acceleration += ((diffCounter > 0 ? CONF_ENCODER_ACCELERATION_INCREMENT_FACTOR : 0) - CONF_ENCODER_ACCELERATION_DECREMENT_PER_MS) * diffTick;
    if (g_acceleration < 0) g_acceleration = 0;
    if (g_acceleration > 99) g_acceleration = 99;
    
    DebugTrace("%g\n", g_acceleration);

    diffTick = tick - g_lastIntervalTick;
    if (diffTick < CONF_INTERVAL) {
        return 0;
    }
    g_lastIntervalTick = tick;

    float speed = 1.0f * (g_deltaCounter > 0 ? psu::persist_conf::devConf2.encoderMovingSpeedUp : psu::persist_conf::devConf2.encoderMovingSpeedDown) / MAX_MOVING_SPEED;

    int deltaCounter = (int)(g_deltaCounter > 0 ? floorf(g_deltaCounter * speed) : ceilf(g_deltaCounter * speed));
    g_deltaCounter = g_nextDeltaCounter + (g_deltaCounter - deltaCounter / speed);
    g_nextDeltaCounter = 0;

    if (deltaCounter != 0) {
        if (deltaCounter > 0) {
			if (g_direction == -1) {
				g_direction = 1;
                deltaCounter = 0;
			}
		} else {
			if (g_direction == 1) {
				g_direction = -1;
                deltaCounter = 0;
			}
		}        
    } else {
        g_direction = 0;
    }

    return deltaCounter;
}

void resetEncoder() {
    g_deltaCounter = 0;
    g_nextDeltaCounter = 0;
    g_lastTick = millis() + CONF_INTERVAL;
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
    clicked = isButtonClicked();
    if (clicked) {
        resetEncoder();
    }
    counter = getCounter();
}

#if defined(EEZ_PLATFORM_SIMULATOR)
void write(int counter, bool clicked) {
	g_simulatorCounter = counter;
	g_simulatorClicked = clicked;
}
#endif

void switchEncoderMode() {
    if (g_encoderMode == ENCODER_MODE_STEP5) {
        g_encoderMode = ENCODER_MODE_AUTO;
    } else {
        g_encoderMode = EncoderMode(g_encoderMode + 1);
    }
}

float increment(gui::data::Value value, int counter, float min, float max, int channelIndex, float precision) {
    float newValue;
    if (g_encoderMode == ENCODER_MODE_AUTO) {
        float factor;

        if (channelIndex != -1) {
            factor = psu::Channel::get(channelIndex).getValuePrecision(value.getUnit(), value.getFloat());
        } else {
            factor = precision;
        }

        newValue = value.getFloat() + factor * (1 + g_acceleration) * counter;
    } else {
        newValue = value.getFloat() + psu::gui::edit_mode_step::getCurrentEncoderStepValue().getFloat() * counter;
    }

    if (channelIndex != -1) {
        newValue = psu::Channel::get(channelIndex).roundChannelValue(value.getUnit(), newValue);
    } else {
        newValue = roundPrec(newValue, precision);
    }

    if (g_encoderMode == ENCODER_MODE_AUTO) {
        float diff = fabs(newValue - value.getFloat());
        if (diff > 1) {
            newValue = roundPrec(newValue, 1);
        } else if (diff > 0.1) {
            newValue = roundPrec(newValue, 0.1f);
        } else if (diff > 0.01) {
            newValue = roundPrec(newValue, 0.01f);
        } else if (diff > 0.001) {
            newValue = roundPrec(newValue, 0.001f);
        } else if (diff > 0.0001) {
            newValue = roundPrec(newValue, 0.0001f);
        }
    }

    if (newValue < min) {
        newValue = min;
    }

    if (newValue > max) {
        newValue = max;
    }

    return newValue;
}

} // namespace encoder
} // namespace mcu
} // namespace eez

#endif
