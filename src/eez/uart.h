/*
* EEZ Generic Firmware
* Copyright (C) 2020-present, Envox d.o.o.
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

#include <stdint.h>

#ifdef EEZ_PLATFORM_STM32
#include "main.h"
#include "usart.h"
#endif

namespace eez {
namespace uart {

#if defined(EEZ_PLATFORM_SIMULATOR)
typedef enum 
{
  HAL_OK       = 0x00U,
  HAL_ERROR    = 0x01U,
  HAL_BUSY     = 0x02U,
  HAL_TIMEOUT  = 0x03U
} HAL_StatusTypeDef;
#endif

void tick();

void refresh();

HAL_StatusTypeDef transmit(uint8_t *data, uint16_t size, uint32_t timeout);
HAL_StatusTypeDef receive(uint8_t *data, uint16_t size, uint32_t timeout, uint16_t *nreceived = nullptr);
bool receiveFromBuffer(uint8_t *data, uint16_t size, uint16_t &n, int *err);
void resetInputBuffer();

void initScpi();
void onQueueMessage(uint32_t type, uint32_t param);

enum UartMode {
	UART_MODE_BUFFER,
	UART_MODE_SCPI,
  UART_MODE_BOOKMARK,
};

} // uart
} // eez
