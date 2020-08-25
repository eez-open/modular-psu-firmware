/*
 * EEZ DIB MIO168
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

#include <new>

#include <assert.h>
#include <stdlib.h>
#include <memory.h>

#if defined(EEZ_PLATFORM_STM32)
#include <spi.h>
#include <eez/platform/stm32/spi.h>
#endif

#include <eez/debug.h>
#include <eez/firmware.h>
#include <eez/index.h>
#include <eez/hmi.h>
#include <eez/gui/gui.h>
#include <eez/modules/psu/psu.h>
#include <eez/modules/psu/event_queue.h>
#include <eez/modules/psu/gui/psu.h>
#include <eez/modules/bp3c/comm.h>
#include <eez/modules/bp3c/flash_slave.h>

#include "./dib-mio168.h"

using namespace eez::psu;
using namespace eez::psu::gui;
using namespace eez::gui;

namespace eez {
namespace dib_mio168 {

static const int DIN_SUBCHANNEL_INDEX = 0;
static const int DOUT_SUBCHANNEL_INDEX = 1;
static const int ADC_1_SUBCHANNEL_INDEX = 2;
static const int ADC_2_SUBCHANNEL_INDEX = 3;
static const int ADC_3_SUBCHANNEL_INDEX = 4;
static const int ADC_4_SUBCHANNEL_INDEX = 5;
static const int DAC_7760_1_SUBCHANNEL_INDEX = 6;
static const int DAC_7760_2_SUBCHANNEL_INDEX = 7;
static const int DAC_7563_1_SUBCHANNEL_INDEX = 8;
static const int DAC_7563_2_SUBCHANNEL_INDEX = 9;
static const int PWM_1_SUBCHANNEL_INDEX = 10;
static const int PWM_2_SUBCHANNEL_INDEX = 11;

struct MioChannel : public Channel {
    MioChannel(uint8_t slotIndex, uint8_t channelIndex, uint8_t subchannelIndex)
        : Channel(slotIndex, channelIndex, subchannelIndex)
    {
        flags.powerOk = 1;
    }
    
    TestResult getTestResult() override;

    int getChannelSettingsPageId() override {
		return PAGE_ID_SLOT_SETTINGS;
	}
};

struct Mio168DinChannel : public MioChannel {
    uint8_t pinStates = 0;

    Mio168DinChannel(int slotIndex, int channelIndex)
        : MioChannel(slotIndex, channelIndex, DIN_SUBCHANNEL_INDEX)
    {
    }

    void getParams(uint16_t moduleRevision) override {
        params.features = CH_FEATURE_DINPUT;
    }

    uint8_t getDigitalInputData() override {
        return pinStates;
    }

    int getPinState(int pin) {
        return pinStates & (1 << pin) ? 1 : 0;
    }

    // defaults
    virtual void init() override {}
    bool test() override { return true;  }
    void tickSpecific(uint32_t tickCount) override {}
    bool isInCcMode() override { return false;  }
    bool isInCvMode() override { return false;  }
    void adcMeasureUMon() override {}
    void adcMeasureIMon() override {}
    void adcMeasureMonDac() override {}
    void adcMeasureAll() override {}
    void setOutputEnable(bool enable, uint16_t tasks) override {}
    void setDacVoltage(uint16_t value) override {}
    void setDacVoltageFloat(float value) override {}
    void setDacCurrent(uint16_t value) override {}
    void setDacCurrentFloat(float value) override {}
    bool isDacTesting() override { return false;  }
    void getVoltageStepValues(StepValues *stepValues, bool calibrationMode) override {}
    void getCurrentStepValues(StepValues *stepValues, bool calibrationMode) override {}
    void getPowerStepValues(StepValues *stepValues) override {}
    bool isPowerLimitExceeded(float u, float i, int *err) override { return false; }
    float readTemperature() override { return 25.0f; }
};

struct Mio168DoutChannel : public MioChannel {
    uint8_t pinStates = 0;

    Mio168DoutChannel(int slotIndex, int channelIndex)
        : MioChannel(slotIndex, channelIndex, DOUT_SUBCHANNEL_INDEX)
    {
    }

    void getParams(uint16_t moduleRevision) override {
        params.features = CH_FEATURE_DOUTPUT;
    }

    uint8_t getDigitalOutputData() override {
        return pinStates;
    }

    void setDigitalOutputData(uint8_t data) override {
        pinStates = data;
    }

    int getPinState(int pin) {
        return pinStates & (1 << pin) ? 1 : 0;
    }

    void setPinState(int pin, int state) {
        if (state) {
            pinStates |= 1 << pin;
        } else {
            pinStates &= ~(1 << pin);
        }
    }

    // defaults
    virtual void init() override {}
    bool test() override { return true; }
    void tickSpecific(uint32_t tickCount) override {}
    bool isInCcMode() override { return false; }
    bool isInCvMode() override { return false; }
    void adcMeasureUMon() override {}
    void adcMeasureIMon() override {}
    void adcMeasureMonDac() override {}
    void adcMeasureAll() override {}
    void setOutputEnable(bool enable, uint16_t tasks) override {}
    void setDacVoltage(uint16_t value) override {}
    void setDacVoltageFloat(float value) override {}
    void setDacCurrent(uint16_t value) override {}
    void setDacCurrentFloat(float value) override {}
    bool isDacTesting() override { return false; }
    void getVoltageStepValues(StepValues *stepValues, bool calibrationMode) override {}
    void getCurrentStepValues(StepValues *stepValues, bool calibrationMode) override {}
    void getPowerStepValues(StepValues *stepValues) override {}
    bool isPowerLimitExceeded(float u, float i, int *err) override { return false; }
    float readTemperature() override { return 25.0f; }
};

struct Mio168AdcChannel : public MioChannel {
    Mio168AdcChannel(int slotIndex, int channelIndex, int subchannelIndex)
        : MioChannel(slotIndex, channelIndex, subchannelIndex)
    {
    }

    void getParams(uint16_t moduleRevision) override {
        params.features = 0;
    }

    // defaults
    virtual void init() override {}
    bool test() override { return true; }
    void tickSpecific(uint32_t tickCount) override {}
    bool isInCcMode() override { return false; }
    bool isInCvMode() override { return false; }
    void adcMeasureUMon() override {}
    void adcMeasureIMon() override {}
    void adcMeasureMonDac() override {}
    void adcMeasureAll() override {}
    void setOutputEnable(bool enable, uint16_t tasks) override {}
    void setDacVoltage(uint16_t value) override {}
    void setDacVoltageFloat(float value) override {}
    void setDacCurrent(uint16_t value) override {}
    void setDacCurrentFloat(float value) override {}
    bool isDacTesting() override { return false; }
    void getVoltageStepValues(StepValues *stepValues, bool calibrationMode) override {}
    void getCurrentStepValues(StepValues *stepValues, bool calibrationMode) override {}
    void getPowerStepValues(StepValues *stepValues) override {}
    bool isPowerLimitExceeded(float u, float i, int *err) override { return false; }
    float readTemperature() override { return 25.0f; }
};

struct Mio168Dac7760Channel : public MioChannel {
    SourceMode m_mode;
    int8_t m_currentRange;
    int8_t m_voltageRange;

    Mio168Dac7760Channel(int slotIndex, int channelIndex, int subchannelIndex)
        : MioChannel(slotIndex, channelIndex, subchannelIndex)
    {
    }

    void getParams(uint16_t moduleRevision) override {
		params.U_MIN = 0.0f;
		params.U_DEF = 0.0f;
		params.U_MAX = 10.5f;

		params.U_MIN_STEP = 0.01f;
		params.U_DEF_STEP = 0.1f;
		params.U_MAX_STEP = 5.0f;

		params.I_MIN = 0.0f;
		params.I_DEF = 0.0f;
		params.I_MAX = 0.024f;

	
		params.I_MIN_STEP = 0.001f;
		params.I_DEF_STEP = 0.001f;
		params.I_MAX_STEP = 0.01f; 
		
		params.U_RESOLUTION = 0.005f;
		params.I_RESOLUTION = 0.0005f;

		params.VOLTAGE_GND_OFFSET = 0;
		params.CURRENT_GND_OFFSET = 0;

		params.features = CH_FEATURE_VOLT | CH_FEATURE_CURRENT | CH_FEATURE_OE;
    }

    bool getMode(SourceMode &mode, int *err) override {
        mode = m_mode;
        return true;
    }

    bool setMode(SourceMode mode, int *err) override {
        m_mode = mode;
        return true;
    }

    bool getCurrentRange(int8_t &range, int *err) override {
        range = m_currentRange;
        return true;
    }
    
    bool setCurrentRange(int8_t range, int *err) override {
        m_currentRange = range;
        return true;
    }

    bool getVoltageRange(int8_t &range, int *err) override {
        range = m_voltageRange;
        return true;
    }
    
    bool setVoltageRange(int8_t range, int *err) override {
        m_voltageRange = range;
        return true;
    }  

    // defaults
    virtual void init() override {}
    bool test() override { return true; }
    void tickSpecific(uint32_t tickCount) override {}
    bool isInCcMode() override { return false; }
    bool isInCvMode() override { return false; }
    void adcMeasureUMon() override {}
    void adcMeasureIMon() override {}
    void adcMeasureMonDac() override {}
    void adcMeasureAll() override {}
    void setOutputEnable(bool enable, uint16_t tasks) override {}
    void setDacVoltage(uint16_t value) override {}
    void setDacVoltageFloat(float value) override {}
    void setDacCurrent(uint16_t value) override {}
    void setDacCurrentFloat(float value) override {}
    bool isDacTesting() override { return false; }
    void getVoltageStepValues(StepValues *stepValues, bool calibrationMode) override {}
    void getCurrentStepValues(StepValues *stepValues, bool calibrationMode) override {}
    void getPowerStepValues(StepValues *stepValues) override {}
    bool isPowerLimitExceeded(float u, float i, int *err) override { return false; }
    float readTemperature() override { return 25.0f; }
};

struct Mio168Dac7563Channel : public MioChannel {
    Mio168Dac7563Channel(int slotIndex, int channelIndex, int subchannelIndex)
        : MioChannel(slotIndex, channelIndex, subchannelIndex)
    {
    }

    void getParams(uint16_t moduleRevision) override {
        params.features = 0;
    }

    // defaults
    virtual void init() override {}
    bool test() override { return true; }
    void tickSpecific(uint32_t tickCount) override {}
    bool isInCcMode() override { return false; }
    bool isInCvMode() override { return false; }
    void adcMeasureUMon() override {}
    void adcMeasureIMon() override {}
    void adcMeasureMonDac() override {}
    void adcMeasureAll() override {}
    void setOutputEnable(bool enable, uint16_t tasks) override {}
    void setDacVoltage(uint16_t value) override {}
    void setDacVoltageFloat(float value) override {}
    void setDacCurrent(uint16_t value) override {}
    void setDacCurrentFloat(float value) override {}
    bool isDacTesting() override { return false; }
    void getVoltageStepValues(StepValues *stepValues, bool calibrationMode) override {}
    void getCurrentStepValues(StepValues *stepValues, bool calibrationMode) override {}
    void getPowerStepValues(StepValues *stepValues) override {}
    bool isPowerLimitExceeded(float u, float i, int *err) override { return false; }
    float readTemperature() override { return 25.0f; }
};

struct Mio168PwmChannel : public MioChannel {
    Mio168PwmChannel(int slotIndex, int channelIndex, int subchannelIndex)
        : MioChannel(slotIndex, channelIndex, subchannelIndex)
    {
    }

    void getParams(uint16_t moduleRevision) override {
        params.features = 0;
    }

    // defaults
    virtual void init() override {}
    bool test() override { return true; }
    void tickSpecific(uint32_t tickCount) override {}
    bool isInCcMode() override { return false; }
    bool isInCvMode() override { return false; }
    void adcMeasureUMon() override {}
    void adcMeasureIMon() override {}
    void adcMeasureMonDac() override {}
    void adcMeasureAll() override {}
    void setOutputEnable(bool enable, uint16_t tasks) override {}
    void setDacVoltage(uint16_t value) override {}
    void setDacVoltageFloat(float value) override {}
    void setDacCurrent(uint16_t value) override {}
    void setDacCurrentFloat(float value) override {}
    bool isDacTesting() override { return false; }
    void getVoltageStepValues(StepValues *stepValues, bool calibrationMode) override {}
    void getCurrentStepValues(StepValues *stepValues, bool calibrationMode) override {}
    void getPowerStepValues(StepValues *stepValues) override {}
    bool isPowerLimitExceeded(float u, float i, int *err) override { return false; }
    float readTemperature() override { return 25.0f; }
};

struct Mio168ModuleInfo : public ModuleInfo {
public:
    Mio168ModuleInfo() 
        : ModuleInfo(MODULE_TYPE_DIB_MIO168, "MIO168", "Envox", MODULE_REVISION_R1B2, FLASH_METHOD_STM32_BOOTLOADER_SPI, 10000,
#if defined(EEZ_PLATFORM_STM32)
            SPI_BAUDRATEPRESCALER_4,
            true,
#else
            0,
            false,
#endif
            12
        )
    {}

    Module *createModule(uint8_t slotIndex, uint16_t moduleRevision, bool firmwareInstalled) override;
    
    Channel *createChannel(int slotIndex, int channelIndex, int subchannelIndex) override;

    int getSlotView(SlotViewType slotViewType, int slotIndex, int cursor) override {
        if (slotViewType == SLOT_VIEW_TYPE_DEFAULT) {
            return isDefaultViewVertical() ? PAGE_ID_DIB_MIO168_SLOT_VIEW_DEF : PAGE_ID_SLOT_DEF_HORZ_EMPTY;
        }
        if (slotViewType == SLOT_VIEW_TYPE_DEFAULT_2COL) {
            return isDefaultViewVertical() ? PAGE_ID_DIB_MIO168_SLOT_VIEW_DEF_2COL : PAGE_ID_SLOT_DEF_HORZ_EMPTY_2COL;
        }
        if (slotViewType == SLOT_VIEW_TYPE_MAX) {
            return PAGE_ID_DIB_MIO168_SLOT_VIEW_MAX;
        }
        if (slotViewType == SLOT_VIEW_TYPE_MIN) {
            return PAGE_ID_DIB_MIO168_SLOT_VIEW_MIN;
        }
        assert(slotViewType == SLOT_VIEW_TYPE_MICRO);
        return PAGE_ID_DIB_MIO168_SLOT_VIEW_MICRO;
    }
};

#define BUFFER_SIZE 1024

struct Mio168Module : public Module {
public:
    TestResult testResult = TEST_NONE;
    bool synchronized = false;
    int numCrcErrors = 0;
    uint8_t input[BUFFER_SIZE];
    uint8_t output[BUFFER_SIZE];
    uint16_t analogInputValues[4];
    bool spiReady;

    Mio168Module(uint8_t slotIndex, ModuleInfo *moduleInfo, uint16_t moduleRevision, bool firmwareInstalled)
        : Module(slotIndex, moduleInfo, moduleRevision, firmwareInstalled)
    {
        memset(input, 0, sizeof(input));
        memset(output, 0, sizeof(output));
    }

    TestResult getTestResult() override {
        return testResult;
    }

    void initChannels() override {
        if (!synchronized) {
            if (bp3c::comm::masterSynchroV2(slotIndex)) {
                synchronized = true;
                numCrcErrors = 0;
                testResult = TEST_OK;
            } else {
                if (g_slots[slotIndex]->firmwareInstalled) {
                    event_queue::pushEvent(event_queue::EVENT_ERROR_SLOT1_SYNC_ERROR + slotIndex);
                }
                testResult = TEST_FAILED;
            }
        }
    }

    void tick() override {
        if (!synchronized) {
            return;
        }

        // static int cnt = 0;
        // if (++cnt < 2) {
        //     return;
        // }
        // cnt = 0;

#if defined(EEZ_PLATFORM_STM32)
        if (spiReady) {
            spiReady = false;
            transfer();
        }
#endif
    }

#if defined(EEZ_PLATFORM_STM32)
    void onSpiIrq() {
        spiReady = true;
    }
#endif

    void transfer() {
        struct Dac7760 {
            SourceMode mode;
            int8_t currentRange;
            float current;
            int8_t voltageRange;
            float voltage;
        };
        struct TypedOutput {
            uint8_t inputPinStates;
            uint8_t outputPinStates;
            Dac7760 dac7760[2];
        };
        TypedOutput typedOutput = (TypedOutput &)*output;

        typedOutput.inputPinStates = ((Mio168DinChannel *)Channel::getBySlotIndex(slotIndex, DIN_SUBCHANNEL_INDEX))->pinStates;
        typedOutput.outputPinStates = ((Mio168DoutChannel *)Channel::getBySlotIndex(slotIndex, DOUT_SUBCHANNEL_INDEX))->pinStates;

        {
            auto channel = (Mio168Dac7760Channel *)Channel::getBySlotIndex(slotIndex, DAC_7760_1_SUBCHANNEL_INDEX);
            typedOutput.dac7760[0].mode = channel->m_mode;
            typedOutput.dac7760[0].currentRange = channel->m_currentRange;
            typedOutput.dac7760[0].current = channel->i.set; 
            typedOutput.dac7760[0].voltageRange = channel->m_voltageRange;
            typedOutput.dac7760[0].voltage = channel->u.set; 
        }

        {
            auto channel = (Mio168Dac7760Channel *)Channel::getBySlotIndex(slotIndex, DAC_7760_2_SUBCHANNEL_INDEX);
            typedOutput.dac7760[0].mode = channel->m_mode;
            typedOutput.dac7760[0].currentRange = channel->m_currentRange;
            typedOutput.dac7760[0].current = channel->i.set; 
            typedOutput.dac7760[0].voltageRange = channel->m_voltageRange;
            typedOutput.dac7760[0].voltage = channel->u.set; 
        }

        auto status = bp3c::comm::transferDMA(slotIndex, output, input, BUFFER_SIZE);
        if (status != bp3c::comm::TRANSFER_STATUS_OK) {
        	onSpiDmaTransferCompleted(status);
        }
    }

    void onSpiDmaTransferCompleted(int status) override {
        if (status == bp3c::comm::TRANSFER_STATUS_OK) {
            numCrcErrors = 0;

            ((Mio168DinChannel *)Channel::getBySlotIndex(slotIndex, DIN_SUBCHANNEL_INDEX))->pinStates = input[0];

            static uint32_t totalSamples = 0;

            uint16_t *inputU16 = (uint16_t *)(input + 24);

            uint16_t numSamples = inputU16[-1];

            analogInputValues[(totalSamples + 0) % 4] = inputU16[0];
            analogInputValues[(totalSamples + 1) % 4] = inputU16[1];
            analogInputValues[(totalSamples + 2) % 4] = inputU16[2];
            analogInputValues[(totalSamples + 3) % 4] = inputU16[3];

            analogInputValues[3] = numSamples;

            totalSamples += numSamples;
        } else {
            if (status == bp3c::comm::TRANSFER_STATUS_CRC_ERROR) {
                if (++numCrcErrors >= 10) {
                    event_queue::pushEvent(event_queue::EVENT_ERROR_SLOT1_CRC_CHECK_ERROR + slotIndex);
                    synchronized = false;
                    testResult = TEST_FAILED;
                } else {
                    DebugTrace("Slot %d CRC %d\n", slotIndex + 1, numCrcErrors);
                }
            } else {
                DebugTrace("Slot %d SPI transfer error %d\n", slotIndex + 1, status);
            }
        }
    }

    void onPowerDown() override {
        synchronized = false;
    }
};

Module *Mio168ModuleInfo::createModule(uint8_t slotIndex, uint16_t moduleRevision, bool firmwareInstalled) {
    return new Mio168Module(slotIndex, this, moduleRevision, firmwareInstalled);
}

Channel *Mio168ModuleInfo::createChannel(int slotIndex, int channelIndex, int subchannelIndex) {
    if (subchannelIndex == DIN_SUBCHANNEL_INDEX) {
        void *buffer = malloc(sizeof(Mio168DinChannel));
        memset(buffer, 0, sizeof(Mio168DinChannel));
        return new (buffer) Mio168DinChannel(slotIndex, channelIndex);
    } else if (subchannelIndex == DOUT_SUBCHANNEL_INDEX) {
        void *buffer = malloc(sizeof(Mio168DoutChannel));
        memset(buffer, 0, sizeof(Mio168DoutChannel));
        return new (buffer) Mio168DoutChannel(slotIndex, channelIndex);
    } else if (subchannelIndex >= ADC_1_SUBCHANNEL_INDEX && subchannelIndex <= ADC_4_SUBCHANNEL_INDEX) {
        void *buffer = malloc(sizeof(Mio168AdcChannel));
        memset(buffer, 0, sizeof(Mio168AdcChannel));
        return new (buffer) Mio168AdcChannel(slotIndex, channelIndex, subchannelIndex);
    } else if (subchannelIndex >= DAC_7760_1_SUBCHANNEL_INDEX && subchannelIndex <= DAC_7760_2_SUBCHANNEL_INDEX) {
        void *buffer = malloc(sizeof(Mio168Dac7760Channel));
        memset(buffer, 0, sizeof(Mio168Dac7760Channel));
        return new (buffer) Mio168Dac7760Channel(slotIndex, channelIndex, subchannelIndex);
    } else if (subchannelIndex >= DAC_7563_1_SUBCHANNEL_INDEX && subchannelIndex <= DAC_7563_2_SUBCHANNEL_INDEX) {
        void *buffer = malloc(sizeof(Mio168Dac7563Channel));
        memset(buffer, 0, sizeof(Mio168Dac7563Channel));
        return new (buffer) Mio168Dac7563Channel(slotIndex, channelIndex, subchannelIndex);
    } else if (subchannelIndex >= PWM_1_SUBCHANNEL_INDEX && subchannelIndex <= PWM_2_SUBCHANNEL_INDEX) {
        void *buffer = malloc(sizeof(Mio168PwmChannel));
        memset(buffer, 0, sizeof(Mio168PwmChannel));
        return new (buffer) Mio168PwmChannel(slotIndex, channelIndex, subchannelIndex);
    }
    return nullptr;
}

static Mio168ModuleInfo g_mio168ModuleInfo;
ModuleInfo *g_moduleInfo = &g_mio168ModuleInfo;

TestResult MioChannel::getTestResult() {
    auto mio168Module = (Mio168Module *)g_slots[slotIndex];
    return mio168Module->testResult;
}

} // namespace dib_mio168

namespace gui {

using namespace dib_mio168;

void data_dib_mio168_inputs(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_COUNT) {
        value = 8;
    } else if (operation == DATA_OPERATION_GET_CURSOR_VALUE) {
        value = hmi::g_selectedSlotIndex * 8 + value.getInt();
    }
}

void data_dib_mio168_input_no(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = cursor % 8 + 1;
    }
}

void data_dib_mio168_input_state(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        ((Mio168DinChannel *)Channel::getBySlotIndex(cursor / 8, DIN_SUBCHANNEL_INDEX))->getPinState(cursor % 8);
    }
}

void data_dib_mio168_outputs(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_COUNT) {
        value = 8;
    } else if (operation == DATA_OPERATION_GET_CURSOR_VALUE) {
        value = hmi::g_selectedSlotIndex * 8 + value.getInt();
    }
}

void data_dib_mio168_output_no(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = cursor % 8 + 1;
    }
}

void data_dib_mio168_output_state(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = ((Mio168DoutChannel *)Channel::getBySlotIndex(cursor / 8, DOUT_SUBCHANNEL_INDEX))->getPinState(cursor % 8);
    }
}

void action_dib_mio168_toggle_output_state() {
    int cursor = getFoundWidgetAtDown().cursor;
    auto channel = ((Mio168DoutChannel *)Channel::getBySlotIndex(cursor / 8, DOUT_SUBCHANNEL_INDEX));
    int pin = cursor % 8;
    channel->setPinState(pin, channel->getPinState(pin));
}

void data_dib_mio168_analog_inputs(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_COUNT) {
        value = 4;
    } else if (operation == DATA_OPERATION_GET_CURSOR_VALUE) {
        value = hmi::g_selectedSlotIndex * 4 + value.getInt();
    }
}

void data_dib_mio168_analog_input_value(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        auto mio168Module = (Mio168Module *)g_slots[cursor / 4];
        value = mio168Module->analogInputValues[cursor % 4];
    }
}


} // namespace gui

} // namespace eez
