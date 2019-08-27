/*
 * EEZ Generic Firmware
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
#include <usb_device.h>

#include <dwt_stm32_delay.h>

#include "FreeRTOS.h"

#include <eez/system.h>

#include <eez/platform/stm32/bsp_sdram.h>
#include <eez/platform/stm32/defines.h>

extern "C" void SystemClock_Config(void);

extern "C" void vApplicationStackOverflowHook(TaskHandle_t xTask, signed char *pcTaskName ) {
    while (true) {}
}

//#if OPTION_SDRAM
//#include <assert.h>
//#include <stdlib.h>
//
//#define SDRAM_TEST_LOOPS 1
//#define SDRAM_TEST_STEP 64
//
//void SDRAM_Test() {
//    for (unsigned int seedValue = 0; seedValue < SDRAM_TEST_LOOPS; ++seedValue) {
//        {
//            uint32_t *p;
//
//            srand(seedValue);
//            p = (uint32_t *)0xc0000000;
//
//            for (int i = 0; i < 8 * 1024 * 1024 / 4; i += SDRAM_TEST_STEP) {
//                p[i] = (uint32_t)rand();
//            }
//
//            srand(seedValue);
//            p = (uint32_t *)0xc0000000;
//
//            for (int i = 0; i < 8 * 1024 * 1024 / 4; i += SDRAM_TEST_STEP) {
//                assert(p[i] == (uint32_t)rand());
//            }
//        }
//
//        {
//            uint16_t *p;
//
//            srand(seedValue);
//            p = (uint16_t *)0xc0000000;
//
//            for (int i = 0; i < 8 * 1024 * 1024 / 2; i += SDRAM_TEST_STEP) {
//                p[i] = (uint16_t)rand();
//            }
//
//            srand(seedValue);
//            p = (uint16_t *)0xc0000000;
//
//            for (int i = 0; i < 8 * 1024 * 1024 / 2; i += SDRAM_TEST_STEP) {
//                assert(p[i] == (uint16_t)rand());
//            }
//        }
//
//        {
//            uint8_t *p;
//
//            srand(seedValue);
//            p = (uint8_t *)0xc0000000;
//
//            for (int i = 0; i < 8 * 1024 * 1024; i += SDRAM_TEST_STEP) {
//                p[i] = (uint8_t)rand();
//            }
//
//            srand(seedValue);
//            p = (uint8_t *)0xc0000000;
//
//            for (int i = 0; i < 8 * 1024 * 1024; i += SDRAM_TEST_STEP) {
//                assert(p[i] == (uint8_t)rand());
//            }
//        }
//    }
//}
//#endif

int main(int argc, char **argv) {
    HAL_Init();

    SystemClock_Config();

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

#if OPTION_SDRAM
    /* SDRAM initialization sequence */
    BSP_SDRAM_Initialization_sequence(REFRESH_COUNT);
    // SDRAM_Test();
#endif

    DWT_Delay_Init();

    eez::boot();

    return 0;
}
