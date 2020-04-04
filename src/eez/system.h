/*
* EEZ Generic Firmware
* Copyright (C) 2018-present, Envox d.o.o.
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

#pragma once

#include "cmsis_os.h"
#include <stdint.h>

#if defined(EEZ_PLATFORM_STM32)
#include <main.h>
#include <iwdg.h>
#define WATCHDOG_RESET(...) HAL_IWDG_Refresh(&hiwdg)
extern volatile uint32_t g_tickCount;
#else
#define WATCHDOG_RESET(...) 0
#endif

namespace eez {

uint32_t micros();
uint32_t millis();
void delay(uint32_t millis);
void delayMicroseconds(uint32_t microseconds);

const char *getSerialNumber();

} // namespace eez
