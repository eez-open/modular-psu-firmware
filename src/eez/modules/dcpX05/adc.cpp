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

#include <eez/apps/psu/psu.h>

#include <eez/apps/psu/channel_dispatcher.h>
#include <eez/modules/dcpX05/adc.h>

#if defined(EEZ_PLATFORM_STM32)
#include <scpi/scpi.h>
#include <eez/platform/stm32/spi.h>
#include <eez/system.h>
#include <eez/index.h>
#endif

namespace eez {
namespace psu {

#if defined(EEZ_PLATFORM_STM32)

float remapAdcDataToVoltage(Channel& channel, AdcDataType adcDataType, int16_t adcData) {
    float value = remap((float)adcData, (float)AnalogDigitalConverter::ADC_MIN, channel.params.U_MIN, (float)AnalogDigitalConverter::ADC_MAX, channel.params.U_MAX_CONF);
#if !defined(EEZ_PLATFORM_SIMULATOR)    
    if (adcDataType == ADC_DATA_TYPE_U_MON || !channel.flags.rprogEnabled) {
        value -= channel.params.VOLTAGE_GND_OFFSET;
    }
#endif
    return value;
}

float remapAdcDataToCurrent(Channel& channel, AdcDataType adcDataType, int16_t adcData) {
    float value = remap((float)adcData, (float)AnalogDigitalConverter::ADC_MIN, channel.params.I_MIN, (float)AnalogDigitalConverter::ADC_MAX, channel.getDualRangeMax());
#if !defined(EEZ_PLATFORM_SIMULATOR)    
    value -= channel.getDualRangeGndOffset();
#endif
    return value;
}

////////////////////////////////////////////////////////////////////////////////

// http://www.ti.com/lit/ds/symlink/ads1120.pdf

static const uint8_t ADC_RESET = 0B00000110;
static const uint8_t ADC_RDATA = 0B00010000;
static const uint8_t ADC_START = 0B00001000;

static const uint8_t ADC_WR3S1 = 0B01000110;
static const uint8_t ADC_RD3S1 = 0B00100110;
static const uint8_t ADC_WR1S0 = 0B01000000;
static const uint8_t ADC_WR4S0 = 0B01000011;
static const uint8_t ADC_RD4S0 = 0B00100011;

static const uint8_t ADC_REG0_READ_U_MON = 0x81; // B10000001: [7:4] AINP = AIN0, AINN = AVSS, [3:1] Gain = 1, [0] PGA disabled and bypassed
static const uint8_t ADC_REG0_READ_I_SET = 0x91; // B10010001: [7:4] AINP = AIN1, AINN = AVSS, [3:1] Gain = 1, [0] PGA disabled and bypassed
static const uint8_t ADC_REG0_READ_U_SET = 0xA1; // B10100001: [7:4] AINP = AIN2, AINN = AVSS, [3:1] Gain = 1, [0] PGA disabled and bypassed
static const uint8_t ADC_REG0_READ_I_MON = 0xB1; // B10110001: [7:4] AINP = AIN3, AINN = AVSS, [3:1] Gain = 1, [0] PGA disabled and bypassed

////////////////////////////////////////////////////////////////////////////////

// Register 02h: External Vref, 50Hz rejection, PSW off, IDAC off
static const uint8_t ADC_REG2_VAL = 0B01100000;

// Register 03h: IDAC1 disabled, IDAC2 disabled, dedicated DRDY
static const uint8_t ADC_REG3_VAL = 0B00000000;

////////////////////////////////////////////////////////////////////////////////

uint8_t AnalogDigitalConverter::getReg1Val() {
    return (ADC_SPS << 5) | 0B00000000;
}

#endif

#if defined(EEZ_PLATFORM_SIMULATOR)
static float g_uMon[CH_MAX];
static float g_iMon[CH_MAX];

float g_uSet[CH_MAX];
float g_iSet[CH_MAX];

void updateValues(Channel &channel) {
    int channelIndex = channel.channelIndex;

    if (channel.simulator.getLoadEnabled()) {
        float u_set_v = channel.isRemoteProgrammingEnabled()
                            ? remap(channel.simulator.voltProgExt, 0, 0, 2.5, channel.u.max)
                            : g_uSet[channelIndex];
        float i_set_a = g_iSet[channelIndex];

        float u_mon_v = i_set_a * channel.simulator.load;
        float i_mon_a = i_set_a;
        if (u_mon_v > u_set_v) {
            u_mon_v = u_set_v;
            i_mon_a = u_set_v / channel.simulator.load;

            simulator::setCV(channel.channelIndex, true);
            simulator::setCC(channel.channelIndex, false);
        } else {
            simulator::setCV(channel.channelIndex, false);
            simulator::setCC(channel.channelIndex, true);
        }

        g_uMon[channelIndex] = u_mon_v;
        g_iMon[channelIndex] = i_mon_a;

        return;
    } else {
        if (channel.isOutputEnabled()) {
            g_uMon[channelIndex] = g_uSet[channelIndex];
            g_iMon[channelIndex] = 0;
            if (g_uSet[channelIndex] > 0 && g_iSet[channelIndex] > 0) {
                simulator::setCV(channel.channelIndex, true);
                simulator::setCC(channel.channelIndex, false);
            } else {
                simulator::setCV(channel.channelIndex, false);
                simulator::setCC(channel.channelIndex, true);
            }
            return;
        }
    }

    g_uMon[channelIndex] = 0;
    g_iMon[channelIndex] = 0;
    simulator::setCV(channel.channelIndex, true);
    simulator::setCC(channel.channelIndex, false);
}
#endif

void AnalogDigitalConverter::init() {
#if defined(EEZ_PLATFORM_STM32)
    uint8_t data[4];
    uint8_t result[4];

    spi::select(slotIndex, spi::CHIP_ADC);

    // Send RESET command
    data[0] = ADC_RESET;
    spi::transfer(slotIndex, data, result, 1);

    delay(1);

    data[0] = ADC_WR3S1;
    data[1] = getReg1Val();
    data[2] = ADC_REG2_VAL;
    data[3] = ADC_REG3_VAL;
    spi::transfer(slotIndex, data, result, 4);

    spi::deselect(slotIndex);
#endif

#if defined(EEZ_PLATFORM_SIMULATOR)
    g_testResult = TEST_OK;
#endif
}

bool AnalogDigitalConverter::test() {
#if defined(EEZ_PLATFORM_STM32)    
    uint8_t data[4];
    uint8_t result[4];

    data[0] = ADC_RD3S1;
    data[1] = 0;
    data[2] = 0;
    data[3] = 0;

    spi::select(slotIndex, spi::CHIP_ADC);
    spi::transfer(slotIndex, data, result, 4);
    spi::deselect(slotIndex);

    uint8_t reg1 = result[1];
    uint8_t reg2 = result[2];
    uint8_t reg3 = result[3];

    Channel &channel = Channel::getBySlotIndex(slotIndex);

	if (reg1 != getReg1Val()) {
		// DebugTrace("Ch%d ADC test failed reg1: expected=%d, got=%d", channel.channelIndex + 1, getReg1Val(), reg1);
		g_testResult = TEST_FAILED;
	}

	if (reg2 != ADC_REG2_VAL) {
		// DebugTrace("Ch%d ADC test failed reg2: expected=%d, got=%d", channel.channelIndex + 1, ADC_REG2_VAL, reg2);
		g_testResult = TEST_FAILED;
	}

	if (reg3 != ADC_REG3_VAL) {
	   // DebugTrace("Ch%d ADC test failed reg3: expected=%d, got=%d", channel.channelIndex + 1, ADC_REG3_VAL, reg3);
	   g_testResult = TEST_FAILED;
	}

    if (g_testResult == TEST_FAILED) {
		generateError(SCPI_ERROR_CH1_ADC_TEST_FAILED + channel.channelIndex);
    } else {
    	g_testResult = TEST_OK;
    }
#endif

    return g_testResult != TEST_FAILED;
}

void AnalogDigitalConverter::start(AdcDataType adcDataType_) {
    adcDataType = adcDataType_;

#if defined(EEZ_PLATFORM_STM32)
    if (adcDataType) {
        uint8_t data[3];
        uint8_t result[3];

        data[0] = ADC_WR1S0;

        if (adcDataType == ADC_DATA_TYPE_U_MON) {
            data[1] = ADC_REG0_READ_U_MON;
        } else if (adcDataType == ADC_DATA_TYPE_I_MON) {
            data[1] = ADC_REG0_READ_I_MON;
        } else if (adcDataType == ADC_DATA_TYPE_U_MON_DAC) {
            data[1] = ADC_REG0_READ_U_SET;
        } else {
            data[1] = ADC_REG0_READ_I_SET;
        }

        data[2] = ADC_START;

        spi::select(slotIndex, spi::CHIP_ADC);
        spi::transfer(slotIndex, data, result, 3);
        spi::deselect(slotIndex);
    }
#endif
}

float AnalogDigitalConverter::read(Channel& channel) {
#if defined(EEZ_PLATFORM_STM32)
    uint8_t data[3];
    uint8_t result[3];

    data[0] = ADC_RDATA;
    data[1] = 0;
    data[2] = 0;

    spi::select(slotIndex, spi::CHIP_ADC);
    spi::transfer(slotIndex, data, result, 3);
    spi::deselect(slotIndex);

    uint16_t dmsb = result[1];
    uint16_t dlsb = result[2];

    int16_t adcValue = (int16_t)((dmsb << 8) | dlsb);

    float value;

    if (adcDataType == ADC_DATA_TYPE_U_MON) {
        channel.u.mon_adc = adcValue;
        value = remapAdcDataToVoltage(channel, adcDataType, adcValue);
#ifdef DEBUG
        debug::g_uMon[channel.channelIndex].set(adcValue);
#endif
    } else if (adcDataType == ADC_DATA_TYPE_I_MON) {
        channel.i.mon_adc = adcValue;
        value = remapAdcDataToCurrent(channel, adcDataType, adcValue);
#ifdef DEBUG
        debug::g_iMon[channel.channelIndex].set(adcValue);
#endif
    } else if (adcDataType == ADC_DATA_TYPE_U_MON_DAC) {
        value = remapAdcDataToVoltage(channel, adcDataType, adcValue);
#ifdef DEBUG
        debug::g_uMonDac[channel.channelIndex].set(adcValue);
#endif
    } else {
        value = remapAdcDataToCurrent(channel, adcDataType, adcValue);
#ifdef DEBUG
        debug::g_iMonDac[channel.channelIndex].set(adcValue);
#endif
    }

    return value;
#endif

#if defined(EEZ_PLATFORM_SIMULATOR)
    updateValues(channel);

    if (adcDataType == ADC_DATA_TYPE_U_MON) {
        return g_uMon[channel.channelIndex];
    }

    if (adcDataType == ADC_DATA_TYPE_I_MON) {
        return g_iMon[channel.channelIndex];
    }

    if (adcDataType == ADC_DATA_TYPE_U_MON_DAC) {
        return g_uSet[channel.channelIndex];
    }

    return g_iSet[channel.channelIndex];
#endif
}

void AnalogDigitalConverter::readAllRegisters(uint8_t registers[]) {
#if defined(EEZ_PLATFORM_STM32)    
    uint8_t data[5];
    uint8_t result[5];

    data[0] = ADC_RD4S0;
    data[1] = 0;
    data[2] = 0;
    data[3] = 0;
    data[4] = 0;

    spi::select(slotIndex, spi::CHIP_ADC);
    spi::transfer(slotIndex, data, result, 5);
    spi::deselect(slotIndex);

    registers[0] = result[1];
    registers[1] = result[2];
    registers[2] = result[3];
    registers[3] = result[4];

    if (adcDataType) {
    	start(adcDataType);
    }
#endif
}

} // namespace psu
} // namespace eez
