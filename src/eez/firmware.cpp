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
#include <stdlib.h>

#if defined(EEZ_PLATFORM_STM32)
#include <main.h>
#include <usb_device.h>
#endif

#include <eez/firmware.h>
#include <eez/alloc.h>
#include <eez/system.h>
#include <eez/tasks.h>
#include <eez/scripting/scripting.h>
#include <eez/sound.h>
#include <eez/memory.h>
#include <eez/uart.h>
#include <eez/usb.h>
#include <eez/modules/aux_ps/fan.h>

#include <eez/gui/gui.h>

#include <eez/modules/psu/psu.h>
#include <eez/modules/psu/channel_dispatcher.h>
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

int g_mcuRevision;
#if defined(EEZ_PLATFORM_STM32)
extern "C" void SystemClock_Config_R2B4();
extern "C" void MX_GPIO_Init_R2B4();
extern "C" void MX_TIM12_Init();
extern "C" void MX_TIM4_Init();
#endif

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
#if OPTION_ETHERNET
    psu::ethernet::initScpi();
#endif

    psu::event_queue::init();

#if defined(EEZ_PLATFORM_STM32)
    mcu::sdram::init();
    //mcu::sdram::test();
#endif

    initAllocHeap(ALLOC_BUFFER, ALLOC_BUFFER_SIZE);

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

#if CONF_SURVIVE_MODE
    g_mcuRevision = MCU_REVISION_R3B3;
    psu::sd_card::init();
#endif

    mcu::eeprom::init();
    mcu::eeprom::test();

    bp3c::eeprom::init();
    bp3c::eeprom::test();

    bp3c::io_exp::init();

#if !CONF_SURVIVE_MODE
    psu::ontime::g_mcuCounter.init();
#endif

    psu::persist_conf::init();

    gui::startThread();

#if !CONF_SURVIVE_MODE
    if (psu::persist_conf::devConf.mcuRevisionTag == MCU_REVISION_TAG && psu::persist_conf::devConf.mcuRevision != 0) {
        g_mcuRevision = psu::persist_conf::devConf.mcuRevision;
    } else {
#ifdef __EMSCRIPTEN__
        g_mcuRevision = MCU_REVISION_R3B3;
#else

#if defined(EEZ_PLATFORM_STM32)
        GPIO_InitTypeDef GPIO_InitStruct = {0};

        HAL_GPIO_WritePin(LCD_BRIGHTNESS_GPIO_Port, LCD_BRIGHTNESS_Pin, GPIO_PIN_SET);
        GPIO_InitStruct.Pin = LCD_BRIGHTNESS_Pin;
        GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
        HAL_GPIO_Init(LCD_BRIGHTNESS_GPIO_Port, &GPIO_InitStruct);

        HAL_GPIO_WritePin(R2B4_LCD_BRIGHTNESS_GPIO_Port, R2B4_LCD_BRIGHTNESS_Pin, GPIO_PIN_SET);
        GPIO_InitStruct.Pin = R2B4_LCD_BRIGHTNESS_Pin;
        GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
        HAL_GPIO_Init(R2B4_LCD_BRIGHTNESS_GPIO_Port, &GPIO_InitStruct);
#endif

        g_mcuRevision = psu::gui::askMcuRevision();
#endif
    }

    psu::sd_card::init();
#endif

#if defined(EEZ_PLATFORM_STM32)
    if (g_mcuRevision < MCU_REVISION_R3B3) {
        SystemClock_Config_R2B4();
        MX_GPIO_Init_R2B4();
        MX_TIM12_Init();
    } else {
        MX_TIM4_Init();
    }
    mcu::display::turnOff();
    mcu::display::turnOn();
#endif

    int numInstalledModules = 0;
    for (uint8_t slotIndex = 0; slotIndex < NUM_SLOTS; slotIndex++) {
        uint16_t prolog[3];
        assert(sizeof(prolog) <= bp3c::eeprom::EEPROM_PROLOG_SIZE);
        if (!bp3c::eeprom::read(slotIndex, (uint8_t *)prolog, bp3c::eeprom::EEPROM_PROLOG_SIZE, bp3c::eeprom::EEPROM_PROLOG_START_ADDRESS)) {
            g_slots[slotIndex]->setTestResult(TEST_SKIPPED);
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
#if !CONF_SURVIVE_MODE
            psu::ontime::g_moduleCounters[slotIndex].init();
#endif
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

    psu::gui::showWelcomePage();

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

    scripting::initMessageQueue();
    scripting::startThread();

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

    if (g_numMasterErrors > 0) {
		ErrorTrace("MCU errors: %s\n", g_masterErrorMessage);
    }

    // DebugTrace("Memory left: %d\n", MEMORY_SIZE - (uint32_t)(MEMORY_END - MEMORY_BEGIN));
    // DebugTrace("sizeof(Value) = %d\n", sizeof(Value));

    // Value value;
    // value.type_ = VALUE_TYPE_NULL;
    // value.unit_ = 2;
    // value.options_ = 3;
    // value.int64_ = 4;
    // uint8_t *pValue = (uint8_t *)&value;
    // DebugTrace("%d,%d,%d,%d, %d,%d,%d,%d, %d,%d,%d,%d, %d,%d,%d,%d\n",
    //     pValue[0], pValue[1], pValue[2], pValue[3],
    //     pValue[4], pValue[5], pValue[6], pValue[7],
    //     pValue[8], pValue[9], pValue[10], pValue[11],
    //     pValue[12], pValue[13], pValue[14], pValue[15]
    // );
}

bool testMaster() {
	g_numMasterErrors = 0;
    g_masterErrorMessage[0] = 0;

#if OPTION_FAN
    if (!aux_ps::fan::test()) {
        g_numMasterErrors++;
        stringAppendString(g_masterErrorMessage, MASTER_ERROR_MESSAGE_SIZE, g_masterErrorMessage[0] ? ", FAN" : "FAN");
    }
#endif

    if (!psu::rtc::test()) {
        g_numMasterErrors++;
        stringAppendString(g_masterErrorMessage, MASTER_ERROR_MESSAGE_SIZE, g_masterErrorMessage[0] ? ", RTC" : "RTC");
    }

    if (!psu::datetime::test()) {
        g_numMasterErrors++;
        stringAppendString(g_masterErrorMessage, MASTER_ERROR_MESSAGE_SIZE, g_masterErrorMessage[0] ? ", DateTime" : "DateTime");
    }

    if (!mcu::eeprom::test()) {
        g_numMasterErrors++;
        stringAppendString(g_masterErrorMessage, MASTER_ERROR_MESSAGE_SIZE, g_masterErrorMessage[0] ? ", EEPROM" : "EEPROM");
    }

    if (!psu::sd_card::test()) {
        g_numMasterErrors++;
        stringAppendString(g_masterErrorMessage, MASTER_ERROR_MESSAGE_SIZE, g_masterErrorMessage[0] ? ", SD card" : "SD card");
    }

#if OPTION_ETHERNET
    if (!psu::ethernet::test()) {
        g_numMasterErrors++;
        stringAppendString(g_masterErrorMessage, MASTER_ERROR_MESSAGE_SIZE, g_masterErrorMessage[0] ? ", ETH" : "ETH");
    }
#endif

    if (!psu::temperature::test()) {
        g_numMasterErrors++;
        stringAppendString(g_masterErrorMessage, MASTER_ERROR_MESSAGE_SIZE, g_masterErrorMessage[0] ? ", TEMP" : "TEMP");
    }

    if (!mcu::battery::test()) {
        g_numMasterErrors++;
        stringAppendString(g_masterErrorMessage, MASTER_ERROR_MESSAGE_SIZE, g_masterErrorMessage[0] ? ", BATT" : "BATT");
    }

    // test BP3c
    if (!bp3c::io_exp::test()) {
        g_numMasterErrors++;
        stringAppendString(g_masterErrorMessage, MASTER_ERROR_MESSAGE_SIZE, g_masterErrorMessage[0] ? ", BP3C" : "BP3C");
    }

	if (g_numMasterErrors > 0) {
		g_masterTestResult = TEST_FAILED;
		return false;
	}

	g_masterTestResult = TEST_OK;
    return true;
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
    sendMessageToLowPriorityThread(THREAD_MESSAGE_PROFILE_SAVE);

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

    uart::disable();

#if !defined(__EMSCRIPTEN__)
    // shutdown SCPI thread
    using namespace eez::scpi;
    sendMessageToLowPriorityThread(THREAD_MESSAGE_SHUTDOWN);
    do {
        osDelay(10);
        WATCHDOG_RESET(WATCHDOG_LONG_OPERATION);
    } while (isLowPriorityThreadAlive());
#endif

    profile::saveIfDirty();

    channel_dispatcher::disableOutputForAllChannels();
    psu::tick();

    if (psu::isPowerUp()) {
    	psu::changePowerState(false);
    }

    osDelay(50);

    while (persist_conf::saveAllDirtyBlocks()) {
        delay(1);
        WATCHDOG_RESET(WATCHDOG_LONG_OPERATION);
    }

    event_queue::shutdownSave();

#if !CONF_SURVIVE_MODE
    // save on-time counters
    persist_conf::writeTotalOnTime(ontime::g_mcuCounter.getType(), ontime::g_mcuCounter.getTotalTime());
    for (int slotIndex = 0; slotIndex < NUM_SLOTS; slotIndex++) {
        if (g_slots[slotIndex]->moduleType != MODULE_TYPE_NONE) {
            persist_conf::writeTotalOnTime(ontime::g_moduleCounters[slotIndex].getType(), ontime::g_moduleCounters[slotIndex].getTotalTime());
        }
        WATCHDOG_RESET(WATCHDOG_LONG_OPERATION);
    }
#endif

    //
    for (int slotIndex = 0; slotIndex < NUM_SLOTS; slotIndex++) {
        g_slots[slotIndex]->writeUnsavedData();
        WATCHDOG_RESET(WATCHDOG_LONG_OPERATION);
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

        osDelay(100);

        g_shutdown = true;

#if defined(EEZ_PLATFORM_STM32)
        while (1) {
        	WATCHDOG_RESET(WATCHDOG_LONG_OPERATION);
        	psu::tick();
        	osDelay(1);
        }
#endif
    }
}

void updateSpiIrqPin(int slotIndex) {
	if (slotIndex >= 0 && slotIndex < 3) {
#if defined(EEZ_PLATFORM_STM32)
		GPIO_InitTypeDef GPIO_InitStruct = {0};
#endif

		TestResult testResult = g_slots[slotIndex]->getTestResult();
		
		static bool irqDisabled[NUM_SLOTS];

		if (testResult == TEST_SKIPPED || testResult == TEST_FAILED) {
			if (!irqDisabled[slotIndex]) {
				irqDisabled[slotIndex] = true;

#if defined(EEZ_PLATFORM_STM32)
				GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
				GPIO_InitStruct.Pull = GPIO_PULLUP;
#endif
			}
		} else {
			if (irqDisabled[slotIndex]) {
				irqDisabled[slotIndex] = false;

#if defined(EEZ_PLATFORM_STM32)
				GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
				GPIO_InitStruct.Pull = GPIO_PULLUP;
#endif
			}
		}

		if (slotIndex == 0) {
#if defined(EEZ_PLATFORM_STM32)
			GPIO_InitStruct.Pin = SPI2_IRQ_Pin;
			HAL_GPIO_Init(SPI2_IRQ_GPIO_Port, &GPIO_InitStruct);
#endif
		} else if (slotIndex == 1) {
#if defined(EEZ_PLATFORM_STM32)
			GPIO_InitStruct.Pin = SPI4_IRQ_Pin;
			HAL_GPIO_Init(SPI4_IRQ_GPIO_Port, &GPIO_InitStruct);
#endif
		} else if (slotIndex == 2) {
#if defined(EEZ_PLATFORM_STM32)
			GPIO_InitStruct.Pin = SPI5_IRQ_Pin;
			HAL_GPIO_Init(SPI5_IRQ_GPIO_Port, &GPIO_InitStruct);
#endif
		}
	}

#if defined(EEZ_PLATFORM_STM32)
	HAL_NVIC_EnableIRQ(EXTI9_5_IRQn);
	HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);
#endif
}

} // namespace eez
