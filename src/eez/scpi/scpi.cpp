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

#include <eez/scpi/scpi.h>

#include <eez/system.h>

#include <eez/apps/psu/psu.h>
#include <eez/apps/psu/list_program.h>
#include <eez/apps/psu/serial_psu.h>
#if OPTION_ETHERNET
#include <eez/apps/psu/ethernet.h>
#include <eez/apps/psu/ntp.h>
#endif
#include <eez/apps/psu/event_queue.h>
#include <eez/apps/psu/profile.h>
#include <eez/apps/psu/scpi/psu.h>

using namespace eez::psu;
using namespace eez::psu::scpi;

namespace eez {
namespace scpi {

void mainLoop(const void *);

osThreadId g_scpiTaskHandle;

#if defined(EEZ_PLATFORM_STM32)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wwrite-strings"
#endif

osThreadDef(g_scpiTask, mainLoop, osPriorityNormal, 0, 4096);

#if defined(EEZ_PLATFORM_STM32)
#pragma GCC diagnostic pop
#endif

osMessageQDef(g_scpiMessageQueue, SCPI_QUEUE_SIZE, uint32_t);
osMessageQId g_scpiMessageQueueId;

char g_listFilePath[CH_MAX][MAX_PATH_LENGTH];

bool onSystemStateChanged() {
    if (eez::g_systemState == eez::SystemState::BOOTING) {
        if (eez::g_systemStatePhase == 0) {
            g_scpiMessageQueueId = osMessageCreate(osMessageQ(g_scpiMessageQueue), NULL);
            g_scpiTaskHandle = osThreadCreate(osThread(g_scpiTask), nullptr);
        }
    }

    return true;
}

void oneIter();

void mainLoop(const void *) {
#ifdef __EMSCRIPTEN__
    oneIter();
#else
    while (1) {
        oneIter();
    }
#endif
}

void oneIter() {
    osEvent event = osMessageGet(g_scpiMessageQueueId, 1000);
    if (event.status == osEventMessage) {
    	uint32_t message = event.value.v;
    	uint32_t target = SCPI_QUEUE_MESSAGE_TARGET(message);
    	uint32_t type = SCPI_QUEUE_MESSAGE_TYPE(message);
    	uint32_t param = SCPI_QUEUE_MESSAGE_PARAM(message);
        if (target == SCPI_QUEUE_MESSAGE_TARGET_SERIAL) {
            serial::onQueueMessage(type, param);
        }
#if OPTION_ETHERNET
        else if (target == SCPI_QUEUE_MESSAGE_TARGET_ETHERNET) {
            ethernet::onQueueMessage(type, param);
        }
#endif  
        else if (target == SCPI_QUEUE_MESSAGE_TARGET_NONE) {
            if (type == SCPI_QUEUE_MESSAGE_TYPE_SAVE_LIST) {
                int err;
                if (!eez::psu::list::saveList(param, &g_listFilePath[param][0], &err)) {
                    generateError(err);
                }
            } else if (type == SCPI_QUEUE_MESSAGE_TYPE_DELETE_PROFILE_LISTS) {
                profile::deleteProfileLists(param);
            }
        }      
    } else {
        profile::tick();
#if OPTION_ETHERNET
        ntp::tick();
#endif
    }
}

void resetContext(scpi_t *context) {
    scpi_psu_t *psuContext = (scpi_psu_t *)context->user_context;
    psuContext->selected_channel_index = 0;
#if OPTION_SD_CARD
    psuContext->currentDirectory[0] = 0;
#endif
    SCPI_ErrorClear(context);
}

void resetContext() {
    // SYST:ERR:COUN? 0
    if (serial::g_testResult == TEST_OK) {
        scpi::resetContext(&serial::g_scpiContext);
    }

#if OPTION_ETHERNET
    if (ethernet::g_testResult == TEST_OK) {
        scpi::resetContext(&ethernet::g_scpiContext);
    }
#endif
}

void generateError(int error) {
    if (serial::g_testResult == TEST_OK) {
        SCPI_ErrorPush(&serial::g_scpiContext, error);
    }
#if OPTION_ETHERNET
    if (ethernet::g_testResult == TEST_OK) {
        SCPI_ErrorPush(&ethernet::g_scpiContext, error);
    }
#endif
    event_queue::pushEvent(error);
}

}
}
