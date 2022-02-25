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
#include <stm32f7xx_hal.h>
#include <adc.h>
#include <crc.h>
#include <dac.h>
#include <dma.h>
#include <dma2d.h>
#include <fatfs.h>
#include <fmc.h>
#include <gpio.h>
#include <i2c.h>
#include <iwdg.h>
#include <ltdc.h>
#include <rng.h>
#include <rtc.h>
#include <sdmmc.h>
#include <spi.h>
#include <tim.h>
#include <usart.h>
#include <usb_device.h>
#endif

#include <eez/core/os.h>

#include <bb3/firmware.h>
#include <bb3/tasks.h>

#include <bb3/mcu/encoder.h>

#include <bb3/psu/psu.h>
#include <bb3/psu/serial_psu.h>
#include <bb3/psu/sd_card.h>

////////////////////////////////////////////////////////////////////////////////

#if !defined(__EMSCRIPTEN__)

void mainTask(void *);

EEZ_THREAD_DECLARE(main, Normal, 12 * 1024);

#endif

////////////////////////////////////////////////////////////////////////////////

#if defined(EEZ_PLATFORM_STM32)
extern "C" void SystemClock_Config(void);
#endif

#ifdef __EMSCRIPTEN__
void startEmscripten();
#endif

////////////////////////////////////////////////////////////////////////////////

#if defined(EEZ_PLATFORM_STM32)
uint32_t g_RCC_CSR;

#if CONF_SURVIVE_MODE
#if defined(EEZ_PLATFORM_STM32)
void disableDebugPins() {
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;

    /*Configure GPIO pin : PA13 PA14 */
    GPIO_InitStruct.Pin = GPIO_PIN_13|GPIO_PIN_14;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    /*Configure GPIO pin : PB3 */
    GPIO_InitStruct.Pin = GPIO_PIN_3;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_13, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_14, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_13, GPIO_PIN_RESET);  
}
#endif
#endif

#endif

int main(int argc, char **argv) {
#ifdef __EMSCRIPTEN__
    startEmscripten();
#else

#if defined(EEZ_PLATFORM_STM32)
    // Save reset flags
    g_RCC_CSR = RCC->CSR;
    // Clear reset flags
    RCC->CSR |= RCC_CSR_RMVF;

    // __HAL_RCC_DBGMCU_CLK_ENABLE();
    // __HAL_DBGMCU_FREEZE_IWDG();
    DBGMCU->APB1FZ |= DBGMCU_APB1_FZ_DBG_IWDG_STOP;

    MX_IWDG_Init();

    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();

#if CONF_SURVIVE_MODE
    disableDebugPins();
#endif

    MX_DMA_Init();
    MX_CRC_Init();
    MX_DAC_Init();
    MX_DMA2D_Init();
    MX_FMC_Init();
    MX_I2C1_Init();
    MX_LTDC_Init();
    MX_RNG_Init();

    if (HAL_IS_BIT_CLR(RTC->ISR, RTC_FLAG_INITS)) {
        DebugTrace("RTC init\n");
    }
    MX_RTC_Init();
    
    MX_SPI2_Init();
    MX_SPI4_Init();
    MX_SPI5_Init();
    // MX_TIM3_Init();
    MX_TIM6_Init();
    MX_TIM8_Init();

    MX_TIM7_Init();
    HAL_TIM_Base_Start_IT(&htim7);

    /* Enable I-Cache */
    SCB_EnableICache();

    /* Enable D-Cache */
    //SCB_EnableDCache();
#endif

    osKernelInitialize();

	EEZ_THREAD_CREATE(main, mainTask);

    osKernelStart();

#if defined(EEZ_PLATFORM_SIMULATOR)
    while (!eez::g_shutdown) {
#else
    while (true) {
#endif
        osDelay(100);
    }

#endif

    return 0;
}

////////////////////////////////////////////////////////////////////////////////

#if defined(EEZ_PLATFORM_SIMULATOR) && !defined(__EMSCRIPTEN__)
EEZ_THREAD_DECLARE(consoleInput, Normal, 1024);
void consoleInputTask(void *);
#endif

////////////////////////////////////////////////////////////////////////////////

#if !defined(__EMSCRIPTEN__)

void mainTask(void *) {
    eez::boot();

#if defined(EEZ_PLATFORM_SIMULATOR) && !defined(__EMSCRIPTEN__)
	EEZ_THREAD_CREATE(consoleInput, consoleInputTask);
#endif

    while (true) {
        osDelay(1000);
    }
}

#endif

////////////////////////////////////////////////////////////////////////////////

#if defined(EEZ_PLATFORM_STM32)

#if CONF_SURVIVE_MODE
namespace eez {
void readIntcapRegisterShortcut(int slotIndex);
}
#endif

extern "C" void SystemClock_Config(void);

extern "C" void vApplicationStackOverflowHook(TaskHandle_t xTask, signed char *pcTaskName) {
    while (true) {}
}

extern "C" void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) {
    using namespace eez;
	if (GPIO_Pin == SPI2_IRQ_Pin) {
// #if CONF_SURVIVE_MODE
//         if (g_slots[0]->moduleType == MODULE_TYPE_DCP405) {
//             readIntcapRegisterShortcut(0);
//         }
// #endif

        // if (g_isBooted) {
        //     sendMessageToPsu(PSU_MESSAGE_SPI_IRQ, 0);
        // } else {
            g_slots[0]->onSpiIrq();
        // }
    } else if (GPIO_Pin == SPI4_IRQ_Pin) {
// #if CONF_SURVIVE_MODE
//         if (g_slots[1]->moduleType == MODULE_TYPE_DCP405) {
//             readIntcapRegisterShortcut(1);
//         }
// #endif

        // if (g_isBooted) {
        //     sendMessageToPsu(PSU_MESSAGE_SPI_IRQ, 1);
        // } else {
            g_slots[1]->onSpiIrq();
        // }
    } else if (GPIO_Pin == SPI5_IRQ_Pin) {
// #if CONF_SURVIVE_MODE
//         if (g_slots[2]->moduleType == MODULE_TYPE_DCP405) {
//             readIntcapRegisterShortcut(2);
//         }
// #endif

        // if (g_isBooted) {
        //     sendMessageToPsu(PSU_MESSAGE_SPI_IRQ, 2);
        // } else {
            g_slots[2]->onSpiIrq();
        // }
    } else if (g_mcuRevision < MCU_REVISION_R3B3 && GPIO_Pin == R2B4_SD_DETECT_Pin) {
        eez::psu::sd_card::onSdDetectInterrupt();
    } else if (GPIO_Pin == ENC_A_Pin || GPIO_Pin == ENC_B_Pin) {
        eez::mcu::encoder::onPinInterrupt();
    }
}
#endif

#if defined(EEZ_PLATFORM_SIMULATOR) && !defined(__EMSCRIPTEN__)
void consoleInputTask(void *) {
    using namespace eez;
    sendMessageToLowPriorityThread(SERIAL_LINE_STATE_CHANGED, 1);

    while (1) {
        int ch = getchar();
        if (ch == EOF) {
            break;
        }
        Serial.put(ch);
    }
}
#endif

#ifdef __EMSCRIPTEN__
#include <stdio.h>
#include <string>

#include <emscripten.h>
#include <emscripten/bind.h>

#include <eez/gui/gui.h>
#include <eez/gui/thread.h>

#include <bb3/psu/psu.h>
#include <bb3/psu/serial_psu.h>

void eez_serial_put(int ch) {
    Serial.put(ch);
}

extern void eez_system_tick();

static int g_initialized = false;

// clang-format off
void mountFileSystem() {
    EM_ASM(
        FS.mkdir("/eez_modular_firmware");
        FS.mount(IDBFS, {}, "/eez_modular_firmware");

        //Module.print("start file sync..");

        //flag to check when data are synchronized
        Module.syncdone = 0;

        FS.syncfs(true, function(err) {
            assert(!err);
            //Module.print("end file sync..");
            Module.syncdone = 1;
        });
    , 0);
}
// clang-format on

EM_PORT_API(void) mainLoop() {
    if (emscripten_run_script_int("Module.syncdone") == 1) {
        if (!g_initialized) {
            g_initialized = true;
            eez::boot();
        } else {
            eez_system_tick();

            while (1) {
                int ch = getchar();
                if (ch == 0 || ch == EOF) {
                    break;
                }
                eez_serial_put(ch);
            }

            // clang-format off
            EM_ASM(
                if (Module.syncdone) {
                    //Module.print("Start File sync..");
                    Module.syncdone = 0;

                    FS.syncfs(false, function(err) {
                        assert(!err);
                        //Module.print("End File sync..");
                        Module.syncdone = 1;
                    });
                }
            , 0);
            // clang-format on
        }
    }
}

void startEmscripten() {
    mountFileSystem();
}

std::string getExceptionMessage(intptr_t exceptionPtr) {
    return std::string(reinterpret_cast<std::exception *>(exceptionPtr)->what());
}

void onDebuggerClientConnected() {
    using namespace eez::gui;
    sendMessageToGuiThread(GUI_QUEUE_MESSAGE_DEBUGGER_CLIENT_CONNECTED);
}

void onDebuggerClientDisconnected() {
    using namespace eez::gui;
    sendMessageToGuiThread(GUI_QUEUE_MESSAGE_DEBUGGER_CLIENT_DISCONNECTED);
}

EMSCRIPTEN_BINDINGS(Bindings) {
    emscripten::function("getExceptionMessage", &getExceptionMessage);
    emscripten::function("onDebuggerClientConnected", &onDebuggerClientConnected);
    emscripten::function("onDebuggerClientDisconnected", &onDebuggerClientDisconnected);
};

#endif
