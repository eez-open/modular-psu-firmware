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

#include <eez/platform/stm32/dwt_delay.h>

#define DWT_CONTROL ((volatile unsigned int *)0xE0001000)
#define DWT_LAR     ((volatile unsigned int *)0xE0001FB0)
#define SCB_DEMCR   ((volatile unsigned int *)0xE000EDFC)

uint32_t g_cyclesPerMicrosecond;

void DWT_Delay_Init(void) {
	*DWT_LAR = 0xC5ACCE55; // enable access
	*SCB_DEMCR |= 0x01000000;
	*DWT_CYCCNT = 0; // reset the counter
	*DWT_CONTROL |= 1 ; // enable the counter

	g_cyclesPerMicrosecond = HAL_RCC_GetHCLKFreq() / 1000000;
}
