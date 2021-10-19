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

#if OPTION_DISPLAY && OPTION_ENCODER

#include <math.h>
#include <stdio.h>

#include <eez/system.h>
#include <eez/debug.h>
#include <eez/util.h>

#include <eez/modules/mcu/encoder.h>

#if defined(EEZ_PLATFORM_STM32)	
#include <tim.h>
#include <eez/modules/mcu/button.h>
#endif

#if defined(EEZ_PLATFORM_SIMULATOR)	
#include <eez/platform/simulator/events.h>
#endif

#include <eez/modules/psu/psu.h>
#include <eez/modules/psu/channel_dispatcher.h>
#include <eez/modules/psu/persist_conf.h>

namespace eez {
namespace mcu {
namespace encoder {

#if defined(EEZ_PLATFORM_STM32)
static Button g_encoderSwitch(ENC_SW_GPIO_Port, ENC_SW_Pin, true, false);
#endif	

static volatile int g_counter;

bool g_accelerationEnabled = false;
float g_range;
float g_step;

#if defined(EEZ_PLATFORM_SIMULATOR)
bool g_simulatorClicked;
#endif	

////////////////////////////////////////////////////////////////////////////////

static bool isButtonClicked();
static int getCounter();
static int getAcceleratedCounter(int increment);

////////////////////////////////////////////////////////////////////////////////

void read(int &counter, bool &clicked) {
    clicked = isButtonClicked();
    counter = getCounter();
}

void enableAcceleration(bool enable, float range, float step) {
    g_accelerationEnabled = enable;
    g_range = range;
    g_step = step;
}

#if defined(EEZ_PLATFORM_SIMULATOR)
void write(int counter, bool clicked) {
	g_counter += getAcceleratedCounter(counter);
	g_simulatorClicked = clicked;
}
#endif

#if defined(EEZ_PLATFORM_STM32)
bool g_debounceTimerStarted = false;
volatile uint8_t g_pinState;

#define READ_ENC_PIN_STATE (HAL_GPIO_ReadPin(ENC_B_GPIO_Port, ENC_B_Pin) << 1) | HAL_GPIO_ReadPin(ENC_A_GPIO_Port, ENC_A_Pin)

void onPinInterrupt() {
    if (!g_debounceTimerStarted) {
        g_pinState = READ_ENC_PIN_STATE;
        TIM8->ARR = 500;
        HAL_TIM_Base_Start_IT(&htim8);
		g_debounceTimerStarted = true;
    }
}

void onDebounceTimer() {
	HAL_TIM_Base_Stop_IT(&htim8);
    g_debounceTimerStarted = false;

    uint8_t pinState = READ_ENC_PIN_STATE;
    if (pinState != g_pinState) {
        // debounce
        return;
    }

    // https://github.com/buxtronix/arduino/blob/master/libraries/Rotary/Rotary.cpp
    // static const uint8_t DIR_NONE = 0x0; // No complete step yet.
    static const uint8_t DIR_CW = 0x10; // Clockwise step.
    static const uint8_t DIR_CCW = 0x20; // Anti-clockwise step.

    static const uint8_t R_START = 0x0;

// #define HALF_STEP

#ifdef HALF_STEP
    // Use the half-step state table (emits a code at 00 and 11)
    static const uint8_t R_CCW_BEGIN = 0x1;
    static const uint8_t R_CW_BEGIN = 0x2;
    static const uint8_t R_START_M = 0x3;
    static const uint8_t R_CW_BEGIN_M = 0x4;
    static const uint8_t R_CCW_BEGIN_M = 0x5;

    static const uint8_t g_ttable[6][4] = {
        // R_START (00)
        {R_START_M,            R_CW_BEGIN,     R_CCW_BEGIN,  R_START},
        // R_CCW_BEGIN
        {R_START_M | DIR_CCW,  R_START,        R_CCW_BEGIN,  R_START},
        // R_CW_BEGIN
        {R_START_M | DIR_CW,   R_CW_BEGIN,     R_START,      R_START},
        // R_START_M (11)
        {R_START_M,            R_CCW_BEGIN_M,  R_CW_BEGIN_M, R_START},
        // R_CW_BEGIN_M
        {R_START_M,            R_START_M,      R_CW_BEGIN_M, R_START | DIR_CW},
        // R_CCW_BEGIN_M
        {R_START_M,            R_CCW_BEGIN_M,  R_START_M,    R_START | DIR_CCW},
    };
#else
    // Use the full-step state table (emits a code at 00 only)
    static const uint8_t R_CW_FINAL = 0x1;
    static const uint8_t R_CW_BEGIN = 0x2;
    static const uint8_t R_CW_NEXT = 0x3;
    static const uint8_t R_CCW_BEGIN = 0x4;
    static const uint8_t R_CCW_FINAL = 0x5;
    static const uint8_t R_CCW_NEXT = 0x6;

    const unsigned char g_ttable[7][4] = {
        // R_START
        {R_START,    R_CW_BEGIN,  R_CCW_BEGIN, R_START},
        // R_CW_FINAL
        {R_CW_NEXT,  R_START,     R_CW_FINAL,  R_START | DIR_CW},
        // R_CW_BEGIN
        {R_CW_NEXT,  R_CW_BEGIN,  R_START,     R_START},
        // R_CW_NEXT
        {R_CW_NEXT,  R_CW_BEGIN,  R_CW_FINAL,  R_START},
        // R_CCW_BEGIN
        {R_CCW_NEXT, R_START,     R_CCW_BEGIN, R_START},
        // R_CCW_FINAL
        {R_CCW_NEXT, R_CCW_FINAL, R_START,     R_START | DIR_CCW},
        // R_CCW_NEXT
        {R_CCW_NEXT, R_CCW_FINAL, R_CCW_BEGIN, R_START},
    };
#endif

    static volatile uint8_t g_rotationState = R_START;
    
    g_rotationState = g_ttable[g_rotationState & 0xf][pinState];
    
    uint8_t dir = g_rotationState & 0x30;
    if (dir == DIR_CCW) {
        g_counter += getAcceleratedCounter(1);
    } else if (dir == DIR_CW) {
        g_counter += getAcceleratedCounter(-1);
    }
}
#endif

////////////////////////////////////////////////////////////////////////////////

bool isButtonPressed() {
#if defined(EEZ_PLATFORM_SIMULATOR)
    return eez::platform::simulator::isMiddleButtonPressed();
#endif

#if defined(EEZ_PLATFORM_STM32)
    return g_encoderSwitch.isPressed();
#endif
}

static bool isButtonClicked() {
#if defined(EEZ_PLATFORM_SIMULATOR)
    return g_simulatorClicked;
#endif

#if defined(EEZ_PLATFORM_STM32)
    return g_encoderSwitch.isClicked();
#endif
}

static int getCounter() {
#if defined(EEZ_PLATFORM_STM32)
    taskENTER_CRITICAL();
#endif

    int counter = g_counter;
    g_counter = 0;

#if defined(EEZ_PLATFORM_STM32)
    taskEXIT_CRITICAL();
#endif

    return counter;
}

static int getAcceleratedCounter(int increment) {
    if (increment == 0 || !g_accelerationEnabled) {
        return increment;
    }

    static int g_lastSign = 0;
    int sign = increment > 0 ? 1 : -1;
    bool diffSign = sign != g_lastSign;
    g_lastSign = sign;

    static uint32_t g_lastTimeMs = 0;
    uint32_t currentTimeMs = millis();
    float dt = 1.0f * (currentTimeMs - g_lastTimeMs) * sign / increment;
    g_lastTimeMs = currentTimeMs;

    const float MIN_DT_MS = 8;
    const float MAX_DT_MS = 150;

    if (diffSign) {
        dt = MAX_DT_MS;
    } else {
        dt = clamp(dt, MIN_DT_MS, MAX_DT_MS);
    }

    // Heuristic: when dt is MIN_DT_MS and speed options is MAX_MOVING_SPEED then 1/2 rotation (= 12 pulses) should give us full range.
    const float MIN_VELOCITY = 1.0f;
    const float MAX_VELOCITY = g_range / 12.0f / g_step;

    uint8_t speedOption = increment > 0 ? psu::persist_conf::devConf.encoderMovingSpeedUp : psu::persist_conf::devConf.encoderMovingSpeedDown;
    float maxVelocity = remap(
        1.0f * speedOption, 
        1.0f * MIN_MOVING_SPEED, MAX_VELOCITY / (MAX_MOVING_SPEED - MIN_MOVING_SPEED + 1), 
        1.0f * MAX_MOVING_SPEED, MAX_VELOCITY);

    float velocity = remap(dt, MIN_DT_MS, maxVelocity, MAX_DT_MS, MIN_VELOCITY);
    if (velocity < 1.0f) {
        velocity = 1.0f;
    }

    //printf("inc=%d, dt=%f, sign=%d, v=%f\n", increment, dt, sign, adjustedVelocity);

    return (int)(increment * velocity);
}

} // namespace encoder
} // namespace mcu
} // namespace eez

#endif

#if defined(EEZ_PLATFORM_STM32)
extern "C" void Encoder_OnDebounceTimer() {
    eez::mcu::encoder::onDebounceTimer();
}
#endif
