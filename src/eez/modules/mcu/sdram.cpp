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

#if OPTION_SDRAM

#include <eez/modules/mcu/sdram.h>

#if defined(EEZ_PLATFORM_STM32)

#include <assert.h>
#include <stdlib.h>
#include <stm32f7xx_hal.h>
#include <fmc.h>

#define REFRESH_COUNT ((uint32_t)1386) /* SDRAM refresh counter */

#define SDRAM_TIMEOUT ((uint32_t)0xFFFF)

#define SDRAM_MODEREG_BURST_LENGTH_1 ((uint16_t)0x0000)
#define SDRAM_MODEREG_BURST_TYPE_SEQUENTIAL ((uint16_t)0x0000)
#define SDRAM_MODEREG_CAS_LATENCY_3 ((uint16_t)0x0030)
#define SDRAM_MODEREG_OPERATING_MODE_STANDARD ((uint16_t)0x0000)
#define SDRAM_MODEREG_WRITEBURST_MODE_SINGLE ((uint16_t)0x0200)

#endif

namespace eez {
namespace mcu {
namespace sdram {

#if defined(EEZ_PLATFORM_STM32)

/**
 * @brief  Programs the SDRAM device.
 * @param  RefreshCount: SDRAM refresh counter value
 */
void BSP_SDRAM_Initialization_sequence(uint32_t RefreshCount) {
    FMC_SDRAM_CommandTypeDef Command;

    /* Step 1:  Configure a clock configuration enable command */
    Command.CommandMode = FMC_SDRAM_CMD_CLK_ENABLE;
    Command.CommandTarget = FMC_SDRAM_CMD_TARGET_BANK1;
    Command.AutoRefreshNumber = 1;
    Command.ModeRegisterDefinition = 0;

    /* Send the command */
    HAL_SDRAM_SendCommand(&hsdram1, &Command, SDRAM_TIMEOUT);

    /* Step 2: Insert 100 us minimum delay */
    /* Inserted delay is equal to 1 ms due to systick time base unit (ms) */
    HAL_Delay(1);

    /* Step 3: Configure a PALL (precharge all) command */
    Command.CommandMode = FMC_SDRAM_CMD_PALL;
    Command.CommandTarget = FMC_SDRAM_CMD_TARGET_BANK1;
    Command.AutoRefreshNumber = 1;
    Command.ModeRegisterDefinition = 0;

    /* Send the command */
    HAL_SDRAM_SendCommand(&hsdram1, &Command, SDRAM_TIMEOUT);

    /* Step 4: Configure an Auto Refresh command */
    Command.CommandMode = FMC_SDRAM_CMD_AUTOREFRESH_MODE;
    Command.CommandTarget = FMC_SDRAM_CMD_TARGET_BANK1;
    Command.AutoRefreshNumber = 4;
    Command.ModeRegisterDefinition = 0;

    /* Send the command */
    HAL_SDRAM_SendCommand(&hsdram1, &Command, SDRAM_TIMEOUT);

    /* Step 5: Program the external memory mode register */
    __IO uint32_t tmpmrd = (uint32_t)SDRAM_MODEREG_BURST_LENGTH_1 |
                           SDRAM_MODEREG_BURST_TYPE_SEQUENTIAL | SDRAM_MODEREG_CAS_LATENCY_3 |
                           SDRAM_MODEREG_OPERATING_MODE_STANDARD |
                           SDRAM_MODEREG_WRITEBURST_MODE_SINGLE;

    Command.CommandMode = FMC_SDRAM_CMD_LOAD_MODE;
    Command.CommandTarget = FMC_SDRAM_CMD_TARGET_BANK1;
    Command.AutoRefreshNumber = 1;
    Command.ModeRegisterDefinition = tmpmrd;

    /* Send the command */
    HAL_SDRAM_SendCommand(&hsdram1, &Command, SDRAM_TIMEOUT);

    /* Step 6: Set the refresh rate counter */
    /* Set the device refresh rate */
    HAL_SDRAM_ProgramRefreshRate(&hsdram1, RefreshCount);
}

#define SDRAM_TEST_LOOPS 1
#define SDRAM_TEST_STEP 1024

bool SDRAM_Test() {
   for (unsigned int seedValue = 0; seedValue < SDRAM_TEST_LOOPS; ++seedValue) {
//       {
//           uint32_t *p;
//
//           srand(seedValue);
//           p = (uint32_t *)0xc0000000;
//
//           for (int i = 0; i < 8 * 1024 * 1024 / 4; i += SDRAM_TEST_STEP) {
//               p[i] = (uint32_t)rand();
//           }
//
//           srand(seedValue);
//           p = (uint32_t *)0xc0000000;
//
//           for (int i = 0; i < 8 * 1024 * 1024 / 4; i += SDRAM_TEST_STEP) {
//               if (p[i] != (uint32_t)rand()) {
//                   return false;
//               }
//           }
//       }
//
//       {
//           uint16_t *p;
//
//           srand(seedValue);
//           p = (uint16_t *)0xc0000000;
//
//           for (int i = 0; i < 8 * 1024 * 1024 / 2; i += SDRAM_TEST_STEP) {
//               p[i] = (uint16_t)rand();
//           }
//
//           srand(seedValue);
//           p = (uint16_t *)0xc0000000;
//
//           for (int i = 0; i < 8 * 1024 * 1024 / 2; i += SDRAM_TEST_STEP) {
//               if (p[i] != (uint16_t)rand()) {
//                   return false;
//               }
//           }
//       }
//
       {
           uint8_t *p;

           srand(seedValue);
           p = (uint8_t *)0xc0000000;

           for (int i = 0; i < 8 * 1024 * 1024; i += SDRAM_TEST_STEP) {
               p[i] = (uint8_t)rand();
           }

           srand(seedValue);
           p = (uint8_t *)0xc0000000;

           for (int i = 0; i < 8 * 1024 * 1024; i += SDRAM_TEST_STEP) {
               if (p[i] != (uint8_t)rand()) {
                   return false;
               }
           }
       }
   }

   return true;
}

#endif

TestResult g_testResult;

void init() {
#if defined(EEZ_PLATFORM_STM32)
    BSP_SDRAM_Initialization_sequence(REFRESH_COUNT);
#endif
}

bool test() {
#if defined(EEZ_PLATFORM_STM32)
    g_testResult = SDRAM_Test() ? TEST_OK : TEST_FAILED;
#endif

#if defined(EEZ_PLATFORM_SIMULATOR)
    g_testResult = TEST_OK;
#endif

    return g_testResult == TEST_OK;
}

} // namespace sdram
} // namespace mcu
} // namespace eez

#endif
