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

#include <scpi/scpi.h>

using namespace eez::psu;
using namespace eez::psu::gui;
using namespace eez::gui;

namespace eez {
namespace dib_mio168 {

static const uint16_t MODULE_REVISION_R1B2  = 0x0102;

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

////////////////////////////////////////////////////////////////////////////////

struct MioChannel {
};

struct Mio168DinChannel : public MioChannel {
    uint8_t pinStates = 0;

    uint8_t getDigitalInputData() {
        return pinStates;
    }

    int getPinState(int pin) {
        return pinStates & (1 << pin) ? 1 : 0;
    }
};

struct Mio168DoutChannel : public MioChannel {
    uint8_t pinStates = 0;

    uint8_t getDigitalOutputData() {
        return pinStates;
    }

    void setDigitalOutputData(uint8_t data) {
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
};

struct Mio168AdcChannel : public MioChannel {
};

struct Mio168Dac7760Channel : public MioChannel {
    SourceMode m_mode;
    int8_t m_currentRange;
    int8_t m_voltageRange;

    SourceMode getMode() {
        return m_mode;
    }

    void setMode(SourceMode mode) {
        m_mode = mode;
    }

    int8_t getCurrentRange() {
        return m_currentRange;
    }
    
    void setCurrentRange(int8_t range) {
        m_currentRange = range;
    }

    int8_t getVoltageRange() {
        return m_voltageRange;
    }
    
    void setVoltageRange(int8_t range) {
        m_voltageRange = range;
    }  
};

struct Mio168Dac7563Channel : public MioChannel {
};

struct Mio168PwmChannel : public MioChannel {
};

Mio168DinChannel g_dinChannels[NUM_SLOTS];
Mio168DoutChannel g_doutChannels[NUM_SLOTS];
Mio168AdcChannel g_adcChannels[NUM_SLOTS][4];
Mio168Dac7760Channel g_dac7760Channels[NUM_SLOTS][2];
Mio168Dac7563Channel g_dac7563Channels[NUM_SLOTS][2];
Mio168PwmChannel g_pwmChannels[NUM_SLOTS][2];

////////////////////////////////////////////////////////////////////////////////

#define BUFFER_SIZE 1024

struct Mio168Module : public Module {
public:
    TestResult testResult = TEST_NONE;
    bool synchronized = false;
    int numCrcErrors = 0;
    uint8_t input[BUFFER_SIZE];
    uint8_t output[BUFFER_SIZE];
    uint16_t analogInputValues[4];
    bool spiReady = false;

    Mio168Module() {
        moduleType = MODULE_TYPE_DIB_MIO168;
        moduleName = "MIO168";
        moduleBrand = "Envox";
        latestModuleRevision = MODULE_REVISION_R1B2;
        flashMethod = FLASH_METHOD_STM32_BOOTLOADER_SPI;
        flashDuration = 10000;
#if defined(EEZ_PLATFORM_STM32)        
        spiBaudRatePrescaler = SPI_BAUDRATEPRESCALER_4;
        spiCrcCalculationEnable = true;
#else
        spiBaudRatePrescaler = 0;
        spiCrcCalculationEnable = false;
#endif
        numPowerChannels = 0;
        numOtherChannels = 12;

        memset(input, 0, sizeof(input));
        memset(output, 0, sizeof(output));
    }

    Module *createModule() override {
        return new Mio168Module();
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

        typedOutput.inputPinStates = g_dinChannels[slotIndex].pinStates;
        typedOutput.outputPinStates = g_doutChannels[slotIndex].pinStates;

        {
            auto channel = &g_dac7760Channels[slotIndex][0];
            typedOutput.dac7760[0].mode = channel->m_mode;
            typedOutput.dac7760[0].currentRange = channel->m_currentRange;
            typedOutput.dac7760[0].current = 0; // TODO channel->i.set;
            typedOutput.dac7760[0].voltageRange = channel->m_voltageRange;
            typedOutput.dac7760[0].voltage = 0; // TODO channel->u.set;
        }

        {
            auto channel = &g_dac7760Channels[slotIndex][1];
            typedOutput.dac7760[0].mode = channel->m_mode;
            typedOutput.dac7760[0].currentRange = channel->m_currentRange;
            typedOutput.dac7760[0].current = 0; // TODO channel->i.set;
            typedOutput.dac7760[0].voltageRange = channel->m_voltageRange;
            typedOutput.dac7760[0].voltage = 0; // TODO channel->u.set;
        }

        auto status = bp3c::comm::transferDMA(slotIndex, (uint8_t *)&typedOutput, input, BUFFER_SIZE);
        if (status != bp3c::comm::TRANSFER_STATUS_OK) {
        	onSpiDmaTransferCompleted(status);
        }
    }

    void onSpiDmaTransferCompleted(int status) override {
        if (status == bp3c::comm::TRANSFER_STATUS_OK) {
            numCrcErrors = 0;

            g_dinChannels[slotIndex].pinStates = input[0];

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

    bool getDigitalInputData(int subchannelIndex, uint8_t &data, int *err) override {
        if (subchannelIndex != DIN_SUBCHANNEL_INDEX) {
            if (*err) {
                *err = SCPI_ERROR_ILLEGAL_PARAMETER_VALUE;
            }
            return false;
        }
        data = g_dinChannels[slotIndex].getDigitalInputData();
        return true;
    }

    bool getDigitalOutputData(int subchannelIndex, uint8_t &data, int *err) override {
        if (subchannelIndex != DOUT_SUBCHANNEL_INDEX) {
            if (*err) {
                *err = SCPI_ERROR_ILLEGAL_PARAMETER_VALUE;
            }
            return false;
        }
        data = g_doutChannels[slotIndex].getDigitalOutputData();
        return true;
    }
    
    bool setDigitalOutputData(int subchannelIndex, uint8_t data, int *err) override {
        if (subchannelIndex != DOUT_SUBCHANNEL_INDEX) {
            if (*err) {
                *err = SCPI_ERROR_ILLEGAL_PARAMETER_VALUE;
            }
            return false;
        }
        g_doutChannels[slotIndex].setDigitalOutputData(data);
        return true;
    }

    bool getMode(int subchannelIndex, SourceMode &mode, int *err) override {
        if (subchannelIndex != DAC_7760_1_SUBCHANNEL_INDEX && subchannelIndex != DAC_7760_2_SUBCHANNEL_INDEX) {
            if (*err) {
                *err = SCPI_ERROR_ILLEGAL_PARAMETER_VALUE;
            }
            return false;
        }
        mode = g_dac7760Channels[slotIndex][subchannelIndex - DAC_7760_1_SUBCHANNEL_INDEX].getMode();
        return true;
    }
    
    bool setMode(int subchannelIndex, SourceMode mode, int *err) override {
        if (subchannelIndex != DAC_7760_1_SUBCHANNEL_INDEX && subchannelIndex != DAC_7760_2_SUBCHANNEL_INDEX) {
            if (*err) {
                *err = SCPI_ERROR_ILLEGAL_PARAMETER_VALUE;
            }
            return false;
        }
        g_dac7760Channels[slotIndex][subchannelIndex - DAC_7760_1_SUBCHANNEL_INDEX].setMode(mode);
        return true;
    }

    bool getCurrentRange(int subchannelIndex, int8_t &range, int *err) override {
        if (subchannelIndex != DAC_7760_1_SUBCHANNEL_INDEX && subchannelIndex != DAC_7760_2_SUBCHANNEL_INDEX) {
            if (*err) {
                *err = SCPI_ERROR_ILLEGAL_PARAMETER_VALUE;
            }
            return false;
        }
        range = g_dac7760Channels[slotIndex][subchannelIndex - DAC_7760_1_SUBCHANNEL_INDEX].getCurrentRange();
        return true;
    }
    
    bool setCurrentRange(int subchannelIndex, int8_t range, int *err) override {
        if (subchannelIndex != DAC_7760_1_SUBCHANNEL_INDEX && subchannelIndex != DAC_7760_2_SUBCHANNEL_INDEX) {
            if (*err) {
                *err = SCPI_ERROR_ILLEGAL_PARAMETER_VALUE;
            }
            return false;
        }
        g_dac7760Channels[slotIndex][subchannelIndex - DAC_7760_1_SUBCHANNEL_INDEX].setCurrentRange(range);
        return true;
    }

    bool getVoltageRange(int subchannelIndex, int8_t &range, int *err) override {
        if (subchannelIndex != DAC_7760_1_SUBCHANNEL_INDEX && subchannelIndex != DAC_7760_2_SUBCHANNEL_INDEX) {
            if (*err) {
                *err = SCPI_ERROR_ILLEGAL_PARAMETER_VALUE;
            }
            return false;
        }
        range = g_dac7760Channels[slotIndex][subchannelIndex - DAC_7760_1_SUBCHANNEL_INDEX].getVoltageRange();
        return true;
    }
    
    bool setVoltageRange(int subchannelIndex, int8_t range, int *err) override {
        if (subchannelIndex != DAC_7760_1_SUBCHANNEL_INDEX && subchannelIndex != DAC_7760_2_SUBCHANNEL_INDEX) {
            if (*err) {
                *err = SCPI_ERROR_ILLEGAL_PARAMETER_VALUE;
            }
            return false;
        }
        g_dac7760Channels[slotIndex][subchannelIndex - DAC_7760_1_SUBCHANNEL_INDEX].setVoltageRange(range);
        return true;
    }
};

static Mio168Module g_mio168Module;
Module *g_module = &g_mio168Module;

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
        g_dinChannels[cursor / 8].getPinState(cursor % 8);
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
        value = g_doutChannels[cursor / 8].getPinState(cursor % 8);
    }
}

void action_dib_mio168_toggle_output_state() {
    int cursor = getFoundWidgetAtDown().cursor;
    int pin = cursor % 8;
    g_doutChannels[pin].setPinState(pin, g_doutChannels[pin].getPinState(pin));
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
