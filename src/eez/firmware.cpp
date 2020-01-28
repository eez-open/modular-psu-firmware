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

#include <assert.h>
#include <stdio.h>

#if defined(EEZ_PLATFORM_STM32)
#include <main.h>
#include <usb_device.h>
#endif

#include <eez/firmware.h>
#include <eez/system.h>
#include <eez/mp.h>
#include <eez/sound.h>

#include <eez/scpi/scpi.h>

#include <eez/gui/gui.h>

#include <eez/modules/psu/psu.h>
#if OPTION_SD_CARD
#include <eez/modules/psu/sd_card.h>
#endif
#include <eez/modules/psu/serial_psu.h>
#if OPTION_SD_CARD
#include <eez/modules/psu/sd_card.h>
#endif
#include <eez/modules/psu/ontime.h>
#include <eez/modules/psu/persist_conf.h>
#include <eez/modules/psu/serial_psu.h>
#include <eez/modules/psu/rtc.h>
#include <eez/modules/psu/datetime.h>
#include <eez/modules/psu/event_queue.h>
#include <eez/modules/psu/io_pins.h>
#include <eez/modules/psu/profile.h>
#include <eez/modules/psu/list_program.h>
#include <eez/modules/psu/trigger.h>
#if OPTION_ETHERNET
#include <eez/modules/psu/ethernet.h>
#include <eez/modules/psu/ntp.h>
#endif

#if OPTION_FAN
#include <eez/modules/aux_ps/fan.h>
#endif

#include <eez/modules/mcu/battery.h>
#include <eez/modules/mcu/eeprom.h>
#if OPTION_SDRAM
#include <eez/modules/mcu/sdram.h>
#endif
#include <eez/modules/mcu/ethernet.h>
#if OPTION_ENCODER
#include <eez/modules/mcu/encoder.h>
#endif

#include <eez/modules/bp3c/eeprom.h>
#include <eez/modules/bp3c/io_exp.h>

 ////////////////////////////////////////////////////////////////////////////////

namespace eez {

bool g_isBooted;
bool g_bootTestSuccess;
bool g_shutdownInProgress;
bool g_shutdown;

void boot() {
#if OPTION_SDRAM
    mcu::sdram::init();
    //mcu::sdram::test();
#endif

#if OPTION_DISPLAY
    gui::startThread();
#endif

#if defined(EEZ_PLATFORM_STM32)
    MX_USB_DEVICE_Init();
#endif

#ifdef EEZ_PLATFORM_SIMULATOR
    eez::psu::simulator::init();
#endif

#if OPTION_ETHERNET
    mcu::ethernet::initMessageQueue();
#endif
    scpi::initMessageQueue();

    psu::startThread();

    // INIT

    for (int i = 0; i < CH_MAX; i++) {
        psu::Channel::get(i).setChannelIndex(i);
    }

    sound::init();

    mcu::eeprom::init();
    mcu::eeprom::test();

    bp3c::eeprom::init();
    bp3c::eeprom::test();

#if OPTION_SD_CARD
    psu::sd_card::init();
#endif

    bp3c::io_exp::init();

    // inst:memo 1,0,2,406
    // 1: slot no. (1, 2 or 3)
    // 0: address
    // 2: size of value (1 byte or 2 bytes)
    // 406: value

    psu::ontime::g_mcuCounter.init();

    int channelIndex = 0;
    int uninstalledChannelIndex = CH_MAX - 1;
    for (uint8_t slotIndex = 0; slotIndex < NUM_SLOTS; slotIndex++) {
        auto& slot = g_slots[slotIndex];

        static const uint16_t ADDRESS = 0;
        uint16_t value[2];
        if (!bp3c::eeprom::read(slotIndex, (uint8_t *)&value, sizeof(value), ADDRESS)) {
            value[0] = MODULE_TYPE_NONE;
            value[1] = 0;
        }

        uint16_t moduleType = value[0];
        uint16_t moduleRevision = value[1];

        slot.moduleInfo = getModuleInfo(moduleType);
        slot.moduleRevision = moduleRevision;

        if (slot.moduleInfo->moduleType != MODULE_TYPE_NONE) {
            slot.channelIndex = channelIndex;

            for (uint8_t subchannelIndex = 0; subchannelIndex < slot.moduleInfo->numChannels; subchannelIndex++) {
                psu::Channel::get(channelIndex++).set(slotIndex, subchannelIndex);
            }

            psu::persist_conf::loadModuleConf(slotIndex);
            psu::ontime::g_moduleCounters[slotIndex].init();
            psu::CH_NUM = channelIndex;
        } else {
            slot.channelIndex = uninstalledChannelIndex;
            psu::Channel::get(uninstalledChannelIndex).set(slotIndex, 0);
            uninstalledChannelIndex--;
        }
    }

    psu::persist_conf::init();

#if OPTION_DISPLAY
    eez::gui::showWelcomePage();
#endif

    psu::serial::init();

    psu::rtc::init();
    psu::datetime::init();

    psu::event_queue::init();
    psu::profile::init();

    psu::io_pins::init();

    psu::list::init();

#if OPTION_ETHERNET
    psu::ethernet::init();
    psu::ntp::init();
#else
    // DebugTrace("Ethernet initialization skipped!");
#endif

#if OPTION_FAN
    aux_ps::fan::init();
#endif

    psu::temperature::init();

    mcu::battery::init();

    psu::trigger::init();

#if OPTION_ENCODER
    mcu::encoder::init();
#endif

    // TEST

    g_bootTestSuccess = true;

    g_bootTestSuccess &= testMaster();

    if (!psu::autoRecall()) {
        psu::psuReset();
    }

    // play beep if there is an error during boot procedure
    if (!g_bootTestSuccess) {
        sound::playBeep();
    }

    g_isBooted = true;

#if OPTION_ETHERNET
    mcu::ethernet::startThread();
#endif
    scpi::startThread();

    mp::initMessageQueue();
    mp::startThread();
}

bool testMaster() {
    bool result = true;

#if OPTION_FAN
    result &= aux_ps::fan::test();
#endif

    result &= psu::rtc::test();
    result &= psu::datetime::test();
    result &= mcu::eeprom::test();

#if OPTION_SD_CARD
    result &= psu::sd_card::test();
#endif

#if OPTION_ETHERNET
    result &= psu::ethernet::test();
#endif

    result &= psu::temperature::test();

    result &= mcu::battery::test();

    // test BP3c
    result &= bp3c::io_exp::test();

    g_masterTestResult = result ? TEST_OK : TEST_FAILED;

    return result;
}

bool test() {
    bool testResult = true;

    psu::profile::saveAtLocation(10);
    bool wasSaveProfileEnabled = psu::profile::enableSave(false);
    psu::psuReset();

    testResult &= testMaster();

    testResult &= psu::testChannels();

    psu::profile::enableSave(wasSaveProfileEnabled);
    int err;
    psu::profile::recall(10, &err);

    if (!testResult) {
        sound::playBeep();
    }

    return testResult;
}

bool reset() {
    using namespace psu;

    if (osThreadGetId() != g_psuTaskHandle) {
        osMessagePut(g_psuMessageQueueId, PSU_QUEUE_MESSAGE(PSU_QUEUE_MESSAGE_TYPE_RESET, 0), osWaitForever);
        return true;
    }

    if (psuReset()) {
        profile::save();
        return true;
    }

    return false;
}

void standBy() {
    psu::changePowerState(false);
}

static bool g_reset;

void hardReset() {
    g_reset = true;
    shutdown();
}

void shutdown() {
    using namespace psu;

    if (osThreadGetId() != g_psuTaskHandle) {
        gui::showSavingPage();
        osMessagePut(g_psuMessageQueueId, PSU_QUEUE_MESSAGE(PSU_QUEUE_MESSAGE_TYPE_SHUTDOWN, 0), osWaitForever);
        return;
    }

    osDelay(50);

    g_shutdownInProgress = true;

    if (psu::isPowerUp()) {
    	profile::saveIfDirty();
    	profile::enableSave(false);
    	psu::changePowerState(false);
    } else {
    	profile::saveIfDirty();
    }

    osDelay(50);

    // shutdown SCPI thread
    using namespace eez::scpi;
    osMessagePut(g_scpiMessageQueueId, SCPI_QUEUE_MESSAGE(SCPI_QUEUE_MESSAGE_TARGET_NONE, SCPI_QUEUE_MESSAGE_TYPE_SHUTDOWN, 0), osWaitForever);
    do {
        osDelay(10);
    } while (isThreadAlive());

    // save on-time counters
    persist_conf::writeTotalOnTime(ontime::g_mcuCounter.getType(), ontime::g_mcuCounter.getTotalTime());
    for (int slotIndex = 0; slotIndex < NUM_SLOTS; slotIndex++) {
        if (g_slots[slotIndex].moduleInfo->moduleType != MODULE_TYPE_NONE) {
            persist_conf::writeTotalOnTime(ontime::g_moduleCounters[slotIndex].getType(), ontime::g_moduleCounters[slotIndex].getTotalTime());
        }
    }

    while (persist_conf::saveAllDirtyBlocks()) {
        delay(1);
    }

    if (g_reset) {
        delay(1000);

#if defined(EEZ_PLATFORM_STM32)
        NVIC_SystemReset();
#endif

        g_shutdown = true;
    } else {
        gui::showShutdownPage();

#if defined(EEZ_PLATFORM_SIMULATOR)
        osDelay(100);
#endif

        g_shutdown = true;
    }
}

}
