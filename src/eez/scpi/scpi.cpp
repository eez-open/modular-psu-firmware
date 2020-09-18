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
#include <eez/modules/psu/psu.h>
#include <eez/modules/psu/event_queue.h>
#include <eez/modules/psu/scpi/psu.h>
#include <eez/modules/psu/serial_psu.h>
#if OPTION_ETHERNET
#include <eez/modules/psu/ethernet.h>
#endif
#include <scpi/scpi.h>

using namespace eez::psu;

namespace eez {
namespace scpi {

void resetContext(scpi_t *context) {
    auto psuContext = (eez::psu::scpi::scpi_psu_t *)context->user_context;

    if (CH_NUM > 0) {
        auto &channel = Channel::get(0);
        psuContext->selectedChannels.numChannels = 1;
        psuContext->selectedChannels.channels[0].slotIndex = channel.slotIndex;
        psuContext->selectedChannels.channels[0].subchannelIndex = channel.subchannelIndex;
    } else {
        psuContext->selectedChannels.numChannels = 0;
    }

    psuContext->currentDirectory[0] = 0;
    SCPI_ErrorClear(context);
}

void resetContext() {
    // SYST:ERR:COUN? 0
    if (psu::serial::g_testResult == TEST_OK) {
        scpi::resetContext(&psu::serial::g_scpiContext);
    }

#if OPTION_ETHERNET
    if (psu::ethernet::g_testResult == TEST_OK) {
        scpi::resetContext(&psu::ethernet::g_scpiContext);
    }
#endif
}

void generateError(int error) {
    if (g_isBooted && !isLowPriorityThread()) {
        sendMessageToLowPriorityThread(THREAD_MESSAGE_GENERATE_ERROR, error);
    } else {
        if (psu::serial::g_testResult == TEST_OK) {
            SCPI_ErrorPush(&psu::serial::g_scpiContext, error);
        }

#if OPTION_ETHERNET
        if (psu::ethernet::g_testResult == TEST_OK) {
            SCPI_ErrorPush(&psu::ethernet::g_scpiContext, error);
        }
#endif

        psu::event_queue::pushEvent(error);
    }
}

}
}
