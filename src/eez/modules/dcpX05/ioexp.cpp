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

#include <eez/modules/psu/psu.h>
#include <eez/modules/psu/channel_dispatcher.h>
#include <eez/modules/dcpX05/ioexp.h>

#if defined(EEZ_PLATFORM_STM32)
#include <scpi/scpi.h>
#include <main.h>
#include <eez/platform/stm32/spi.h>
#include <eez/system.h>
#include <eez/index.h>
#include <eez/modules/psu/event_queue.h>
#endif

namespace eez {
namespace psu {

#if defined(EEZ_PLATFORM_STM32)
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
static const uint8_t REG_INTCAPA = 0x10;
static const uint8_t REG_INTCAPB = 0x11;
static const uint8_t REG_GPIOA = 0x12;
static const uint8_t REG_GPIOB = 0x13;

static const uint8_t DCP505_REG_VALUE_IODIRA   = 0B00011111; // pins 0, 1, 2, 3, 4, 12, 13 and 14 are inputs (set to 1)
static const uint8_t DCP505_REG_VALUE_IODIRB   = 0B11110000; //

static const uint8_t DCP405_REG_VALUE_IODIRA   = 0B00011111; // pins 0, 1, 2, 3 and 4 are inputs (set to 1)
static const uint8_t DCP405_R2B5_REG_VALUE_IODIRA = 0B00111111; // pins 0, 1, 2, 3, 4 and 5 are inputs (set to 1)
static const uint8_t DCP405_REG_VALUE_IODIRB   = 0B00000000; //

static const uint8_t DCP405B_REG_VALUE_IODIRA   = 0B00011111; // pins 0, 1, 2, 3 and 4 are inputs (set to 1)
static const uint8_t DCP405B_REG_VALUE_IODIRB   = 0B00000000; //

static const uint8_t REG_VALUE_IPOLA    = 0B00000000; // no pin is inverted
static const uint8_t REG_VALUE_IPOLB    = 0B00000000; // no pin is inverted

static const uint8_t REG_VALUE_GPINTENA = 0B00000000; // no interrupts
static const uint8_t DCP405_REG_VALUE_GPINTENA = 0B00100000; // enable interrupt for HW OVP Fault
static const uint8_t REG_VALUE_GPINTENB = 0B00000000; // no interrupts

static const uint8_t REG_VALUE_DEFVALA  = 0B00000000; //
static const uint8_t DCP405_REG_VALUE_DEFVALA  = 0B00100000; // default value for HW OVP Fault is 1
static const uint8_t REG_VALUE_DEFVALB  = 0B00000000; //

static const uint8_t REG_VALUE_INTCONA  = 0B00000000;
static const uint8_t DCP405_REG_VALUE_INTCONA  = 0B00100000; // compare HW OVP Fault value with default value
static const uint8_t REG_VALUE_INTCONB  = 0B00000000; //

static const uint8_t REG_VALUE_IOCON    = 0B00100000; // sequential operation disabled, hw addressing disabled
static const uint8_t REG_VALUE_GPPUA    = 0B00100001; // pull up with 100K
static const uint8_t REG_VALUE_GPPUB    = 0B00000000; //

static const uint8_t DCP505_REG_VALUE_GPIOA = 0B00100000; // disable OVP
static const uint8_t DCP505_REG_VALUE_GPIOB = 0B00000001; // DP is OFF

static const uint8_t DCP405_REG_VALUE_GPIOA = 0B00000000; //
static const uint8_t DCP405_REG_VALUE_GPIOB = 0B00000001; // DP is OFF

static const uint8_t DCP405B_REG_VALUE_GPIOA = 0B00000000; //
static const uint8_t DCP405B_REG_VALUE_GPIOB = 0B00000000; //

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

static const int REG_IODIRA_INDEX = 0;
static const int REG_IODIRB_INDEX = 1;
#endif

////////////////////////////////////////////////////////////////////////////////

#if defined(EEZ_PLATFORM_STM32)
uint8_t IOExpander::getRegValue(int i) {
    uint8_t reg = REG_VALUES[3 * i];
    uint8_t value = REG_VALUES[3 * i + 1];

    auto &slot = g_slots[slotIndex];

    if (slot.moduleInfo->moduleType == MODULE_TYPE_DCP405) {
        if (reg == REG_IODIRA) {
            if (slot.moduleRevision < MODULE_REVISION_DCP405_R2B5) {
                value = DCP405_REG_VALUE_IODIRA;
            } else {
                value = DCP405_R2B5_REG_VALUE_IODIRA;
            }
        } else if (reg == REG_IODIRB) {
            value = DCP405_REG_VALUE_IODIRB;
        } else if (reg == REG_GPIOA) {
            value = DCP405_REG_VALUE_GPIOA;
        } else if (reg == REG_GPIOB) {
            value = DCP405_REG_VALUE_GPIOB;
        } else if (reg == REG_GPINTENA) {
            value = DCP405_REG_VALUE_GPINTENA;
        } else if (reg == REG_DEFVALA) {
            value = DCP405_REG_VALUE_DEFVALA;
        } else if (reg == REG_INTCONA) {
            value = DCP405_REG_VALUE_INTCONA;
        }
    } else if (slot.moduleInfo->moduleType == MODULE_TYPE_DCP405B) {
        if (reg == REG_IODIRA) {
            value = DCP405B_REG_VALUE_IODIRA;
        } else if (reg == REG_IODIRB) {
            value = DCP405B_REG_VALUE_IODIRB;
        } else if (reg == REG_GPIOA) {
            value = DCP405B_REG_VALUE_GPIOA;
        } else if (reg == REG_GPIOB) {
            value = DCP405B_REG_VALUE_GPIOB;
        }
    }

    return value;
}
#endif

void IOExpander::init() {
#if defined(EEZ_PLATFORM_STM32)
    auto &slot = g_slots[slotIndex];

    gpioOutputPinsMask = 0;

    if (g_slots[slotIndex].moduleInfo->moduleType == MODULE_TYPE_DCP405) {
        gpioOutputPinsMask |= 1 << IO_BIT_OUT_DP_ENABLE;
        gpioOutputPinsMask |= 1 << IO_BIT_OUT_OUTPUT_ENABLE;
        gpioOutputPinsMask |= 1 << IO_BIT_OUT_REMOTE_SENSE;
        gpioOutputPinsMask |= 1 << IO_BIT_OUT_REMOTE_PROGRAMMING;

        if (slot.moduleRevision < MODULE_REVISION_DCP405_R2B5) {
            gpioOutputPinsMask |= 1 << DCP405_IO_BIT_OUT_CURRENT_RANGE_50MA;
            gpioOutputPinsMask |= 1 << DCP405_IO_BIT_OUT_CURRENT_RANGE_500MA;
        } else {
            gpioOutputPinsMask |= 1 << DCP405_R2B5_IO_BIT_OUT_CURRENT_RANGE_50MA;
        }

        gpioOutputPinsMask |= 1 << DCP405_IO_BIT_OUT_CURRENT_RANGE_5A;

        gpioOutputPinsMask |= 1 << DCP405_IO_BIT_OUT_OVP_ENABLE;
        gpioOutputPinsMask |= 1 << DCP405_IO_BIT_OUT_OE_UNCOUPLED_LED;
        gpioOutputPinsMask |= 1 << DCP405_IO_BIT_OUT_OE_COUPLED_LED;
    } else if (g_slots[slotIndex].moduleInfo->moduleType == MODULE_TYPE_DCP405B) {
        gpioOutputPinsMask |= 1 << IO_BIT_OUT_OUTPUT_ENABLE;

        gpioOutputPinsMask |= 1 << DCP405_R2B5_IO_BIT_OUT_CURRENT_RANGE_50MA;
        gpioOutputPinsMask |= 1 << DCP405_IO_BIT_OUT_CURRENT_RANGE_5A;

        gpioOutputPinsMask |= 1 << DCP405_IO_BIT_OUT_OE_UNCOUPLED_LED;
        gpioOutputPinsMask |= 1 << DCP405_IO_BIT_OUT_OE_COUPLED_LED;
    } else {
        // DCP505
        gpioOutputPinsMask |= 1 << IO_BIT_OUT_DP_ENABLE;
        gpioOutputPinsMask |= 1 << IO_BIT_OUT_OUTPUT_ENABLE;
        gpioOutputPinsMask |= 1 << IO_BIT_OUT_REMOTE_SENSE;
        gpioOutputPinsMask |= 1 << IO_BIT_OUT_REMOTE_PROGRAMMING;
        gpioOutputPinsMask |= 1 << DCP505_IO_BIT_OUT_OVP_ENABLE;
        gpioOutputPinsMask |= 1 << DCP505_IO_BIT_OUT_OE_UNCOUPLED_LED;
        gpioOutputPinsMask |= 1 << DCP505_IO_BIT_OUT_OE_COUPLED_LED;
    }

    const uint8_t N_REGS = sizeof(REG_VALUES) / 3;
    for (int i = 0; i < N_REGS; i++) {
    	uint8_t reg = REG_VALUES[3 * i];
    	uint8_t value = getRegValue(i);
    	write(reg, value);
    }

#if defined(EEZ_PLATFORM_STM32)
    HAL_GPIO_WritePin(OE_SYNC_GPIO_Port, OE_SYNC_Pin, GPIO_PIN_RESET);
    delay(1);
    HAL_GPIO_WritePin(OE_SYNC_GPIO_Port, OE_SYNC_Pin, GPIO_PIN_SET);
    delay(1);
    HAL_GPIO_WritePin(OE_SYNC_GPIO_Port, OE_SYNC_Pin, GPIO_PIN_RESET);
#endif

#endif
}

bool IOExpander::test() {
#if defined(EEZ_PLATFORM_STM32)    
    Channel &channel = Channel::getBySlotIndex(slotIndex);
    auto &slot = g_slots[slotIndex];

    channel.flags.powerOk = 1;

    const uint8_t N_REGS = sizeof(REG_VALUES) / 3;
    for (int i = 0; i < N_REGS; i++) {
        if (REG_VALUES[3 * i + 2]) {
            uint8_t reg = REG_VALUES[3 * i];
            uint8_t value = read(reg);
            uint8_t expectedValue = getRegValue(i);
            if (value != expectedValue) {
                DebugTrace("Ch%d IO expander reg check failure: reg=%d, expected=%d, got=%d", slot.channelIndex + 1, (int)REG_VALUES[3 * i], (int)expectedValue, (int)value);

                g_testResult = TEST_FAILED;
                break;
            }
        }
    }

    readGpio();

    if (g_testResult == TEST_FAILED) {
		 generateError(SCPI_ERROR_CH1_IOEXP_TEST_FAILED + slot.channelIndex);
		 channel.flags.powerOk = 0;
    } else {
    	g_testResult = TEST_OK;

#if !CONF_SKIP_PWRGOOD_TEST
        channel.flags.powerOk = testBit(IO_BIT_IN_PWRGOOD);
        if (!channel.flags.powerOk) {
            DebugTrace("Ch%d power fault", slot.channelIndex + 1);
            generateError(SCPI_ERROR_CH1_FAULT_DETECTED + slot.channelIndex);
        }
#endif
	}
#endif

#if defined(EEZ_PLATFORM_SIMULATOR)
    Channel &channel = Channel::getBySlotIndex(slotIndex);
    g_testResult = TEST_OK;
    channel.flags.powerOk = 1;
#endif

	return g_testResult != TEST_FAILED;
}

#if defined(EEZ_PLATFORM_STM32)
void IOExpander::reinit() {
    const uint8_t N_REGS = sizeof(REG_VALUES) / 3;
    for (int i = 0; i < N_REGS; i++) {
    	uint8_t reg = REG_VALUES[3 * i];
    	uint8_t value = getRegValue(i);

        if (reg == REG_GPIOA) {
            value = (uint8_t)gpioWritten;
        } else if (reg == REG_GPIOB) {
            value = (uint8_t)(gpioWritten >> 8);
        }        

    	write(reg, value);
    }
}
#endif

void IOExpander::tick(uint32_t tick_usec) {
#if defined(EEZ_PLATFORM_STM32)    
    auto &slot = g_slots[slotIndex];

	readGpio();

    uint8_t iodira = read(REG_IODIRA);
    if (iodira == 0xFF || (gpio & gpioOutputPinsMask) != gpioWritten) {
        if (iodira == 0xFF) {
            event_queue::pushEvent(event_queue::EVENT_ERROR_CH1_IOEXP_RESET_DETECTED + slot.channelIndex);
        } else {
            event_queue::pushEvent(event_queue::EVENT_ERROR_CH1_IOEXP_FAULT_MATCH_DETECTED + slot.channelIndex);
        }

        reinit();

        readGpio();
    }
#endif

#if defined(EEZ_PLATFORM_SIMULATOR)
    Channel &channel = Channel::getBySlotIndex(slotIndex);

    if (simulator::getPwrgood(channel.channelIndex)) {
        gpio |= 1 << IOExpander::IO_BIT_IN_PWRGOOD;
    } else {
        gpio &= ~(1 << IOExpander::IO_BIT_IN_PWRGOOD);
    }

    if (channel.params.features & CH_FEATURE_RPOL) {
        if (!simulator::getRPol(channel.channelIndex)) {
            gpio |= 1 << IOExpander::IO_BIT_IN_RPOL;
        } else {
            gpio &= ~(1 << IOExpander::IO_BIT_IN_RPOL);
        }
    }

    if (simulator::getCV(channel.channelIndex)) {
        gpio |= 1 << IOExpander::IO_BIT_IN_CV_ACTIVE;
    } else {
        gpio &= ~(1 << IOExpander::IO_BIT_IN_CV_ACTIVE);
    }

    if (simulator::getCC(channel.channelIndex)) {
        gpio |= 1 << IOExpander::IO_BIT_IN_CC_ACTIVE;
    } else {
        gpio &= ~(1 << IOExpander::IO_BIT_IN_CC_ACTIVE);
    }
#endif
}

#if defined(EEZ_PLATFORM_STM32)
int IOExpander::getBitDirection(int bit) {
    uint8_t dir;
    if (bit < 8) {
        dir = getRegValue(REG_IODIRA_INDEX);
    } else {
        dir = getRegValue(REG_IODIRB_INDEX);
        bit -= 8;
    }
    return dir & (1 << bit) ? 1 : 0;
}
#endif

bool IOExpander::testBit(int io_bit) {
    return (gpio & (1 << io_bit)) ? true : false;
}

bool IOExpander::isAdcReady() {
#if defined(EEZ_PLATFORM_STM32)
    // ready = !HAL_GPIO_ReadPin(SPI2_IRQ_GPIO_Port, SPI2_IRQ_Pin);
	auto &slot = g_slots[slotIndex];
    return !testBit(slot.moduleInfo->moduleType == MODULE_TYPE_DCP405 || slot.moduleInfo->moduleType == MODULE_TYPE_DCP405B ? DCP405_IO_BIT_IN_ADC_DRDY : DCP505_IO_BIT_IN_ADC_DRDY);
#else
    return true;
#endif
}

void IOExpander::changeBit(int io_bit, bool set) {
#if defined(EEZ_PLATFORM_STM32)
    if (io_bit < 8) {
        uint8_t oldValue = (uint8_t)gpioWritten;
        uint8_t newValue = set ? (oldValue | (1 << io_bit)) : (oldValue & ~(1 << io_bit));
	    if (newValue != oldValue) {
			write(REG_GPIOA, newValue);
	    }
    } else {
        uint8_t oldValue = (uint8_t)(gpioWritten >> 8);
        uint8_t newValue = set ? (oldValue | (1 << (io_bit - 8))) : (oldValue & ~(1 << (io_bit - 8)));
	    if (newValue != oldValue) {
            write(REG_GPIOB, newValue);
	    }
    }
#endif

    gpio = set ? (gpio | (1 << io_bit)) : (gpio & ~(1 << io_bit));
}

#if defined(EEZ_PLATFORM_STM32)
void IOExpander::readGpio() {
    uint8_t gpioa = read(REG_GPIOA);
    uint8_t gpiob = read(REG_GPIOB);
    gpio = (gpiob << 8) | gpioa;
}

uint8_t IOExpander::read(uint8_t reg) {
    uint8_t data[3];
    data[0] = IOEXP_READ;
    data[1] = reg;
    data[2] = 0;
    uint8_t result[3];

    spi::select(slotIndex, spi::CHIP_IOEXP);
    spi::transfer(slotIndex, data, result, 3);
    spi::deselect(slotIndex);

    return result[2];
}

void IOExpander::write(uint8_t reg, uint8_t val) {
    uint8_t data[3];
    data[0] = IOEXP_WRITE;
    data[1] = reg;
    data[2] = val;
    uint8_t result[3];

    spi::select(slotIndex, spi::CHIP_IOEXP);

    spi::transfer(slotIndex, data, result, 3);

    if (reg == REG_GPIOA) {
        gpioWritten = (gpioWritten & 0xFF00) | val;
    } else if (reg == REG_GPIOB) {
        gpioWritten = (gpioWritten & 0x00FF) | (val << 8);
    }

    spi::deselect(slotIndex);
}
#endif

#if defined(EEZ_PLATFORM_STM32)
uint8_t IOExpander::readIntcapRegister() {
    return read(REG_INTCAPA);
}
#endif

void IOExpander::readAllRegisters(uint8_t registers[]) {
#if defined(EEZ_PLATFORM_STM32)
    for (int i = 0; i < 22; i++) {
        registers[i] = read(i);
    }
#endif

#if defined(EEZ_PLATFORM_SIMULATOR)
    for (int i = 0; i < 22; i++) {
        registers[i] = 0;
    }
#endif
}

}
} // namespace eez::psu
