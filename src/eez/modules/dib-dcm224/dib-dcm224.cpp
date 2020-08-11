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
#include <assert.h>

#if defined(EEZ_PLATFORM_STM32)
#include <main.h>
#include <crc.h>
#include <eez/platform/stm32/spi.h>
#include <memory.h>
#include <stdlib.h>
#endif

#include <eez/firmware.h>
#include <eez/system.h>
#include <eez/hmi.h>

#include <eez/scpi/regs.h>

#include <eez/gui/gui.h>

#include <eez/modules/psu/psu.h>
#include <eez/modules/psu/channel_dispatcher.h>
#include <eez/modules/psu/event_queue.h>
#include <eez/modules/psu/profile.h>
#include <eez/modules/psu/gui/psu.h>
#include <eez/modules/psu/gui/edit_mode.h>

#include <eez/modules/bp3c/comm.h>

#include <eez/modules/dib-dcm224/dib-dcm224.h>

#define CONF_MAX_ALLOWED_CONSECUTIVE_TRANSFER_ERRORS 10
#define CONF_TRANSFER_TIMEOUT_MS 1000

#define PWM_MIN_FREQUENCY 0.1f
#define PWM_MAX_FREQUENCY 10000.0f
#define PWM_DEF_FREQUENCY 10.0f

#define PWM_MIN_DUTY 0.0f
#define PWM_MAX_DUTY 100.0f
#define PWM_DEF_DUTY 100.0f

#define COUNTERPHASE_MIN_FREQUENCY 350000.f
#define COUNTERPHASE_MAX_FREQUENCY 600000.f
#define COUNTERPHASE_DEF_FREQUENCY 500000.f

using namespace eez::psu;
using namespace eez::psu::gui;
using namespace eez::gui;
using namespace eez::dcm224;
using namespace eez::hmi;

namespace eez {

namespace dcm224 {

static const uint16_t DAC_MIN = 0;
static const uint16_t DAC_MAX = 4095;

static const uint16_t ADC_MIN = 0;
static const uint16_t ADC_MAX = 65535;

#define REG0_OE1_MASK     (1 << 0)
#define REG0_OE2_MASK     (1 << 2)

#define COUNTERPHASE_DITHERING_MASK (1 << 0)

#define BUFFER_SIZE 30

static const float PTOT = 155.0f;

static const float I_MON_RESOLUTION = 0.02f;

#define REG0_CC1_MASK     (1 << 1)
#define REG0_CC2_MASK     (1 << 3)
#define REG0_PWRGOOD_MASK (1 << 4)

enum TransferResult {
    TRANSFER_OK,
    TRANSFER_NOT_READY,
    TRANSFER_ERROR,
    TRANSFER_TIMEOUT
};

struct DcmChannel : public Channel {
    bool outputEnable;

#if defined(EEZ_PLATFORM_STM32)
	uint16_t uSet;
	uint16_t iSet;
#endif

#if defined(EEZ_PLATFORM_SIMULATOR)
    float uMon;
    float iMon;
    float uSet;
    float iSet;
#endif

    float temperature = 25.0f;

	float I_MAX_FOR_REMAP;

	float U_CAL_POINTS[2];
	float I_CAL_POINTS[6];

	bool ccMode = false;

    float pwmFrequency = PWM_DEF_FREQUENCY;
    float pwmDuty = PWM_DEF_DUTY;

    DcmChannel(uint8_t slotIndex, uint8_t channelIndex, uint8_t subchannelIndex)
        : Channel(slotIndex, channelIndex, subchannelIndex)
    {
        channelHistory = new ChannelHistory(*this);
    }

	void getParams(uint16_t moduleRevision) override {

		params.U_MIN = 1.0f;
		params.U_DEF = 1.0f;
		params.U_MAX = 24.0f;

		params.U_MIN_STEP = 0.01f;
		params.U_DEF_STEP = 0.1f;
		params.U_MAX_STEP = 5.0f;

		U_CAL_POINTS[0] = 2.0f;
		U_CAL_POINTS[1] = 22.0f;
		params.U_CAL_NUM_POINTS = 2;
		params.U_CAL_POINTS = U_CAL_POINTS;
		params.U_CAL_I_SET = 1.0f;

		params.I_MIN = 0.0f;
		params.I_DEF = 0.0f;
		params.I_MAX = 4.9f;

    	params.I_MON_MIN = 0.0f;

		params.I_MIN_STEP = 0.01f;
		params.I_DEF_STEP = 0.01f;
		params.I_MAX_STEP = 1.0f; 

        I_CAL_POINTS[0] = 0.1f;
        I_CAL_POINTS[1] = 4.5f;
        params.I_CAL_NUM_POINTS = 2;
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

		params.OPP_DEFAULT_STATE = false;
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

		I_MAX_FOR_REMAP = 5.0f;

		params.U_RAMP_DURATION_MIN_VALUE = 0.002f;
	}

    void init() override;
    void onPowerDown() override;
    bool test() override;
    TestResult getTestResult() override;
    void tickSpecific(uint32_t tickCount) override;
	
	bool isInCcMode() override {
#if defined(EEZ_PLATFORM_STM32)
        return ccMode;
#endif

#if defined(EEZ_PLATFORM_SIMULATOR)
        return simulator::getCC(channelIndex);
#endif
	}

	bool isInCvMode() override {
		return !isInCcMode();
	}

	void adcMeasureUMon() override {
	}

	void adcMeasureIMon() override {
	}

	void adcMeasureMonDac() override {
	}

	void adcMeasureAll() override {
	}

	void setOutputEnable(bool enable, uint16_t tasks) override {
		outputEnable = enable;
	}

    void setDacVoltage(uint16_t value) override {

#if defined(EEZ_PLATFORM_STM32)
        value = (uint16_t)clamp((float)value, (float)DAC_MIN, (float)DAC_MAX);
        uSet = clamp(value, DAC_MIN, DAC_MAX);
#endif

#if defined(EEZ_PLATFORM_SIMULATOR)
        uSet = remap(clamp((float)value, (float)DAC_MIN, (float)DAC_MAX), (float)DAC_MIN, 0, (float)DAC_MAX, params.U_MAX);
#endif
	}

	void setDacVoltageFloat(float value) override {
#if defined(EEZ_PLATFORM_STM32)
        value = remap(value, 0, (float)DAC_MIN, params.U_MAX, (float)DAC_MAX);
        uSet = (uint16_t)clamp(round(value), DAC_MIN, DAC_MAX);
#endif

#if defined(EEZ_PLATFORM_SIMULATOR)
        uSet = value;
#endif
	}

	void setDacCurrent(uint16_t value) override {
#if defined(EEZ_PLATFORM_STM32)
        value = (uint16_t)clamp((float)value, (float)DAC_MIN, (float)DAC_MAX);
        iSet = value;
#endif

#if defined(EEZ_PLATFORM_SIMULATOR)
        uSet = remap(clamp((float)value, (float)DAC_MIN, (float)DAC_MAX), (float)DAC_MIN, params.I_MIN, (float)DAC_MAX, params.I_MAX);
#endif
    }

	void setDacCurrentFloat(float value) override {
#if defined(EEZ_PLATFORM_STM32)
        value = remap(value, /*params.I_MIN*/ 0, (float)DAC_MIN, /*params.I_MAX*/ I_MAX_FOR_REMAP, (float)DAC_MAX);
        iSet = (uint16_t)clamp(round(value), DAC_MIN, DAC_MAX);
#endif

#if defined(EEZ_PLATFORM_SIMULATOR)
        iSet = value;
#endif
	}

	bool isDacTesting() override {
		return false;
	}

    void getVoltageStepValues(StepValues *stepValues, bool calibrationMode) override {
        static float values[] = { 1.0f, 0.5f, 0.1f, 0.01f };
		static float calibrationModeValues[] = { 1.0f, 0.1f, 0.01f, 0.001f };
        stepValues->values = calibrationMode ? calibrationModeValues : values;
        stepValues->count = sizeof(values) / sizeof(float);
		stepValues->unit = UNIT_VOLT;
	}
    
	void getCurrentStepValues(StepValues *stepValues, bool calibrationMode) override {
        static float values[] = { 0.5f, 0.25f, 0.1f,  0.01f };
		static float calibrationModeValues[] = { 0.05f, 0.01f, 0.005f,  0.001f };
        stepValues->values = calibrationMode ? calibrationModeValues : values;
        stepValues->count = sizeof(values) / sizeof(float);
		stepValues->unit = UNIT_AMPER;
	}

    void getPowerStepValues(StepValues *stepValues) override {
        static float values[] = { 10.0f, 1.0f, 0.1f, 0.01f };
        stepValues->values = values;
        stepValues->count = sizeof(values) / sizeof(float);
		stepValues->unit = UNIT_WATT;
	}
	
	bool isPowerLimitExceeded(float u, float i, int *err) override {
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

    float readTemperature() override {
#if defined(EEZ_PLATFORM_STM32)
        return temperature;
#else
        return NAN;
#endif
    }

	int getAdvancedOptionsPageId() override {
		return PAGE_ID_DIB_DCM224_CH_SETTINGS_ADV_OPTIONS;
	}
};

struct DcmModuleInfo : public PsuModuleInfo {
public:
	DcmModuleInfo(uint16_t moduleType, const char *moduleName, uint16_t latestModuleRevision) 
		: PsuModuleInfo(moduleType, moduleName, "Envox", latestModuleRevision, FLASH_METHOD_STM32_BOOTLOADER_UART, 0,
#if defined(EEZ_PLATFORM_STM32)
            SPI_BAUDRATEPRESCALER_16,
            false,
#else
            0,
            false,
#endif
            2)
	{
	}

	Module *createModule(uint8_t slotIndex, uint16_t moduleRevision, bool firmwareInstalled) override;

	Channel *createChannel(int slotIndex, int channelIndex, int subchannelIndex) override {
        void *buffer = malloc(sizeof(DcmChannel));
        memset(buffer, 0, sizeof(DcmChannel));
		return new (buffer) DcmChannel(slotIndex, channelIndex, subchannelIndex);
	}

    struct DcmProfileParameters {
        ProfileParameters baseParameters;
        unsigned int dcmVersion;
        float pwmFrequency;
        float pwmDuty;
        float counterphaseFrequency;
        bool counterphaseDithering;
    };

    void getProfileParameters(int channelIndex, uint8_t *buffer) override;
    void setProfileParameters(int channelIndex, uint8_t *buffer, bool mismatch, int recallOptions, int &numTrackingChannels) override;
    bool writeProfileProperties(profile::WriteContext &ctx, const uint8_t *buffer) override;
    bool readProfileProperties(profile::ReadContext &ctx, uint8_t *buffer) override;
};

struct DcmModule : public PsuModule {
public:
    TestResult testResult = TEST_NONE;
    bool synchronized = false;
    uint8_t numConsecutiveTransferErrors;
    uint32_t lastTransferTickCount;
    uint8_t input[BUFFER_SIZE];
    uint8_t output[BUFFER_SIZE];
    float counterphaseFrequency = 500000.0f;
    bool counterphaseDithering = false;

    DcmModule(uint8_t slotIndex, DcmModuleInfo *moduleInfo, uint16_t moduleRevision, bool firmwareInstalled)
        : PsuModule(slotIndex, moduleInfo, moduleRevision, firmwareInstalled)
    {
    	memset(output, 0, sizeof(output));
    	memset(input, 0, sizeof(input));
    }

    void init() {
        if (!synchronized) {
            if (bp3c::comm::masterSynchroV2(slotIndex)) {
                //DebugTrace("DCM224 slot #%d firmware version %d.%d\n", slotIndex + 1, (int)firmwareMajorVersion, (int)firmwareMinorVersion);
                synchronized = true;
                lastTransferTickCount = millis();
                numConsecutiveTransferErrors = 0;
            } else {
                if (g_slots[slotIndex]->firmwareInstalled) {
                    event_queue::pushEvent(event_queue::EVENT_ERROR_SLOT1_SYNC_ERROR + slotIndex);
                }
            }
        }
    }

    void onPowerDown() {
        synchronized = false;
    }

    void test() {
        if (!synchronized) {
            testResult = TEST_FAILED;
            return;
        }

        bool successfulTransfer;

#if defined(EEZ_PLATFORM_STM32)
        output[0] = 0;
        while (true) {
            auto transferResult = transfer();
            if (transferResult == TRANSFER_OK) {
                successfulTransfer = true;
                break;
            }

            if (transferResult == TRANSFER_TIMEOUT) {
                successfulTransfer = false;
                break;
            }
        }
#endif

#if defined(EEZ_PLATFORM_SIMULATOR)
        successfulTransfer = true;
#endif

        bool pwrGood;
        if (successfulTransfer) {
#if defined(EEZ_PLATFORM_STM32)
            pwrGood = input[0] & REG0_PWRGOOD_MASK ? true : false;
#endif

#if defined(EEZ_PLATFORM_SIMULATOR)
            auto channelIndex = Channel::getBySlotIndex(slotIndex)->channelIndex;
            pwrGood = simulator::getPwrgood(channelIndex) && simulator::getPwrgood(channelIndex + 1);
#endif
        } else {
            pwrGood = true;
        }

        for (int subchannelIndex = 0; subchannelIndex < 2; subchannelIndex++) {
            auto &channel = *Channel::getBySlotIndex(slotIndex, subchannelIndex);
            channel.flags.powerOk = pwrGood ? 1 : 0;
        }

        testResult = pwrGood ? TEST_OK : TEST_FAILED;

        // test temp. sensors
		for (int subchannelIndex = 0; testResult == TEST_OK && subchannelIndex < 2; subchannelIndex++) {
			auto &channel = *Channel::getBySlotIndex(slotIndex, subchannelIndex);
			testResult = temp_sensor::sensors[temp_sensor::CH1 + channel.channelIndex].test() ? TEST_OK : TEST_FAILED;
		}
    }

#if defined(EEZ_PLATFORM_STM32)
    TransferResult transfer() {
        TransferResult result;
        
        if (HAL_GPIO_ReadPin(spi::IRQ_GPIO_Port[slotIndex], spi::IRQ_Pin[slotIndex]) == GPIO_PIN_SET) {
            auto status = bp3c::comm::transfer(slotIndex, output, input, BUFFER_SIZE);
            if (status == bp3c::comm::TRANSFER_STATUS_OK) {
                lastTransferTickCount = millis();
                numConsecutiveTransferErrors = 0;
                result = TRANSFER_OK;
            } else {
                // DebugTrace("Slot %d SPI transfer error %d\n", slotIndex + 1, status);
                result = TRANSFER_ERROR;
                numConsecutiveTransferErrors++;
            }
        } else {
            result = TRANSFER_NOT_READY;
        }

        if (result != TRANSFER_OK) {
            int32_t diff = millis() - lastTransferTickCount;
            if (diff > CONF_TRANSFER_TIMEOUT_MS || numConsecutiveTransferErrors > CONF_MAX_ALLOWED_CONSECUTIVE_TRANSFER_ERRORS) {
                event_queue::pushEvent(event_queue::EVENT_ERROR_SLOT1_CRC_CHECK_ERROR + slotIndex);
                synchronized = false;
                testResult = TEST_FAILED;
                result = TRANSFER_TIMEOUT;
            }
        }

        return result;
    }

    static float calcTemperature(uint16_t adcValue) {
        if (adcValue == 65535) {
            // not measured yet
            return 25.0f;
        }

        // http://www.giangrandi.ch/electronics/ntc/ntc.shtml

        float RF = 3300.0f;
        float T25 = 298.15F;
        float R25 = 10000;
        float BETA = 3570;
        float ADC_MAX_FOR_TEMP = 4095.0f;

        float RT = RF * (ADC_MAX_FOR_TEMP - adcValue) / adcValue;

        float Tkelvin = 1 / (logf(RT / R25) / BETA + 1 / T25);
        float Tcelsius = Tkelvin - 273.15f;

        float TEMP_OFFSET = 10.0f; // empirically determined
        Tcelsius -= TEMP_OFFSET;

        return roundPrec(Tcelsius, 1.0f);
    }

    void tick(uint8_t slotIndex) {
        DcmChannel &channel1 = (DcmChannel &)*Channel::getBySlotIndex(slotIndex, 0);
        DcmChannel &channel2 = (DcmChannel &)*Channel::getBySlotIndex(slotIndex, 1);

        output[0] = 0x80 | (channel1.outputEnable ? REG0_OE1_MASK : 0) | (channel2.outputEnable ? REG0_OE2_MASK : 0);

        output[1] = (counterphaseDithering ? COUNTERPHASE_DITHERING_MASK : 0);

        uint16_t *outputSetValues = (uint16_t *)(output + 2);
        outputSetValues[0] = channel1.uSet;
        outputSetValues[1] = channel1.iSet;
        outputSetValues[2] = channel2.uSet;
        outputSetValues[3] = channel2.iSet;

        float *floatValues = (float *)(outputSetValues + 4);
        floatValues[0] = channel1.pwmFrequency;
        floatValues[1] = channel1.pwmDuty;
        floatValues[2] = channel2.pwmFrequency;
        floatValues[3] = channel2.pwmDuty;
        floatValues[4] = counterphaseFrequency;

#ifdef DEBUG
        psu::debug::g_uDac[channel1.channelIndex].set(channel1.uSet);
        psu::debug::g_iDac[channel1.channelIndex].set(channel1.iSet);
        psu::debug::g_uDac[channel2.channelIndex].set(channel2.uSet);
        psu::debug::g_iDac[channel2.channelIndex].set(channel2.iSet);
#endif

        auto tranferResult = transfer();

        if (tranferResult == TRANSFER_OK) {
            uint16_t *inputSetValues = (uint16_t *)(input + 2);

            for (int subchannelIndex = 0; subchannelIndex < 2; subchannelIndex++) {
                auto &channel = *(DcmChannel *)Channel::getBySlotIndex(slotIndex, subchannelIndex);
                int offset = subchannelIndex * 2;

                channel.ccMode = (input[0] & (subchannelIndex == 0 ? REG0_CC1_MASK : REG0_CC2_MASK)) != 0;

                uint16_t uMonAdc = inputSetValues[offset];
                float uMon = remap(uMonAdc, (float)ADC_MIN, 0, (float)ADC_MAX, channel.params.U_MAX);
                channel.onAdcData(ADC_DATA_TYPE_U_MON, uMon);

                uint16_t iMonAdc = inputSetValues[offset + 1];
                const float FULL_SCALE = 2.0F;
                const float U_REF = 2.5F;
                float iMon = remap(iMonAdc, (float)ADC_MIN, 0, FULL_SCALE * ADC_MAX / U_REF, /*params.I_MAX*/ channel.I_MAX_FOR_REMAP);
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

                channel.temperature = calcTemperature(*((uint16_t *)(input + 10 + subchannelIndex * 2)));

#ifdef DEBUG
                psu::debug::g_uMon[channel.channelIndex].set(uMonAdc);
                psu::debug::g_iMon[channel.channelIndex].set(iMonAdc);
#endif
            }
        }
    }
#endif

    Page *getPageFromId(int pageId) override;
};

Module *DcmModuleInfo::createModule(uint8_t slotIndex, uint16_t moduleRevision, bool firmwareInstalled) {
	return new DcmModule(slotIndex, this, moduleRevision, firmwareInstalled);
}

void DcmChannel::init() {
    if (subchannelIndex == 0) {
        ((DcmModule *)g_slots[slotIndex])->init();
    }
}

void DcmChannel::onPowerDown() {
    Channel::onPowerDown();
    if (subchannelIndex == 0) {
        ((DcmModule *)g_slots[slotIndex])->onPowerDown();
    }
}

bool DcmChannel::test() {
    if (subchannelIndex == 0) {
        ((DcmModule *)g_slots[slotIndex])->test();
    }
    return isOk();
}

TestResult DcmChannel::getTestResult() {
    return ((DcmModule *)g_slots[slotIndex])->testResult;
}

void DcmChannel::tickSpecific(uint32_t tickCount) {
#if defined(EEZ_PLATFORM_STM32)
    if (subchannelIndex == 0) {
        ((DcmModule *)g_slots[slotIndex])->tick(slotIndex);
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

void DcmModuleInfo::getProfileParameters(int channelIndex, uint8_t *buffer) {
    assert(sizeof(DcmProfileParameters) < MAX_CHANNEL_PARAMETERS_SIZE);

    PsuModuleInfo::getProfileParameters(channelIndex, buffer);

    auto &channel = (DcmChannel &)Channel::get(channelIndex);
    auto parameters = (DcmProfileParameters *)buffer;

    parameters->pwmFrequency = channel.pwmFrequency;
    parameters->pwmDuty = channel.pwmDuty;

    auto module = (DcmModule *)g_slots[channel.slotIndex];
    parameters->counterphaseFrequency = module->counterphaseFrequency;
    parameters->counterphaseDithering = module->counterphaseDithering;
}

void DcmModuleInfo::setProfileParameters(int channelIndex, uint8_t *buffer, bool mismatch, int recallOptions, int &numTrackingChannels) {
    PsuModuleInfo::setProfileParameters(channelIndex, buffer, mismatch, recallOptions, numTrackingChannels);

    auto &channel = (DcmChannel &)Channel::get(channelIndex);
    auto parameters = (DcmProfileParameters *)buffer;

    if (parameters->dcmVersion > 0) {
        channel.pwmFrequency = parameters->pwmFrequency;
        channel.pwmDuty = parameters->pwmDuty;

        if (channel.subchannelIndex == 0) {
            auto module = (DcmModule *)g_slots[channel.slotIndex];
            module->counterphaseFrequency = parameters->counterphaseFrequency;
            module->counterphaseDithering = parameters->counterphaseDithering;
        }
    }
}

bool DcmModuleInfo::writeProfileProperties(profile::WriteContext &ctx, const uint8_t *buffer) {
    if (!PsuModuleInfo::writeProfileProperties(ctx, buffer)) {
        return false;
    }

    auto parameters = (DcmProfileParameters *)buffer;

    WRITE_PROPERTY("dcmVersion", 1);
    WRITE_PROPERTY("pwmFrequency", parameters->pwmFrequency);
    WRITE_PROPERTY("pwmDuty", parameters->pwmDuty);
    WRITE_PROPERTY("counterphaseFrequency", parameters->counterphaseFrequency);
    WRITE_PROPERTY("counterphaseDithering", parameters->counterphaseDithering);

    return true;
}

bool DcmModuleInfo::readProfileProperties(profile::ReadContext &ctx, uint8_t *buffer) {
    if (PsuModuleInfo::readProfileProperties(ctx, buffer)) {
        return true;
    }

    auto parameters = (DcmProfileParameters *)buffer;

    READ_PROPERTY(dcmVersion, parameters->dcmVersion);
    READ_PROPERTY(pwmFrequency, parameters->pwmFrequency);
    READ_PROPERTY(pwmDuty, parameters->pwmDuty);
    READ_PROPERTY(counterphaseFrequency, parameters->counterphaseFrequency);
    READ_PROPERTY(counterphaseDithering, parameters->counterphaseDithering);

    return false;
}

static DcmModuleInfo g_moduleInfo_(MODULE_TYPE_DCM224, "DCM224", MODULE_REVISION_DCM224_R3B2);

ModuleInfo *g_moduleInfo = &g_moduleInfo_;

class ChSettingsAdvOptionsPage : public SetPage {
public:
    void pageAlloc() {
        auto channel = (DcmChannel *)g_channel;
        m_pwmFrequency = m_pwmFrequencyOrig = channel->pwmFrequency;
        m_pwmDuty = m_pwmDutyOrig = channel->pwmDuty;

        auto module = (DcmModule *)g_slots[g_selectedSlotIndex];
        m_counterphaseFrequency = m_counterphaseFrequencyOrig = module->counterphaseFrequency;
        m_counterphaseDithering = m_counterphaseDitheringOrig = module->counterphaseDithering;
    }

    int getDirty() { 
        return m_pwmFrequency != m_pwmFrequencyOrig || 
            m_pwmDuty != m_pwmDutyOrig || 
            m_counterphaseFrequency != m_counterphaseFrequencyOrig || 
            m_counterphaseDithering != m_counterphaseDitheringOrig;
    }

    void set() {
        if (getDirty()) {
            auto channel = (DcmChannel *)g_channel;
            channel->pwmFrequency = m_pwmFrequency;
            channel->pwmDuty = m_pwmDuty;
        
            auto module = (DcmModule *)g_slots[g_selectedSlotIndex];
            module->counterphaseFrequency = m_counterphaseFrequency;
            module->counterphaseDithering = m_counterphaseDithering;
        }

        popPage();
    }

    float m_pwmFrequency;
    float m_pwmDuty;
    float m_counterphaseFrequency;
    bool m_counterphaseDithering;

private:
    float m_pwmFrequencyOrig;
    float m_pwmDutyOrig;
    float m_counterphaseFrequencyOrig;
    bool m_counterphaseDitheringOrig;
};

static ChSettingsAdvOptionsPage g_ChSettingsAdvOptionsPage;

Page *DcmModule::getPageFromId(int pageId) {
    if (pageId == PAGE_ID_DIB_DCM224_CH_SETTINGS_ADV_OPTIONS) {
        return &g_ChSettingsAdvOptionsPage;
    }
    return nullptr;
}

} // namespace dcm224

namespace gui {

void data_dib_dcm224_pwm_frequency(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        bool focused = g_focusCursor == cursor && g_focusDataId == DATA_ID_DIB_DCM224_PWM_FREQUENCY;
        if (focused && g_focusEditValue.getType() != VALUE_TYPE_NONE) {
            value = g_focusEditValue;
        } else if (focused && getActivePageId() == PAGE_ID_EDIT_MODE_KEYPAD && edit_mode_keypad::g_keypad->isEditing()) {
            data_keypad_text(operation, cursor, value);
        } else {
            ChSettingsAdvOptionsPage *page = (ChSettingsAdvOptionsPage *)getPage(PAGE_ID_DIB_DCM224_CH_SETTINGS_ADV_OPTIONS);
            if (page) {
                value = MakeValue(page->m_pwmFrequency, UNIT_HERTZ);
            } else {
                DcmChannel *channel = (DcmChannel *)g_channel;
                value = MakeValue(channel->pwmFrequency, UNIT_HERTZ);
            }
        }
    } else if (operation == DATA_OPERATION_GET_ALLOW_ZERO) {
        value = 0;
    } else if (operation == DATA_OPERATION_GET_MIN) {
        value = MakeValue(PWM_MIN_FREQUENCY, UNIT_HERTZ);
    } else if (operation == DATA_OPERATION_GET_MAX) {
        value = MakeValue(PWM_MAX_FREQUENCY, UNIT_HERTZ);
    } else if (operation == DATA_OPERATION_GET_DEF) {
        value = MakeValue(PWM_DEF_FREQUENCY, UNIT_HERTZ);
    } else if (operation == DATA_OPERATION_GET_NAME) {
        value = "PWM frequency";
    } else if (operation == DATA_OPERATION_GET_UNIT) {
        value = UNIT_HERTZ;
    } else if (operation == DATA_OPERATION_SET) {
        ChSettingsAdvOptionsPage *page = (ChSettingsAdvOptionsPage *)getPage(PAGE_ID_DIB_DCM224_CH_SETTINGS_ADV_OPTIONS);
        if (page) {
            page->m_pwmFrequency = value.getFloat();
        } else {
            DcmChannel *channel = (DcmChannel *)g_channel;
            channel->pwmFrequency = value.getFloat();
        }
    } else if (operation == DATA_OPERATION_GET_ENCODER_STEP) {
        float fvalue;

        ChSettingsAdvOptionsPage *page = (ChSettingsAdvOptionsPage *)getPage(PAGE_ID_DIB_DCM224_CH_SETTINGS_ADV_OPTIONS);
        if (page && page->getDirty()) {
            fvalue = page->m_pwmFrequency;
        } else {
            DcmChannel *channel = (DcmChannel *)g_channel;
            fvalue = channel->pwmFrequency;
        }

        value = Value(MAX(powf(10.0f, floorf(log10f(fabsf(fvalue))) - 1), 0.001f), UNIT_HERTZ);
    } else if (operation == DATA_OPERATION_GET_ENCODER_STEP_VALUES) {
        static float values[] = { 1000.0f, 500.0f, 100.0f, 1.0f };
        StepValues *stepValues = value.getStepValues();
        stepValues->values = values;
        stepValues->count = sizeof(values) / sizeof(float);
        stepValues->unit = UNIT_HERTZ;
        value = 1;
    }
}

void data_dib_dcm224_pwm_duty(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        bool focused = g_focusCursor == cursor && g_focusDataId == DATA_ID_IO_PIN_PWM_DUTY;
        if (focused && g_focusEditValue.getType() != VALUE_TYPE_NONE) {
            value = g_focusEditValue;
        } else if (focused && getActivePageId() == PAGE_ID_EDIT_MODE_KEYPAD && edit_mode_keypad::g_keypad->isEditing()) {
            data_keypad_text(operation, cursor, value);
        } else {
            ChSettingsAdvOptionsPage *page = (ChSettingsAdvOptionsPage *)getPage(PAGE_ID_DIB_DCM224_CH_SETTINGS_ADV_OPTIONS);
            if (page) {
                value = MakeValue(page->m_pwmDuty, UNIT_PERCENT);
            } else {
                auto channel = (DcmChannel *)g_channel;
                value = MakeValue(channel->pwmDuty, UNIT_PERCENT);
            }
        }
    } else if (operation == DATA_OPERATION_GET_MIN) {
        value = MakeValue(PWM_MIN_DUTY, UNIT_PERCENT);
    } else if (operation == DATA_OPERATION_GET_MAX) {
        value = MakeValue(PWM_MAX_DUTY, UNIT_PERCENT);
    } else if (operation == DATA_OPERATION_GET_DEF) {
        value = MakeValue(PWM_DEF_DUTY, UNIT_PERCENT);
    } else if (operation == DATA_OPERATION_GET_NAME) {
        value = "PWM duty cycle";
    } else if (operation == DATA_OPERATION_GET_UNIT) {
        value = UNIT_PERCENT;
    } else if (operation == DATA_OPERATION_SET) {
        ChSettingsAdvOptionsPage *page = (ChSettingsAdvOptionsPage *)getPage(PAGE_ID_DIB_DCM224_CH_SETTINGS_ADV_OPTIONS);
        if (page) {
            page->m_pwmDuty = value.getFloat();
        } else {
            auto channel = (DcmChannel *)g_channel;
            channel->pwmDuty = value.getFloat();
        }
    } else if (operation == DATA_OPERATION_GET_ENCODER_STEP) {
        value = Value(1.0f, UNIT_PERCENT);
    } else if (operation == DATA_OPERATION_GET_ENCODER_STEP_VALUES) {
        static float values[] = { 5.0f, 1.0f, 0.5f, 0.1f };
        StepValues *stepValues = value.getStepValues();
        stepValues->values = values;
        stepValues->count = sizeof(values) / sizeof(float);
        stepValues->unit = UNIT_PERCENT;
        value = 1;
    }
}

void data_dib_dcm224_counterphase_frequency(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        bool focused = g_focusCursor == cursor && g_focusDataId == DATA_ID_DIB_DCM224_COUNTERPHASE_FREQUENCY;
        if (focused && g_focusEditValue.getType() != VALUE_TYPE_NONE) {
            value = g_focusEditValue;
        } else if (focused && getActivePageId() == PAGE_ID_EDIT_MODE_KEYPAD && edit_mode_keypad::g_keypad->isEditing()) {
            data_keypad_text(operation, cursor, value);
        } else {
            ChSettingsAdvOptionsPage *page = (ChSettingsAdvOptionsPage *)getPage(PAGE_ID_DIB_DCM224_CH_SETTINGS_ADV_OPTIONS);
            if (page) {
                value = MakeValue(page->m_counterphaseFrequency, UNIT_HERTZ);
            } else {
                auto module = (DcmModule *)g_slots[g_selectedSlotIndex];
                value = MakeValue(module->counterphaseFrequency, UNIT_HERTZ);
            }
        }
    } else if (operation == DATA_OPERATION_GET_ALLOW_ZERO) {
        value = 0;
    } else if (operation == DATA_OPERATION_GET_MIN) {
        value = MakeValue(COUNTERPHASE_MIN_FREQUENCY, UNIT_HERTZ);
    } else if (operation == DATA_OPERATION_GET_MAX) {
        value = MakeValue(COUNTERPHASE_MAX_FREQUENCY, UNIT_HERTZ);
    } else if (operation == DATA_OPERATION_GET_DEF) {
        value = MakeValue(COUNTERPHASE_DEF_FREQUENCY, UNIT_HERTZ);
    } else if (operation == DATA_OPERATION_GET_NAME) {
        value = "Counterph. freq.";
    } else if (operation == DATA_OPERATION_GET_UNIT) {
        value = UNIT_HERTZ;
    } else if (operation == DATA_OPERATION_SET) {
        ChSettingsAdvOptionsPage *page = (ChSettingsAdvOptionsPage *)getPage(PAGE_ID_DIB_DCM224_CH_SETTINGS_ADV_OPTIONS);
        if (page) {
            page->m_counterphaseFrequency = value.getFloat();
        } else {
            auto module = (DcmModule *)g_slots[g_selectedSlotIndex];
            module->counterphaseFrequency = value.getFloat();
        }
    } else if (operation == DATA_OPERATION_GET_ENCODER_STEP) {
        float fvalue;

        ChSettingsAdvOptionsPage *page = (ChSettingsAdvOptionsPage *)getPage(PAGE_ID_DIB_DCM224_CH_SETTINGS_ADV_OPTIONS);
        if (page) {
            fvalue = page->m_counterphaseFrequency;
        } else {
            auto module = (DcmModule *)g_slots[g_selectedSlotIndex];
            fvalue = module->counterphaseFrequency;
        }

        value = Value(MAX(powf(10.0f, floorf(log10f(fabsf(fvalue))) - 1), 0.001f), UNIT_HERTZ);
    } else if (operation == DATA_OPERATION_GET_ENCODER_STEP_VALUES) {
        static float values[] = { 10000.0f, 1000.0f, 100.0f, 1.0f };
        StepValues *stepValues = value.getStepValues();
        stepValues->values = values;
        stepValues->count = sizeof(values) / sizeof(float);
        stepValues->unit = UNIT_HERTZ;
        value = 1;
    }
}

void data_dib_dcm224_counterphase_dithering(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        ChSettingsAdvOptionsPage *page = (ChSettingsAdvOptionsPage *)getPage(PAGE_ID_DIB_DCM224_CH_SETTINGS_ADV_OPTIONS);
        if (page && page->getDirty()) {
            value = page->m_counterphaseDithering ? 1 : 0;
        } else {
            auto module = (DcmModule *)g_slots[g_selectedSlotIndex];
            value = module->counterphaseDithering ? 1 : 0;
        }
    }
}

void action_dib_dcm224_ch_settings_adv_toggle_counterphase_dithering() {
    ChSettingsAdvOptionsPage *page = (ChSettingsAdvOptionsPage *)getPage(PAGE_ID_DIB_DCM224_CH_SETTINGS_ADV_OPTIONS);
    page->m_counterphaseDithering = !page->m_counterphaseDithering;
}

} // namespace gui

} // namespace eez
