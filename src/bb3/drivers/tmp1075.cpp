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

#include <math.h>
#include <i2c.h>
#include <bb3/drivers/tmp1075.h>
#include <eez/core/debug.h>
#include <eez/core/util.h>
#include <eez/core/os.h>

// TMP1075 Temperature Sensor With I2C
// http://www.ti.com/lit/ds/symlink/tmp1075.pdf

namespace eez {
namespace drivers {
namespace tmp1075 {

static const int DEVICE_ADDRESS[3] = { 
    0b10010010, 
    0b10010100, 
    0b10010110 
};

static const uint16_t REG_TEMP = 0x00;
static const uint16_t REG_CFGR = 0x01;
static const uint16_t REG_LLIM = 0x02;
static const uint16_t REG_HLIM = 0x03;
static const uint16_t REG_DIEID = 0x0F;

float readTemperature(uint8_t slotIndex) {
    uint8_t data[2];

//    HAL_I2C_Mem_Read(&hi2c1, DEVICE_ADDRESS[slotIndex], REG_DIEID, I2C_MEMADD_SIZE_8BIT, data, 2, 5);
//    uint16_t id = (data[0] << 8) + data[1];
//    if (id != 0x7500) {
//        DebugTrace("tmp1075 incorrect DIE ID (expect 0x7500, got 0x%X) or bad I2C comms on slot %d\n", (int)id, slotIndex + 1);
//        return NAN;
//    }

    taskENTER_CRITICAL();
    HAL_StatusTypeDef returnValue = HAL_I2C_Mem_Read(&hi2c1, DEVICE_ADDRESS[slotIndex], REG_TEMP, I2C_MEMADD_SIZE_8BIT, data, 2, 5);
    taskEXIT_CRITICAL();

    if (returnValue != HAL_OK) {
        return NAN;
    }

	float temperature = roundPrec(((data[0] << 4) | (data[1] >> 4)) * 0.0625, 1);
    //DebugTrace("T on slot %d: %f\n", slotIndex + 1, temperature);
    return temperature;
}

} // namespace tmp1075
} // namespace drivers
} // namespace eez

#endif // EEZ_PLATFORM_STM32
