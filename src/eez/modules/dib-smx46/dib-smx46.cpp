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
#include "eez/modules/bp3c/comm.h"
#include "eez/modules/psu/gui/edit_mode.h"

#include "scpi/scpi.h"

#include "./dib-smx46.h"

using namespace eez::psu;
using namespace eez::psu::gui;
using namespace eez::gui;

namespace eez {
namespace dib_smx46 {

static const uint16_t MODULE_REVISION_R1B2  = 0x0102;

static const int32_t CONF_TRANSFER_TIMEOUT_MS = 1000;

static const uint32_t BUFFER_SIZE = 20;

static float U_CAL_POINTS[2] = { 1.0f, 9.0f };
static float DAC_MIN = 0.0f;
static float DAC_MAX = 10.0f;
static float DAC_RESOLUTION = 0.01f;
static float DAC_ENCODER_STEP_VALUES[] = { 0.5f, 0.2f, 0.1f, 0.01f };

static const int NUM_COLUMNS = 6;
static const int NUM_ROWS = 4;
static const int MAX_LABEL_LENGTH = 5;


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

    uint32_t routes = 0;

    char xLabels[NUM_COLUMNS][MAX_LABEL_LENGTH + 1];
    char yLabels[NUM_ROWS][MAX_LABEL_LENGTH + 1];

    float dac1 = 0.0f;
    float dac2 = 0.0f;
    bool calibrationEnabled[2] = { true, true };
    CalibrationConfiguration calConf[2];

    bool relayOn = false;
    
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
        flashDuration = 0;
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
        data->routes = 0xFFFFFFFF;

        memset(calConf, 0, sizeof(calConf));

        memset(signalRelayCycles, 0, NUM_ROWS * NUM_COLUMNS * sizeof(uint32_t));
        powerRelayCycles = 0;
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
            saveRelayCycles();
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

    struct ProfileParameters {
        uint32_t routes;
        char xLabels[NUM_COLUMNS][MAX_LABEL_LENGTH + 1];
        char yLabels[NUM_ROWS][MAX_LABEL_LENGTH + 1];
        float dac1;
        float dac2;
        bool relayOn;
    };

    int getSlotSettingsPageId() override {
        return getTestResult() == TEST_OK ? PAGE_ID_DIB_SMX46_SETTINGS : PAGE_ID_SLOT_SETTINGS;
    }

    void getProfileParameters(uint8_t *buffer) override {
        assert(sizeof(ProfileParameters) < MAX_CHANNEL_PARAMETERS_SIZE);

        auto parameters = (ProfileParameters *)buffer;

        parameters->routes = routes;
        memcpy(&parameters->xLabels[0][0], &xLabels[0][0], sizeof(xLabels));
        memcpy(&parameters->yLabels[0][0], &yLabels[0][0], sizeof(yLabels));
        parameters->dac1 = dac1;
        parameters->dac2 = dac2;
        parameters->relayOn = relayOn;
    }
    
    void setProfileParameters(uint8_t *buffer, bool mismatch, int recallOptions) override {
        auto parameters = (ProfileParameters *)buffer;

        routes = parameters->routes;
        memcpy(&xLabels[0][0], &parameters->xLabels[0][0], sizeof(xLabels));
        memcpy(&yLabels[0][0], &parameters->yLabels[0][0], sizeof(yLabels));
        dac1 = parameters->dac1;
        dac2 = parameters->dac2;
        relayOn = parameters->relayOn;
    }
    
    bool writeProfileProperties(psu::profile::WriteContext &ctx, const uint8_t *buffer) override {
        auto parameters = (const ProfileParameters *)buffer;
        
        WRITE_PROPERTY("routes", (unsigned int)parameters->routes);

        for (int i = 0; i < NUM_COLUMNS; i++) {
            char propName[16];
            sprintf(propName, "xLabel%d", i+1);
            WRITE_PROPERTY(propName, parameters->xLabels[i]);
        }

        for (int i = 0; i < NUM_ROWS; i++) {
            char propName[16];
            sprintf(propName, "yLabel%d", i+1);
            WRITE_PROPERTY(propName, parameters->yLabels[i]);
        }

        WRITE_PROPERTY("dac1", parameters->dac1);
        WRITE_PROPERTY("dac2", parameters->dac2);
        
        WRITE_PROPERTY("relayOn", parameters->relayOn);

        return true;
    }
    
    bool readProfileProperties(psu::profile::ReadContext &ctx, uint8_t *buffer) override {
        auto parameters = (ProfileParameters *)buffer;

        unsigned int routes;
        READ_PROPERTY("routes", routes);
        parameters->routes = routes;
        
        for (int i = 0; i < NUM_COLUMNS; i++) {
            char propName[16];
            sprintf(propName, "xLabel%d", i+1);
            READ_STRING_PROPERTY(propName, parameters->xLabels[i], MAX_LABEL_LENGTH);
        }

        for (int i = 0; i < NUM_ROWS; i++) {
            char propName[16];
            sprintf(propName, "yLabel%d", i+1);
            READ_STRING_PROPERTY(propName, parameters->yLabels[i], MAX_LABEL_LENGTH);
        }

        READ_PROPERTY("dac1", parameters->dac1);
        READ_PROPERTY("dac2", parameters->dac2);

        READ_PROPERTY("relayOn", parameters->relayOn);

        return true;
    }

    void resetConfiguration() {
        routes = 0;

        for (int i = 0; i < NUM_COLUMNS; i++) {
            xLabels[i][0] = 'X';
            xLabels[i][1] = '1' + i;
            xLabels[i][2] = 0;
        }

        for (int i = 0; i < NUM_ROWS; i++) {
            yLabels[i][0] = 'Y';
            yLabels[i][1] = '1' + i;
            yLabels[i][2] = 0;
        }

        dac1 = 0.0f;
        dac2 = 0.0f;

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
        relativeChannelIndex++;
        int subchannelIndex;
        if (relativeChannelIndex >= 21) {
            subchannelIndex = 41 + (relativeChannelIndex - 21);
        }
        if (relativeChannelIndex >= 15) {
            subchannelIndex = 31 + (relativeChannelIndex - 15);
        }
        if (relativeChannelIndex >= 9) {
            subchannelIndex = 21 + (relativeChannelIndex - 9);
        }
        if (relativeChannelIndex >= 3) {
            subchannelIndex = 11 + (relativeChannelIndex - 3);
        } else {
            subchannelIndex = relativeChannelIndex;
        }
        return subchannelIndex - 1;
    }

    bool isRouteOpen(int subchannelIndex, bool &isRouteOpen_, int *err) override {
        subchannelIndex++;

        if (subchannelIndex == 3) {
            isRouteOpen_ = relayOn;
        } else {
            int x = subchannelIndex % 10 - 1;
            int y = subchannelIndex / 10 - 1;

            if (x < 0 || x >= NUM_COLUMNS || y < 0 || y >= NUM_ROWS) {
                if (*err) {
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
                tempRelayOn = true;
                continue;
            }
            
            int x = subchannelIndex % 10 - 1;
            int y = subchannelIndex / 10 - 1;

            if (x < 0 || x >= NUM_COLUMNS || y < 0 || y >= NUM_ROWS) {
                if (*err) {
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
    
    bool routeClose(ChannelList channelList, int *err) override {
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
                if (*err) {
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

    bool getRelayCycles(int subchannelIndex, uint32_t &relayCycles, int *err) override {
        subchannelIndex++;

        if (subchannelIndex == 3) {
            relayCycles = powerRelayCycles;
        } else {
            int x = subchannelIndex % 10 - 1;
            int y = subchannelIndex / 10 - 1;

            if (x < 0 || x >= NUM_COLUMNS || y < 0 || y >= NUM_ROWS) {
                if (*err) {
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
            if (*err) {
                *err = SCPI_ERROR_ILLEGAL_PARAMETER_VALUE;
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
            if (*err) {
                *err = SCPI_ERROR_ILLEGAL_PARAMETER_VALUE;
            }
            return false;
        }

        if (value < DAC_MIN || value > DAC_MAX) {
            if (*err) {
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

    void getCalibrationPoints(CalibrationValueType type, unsigned int &numPoints, float *&points) override {
        points = U_CAL_POINTS;
        numPoints = sizeof(U_CAL_POINTS) / sizeof(float);
    }

    CalibrationConfiguration *getCalibrationConfiguration(int subchannelIndex) override {
        if (subchannelIndex == 0) {
            return &calConf[0];
        } else if (subchannelIndex == 1) {
            return &calConf[1];
        }
        return nullptr;
    }

    bool isRouteOpen(int x, int y) {
        return routes & (1 << (y * NUM_COLUMNS + x));
    }

    void toggleRoute(int x, int y) {
        routes ^= 1 << (y * NUM_COLUMNS + x);
    }

    void updateRelayCycles(uint32_t oldRoutes, uint32_t newRoutes, uint8_t oldRelayOn, uint8_t newRelayOn) {
        for (int y = 0; y < NUM_ROWS; y++) {
            for (int x = 0; x < NUM_COLUMNS; x++) {
                if ((oldRoutes & (1 << (y * NUM_COLUMNS + x))) == 0 && (newRoutes & (1 << (y * NUM_COLUMNS + x)))) {
                    signalRelayCycles[y][x]++;
                }
            }
        }

        if (oldRelayOn == 0 && newRelayOn == 1) {
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

        memcpy(xLabels, module->xLabels, sizeof(xLabels));
        memcpy(xLabelsOrig, module->xLabels, sizeof(xLabels));

        memcpy(yLabels, module->yLabels, sizeof(yLabels));
        memcpy(yLabelsOrig, module->yLabels, sizeof(yLabels));

        routes = routesOrig = module->routes;
    }

    int getDirty() { 
        return memcmp(xLabels, xLabelsOrig, sizeof(xLabels)) != 0 || 
            memcmp(yLabels, yLabelsOrig, sizeof(yLabels)) != 0 ||
            routes != routesOrig;
    }

    void set() {
        if (getDirty()) {
            Smx46Module *module = (Smx46Module *)g_slots[hmi::g_selectedSlotIndex];

            memcpy(module->xLabels, xLabels, sizeof(xLabels));
            memcpy(module->yLabels, yLabels, sizeof(yLabels));
            
            module->routes = routes;
        }

        popPage();
    }

    bool isRouteOpen(int x, int y) {
        return routes & (1 << (y * NUM_COLUMNS + x));
    }

    void toggleRoute(int x, int y) {
        routes ^= 1 << (y * NUM_COLUMNS + x);
    }

    char xLabels[NUM_COLUMNS][MAX_LABEL_LENGTH + 1];
    char yLabels[NUM_COLUMNS][MAX_LABEL_LENGTH + 1];
    uint32_t routes = 0;

private:
    char xLabelsOrig[NUM_COLUMNS][MAX_LABEL_LENGTH + 1];
    char yLabelsOrig[NUM_COLUMNS][MAX_LABEL_LENGTH + 1];
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
            value = (const char *)&page->xLabels[i][0];
        } else {
            value = (const char *)&((Smx46Module *)g_slots[slotIndex])->xLabels[i][0];
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
            value = (const char *)&page->yLabels[i][0];
        } else {
            value = (const char *)&((Smx46Module *)g_slots[slotIndex])->yLabels[i][0];
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
    } else if (operation == DATA_OPERATION_GET_UNIT) {
        value = UNIT_VOLT;
    } else if (operation == DATA_OPERATION_GET_ENCODER_STEP) {
        value = Value(DAC_RESOLUTION, UNIT_VOLT);
    } else if (operation == DATA_OPERATION_GET_ENCODER_STEP_VALUES) {
        StepValues *stepValues = value.getStepValues();
        stepValues->values = DAC_ENCODER_STEP_VALUES;
        stepValues->count = sizeof(DAC_ENCODER_STEP_VALUES) / sizeof(float);
        stepValues->unit = UNIT_VOLT;
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
        value = "DAC 2";
    } else if (operation == DATA_OPERATION_GET_UNIT) {
        value = UNIT_VOLT;
    } else if (operation == DATA_OPERATION_GET_ENCODER_STEP) {
        value = Value(DAC_RESOLUTION, UNIT_VOLT);
    } else if (operation == DATA_OPERATION_GET_ENCODER_STEP_VALUES) {
        StepValues *stepValues = value.getStepValues();
        stepValues->values = DAC_ENCODER_STEP_VALUES;
        stepValues->count = sizeof(DAC_ENCODER_STEP_VALUES) / sizeof(float);
        stepValues->unit = UNIT_VOLT;
        value = 1;
    } else if (operation == DATA_OPERATION_SET) {
        ((Smx46Module *)g_slots[cursor])->dac2 = roundPrec(value.getFloat(), DAC_RESOLUTION);
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

void action_dib_smx46_toggle_route() {
    int cursor = getFoundWidgetAtDown().cursor;
    int slotIndex = cursor / (NUM_COLUMNS * NUM_ROWS);
    int i = cursor % (NUM_COLUMNS * NUM_ROWS);
    int x = i % NUM_COLUMNS;
    int y = i / NUM_COLUMNS;
    ConfigureRoutesPage *page = (ConfigureRoutesPage *)getPage(PAGE_ID_DIB_SMX46_CONFIGURE_ROUTES);
    if (page) {
        page->toggleRoute(x, y);
    } else {
        ((Smx46Module *)g_slots[slotIndex])->toggleRoute(x, y);
    }
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
};

void action_dib_smx46_edit_x_label() {
    int cursor = getFoundWidgetAtDown().cursor;
    int i = cursor % NUM_COLUMNS;
    ConfigureRoutesPage *page = (ConfigureRoutesPage *)getPage(PAGE_ID_DIB_SMX46_CONFIGURE_ROUTES);
    g_labelPointer = &page->xLabels[i][0];
    Keypad::startPush("Label: ", g_labelPointer, 1, MAX_LABEL_LENGTH, false, onSetLabel, popPage);
}

void action_dib_smx46_edit_y_label() {
    int cursor = getFoundWidgetAtDown().cursor;
    int i = cursor % NUM_ROWS;
    ConfigureRoutesPage *page = (ConfigureRoutesPage *)getPage(PAGE_ID_DIB_SMX46_CONFIGURE_ROUTES);
    g_labelPointer = &page->yLabels[i][0];
    Keypad::startPush("Label: ", g_labelPointer, 1, MAX_LABEL_LENGTH, false, onSetLabel, popPage);
}

void action_dib_smx46_show_configure_routes() {
    hmi::selectSlot(getFoundWidgetAtDown().cursor);
    pushPage(PAGE_ID_DIB_SMX46_CONFIGURE_ROUTES);
}

void action_dib_smx46_close_all_routes() {
    ConfigureRoutesPage *page = (ConfigureRoutesPage *)getPage(PAGE_ID_DIB_SMX46_CONFIGURE_ROUTES);
    if (page) {
        page->routes = 0;
    } else {
        int slotIndex = getFoundWidgetAtDown().cursor;
        ((Smx46Module *)g_slots[slotIndex])->routes = 0;
    }
}

void action_dib_smx46_show_dac1_calibration() {
    hmi::g_selectedSubchannelIndex = 0;
    pushPage(PAGE_ID_CH_SETTINGS_CALIBRATION);
}

void action_dib_smx46_show_dac2_calibration() {
    hmi::g_selectedSubchannelIndex = 1;
    pushPage(PAGE_ID_CH_SETTINGS_CALIBRATION);
}

void action_dib_smx46_show_info() {
    pushPage(PAGE_ID_DIB_SMX46_INFO);
}

void action_dib_smx46_show_relay_cycles() {
    pushPage(PAGE_ID_DIB_SMX46_RELAY_CYCLES);
}

} // gui

} // namespace eez
