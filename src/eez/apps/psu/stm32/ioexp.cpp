/*
 * EEZ PSU Firmware
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

#include <scpi/scpi.h>

#include <main.h>
#include <eez/platform/stm32/spi.h>

#include <eez/system.h>
#include <eez/index.h>

#include <eez/apps/psu/psu.h>
#include <eez/apps/psu/ioexp.h>

namespace eez {
namespace psu {

// I/O expander MCP23S17-E/SS
// http://ww1.microchip.com/downloads/en/devicedoc/20001952c.pdf
static const uint8_t IOEXP_WRITE = 0B01000000;
static const uint8_t IOEXP_READ = 0B01000001;

static const uint8_t REG_IODIRA = 0x00;
static const uint8_t REG_IODIRB = 0x01;
static const uint8_t REG_IPOLA = 0x02;
static const uint8_t REG_IPOLB = 0x03;
static const uint8_t REG_GPINTENA = 0x04;
static const uint8_t REG_GPINTENB = 0x05;
static const uint8_t REG_DEFVALA = 0x06;
static const uint8_t REG_DEFVALB = 0x07;
static const uint8_t REG_INTCONA = 0x08;
static const uint8_t REG_INTCONB = 0x09;
static const uint8_t REG_IOCON = 0x0A;
static const uint8_t REG_GPPUA = 0x0C;
static const uint8_t REG_GPPUB = 0x0D;
static const uint8_t REG_GPIOA = 0x12;
static const uint8_t REG_GPIOB = 0x13;

static const uint8_t DCP505_REG_VALUE_IODIRA   = 0B00011111; // pins 0, 1, 2, 3, 4, 12, 13 and 14 are inputs (set to 1)
static const uint8_t DCP505_REG_VALUE_IODIRB   = 0B11110000; //

static const uint8_t DCP405_REG_VALUE_IODIRA   = 0B00011111; // pins 0, 1, 2, 3 and 4 are inputs (set to 1)
static const uint8_t DCP405_REG_VALUE_IODIRB   = 0B00000000; //

static const uint8_t REG_VALUE_IPOLA    = 0B00000000; // no pin is inverted
static const uint8_t REG_VALUE_IPOLB    = 0B00000000; // no pin is inverted
static const uint8_t REG_VALUE_GPINTENA = 0B00000000; // no interrupts
static const uint8_t REG_VALUE_GPINTENB = 0B00000000; // no interrupts
static const uint8_t REG_VALUE_DEFVALA  = 0B00000000; //
static const uint8_t REG_VALUE_DEFVALB  = 0B00000000; //
static const uint8_t REG_VALUE_INTCONA  = 0B00000000; //
static const uint8_t REG_VALUE_INTCONB  = 0B00000000; //
static const uint8_t REG_VALUE_IOCON    = 0B00100000; // sequential operation disabled, hw addressing disabled
static const uint8_t REG_VALUE_GPPUA    = 0B00000000; // pull up with 100K resistor pins 1 (CC) and 2 (CV)
static const uint8_t REG_VALUE_GPPUB    = 0B00000000; //

static const uint8_t DCP505_REG_VALUE_GPIOA = 0B00100000; // disable OVP
static const uint8_t DCP505_REG_VALUE_GPIOB = 0B00000001; // DP is OFF

static const uint8_t DCP405_REG_VALUE_GPIOA = 0B00000000; //
static const uint8_t DCP405_REG_VALUE_GPIOB = 0B00100001; // DP is OFF, disable OVP

static const uint8_t REG_VALUES[] = {
    REG_IODIRA,   DCP505_REG_VALUE_IODIRA, 1,
    REG_IODIRB,   DCP505_REG_VALUE_IODIRB, 1,
    REG_IPOLA,    REG_VALUE_IPOLA,    1,
    REG_IPOLB,    REG_VALUE_IPOLB,    1,
    REG_GPINTENA, REG_VALUE_GPINTENA, 1,
    REG_GPINTENB, REG_VALUE_GPINTENB, 1,
    REG_DEFVALA,  REG_VALUE_DEFVALA,  1,
    REG_DEFVALB,  REG_VALUE_DEFVALB,  1,
    REG_INTCONA,  REG_VALUE_INTCONA,  1,
    REG_INTCONB,  REG_VALUE_INTCONB,  1,
    REG_IOCON,    REG_VALUE_IOCON,    1,
    REG_GPPUA,    REG_VALUE_GPPUA,    1,
    REG_GPPUB,    REG_VALUE_GPPUB,    1,
    REG_GPIOA,    DCP505_REG_VALUE_GPIOA, 0,
    REG_GPIOB,    DCP505_REG_VALUE_GPIOB, 0,
};

////////////////////////////////////////////////////////////////////////////////

IOExpander::IOExpander(Channel &channel_) : channel(channel_) {
    g_testResult = TEST_SKIPPED;
}

void IOExpander::init() {
    const uint8_t N_REGS = sizeof(REG_VALUES) / 3;
    for (int i = 0; i < N_REGS; i++) {
    	uint8_t reg = REG_VALUES[3 * i];
    	uint8_t value = REG_VALUES[3 * i + 1];

    	if (g_slots[channel.slotIndex].moduleType == MODULE_TYPE_DCP405) {
        	if (reg == REG_IODIRA) {
        		value = DCP405_REG_VALUE_IODIRA;
        	} else if (reg == REG_IODIRB) {
        		value = DCP405_REG_VALUE_IODIRB;
        	} else if (reg == REG_GPIOA) {
        		value = DCP405_REG_VALUE_GPIOA;
        	} else if (reg == REG_GPIOB) {
        		value = DCP405_REG_VALUE_GPIOB;
        	}
    	}

    	write(reg, value);
    }

    if (g_slots[channel.slotIndex].moduleType == MODULE_TYPE_DCP405) {
    	gpioa = DCP405_REG_VALUE_GPIOA;
    	gpiob = DCP405_REG_VALUE_GPIOB;
    } else {
    	gpioa = DCP505_REG_VALUE_GPIOA;
    	gpiob = DCP505_REG_VALUE_GPIOB;
    }
}

bool IOExpander::test() {
    g_testResult = TEST_OK;

    channel.flags.powerOk = 1;

    const uint8_t N_REGS = sizeof(REG_VALUES) / 3;
    for (int i = 0; i < N_REGS; i++) {
		if (REG_VALUES[3 * i + 2]) {
			uint8_t reg = REG_VALUES[3 * i];
			uint8_t value = read(reg);

			uint8_t expectedValue = REG_VALUES[3 * i + 1];

			if (g_slots[channel.slotIndex].moduleType == MODULE_TYPE_DCP405) {
				if (reg == REG_IODIRA) {
					expectedValue = DCP405_REG_VALUE_IODIRA;
				} else if (reg == REG_IODIRB) {
					expectedValue = DCP405_REG_VALUE_IODIRB;
				}
			}

			if (value != expectedValue) {
				DebugTrace("Ch%d IO expander reg check failure: reg=%d, expected=%d, got=%d",
						channel.index, (int)REG_VALUES[3 * i], (int)expectedValue, (int)value);

				g_testResult = TEST_FAILED;
				break;
			}
		}
     }

     if (g_testResult == TEST_OK) {
 #if !CONF_SKIP_PWRGOOD_TEST
         channel.flags.powerOk = testBit(IO_BIT_IN_PWRGOOD);
         if (!channel.flags.powerOk) {
             DebugTrace("Ch%d power fault", channel.index);
             generateError(SCPI_ERROR_CH1_FAULT_DETECTED - (channel.index - 1));
         }
 #else
         channel.flags.powerOk = 1;
 #endif
     } else {
         channel.flags.powerOk = 0;
     }

     if (g_testResult == TEST_FAILED) {
         if (channel.index == 1) {
             generateError(SCPI_ERROR_CH1_IOEXP_TEST_FAILED);
         }
         else if (channel.index == 2) {
             generateError(SCPI_ERROR_CH2_IOEXP_TEST_FAILED);
         }
         else {
             // TODO
         }
     }

    return g_testResult != TEST_FAILED;
}

void IOExpander::tick(uint32_t tick_usec) {
    if (isPowerUp()) {
        channel.eventGpio(readGpio());
    }
}

uint8_t IOExpander::readGpio() {
    return read(REG_GPIOA);
}

uint8_t IOExpander::readGpioB() {
	return read(REG_GPIOB);
}

bool IOExpander::testBit(int io_bit) {
    uint8_t value = readGpio();
    return value & (1 << io_bit) ? true : false;
}

void IOExpander::changeBit(int io_bit, bool set) {
    if (io_bit == IO_BIT_OUT_OUTPUT_ENABLE) {
        HAL_GPIO_WritePin(OE_SYNC_GPIO_Port, OE_SYNC_Pin, GPIO_PIN_RESET);
    }

    if (io_bit < 8) {
        uint8_t newValue = set ? (gpioa | (1 << io_bit)) : (gpioa & ~(1 << io_bit));
	    if (gpioa != newValue) {
		    gpioa = newValue;
			write(REG_GPIOA, gpioa);
	    }
    } else {
        uint8_t newValue = set ? (gpiob | (1 << (io_bit - 8))) : (gpiob & ~(1 << (io_bit - 8)));
	    if (gpiob != newValue) {
		    gpiob = newValue;
            write(REG_GPIOB, gpiob);
	    }
    }

    if (io_bit == IO_BIT_OUT_OUTPUT_ENABLE) {
        HAL_GPIO_WritePin(OE_SYNC_GPIO_Port, OE_SYNC_Pin, GPIO_PIN_SET);
    }
}

uint8_t IOExpander::read(uint8_t reg) {
    uint8_t data[3];
    data[0] = IOEXP_READ;
    data[1] = reg;
    data[2] = 0;
    uint8_t result[3];

    spi::select(channel.slotIndex, spi::CHIP_IOEXP);
    spi::transfer(channel.slotIndex, data, result, 3);
    spi::deselect(channel.slotIndex);

    return result[2];
}

void IOExpander::write(uint8_t reg, uint8_t val) {
    uint8_t data[3];
    data[0] = IOEXP_WRITE;
    data[1] = reg;
    data[2] = val;
    uint8_t result[3];

    spi::select(channel.slotIndex, spi::CHIP_IOEXP);
    spi::transfer(channel.slotIndex, data, result, 3);
    spi::deselect(channel.slotIndex);
}

}
} // namespace eez::psu
