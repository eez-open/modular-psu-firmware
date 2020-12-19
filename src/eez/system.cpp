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

#include <stdio.h>

#include <eez/system.h>

#if defined(EEZ_PLATFORM_STM32)
#include <main.h>
#include <iwdg.h>

static const uint32_t CONF_WATCHDOG_REFRESH_FREQ_MS = 1000;

volatile uint64_t g_tickCount;

static uint32_t g_watchdogLastTime = 0;
static int g_watchdogExpectingTask = WATCHDOG_HIGH_PRIORITY_THREAD;

#ifdef MASTER_MCU_REVISION_R3B3_OR_NEWER
bool g_supervisorWatchdogEnabled = 1;
#endif

namespace eez {
extern bool g_isBooted;
}

static void doWatchdogRefresh() {
	uint32_t currentTime = eez::millis();
	if (!g_watchdogLastTime || currentTime - g_watchdogLastTime >= CONF_WATCHDOG_REFRESH_FREQ_MS) {
		HAL_IWDG_Refresh(&hiwdg);

#ifdef MASTER_MCU_REVISION_R3B3_OR_NEWER
		if (g_supervisorWatchdogEnabled) {
			HAL_GPIO_WritePin(SPI5_CSC_GPIO_Port, SPI5_CSC_Pin, GPIO_PIN_SET);
			eez::delayMicroseconds(1);
			HAL_GPIO_WritePin(SPI5_CSC_GPIO_Port, SPI5_CSC_Pin, GPIO_PIN_RESET);
		}
#endif

		g_watchdogLastTime = currentTime;
		if (!g_watchdogLastTime) {
			g_watchdogLastTime = 1;
		}
	}
}

void watchdogReset(int fromTask) {
	if (fromTask == WATCHDOG_LONG_OPERATION || !eez::g_isBooted) {
		doWatchdogRefresh();
	} else if (fromTask == WATCHDOG_HIGH_PRIORITY_THREAD) {
		if (g_watchdogExpectingTask == WATCHDOG_HIGH_PRIORITY_THREAD) {
			doWatchdogRefresh();
			g_watchdogExpectingTask = WATCHDOG_GUI_THREAD;
		}
	} else if (fromTask == WATCHDOG_GUI_THREAD) {
		if (g_watchdogExpectingTask == WATCHDOG_GUI_THREAD) {
			doWatchdogRefresh();
			g_watchdogExpectingTask = WATCHDOG_HIGH_PRIORITY_THREAD;
		}
	}
}
#endif

namespace eez {

uint32_t millis() {
    return osKernelSysTick();
}

void delay(uint32_t millis) {
#if defined(EEZ_PLATFORM_STM32)
    HAL_Delay(millis);
#endif

#if defined(EEZ_PLATFORM_SIMULATOR)
	osDelay(millis);
#endif
}

uint32_t micros() {
#if defined(EEZ_PLATFORM_STM32)
    auto tc1 = g_tickCount;
	auto cnt = TIM7->CNT;
	auto tc2 = g_tickCount;
	if (tc1 == tc2) {
		return (uint32_t)(tc1 * 200 + 2 * cnt);
	}
	return (uint32_t)(tc2 * 200);
#endif

#if defined(EEZ_PLATFORM_SIMULATOR)
	return osKernelSysTick() * 1000;
#endif
}

void delayMicroseconds(uint32_t microseconds) {
#if defined(EEZ_PLATFORM_STM32)
	while (microseconds--) {
		// 216 NOP's

		// remove 6 NOP's to compensate looping costs

		// __ASM volatile ("NOP");
		// __ASM volatile ("NOP");
		// __ASM volatile ("NOP");
		// __ASM volatile ("NOP");
		// __ASM volatile ("NOP");
		// __ASM volatile ("NOP");

		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
		__ASM volatile ("NOP");
	}
#endif

#if defined(EEZ_PLATFORM_SIMULATOR)
	osDelay((microseconds + 500) / 1000);
#endif
}

const char *getSerialNumber() {
	static char g_serialNumber[24 + 1] = { 0 };
	if (!g_serialNumber[0]) {
#if defined(EEZ_PLATFORM_STM32)
		uint32_t idw0 = HAL_GetUIDw0();
		uint32_t idw1 = HAL_GetUIDw1();
		uint32_t idw2 = HAL_GetUIDw2();
#endif

#if defined(EEZ_PLATFORM_SIMULATOR)
		uint32_t idw0 = 0x00000000;
		uint32_t idw1 = 0x00000000;
		uint32_t idw2 = 0x00000001;
#endif

		sprintf(g_serialNumber, "%08X", (unsigned int)idw0);
		sprintf(g_serialNumber + 8, "%08X", (unsigned int)idw1);
		sprintf(g_serialNumber + 16, "%08X", (unsigned int)idw2);

		g_serialNumber[24] = 0;
	}

	return g_serialNumber;
}

} // namespace eez
