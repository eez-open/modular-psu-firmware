/*
* EEZ PSU Firmware
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

#include <cmsis_os.h>

namespace eez {
namespace psu {

bool onSystemStateChanged();

extern osThreadId g_psuTaskHandle;
extern osMessageQId g_psuMessageQueueId;

#define PSU_QUEUE_MESSAGE_TYPE_CHANGE_POWER_STATE 1
#define PSU_QUEUE_MESSAGE_TYPE_RESET 2
#define PSU_QUEUE_MESSAGE_SPI_IRQ 3
#define PSU_QUEUE_MESSAGE_ADC_MEASURE_ALL 4

#define PSU_QUEUE_MESSAGE(type, param) (((param) << 4) | (type))
#define PSU_QUEUE_MESSAGE_TYPE(message) ((message) & 0xF)
#define PSU_QUEUE_MESSAGE_PARAM(param) ((message) >> 4)

bool measureAllAdcValuesOnChannel(int channelIndex);

void lock();
void unlock();

}
} // namespace eez
