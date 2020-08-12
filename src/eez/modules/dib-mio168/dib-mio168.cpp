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

struct Mio168DinChannel : public Channel {
    Mio168DinChannel(int slotIndex, int channelIndex)
        : Channel(slotIndex, channelIndex, DIN_SUBCHANNEL_INDEX) 
    {
        flags.powerOk = 1;
    }

    TestResult getTestResult() override;

    void getParams(uint16_t moduleRevision) override {
        params.features = CH_FEATURE_DINPUT;
    }

    uint8_t getDigitalInputData() override;

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

struct Mio168DoutChannel : public Channel {
    Mio168DoutChannel(int slotIndex, int channelIndex)
        : Channel(slotIndex, channelIndex, DOUT_SUBCHANNEL_INDEX) 
    {
        flags.powerOk = 1;
    }

    void getParams(uint16_t moduleRevision) override {
        params.features = CH_FEATURE_DOUTPUT;
    }

    TestResult getTestResult() override;

    uint8_t getDigitalOutputData() override;
    void setDigitalOutputData(uint8_t data) override;

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
            2
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
    uint8_t inputPinStates = 0;
    uint8_t outputPinStates = 0;
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
        output[0] = inputPinStates;
        output[1] = outputPinStates;

        auto status = bp3c::comm::transferDMA(slotIndex, output, input, BUFFER_SIZE);
        if (status != bp3c::comm::TRANSFER_STATUS_OK) {
        	onSpiDmaTransferCompleted(status);
        }
    }

    void onSpiDmaTransferCompleted(int status) override {
        if (status == bp3c::comm::TRANSFER_STATUS_OK) {
            numCrcErrors = 0;

            inputPinStates = input[0];

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

    int getInputPinState(int pin) {
        return inputPinStates & (1 << pin) ? 1 : 0;
    }

    int getOutputPinState(int pin) {
        return outputPinStates & (1 << pin) ? 1 : 0;
    }

    void setOutputPinState(int pin, int state) {
        if (state) {
            outputPinStates |= 1 << pin;
        } else {
            outputPinStates &= ~(1 << pin);
        }
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
    }
    return nullptr;
}

static Mio168ModuleInfo g_mio168ModuleInfo;
ModuleInfo *g_moduleInfo = &g_mio168ModuleInfo;

TestResult Mio168DinChannel::getTestResult() {
    auto mio168Module = (dib_mio168::Mio168Module *)g_slots[slotIndex];
    return mio168Module->testResult;
}

uint8_t Mio168DinChannel::getDigitalInputData() {
    auto mio168Module = (dib_mio168::Mio168Module *)g_slots[slotIndex];
    return mio168Module->inputPinStates;
}

TestResult Mio168DoutChannel::getTestResult() {
    auto mio168Module = (dib_mio168::Mio168Module *)g_slots[slotIndex];
    return mio168Module->testResult;
}

uint8_t Mio168DoutChannel::getDigitalOutputData() {
    auto mio168Module = (dib_mio168::Mio168Module *)g_slots[slotIndex];
    return mio168Module->outputPinStates;
}

void Mio168DoutChannel::setDigitalOutputData(uint8_t data) {
    auto mio168Module = (dib_mio168::Mio168Module *)g_slots[slotIndex];
    mio168Module->outputPinStates = data;
}

} // namespace dib_mio168

namespace gui {

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
        auto mio168Module = (dib_mio168::Mio168Module *)g_slots[cursor / 8];
        value = mio168Module->getInputPinState(cursor % 8);
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
        auto mio168Module = (dib_mio168::Mio168Module *)g_slots[cursor / 8];
        value = mio168Module->getOutputPinState(cursor % 8);
    }
}

void action_dib_mio168_toggle_output_state() {
    int cursor = getFoundWidgetAtDown().cursor;
    auto mio168Module = (dib_mio168::Mio168Module *)g_slots[cursor / 8];
    mio168Module->setOutputPinState(cursor % 8, !mio168Module->getOutputPinState(cursor % 8));
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
        auto mio168Module = (dib_mio168::Mio168Module *)g_slots[cursor / 4];
        value = mio168Module->analogInputValues[cursor % 4];
    }
}


} // namespace gui

} // namespace eez
