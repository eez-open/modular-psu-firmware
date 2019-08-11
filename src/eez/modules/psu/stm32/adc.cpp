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

#include <eez/platform/stm32/spi.h>

#include <eez/system.h>

#include <eez/apps/psu/psu.h>

#include <eez/apps/psu/channel_dispatcher.h>
#include <eez/modules/psu/adc.h>
#include <eez/system.h>

#include <eez/index.h>

#undef ADC_SPS
/// How many times per second will ADC take snapshot value?
/// 0: 20 SPS, 1: 45 SPS, 2: 90 SPS, 3: 175 SPS, 4: 330 SPS, 5: 600 SPS, 6: 1000 SPS
#define ADC_SPS 5

namespace eez {
namespace psu {

////////////////////////////////////////////////////////////////////////////////

// Register 02h: External Vref, 50Hz rejection, PSW off, IDAC off
static const uint8_t ADC_REG2_VAL = 0B01100000;

// Register 03h: IDAC1 disabled, IDAC2 disabled, dedicated DRDY
static const uint8_t ADC_REG3_VAL = 0B00000000;

////////////////////////////////////////////////////////////////////////////////

AnalogDigitalConverter::AnalogDigitalConverter(Channel &channel_) : channel(channel_) {
    g_testResult = TEST_SKIPPED;
}

uint8_t AnalogDigitalConverter::getReg1Val() {
    return (ADC_SPS << 5) | 0B00000000;
}

void AnalogDigitalConverter::init() {
    uint8_t data[4];
    uint8_t result[4];

    spi::select(channel.slotIndex, spi::CHIP_ADC);

    // Send RESET command
    data[0] = ADC_RESET;
    spi::transfer(channel.slotIndex, data, result, 1);

    delay(1);

    data[0] = ADC_WR3S1;
    data[1] = getReg1Val();
    data[2] = ADC_REG2_VAL;
    data[3] = ADC_REG3_VAL;
    spi::transfer(channel.slotIndex, data, result, 4);

    spi::deselect(channel.slotIndex);
}

bool AnalogDigitalConverter::test() {
    uint8_t data[4];
    uint8_t result[4];

    data[0] = ADC_RD3S1;
    data[1] = 0;
    data[2] = 0;
    data[3] = 0;

    spi::select(channel.slotIndex, spi::CHIP_ADC);
    spi::transfer(channel.slotIndex, data, result, 4);
    spi::deselect(channel.slotIndex);

    uint8_t reg1 = result[1];
    uint8_t reg2 = result[2];
    uint8_t reg3 = result[3];

    g_testResult = TEST_OK;

	if (reg1 != getReg1Val()) {
		DebugTrace("Ch%d ADC test failed reg1: expected=%d, got=%d", channel.index,
				getReg1Val(), reg1);
		g_testResult = TEST_FAILED;
	}

	if (reg2 != ADC_REG2_VAL) {
		DebugTrace("Ch%d ADC test failed reg2: expected=%d, got=%d", channel.index,
				ADC_REG2_VAL, reg2);
		g_testResult = TEST_FAILED;
	}

	if (reg3 != ADC_REG3_VAL) {
	   DebugTrace("Ch%d ADC test failed reg3: expected=%d, got=%d", channel.index,
	   ADC_REG3_VAL, reg3);
	   g_testResult = TEST_FAILED;
	}

    if (g_testResult == TEST_FAILED) {
        if (channel.index == 1) {
            generateError(SCPI_ERROR_CH1_ADC_TEST_FAILED);
        }
        else if (channel.index == 2) {
            generateError(SCPI_ERROR_CH2_ADC_TEST_FAILED);
        }
        else {
            // TODO
        }
    }

    return g_testResult != TEST_FAILED;
}

void AnalogDigitalConverter::tick(uint32_t tick_usec) {
    if (start_reg0) {
    	// int ready = !HAL_GPIO_ReadPin(SPI2_IRQ_GPIO_Port, SPI2_IRQ_Pin)
    	int ready;
    	if (g_slots[channel.slotIndex].moduleType == MODULE_TYPE_DCP405) {
            // DCP405_IO_BIT_IN_ADC_DRDY
        	uint8_t gpioa = channel.ioexp.readGpio();
    		ready = !(gpioa & 0b00010000);
    	} else {
        	uint8_t gpiob = channel.ioexp.readGpioB();
    		ready = !(gpiob & 0b10000000);
    	}
    	if (ready) {
			int16_t adc_data = read();
			channel.eventAdcData(adc_data);

	#ifdef DEBUG
			debug::g_adcCounter.inc();
	#endif
		}
    }
}

void AnalogDigitalConverter::start(uint8_t reg0) {
    start_reg0 = reg0;

    if (start_reg0) {
        uint8_t data[3];
        uint8_t result[3];

        data[0] = ADC_WR1S0;
        data[1] = start_reg0;
        data[2] = ADC_START;

        spi::select(channel.slotIndex, spi::CHIP_ADC);
        spi::transfer(channel.slotIndex, data, result, 3);
        spi::deselect(channel.slotIndex);
    }
}

int16_t AnalogDigitalConverter::read() {
    uint8_t data[3];
    uint8_t result[3];

    data[0] = ADC_RDATA;
    data[1] = 0;
    data[2] = 0;

    spi::select(channel.slotIndex, spi::CHIP_ADC);
    spi::transfer(channel.slotIndex, data, result, 3);
    spi::deselect(channel.slotIndex);

    uint16_t dmsb = result[1];
    uint16_t dlsb = result[2];

    return (int16_t)((dmsb << 8) | dlsb);
}

void AnalogDigitalConverter::readAllRegisters(uint8_t registers[]) {
    uint8_t data[5];
    uint8_t result[5];

    data[0] = ADC_RD4S0;
    data[1] = 0;
    data[2] = 0;
    data[3] = 0;
    data[4] = 0;

    spi::select(channel.slotIndex, spi::CHIP_ADC);
    spi::transfer(channel.slotIndex, data, result, 5);
    spi::deselect(channel.slotIndex);

    registers[0] = result[1];
    registers[1] = result[2];
    registers[2] = result[3];
    registers[3] = result[4];

    if (start_reg0) {
    	start(start_reg0);
    }
}

} // namespace psu
} // namespace eez
