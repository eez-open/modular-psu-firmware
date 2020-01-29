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

#include <eez/firmware.h>

#include <eez/scpi/scpi.h>

#include <eez/modules/psu/psu.h>
#include <eez/modules/psu/serial_psu.h>
#if OPTION_SD_CARD
#include <eez/modules/psu/sd_card.h>
#endif

 ////////////////////////////////////////////////////////////////////////////////

#if !defined(__EMSCRIPTEN__)

void mainTask(const void *);

#if defined(EEZ_PLATFORM_STM32)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wwrite-strings"
#endif

osThreadDef(g_mainTask, mainTask, osPriorityNormal, 0, 2048);

#if defined(EEZ_PLATFORM_STM32)
#pragma GCC diagnostic pop
#endif

osThreadId g_mainTaskHandle;

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
bool g_isResetByIWDG;
#endif

int main(int argc, char **argv) {
#ifdef __EMSCRIPTEN__
    startEmscripten();
#else

#if defined(EEZ_PLATFORM_STM32)
	if (RCC->CSR & RCC_CSR_IWDGRSTF) {	
		/* Reset by IWDG */
		g_isResetByIWDG = true;
		/* Clear reset flags */
		RCC->CSR |= RCC_CSR_RMVF;
	}
    MX_IWDG_Init();

    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_DMA_Init();
    MX_CRC_Init();
    MX_DAC_Init();
    MX_DMA2D_Init();
    MX_FMC_Init();
    MX_I2C1_Init();
    MX_LTDC_Init();
    MX_RNG_Init();
    MX_RTC_Init();
    MX_SPI2_Init();
    MX_SPI4_Init();
    MX_SPI5_Init();
    MX_TIM6_Init();
    MX_TIM8_Init();
    MX_TIM12_Init();
#endif

    g_mainTaskHandle = osThreadCreate(osThread(g_mainTask), nullptr);

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
void consoleInputTask(const void *);
osThreadDef(g_consoleInputTask, consoleInputTask, osPriorityNormal, 0, 1024);
osThreadId g_consoleInputTaskHandle;
#endif

////////////////////////////////////////////////////////////////////////////////

#if !defined(__EMSCRIPTEN__)

void mainTask(const void *) {
    eez::boot();

#if defined(EEZ_PLATFORM_SIMULATOR) && !defined(__EMSCRIPTEN__)
    g_consoleInputTaskHandle = osThreadCreate(osThread(g_consoleInputTask), nullptr);
#endif

    while (true) {
        osDelay(1000);
    }
}

#endif

////////////////////////////////////////////////////////////////////////////////

#if defined(EEZ_PLATFORM_STM32)
extern "C" void SystemClock_Config(void);

extern "C" void vApplicationStackOverflowHook(TaskHandle_t xTask, signed char *pcTaskName) {
    while (true) {}
}

extern "C" void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) {
    int slotIndex = -1;

    if (GPIO_Pin == SPI2_IRQ_Pin) {
        slotIndex = 0;
    } else if (GPIO_Pin == SPI4_IRQ_Pin) {
        slotIndex = 1;
    } else if (GPIO_Pin == SPI5_IRQ_Pin) {
        slotIndex = 2;
    }

#if OPTION_SD_CARD
    else if (GPIO_Pin == SD_DETECT_Pin) {
        eez::psu::sd_card::onSdDetectInterrupt();
        return;
    }
#endif

    if (slotIndex != -1) {
        osMessagePut(eez::psu::g_psuMessageQueueId, PSU_QUEUE_MESSAGE(PSU_QUEUE_MESSAGE_SPI_IRQ, slotIndex), 0);
    }
}
#endif

#if defined(EEZ_PLATFORM_SIMULATOR) && !defined(__EMSCRIPTEN__)
void consoleInputTask(const void *) {
    using namespace eez::scpi;
    osMessagePut(eez::scpi::g_scpiMessageQueueId, SCPI_QUEUE_SERIAL_MESSAGE(SERIAL_LINE_STATE_CHANGED, 1), osWaitForever);

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
#include <emscripten.h>
#include <eez/modules/psu/psu.h>
#include <eez/modules/psu/serial_psu.h>

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
    );
}
// clang-format on

void mainLoop() {
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
            );
            // clang-format on
        }
    }
}

void startEmscripten() {
    mountFileSystem();
    emscripten_set_main_loop(mainLoop, 4, true);
}
#endif
