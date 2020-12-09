/*
 * EEZ DIB SMX46
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

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#if defined(EEZ_PLATFORM_STM32)
#include <spi.h>
#include <eez/platform/stm32/spi.h>
#endif

#include "eez/debug.h"
#include "eez/firmware.h"
#include <eez/system.h>
#include "eez/hmi.h"
#include "eez/util.h"
#include "eez/gui/document.h"
#include <eez/modules/psu/timer.h>
#include "eez/modules/psu/psu.h"
#include "eez/modules/psu/profile.h"
#include "eez/modules/psu/event_queue.h"
#include "eez/modules/psu/calibration.h"
#include "eez/modules/psu/gui/psu.h"
#include "eez/modules/psu/gui/animations.h"
#include "eez/modules/psu/gui/keypad.h"
#include "eez/modules/psu/gui/labels_and_colors.h"
#include "eez/modules/bp3c/comm.h"
#include "eez/modules/psu/gui/edit_mode.h"

#include "scpi/scpi.h"

#include "./dib-smx46.h"

using namespace eez::psu;
using namespace eez::psu::gui;
using namespace eez::gui;

namespace eez {
namespace dib_smx46 {

enum Smx46LowPriorityThreadMessage {
    THREAD_MESSAGE_SAVE_RELAY_CYCLES = THREAD_MESSAGE_MODULE_SPECIFIC
};

static const uint16_t MODULE_REVISION_R1B2  = 0x0102;

static const int32_t CONF_TRANSFER_TIMEOUT_MS = 1000;

static const uint32_t BUFFER_SIZE = 20;

static float U_CAL_POINTS[2] = { 1.0f, 9.0f };
static float DAC_MIN = 0.0f;
static float DAC_MAX = 10.0f;
static float DAC_RESOLUTION = 0.01f;
static float DAC_ENCODER_STEP_VALUES[] = { 0.01f, 0.1f, 0.2f, 0.5f  };

static const int NUM_COLUMNS = 6;
static const int NUM_ROWS = 4;

static const uint32_t MIN_TO_MS = 60L * 1000L;

struct FromMasterToSlave {
    uint32_t routes;
    float dac1;
    float dac2;
    uint8_t relayOn;
};

struct FromSlaveToMaster {
};

struct Smx46Module : public Module {
public:
    TestResult testResult = TEST_NONE;
    bool synchronized = false;
    int numCrcErrors = 0;
    uint32_t lastTransferTickCount;
    uint8_t input[BUFFER_SIZE];
    uint8_t output[BUFFER_SIZE];
    bool spiReady = false;

    uint32_t routes;

    char columnLabels[NUM_COLUMNS][MAX_SWITCH_MATRIX_LABEL_LENGTH + 1];
    char rowLabels[NUM_ROWS][MAX_SWITCH_MATRIX_LABEL_LENGTH + 1];

    float dac1;
    float dac2;
    bool calibrationEnabled[2];
    CalibrationConfiguration calConf[2];

    static const size_t CHANNEL_LABEL_MAX_LENGTH = 5;
    char aoChannelLabels[2][CHANNEL_LABEL_MAX_LENGTH + 1];

    bool relayOn;

    // relay cycles counting    
    uint32_t signalRelayCycles[NUM_ROWS][NUM_COLUMNS];
    uint32_t powerRelayCycles;
    uint32_t lastWrittenSignalRelayCycles[NUM_ROWS][NUM_COLUMNS];
    uint32_t lastWrittenPowerRelayCycles;
    Interval relayCyclesWriteInterval = WRITE_ONTIME_INTERVAL * MIN_TO_MS;

    Smx46Module() {
        moduleType = MODULE_TYPE_DIB_SMX46;
        moduleName = "SMX46";
        moduleBrand = "Envox";
        latestModuleRevision = MODULE_REVISION_R1B2;
        flashMethod = FLASH_METHOD_STM32_BOOTLOADER_UART;
#if defined(EEZ_PLATFORM_STM32)        
        spiBaudRatePrescaler = SPI_BAUDRATEPRESCALER_64;
        spiCrcCalculationEnable = true;
#else
        spiBaudRatePrescaler = 0;
        spiCrcCalculationEnable = false;
#endif
        numPowerChannels = 0;
        numOtherChannels = 2 + 1 + NUM_ROWS * NUM_COLUMNS;

        resetConfiguration();

        auto data = (FromMasterToSlave *)output;
        data->routes = routes;
        data->dac1 = dac1;
        data->dac2 = dac2;
        data->relayOn = relayOn;
    }

    void boot() override {
        Module::boot();

        loadRelayCylces();
        
        calibrationEnabled[0] = persist_conf::isChannelCalibrationEnabled(slotIndex, 0);
        calibrationEnabled[1] = persist_conf::isChannelCalibrationEnabled(slotIndex, 1);
    }

    Module *createModule() override {
        return new Smx46Module();
    }

    TestResult getTestResult() override {
        return testResult;
    }

    void initChannels() override {
        if (!synchronized) {
            if (bp3c::comm::masterSynchro(slotIndex)) {
                synchronized = true;
                numCrcErrors = 0;
                lastTransferTickCount = millis();
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
#endif
            FromMasterToSlave data;

            uint32_t newRoutes = routes;
            uint8_t newRelayOn = relayOn ? 1 : 0;

            data.routes = newRoutes;
            data.dac1 = calibrationEnabled[0] && isVoltageCalibrationExists(0) ? calibration::remapValue(dac1, calConf[0].u) : dac1;
            data.dac2 = calibrationEnabled[1] && isVoltageCalibrationExists(1) ? calibration::remapValue(dac2, calConf[1].u) : dac2;
            data.relayOn = newRelayOn;

            if (memcmp(output, &data, sizeof(FromMasterToSlave)) != 0) {
                FromMasterToSlave *oldData = (FromMasterToSlave *)output;

                uint32_t oldRoutes = oldData->routes;
                uint8_t oldRelayOn = oldData->relayOn;

            	memcpy(output, &data, sizeof(FromMasterToSlave));
                if (transfer()) {
                    updateRelayCycles(oldRoutes, newRoutes, oldRelayOn, newRelayOn);
                }
#if defined(EEZ_PLATFORM_STM32)
                spiReady = false;
#endif
            }
#if defined(EEZ_PLATFORM_STM32)
        } else {
            int32_t diff = millis() - lastTransferTickCount;
            if (diff > CONF_TRANSFER_TIMEOUT_MS) {
                event_queue::pushEvent(event_queue::EVENT_ERROR_SLOT1_SYNC_ERROR + slotIndex);
                synchronized = false;
                testResult = TEST_FAILED;
            }
        }
#endif

        if (relayCyclesWriteInterval.test(micros())) {
            sendMessageToLowPriorityThread((LowPriorityThreadMessage)THREAD_MESSAGE_SAVE_RELAY_CYCLES, slotIndex);
        }
    }

#if defined(EEZ_PLATFORM_STM32)
    void onSpiIrq() {
        spiReady = true;
    }
#endif

    bool transfer() {
#if defined(EEZ_PLATFORM_STM32)
        auto status = bp3c::comm::transfer(slotIndex, output, input, BUFFER_SIZE);
        if (status == bp3c::comm::TRANSFER_STATUS_OK) {
            numCrcErrors = 0;
            lastTransferTickCount = millis();
            return true;
        } else {
            if (status == bp3c::comm::TRANSFER_STATUS_CRC_ERROR) {
                if (++numCrcErrors >= 10) {
                    psu::event_queue::pushEvent(psu::event_queue::EVENT_ERROR_SLOT1_CRC_CHECK_ERROR + slotIndex);
                    synchronized = false;
                    testResult = TEST_FAILED;
                } else {
                    DebugTrace("Slot %d CRC %d\n", slotIndex + 1, numCrcErrors);
                }
            } else {
                DebugTrace("Slot %d SPI transfer error %d\n", slotIndex + 1, status);
            }
            return false;
        }
#endif
        return true;
    }

    void writeUnsavedData() override {
        saveRelayCycles();
    }

    void onPowerDown() override {
        synchronized = false;
    }

    Page *getPageFromId(int pageId) override;

    void animatePageAppearance(int previousPageId, int activePageId) override {
        if (previousPageId == PAGE_ID_MAIN && activePageId == PAGE_ID_DIB_SMX46_CONFIGURE_ROUTES) {
            animateSlideDown();
        } else if (previousPageId == PAGE_ID_DIB_SMX46_CONFIGURE_ROUTES && activePageId == PAGE_ID_MAIN) {
            animateSlideUp();
        }
    }

    int getSlotView(SlotViewType slotViewType, int slotIndex, int cursor) override {
        if (slotViewType == SLOT_VIEW_TYPE_DEFAULT) {
            return isDefaultViewVertical() ? PAGE_ID_DIB_SMX46_SLOT_VIEW_DEF : PAGE_ID_SLOT_DEF_HORZ_EMPTY;
        }
        if (slotViewType == SLOT_VIEW_TYPE_DEFAULT_2COL) {
            return isDefaultViewVertical() ? PAGE_ID_DIB_SMX46_SLOT_VIEW_DEF_2COL : PAGE_ID_SLOT_DEF_HORZ_EMPTY;
        }
        if (slotViewType == SLOT_VIEW_TYPE_MAX) {
            return PAGE_ID_DIB_SMX46_SLOT_VIEW_MAX;
        }
        if (slotViewType == SLOT_VIEW_TYPE_MIN) {
            return PAGE_ID_DIB_SMX46_SLOT_VIEW_MIN;
        }
        assert(slotViewType == SLOT_VIEW_TYPE_MICRO);
        return PAGE_ID_DIB_SMX46_SLOT_VIEW_MICRO;
    }

    struct ProfileParameters : public Module::ProfileParameters {
        unsigned int routes;
        char columnLabels[NUM_COLUMNS][MAX_SWITCH_MATRIX_LABEL_LENGTH + 1];
        char rowLabels[NUM_ROWS][MAX_SWITCH_MATRIX_LABEL_LENGTH + 1];
        float dac1;
        float dac2;
        char aoChannelLabels[2][CHANNEL_LABEL_MAX_LENGTH + 1];
        bool relayOn;
    };

    int getSlotSettingsPageId() override {
        return getTestResult() == TEST_OK ? PAGE_ID_DIB_SMX46_SETTINGS : PAGE_ID_SLOT_SETTINGS;
    }

    int getLabelsAndColorsPageId() override {
        return PAGE_ID_DIB_SMX46_LABELS_AND_COLORS;
    }

    void onLowPriorityThreadMessage(uint8_t type, uint32_t param) override {
        if (type == THREAD_MESSAGE_SAVE_RELAY_CYCLES) {
            saveRelayCycles();
        }
    }

    void resetProfileToDefaults(uint8_t *buffer) override {
        Module::resetProfileToDefaults(buffer);

        auto parameters = (ProfileParameters *)buffer;

        parameters->routes = 0;

        for (int i = 0; i < NUM_COLUMNS; i++) {
            parameters->columnLabels[i][0] = 'X';
            parameters->columnLabels[i][1] = '1' + i;
            parameters->columnLabels[i][2] = 0;
        }

        for (int i = 0; i < NUM_ROWS; i++) {
            parameters->rowLabels[i][0] = 'Y';
            parameters->rowLabels[i][1] = '1' + i;
            parameters->rowLabels[i][2] = 0;
        }

        parameters->dac1 = 0.0f;
        parameters->dac2 = 0.0f;

        strcpy(parameters->aoChannelLabels[0], "");
        strcpy(parameters->aoChannelLabels[1], "");

        parameters->relayOn = false;
    }

    void getProfileParameters(uint8_t *buffer) override {
        Module::getProfileParameters(buffer);

        assert(sizeof(ProfileParameters) < MAX_CHANNEL_PARAMETERS_SIZE);

        auto parameters = (ProfileParameters *)buffer;

        parameters->routes = routes;
        memcpy(&parameters->columnLabels[0][0], &columnLabels[0][0], sizeof(columnLabels));
        memcpy(&parameters->rowLabels[0][0], &rowLabels[0][0], sizeof(rowLabels));
        parameters->dac1 = dac1;
        parameters->dac2 = dac2;
        memcpy(&parameters->aoChannelLabels[0][0], &aoChannelLabels[0][0], sizeof(aoChannelLabels));
        parameters->relayOn = relayOn;
    }
    
    void setProfileParameters(uint8_t *buffer, bool mismatch, int recallOptions) override {
        Module::setProfileParameters(buffer, mismatch, recallOptions);

        auto parameters = (ProfileParameters *)buffer;

        routes = parameters->routes;
        memcpy(&columnLabels[0][0], &parameters->columnLabels[0][0], sizeof(columnLabels));
        memcpy(&rowLabels[0][0], &parameters->rowLabels[0][0], sizeof(rowLabels));
        dac1 = parameters->dac1;
        dac2 = parameters->dac2;
        memcpy(&aoChannelLabels[0][0], &parameters->aoChannelLabels[0][0], sizeof(aoChannelLabels));
        relayOn = parameters->relayOn;
    }
    
    bool writeProfileProperties(psu::profile::WriteContext &ctx, const uint8_t *buffer) override {
        if (!Module::writeProfileProperties(ctx, buffer)) {
            return false;
        }

        auto parameters = (const ProfileParameters *)buffer;
        
        WRITE_PROPERTY("routes", parameters->routes);

        for (int i = 0; i < NUM_COLUMNS; i++) {
            char propName[16];
            sprintf(propName, "xLabel%d", i+1);
            WRITE_PROPERTY(propName, parameters->columnLabels[i]);
        }

        for (int i = 0; i < NUM_ROWS; i++) {
            char propName[16];
            sprintf(propName, "yLabel%d", i+1);
            WRITE_PROPERTY(propName, parameters->rowLabels[i]);
        }

        WRITE_PROPERTY("dac1", parameters->dac1);
        WRITE_PROPERTY("dac2", parameters->dac2);

        WRITE_PROPERTY("ao1Label", parameters->aoChannelLabels[0]);
        WRITE_PROPERTY("ao2Label", parameters->aoChannelLabels[1]);

        WRITE_PROPERTY("relayOn", parameters->relayOn);

        return true;
    }
    
    bool readProfileProperties(psu::profile::ReadContext &ctx, uint8_t *buffer) override {
        if (Module::readProfileProperties(ctx, buffer)) {
            return true;
        }

        auto parameters = (ProfileParameters *)buffer;

        READ_PROPERTY("routes", parameters->routes);
        
        for (int i = 0; i < NUM_COLUMNS; i++) {
            char propName[16];
            sprintf(propName, "xLabel%d", i+1);
            READ_STRING_PROPERTY(propName, parameters->columnLabels[i], MAX_SWITCH_MATRIX_LABEL_LENGTH);
        }

        for (int i = 0; i < NUM_ROWS; i++) {
            char propName[16];
            sprintf(propName, "yLabel%d", i+1);
            READ_STRING_PROPERTY(propName, parameters->rowLabels[i], MAX_SWITCH_MATRIX_LABEL_LENGTH);
        }

        READ_PROPERTY("dac1", parameters->dac1);
        READ_PROPERTY("dac2", parameters->dac2);

        READ_STRING_PROPERTY("ao1Label", parameters->aoChannelLabels[0], CHANNEL_LABEL_MAX_LENGTH);
        READ_STRING_PROPERTY("ao2Label", parameters->aoChannelLabels[1], CHANNEL_LABEL_MAX_LENGTH);

        READ_PROPERTY("relayOn", parameters->relayOn);

        return false;
    }

    void resetConfiguration() {
        Module::resetConfiguration();

        routes = 0;

        for (int i = 0; i < NUM_COLUMNS; i++) {
            columnLabels[i][0] = 'X';
            columnLabels[i][1] = '1' + i;
            columnLabels[i][2] = 0;
        }

        for (int i = 0; i < NUM_ROWS; i++) {
            rowLabels[i][0] = 'Y';
            rowLabels[i][1] = '1' + i;
            rowLabels[i][2] = 0;
        }

        dac1 = 0.0f;
        dac2 = 0.0f;

        strcpy(aoChannelLabels[0], "");
        strcpy(aoChannelLabels[1], "");

        relayOn = false;
    }

    bool isValidSubchannelIndex(int subchannelIndex) override {
        subchannelIndex++;
        return subchannelIndex == 1 || subchannelIndex == 2 || subchannelIndex == 3 ||
            (subchannelIndex >= 11 && subchannelIndex <= 16) ||
            (subchannelIndex >= 21 && subchannelIndex <= 26) ||
            (subchannelIndex >= 31 && subchannelIndex <= 36) ||
            (subchannelIndex >= 41 && subchannelIndex <= 46);
    }

    int getSubchannelIndexFromRelativeChannelIndex(int relativeChannelIndex) override {
        int subchannelIndex;
        if (relativeChannelIndex >= 21) {
            subchannelIndex = 40 + (relativeChannelIndex - 21);
        } else if (relativeChannelIndex >= 15) {
            subchannelIndex = 30 + (relativeChannelIndex - 15);
        } else  if (relativeChannelIndex >= 9) {
            subchannelIndex = 20 + (relativeChannelIndex - 9);
        } else if (relativeChannelIndex >= 3) {
            subchannelIndex = 10 + (relativeChannelIndex - 3);
        } else {
            subchannelIndex = relativeChannelIndex;
        }
        return subchannelIndex;
    }

    size_t getChannelLabelMaxLength(int subchannelIndex) override {
        return CHANNEL_LABEL_MAX_LENGTH;
    }

    eez_err_t getChannelLabel(int subchannelIndex, const char *&label) override {
        if (subchannelIndex == 0) {
            label = aoChannelLabels[0];
            return SCPI_RES_OK;
        } 
        
        if (subchannelIndex == 1) {
            label = aoChannelLabels[1];
            return SCPI_RES_OK;
        }
        
        return SCPI_ERROR_HARDWARE_MISSING;
    }

    const char *getChannelLabel(int subchannelIndex) override {
        const char *label;
        auto err = getChannelLabel(subchannelIndex, label);
        if (err == SCPI_RES_OK) {
            return label;
        } 
        return "";
    }

    const char *getDefaultChannelLabel(int subchannelIndex) override {
        if (subchannelIndex == 0) {
            return "AO1";
        } 
        if (subchannelIndex == 1) {
            return "AO2";
        }
        return "";
    }

    eez_err_t setChannelLabel(int subchannelIndex, const char *label, int length) override {
        if (length == -1) {
            length = strlen(label);
        }
        if (length > (int)CHANNEL_LABEL_MAX_LENGTH) {
            length = CHANNEL_LABEL_MAX_LENGTH;
        }

        if (subchannelIndex == 0) {
            strncpy(aoChannelLabels[0], label, length);
            aoChannelLabels[0][length] = 0;
            return SCPI_RES_OK;
        }
        
        if (subchannelIndex == 1) {
            strcpy(aoChannelLabels[1], label);
            aoChannelLabels[1][length] = 0;
            return SCPI_RES_OK;
        }

        return SCPI_ERROR_HARDWARE_MISSING;
    }

    bool isRouteOpen(int subchannelIndex, bool &isRouteOpen_, int *err) override {
        subchannelIndex++;

        if (subchannelIndex == 3) {
            isRouteOpen_ = !relayOn;
        } else {
            int x = subchannelIndex % 10 - 1;
            int y = subchannelIndex / 10 - 1;

            if (x < 0 || x >= NUM_COLUMNS || y < 0 || y >= NUM_ROWS) {
                if (err) {
                    *err = SCPI_ERROR_ILLEGAL_PARAMETER_VALUE;
                }
                return false;
            }

            isRouteOpen_ = isRouteOpen(x, y);
        }

        return true;
    }

    bool routeOpen(ChannelList channelList, int *err) override {
        uint32_t tempRoutes = routes;
        bool tempRelayOn = relayOn;
        for (int i = 0; i < channelList.numChannels; i++) {
            int subchannelIndex = channelList.channels[i].subchannelIndex + 1;
            if (subchannelIndex == 3) {
                tempRelayOn = false;
                continue;
            }

            int x = subchannelIndex % 10 - 1;
            int y = subchannelIndex / 10 - 1;

            if (x < 0 || x >= NUM_COLUMNS || y < 0 || y >= NUM_ROWS) {
                if (err) {
                    *err = SCPI_ERROR_ILLEGAL_PARAMETER_VALUE;
                }
                return false;
            }

            tempRoutes &= ~(1 << (y * NUM_COLUMNS + x));
        }
        routes = tempRoutes;
        relayOn = tempRelayOn;
        return true;
    }
    
    bool routeClose(ChannelList channelList, int *err) override {
        uint32_t tempRoutes = routes;
        bool tempRelayOn = relayOn;
        for (int i = 0; i < channelList.numChannels; i++) {
            int subchannelIndex = channelList.channels[i].subchannelIndex + 1;
            if (subchannelIndex == 3) {
                tempRelayOn = true;
                continue;
            }
            
            int x = subchannelIndex % 10 - 1;
            int y = subchannelIndex / 10 - 1;

            if (x < 0 || x >= NUM_COLUMNS || y < 0 || y >= NUM_ROWS) {
                if (err) {
                    *err = SCPI_ERROR_ILLEGAL_PARAMETER_VALUE;
                }
                return false;
            }

            tempRoutes |= (1 << (y * NUM_COLUMNS + x));
        }
        routes = tempRoutes;
        relayOn = tempRelayOn;
        return true;
    }

    bool getSwitchMatrixNumRows(int &numRows, int *err) override {
        numRows = NUM_ROWS;
        return true;
    }

    bool getSwitchMatrixNumColumns(int &numRows, int *err) override {
        numRows = NUM_COLUMNS;
        return true;
    }

    bool setSwitchMatrixRowLabel(int rowIndex, const char *label, int *err) override {
        strcpy(rowLabels[rowIndex], label);
        sendMessageToGuiThread(GUI_QUEUE_MESSAGE_REFRESH_SCREEN);
        return true;
    }
    
    bool getSwitchMatrixRowLabel(int rowIndex, char *label, int *err) override {
        strcpy(label, rowLabels[rowIndex]);
        return true;
    }

    bool setSwitchMatrixColumnLabel(int columnIndex, const char *label, int *err) override {
        strcpy(columnLabels[columnIndex], label);
        sendMessageToGuiThread(GUI_QUEUE_MESSAGE_REFRESH_SCREEN);
        return true;
    }
    
    bool getSwitchMatrixColumnLabel(int columnIndex, char *label, int *err) override {
        strcpy(label, columnLabels[columnIndex]);
        return true;
    }

    bool getRelayCycles(int subchannelIndex, uint32_t &relayCycles, int *err) override {
        subchannelIndex++;

        if (subchannelIndex == 3) {
            relayCycles = powerRelayCycles;
        } else {
            int x = subchannelIndex % 10 - 1;
            int y = subchannelIndex / 10 - 1;

            if (x < 0 || x >= NUM_COLUMNS || y < 0 || y >= NUM_ROWS) {
                if (err) {
                    *err = SCPI_ERROR_ILLEGAL_PARAMETER_VALUE;
                }
                return false;
            }

            relayCycles = signalRelayCycles[y][x];
        }

        return true;
    }

    bool getVoltage(int subchannelIndex, float &value, int *err) override {
        ++subchannelIndex;

        if (subchannelIndex != 1 && subchannelIndex != 2) {
            if (err) {
                *err = SCPI_ERROR_HARDWARE_MISSING;
            }
            return false;
        }

        if (subchannelIndex == 1) {
            value = dac1;
        } else {
            value = dac2;
        }

        return true;
    }

    bool setVoltage(int subchannelIndex, float value, int *err) override {
        ++subchannelIndex;

        if (subchannelIndex != 1 && subchannelIndex != 2) {
            if (err) {
                *err = SCPI_ERROR_HARDWARE_MISSING;
            }
            return false;
        }

        if (value < DAC_MIN || value > DAC_MAX) {
            if (err) {
                *err = SCPI_ERROR_VOLTAGE_LIMIT_EXCEEDED;
            }
            return false;
        }

        if (subchannelIndex == 1) {
            dac1 = value;
        } else {
            dac2 = value;
        }

        return true;
    }

    void getVoltageStepValues(int subchannelIndex, StepValues *stepValues, bool calibrationMode) override {
        stepValues->values = DAC_ENCODER_STEP_VALUES;
        stepValues->count = sizeof(DAC_ENCODER_STEP_VALUES) / sizeof(float);
        stepValues->unit = UNIT_VOLT;

		stepValues->encoderSettings.accelerationEnabled = true;
		stepValues->encoderSettings.range = DAC_MAX - DAC_MIN;
		stepValues->encoderSettings.step = stepValues->values[0];
        if (calibrationMode) {
            stepValues->encoderSettings.step /= 10.0f;
            stepValues->encoderSettings.range = stepValues->encoderSettings.step * 10.0f;
        }
    }
    
    float getVoltageResolution(int subchannelIndex) override {
        return DAC_RESOLUTION;
    }

    float getVoltageMinValue(int subchannelIndex) override {
        return DAC_MIN;
    }

    float getVoltageMaxValue(int subchannelIndex) override {
        return DAC_MAX;
    }

    bool isConstantVoltageMode(int subchannelIndex) override {
        return true;
    }

    bool isVoltageCalibrationExists(int subchannelIndex) override {
        return subchannelIndex < 2 && calConf[subchannelIndex].u.numPoints > 1;
    }

    bool isVoltageCalibrationEnabled(int subchannelIndex) override {
        return subchannelIndex < 2 && calibrationEnabled[subchannelIndex];
    }
    
    void enableVoltageCalibration(int subchannelIndex, bool enabled) override {
        if (subchannelIndex < 2) {
            calibrationEnabled[subchannelIndex] = enabled;
            persist_conf::saveCalibrationEnabledFlag(slotIndex, subchannelIndex, enabled);
        }
    }

    bool loadChannelCalibration(int subchannelIndex, int *err) override {
        if (subchannelIndex == 0 || subchannelIndex == 1) {
            persist_conf::loadChannelCalibrationConfiguration(slotIndex, subchannelIndex, calConf[subchannelIndex]);
            return true;
        }
        
        return true;
    }

    bool saveChannelCalibration(int subchannelIndex, int *err) override {
        if (subchannelIndex == 0 || subchannelIndex == 1) {
            return persist_conf::saveChannelCalibrationConfiguration(slotIndex, subchannelIndex, calConf[subchannelIndex]);
        }
      
        if (err) {
            *err = SCPI_ERROR_HARDWARE_MISSING;
        }
        return false;
    }

    void getDefaultCalibrationPoints(int subchannelIndex, CalibrationValueType type, unsigned int &numPoints, float *&points) override {
        points = U_CAL_POINTS;
        numPoints = sizeof(U_CAL_POINTS) / sizeof(float);
    }

    bool getCalibrationConfiguration(int subchannelIndex, CalibrationConfiguration &calConf, int *err) override {
        if (subchannelIndex == 0) {
            memcpy(&calConf, &this->calConf[0], sizeof(CalibrationConfiguration));
            return true;
        } else if (subchannelIndex == 1) {
            memcpy(&calConf, &this->calConf[1], sizeof(CalibrationConfiguration));
            return true;
        }

        if (err) {
            *err = SCPI_ERROR_HARDWARE_MISSING;
        }
        return false;
    }

    bool setCalibrationConfiguration(int subchannelIndex, const CalibrationConfiguration &calConf, int *err) override {
        if (subchannelIndex == 0) {
            memcpy(&this->calConf[0], &calConf, sizeof(CalibrationConfiguration));
            return true;
        } else if (subchannelIndex == 1) {
            memcpy(&this->calConf[1], &calConf, sizeof(CalibrationConfiguration));
            return true;
        }

        if (err) {
            *err = SCPI_ERROR_HARDWARE_MISSING;
        }
        return false;
    }

    bool getCalibrationRemark(int subchannelIndex, const char *&calibrationRemark, int *err) override {
        if (subchannelIndex == 0) {
            calibrationRemark = calConf[0].calibrationRemark;
            return true;
        } else if (subchannelIndex == 1) {
            calibrationRemark = calConf[1].calibrationRemark;
            return true;
        }

        if (err) {
            *err = SCPI_ERROR_HARDWARE_MISSING;
        }
        return false;
    }

    bool getCalibrationDate(int subchannelIndex, uint32_t &calibrationDate, int *err) override {
        if (subchannelIndex == 0) {
            calibrationDate = calConf[0].calibrationDate;
            return true;
        } else if (subchannelIndex == 1) {
            calibrationDate = calConf[1].calibrationDate;
            return true;
        }

        if (err) {
            *err = SCPI_ERROR_HARDWARE_MISSING;
        }
        return false;
    }

    bool isRouteOpen(int x, int y) {
        return !(routes & (1 << (y * NUM_COLUMNS + x)));
    }

    void toggleRoute(int x, int y) {
        routes ^= 1 << (y * NUM_COLUMNS + x);
    }

    void updateRelayCycles(uint32_t oldRoutes, uint32_t newRoutes, uint8_t oldRelayOn, uint8_t newRelayOn) {
        for (int y = 0; y < NUM_ROWS; y++) {
            for (int x = 0; x < NUM_COLUMNS; x++) {
                if (!(oldRoutes & (1 << (y * NUM_COLUMNS + x))) && (newRoutes & (1 << (y * NUM_COLUMNS + x)))) {
                    signalRelayCycles[y][x]++;
                }
            }
        }

        if (!oldRelayOn && newRelayOn) {
            powerRelayCycles++;
        }
    }

    void loadRelayCylces() {
        powerRelayCycles = lastWrittenPowerRelayCycles = persist_conf::readCounter(slotIndex, 0);

        for (int y = 0; y < NUM_ROWS; y++) {
            for (int x = 0; x < NUM_COLUMNS; x++) {
                signalRelayCycles[y][x] = lastWrittenSignalRelayCycles[y][x] = persist_conf::readCounter(slotIndex, 1 + y * NUM_COLUMNS + x);
            }
        }
    }

    void saveRelayCycles() {
        if (powerRelayCycles != lastWrittenPowerRelayCycles) {
            persist_conf::writeCounter(slotIndex, 0, powerRelayCycles);
            lastWrittenPowerRelayCycles = powerRelayCycles;
        }

        for (int y = 0; y < NUM_ROWS; y++) {
            for (int x = 0; x < NUM_COLUMNS; x++) {
                if (signalRelayCycles[y][x] != lastWrittenSignalRelayCycles[y][x]) {
                    persist_conf::writeCounter(slotIndex, 1 + y * NUM_COLUMNS + x, signalRelayCycles[y][x]);
                    lastWrittenSignalRelayCycles[y][x] = signalRelayCycles[y][x];
                }
            }
        }
    }
};

static Smx46Module g_smx46Module;
Module *g_module = &g_smx46Module;

////////////////////////////////////////////////////////////////////////////////

class ConfigureRoutesPage : public SetPage {
public:
    void pageAlloc() {
        Smx46Module *module = (Smx46Module *)g_slots[hmi::g_selectedSlotIndex];

        memcpy(columnLabels, module->columnLabels, sizeof(columnLabels));
        memcpy(columnLabelsOrig, module->columnLabels, sizeof(columnLabels));

        memcpy(rowLabels, module->rowLabels, sizeof(rowLabels));
        memcpy(rowLabelsOrig, module->rowLabels, sizeof(rowLabels));

        routes = routesOrig = module->routes;
    }

    int getDirty() { 
        return memcmp(columnLabels, columnLabelsOrig, sizeof(columnLabels)) != 0 || 
            memcmp(rowLabels, rowLabelsOrig, sizeof(rowLabels)) != 0 ||
            routes != routesOrig;
    }

    void set() {
        if (getDirty()) {
            Smx46Module *module = (Smx46Module *)g_slots[hmi::g_selectedSlotIndex];

            memcpy(module->columnLabels, columnLabels, sizeof(columnLabels));
            memcpy(module->rowLabels, rowLabels, sizeof(rowLabels));
            
            module->routes = routes;
        }

        popPage();
    }

    bool isRouteOpen(int x, int y) {
        return !(routes & (1 << (y * NUM_COLUMNS + x)));
    }

    void toggleRoute(int x, int y) {
        routes ^= 1 << (y * NUM_COLUMNS + x);
    }

    char columnLabels[NUM_COLUMNS][Module::MAX_SWITCH_MATRIX_LABEL_LENGTH + 1];
    char rowLabels[NUM_COLUMNS][Module::MAX_SWITCH_MATRIX_LABEL_LENGTH + 1];
    uint32_t routes = 0;

private:
    char columnLabelsOrig[NUM_COLUMNS][Module::MAX_SWITCH_MATRIX_LABEL_LENGTH + 1];
    char rowLabelsOrig[NUM_COLUMNS][Module::MAX_SWITCH_MATRIX_LABEL_LENGTH + 1];
    uint32_t routesOrig = 0;
};

static ConfigureRoutesPage g_ConfigureRoutesPage;

Page *Smx46Module::getPageFromId(int pageId) {
    if (pageId == PAGE_ID_DIB_SMX46_CONFIGURE_ROUTES) {
        return &g_ConfigureRoutesPage;
    }
    return nullptr;
}

} // namespace dib_smx46

namespace gui {

using namespace eez::dib_smx46;

void data_dib_smx46_routes(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_COUNT) {
        value = NUM_COLUMNS * NUM_ROWS;
    } else if (operation == DATA_OPERATION_GET_CURSOR_VALUE) {
        value = hmi::g_selectedSlotIndex * (NUM_COLUMNS * NUM_ROWS) + value.getInt();
    }
}

void data_dib_smx46_route_open(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        int slotIndex = cursor / (NUM_COLUMNS * NUM_ROWS);
        int i = cursor % (NUM_COLUMNS * NUM_ROWS);
        int x = i % NUM_COLUMNS;
        int y = i / NUM_COLUMNS;
        ConfigureRoutesPage *page = (ConfigureRoutesPage *)getPage(PAGE_ID_DIB_SMX46_CONFIGURE_ROUTES);
        if (page) {
            value = page->isRouteOpen(x, y);
        } else {
            value = ((Smx46Module *)g_slots[slotIndex])->isRouteOpen(x, y);
        }
    }
}

void data_dib_smx46_x_labels(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_COUNT) {
        value = NUM_COLUMNS;
    } else if (operation == DATA_OPERATION_GET_CURSOR_VALUE) {
        value = hmi::g_selectedSlotIndex * NUM_COLUMNS + value.getInt();
    }
}

void data_dib_smx46_x_label(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        int slotIndex = cursor / NUM_COLUMNS;
        int i = cursor % NUM_COLUMNS;
        ConfigureRoutesPage *page = (ConfigureRoutesPage *)getPage(PAGE_ID_DIB_SMX46_CONFIGURE_ROUTES);
        if (page) {
            value = (const char *)&page->columnLabels[i][0];
        } else {
            value = (const char *)&((Smx46Module *)g_slots[slotIndex])->columnLabels[i][0];
        }
    }
}

void data_dib_smx46_y_labels(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_COUNT) {
        value = NUM_ROWS;
    } else if (operation == DATA_OPERATION_GET_CURSOR_VALUE) {
        value = hmi::g_selectedSlotIndex * NUM_ROWS + value.getInt();
    }
}

void data_dib_smx46_y_label(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        int slotIndex = cursor / NUM_ROWS;
        int i = cursor % NUM_ROWS;
        ConfigureRoutesPage *page = (ConfigureRoutesPage *)getPage(PAGE_ID_DIB_SMX46_CONFIGURE_ROUTES);
        if (page) {
            value = (const char *)&page->rowLabels[i][0];
        } else {
            value = (const char *)&((Smx46Module *)g_slots[slotIndex])->rowLabels[i][0];
        }
    }
}

void data_dib_smx46_dac1(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        bool focused = g_focusCursor == cursor && g_focusDataId == DATA_ID_DIB_SMX46_DAC1;
        if (focused && g_focusEditValue.getType() != VALUE_TYPE_NONE) {
            value = g_focusEditValue;
        } else if (focused && getActivePageId() == PAGE_ID_EDIT_MODE_KEYPAD && edit_mode_keypad::g_keypad->isEditing()) {
            data_keypad_text(operation, cursor, value);
        } else {
            value = MakeValue(((Smx46Module *)g_slots[cursor])->dac1, UNIT_VOLT);
        }
    } else if (operation == DATA_OPERATION_GET_MIN) {
        value = MakeValue(DAC_MIN, UNIT_VOLT);
    } else if (operation == DATA_OPERATION_GET_MAX) {
        value = MakeValue(DAC_MAX, UNIT_VOLT);
    }else if (operation == DATA_OPERATION_GET_NAME) {
        value = ((Smx46Module *)g_slots[cursor])->aoChannelLabels[0];
    }  else if (operation == DATA_OPERATION_GET_UNIT) {
        value = UNIT_VOLT;
    } else if (operation == DATA_OPERATION_GET_ENCODER_STEP_VALUES) {
        StepValues *stepValues = value.getStepValues();

        stepValues->values = DAC_ENCODER_STEP_VALUES;
        stepValues->count = sizeof(DAC_ENCODER_STEP_VALUES) / sizeof(float);
        stepValues->unit = UNIT_VOLT;

        stepValues->encoderSettings.accelerationEnabled = true;
        stepValues->encoderSettings.range = DAC_MAX - DAC_MIN;
        stepValues->encoderSettings.step = stepValues->values[0];

        value = 1;
    } else if (operation == DATA_OPERATION_SET) {
        ((Smx46Module *)g_slots[cursor])->dac1 = roundPrec(value.getFloat(), DAC_RESOLUTION);
    }
}

void data_dib_smx46_dac2(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        bool focused = g_focusCursor == cursor && g_focusDataId == DATA_ID_DIB_SMX46_DAC2;
        if (focused && g_focusEditValue.getType() != VALUE_TYPE_NONE) {
            value = g_focusEditValue;
        } else if (focused && getActivePageId() == PAGE_ID_EDIT_MODE_KEYPAD && edit_mode_keypad::g_keypad->isEditing()) {
            data_keypad_text(operation, cursor, value);
        } else {
            value = MakeValue(((Smx46Module *)g_slots[cursor])->dac2, UNIT_VOLT);
        }
    } else if (operation == DATA_OPERATION_GET_MIN) {
        value = MakeValue(DAC_MIN, UNIT_VOLT);
    } else if (operation == DATA_OPERATION_GET_MAX) {
        value = MakeValue(DAC_MAX, UNIT_VOLT);
    } else if (operation == DATA_OPERATION_GET_NAME) {
        value = ((Smx46Module *)g_slots[cursor])->aoChannelLabels[0];
    } else if (operation == DATA_OPERATION_GET_UNIT) {
        value = UNIT_VOLT;
    } else if (operation == DATA_OPERATION_GET_ENCODER_STEP_VALUES) {
        StepValues *stepValues = value.getStepValues();

        stepValues->values = DAC_ENCODER_STEP_VALUES;
        stepValues->count = sizeof(DAC_ENCODER_STEP_VALUES) / sizeof(float);
        stepValues->unit = UNIT_VOLT;

        stepValues->encoderSettings.accelerationEnabled = true;
        stepValues->encoderSettings.range = DAC_MAX - DAC_MIN;
        stepValues->encoderSettings.step = stepValues->values[0];

        value = 1;
    } else if (operation == DATA_OPERATION_SET) {
        ((Smx46Module *)g_slots[cursor])->dac2 = roundPrec(value.getFloat(), DAC_RESOLUTION);
    }
}

void data_dib_smx46_analog_outputs(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_COUNT) {
        value = 2;
    } else if (operation == DATA_OPERATION_GET_CURSOR_VALUE) {
        value = hmi::g_selectedSlotIndex * 2 + value.getInt();
    } 
}

void data_dib_smx46_ao_label(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        int slotIndex = cursor / 2;
        int subchannelIndex = cursor % 2;
        const char *label = LabelsAndColorsPage::getChannelLabel(slotIndex, subchannelIndex);
        if (*label) {
            value = label;
        } else {
            value = g_slots[slotIndex]->getDefaultChannelLabel(subchannelIndex);
        }
    }
}

void data_dib_smx46_ao1_label(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        const char *label = ((Smx46Module *)g_slots[cursor])->aoChannelLabels[0];
        if (*label) {
            value = label;
        } else {
            value = ((Smx46Module *)g_slots[cursor])->getDefaultChannelLabel(0);
        }
    }
}

void data_dib_smx46_ao2_label(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        const char *label = ((Smx46Module *)g_slots[cursor])->aoChannelLabels[1];
        if (*label) {
            value = label;
        } else {
            value = ((Smx46Module *)g_slots[cursor])->getDefaultChannelLabel(1);
        }
    }
}

void data_dib_smx46_relay_on(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = ((Smx46Module *)g_slots[cursor])->relayOn ? 1 : 0;
    }
}

void data_dib_smx46_signal_relay_cycles(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        int i = cursor % (NUM_COLUMNS * NUM_ROWS);
        int x = i % NUM_COLUMNS;
        int y = i / NUM_COLUMNS;
        value = Value(((Smx46Module *)g_slots[hmi::g_selectedSlotIndex])->signalRelayCycles[y][x], VALUE_TYPE_UINT32);
    }
}

void data_dib_smx46_power_relay_cycles(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = Value(((Smx46Module *)g_slots[hmi::g_selectedSlotIndex])->powerRelayCycles, VALUE_TYPE_UINT32);
    }
}

void action_dib_smx46_show_configure_routes() {
    hmi::selectSlot(getFoundWidgetAtDown().cursor);
    pushPage(PAGE_ID_DIB_SMX46_CONFIGURE_ROUTES);
}

void action_dib_smx46_toggle_route() {
    int cursor = getFoundWidgetAtDown().cursor;
    int i = cursor % (NUM_COLUMNS * NUM_ROWS);
    int x = i % NUM_COLUMNS;
    int y = i / NUM_COLUMNS;
    ConfigureRoutesPage *page = (ConfigureRoutesPage *)getPage(PAGE_ID_DIB_SMX46_CONFIGURE_ROUTES);
    page->toggleRoute(x, y);
}

void action_dib_smx46_toggle_relay() {
    int cursor = getFoundWidgetAtDown().cursor;
    Smx46Module *module = (Smx46Module *)g_slots[cursor];
    module->relayOn = !module->relayOn;
}

static char *g_labelPointer;

void onSetLabel(char *value) {
    strcpy(g_labelPointer, value);
    popPage();
}

void action_dib_smx46_edit_x_label() {
    int cursor = getFoundWidgetAtDown().cursor;
    int i = cursor % NUM_COLUMNS;
    ConfigureRoutesPage *page = (ConfigureRoutesPage *)getPage(PAGE_ID_DIB_SMX46_CONFIGURE_ROUTES);
    g_labelPointer = &page->columnLabels[i][0];
    Keypad::startPush("Label: ", g_labelPointer, 1, Module::MAX_SWITCH_MATRIX_LABEL_LENGTH, false, onSetLabel, popPage);
}

void action_dib_smx46_edit_y_label() {
    int cursor = getFoundWidgetAtDown().cursor;
    int i = cursor % NUM_ROWS;
    ConfigureRoutesPage *page = (ConfigureRoutesPage *)getPage(PAGE_ID_DIB_SMX46_CONFIGURE_ROUTES);
    g_labelPointer = &page->rowLabels[i][0];
    Keypad::startPush("Label: ", g_labelPointer, 1, Module::MAX_SWITCH_MATRIX_LABEL_LENGTH, false, onSetLabel, popPage);
}

void action_dib_smx46_clear_all_routes() {
    ConfigureRoutesPage *page = (ConfigureRoutesPage *)getPage(PAGE_ID_DIB_SMX46_CONFIGURE_ROUTES);
    page->routes = 0;
}

void action_dib_smx46_clear_all_labels() {
    ConfigureRoutesPage *page = (ConfigureRoutesPage *)getPage(PAGE_ID_DIB_SMX46_CONFIGURE_ROUTES);

    for (int i = 0; i < NUM_COLUMNS; i++) {
        page->columnLabels[i][0] = 'X';
        page->columnLabels[i][1] = '1' + i;
        page->columnLabels[i][2] = 0;
    }

    for (int i = 0; i < NUM_ROWS; i++) {
        page->rowLabels[i][0] = 'Y';
        page->rowLabels[i][1] = '1' + i;
        page->rowLabels[i][2] = 0;
    }

    refreshScreen();
}

void action_dib_smx46_show_dac1_calibration() {
    hmi::g_selectedSubchannelIndex = 0;
    calibration::g_viewer.start(hmi::g_selectedSlotIndex, hmi::g_selectedSubchannelIndex);
    pushPage(PAGE_ID_CH_SETTINGS_CALIBRATION);
}

void action_dib_smx46_show_dac2_calibration() {
    hmi::g_selectedSubchannelIndex = 1;
    calibration::g_viewer.start(hmi::g_selectedSlotIndex, hmi::g_selectedSubchannelIndex);
    pushPage(PAGE_ID_CH_SETTINGS_CALIBRATION);
}

void action_dib_smx46_change_subchannel_label() {
    int cursor = getFoundWidgetAtDown().cursor;
    int slotIndex = cursor / 2;
    int subchannelIndex = cursor % 2;
    LabelsAndColorsPage::editChannelLabel(slotIndex, subchannelIndex);
}

void action_dib_smx46_show_info() {
    pushPage(PAGE_ID_DIB_SMX46_INFO);
}

void action_dib_smx46_show_relay_cycles() {
    pushPage(PAGE_ID_DIB_SMX46_RELAY_CYCLES);
}

} // gui

} // namespace eez
