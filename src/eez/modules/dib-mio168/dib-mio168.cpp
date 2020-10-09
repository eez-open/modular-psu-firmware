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
#include "eez/modules/psu/gui/keypad.h"
#include "eez/modules/psu/gui/edit_mode.h"
#include <eez/modules/bp3c/comm.h>
#include <eez/modules/bp3c/flash_slave.h>

#include "./dib-mio168.h"

#include <scpi/scpi.h>

using namespace eez::psu;
using namespace eez::psu::gui;
using namespace eez::gui;

namespace eez {
namespace dib_mio168 {

enum Mio168HighPriorityThreadMessage {
    PSU_MESSAGE_DAC7760_CONFIGURE = PSU_MESSAGE_MODULE_SPECIFIC
};

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

static float U_CAL_POINTS[2] = { 1.0f, 9.0f };
static float DAC_MIN = 0.0f;
static float DAC_MAX = 10.0f;
static float DAC_RESOLUTION = 0.01f;
static float DAC_ENCODER_STEP_VALUES[] = { 0.5f, 0.2f, 0.1f, 0.01f };
static float DAC_AMPER_ENCODER_STEP_VALUES[] = { 0.05f, 0.01f, 0.005f, 0.001f };

static float PWM_MIN_FREQUENCY = 0.1f;
static float PWM_MAX_FREQUENCY = 1000000.0f;

////////////////////////////////////////////////////////////////////////////////

struct FromMasterToSlave {
    uint8_t reserved;
    uint8_t outputPinStates;
    struct {
        SourceMode mode;
        int8_t currentRange;
        float current;
        int8_t voltageRange;
        float voltage;
    } dac7760[2];
};

struct FromSlaveToMaster {
    uint8_t inputPinStates;
};

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
    uint16_t value = 0;
};

struct Mio168Dac7760Channel : public MioChannel {
    bool m_outputEnabled = false;
    SourceMode m_mode = SOURCE_MODE_VOLTAGE;
    int8_t m_currentRange = 5;
    int8_t m_voltageRange = 0;
    float m_currentValue = 0;
    float m_voltageValue = 0;

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

    float getValue() {
        return m_mode == SOURCE_MODE_VOLTAGE ? m_voltageValue : m_currentValue;
    }

    Unit getUnit() {
        return m_mode == SOURCE_MODE_VOLTAGE ? UNIT_VOLT : UNIT_AMPER;
    }

    void setValue(float value) {
        if (m_mode == SOURCE_MODE_VOLTAGE) {
            m_voltageValue = value;
        } else {
            m_currentValue = value;
        }
    }

    float getMinValue() {
        if (m_mode == SOURCE_MODE_VOLTAGE) {
            if (m_voltageRange == 0) {
                return 0;
            } 
            if (m_voltageRange == 1) {
                return 0;
            } 
            if (m_voltageRange == 2) {
                return -5.0f;
            } 
            return -10.0f;
        } else {
            if (m_currentRange == 5) {
                return 0.004f;
            }
            if (m_currentRange == 6) {
                return 0;
            }
            return 0;
        }
    }

    float getMaxValue() {
        if (m_mode == SOURCE_MODE_VOLTAGE) {
            if (m_voltageRange == 0) {
                return 5.0f;
            } 
            if (m_voltageRange == 1) {
                return 10.0f;
            } 
            if (m_voltageRange == 2) {
                return 5.0f;
            } 
            return 10.0f;
        } else {
            if (m_currentRange == 5) {
                return 0.02f;
            }
            if (m_currentRange == 6) {
                return 0.02f;
            }
            return 0.024f;
        }
    }

    float getResolution() {
        return 0.001f;
    }

    void getStepValues(StepValues *stepValues) {
        if (m_mode == SOURCE_MODE_VOLTAGE) {
            stepValues->values = DAC_ENCODER_STEP_VALUES;
            stepValues->count = sizeof(DAC_ENCODER_STEP_VALUES) / sizeof(float);
            stepValues->unit = UNIT_VOLT;
        } else {
            stepValues->values = DAC_AMPER_ENCODER_STEP_VALUES;
            stepValues->count = sizeof(DAC_AMPER_ENCODER_STEP_VALUES) / sizeof(float);
            stepValues->unit = UNIT_AMPER;
        }
    }
};

struct Mio168Dac7563Channel : public MioChannel {
    float value = 0;
};

struct Mio168PwmChannel : public MioChannel {
    float freq = 0;
    float duty = 0;
};

////////////////////////////////////////////////////////////////////////////////

#define BUFFER_SIZE 1024

struct Mio168Module : public Module {
public:
    TestResult testResult = TEST_NONE;
    bool synchronized = false;
    int numCrcErrors = 0;
    uint8_t input[BUFFER_SIZE];
    uint8_t output[BUFFER_SIZE];
    bool spiReady = false;

    Mio168DinChannel dinChannel;
    Mio168DoutChannel doutChannel;
    Mio168AdcChannel adcChannels[4];
    Mio168Dac7760Channel dac7760Channels[2];
    Mio168Dac7563Channel dac7563Channels[2];
    Mio168PwmChannel pwmChannels[2];

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
        FromMasterToSlave data = (FromMasterToSlave &)*output;

        data.outputPinStates = doutChannel.pinStates;

        for (int i = 0; i < 2; i++) {
            auto channel = &dac7760Channels[i];
            data.dac7760[0].mode = channel->m_mode;
            data.dac7760[0].currentRange = channel->m_currentRange;
            data.dac7760[0].current = 0; // TODO channel->i.set;
            data.dac7760[0].voltageRange = channel->m_voltageRange;
            data.dac7760[0].voltage = 0; // TODO channel->u.set;
        }

        auto status = bp3c::comm::transferDMA(slotIndex, (uint8_t *)&data, input, BUFFER_SIZE);
        if (status != bp3c::comm::TRANSFER_STATUS_OK) {
        	onSpiDmaTransferCompleted(status);
        }
    }

    void onSpiDmaTransferCompleted(int status) override {
        if (status == bp3c::comm::TRANSFER_STATUS_OK) {
            numCrcErrors = 0;

            FromSlaveToMaster data = (FromSlaveToMaster &)*input;

            dinChannel.pinStates = data.inputPinStates;

            static uint32_t totalSamples = 0;

            uint16_t *inputU16 = (uint16_t *)(input + 24);

            uint16_t numSamples = inputU16[-1];

            adcChannels[(totalSamples + 0) % 4].value = inputU16[0];
            adcChannels[(totalSamples + 1) % 4].value = inputU16[1];
            adcChannels[(totalSamples + 2) % 4].value = inputU16[2];
            adcChannels[(totalSamples + 3) % 4].value = inputU16[3];

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

    Page *getPageFromId(int pageId) override;

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

    int getSlotSettingsPageId() override {
        return getTestResult() == TEST_OK ? PAGE_ID_DIB_MIO168_SETTINGS : PAGE_ID_SLOT_SETTINGS;
    }

    void onHighPriorityThreadMessage(uint8_t type, uint32_t param) override;

    bool getDigitalInputData(int subchannelIndex, uint8_t &data, int *err) override {
        if (subchannelIndex != DIN_SUBCHANNEL_INDEX) {
            if (*err) {
                *err = SCPI_ERROR_ILLEGAL_PARAMETER_VALUE;
            }
            return false;
        }
        data = dinChannel.getDigitalInputData();
        return true;
    }

    bool getDigitalOutputData(int subchannelIndex, uint8_t &data, int *err) override {
        if (subchannelIndex != DOUT_SUBCHANNEL_INDEX) {
            if (*err) {
                *err = SCPI_ERROR_ILLEGAL_PARAMETER_VALUE;
            }
            return false;
        }
        data = doutChannel.getDigitalOutputData();
        return true;
    }
    
    bool setDigitalOutputData(int subchannelIndex, uint8_t data, int *err) override {
        if (subchannelIndex != DOUT_SUBCHANNEL_INDEX) {
            if (*err) {
                *err = SCPI_ERROR_ILLEGAL_PARAMETER_VALUE;
            }
            return false;
        }
        doutChannel.setDigitalOutputData(data);
        return true;
    }

    bool getMode(int subchannelIndex, SourceMode &mode, int *err) override {
        if (subchannelIndex != DAC_7760_1_SUBCHANNEL_INDEX && subchannelIndex != DAC_7760_2_SUBCHANNEL_INDEX) {
            if (*err) {
                *err = SCPI_ERROR_ILLEGAL_PARAMETER_VALUE;
            }
            return false;
        }
        mode = dac7760Channels[subchannelIndex - DAC_7760_1_SUBCHANNEL_INDEX].getMode();
        return true;
    }
    
    bool setMode(int subchannelIndex, SourceMode mode, int *err) override {
        if (subchannelIndex != DAC_7760_1_SUBCHANNEL_INDEX && subchannelIndex != DAC_7760_2_SUBCHANNEL_INDEX) {
            if (*err) {
                *err = SCPI_ERROR_ILLEGAL_PARAMETER_VALUE;
            }
            return false;
        }
        dac7760Channels[subchannelIndex - DAC_7760_1_SUBCHANNEL_INDEX].setMode(mode);
        return true;
    }

    bool getCurrentRange(int subchannelIndex, int8_t &range, int *err) override {
        if (subchannelIndex != DAC_7760_1_SUBCHANNEL_INDEX && subchannelIndex != DAC_7760_2_SUBCHANNEL_INDEX) {
            if (*err) {
                *err = SCPI_ERROR_ILLEGAL_PARAMETER_VALUE;
            }
            return false;
        }
        range = dac7760Channels[subchannelIndex - DAC_7760_1_SUBCHANNEL_INDEX].getCurrentRange();
        return true;
    }
    
    bool setCurrentRange(int subchannelIndex, int8_t range, int *err) override {
        if (subchannelIndex != DAC_7760_1_SUBCHANNEL_INDEX && subchannelIndex != DAC_7760_2_SUBCHANNEL_INDEX) {
            if (*err) {
                *err = SCPI_ERROR_ILLEGAL_PARAMETER_VALUE;
            }
            return false;
        }
        dac7760Channels[subchannelIndex - DAC_7760_1_SUBCHANNEL_INDEX].setCurrentRange(range);
        return true;
    }

    bool getVoltageRange(int subchannelIndex, int8_t &range, int *err) override {
        if (subchannelIndex != DAC_7760_1_SUBCHANNEL_INDEX && subchannelIndex != DAC_7760_2_SUBCHANNEL_INDEX) {
            if (*err) {
                *err = SCPI_ERROR_ILLEGAL_PARAMETER_VALUE;
            }
            return false;
        }
        range = dac7760Channels[subchannelIndex - DAC_7760_1_SUBCHANNEL_INDEX].getVoltageRange();
        return true;
    }
    
    bool setVoltageRange(int subchannelIndex, int8_t range, int *err) override {
        if (subchannelIndex != DAC_7760_1_SUBCHANNEL_INDEX && subchannelIndex != DAC_7760_2_SUBCHANNEL_INDEX) {
            if (*err) {
                *err = SCPI_ERROR_ILLEGAL_PARAMETER_VALUE;
            }
            return false;
        }
        dac7760Channels[subchannelIndex - DAC_7760_1_SUBCHANNEL_INDEX].setVoltageRange(range);
        return true;
    }
};

static Mio168Module g_mio168Module;
Module *g_module = &g_mio168Module;

////////////////////////////////////////////////////////////////////////////////

class Dac7760ConfigurationPage : public SetPage {
public:
    static int g_selectedChannelIndex;

    void pageAlloc() {
        Mio168Module *module = (Mio168Module *)g_slots[hmi::g_selectedSlotIndex];
        Mio168Dac7760Channel &channel = module->dac7760Channels[g_selectedChannelIndex - DAC_7760_1_SUBCHANNEL_INDEX];

        m_outputEnabled = m_outputEnabledOrig = channel.m_outputEnabled;
        m_mode = m_modeOrig = channel.m_mode;
        m_currentRange = m_currentRangeOrig = channel.m_currentRange;
        m_voltageRange = m_voltageRangeOrig = channel.m_voltageRange;
    }

    int getDirty() { 
        return m_outputEnabled != m_outputEnabledOrig ||
            m_mode != m_modeOrig ||
            m_currentRange != m_currentRangeOrig ||
            m_voltageRange != m_voltageRangeOrig;
    }

    void set() {
        if (getDirty()) {
            sendMessageToPsu((HighPriorityThreadMessage)PSU_MESSAGE_DAC7760_CONFIGURE, hmi::g_selectedSlotIndex);
        }

        popPage();
    }

    bool m_outputEnabled;
    SourceMode m_mode;
    int8_t m_currentRange;
    int8_t m_voltageRange;

private:
    bool m_outputEnabledOrig;
    SourceMode m_modeOrig;
    int8_t m_currentRangeOrig;
    int8_t m_voltageRangeOrig;
};

int Dac7760ConfigurationPage::g_selectedChannelIndex;
static Dac7760ConfigurationPage g_dac7760ConfigurationPage;

////////////////////////////////////////////////////////////////////////////////

Page *Mio168Module::getPageFromId(int pageId) {
    if (pageId == PAGE_ID_DIB_MIO168_DAC_7760_CONFIGURATION) {
        return &g_dac7760ConfigurationPage;
    }
    return nullptr;
}

void Mio168Module::onHighPriorityThreadMessage(uint8_t type, uint32_t param) {
    if (type == PSU_MESSAGE_DAC7760_CONFIGURE) {
        Mio168Dac7760Channel &channel = dac7760Channels[Dac7760ConfigurationPage::g_selectedChannelIndex - DAC_7760_1_SUBCHANNEL_INDEX];
        channel.m_outputEnabled = g_dac7760ConfigurationPage.m_outputEnabled;
        channel.m_mode = g_dac7760ConfigurationPage.m_mode;
        channel.m_currentRange = g_dac7760ConfigurationPage.m_currentRange;
        channel.m_voltageRange = g_dac7760ConfigurationPage.m_voltageRange;

        if (channel.getValue() < channel.getMinValue()) {
            channel.setValue(channel.getMinValue());
        } else if (channel.getValue() > channel.getMaxValue()) {
            channel.setValue(channel.getMaxValue());
        }
    }
}

} // namespace dib_mio168

namespace gui {

using namespace dib_mio168;

static EnumItem g_dac7760OutputModeEnumDefinition[] = {
    { SOURCE_MODE_CURRENT, "Current" },
    { SOURCE_MODE_VOLTAGE, "Voltage" },
    { 0, 0 }
};

static EnumItem g_dac7760VoltageRangeEnumDefinition[] = {
    { 0, "0 V to +5 V" },
    { 1, "0 V to +10 V" },
    { 2, "-5 V to +5 V" },
    { 3, "-10 V to +10 V" },
    { 0, 0 }
};

static EnumItem g_dac7760CurrentRangeEnumDefinition[] = {
    { 5, "4 mA to 20 mA" },
    { 6, "0 mA to 20 mA" },
    { 7, "0 mA to 24 mA" },
    { 0, 0 }
};

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
        auto mio168Module = (Mio168Module *)g_slots[cursor / 8];
        mio168Module->dinChannel.getPinState(cursor % 8);
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
        auto mio168Module = (Mio168Module *)g_slots[cursor / 8];
        value = mio168Module->doutChannel.getPinState(cursor % 8);
    }
}

void action_dib_mio168_toggle_output_state() {
    auto mio168Module = (Mio168Module *)g_slots[hmi::g_selectedSlotIndex];
    int cursor = getFoundWidgetAtDown().cursor;
    int pin = cursor % 8;
    mio168Module->doutChannel.setPinState(pin, mio168Module->doutChannel.getPinState(pin));
}

void data_dib_mio168_analog_inputs(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_COUNT) {
        value = 4;
    } else if (operation == DATA_OPERATION_GET_CURSOR_VALUE) {
        value = hmi::g_selectedSlotIndex * 4 + value.getInt();
    }
}

void data_dib_mio168_analog_input_label(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        static const char *labels[4] = { "AIN1", "AIN2", "AIN3", "AIN4" };
        value = labels[cursor % 4];
    }
}

void data_dib_mio168_analog_input_value(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        auto mio168Module = (Mio168Module *)g_slots[cursor / 4];
        value = mio168Module->adcChannels[cursor % 4].value;
    }
}

void data_dib_mio168_analog_outputs(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_COUNT) {
        value = 4;
    } else if (operation == DATA_OPERATION_GET_CURSOR_VALUE) {
        value = hmi::g_selectedSlotIndex * 4 + value.getInt();
    }
}

void data_dib_mio168_analog_output_label(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        static const char *labels[4] = { "AO1", "AO2", "AO3", "AO4" };

        int dacChannelIndex;

        Dac7760ConfigurationPage *page = (Dac7760ConfigurationPage *)getPage(PAGE_ID_DIB_MIO168_DAC_7760_CONFIGURATION);
        if (page) {
            dacChannelIndex = Dac7760ConfigurationPage::g_selectedChannelIndex - DAC_7760_1_SUBCHANNEL_INDEX;
        } else {
            dacChannelIndex = cursor % 4;
        }

        value = labels[dacChannelIndex];
    }
}

void data_dib_mio168_analog_output_value(DataOperationEnum operation, Cursor cursor, Value &value) {
    int slotIndex = cursor / 4;
    int dacChannelIndex = cursor % 4;

    if (operation == DATA_OPERATION_GET) {
        bool focused = g_focusCursor == cursor && g_focusDataId == DATA_ID_DIB_MIO168_ANALOG_OUTPUT_VALUE;
        if (focused && g_focusEditValue.getType() != VALUE_TYPE_NONE) {
            value = g_focusEditValue;
        } else if (focused && getActivePageId() == PAGE_ID_EDIT_MODE_KEYPAD && edit_mode_keypad::g_keypad->isEditing()) {
            data_keypad_text(operation, cursor, value);
        } else {
            if (dacChannelIndex < 2) {
                auto &channel = ((Mio168Module *)g_slots[slotIndex])->dac7760Channels[dacChannelIndex];
                value = MakeValue(channel.getValue(), channel.getUnit());
            } else {
                value = MakeValue(((Mio168Module *)g_slots[slotIndex])->dac7563Channels[dacChannelIndex - 2].value, UNIT_VOLT);
            }
        }
    } else if (operation == DATA_OPERATION_GET_MIN) {
        if (dacChannelIndex < 2) {
            auto &channel = ((Mio168Module *)g_slots[slotIndex])->dac7760Channels[dacChannelIndex];
            value = MakeValue(channel.getMinValue(), channel.getUnit());
        } else {
            value = MakeValue(DAC_MIN, UNIT_VOLT);
        }
    } else if (operation == DATA_OPERATION_GET_MAX) {
        if (dacChannelIndex < 2) {
            auto &channel = ((Mio168Module *)g_slots[slotIndex])->dac7760Channels[dacChannelIndex];
            value = MakeValue(channel.getMaxValue(), channel.getUnit());
        } else {
            value = MakeValue(DAC_MAX, UNIT_VOLT);
        }
    } else if (operation == DATA_OPERATION_GET_UNIT) {
        if (dacChannelIndex < 2) {
            auto &channel = ((Mio168Module *)g_slots[slotIndex])->dac7760Channels[dacChannelIndex];
            value = channel.getUnit();
        } else {
            value = UNIT_VOLT;
        }
    } else if (operation == DATA_OPERATION_GET_ENCODER_STEP) {
        if (dacChannelIndex < 2) {
            auto &channel = ((Mio168Module *)g_slots[slotIndex])->dac7760Channels[dacChannelIndex];
            value = Value(channel.getResolution(), channel.getUnit());
        } else {
            value = Value(DAC_RESOLUTION, UNIT_VOLT);
        }
    } else if (operation == DATA_OPERATION_GET_ENCODER_STEP_VALUES) {
        StepValues *stepValues = value.getStepValues();
        if (dacChannelIndex < 2) {
            auto &channel = ((Mio168Module *)g_slots[slotIndex])->dac7760Channels[dacChannelIndex];
            channel.getStepValues(stepValues);
        } else {
            stepValues->values = DAC_ENCODER_STEP_VALUES;
            stepValues->count = sizeof(DAC_ENCODER_STEP_VALUES) / sizeof(float);
            stepValues->unit = UNIT_VOLT;
        }
        value = 1;
    } else if (operation == DATA_OPERATION_SET) {
        if (dacChannelIndex < 2) {
            auto &channel = ((Mio168Module *)g_slots[slotIndex])->dac7760Channels[dacChannelIndex];
            channel.setValue(roundPrec(value.getFloat(), channel.getResolution()));
        } else {
            ((Mio168Module *)g_slots[slotIndex])->dac7563Channels[dacChannelIndex - 2].value = roundPrec(value.getFloat(), DAC_RESOLUTION);
        }
    }
}

void data_dib_mio168_pwms(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_COUNT) {
        value = 2;
    } else if (operation == DATA_OPERATION_GET_CURSOR_VALUE) {
        value = hmi::g_selectedSlotIndex * 2 + value.getInt();
    }
}

void data_dib_mio168_pwm_label(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        static const char *labels[2] = { "P1", "P2" };
        static const char *labels2Col[2] = { "PWM1", "PWM2" };
        value = g_isCol2Mode || persist_conf::isMaxView() ? labels2Col[cursor % 4] : labels[cursor % 4];
    }
}

void data_dib_mio168_pwm_freq(DataOperationEnum operation, Cursor cursor, Value &value) {
    int slotIndex = cursor / 2;
    int pwmChannelIndex = cursor % 2;

    if (operation == DATA_OPERATION_GET) {
        bool focused = g_focusCursor == cursor && g_focusDataId == DATA_ID_DIB_MIO168_PWM_FREQ;
        if (focused && g_focusEditValue.getType() != VALUE_TYPE_NONE) {
            value = g_focusEditValue;
        } else if (focused && getActivePageId() == PAGE_ID_EDIT_MODE_KEYPAD && edit_mode_keypad::g_keypad->isEditing()) {
            data_keypad_text(operation, cursor, value);
        } else {
            value = MakeValue(((Mio168Module *)g_slots[slotIndex])->pwmChannels[pwmChannelIndex].freq, UNIT_HERTZ);
        }
    } else if (operation == DATA_OPERATION_GET_ALLOW_ZERO) {
        value = 1;
    } else if (operation == DATA_OPERATION_GET_MIN) {
        value = MakeValue(PWM_MIN_FREQUENCY, UNIT_HERTZ);
    } else if (operation == DATA_OPERATION_GET_MAX) {
        value = MakeValue(PWM_MAX_FREQUENCY, UNIT_HERTZ);
    } else if (operation == DATA_OPERATION_GET_NAME) {
        value = "PWM frequency";
    } else if (operation == DATA_OPERATION_GET_UNIT) {
        value = UNIT_HERTZ;
    } else if (operation == DATA_OPERATION_GET_ENCODER_STEP) {
        float fvalue = ((Mio168Module *)g_slots[slotIndex])->pwmChannels[pwmChannelIndex].freq;
        value = Value(MAX(powf(10.0f, floorf(log10f(fabsf(fvalue))) - 1), 0.001f), UNIT_HERTZ);
    } else if (operation == DATA_OPERATION_GET_ENCODER_STEP_VALUES) {
        static float values[] = { 10000.0f, 1000.0f, 100.0f, 1.0f };
        StepValues *stepValues = value.getStepValues();
        stepValues->values = values;
        stepValues->count = sizeof(values) / sizeof(float);
        stepValues->unit = UNIT_HERTZ;
        value = 1;
    } else if (operation == DATA_OPERATION_SET) {
        ((Mio168Module *)g_slots[slotIndex])->pwmChannels[pwmChannelIndex].freq = value.getFloat();
    }
}

void data_dib_mio168_pwm_duty(DataOperationEnum operation, Cursor cursor, Value &value) {
    int slotIndex = cursor / 2;
    int pwmChannelIndex = cursor % 2;
    
    if (operation == DATA_OPERATION_GET) {
        bool focused = g_focusCursor == cursor && g_focusDataId == DATA_ID_DIB_MIO168_PWM_DUTY;
        if (focused && g_focusEditValue.getType() != VALUE_TYPE_NONE) {
            value = g_focusEditValue;
        } else if (focused && getActivePageId() == PAGE_ID_EDIT_MODE_KEYPAD && edit_mode_keypad::g_keypad->isEditing()) {
            data_keypad_text(operation, cursor, value);
        } else {
            value = MakeValue(((Mio168Module *)g_slots[slotIndex])->pwmChannels[pwmChannelIndex].duty, UNIT_PERCENT);
        }
    } else if (operation == DATA_OPERATION_GET_MIN) {
        value = MakeValue(0.0f, UNIT_PERCENT);
    } else if (operation == DATA_OPERATION_GET_MAX) {
        value = MakeValue(100.0f, UNIT_PERCENT);
    } else if (operation == DATA_OPERATION_GET_NAME) {
        value = "PWM duty cycle";
    } else if (operation == DATA_OPERATION_GET_UNIT) {
        value = UNIT_PERCENT;
    } else if (operation == DATA_OPERATION_GET_ENCODER_STEP) {
        value = Value(1.0f, UNIT_HERTZ);
    } else if (operation == DATA_OPERATION_GET_ENCODER_STEP_VALUES) {
        static float values[] = { 5.0f, 1.0f, 0.5f, 0.1f };
        StepValues *stepValues = value.getStepValues();
        stepValues->values = values;
        stepValues->count = sizeof(values) / sizeof(float);
        stepValues->unit = UNIT_PERCENT;
        value = 1;
    } else if (operation == DATA_OPERATION_SET) {
        ((Mio168Module *)g_slots[slotIndex])->pwmChannels[pwmChannelIndex].duty = value.getFloat();
    } 
}

void data_dib_mio168_dac_output_enabled(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        Dac7760ConfigurationPage *page = (Dac7760ConfigurationPage *)getPage(PAGE_ID_DIB_MIO168_DAC_7760_CONFIGURATION);
        if (page) {
            value = g_dac7760ConfigurationPage.m_outputEnabled;
        } else {
            int slotIndex = cursor / 4;
            int dacChannelIndex = cursor % 4;
            if (dacChannelIndex < 2) {
                auto &channel = ((Mio168Module *)g_slots[slotIndex])->dac7760Channels[dacChannelIndex];
                value = channel.m_outputEnabled;
            } else {
                value = 0;
            }
        }
    }
}

void data_dib_mio168_output_mode(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = g_dac7760ConfigurationPage.m_mode;
    }
}

void data_dib_mio168_voltage_range(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = getWidgetLabel(g_dac7760VoltageRangeEnumDefinition, g_dac7760ConfigurationPage.m_voltageRange);
    }
}

void data_dib_mio168_current_range(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = getWidgetLabel(g_dac7760CurrentRangeEnumDefinition, g_dac7760ConfigurationPage.m_currentRange);
    }
}

void action_dib_mio168_dac_toggle_output_enabled() {
    g_dac7760ConfigurationPage.m_outputEnabled = !g_dac7760ConfigurationPage.m_outputEnabled;
}

void onSetOutputMode(uint16_t value) {
    popPage();
    g_dac7760ConfigurationPage.m_mode = (SourceMode)value;
}

void action_dib_mio168_select_output_mode() {
    pushSelectFromEnumPage(g_dac7760OutputModeEnumDefinition, g_dac7760ConfigurationPage.m_mode, nullptr, onSetOutputMode);
}

void onSetVoltageRange(uint16_t value) {
    popPage();
    g_dac7760ConfigurationPage.m_voltageRange = (SourceMode)value;
}

void action_dib_mio168_select_voltage_range() {
    pushSelectFromEnumPage(g_dac7760VoltageRangeEnumDefinition, g_dac7760ConfigurationPage.m_voltageRange, nullptr, onSetVoltageRange);
}

void onSetCurrentRange(uint16_t value) {
    popPage();
    g_dac7760ConfigurationPage.m_currentRange = (SourceMode)value;
}

void action_dib_mio168_select_current_range() {
    pushSelectFromEnumPage(g_dac7760CurrentRangeEnumDefinition, g_dac7760ConfigurationPage.m_currentRange, nullptr, onSetCurrentRange);
}

void action_dib_mio168_show_dac_configuration() {
    int cursor = getFoundWidgetAtDown().cursor;
    
    int slotIndex = cursor / 4;
    hmi::selectSlot(slotIndex);
    
    int dacChannelIndex = cursor % 4;
    if (dacChannelIndex < 2) {
        Dac7760ConfigurationPage::g_selectedChannelIndex = DAC_7760_1_SUBCHANNEL_INDEX + dacChannelIndex;
        pushPage(PAGE_ID_DIB_MIO168_DAC_7760_CONFIGURATION);
    }
}

void action_dib_mio168_show_info() {
    pushPage(PAGE_ID_DIB_MIO168_INFO);
}

void data_dib_mio168_inputs_1_2(DataOperationEnum operation, Cursor cursor, Value &value) {
}

void data_dib_mio168_inputs_3_5(DataOperationEnum operation, Cursor cursor, Value &value) {
}

void data_dib_mio168_inputs_6_8(DataOperationEnum operation, Cursor cursor, Value &value) {
}

void data_dib_mio168_input_range(DataOperationEnum operation, Cursor cursor, Value &value) {
}

void data_dib_mio168_input_speed(DataOperationEnum operation, Cursor cursor, Value &value) {
}

void data_dib_mio168_adc_mode(DataOperationEnum operation, Cursor cursor, Value &value) {
}

void data_dib_mio168_adc_range(DataOperationEnum operation, Cursor cursor, Value &value) {
}

void data_dib_mio168_adc_has_temp_sensor_bias_feature(DataOperationEnum operation, Cursor cursor, Value &value) {
}

void data_dib_mio168_adc_temp_sensor_bias(DataOperationEnum operation, Cursor cursor, Value &value) {
}

void data_dib_mio168_dac_channel_has_settings(DataOperationEnum operation, Cursor cursor, Value &value) {
}

void data_dib_mio168_dac_channel_has_settings() {
    g_dac7760ConfigurationPage.m_outputEnabled = !g_dac7760ConfigurationPage.m_outputEnabled;
}

void action_dib_mio168_show_din_configuration() {
    g_dac7760ConfigurationPage.m_outputEnabled = !g_dac7760ConfigurationPage.m_outputEnabled;
}

void action_dib_mio168_adc_toggle_temp_sensor_bias() {
    g_dac7760ConfigurationPage.m_outputEnabled = !g_dac7760ConfigurationPage.m_outputEnabled;
}

void action_dib_mio168_show_adc_configuration() {
    g_dac7760ConfigurationPage.m_outputEnabled = !g_dac7760ConfigurationPage.m_outputEnabled;
}

void action_dib_mio168_show_dac_calibration() {
    g_dac7760ConfigurationPage.m_outputEnabled = !g_dac7760ConfigurationPage.m_outputEnabled;
}

void action_dib_mio168_select_adc_mode() {
    g_dac7760ConfigurationPage.m_outputEnabled = !g_dac7760ConfigurationPage.m_outputEnabled;
}

void action_dib_mio168_select_adc_range() {
    g_dac7760ConfigurationPage.m_outputEnabled = !g_dac7760ConfigurationPage.m_outputEnabled;
}

void action_dib_mio168_select_input_range() {
    g_dac7760ConfigurationPage.m_outputEnabled = !g_dac7760ConfigurationPage.m_outputEnabled;
}

void action_dib_mio168_select_input_speed() {
    g_dac7760ConfigurationPage.m_outputEnabled = !g_dac7760ConfigurationPage.m_outputEnabled;
}

} // namespace gui

} // namespace eez
