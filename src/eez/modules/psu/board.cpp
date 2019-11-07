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

#include <eez/modules/psu/psu.h>
#include <eez/modules/psu/board.h>

#if defined(EEZ_PLATFORM_STM32)
#include <eez/system.h>
#include <main.h>
#endif

namespace eez {
namespace psu {
namespace board {

void powerUp() {
#if defined(EEZ_PLATFORM_STM32)    
    HAL_GPIO_WritePin(PWR_SSTART_GPIO_Port, PWR_SSTART_Pin, GPIO_PIN_SET);
    delay(700);

    HAL_GPIO_WritePin(PWR_DIRECT_GPIO_Port, PWR_DIRECT_Pin, GPIO_PIN_SET);
    delay(100);
#endif
}

void powerDown() {
#if defined(EEZ_PLATFORM_STM32)    
    HAL_GPIO_WritePin(PWR_DIRECT_GPIO_Port, PWR_DIRECT_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(PWR_SSTART_GPIO_Port, PWR_SSTART_Pin, GPIO_PIN_RESET);
#endif
}

} // namespace board
} // namespace psu
} // namespace eez
