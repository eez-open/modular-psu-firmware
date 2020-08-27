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

#pragma once

/*! \page module_eeprom_map Module EEPROM map

# Overview

|Address|Size|Description                               |
|-------|----|------------------------------------------|
|0      |   2|Module type                               |
|2      |   2|Module revision                           |
|4      |   2|0xA5A5 if firmware is installed           |
|40     |  24|[ON-time counter](#ontime-counter)        |
|64     |  64|[Module configuration](#module-congiguration)|
|128    | 144|CH1 [calibration parameters](#calibration)|
|272    | 144|CH2 [calibration parameters](#calibration)|

## <a name="ontime-counter">ON-time counter</a>

|Offset|Size|Type                     |Description                  |
|------|----|-------------------------|-----------------------------|
|0     |4   |int                      |1st magic number             |
|4     |4   |int                      |1st counter                  |
|8     |4   |int                      |1st counter (copy)           |
|12    |4   |int                      |2nd magic number             |
|16    |4   |int                      |2nd counter                  |
|20    |4   |int                      |2bd counter (copy)           |

## <a name="block-header">Block header</a>

|Offset|Size|Type|Description|
|------|----|----|-----------|
|0     |4   |int |Checksum   |
|4     |2   |int |Version    |

## <a name="device">Module configuration</a>

|Offset|Size|Type                     |Description                  |
|------|----|-------------------------|-----------------------------|
|0     |6   |[struct](#block-header)  |[Block header](#block-header)|
|6     |1   |int                      |Channel calibration enabled  |

## <a name="calibration">Calibration parameters</a>

|Offset|Size|Type                   |Description                  |
|------|----|-----------------------|-----------------------------|
|0     |8   |[struct](#block-header)|[Block header](#block-header)|
|8     |4   |[bitarray](#cal-flags) |[Flags](#cal-flags)          |
|12    |44  |[struct](#cal-points)  |Voltage [points](#cal-points)|
|56    |44  |[struct](#cal-points)  |Current [points](#cal-points)|
|100   |9   |string                 |Date                         |
|109   |33  |string                 |Remark                       |

#### <a name="cal-flags">Calibration flags</a>

|Bit|Description        |
|---|-------------------|
|0  |Voltage calibrated?|
|1  |Current calibrated?|

#### <a name="cal-points">Value points</a>

|Offset|Size|Type                |Description            |
|------|----|--------------------|-----------------------|
|0     |12  |[struct](#cal-point)|Min [point](#cal-point)|
|12    |12  |[struct](#cal-point)|Mid [point](#cal-point)|
|24    |12  |[struct](#cal-point)|Max [point](#cal-point)|
|36    |4   |[struct](#cal-point)|Min. set value possible|
|40    |4   |[struct](#cal-point)|Max. set value possible|

#### <a name="cal-point">Value point</a>

|Offset|Size|Type |Description|
|------|----|-----|-----------|
|0     |4   |float|DAC value  |
|4     |4   |float|Real value |
|8     |4   |float|ADC value  |

*/

namespace eez {
namespace bp3c {
namespace eeprom {

static const uint16_t EEPROM_ONTIME_START_ADDRESS = 40;
static const uint16_t EEPROM_SIZE = 4096;

void init();
bool test();

extern TestResult g_testResult;

bool read(uint8_t slotIndex, uint8_t *buffer, uint16_t buffer_size, uint16_t address);
bool write(uint8_t slotIndex, const uint8_t *buffer, uint16_t buffer_size, uint16_t address);

void writeModuleType(uint8_t slotIndex, uint16_t moduleType);

void resetAllExceptOnTimeCounters(uint8_t slotIndex);

} // namespace eeprom
} // namespace bp3c
} // namespace eez
