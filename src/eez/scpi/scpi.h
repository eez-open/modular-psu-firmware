/*
* EEZ Generic Firmware
* Copyright (C) 2019-present, Envox d.o.o.
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
* along with this program.  If not, see http://www.gnu.org/licenses.
*/

#pragma once

#include <cmsis_os.h>

#include <eez/modules/psu/conf.h>
#include <eez/modules/psu/conf_advanced.h>

namespace eez {
namespace scpi {

bool onSystemStateChanged();

void resetContext();
void generateError(int error);

extern osThreadId g_scpiTaskHandle;
extern osMessageQId g_scpiMessageQueueId;

#define SCPI_QUEUE_SIZE 10

#define SCPI_QUEUE_MESSAGE_TARGET_NONE 0
#define SCPI_QUEUE_MESSAGE_TARGET_SERIAL 1
#define SCPI_QUEUE_MESSAGE_TARGET_ETHERNET 2

#define SCPI_QUEUE_MESSAGE(target, type, param) (((target) << 30) | ((param) << 4) | (type))
#define SCPI_QUEUE_MESSAGE_TARGET(message) ((message) >> 30)
#define SCPI_QUEUE_MESSAGE_TYPE(message) ((message) & 0xF)
#define SCPI_QUEUE_MESSAGE_PARAM(param) (((message) & 0x3FFFFFFF) >> 4)

#define SCPI_QUEUE_SERIAL_MESSAGE(type, param) SCPI_QUEUE_MESSAGE(SCPI_QUEUE_MESSAGE_TARGET_SERIAL, type, param)
#define SCPI_QUEUE_ETHERNET_MESSAGE(type, param) SCPI_QUEUE_MESSAGE(SCPI_QUEUE_MESSAGE_TARGET_ETHERNET, type, param)

#define SCPI_QUEUE_MESSAGE_TYPE_SAVE_LIST 1
#define SCPI_QUEUE_MESSAGE_TYPE_DELETE_PROFILE_LISTS 2
#define SCPI_QUEUE_MESSAGE_TYPE_SD_DETECT_IRQ 3
#define SCPI_QUEUE_MESSAGE_DLOG_FILE_WRITE 4
#define SCPI_QUEUE_MESSAGE_DLOG_TOGGLE 5
#define SCPI_QUEUE_MESSAGE_ABORT_DOWNLOADING 6
#define SCPI_QUEUE_MESSAGE_SCREENSHOT 7

extern char g_listFilePath[CH_MAX][MAX_PATH_LENGTH];

}
}
