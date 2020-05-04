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
#include <eez/tasks.h>
#include <eez/mp.h>
#include <eez/sound.h>
#include <eez/memory.h>

#include <eez/gui/gui.h>

#include <eez/modules/psu/psu.h>
#include <eez/modules/psu/sd_card.h>
#include <eez/modules/psu/serial_psu.h>
#include <eez/modules/psu/sd_card.h>
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
#include <eez/modules/psu/gui/psu.h>

#if OPTION_FAN
#include <eez/modules/aux_ps/fan.h>
#endif

#include <eez/modules/mcu/battery.h>
#include <eez/modules/mcu/eeprom.h>
#include <eez/modules/mcu/sdram.h>
#include <eez/modules/mcu/ethernet.h>
#if OPTION_ENCODER
#include <eez/modules/mcu/encoder.h>
#endif

#include <eez/modules/bp3c/eeprom.h>
#include <eez/modules/bp3c/io_exp.h>

////////////////////////////////////////////////////////////////////////////////

#if defined(EEZ_PLATFORM_STM32)
extern bool g_isResetByIWDG;
#endif

namespace eez {

bool g_isBooted;
bool g_bootTestSuccess;
bool g_shutdownInProgress;
bool g_shutdown;

void boot() {
    assert((uint32_t)(MEMORY_END - MEMORY_BEGIN) <= MEMORY_SIZE);

    psu::event_queue::init();

#if defined(EEZ_PLATFORM_STM32)
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
    initLowPriorityMessageQueue();

    startHighPriorityThread();

    // INIT
    psu::init();

    psu::rtc::init();
    psu::datetime::init();

    mcu::eeprom::init();
    mcu::eeprom::test();

    bp3c::eeprom::init();
    bp3c::eeprom::test();

    psu::sd_card::init();

    bp3c::io_exp::init();

    // inst:memo 1,0,2,406
    // 1: slot no. (1, 2 or 3)
    // 0: address
    // 2: size of value (1 byte or 2 bytes)
    // 406: value

    psu::ontime::g_mcuCounter.init();

    for (uint8_t slotIndex = 0; slotIndex < NUM_SLOTS; slotIndex++) {
        auto &slot = g_slots[slotIndex];

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
    }

    psu::enumChannels();

    psu::persist_conf::init();

#if OPTION_DISPLAY
    psu::gui::showWelcomePage();
#endif

    sound::init();

    psu::serial::init();

    psu::profile::init();

    psu::io_pins::init();

    psu::list::init();

#if OPTION_ETHERNET
    psu::ethernet::init();
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

#if defined(EEZ_PLATFORM_STM32)
    if (g_isResetByIWDG) {
        psu::event_queue::pushEvent(psu::event_queue::EVENT_ERROR_WATCHDOG_RESET);
    }
#endif

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
    startLowPriorityThread();

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

    result &= psu::sd_card::test();

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

bool doTest() {
    bool testResult = true;

    psu::profile::saveToLocation(10);
    psu::psuReset();

    testResult &= testMaster();

    psu::initChannels();
    osDelay(100);
    testResult &= psu::testChannels();

    psu::profile::recallFromLocation(10);

    if (!testResult) {
        sound::playBeep();
    }

    return testResult;
}

bool test() {
    using namespace psu;

    static bool g_testResult;
    static bool g_testFinished;

    if (!isPsuThread()) {
        g_testFinished = false;
        
        sendMessageToPsu(PSU_MESSAGE_TEST);
        
        while (!g_testFinished) {
            osDelay(10);
        }

        return g_testResult;
    } else {
        g_testResult = doTest();
        g_testFinished = true;
        return g_testResult;
    }
}

bool reset() {
    using namespace psu;

    if (!isPsuThread()) {
        sendMessageToPsu(PSU_MESSAGE_RESET);
        return true;
    }

    return psuReset();
}

void standBy() {
    psu::changePowerState(false);
}

static bool g_restart;

void restart() {
    g_restart = true;
    shutdown();
}

void shutdown() {
    using namespace psu;

    if (!isPsuThread()) {
        psu::gui::showSavingPage();
        sendMessageToPsu(PSU_MESSAGE_SHUTDOWN);
        return;
    }

    event_queue::pushEvent(g_restart ? event_queue::EVENT_INFO_SYSTEM_RESTART : event_queue::EVENT_INFO_SYSTEM_SHUTDOWN);

    osDelay(50);

    g_shutdownInProgress = true;

#if !defined(__EMSCRIPTEN__)
    // shutdown SCPI thread
    using namespace eez::scpi;
    sendMessageToLowPriorityThread(THREAD_MESSAGE_SHUTDOWN);
    do {
        osDelay(10);
    } while (isLowPriorityThreadAlive());
#endif

    profile::shutdownSave();

    if (psu::isPowerUp()) {
    	psu::changePowerState(false);
    }

    osDelay(50);

    while (persist_conf::saveAllDirtyBlocks()) {
        delay(1);
    }

    event_queue::shutdownSave();

    // save on-time counters
    persist_conf::writeTotalOnTime(ontime::g_mcuCounter.getType(), ontime::g_mcuCounter.getTotalTime());
    for (int slotIndex = 0; slotIndex < NUM_SLOTS; slotIndex++) {
        if (g_slots[slotIndex].moduleInfo->moduleType != MODULE_TYPE_NONE) {
            persist_conf::writeTotalOnTime(ontime::g_moduleCounters[slotIndex].getType(), ontime::g_moduleCounters[slotIndex].getTotalTime());
        }
    }

    if (g_restart) {
        delay(800);

#if defined(EEZ_PLATFORM_STM32)
        NVIC_SystemReset();
#endif

#if defined(EEZ_PLATFORM_SIMULATOR)
        psu::gui::showShutdownPage();
#endif

        g_shutdown = true;
    } else {
        psu::gui::showShutdownPage();

#if defined(EEZ_PLATFORM_SIMULATOR)
        osDelay(100);
#endif

        g_shutdown = true;
    }
}

}
