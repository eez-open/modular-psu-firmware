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

#include <eez/modules/dcpX05/channel.h>

#include <eez/apps/psu/psu.h>
#include <eez/scpi/regs.h>
#include <eez/system.h>

#define CONF_MASTER_SYNC_TIMEOUT_MS 500

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

bool masterSynchro(int slotIndex) {
	uint32_t start = millis();
	uint8_t txackbytes = SPI_MASTER_SYNBYTE, rxackbytes = 0x00;
	do {
	    spi::select(slotIndex, spi::CHIP_DCM220);
	    spi::transfer(slotIndex, &txackbytes, &rxackbytes, 1);
	    spi::deselect(slotIndex);

	    int32_t diff = millis() - start;
	    if (diff > CONF_MASTER_SYNC_TIMEOUT_MS) {
	    	return false;
	    }
	} while(rxackbytes != SPI_SLAVE_SYNBYTE);

	while (HAL_GPIO_ReadPin(SPI_IRQ_GPIO_Port[slotIndex], SPI_IRQ_Pin[slotIndex]) != GPIO_PIN_SET) {
	    int32_t diff = millis() - start;
	    if (diff > CONF_MASTER_SYNC_TIMEOUT_MS) {
	    	return false;
	    }
	}

	return true;
}

float calcTemperature(uint16_t adcValue) {
	if (adcValue == 65535) {
		// not measured yet
		return 25;
	}

	// http://www.giangrandi.ch/electronics/ntc/ntc.shtml

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

	void getParams(int subchannelIndex, ChannelParams &params) {
		params.U_MIN = 0.0f;
		params.U_DEF = 0.0f;
		params.U_MAX = 20.0f;
		params.U_MAX_CONF = 20.0f;

		params.U_MIN_STEP = 0.01f;
		params.U_DEF_STEP = 0.1f;
		params.U_MAX_STEP = 5.0f;

		params.U_CAL_VAL_MIN = 0.75f;
		params.U_CAL_VAL_MID = 10.0f;
		params.U_CAL_VAL_MAX = 18.0f;
		params.U_CURR_CAL = 20.0f;

		params.I_MIN = 0.0f;
		params.I_DEF = 0.0f;
		params.I_MAX = 4.0f;
		params.I_MAX_CONF = 4.0f;

		params.I_MIN_STEP = 0.01f;
		params.I_DEF_STEP = 0.01f;
		params.I_MAX_STEP = 1.0f; 

		params.I_CAL_VAL_MIN = 0.05f;
		params.I_CAL_VAL_MID = 1.95f;
		params.I_CAL_VAL_MAX = 3.8f;
		params.I_VOLT_CAL = 0.5f;

		params.OVP_DEFAULT_STATE = false;
		params.OVP_MIN_DELAY = 0.0f;
		params.OVP_DEFAULT_DELAY = 0.005f;
		params.OVP_MAX_DELAY = 10.0f;

		params.OCP_DEFAULT_STATE = false;
		params.OCP_MIN_DELAY = 0.0f;
		params.OCP_DEFAULT_DELAY = 0.02f;
		params.OCP_MAX_DELAY = 10.0f;

		params.OPP_DEFAULT_STATE = true;
		params.OPP_MIN_DELAY = 1.0f;
		params.OPP_DEFAULT_DELAY = 10.0f;
		params.OPP_MAX_DELAY = 300.0f;
		params.OPP_MIN_LEVEL = 0.0f;
		params.OPP_DEFAULT_LEVEL = 80.0f;
		params.OPP_MAX_LEVEL = 80.0f;

		params.PTOT = 80.0f;

		params.U_RESOLUTION = 0.01f;
		params.I_RESOLUTION = 0.02f;
		params.I_LOW_RESOLUTION = 0;
		params.P_RESOLUTION = 0.001f;

		params.VOLTAGE_GND_OFFSET = 0;
		params.CURRENT_GND_OFFSET = 0;

		params.CALIBRATION_DATA_TOLERANCE_PERCENT = 15.0f;

		params.CALIBRATION_MID_TOLERANCE_PERCENT = 2.0f;

		params.features = CH_FEATURE_VOLT | CH_FEATURE_CURRENT | CH_FEATURE_POWER | CH_FEATURE_OE;
	}

#if defined(EEZ_PLATFORM_STM32)
    bool isCrcOk;

	void transfer() {
		spi::select(slotIndex, spi::CHIP_DCM220);
		spi::transfer(slotIndex, output, input, BUFFER_SIZE);
		spi::deselect(slotIndex);

		lastTickCount = micros();

		uint32_t crc = HAL_CRC_Calculate(&hcrc, (uint32_t *)input, BUFFER_SIZE - 4);
		isCrcOk = crc == *((uint32_t *)(input + BUFFER_SIZE - 4));
	}
#endif

	void init(int subchannelIndex) {
#if defined(EEZ_PLATFORM_STM32)
		if (!synchronized) {
			spi::deselectA(slotIndex);
			spi::init(slotIndex, spi::CHIP_DCM220);
			if (masterSynchro(slotIndex)) {
				synchronized = true;
				delay(1);
			}
		}
#endif
	}

	void reset(int subchannelIndex) {
	}

	void test(int subchannelIndex) {
#if defined(EEZ_PLATFORM_STM32)
		if (!synchronized) {
			testResult = TEST_FAILED;
			return;
		}
#endif

		psu::Channel &channel = psu::Channel::getBySlotIndex(slotIndex, subchannelIndex);

#if defined(EEZ_PLATFORM_STM32)
		if (subchannelIndex == 0) {
			output[0] = 0;

			transfer();
		}

		bool pwrGood;
		if (isCrcOk) {
			pwrGood = input[0] & REG0_PWRGOOD_MASK ? true : false;
		} else{
			pwrGood = true;
		}
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

		    if (isCrcOk) {
		    	temperature[0] = calcTemperature(*((uint16_t *)(input + 10)));
		    	temperature[1] = calcTemperature(*((uint16_t *)(input + 12)));
		    }
        }
#endif

#if defined(EEZ_PLATFORM_SIMULATOR)
        if (channel.isOutputEnabled()) {
            if (channel.simulator.getLoadEnabled()) {
                float u_set_v = uSet[channel.subchannelIndex];
                float i_set_a = iSet[channel.subchannelIndex];

                float u_mon_v = i_set_a * channel.simulator.load;
                float i_mon_a = i_set_a;
                if (u_mon_v > u_set_v) {
                    u_mon_v = u_set_v;
                    i_mon_a = u_set_v / channel.simulator.load;

                    simulator::setCC(channel.channelIndex, false);
                } else {
                    simulator::setCC(channel.channelIndex, true);
                }

                uMon[channel.subchannelIndex] = u_mon_v;
                iMon[channel.subchannelIndex] = i_mon_a;
            } else {
                uMon[channel.subchannelIndex] = uSet[channel.subchannelIndex];
                iMon[channel.subchannelIndex] = 0;
                if (uSet[channel.subchannelIndex] > 0 && iSet[channel.subchannelIndex] > 0) {
                    simulator::setCC(channel.channelIndex, false);
                } else {
                    simulator::setCC(channel.channelIndex, true);
                }
            }
        } else {
            uMon[channel.subchannelIndex] = 0;
            iMon[channel.subchannelIndex] = 0;
            simulator::setCC(channel.channelIndex, false);
        }
#endif

        //

#if defined(EEZ_PLATFORM_STM32)
        if (isCrcOk) {
        	uint16_t *inputSetValues = (uint16_t *)(input + 2);

        	int offset = subchannelIndex * 2;

        	uint16_t uMonAdc = inputSetValues[offset];
        	float uMon = remap(uMonAdc, (float)ADC_MIN, channel.params.U_MIN, (float)ADC_MAX, channel.params.U_MAX);
        	channel.onAdcData(ADC_DATA_TYPE_U_MON, uMon);

        	uint16_t iMonAdc = inputSetValues[offset + 1];
        	float iMon = remap(iMonAdc, (float)ADC_MIN, channel.params.I_MIN, (float)ADC_MAX, channel.params.I_MAX);
        	channel.onAdcData(ADC_DATA_TYPE_I_MON, iMon);
        }
#endif

#if defined(EEZ_PLATFORM_SIMULATOR)
        channel.onAdcData(ADC_DATA_TYPE_U_MON, uMon[channel.subchannelIndex]);
        channel.onAdcData(ADC_DATA_TYPE_I_MON, iMon[channel.subchannelIndex]);
#endif

        // PWRGOOD

#if defined(EEZ_PLATFORM_STM32)
		bool pwrGood;
		if (isCrcOk) {
			pwrGood = input[0] & REG0_PWRGOOD_MASK ? true : false;
		} else{
			pwrGood = true;
		}
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

    DprogState getDprogState() {
        return DPROG_STATE_OFF;
    }

    void setDprogState(DprogState dprogState) {
    }
    
    void setDacVoltage(int subchannelIndex, uint16_t value) {

#if defined(EEZ_PLATFORM_STM32)
        value = (uint16_t)clamp((float)value, (float)DAC_MIN, (float)DAC_MAX);
        uSet[subchannelIndex] = clamp(value, DAC_MIN, DAC_MAX);
#endif

#if defined(EEZ_PLATFORM_SIMULATOR)
        psu::Channel &channel = psu::Channel::getBySlotIndex(slotIndex, subchannelIndex);
        uSet[subchannelIndex] = remap(clamp((float)value, (float)DAC_MIN, (float)DAC_MAX), (float)DAC_MIN, channel.params.U_MIN, (float)DAC_MAX, channel.params.U_MAX);
#endif
	}

	void setDacVoltageFloat(int subchannelIndex, float value) {
#if defined(EEZ_PLATFORM_STM32)
        psu::Channel &channel = psu::Channel::getBySlotIndex(slotIndex, subchannelIndex);
        value = remap(value, channel.params.U_MIN, (float)DAC_MIN, channel.params.U_MAX, (float)DAC_MAX);
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
        uSet[subchannelIndex] = remap(clamp((float)value, (float)DAC_MIN, (float)DAC_MAX), (float)DAC_MIN, channel.params.I_MIN, (float)DAC_MAX, channel.params.I_MAX);
#endif
    }

	void setDacCurrentFloat(int subchannelIndex, float value) {
#if defined(EEZ_PLATFORM_STM32)
        psu::Channel &channel = psu::Channel::getBySlotIndex(slotIndex, subchannelIndex);
        value = remap(value, channel.params.I_MIN, (float)DAC_MIN, channel.params.I_MAX, (float)DAC_MAX);
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
ChannelInterface *g_channelInterfaces[NUM_SLOTS] = { &g_channel0, &g_channel1, &g_channel2 };

#if defined(EEZ_PLATFORM_STM32)

float readTemperature(int channelIndex) {
	psu::Channel& channel = psu::Channel::get(channelIndex);
	int slotIndex = channel.slotIndex;
	int subchannelIndex = channel.subchannelIndex;
	Channel *dcm220Channel = (Channel *)g_channelInterfaces[slotIndex];
	return dcm220Channel->temperature[subchannelIndex];
}

#endif

} // namespace dcm220
} // namespace eez
