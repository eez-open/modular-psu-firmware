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
#include <new>

#if defined(EEZ_PLATFORM_STM32)
#include <main.h>
#include <crc.h>
#include <eez/platform/stm32/spi.h>
#include <memory.h>
#include <stdlib.h>
#endif

#include <eez/modules/dcm220/channel.h>

#include <eez/modules/psu/psu.h>
#include <eez/modules/psu/channel_dispatcher.h>
#include <eez/modules/psu/event_queue.h>
#include <eez/scpi/regs.h>
#include <eez/system.h>
#include <eez/firmware.h>

#define CONF_MASTER_SYNC_TIMEOUT_MS 500
#define CONF_MASTER_SYNC_IRQ_TIMEOUT_MS 50

namespace eez {

using namespace psu;

namespace dcm220 {

static const uint16_t DAC_MIN = 0;
static const uint16_t DAC_MAX = 4095;

static const uint16_t ADC_MIN = 0;
static const uint16_t ADC_MAX = 65535;

#define REG0_OE1_MASK     (1 << 0)
#define REG0_OE2_MASK     (1 << 2)

#define BUFFER_SIZE 20

static const float PTOT = 155.0f;

static const float I_MON_RESOLUTION = 0.02f;

#if defined(EEZ_PLATFORM_STM32)

#define REG0_CC1_MASK     (1 << 1)
#define REG0_CC2_MASK     (1 << 3)
#define REG0_PWRGOOD_MASK (1 << 4)

#define SPI_SLAVE_SYNBYTE         0x53
#define SPI_MASTER_SYNBYTE        0xAC

static GPIO_TypeDef *SPI_IRQ_GPIO_Port[] = { SPI2_IRQ_GPIO_Port, SPI4_IRQ_GPIO_Port, SPI5_IRQ_GPIO_Port };
static const uint16_t SPI_IRQ_Pin[] = { SPI2_IRQ_Pin, SPI4_IRQ_Pin, SPI5_IRQ_Pin };

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

bool masterSynchro(int slotIndex) {
	auto &slot = g_slots[slotIndex];

#if defined(EEZ_PLATFORM_STM32)
	uint32_t start = millis();

    uint8_t txBuffer[15] = { SPI_MASTER_SYNBYTE, 0, 0 };
    uint8_t rxBuffer[15] = { 0, 0, 0 };

	while (true) {
		WATCHDOG_RESET();

	    spi::select(slotIndex, spi::CHIP_DCM220);
	    spi::transfer(slotIndex, txBuffer, rxBuffer, sizeof(rxBuffer));
	    spi::deselect(slotIndex);

	    if (rxBuffer[0] == SPI_SLAVE_SYNBYTE) {
			uint32_t startIrq = millis();
			while (true) {
				if (HAL_GPIO_ReadPin(SPI_IRQ_GPIO_Port[slotIndex], SPI_IRQ_Pin[slotIndex]) == GPIO_PIN_SET) {
					slot.firmwareMajorVersion = rxBuffer[1];
					slot.firmwareMinorVersion = rxBuffer[2];
					slot.idw0 = (rxBuffer[3] << 24) | (rxBuffer[4] << 16) | (rxBuffer[5] << 8) | rxBuffer[6];
					slot.idw1 = (rxBuffer[7] << 24) | (rxBuffer[8] << 16) | (rxBuffer[9] << 8) | rxBuffer[10];
					slot.idw2 = (rxBuffer[11] << 24) | (rxBuffer[12] << 16) | (rxBuffer[13] << 8) | rxBuffer[14];
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
			slot.firmwareMajorVersion = 0;
			slot.firmwareMinorVersion = 0;
			slot.idw0 = 0;
			slot.idw1 = 0;
			slot.idw2 = 0;
	    	return false;
	    }
	}
#endif

#if defined(EEZ_PLATFORM_SIMULATOR)
	slot.firmwareMajorVersion = 1;
	slot.firmwareMinorVersion = 0;
	slot.idw0 = 0;
	slot.idw1 = 0;
	slot.idw2 = 0;
	return true;
#endif

}

struct DcmChannel : public Channel {
    bool outputEnable;

    uint8_t output[BUFFER_SIZE];

	bool synchronized;

#if defined(EEZ_PLATFORM_STM32)
	uint8_t input[BUFFER_SIZE];

	uint16_t uSet;
	uint16_t iSet;

	float temperature;
#endif

#if defined(EEZ_PLATFORM_SIMULATOR)
    float uMon;
    float iMon;
    float uSet;
    float iSet;
#endif

    TestResult testResult;

	float I_MAX_FOR_REMAP;

	float U_CAL_POINTS[2];
	float I_CAL_POINTS[6];

	bool ccMode = false;

    DcmChannel(uint8_t slotIndex, uint8_t channelIndex, uint8_t subchannelIndex)
        : Channel(slotIndex, channelIndex, subchannelIndex)
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

	void getParams() override {
		auto slot = g_slots[slotIndex];

		params.U_MIN = 1.0f;
		params.U_DEF = 1.0f;
		params.U_MAX = slot.moduleInfo->moduleType == MODULE_TYPE_DCM224 ? 24.0f : 20.0f;

		params.U_MIN_STEP = 0.01f;
		params.U_DEF_STEP = 0.1f;
		params.U_MAX_STEP = 5.0f;

		U_CAL_POINTS[0] = 2.0f;
		U_CAL_POINTS[1] = slot.moduleInfo->moduleType == MODULE_TYPE_DCM224 ? 22.0f : 18.0f;
		params.U_CAL_NUM_POINTS = 2;
		params.U_CAL_POINTS = U_CAL_POINTS;
		params.U_CAL_I_SET = 1.0f;

		params.I_MIN = slot.moduleInfo->moduleType == MODULE_TYPE_DCM224 ? 0.0f : 0.25f;
		params.I_DEF = 0.0f;
		params.I_MAX = slot.moduleInfo->moduleType == MODULE_TYPE_DCM224 ? 4.9f : 4.0f;

    	params.I_MON_MIN = slot.moduleInfo->moduleType == MODULE_TYPE_DCM224 ? 0.0f : 0.25f;

		params.I_MIN_STEP = 0.01f;
		params.I_DEF_STEP = 0.01f;
		params.I_MAX_STEP = 1.0f; 

		if (slot.moduleInfo->moduleType == MODULE_TYPE_DCM224) {
			I_CAL_POINTS[0] = 0.1f;
			I_CAL_POINTS[1] = 4.5f;
			params.I_CAL_NUM_POINTS = 2;
		} else {
			I_CAL_POINTS[0] = 0.1f;
			I_CAL_POINTS[1] = 0.2f;
			I_CAL_POINTS[2] = 0.3f;
			I_CAL_POINTS[3] = 0.4f;
			I_CAL_POINTS[4] = 0.5f;
			I_CAL_POINTS[5] = 3.5f;
			params.I_CAL_NUM_POINTS = 6;
		}
		params.I_CAL_POINTS = I_CAL_POINTS;
		params.I_CAL_U_SET = 20.0f;

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

		params.PTOT = MIN(params.U_MAX * params.I_MAX, 155.0f);

		params.U_RESOLUTION = 0.01f;
		params.U_RESOLUTION_DURING_CALIBRATION = 0.001f;
		params.I_RESOLUTION = 0.01f;
		params.I_RESOLUTION_DURING_CALIBRATION = 0.001f;
		params.I_LOW_RESOLUTION = 0.0f;
		params.I_LOW_RESOLUTION_DURING_CALIBRATION = 0.0f;
		params.P_RESOLUTION = 0.001f;

		params.VOLTAGE_GND_OFFSET = 0;
		params.CURRENT_GND_OFFSET = 0;

		params.CALIBRATION_DATA_TOLERANCE_PERCENT = 15.0f;

		params.CALIBRATION_MID_TOLERANCE_PERCENT = 3.0f;

		params.features = CH_FEATURE_VOLT | CH_FEATURE_CURRENT | CH_FEATURE_POWER | CH_FEATURE_OE;

		params.MON_REFRESH_RATE_MS = 500;

		params.DAC_MAX = DAC_MAX;
		params.ADC_MAX = ADC_MAX;

		I_MAX_FOR_REMAP = slot.moduleInfo->moduleType == MODULE_TYPE_DCM224 ? 5.0f : 4.1667f;

		params.U_RAMP_DURATION_MIN_VALUE = 0.002f;
	}

#if defined(EEZ_PLATFORM_STM32)
    int numCrcErrors;

	void transfer() {
		spi::select(slotIndex, spi::CHIP_DCM220);
		spi::transfer(slotIndex, output, input, BUFFER_SIZE);
		spi::deselect(slotIndex);

		uint32_t crc = HAL_CRC_Calculate(&hcrc, (uint32_t *)input, BUFFER_SIZE - 4);
		if (crc == *((uint32_t *)(input + BUFFER_SIZE - 4))) {
			numCrcErrors = 0;
		} else {
			if (++numCrcErrors >= 4) {
				event_queue::pushEvent(event_queue::EVENT_ERROR_SLOT1_CRC_CHECK_ERROR + slotIndex);
				synchronized = false;
				testResult = TEST_FAILED;
			} else {
				DebugTrace("CRC %d\n", numCrcErrors);
			}
		}
	}
#endif

	void init() override {
		if (!synchronized && subchannelIndex == 0) {
			if (masterSynchro(slotIndex)) {
	    		//DebugTrace("DCM220 slot #%d firmware version %d.%d\n", slotIndex + 1, (int)firmwareMajorVersion, (int)firmwareMinorVersion);
				synchronized = true;
#if defined(EEZ_PLATFORM_STM32)
				numCrcErrors = 0;
#endif
			} else {
				event_queue::pushEvent(event_queue::EVENT_ERROR_SLOT1_SYNC_ERROR + slotIndex);
			}
		}
    }

	void onPowerDown() {
		Channel::onPowerDown();

#if defined(EEZ_PLATFORM_STM32)
		synchronized = false;
#endif
	}

	bool test() {
		if (subchannelIndex == 1) {
			auto& firstChannel = (DcmChannel &)Channel::get(channelIndex - 1);
			testResult = firstChannel.testResult;
			flags.powerOk = firstChannel.flags.powerOk;
			return isOk();
		}

		Channel::test();

#if defined(EEZ_PLATFORM_STM32)
		if (!synchronized) {
			testResult = TEST_FAILED;
			return false;
		}
#endif

#if defined(EEZ_PLATFORM_STM32)
			output[0] = 0;

			transfer();

		bool pwrGood;
		if (numCrcErrors == 0) {
			pwrGood = input[0] & REG0_PWRGOOD_MASK ? true : false;
		} else{
			pwrGood = true;
		}
#endif

#if defined(EEZ_PLATFORM_SIMULATOR)
        bool pwrGood = simulator::getPwrgood(channelIndex);
#endif

		flags.powerOk = pwrGood ? 1 : 0;

		testResult = flags.powerOk ? TEST_OK : TEST_FAILED;

		return isOk();
	}

	TestResult getTestResult() {
		return testResult;
	}

	void tickSpecific(uint32_t tickCount) {
#if defined(EEZ_PLATFORM_STM32)
        if (subchannelIndex == 0) {
			DcmChannel &secondChannel = (DcmChannel &)Channel::get(channelIndex + 1);

        	uint8_t output0 = 0x80 | (outputEnable ? REG0_OE1_MASK : 0) | (secondChannel.outputEnable ? REG0_OE2_MASK : 0);

            output[0] = output0;

        	output[1] = 0;

        	uint16_t *outputSetValues = (uint16_t *)(output + 2);

        	outputSetValues[0] = uSet;
        	outputSetValues[1] = iSet;
        	outputSetValues[2] = secondChannel.uSet;
        	outputSetValues[3] = secondChannel.iSet;

#ifdef DEBUG
			psu::debug::g_uDac[channelIndex + 0].set(uSet);
			psu::debug::g_iDac[channelIndex + 0].set(iSet);
			psu::debug::g_uDac[channelIndex + 1].set(secondChannel.uSet);
			psu::debug::g_iDac[channelIndex + 1].set(secondChannel.iSet);
#endif

			transfer();

            if (numCrcErrors == 0) {
		    	temperature = calcTemperature(*((uint16_t *)(input + 10)));
		    	secondChannel.temperature = calcTemperature(*((uint16_t *)(input + 12)));

				uint16_t *inputSetValues = (uint16_t *)(input + 2);

				for (int subchannelIndex = 0; subchannelIndex < 2; subchannelIndex++) {
					auto &channel = (DcmChannel &)Channel::get(channelIndex + subchannelIndex);
					int offset = subchannelIndex * 2;

					channel.ccMode = (input[0] & (subchannelIndex == 0 ? REG0_CC1_MASK : REG0_CC2_MASK)) != 0;

					uint16_t uMonAdc = inputSetValues[offset];
					float uMon = remap(uMonAdc, (float)ADC_MIN, 0, (float)ADC_MAX, params.U_MAX);
					channel.onAdcData(ADC_DATA_TYPE_U_MON, uMon);

					uint16_t iMonAdc = inputSetValues[offset + 1];
					const float FULL_SCALE = 2.0F;
					const float U_REF = 2.5F;
					float iMon = remap(iMonAdc, (float)ADC_MIN, 0, FULL_SCALE * ADC_MAX / U_REF, /*params.I_MAX*/ I_MAX_FOR_REMAP);
					iMon = roundPrec(iMon, I_MON_RESOLUTION);
					channel.onAdcData(ADC_DATA_TYPE_I_MON, iMon);

#if !CONF_SKIP_PWRGOOD_TEST
					bool pwrGood = input[0] & REG0_PWRGOOD_MASK ? true : false;
					if (!pwrGood) {
						channel.flags.powerOk = 0;
						generateError(SCPI_ERROR_CH1_FAULT_DETECTED - channel.channelIndex);
						powerDownBySensor();
		    }
#endif

#ifdef DEBUG
					psu::debug::g_uMon[channelIndex + subchannelIndex].set(uMonAdc);
					psu::debug::g_iMon[channelIndex + subchannelIndex].set(iMonAdc);
#endif
				}
        }
        }
#endif

#if defined(EEZ_PLATFORM_SIMULATOR)
        if (isOutputEnabled()) {
            if (simulator.getLoadEnabled()) {
                float u_set_v = uSet;
                float i_set_a = iSet;

                float u_mon_v = i_set_a * simulator.load;
                float i_mon_a = i_set_a;
                if (u_mon_v > u_set_v) {
                    u_mon_v = u_set_v;
                    i_mon_a = u_set_v / simulator.load;

                    simulator::setCC(channelIndex, false);
                } else {
                    simulator::setCC(channelIndex, true);
                }

                uMon = u_mon_v;
                iMon = i_mon_a;
            } else {
                uMon = uSet;
                iMon = 0;
                if (uSet > 0 && iSet > 0) {
                    simulator::setCC(channelIndex, false);
                } else {
                    simulator::setCC(channelIndex, true);
                }
            }
        } else {
            uMon = 0;
            iMon = 0;
            simulator::setCC(channelIndex, false);
        }

        	onAdcData(ADC_DATA_TYPE_U_MON, uMon);
        	onAdcData(ADC_DATA_TYPE_I_MON, iMon);

#if !CONF_SKIP_PWRGOOD_TEST
        bool pwrGood = simulator::getPwrgood(channelIndex);
		if (!pwrGood) {
			flags.powerOk = 0;
			generateError(SCPI_ERROR_CH1_FAULT_DETECTED - channelIndex);
			powerDownBySensor();
			return;
		}
#endif

#endif
	}
	
	bool isInCcMode() {
#if defined(EEZ_PLATFORM_STM32)
        return ccMode;
#endif

#if defined(EEZ_PLATFORM_SIMULATOR)
        return simulator::getCC(channelIndex);
#endif
	}

	bool isInCvMode() {
		return !isCcMode();
	}

	void adcMeasureUMon() {
	}

	void adcMeasureIMon() {
	}

	void adcMeasureMonDac() {
	}

	void adcMeasureAll() {
	}

	void setOutputEnable(bool enable, uint16_t tasks) {
		outputEnable = enable;
	}

    void setDacVoltage(uint16_t value) {

#if defined(EEZ_PLATFORM_STM32)
        value = (uint16_t)clamp((float)value, (float)DAC_MIN, (float)DAC_MAX);
        uSet = clamp(value, DAC_MIN, DAC_MAX);
#endif

#if defined(EEZ_PLATFORM_SIMULATOR)
        uSet = remap(clamp((float)value, (float)DAC_MIN, (float)DAC_MAX), (float)DAC_MIN, 0, (float)DAC_MAX, params.U_MAX);
#endif
	}

	void setDacVoltageFloat(float value) {
#if defined(EEZ_PLATFORM_STM32)
        value = remap(value, 0, (float)DAC_MIN, params.U_MAX, (float)DAC_MAX);
        uSet = (uint16_t)clamp(round(value), DAC_MIN, DAC_MAX);
#endif

#if defined(EEZ_PLATFORM_SIMULATOR)
        uSet = value;
#endif
	}

	void setDacCurrent(uint16_t value) {
#if defined(EEZ_PLATFORM_STM32)
        value = (uint16_t)clamp((float)value, (float)DAC_MIN, (float)DAC_MAX);
        iSet = value;
#endif

#if defined(EEZ_PLATFORM_SIMULATOR)
        uSet = remap(clamp((float)value, (float)DAC_MIN, (float)DAC_MAX), (float)DAC_MIN, params.I_MIN, (float)DAC_MAX, params.I_MAX);
#endif
    }

	void setDacCurrentFloat(float value) {
#if defined(EEZ_PLATFORM_STM32)
        value = remap(value, /*params.I_MIN*/ 0, (float)DAC_MIN, /*params.I_MAX*/ I_MAX_FOR_REMAP, (float)DAC_MAX);
        iSet = (uint16_t)clamp(round(value), DAC_MIN, DAC_MAX);
#endif

#if defined(EEZ_PLATFORM_SIMULATOR)
        iSet = value;
#endif
	}

	bool isDacTesting() {
		return false;
	}

    void getVoltageStepValues(StepValues *stepValues, bool calibrationMode) {
        static float values[] = { 1.0f, 0.5f, 0.1f, 0.01f };
		static float calibrationModeValues[] = { 1.0f, 0.1f, 0.01f, 0.001f };
        stepValues->values = calibrationMode ? calibrationModeValues : values;
        stepValues->count = sizeof(values) / sizeof(float);
		stepValues->unit = UNIT_VOLT;
	}
    
	void getCurrentStepValues(StepValues *stepValues, bool calibrationMode) {
        static float values[] = { 0.5f, 0.25f, 0.1f,  0.01f };
		static float calibrationModeValues[] = { 0.05f, 0.01f, 0.005f,  0.001f };
        stepValues->values = calibrationMode ? calibrationModeValues : values;
        stepValues->count = sizeof(values) / sizeof(float);
		stepValues->unit = UNIT_AMPER;
	}

    void getPowerStepValues(StepValues *stepValues) {
        static float values[] = { 10.0f, 1.0f, 0.1f, 0.01f };
        stepValues->values = values;
        stepValues->count = sizeof(values) / sizeof(float);
		stepValues->unit = UNIT_WATT;
	}
	
	bool isPowerLimitExceeded(float u, float i, int *err) {
		float power = u * i;
		if (power > channel_dispatcher::getPowerLimit(*this)) {
			if (err) {
				*err = SCPI_ERROR_POWER_LIMIT_EXCEEDED;
			}
			return true;
		}

        auto &otherChannel = Channel::get(channelIndex + (subchannelIndex == 0 ? 1 : -1));
		float powerOtherChannel = channel_dispatcher::getUSet(otherChannel) * channel_dispatcher::getISet(otherChannel);
		if (power + powerOtherChannel > PTOT) {
			if (err) {
				*err = SCPI_ERROR_MODULE_TOTAL_POWER_LIMIT_EXCEEDED;
			}
			return true;
		}

		return false;
	}
};

struct DcmChannelModuleInfo : public PsuChannelModuleInfo {
public:
	DcmChannelModuleInfo(uint16_t moduleType, const char *moduleName, uint16_t latestModuleRevision) 
		: PsuChannelModuleInfo(moduleType, moduleName, "Envox", latestModuleRevision, 2)
	{
	}

	Channel *createChannel(int slotIndex, int channelIndex, int subchannelIndex) override {
        void *buffer = malloc(sizeof(DcmChannel));
        memset(buffer, 0, sizeof(DcmChannel));
		return new (buffer) DcmChannel(slotIndex, channelIndex, subchannelIndex);
	}
};

static DcmChannelModuleInfo g_dcm220PsuChannelModuleInfo(MODULE_TYPE_DCM220, "DCM220", MODULE_REVISION_DCM220_R2B4);
static DcmChannelModuleInfo g_dcm224PsuChannelModuleInfo(MODULE_TYPE_DCM224, "DCM224", MODULE_REVISION_DCM224_R1B1);

ModuleInfo *g_dcm220ModuleInfo = &g_dcm220PsuChannelModuleInfo;
ModuleInfo *g_dcm224ModuleInfo = &g_dcm224PsuChannelModuleInfo;

#if defined(EEZ_PLATFORM_STM32)

float readTemperature(int channelIndex) {
	psu::Channel& channel = psu::Channel::get(channelIndex);
	auto slot = g_slots[channel.slotIndex];
	if (slot.moduleInfo->moduleType == MODULE_TYPE_DCM224) {
		// TODO this is temporary until module hardware is changed
		return 25.0f + (channel.isOutputEnabled() ? 5 * channel.i.set : 0.0f);
	} else {
		return ((DcmChannel&)channel).temperature;
	}
}

#endif

} // namespace dcm220
} // namespace eez
