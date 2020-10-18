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
#include <eez/modules/mcu/button.h>
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

static uint16_t g_totalCounter;
static volatile int16_t g_diffCounter;
#if defined(EEZ_PLATFORM_SIMULATOR)
bool g_simulatorClicked;
#endif	
EncoderMode g_encoderMode = ENCODER_MODE_AUTO;

////////////////////////////////////////////////////////////////////////////////

void init() {
}

int getAcceleratedCounter(int increment) {
    if (increment == 0) {
        return 0;
    }

    if (g_encoderMode != ENCODER_MODE_AUTO) {
        return increment;
    }

    static int g_lastSign = 0;
    int sign = increment > 0 ? 1 : -1;
    bool diffSign = sign != g_lastSign;
    g_lastSign = sign;

    static uint32_t g_lastTime = 0;
    uint32_t currentTime = millis();
    float dt = 1.0f * (currentTime - g_lastTime) * sign / increment;
    g_lastTime = currentTime;

    const float X1 = 200.0f / 24;
    const float X2 = 1000.0f / 24;

    const float Y1 = 1.0f * 40 * 1000 / (24 * 5); // 40 V / (24 pulses per 1/2) / 0.5 mV
    const float Y2 = 1.0F;

    if (diffSign) {
        dt = X2;
    } else {
        dt = clamp(dt, X1, X2);
    }

    float accel = remap(dt, X1, Y1, X2, Y2);

    uint8_t speedOption = increment > 0 ? psu::persist_conf::devConf.encoderMovingSpeedUp : psu::persist_conf::devConf.encoderMovingSpeedDown;

    accel = remap(1.0f * speedOption, 1.0f * MIN_MOVING_SPEED, 1.0f, 1.0f * MAX_MOVING_SPEED, accel);

    printf("inc=%d, dt=%f, sign=%d, accel=%f\n", increment, dt, sign, accel);

    return (int)(increment * accel);
}

#if defined(EEZ_PLATFORM_STM32)

void onPinInterrupt() {
    // https://github.com/buxtronix/arduino/blob/master/libraries/Rotary/Rotary.cpp
    // static const uint8_t DIR_NONE = 0x0; // No complete step yet.
    static const uint8_t DIR_CW = 0x10; // Clockwise step.
    static const uint8_t DIR_CCW = 0x20; // Anti-clockwise step.

    static const uint8_t R_START = 0x0;
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

    static volatile uint8_t g_rotationState = R_START;
    
    uint8_t pinState = (HAL_GPIO_ReadPin(ENC_B_GPIO_Port, ENC_B_Pin) << 1) | HAL_GPIO_ReadPin(ENC_A_GPIO_Port, ENC_A_Pin);
    g_rotationState = g_ttable[g_rotationState & 0xf][pinState];
    
    uint8_t dir = g_rotationState & 0x30;
    if (dir == DIR_CCW) {
        g_diffCounter += getAcceleratedCounter(1);
    } else if (dir == DIR_CW) {
        g_diffCounter += getAcceleratedCounter(-1);
    }
}
#endif

int getCounter() {
#if defined(EEZ_PLATFORM_STM32)
    taskENTER_CRITICAL();
#endif

    int16_t diffCounter = g_diffCounter;
    g_diffCounter -= diffCounter;

#if defined(EEZ_PLATFORM_STM32)
    taskEXIT_CRITICAL();
#endif

    g_totalCounter += diffCounter;
    psu::debug::g_encoderCounter.set(g_totalCounter);

    return diffCounter;
}

void resetEncoder() {
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
	g_diffCounter += getAcceleratedCounter(counter);
	g_simulatorClicked = clicked;
}
#endif

void switchEncoderMode() {
    if (g_encoderMode == ENCODER_MODE_STEP4) {
        g_encoderMode = ENCODER_MODE_AUTO;
    } else {
        g_encoderMode = EncoderMode(g_encoderMode + 1);
    }
}

} // namespace encoder
} // namespace mcu
} // namespace eez

#endif
