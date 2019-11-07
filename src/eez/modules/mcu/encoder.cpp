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

#if OPTION_DISPLAY

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

#include <eez/modules/psu/psu.h>
#include <eez/modules/psu/persist_conf.h>
#include <eez/modules/psu/gui/edit_mode_step.h>

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

static bool g_useSameSpeed = false;

////////////////////////////////////////////////////////////////////////////////

struct CalcAutoModeStepLevel {
    static const int CONF_NUM_STEP_LEVELS = 3; // 0, 1 and 2
    static const int CONF_NUM_ACCELERATION_STEP_COUNTERS = 10;
    static const int CONF_ACCELERATION_STEP_COUNTER_DURATION_MS = 50;
    static const int CONF_COUNTER_TO_STEP_DIVISOR = 20;

    uint32_t m_lastTick;
    int m_accumulatedCounter;
    int m_counters[CONF_NUM_ACCELERATION_STEP_COUNTERS];
    unsigned int m_counterIndex;
    int m_stepLevel;

    void reset() {
        m_lastTick = millis();
        m_accumulatedCounter = 0;
        for (int i = 0; i < CONF_NUM_ACCELERATION_STEP_COUNTERS; i++) {
            m_counters[i] = 0;
        }
        m_counterIndex = 0;
        m_stepLevel = 0;
    }

    void update(int16_t diffCounter) {
        m_accumulatedCounter += diffCounter;

        uint32_t tick = millis();
        int32_t diffTick = tick - m_lastTick;
        if (diffTick >= CONF_ACCELERATION_STEP_COUNTER_DURATION_MS) {
            m_counters[m_counterIndex % CONF_NUM_ACCELERATION_STEP_COUNTERS] = m_accumulatedCounter;

            int stepCountersSum = 0;
            for (int i = 0; i < CONF_NUM_ACCELERATION_STEP_COUNTERS; i++) {
                stepCountersSum += m_counters[(m_counterIndex - i) % CONF_NUM_ACCELERATION_STEP_COUNTERS];
            }

            stepCountersSum = abs(stepCountersSum);

            m_stepLevel = MIN(stepCountersSum / CONF_COUNTER_TO_STEP_DIVISOR, CONF_NUM_STEP_LEVELS - 1);

            //if (stepCountersSum != 0) {
            //    DebugTrace("%d, %d\n", stepCountersSum, m_stepLevel);
            //}

            m_lastTick = tick;
            m_accumulatedCounter = 0;
            m_counterIndex++;
        }
    }

    int stepLevel() {
        return m_stepLevel;
    }
};

static CalcAutoModeStepLevel g_calcAutoModeStepLevel;

////////////////////////////////////////////////////////////////////////////////

void init() {
#if defined(EEZ_PLATFORM_STM32)
	HAL_TIM_Encoder_Start(&htim8, TIM_CHANNEL_ALL);
	g_lastCounter = __HAL_TIM_GET_COUNTER(&htim8);
#endif

    g_calcAutoModeStepLevel.reset();
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
    g_calcAutoModeStepLevel.update(diffCounter);

    //
    g_accumulatedCounter += diffCounter;
    float speed = 1.0f + (MAX_MOVING_SPEED - (g_useSameSpeed ? MAX(psu::persist_conf::devConf.encoderMovingSpeedUp, psu::persist_conf::devConf.encoderMovingSpeedDown) : g_accumulatedCounter > 0 ? psu::persist_conf::devConf.encoderMovingSpeedUp : psu::persist_conf::devConf.encoderMovingSpeedDown)) * CONF_ENCODER_SPEED_FACTOR;
    int result = (int)(g_accumulatedCounter > 0 ? floorf(g_accumulatedCounter / speed) : ceilf(g_accumulatedCounter / speed));
    g_accumulatedCounter -= result * speed;

    return result;
}

void resetEncoder() {
    g_calcAutoModeStepLevel.reset();
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

void setUseSameSpeed(bool enable) {
    g_useSameSpeed = enable;
}

float increment(gui::data::Value value, int counter, float min, float max, int channelIndex, float precision) {
    if (channelIndex != -1) {
        precision = psu::Channel::get(channelIndex).getValuePrecision(value.getUnit(), value.getFloat());
    }

    // TODO 
    if (precision == 0) {
        precision = 0.001f;
    }

    float step;

    if (g_encoderMode == ENCODER_MODE_AUTO) {
        step = precision * powf(10.0f, 1.0f * g_calcAutoModeStepLevel.stepLevel());
    } else {
        step = psu::gui::edit_mode_step::getCurrentEncoderStepValue().getFloat(); 
    }

    float newValue = value.getFloat() + step * counter;
    newValue = roundPrec(newValue, step);

    return clamp(newValue, min, max);
}

} // namespace encoder
} // namespace mcu
} // namespace eez

#endif

#endif