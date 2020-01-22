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

#include <eez/modules/psu/init.h>

#include <eez/system.h>

#include <eez/modules/psu/psu.h>
#include <eez/modules/psu/channel.h>
#include <eez/modules/psu/channel_dispatcher.h>
#include <eez/modules/psu/trigger.h>

namespace eez {
namespace psu {

void mainLoop(const void *);

#if defined(EEZ_PLATFORM_STM32)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wwrite-strings"
#endif

osThreadDef(g_psuTask, mainLoop, osPriorityAboveNormal, 0, 2048);

#if defined(EEZ_PLATFORM_STM32)
#pragma GCC diagnostic pop
#endif

osThreadId g_psuTaskHandle;

#define PSU_QUEUE_SIZE 10

osMessageQDef(g_psuMessageQueue, PSU_QUEUE_SIZE, uint32_t);
osMessageQId g_psuMessageQueueId;

bool onSystemStateChanged() {
    if (g_systemState == eez::SystemState::BOOTING) {
        if (g_systemStatePhase == 0) {
#ifdef EEZ_PLATFORM_SIMULATOR
            eez::psu::simulator::init();
#endif
            g_psuMessageQueueId = osMessageCreate(osMessageQ(g_psuMessageQueue), NULL);
            g_psuTaskHandle = osThreadCreate(osThread(g_psuTask), nullptr);
            boot();
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

bool g_adcMeasureAllFinished = false;

void oneIter() {
    osEvent event = osMessageGet(g_psuMessageQueueId, 1);
    if (event.status == osEventMessage) {
    	uint32_t message = event.value.v;
    	uint32_t type = PSU_QUEUE_MESSAGE_TYPE(message);
    	uint32_t param = PSU_QUEUE_MESSAGE_PARAM(message);
        if (type == PSU_QUEUE_MESSAGE_TYPE_CHANGE_POWER_STATE) {
            changePowerState(param ? true : false);
        } else if (type == PSU_QUEUE_MESSAGE_TYPE_RESET) {
            reset();
        } else if (type == PSU_QUEUE_MESSAGE_SPI_IRQ) {
            auto channelInterface = eez::psu::Channel::getBySlotIndex(param).channelInterface;
            if (channelInterface) {
            	channelInterface->onSpiIrq();
            }
        } else if (type == PSU_QUEUE_MESSAGE_ADC_MEASURE_ALL) {
            eez::psu::Channel::get(param).adcMeasureAll();
            g_adcMeasureAllFinished = true;
        } else if (type == PSU_QUEUE_TRIGGER_START_IMMEDIATELY) {
            trigger::startImmediatelyInPsuThread();
        } else if (type == PSU_QUEUE_TRIGGER_ABORT) {
            trigger::abort();
        } else if (type == PSU_QUEUE_TRIGGER_CHANNEL_SAVE_AND_DISABLE_OE) {
            Channel::saveAndDisableOE();
        } else if (type == PSU_QUEUE_TRIGGER_CHANNEL_RESTORE_OE) {
            Channel::restoreOE();
        } else if (type == PSU_QUEUE_SET_COUPLING_TYPE) {
            channel_dispatcher::setCouplingTypeInPsuThread((channel_dispatcher::CouplingType)param);
        } else if (type == PSU_QUEUE_SET_TRACKING_CHANNELS) {
            channel_dispatcher::setTrackingChannels((uint16_t)param);
        } else if (type == PSU_QUEUE_CHANNEL_OUTPUT_ENABLE) {
            channel_dispatcher::outputEnable(Channel::get((param >> 8) & 0xFF), param & 0xFF ? true : false);
        } else if (type == PSU_QUEUE_SYNC_OUTPUT_ENABLE) {
            channel_dispatcher::syncOutputEnable();
        }
    } else if (g_isBooted) {
        tick();
    }
}

bool measureAllAdcValuesOnChannel(int channelIndex) {
	if (g_slots[Channel::get(channelIndex).slotIndex].moduleInfo->moduleType == MODULE_TYPE_NONE) {
		return true;
	}

    g_adcMeasureAllFinished = false;
    osMessagePut(eez::psu::g_psuMessageQueueId, PSU_QUEUE_MESSAGE(PSU_QUEUE_MESSAGE_ADC_MEASURE_ALL, channelIndex), 0);

    int i;
    for (i = 0; i < 100 && !g_adcMeasureAllFinished; ++i) {
        osDelay(10);
    }

    return g_adcMeasureAllFinished;
}

} // namespace psu
} // namespace eez
