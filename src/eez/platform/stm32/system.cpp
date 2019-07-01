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

#include <dwt_stm32_delay.h>

#include <eez/system.h>

namespace eez {

uint32_t millis() {
    return HAL_GetTick();
}

void delay(uint32_t millis) {
    HAL_Delay(millis);
}

uint32_t micros() {
    //return DWT_micros();
	return millis() * 1000;
}

void delayMicroseconds(uint32_t microseconds) {
    //DWT_Delay_us(microseconds);
	delay((microseconds + 500) / 1000);
}

} // namespace eez
