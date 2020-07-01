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
#include <eez/modules/dib-dcp405/adc.h>

#if defined(EEZ_PLATFORM_STM32)
#include <eez/platform/stm32/spi.h>
#include <eez/system.h>
#include <eez/index.h>
#include <scpi/scpi.h>
#endif

/// How many times per second will ADC take snapshot value?
/// Normal: 0: 20 SPS, 1: 45 SPS, 2:  90 SPS, 3: 175 SPS, 4: 330 SPS, 5:  600 SPS, 6: 1000 SPS
/// Turbo:  0: 40 SPS, 1: 90 SPS, 2: 180 SPS, 3: 350 SPS, 4: 660 SPS, 5: 1200 SPS, 6: 2000 SPS
#define CONF_ADC_SPS 5

/// Operating mode.
/// 0: Normal, 1: Duty cycle, 2: Turbo
#define CONF_ADC_MODE 2

namespace eez {
namespace psu {

#if defined(EEZ_PLATFORM_STM32)

float remapAdcDataToVoltage(Channel& channel, AdcDataType adcDataType, int16_t adcData) {
    float value = remap((float)adcData, (float)AnalogDigitalConverter::ADC_MIN, channel.params.U_MIN, (float)AnalogDigitalConverter::ADC_MAX, channel.params.U_MAX);
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
    return (CONF_ADC_SPS << 5) | (CONF_ADC_MODE << 3) | 0B00000000;
}

#endif

#if defined(EEZ_PLATFORM_SIMULATOR)
static float g_uMon[CH_MAX];
static float g_iMon[CH_MAX];

float g_uSet[CH_MAX];
float g_iSet[CH_MAX];

void updateValues(uint8_t channelIndex) {
    bool series = false;
    bool parallel = false;
    Channel &channel0 = Channel::get(0);
    Channel &channel1 = Channel::get(1);
    if (channelIndex == 0 || channelIndex == 1) {
        if (channel_dispatcher::getCouplingType() == channel_dispatcher::COUPLING_TYPE_SERIES) {
            series = true;
        } else if (channel_dispatcher::getCouplingType() == channel_dispatcher::COUPLING_TYPE_PARALLEL) {
            parallel = true;
        }
    }

    if (series || parallel) {
        if (channelIndex == 1) {
            return;
        }
    }

    auto &channel = Channel::get(channelIndex);

    if (channel.simulator.getLoadEnabled()) {
        float u_set_v;
        if (series) {
            u_set_v = channel.isRemoteProgrammingEnabled()
                ? remap(channel0.simulator.voltProgExt + channel1.simulator.voltProgExt, 0, 0, 2.5, channel.u.max)
                : g_uSet[0] + g_uSet[1];
        } else {
            u_set_v = channel.isRemoteProgrammingEnabled()
                ? remap(channel.simulator.voltProgExt, 0, 0, 2.5, channel.u.max)
                : g_uSet[channelIndex];
        }

        float i_set_a;
        if (parallel) {
            i_set_a = g_iSet[0] + g_iSet[1];
        } else {
            i_set_a = g_iSet[channelIndex];
        }

        float u_mon_v = i_set_a * channel.simulator.load;
        float i_mon_a = i_set_a;
        if (u_mon_v > u_set_v) {
            u_mon_v = u_set_v;
            i_mon_a = u_set_v / channel.simulator.load;

            simulator::setCV(channelIndex, true);
            simulator::setCC(channelIndex, false);
        } else {
            simulator::setCV(channelIndex, false);
            simulator::setCC(channelIndex, true);
        }

        if (series) {
            g_uMon[0] = u_mon_v / 2;
            g_uMon[1] = u_mon_v / 2;
        } else {
            g_uMon[channelIndex] = u_mon_v;
        }

        if (parallel) {
            g_iMon[0] = i_mon_a / 2;
            g_iMon[1] = i_mon_a / 2;
        } else {
            g_iMon[channelIndex] = i_mon_a;
        }

        return;
    } else {
        if (channel.isOutputEnabled()) {
            if (series) {
                g_uMon[0] = g_uSet[0];
                g_uMon[1] = g_uSet[1];
            } else {
                g_uMon[channelIndex] = g_uSet[channelIndex];
            }

            if (series) {
                g_iMon[0] = 0;
                g_iMon[1] = 0;
            } else {
                g_iMon[channelIndex] = 0;
            }

            if (g_uSet[channelIndex] > 0 && g_iSet[channelIndex] > 0) {
                simulator::setCV(channelIndex, true);
                simulator::setCC(channelIndex, false);
            } else {
                simulator::setCV(channelIndex, false);
                simulator::setCC(channelIndex, true);
            }
            return;
        }
    }

    g_uMon[channelIndex] = 0;
    g_iMon[channelIndex] = 0;
    simulator::setCV(channelIndex, true);
    simulator::setCC(channelIndex, false);
}
#endif

void AnalogDigitalConverter::init() {
#if defined(EEZ_PLATFORM_STM32)
    uint8_t data[4];
    uint8_t result[4];

    spi::select(slotIndex, spi::CHIP_ADC);

    // Send RESET command
    data[0] = ADC_RESET;
    spi::transfer1(slotIndex, data, result);

    delay(1);

    data[0] = ADC_WR3S1;
    data[1] = getReg1Val();
    data[2] = ADC_REG2_VAL;
    data[3] = ADC_REG3_VAL;
    spi::transfer4(slotIndex, data, result);

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
    spi::transfer4(slotIndex, data, result);
    spi::deselect(slotIndex);

    uint8_t reg1 = result[1];
    uint8_t reg2 = result[2];
    uint8_t reg3 = result[3];

	if (reg1 != getReg1Val()) {
		g_testResult = TEST_FAILED;
	}

	if (reg2 != ADC_REG2_VAL) {
		g_testResult = TEST_FAILED;
	}

	if (reg3 != ADC_REG3_VAL) {
	   g_testResult = TEST_FAILED;
	}

    if (g_testResult == TEST_FAILED) {
		generateError(SCPI_ERROR_CH1_ADC_TEST_FAILED + channelIndex);
    } else {
    	g_testResult = TEST_OK;
    }
#endif

    return g_testResult != TEST_FAILED;
}

void AnalogDigitalConverter::start(AdcDataType adcDataType_) {
    adcDataType = adcDataType_;

#if defined(EEZ_PLATFORM_STM32)
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
	spi::transfer3(slotIndex, data, result);
	spi::deselect(slotIndex);
#endif
}

float AnalogDigitalConverter::read() {
#if defined(EEZ_PLATFORM_STM32)
    uint8_t data[3];
    uint8_t result[3];

    data[0] = ADC_RDATA;
    data[1] = 0;
    data[2] = 0;

    spi::select(slotIndex, spi::CHIP_ADC);
    spi::transfer3(slotIndex, data, result);
    spi::deselect(slotIndex);

    uint16_t dmsb = result[1];
    uint16_t dlsb = result[2];

    int16_t adcValue = (int16_t)((dmsb << 8) | dlsb);

    float value;

    auto &channel = Channel::get(channelIndex);

    if (adcDataType == ADC_DATA_TYPE_U_MON) {
        value = remapAdcDataToVoltage(channel, adcDataType, adcValue);
        channel.u.mon_adc = value;
#ifdef DEBUG
        debug::g_uMon[channelIndex].set(adcValue);
#endif
    } else if (adcDataType == ADC_DATA_TYPE_I_MON) {
        value = remapAdcDataToCurrent(channel, adcDataType, adcValue);
        channel.i.mon_adc = value;
#ifdef DEBUG
        debug::g_iMon[channelIndex].set(adcValue);
#endif
    } else if (adcDataType == ADC_DATA_TYPE_U_MON_DAC) {
        value = remapAdcDataToVoltage(channel, adcDataType, adcValue);
#ifdef DEBUG
        debug::g_uMonDac[channelIndex].set(adcValue);
#endif
    } else {
        value = remapAdcDataToCurrent(channel, adcDataType, adcValue);
#ifdef DEBUG
        debug::g_iMonDac[channelIndex].set(adcValue);
#endif
    }

    return value;
#endif

#if defined(EEZ_PLATFORM_SIMULATOR)
    updateValues(channelIndex);

    if (adcDataType == ADC_DATA_TYPE_U_MON) {
        return g_uMon[channelIndex];
    }

    if (adcDataType == ADC_DATA_TYPE_I_MON) {
        return g_iMon[channelIndex];
    }

    if (adcDataType == ADC_DATA_TYPE_U_MON_DAC) {
        return g_uSet[channelIndex];
    }

    return g_iSet[channelIndex];
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
    spi::transfer5(slotIndex, data, result);
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
