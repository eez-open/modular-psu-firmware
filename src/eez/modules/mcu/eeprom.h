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

/*! \page eeprom_map MCU EEPROM map

# Overview

|Address|Size|Description                               |
|-------|----|------------------------------------------|
|0      |  64|Not used                                  |
|64     |  24|[Total ON-time counter](#ontime-counter)  |
|1024   |  64|[Device configuration](#device)           |
|1536   | 128|[Device configuration 2](#device2)        |
|16384  | 610|[Event Queue](#event-queue)               |

## <a name="ontime-counter">ON-time counter</a>

|Offset|Size|Type                     |Description                  |
|------|----|-------------------------|-----------------------------|
|0     |4   |int                      |1st magic number             |
|4     |4   |int                      |1st counter                  |
|8     |4   |int                      |1st counter (copy)           |
|12    |4   |int                      |2nd magic number             |
|16    |4   |int                      |2nd counter                  |
|20    |4   |int                      |2bd counter (copy)           |

## <a name="device">Device configuration</a>

|Offset|Size|Type                     |Description                  |
|------|----|-------------------------|-----------------------------|
|0     |6   |[struct](#block-header)  |[Block header](#block-header)|
|8     |8   |string                   |Serial number                |
|16    |17  |string                   |Calibration password         |
|36    |4   |[bitarray](#device-flags)|[Device Flags](#device-flags)|
|40    |1   |int                      |Year                         |
|41    |1   |int                      |Month                        |
|42    |1   |int                      |Day                          |
|43    |1   |int                      |Hour                         |
|44    |1   |int                      |Minute                       |
|45    |1   |int                      |Second                       |
|46    |2   |int                      |Time zone                    |
|48    |1   |int                      |Auto profile location        |
|49    |1   |int                      |Touch screen cal. orientation|
|50    |2   |int                      |Touch screen cal. TLX        |
|52    |2   |int                      |Touch screen cal. TLY        |
|54    |2   |int                      |Touch screen cal. BRX        |
|56    |2   |int                      |Touch screen cal. BRY        |
|58    |2   |int                      |Touch screen cal. TRX        |
|60    |2   |int                      |Touch screen cal. TRY        |

## <a name="device">Device configuration 2</a>

|Offset|Size|Type                     |Description                  |
|------|----|-------------------------|-----------------------------|
|0     |6   |[struct](#block-header)  |[Block header](#block-header)|
|8     |17  |string                   |System password              |

#### <a name="device-flags">Device flags</a>

|Bit|Description         |
|---|--------------------|
|0  |Sound enabled       |
|1  |Date set            |
|2  |Time set            |
|3  |Auto recall profile |
|4  |DST                 |
|5  |Channel display mode|
|6  |Channel display mode|
|7  |Channel display mode|
|8  |Ethernet enabled    |
|9  |Switch off all outputs when protection tripped |
|10 |Shutdown when protection tripped               |
|11 |Force disabling of all outputs on power up     |
|12 |Click sound enabled |

## <a name="block-header">Block header</a>

|Offset|Size|Type|Description|
|------|----|----|-----------|
|0     |4   |int |Checksum   |
|4     |2   |int |Version    |

## <a name="event-queue">Event queue</a>

|Offset|Size |Type                     |Description                  |
|------|-----|-------------------------|-----------------------------|
|0     |4    |int                      |Magic number                 |
|4     |2    |int                      |Version                      |
|6     |2    |int                      |Queue head                   |
|8     |2    |int                      |Queue size                   |
|10    |2    |int                      |Last error event index       |
|12    |1600 |[struct](#event)         |Max. 200 events              |

## <a name="event">Event</a>

|Offset|Size|Type                     |Description                  |
|------|----|-------------------------|-----------------------------|
|0     |4   |datetime                 |Event date and time          |
|4     |2   |int                      |Event ID                     |

*/

namespace eez {
namespace mcu {
namespace eeprom {

// opcodes
static const uint8_t WREN = 6;
static const uint8_t WRDI = 4;
static const uint8_t RDSR = 5;
static const uint8_t WRSR = 1;
static const uint8_t READ = 3;
static const uint8_t WRITE = 2;

static const uint16_t EEPROM_ONTIME_START_ADDRESS = 64;
static const uint16_t EEPROM_EVENT_QUEUE_START_ADDRESS = 16384;

static const uint16_t EEPROM_SIZE = 32768;

void init();
bool test();

extern TestResult g_testResult;

bool read(uint8_t *buffer, uint16_t buffer_size, uint16_t address);
bool write(const uint8_t *buffer, uint16_t buffer_size, uint16_t address);

void resetAllExceptOnTimeCounters();

} // namespace eeprom
} // namespace mcu
} // namespace eez
