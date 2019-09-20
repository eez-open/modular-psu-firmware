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
#include <main.h>
#include <crc.h>
#include <eez/platform/stm32/spi.h>
#include <memory.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#endif

#include <eez/modules/dcpx05/channel.h>

#include <eez/apps/psu/psu.h>
#include <eez/apps/psu/channel.h>
#include <eez/scpi/regs.h>
#include <eez/system.h>

namespace eez {

using namespace psu;

namespace dcm220 {

#define DAC_MIN 0
#define DAC_MAX 4095

#if defined(EEZ_PLATFORM_STM32)

#define REG0_OE1_MASK     (1 << 0)
#define REG0_CC1_MASK     (1 << 1)
#define REG0_OE2_MASK     (1 << 2)
#define REG0_CC2_MASK     (1 << 3)
#define REG0_PWRGOOD_MASK (1 << 4)

#define ADC_MIN 0
#define ADC_MAX 65535

#define BUFFER_SIZE 20

#define SPI_SLAVE_SYNBYTE         0x53
#define SPI_MASTER_SYNBYTE        0xAC

static GPIO_TypeDef *SPI_IRQ_GPIO_Port[] = { SPI2_IRQ_GPIO_Port, SPI4_IRQ_GPIO_Port, SPI5_IRQ_GPIO_Port };
static const uint16_t SPI_IRQ_Pin[] = { SPI2_IRQ_Pin, SPI4_IRQ_Pin, SPI5_IRQ_Pin };

void masterSynchro(int slotIndex) {
	uint8_t txackbytes = SPI_MASTER_SYNBYTE, rxackbytes = 0x00;
	do {
	    spi::select(slotIndex, spi::CHIP_DCM220);
	    spi::transfer(slotIndex, &txackbytes, &rxackbytes, 1);
	    spi::deselect(slotIndex);
	} while(rxackbytes != SPI_SLAVE_SYNBYTE);

	while (HAL_GPIO_ReadPin(SPI_IRQ_GPIO_Port[slotIndex], SPI_IRQ_Pin[slotIndex]) != GPIO_PIN_SET);
}

float calcTemperature(uint16_t adcValue) {
	if (adcValue == 65535) {
		// not measured yet
		return 25;
	}

	float RF = 3300.0f;
	float UREF = 3.3f;
	float T25 = 298.15F;
	float R25 = 10000;
	float BETA = 3570;
	float ADC_MAX_FOR_TEMP = 4095.0f;

	float UT = UREF - adcValue * UREF / ADC_MAX_FOR_TEMP;
	float RT = RF * UT / (UREF - UT);

	float Tkelvin = 1 / (logf(RT / R25) / BETA + 1 / T25);

	float Tcelsius = Tkelvin - 273.15f;

	return roundPrec(Tcelsius, 0.1f);
}

#endif

struct Channel : ChannelInterface {
#if defined(EEZ_PLATFORM_STM32)
	bool synchronized;
	uint32_t lastTickCount;

	uint8_t output[BUFFER_SIZE];
	uint8_t input[BUFFER_SIZE];

	bool outputEnable[2];
	uint16_t uSet[2];
	uint16_t iSet[2];

	float temperature[2];
#endif

#if defined(EEZ_PLATFORM_SIMULATOR)
    float uMon[2];
    float iMon[2];
    float uSet[2];
    float iSet[2];
#endif

    TestResult testResult;

    Channel(int slotIndex_)
		: ChannelInterface(slotIndex_)
#if defined(EEZ_PLATFORM_STM32)
		, synchronized(false)
#endif
    	, testResult(TEST_NONE)
	{
#if defined(EEZ_PLATFORM_STM32)
    	memset(output, 0, sizeof(output));
    	memset(input, 0, sizeof(input));
#endif
    }

#if defined(EEZ_PLATFORM_STM32)
    bool isCrcOk;

	void transfer() {
		spi::select(slotIndex, spi::CHIP_DCM220);
		spi::transfer(slotIndex, output, input, BUFFER_SIZE);
		spi::deselect(slotIndex);

		lastTickCount = micros();

		uint32_t crc = HAL_CRC_Calculate(&hcrc, (uint32_t *)input, BUFFER_SIZE - 4);
		if (crc == *((uint32_t *)(input + BUFFER_SIZE - 4))) {
			isCrcOk = true;
		} else {
			isCrcOk = false;
			DebugTrace("CRC NOT OK %u\n", crc);

		}
	}
#endif

	void init(int subchannelIndex) {
#if defined(EEZ_PLATFORM_STM32)
		if (!synchronized) {
			spi::deselectA(slotIndex);
			spi::init(slotIndex, spi::CHIP_DCM220);
			masterSynchro(slotIndex);
			synchronized = true;
			delay(1);
		}
#endif
	}

	void reset(int subchannelIndex) {
	}

	void test(int subchannelIndex) {
		psu::Channel &channel = psu::Channel::getBySlotIndex(slotIndex, subchannelIndex);

#if defined(EEZ_PLATFORM_STM32)
		if (subchannelIndex == 0) {
			output[0] = 0;

			transfer();
		}

		bool pwrGood = input[0] & REG0_PWRGOOD_MASK ? true : false;
		pwrGood = true;
#endif

#if defined(EEZ_PLATFORM_SIMULATOR)
        bool pwrGood = simulator::getPwrgood(channel.channelIndex);
#endif

		channel.flags.powerOk = pwrGood ? 1 : 0;

		testResult = channel.flags.powerOk ? TEST_OK : TEST_FAILED;
	}

	TestResult getTestResult(int subchannelIndex) {
		return testResult;
	}

	void tick(int subchannelIndex, uint32_t tickCount) {
        psu::Channel &channel = psu::Channel::getBySlotIndex(slotIndex, subchannelIndex);

#if defined(EEZ_PLATFORM_STM32)
        int32_t diff = tickCount - lastTickCount;
        if (subchannelIndex == 0 && diff > 1000) {
        	uint8_t output0 = 0x80 | (outputEnable[0] ? REG0_OE1_MASK : 0) | (outputEnable[1] ? REG0_OE2_MASK : 0);

        	bool oeSync = output[0] != output0;
        	if (oeSync) {
            	HAL_GPIO_WritePin(OE_SYNC_GPIO_Port, OE_SYNC_Pin, GPIO_PIN_RESET);
        	}

        	output[0] = output0;
        	output[1] = 0;

        	uint16_t *outputSetValues = (uint16_t *)(output + 2);

        	outputSetValues[0] = uSet[0];
        	outputSetValues[1] = iSet[0];
        	outputSetValues[2] = uSet[1];
        	outputSetValues[3] = iSet[1];

			transfer();

		    if (oeSync) {
		        HAL_GPIO_WritePin(OE_SYNC_GPIO_Port, OE_SYNC_Pin, GPIO_PIN_SET);
		    }

			temperature[0] = calcTemperature(*((uint16_t *)(input + 10)));
			temperature[1] = calcTemperature(*((uint16_t *)(input + 12)));
        }
#endif

#if defined(EEZ_PLATFORM_SIMULATOR)
        if (channel.isOutputEnabled()) {
            if (channel.simulator.getLoadEnabled()) {
                float u_set_v = uSet[channel.channelIndex];
                float i_set_a = iSet[channel.channelIndex];

                float u_mon_v = i_set_a * channel.simulator.load;
                float i_mon_a = i_set_a;
                if (u_mon_v > u_set_v) {
                    u_mon_v = u_set_v;
                    i_mon_a = u_set_v / channel.simulator.load;

                    simulator::setCC(channel.channelIndex, false);
                } else {
                    simulator::setCC(channel.channelIndex, true);
                }

                uMon[channel.channelIndex] = u_mon_v;
                iMon[channel.channelIndex] = i_mon_a;
            } else {
                uMon[channel.channelIndex] = uSet[channel.channelIndex];
                iMon[channel.channelIndex] = 0;
                if (uSet[channel.channelIndex] > 0 && iSet[channel.channelIndex] > 0) {
                    simulator::setCC(channel.channelIndex, false);
                } else {
                    simulator::setCC(channel.channelIndex, true);
                }
            }
        } else {
            uMon[channel.channelIndex] = 0;
            iMon[channel.channelIndex] = 0;
            simulator::setCC(channel.channelIndex, false);
        }
#endif

        //

#if defined(EEZ_PLATFORM_STM32)
    	uint16_t *inputSetValues = (uint16_t *)(input + 2);

    	int offset = subchannelIndex * 2;

        uint16_t uMonAdc = inputSetValues[offset];
        float uMon = remap(uMonAdc, (float)ADC_MIN, channel.params->U_MIN, (float)ADC_MAX, channel.params->U_MAX);
        channel.onAdcData(ADC_DATA_TYPE_U_MON, uMon);

        uint16_t iMonAdc = inputSetValues[offset + 1];
        float iMon = remap(iMonAdc, (float)ADC_MIN, channel.params->I_MIN, (float)ADC_MAX, channel.params->I_MAX);
        channel.onAdcData(ADC_DATA_TYPE_I_MON, iMon);
#endif

#if defined(EEZ_PLATFORM_SIMULATOR)
        channel.onAdcData(ADC_DATA_TYPE_U_MON, uMon[channel.channelIndex]);
        channel.onAdcData(ADC_DATA_TYPE_I_MON, iMon[channel.channelIndex]);
#endif

        // PWRGOOD

#if defined(EEZ_PLATFORM_STM32)
		bool pwrGood = input[0] & REG0_PWRGOOD_MASK ? true : false;
		pwrGood = true;
#endif

#if defined(EEZ_PLATFORM_SIMULATOR)
        bool pwrGood = simulator::getPwrgood(channel.channelIndex);
#endif

#if !CONF_SKIP_PWRGOOD_TEST
		if (!pwrGood) {
			channel.flags.powerOk = 0;
			generateError(SCPI_ERROR_CH1_FAULT_DETECTED - channel.channelIndex);
			powerDownBySensor();
			return;
		}
#endif

//		diff = tickCount - dumpTempTickCount;
//		if (subchannelIndex == 0 && diff > 2000000) {
//			uint16_t temp0 = *((uint16_t *)(input + 10));
//			uint16_t temp1 = *((uint16_t *)(input + 12));
//			uint16_t temp2 = *((uint16_t *)(input + 14));
//
//			DebugTrace("CH1=%u, CH2=%u, MCU=%u, %g, %g\n", (unsigned)temp0, (unsigned)temp1, (unsigned)temp2, temperature[0], temperature[1]);
//			dumpTempTickCount = tickCount;
//		}
	}

//	uint32_t dumpTempTickCount;

	bool isCcMode(int subchannelIndex) {
#if defined(EEZ_PLATFORM_STM32)
        return (input[0] & (subchannelIndex == 0 ? REG0_CC1_MASK : REG0_CC2_MASK)) != 0;
#endif

#if defined(EEZ_PLATFORM_SIMULATOR)
        psu::Channel &channel = psu::Channel::getBySlotIndex(slotIndex, subchannelIndex);
        return simulator::getCC(channel.channelIndex);
#endif
	}

	bool isCvMode(int subchannelIndex) {
		return !isCcMode(subchannelIndex);
	}

	void adcMeasureUMon(int subchannelIndex) {
	}

	void adcMeasureIMon(int subchannelIndex) {
	}

	void adcMeasureMonDac(int subchannelIndex) {
	}

	void adcMeasureAll(int subchannelIndex) {
	}

	void setOutputEnable(int subchannelIndex, bool enable) {
#if defined(EEZ_PLATFORM_STM32)
		outputEnable[subchannelIndex] = enable;
#endif
	}

	void setDacVoltage(int subchannelIndex, uint16_t value) {

#if defined(EEZ_PLATFORM_STM32)
        value = (uint16_t)clamp((float)value, (float)DAC_MIN, (float)DAC_MAX);
        uSet[subchannelIndex] = clamp(value, DAC_MIN, DAC_MAX);
#endif

#if defined(EEZ_PLATFORM_SIMULATOR)
        psu::Channel &channel = psu::Channel::getBySlotIndex(slotIndex, subchannelIndex);
        uSet[subchannelIndex] = remap(clamp((float)value, (float)DAC_MIN, (float)DAC_MAX), (float)DAC_MIN, channel.params->U_MIN, (float)DAC_MAX, channel.params->U_MAX);
#endif
	}

	void setDacVoltageFloat(int subchannelIndex, float value) {
#if defined(EEZ_PLATFORM_STM32)
        psu::Channel &channel = psu::Channel::getBySlotIndex(slotIndex, subchannelIndex);
        value = remap(value, channel.params->U_MIN, (float)DAC_MIN, channel.params->U_MAX, (float)DAC_MAX);
        uSet[subchannelIndex] = (uint16_t)clamp(round(value), DAC_MIN, DAC_MAX);
#endif

#if defined(EEZ_PLATFORM_SIMULATOR)
        uSet[subchannelIndex] = value;
#endif
	}

	void setDacCurrent(int subchannelIndex, uint16_t value) {
#if defined(EEZ_PLATFORM_STM32)
        value = (uint16_t)clamp((float)value, (float)DAC_MIN, (float)DAC_MAX);
        iSet[subchannelIndex] = value;
#endif

#if defined(EEZ_PLATFORM_SIMULATOR)
        psu::Channel &channel = psu::Channel::getBySlotIndex(slotIndex, subchannelIndex);
        uSet[subchannelIndex] = remap(clamp((float)value, (float)DAC_MIN, (float)DAC_MAX), (float)DAC_MIN, channel.params->I_MIN, (float)DAC_MAX, channel.params->I_MAX);
#endif
    }

	void setDacCurrentFloat(int subchannelIndex, float value) {
#if defined(EEZ_PLATFORM_STM32)
        psu::Channel &channel = psu::Channel::getBySlotIndex(slotIndex, subchannelIndex);
        value = remap(value, channel.params->I_MIN, (float)DAC_MIN, channel.params->I_MAX, (float)DAC_MAX);
        iSet[subchannelIndex] = (uint16_t)clamp(round(value), DAC_MIN, DAC_MAX);
#endif

#if defined(EEZ_PLATFORM_SIMULATOR)
        iSet[subchannelIndex] = value;
#endif
	}

	bool isDacTesting(int subchannelIndex) {
		return false;
	}
};

static Channel g_channel0(0);
static Channel g_channel1(1);
static Channel g_channel2(2);
ChannelInterface *g_channelInterface[NUM_SLOTS] = { &g_channel0, &g_channel1, &g_channel2 };

#if defined(EEZ_PLATFORM_STM32)

float readTemperature(int channelIndex) {
	psu::Channel& channel = psu::Channel::get(channelIndex);
	int slotIndex = channel.slotIndex;
	int subchannelIndex = channel.subchannelIndex;
	Channel *dcm220Channel = (Channel *)g_channelInterface[slotIndex];
	return dcm220Channel->temperature[subchannelIndex];
}

#endif

} // namespace dcm220
} // namespace eez
