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
#include <ltdc.h>
#include <rng.h>
#include <rtc.h>
#include <sdmmc.h>
#include <spi.h>
#include <tim.h>
#include <usart.h>
#include <usb_device.h>

#include "FreeRTOS.h"

#include <eez/platform/stm32/defines.h>
#include <eez/platform/stm32/dwt_delay.h>
#endif

#include <eez/system.h>

#include <eez/apps/psu/psu.h>
#include <eez/apps/psu/init.h>

#if defined(EEZ_PLATFORM_STM32)
extern "C" void SystemClock_Config(void);

extern "C" void vApplicationStackOverflowHook(TaskHandle_t xTask, signed char *pcTaskName ) {
    while (true) {}
}

extern "C" void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) {
    int slotIndex;
    if (GPIO_Pin == SPI2_IRQ_Pin) {
    	slotIndex = 0;
    } else if (GPIO_Pin == SPI4_IRQ_Pin) {
    	slotIndex = 1;
    } else if (GPIO_Pin == SPI5_IRQ_Pin) {
    	slotIndex = 2;
    } else {
        return;
    }
    
    osMessagePut(eez::psu::g_psuMessageQueueId, PSU_QUEUE_MESSAGE(PSU_QUEUE_MESSAGE_SPI_IRQ, slotIndex), 0);
}

#endif

#ifdef __EMSCRIPTEN__
#include <stdio.h>
#include <emscripten.h>
#include <eez/apps/psu/psu.h>
#include <eez/apps/psu/serial_psu.h>

void eez_serial_put(int ch) {
    Serial.put(ch);
}

extern void eez_system_tick();

static int g_initialized = false;

// clang-format off
void mount_file_system() {
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

void main_loop() {
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
                    
                    FS.syncfs(false, function (err) {
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
#endif

int main(int argc, char **argv) {
#if defined(EEZ_PLATFORM_STM32)
    HAL_Init();

    SystemClock_Config();

    // DWT_Delay_Init();

    MX_GPIO_Init();
    MX_DMA_Init();
    MX_ADC1_Init();
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

#ifdef __EMSCRIPTEN__
    mount_file_system();
    emscripten_set_main_loop(main_loop, 0, 1);
#else
    eez::boot();
#endif

    return 0;
}
