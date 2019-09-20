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

#include <eez/modules/mcu/battery.h>

#if defined(EEZ_PLATFORM_STM32)
#include <eez/system.h>
#include <eez/util.h>

#include <adc.h>
#include <dma.h>

using namespace eez::mcu::battery;

#define VBAT_DIV 4
#define MAX_CONVERTED_VALUE 4095
#define VREF 3.3f

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc) {
	uint32_t convertedValue = HAL_ADC_GetValue(hadc);
	g_battery = eez::roundPrec(VBAT_DIV * VREF * convertedValue / MAX_CONVERTED_VALUE, 0.01);
}
#endif

namespace eez {
namespace mcu {
namespace battery {

float g_battery;

#if defined(EEZ_PLATFORM_STM32)

static uint32_t g_lastTickCount;

void adcStart() {
	HAL_ADC_Start_IT(&hadc1);
}

#endif

void init() {
#if defined(EEZ_PLATFORM_STM32)
	adcStart();
	g_lastTickCount = micros();
#endif
}

void tick(uint32_t tickCount) {
#if defined(EEZ_PLATFORM_STM32)
	int32_t diff = tickCount - g_lastTickCount;
	if (diff >= 1000000) {
		adcStart();
		g_lastTickCount = tickCount;
	}
#endif
}

} // namespace battery
} // namespace mcu
} // namespace eez
