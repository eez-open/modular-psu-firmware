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
#include <eez/usb.h>

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
extern uint32_t g_RCC_CSR;
#endif

namespace eez {

bool g_isBooted;
bool g_bootTestSuccess;
bool g_shutdownInProgress;
bool g_shutdown;

void boot() {
    assert((uint32_t)(MEMORY_END - MEMORY_BEGIN) <= MEMORY_SIZE);

    psu::serial::initScpi();
    psu::ethernet::initScpi();

    psu::event_queue::init();

#if defined(EEZ_PLATFORM_STM32)
    mcu::sdram::init();
    //mcu::sdram::test();
#endif

#ifdef EEZ_PLATFORM_SIMULATOR
    eez::psu::simulator::init();
#endif

#if OPTION_ETHERNET
    mcu::ethernet::initMessageQueue();
#endif
    initHighPriorityMessageQueue();
    initLowPriorityMessageQueue();

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

    psu::ontime::g_mcuCounter.init();

    psu::persist_conf::init();

    int numInstalledModules = 0;
    for (uint8_t slotIndex = 0; slotIndex < NUM_SLOTS; slotIndex++) {
        uint16_t prolog[3];
        assert(sizeof(prolog) <= bp3c::eeprom::EEPROM_PROLOG_SIZE);
        if (!bp3c::eeprom::read(slotIndex, (uint8_t *)prolog, bp3c::eeprom::EEPROM_PROLOG_SIZE, bp3c::eeprom::EEPROM_PROLOG_START_ADDRESS)) {
            prolog[0] = MODULE_TYPE_NONE;
            prolog[1] = 0;
            prolog[2] = 0;
        }
        uint16_t moduleType = prolog[0];
        uint16_t moduleRevision = prolog[1];
        bool firmwareInstalled = prolog[2] == 0xA5A5;
        
        g_slots[slotIndex] = getModule(moduleType)->createModule();
        g_slots[slotIndex]->slotIndex = slotIndex;
        g_slots[slotIndex]->enabled = true;
        g_slots[slotIndex]->moduleRevision = moduleRevision;
        g_slots[slotIndex]->firmwareInstalled = firmwareInstalled;
        
        if (g_slots[slotIndex]->moduleType != MODULE_TYPE_NONE) {
            g_slots[slotIndex]->enabled = psu::persist_conf::isSlotEnabled(slotIndex);
            psu::persist_conf::loadModuleConf(slotIndex);
            psu::ontime::g_moduleCounters[slotIndex].init();

            numInstalledModules++;
        }

        g_slots[slotIndex]->boot();
    }

    if (numInstalledModules == 1) {
        g_isCol2Mode = true;

        if (g_slots[0]->moduleType == MODULE_TYPE_NONE) {
            int i;
            for (i = 1; i < NUM_SLOTS; i++) {
                if (g_slots[i]->moduleType != MODULE_TYPE_NONE) {
                    g_slotIndexes[0] = i - 1;
                    g_slotIndexes[1] = i;
                    break;
                }
            }

            int k = 0;
            for (int j = 0; j < i - 1; j++) {
                g_slotIndexes[k++] = j;
            }
            for (int j = i + 1; j < NUM_SLOTS; j++) {
                g_slotIndexes[k++] = j;
            }
        }
    } else if (numInstalledModules == 2)  {
        g_isCol2Mode = true;

        int j = 0;
        for (int i = 0; i < NUM_SLOTS; i++) {
            if (g_slots[i]->moduleType != MODULE_TYPE_NONE) {
                g_slotIndexes[j++] = i;
            } else {
                g_slotIndexes[2] = i;
            }
        }
    }

    psu::persist_conf::initChannels();

#if OPTION_DISPLAY
    gui::startThread();
    psu::gui::showWelcomePage();
#endif

    sound::init();

    usb::init();

    psu::serial::init();

    psu::profile::init();

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

    // TEST

    g_bootTestSuccess = true;

    g_bootTestSuccess &= testMaster();

    if (!psu::autoRecall()) {
        psu::psuReset();
        psu::io_pins::refresh();
    }

    // play beep if there is an error during boot procedure
    if (!g_bootTestSuccess) {
        sound::playBeep();
    }

    g_isBooted = true;

#if OPTION_ETHERNET
    mcu::ethernet::startThread();
#endif

    startHighPriorityThread();
    startLowPriorityThread();

    mp::initMessageQueue();
    mp::startThread();

#if defined(EEZ_PLATFORM_STM32)
    if (g_RCC_CSR & RCC_CSR_LPWRRSTF) {
        DebugTrace("Low-power reset flag\n");
    }
    if (g_RCC_CSR & RCC_CSR_WWDGRSTF) {
        DebugTrace("Window watchdog reset flag\n");
    }
    if (g_RCC_CSR & RCC_CSR_IWDGRSTF) {
        DebugTrace("Independent watchdog reset flag\n");
    }
    if (g_RCC_CSR & RCC_CSR_SFTRSTF) {
        DebugTrace("Software reset flag\n");
    }
    if (g_RCC_CSR & RCC_CSR_PORRSTF) {
        DebugTrace("POR/PDR reset flag\n");
    }
    if (g_RCC_CSR & RCC_CSR_PINRSTF) {
        DebugTrace("PIN reset flag\n");
    }
    if (g_RCC_CSR & RCC_CSR_BORRSTF) {
        DebugTrace("BOR reset flag\n");
    }

    if (g_RCC_CSR & RCC_CSR_IWDGRSTF) {	
        psu::event_queue::pushEvent(psu::event_queue::EVENT_ERROR_WATCHDOG_RESET);
    }
#endif
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
    psu::io_pins::refresh();

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

    bool result = psuReset();
    
    io_pins::refresh();

    return result;
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

#if OPTION_ETHERNET
    ethernet::update();
#endif

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
        if (g_slots[slotIndex]->moduleType != MODULE_TYPE_NONE) {
            persist_conf::writeTotalOnTime(ontime::g_moduleCounters[slotIndex].getType(), ontime::g_moduleCounters[slotIndex].getTotalTime());
        }
    }

    //
    for (int slotIndex = 0; slotIndex < NUM_SLOTS; slotIndex++) {
        g_slots[slotIndex]->writeUnsavedData();
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
