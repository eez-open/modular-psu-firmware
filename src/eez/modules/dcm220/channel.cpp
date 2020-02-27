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

#include <stdio.h>
#include <string.h>

#if defined(EEZ_PLATFORM_STM32)
#include <main.h>
#include <crc.h>
#include <eez/platform/stm32/spi.h>
#include <memory.h>
#include <stdlib.h>
#endif

#include <eez/modules/dcpX05/channel.h>

#include <eez/modules/psu/psu.h>
#include <eez/modules/psu/channel_dispatcher.h>
#include <eez/modules/psu/event_queue.h>
#include <eez/scpi/regs.h>
#include <eez/system.h>

#define CONF_MASTER_SYNC_TIMEOUT_MS 500
#define CONF_MASTER_SYNC_IRQ_TIMEOUT_MS 50

namespace eez {

using namespace psu;

namespace dcm220 {

#define DAC_MIN 0
#define DAC_MAX 4095

#define REG0_OE1_MASK     (1 << 0)
#define REG0_OE2_MASK     (1 << 2)

#define BUFFER_SIZE 20

#if defined(EEZ_PLATFORM_STM32)

#define REG0_CC1_MASK     (1 << 1)
#define REG0_CC2_MASK     (1 << 3)
#define REG0_PWRGOOD_MASK (1 << 4)

#define ADC_MIN 0
#define ADC_MAX 65535

#define SPI_SLAVE_SYNBYTE         0x53
#define SPI_MASTER_SYNBYTE        0xAC

static GPIO_TypeDef *SPI_IRQ_GPIO_Port[] = { SPI2_IRQ_GPIO_Port, SPI4_IRQ_GPIO_Port, SPI5_IRQ_GPIO_Port };
static const uint16_t SPI_IRQ_Pin[] = { SPI2_IRQ_Pin, SPI4_IRQ_Pin, SPI5_IRQ_Pin };

bool masterSynchro(int slotIndex, uint8_t &firmwareMajorVersion, uint8_t &firmwareMinorVersion, uint32_t &idw0, uint32_t &idw1, uint32_t &idw2) {
	uint32_t start = millis();

    uint8_t txBuffer[15] = { SPI_MASTER_SYNBYTE, 0, 0 };
    uint8_t rxBuffer[15] = { 0, 0, 0 };

	while (true) {
	    spi::select(slotIndex, spi::CHIP_DCM220);
	    spi::transfer(slotIndex, txBuffer, rxBuffer, sizeof(rxBuffer));
	    spi::deselect(slotIndex);

	    if (rxBuffer[0] == SPI_SLAVE_SYNBYTE) {
			uint32_t startIrq = millis();
			while (true) {
				if (HAL_GPIO_ReadPin(SPI_IRQ_GPIO_Port[slotIndex], SPI_IRQ_Pin[slotIndex]) == GPIO_PIN_SET) {
					firmwareMajorVersion = rxBuffer[1];
					firmwareMinorVersion = rxBuffer[2];
					idw0 = (rxBuffer[3] << 24) | (rxBuffer[4] << 16) | (rxBuffer[5] << 8) | rxBuffer[6];
					idw1 = (rxBuffer[7] << 24) | (rxBuffer[8] << 16) | (rxBuffer[9] << 8) | rxBuffer[10];
					idw2 = (rxBuffer[11] << 24) | (rxBuffer[12] << 16) | (rxBuffer[13] << 8) | rxBuffer[14];
					return true;
				}

				int32_t diff = millis() - startIrq;
				if (diff > CONF_MASTER_SYNC_IRQ_TIMEOUT_MS) {
					break;
				}
			}
	    }

	    int32_t diff = millis() - start;
	    if (diff > CONF_MASTER_SYNC_TIMEOUT_MS) {
	    	return false;
	    }
	}
}

float calcTemperature(uint16_t adcValue) {
	if (adcValue == 65535) {
		// not measured yet
		return 25;
	}

	// http://www.giangrandi.ch/electronics/ntc/ntc.shtml

	float RF = 3300.0f;
	float T25 = 298.15F;
	float R25 = 10000;
	float BETA = 3570;
	float ADC_MAX_FOR_TEMP = 4095.0f;

	float RT = RF * (ADC_MAX_FOR_TEMP  - adcValue) / adcValue;

	float Tkelvin = 1 / (logf(RT / R25) / BETA + 1 / T25);
	float Tcelsius = Tkelvin - 273.15f;

	float TEMP_OFFSET = 10.0f; // empirically determined
	Tcelsius -= TEMP_OFFSET;

	return roundPrec(Tcelsius, 1.0f);
}

#endif

struct Channel : ChannelInterface {
    uint32_t lastTickCount;

    bool outputEnable[2];

    uint8_t output[BUFFER_SIZE];

#if defined(EEZ_PLATFORM_STM32)
	bool synchronized;

	uint8_t input[BUFFER_SIZE];

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

	uint8_t firmwareMajorVersion = 0;
	uint8_t firmwareMinorVersion = 0;
	uint32_t idw0 = 0;
	uint32_t idw1 = 0;
	uint32_t idw2 = 0;

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
		params.U_MIN = 1.0f;
		params.U_DEF = 1.0f;
		params.U_MAX = 20.0f;

		params.U_MIN_STEP = 0.01f;
		params.U_DEF_STEP = 0.1f;
		params.U_MAX_STEP = 5.0f;

		params.U_CAL_VAL_MIN = 2.0f;
		params.U_CAL_VAL_MID = 10.0f;
		params.U_CAL_VAL_MAX = 18.0f;
		params.I_VOLT_CAL = 1.0f;

		params.I_MIN = 0.0f;
		params.I_DEF = 0.0f;
		params.I_MAX = 4.0f;

		params.I_MIN_STEP = 0.01f;
		params.I_DEF_STEP = 0.01f;
		params.I_MAX_STEP = 1.0f; 

		params.I_CAL_VAL_MIN = 0.5f;
		params.I_CAL_VAL_MID = 2.0f;
		params.I_CAL_VAL_MAX = 3.5f;
		params.U_CURR_CAL = 20.0f;

		params.OVP_DEFAULT_STATE = false;
		params.OVP_MIN_DELAY = 0.0f;
		params.OVP_DEFAULT_DELAY = 0.0f;
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

		params.PTOT = 77.5f;

		params.U_RESOLUTION = 0.01f;
		params.I_RESOLUTION = 0.02f;
		params.I_LOW_RESOLUTION = 0;
		params.P_RESOLUTION = 0.001f;

		params.VOLTAGE_GND_OFFSET = 0;
		params.CURRENT_GND_OFFSET = 0;

		params.CALIBRATION_DATA_TOLERANCE_PERCENT = 15.0f;

		params.CALIBRATION_MID_TOLERANCE_PERCENT = 3.0f;

		params.features = CH_FEATURE_VOLT | CH_FEATURE_CURRENT | CH_FEATURE_POWER | CH_FEATURE_OE;

		params.MON_REFRESH_RATE_MS = 500;
	}

#if defined(EEZ_PLATFORM_STM32)
    int numCrcErrors;

	void transfer() {
		spi::select(slotIndex, spi::CHIP_DCM220);
		spi::transfer(slotIndex, output, input, BUFFER_SIZE);
		spi::deselect(slotIndex);

		lastTickCount = micros();

		uint32_t crc = HAL_CRC_Calculate(&hcrc, (uint32_t *)input, BUFFER_SIZE - 4);
		if (crc == *((uint32_t *)(input + BUFFER_SIZE - 4))) {
			numCrcErrors = 0;
		} else {
			if (++numCrcErrors >= 3) {
				event_queue::pushEvent(event_queue::EVENT_ERROR_SLOT1_CRC_CHECK_ERROR + slotIndex);
				synchronized = false;
				testResult = TEST_FAILED;
			}
		}
	}
#endif

	void init(int subchannelIndex) {
#if defined(EEZ_PLATFORM_STM32)
		if (!synchronized && subchannelIndex == 0) {
			if (masterSynchro(slotIndex, firmwareMajorVersion, firmwareMinorVersion, idw0, idw1, idw2)) {
	    		//DebugTrace("DCM220 slot #%d firmware version %d.%d\n", slotIndex + 1, (int)firmwareMajorVersion, (int)firmwareMinorVersion);
				synchronized = true;
				numCrcErrors = 0;
			} else {
				event_queue::pushEvent(event_queue::EVENT_ERROR_SLOT1_SYNC_ERROR + slotIndex);
			    firmwareMinorVersion = 0;
			    firmwareMajorVersion = 0;
				idw0 = 0;
				idw1 = 0;
				idw2 = 0;
			}
		}
#endif

#if defined(EEZ_PLATFORM_SIMULATOR)
        firmwareMajorVersion = 1;
		firmwareMinorVersion = 0;
		idw0 = 0;
		idw1 = 0;
		idw2 = 0;
#endif
    }

	void onPowerDown(int subchannelIndex) {
#if defined(EEZ_PLATFORM_STM32)
		synchronized = false;
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
		if (numCrcErrors == 0) {
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

        int32_t diff = tickCount - lastTickCount;
        if (subchannelIndex == 0 && diff > 1000) {
        	uint8_t output0 = 0x80 | (outputEnable[0] ? REG0_OE1_MASK : 0) | (outputEnable[1] ? REG0_OE2_MASK : 0);

            output[0] = output0;

#if defined(EEZ_PLATFORM_STM32)
        	output[1] = 0;

        	uint16_t *outputSetValues = (uint16_t *)(output + 2);

        	outputSetValues[0] = uSet[0];
        	outputSetValues[1] = iSet[0];
        	outputSetValues[2] = uSet[1];
        	outputSetValues[3] = iSet[1];

#ifdef DEBUG
			psu::debug::g_uDac[channel.channelIndex + 0].set(uSet[0]);
			psu::debug::g_iDac[channel.channelIndex + 0].set(iSet[0]);
			psu::debug::g_uDac[channel.channelIndex + 1].set(uSet[1]);
			psu::debug::g_iDac[channel.channelIndex + 1].set(iSet[1]);
#endif

			transfer();
#endif

#if defined(EEZ_PLATFORM_STM32)
            if (numCrcErrors == 0) {
		    	temperature[0] = calcTemperature(*((uint16_t *)(input + 10)));
		    	temperature[1] = calcTemperature(*((uint16_t *)(input + 12)));
		    }
#endif
        }

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
        if (numCrcErrors == 0) {
        	uint16_t *inputSetValues = (uint16_t *)(input + 2);

        	int offset = subchannelIndex * 2;

        	uint16_t uMonAdc = inputSetValues[offset];
        	float uMon = remap(uMonAdc, (float)ADC_MIN, 0, (float)ADC_MAX, channel.params.U_MAX);
        	channel.onAdcData(ADC_DATA_TYPE_U_MON, uMon);

        	uint16_t iMonAdc = inputSetValues[offset + 1];

			const float FULL_SCALE = 2.0F;
			const float U_REF = 2.5F;
        	float iMon = remap(iMonAdc, (float)ADC_MIN, 0, FULL_SCALE * ADC_MAX / U_REF, /*channel.params.I_MAX*/ 4.1667f);
        	channel.onAdcData(ADC_DATA_TYPE_I_MON, iMon);

#ifdef DEBUG
			psu::debug::g_uMon[channel.channelIndex].set(uMonAdc);
			psu::debug::g_iMon[channel.channelIndex].set(iMonAdc);
#endif
        }
#endif

#if defined(EEZ_PLATFORM_SIMULATOR)
        channel.onAdcData(ADC_DATA_TYPE_U_MON, uMon[channel.subchannelIndex]);
        channel.onAdcData(ADC_DATA_TYPE_I_MON, iMon[channel.subchannelIndex]);
#endif

        // PWRGOOD

#if defined(EEZ_PLATFORM_STM32)
		bool pwrGood;
		if (numCrcErrors == 0) {
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

	void setOutputEnable(int subchannelIndex, bool enable, uint16_t tasks) {
		outputEnable[subchannelIndex] = enable;
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
        uSet[subchannelIndex] = remap(clamp((float)value, (float)DAC_MIN, (float)DAC_MAX), (float)DAC_MIN, 0, (float)DAC_MAX, channel.params.U_MAX);
#endif
	}

	void setDacVoltageFloat(int subchannelIndex, float value) {
#if defined(EEZ_PLATFORM_STM32)
        psu::Channel &channel = psu::Channel::getBySlotIndex(slotIndex, subchannelIndex);
        value = remap(value, 0, (float)DAC_MIN, channel.params.U_MAX, (float)DAC_MAX);
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
        value = remap(value, channel.params.I_MIN, (float)DAC_MIN, /*channel.params.I_MAX*/ 4.1667f, (float)DAC_MAX);
        iSet[subchannelIndex] = (uint16_t)clamp(round(value), DAC_MIN, DAC_MAX);
#endif

#if defined(EEZ_PLATFORM_SIMULATOR)
        iSet[subchannelIndex] = value;
#endif
	}

	bool isDacTesting(int subchannelIndex) {
		return false;
	}

	void getFirmwareVersion(uint8_t &majorVersion, uint8_t &minorVersion) {
		majorVersion = firmwareMajorVersion;
		minorVersion = firmwareMinorVersion;
	}

    const char *getBrand() {
		return "Envox";
	}
    
	void getSerial(char *text) {
#if defined(EEZ_PLATFORM_STM32)		
		sprintf(text, "%08X", (unsigned int)idw0);
		sprintf(text + 8, "%08X", (unsigned int)idw1);
		sprintf(text + 16, "%08X", (unsigned int)idw2);
#else
		strcpy(text, "N/A");
#endif
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
