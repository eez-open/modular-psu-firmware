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

#include <tim.h>

#include <eez/system.h>

#include <eez/platform/stm32/display.h>

#include <bb3/psu/psu.h>
#include <bb3/psu/persist_conf.h>

using namespace eez::gui;

#define CONF_TURN_ON_OFF_ANIMATION_DURATION 1000

namespace eez {
namespace gui {
namespace display {

static uint32_t g_displayStateTransitionStartTime;

TIM_HandleTypeDef *getTimer() {
    if (!g_mcuRevision) {
        return nullptr;
    }
    return g_mcuRevision >= MCU_REVISION_R3B3 ? &htim4 : &htim12;
}

uint32_t getTimerChannel() {
    return g_mcuRevision >= MCU_REVISION_R3B3 ? TIM_CHANNEL_1 : TIM_CHANNEL_2;
}

void turnOnStartHook() {
    // backlight on, minimal brightness
    if (getTimer()) {
        HAL_TIM_PWM_Start(getTimer(), getTimerChannel());
        __HAL_TIM_SET_COMPARE(getTimer(), getTimerChannel(), 0);
    }

    g_displayState = TURNING_ON;
    g_displayStateTransitionStartTime = millis();
}

void turnOnTickHook() {
    uint32_t max = 0;
    if (getTimer()) {
        max = __HAL_TIM_GET_AUTORELOAD(getTimer());
    }
    max = psu::persist_conf::devConf.displayBrightness * max / 20;
    int32_t diff = millis() - g_displayStateTransitionStartTime;
    if (diff >= CONF_TURN_ON_OFF_ANIMATION_DURATION) {
        g_displayState = ON;
        if (getTimer()) {
            __HAL_TIM_SET_COMPARE(getTimer(), getTimerChannel(), max);
        }
    } else {
        if (getTimer()) {
            __HAL_TIM_SET_COMPARE(getTimer(), getTimerChannel(), (uint32_t)remapQuad(1.0f * diff / CONF_TURN_ON_OFF_ANIMATION_DURATION, 0.0f, 0.0f, 1.0f, 1.0f * max));
        }
    }
}

void updateBrightness() {
	if (g_displayState == ON) {
        if (getTimer()) {
		    uint32_t max = __HAL_TIM_GET_AUTORELOAD(getTimer());
		    __HAL_TIM_SET_COMPARE(getTimer(), getTimerChannel(), psu::persist_conf::devConf.displayBrightness * max / 20);
        }
	}
}

void turnOffStartHook() {
    g_displayState = TURNING_OFF;
    g_displayStateTransitionStartTime = millis();
}

void turnOffTickHook() {
    int32_t diff = millis() - g_displayStateTransitionStartTime;
    if (diff >= CONF_TURN_ON_OFF_ANIMATION_DURATION) {
        __HAL_RCC_DMA2D_CLK_DISABLE();

        if (getTimer()) {
            // backlight off
            HAL_TIM_PWM_Stop(getTimer(), getTimerChannel());
        }

        g_displayState = OFF;
    } else {
        if (getTimer()) {            
            uint32_t max = __HAL_TIM_GET_AUTORELOAD(getTimer());
            max = psu::persist_conf::devConf.displayBrightness * max / 20;
            __HAL_TIM_SET_COMPARE(getTimer(), getTimerChannel(), (uint32_t)remap(1.0f * (CONF_TURN_ON_OFF_ANIMATION_DURATION - diff) / CONF_TURN_ON_OFF_ANIMATION_DURATION, 0.0f, 0.0f, 1.0f, 1.0f * max));
        }
    }
}

} // namespace display
} // namespace gui
} // namespace eez
