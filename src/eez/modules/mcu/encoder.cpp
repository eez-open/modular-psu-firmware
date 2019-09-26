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
#define CONF_ENCODER_SPEED_FACTOR 1.0
#else
#define CONF_ENCODER_SPEED_FACTOR 0.5f
#endif

#if defined(EEZ_PLATFORM_STM32)
static Button g_encoderSwitch(ENC_SW_GPIO_Port, ENC_SW_Pin, true);
static uint16_t g_lastCounter;
#endif	

#if defined(EEZ_PLATFORM_SIMULATOR)
int g_simulatorCounter;
bool g_simulatorClicked;
#endif	

static float g_accumulatedCounter;

EncoderMode g_encoderMode = ENCODER_MODE_AUTO;

////////////////////////////////////////////////////////////////////////////////

#define CONF_NUM_ACCELERATION_STEP_COUNTERS 10
#define CONF_ACCELERATION_STEP_COUNTER_DURATION_MS 50
#define CONF_MAX_ACCELERATION 99

#if defined(EEZ_PLATFORM_STM32)
#define CONF_ACCELERATION_CALC_POW_BASE 1.06f
#else
#define CONF_ACCELERATION_CALC_POW_BASE 1.05f
#endif

static uint32_t g_accelerationStepLastTick;
static int g_accumulatedAccelerationStepCounter;
static int g_accelerationStepCounters[CONF_NUM_ACCELERATION_STEP_COUNTERS];
static unsigned int g_accelerationStepCounterIndex;
static float g_acceleration;

void resetAccelerationCalculation() {
    g_accelerationStepLastTick = millis();
    g_accumulatedAccelerationStepCounter = 0;
    for (int i = 0; i < CONF_NUM_ACCELERATION_STEP_COUNTERS; i++) {
        g_accelerationStepCounters[i] = 0;
    }
    g_accelerationStepCounterIndex = 0;
    g_acceleration = 0;
}

void calculateAcceleration(int16_t diffCounter) {
    g_accumulatedAccelerationStepCounter += diffCounter;

    uint32_t tick = millis();
    int32_t diffTick = tick - g_accelerationStepLastTick;
    if (diffTick >= CONF_ACCELERATION_STEP_COUNTER_DURATION_MS) {
        g_accelerationStepCounters[g_accelerationStepCounterIndex % CONF_NUM_ACCELERATION_STEP_COUNTERS] = g_accumulatedAccelerationStepCounter;

        int stepCountersSum = 0;
        for (int i = 0; i < CONF_NUM_ACCELERATION_STEP_COUNTERS; i++) {
        	stepCountersSum += g_accelerationStepCounters[(g_accelerationStepCounterIndex - i) % CONF_NUM_ACCELERATION_STEP_COUNTERS];
        }

		g_acceleration = powf(CONF_ACCELERATION_CALC_POW_BASE, fabs(1.0f * stepCountersSum)) - 1;

        if (g_acceleration > CONF_MAX_ACCELERATION) g_acceleration = CONF_MAX_ACCELERATION;

//        if (g_acceleration > 0) {
//        	DebugTrace("%g\n", g_acceleration);
//        }

        g_accelerationStepLastTick = tick;
        g_accumulatedAccelerationStepCounter = 0;
        g_accelerationStepCounterIndex++;
    }
}

////////////////////////////////////////////////////////////////////////////////

void init() {
#if defined(EEZ_PLATFORM_STM32)
	HAL_TIM_Encoder_Start(&htim8, TIM_CHANNEL_ALL);
	g_lastCounter = __HAL_TIM_GET_COUNTER(&htim8);
#endif

    resetAccelerationCalculation();
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

    // update acceleration
    calculateAcceleration(diffCounter);

    //
    g_accumulatedCounter += diffCounter;
    float speed = 1.0f + (MAX_MOVING_SPEED - (g_accumulatedCounter > 0 ? psu::persist_conf::devConf2.encoderMovingSpeedUp : psu::persist_conf::devConf2.encoderMovingSpeedDown)) * CONF_ENCODER_SPEED_FACTOR;
    int result = (int)(g_accumulatedCounter > 0 ? floorf(g_accumulatedCounter / speed) : ceilf(g_accumulatedCounter / speed));
    g_accumulatedCounter -= result * speed;

    return result;
}

void resetEncoder() {
    resetAccelerationCalculation();
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

    return clamp(newValue, min, max);
}

} // namespace encoder
} // namespace mcu
} // namespace eez

#endif
