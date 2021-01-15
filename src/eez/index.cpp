/*
* EEZ Generic Firmware
* Copyright (C) 2018-present, Envox d.o.o.
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
#include <string.h>

#if defined(EEZ_PLATFORM_STM32)
#include <ff_gen_drv.h>
#include <usbd_def.h>
#endif

#include <eez/index.h>

#include <eez/modules/dib-dcp405/dib-dcp405.h>
#include <eez/modules/dib-dcm220/dib-dcm220.h>
#include <eez/modules/dib-dcm224/dib-dcm224.h>
#include <eez/modules/dib-mio168/dib-mio168.h>
#include <eez/modules/dib-prel6/dib-prel6.h>
#include <eez/modules/dib-smx46/dib-smx46.h>

#include <eez/gui/gui.h>
#include <eez/modules/psu/psu.h>
#include <eez/modules/psu/profile.h>
#include <eez/modules/psu/persist_conf.h>

#include <scpi/scpi.h>

namespace eez {

bool g_isCol2Mode = false;
int g_slotIndexes[3] = { 0, 1, 2 };

using namespace gui;

////////////////////////////////////////////////////////////////////////////////

void Module::setEnabled(bool value) {
    enabled = value;
    psu::persist_conf::setSlotEnabled(slotIndex, value);
}

TestResult Module::getTestResult() {
    return TEST_SKIPPED;
}

void Module::boot() {
    using namespace psu;

    if (numPowerChannels > 0) {
        Channel::g_slotIndexToChannelIndex[slotIndex] = CH_NUM;

        for (int subchannelIndex = 0; subchannelIndex < numPowerChannels; subchannelIndex++) {
            Channel::g_channels[CH_NUM] = createPowerChannel(slotIndex, CH_NUM, subchannelIndex);
            Channel::g_channels[CH_NUM]->initParams(moduleRevision);
            CH_NUM++;
        }
    }
}

psu::Channel *Module::createPowerChannel(int slotIndex, int channelIndex, int subchannelIndex) {
    return nullptr;
}

void Module::initChannels() {
}

void Module::tick() {
}

void Module::writeUnsavedData() {
}

void Module::onPowerDown() {
}

void Module::resync() {
}

void Module::onSpiIrq() {
}

void Module::onSpiDmaTransferCompleted(int status) {
}

gui::Page *Module::getPageFromId(int pageId) {
    return nullptr;
}

void Module::animatePageAppearance(int previousPageId, int activePageId) {
}

int Module::getSlotView(SlotViewType slotViewType, int slotIndex, int cursor) {
    if (slotViewType == SLOT_VIEW_TYPE_DEFAULT) {
        int isVert = psu::persist_conf::devConf.channelsViewMode == CHANNELS_VIEW_MODE_NUMERIC || psu::persist_conf::devConf.channelsViewMode == CHANNELS_VIEW_MODE_VERT_BAR;
        return isVert ? PAGE_ID_SLOT_DEF_VERT_NOT_INSTALLED : PAGE_ID_SLOT_DEF_HORZ_NOT_INSTALLED;
    }

    if (slotViewType == SLOT_VIEW_TYPE_DEFAULT_2COL) {
        int isVert = psu::persist_conf::devConf.channelsViewMode == CHANNELS_VIEW_MODE_NUMERIC || psu::persist_conf::devConf.channelsViewMode == CHANNELS_VIEW_MODE_VERT_BAR;
        return isVert ? PAGE_ID_SLOT_DEF_VERT_NOT_INSTALLED_2COL : PAGE_ID_SLOT_DEF_HORZ_NOT_INSTALLED_2COL;
    }

    if (slotViewType == SLOT_VIEW_TYPE_MAX) {
        return PAGE_ID_SLOT_MAX_NOT_INSTALLED;
    }

    if (slotViewType == SLOT_VIEW_TYPE_MIN) {
    return PAGE_ID_SLOT_MIN_NOT_INSTALLED;
    }

    assert(slotViewType == SLOT_VIEW_TYPE_MICRO);
    return PAGE_ID_SLOT_MICRO_NOT_INSTALLED;
}

int Module::getSlotSettingsPageId() {
    return PAGE_ID_SLOT_SETTINGS;
}

int Module::getLabelsAndColorsPageId() {
    return moduleType == MODULE_TYPE_NONE ? PAGE_ID_NONE : PAGE_ID_SLOT_LABELS_AND_COLORS;
}

void Module::onLowPriorityThreadMessage(uint8_t type, uint32_t param) {
}

void Module::onHighPriorityThreadMessage(uint8_t type, uint32_t param) {
}

void Module::resetPowerChannelProfileToDefaults(int channelIndex, uint8_t *buffer) {
}

void Module::getPowerChannelProfileParameters(int channelIndex, uint8_t *buffer) {
}

void Module::setPowerChannelProfileParameters(int channelIndex, uint8_t *buffer, bool mismatch, int recallOptions, int &numTrackingChannels) {
}

bool Module::writePowerChannelProfileProperties(psu::profile::WriteContext &ctx, const uint8_t *buffer) {
    return true; 
}

bool Module::readPowerChannelProfileProperties(psu::profile::ReadContext &ctx, uint8_t *buffer) {
    return false;
}

bool Module::getProfileOutputEnable(uint8_t *buffer) {
    return false;
}

float Module::getProfileUSet(uint8_t *buffer) {
    return NAN;
}

float Module::getProfileISet(uint8_t *buffer) {
    return NAN;
}

bool Module::testAutoRecallValuesMatch(uint8_t *bufferRecall, uint8_t *bufferDefault) {
    return true;
}

void Module::resetProfileToDefaults(uint8_t *buffer) {
    auto parameters = (ProfileParameters *)buffer;

    *parameters->label = 0;
    parameters->color = 0;
}

void Module::getProfileParameters(uint8_t *buffer) {
    auto parameters = (ProfileParameters *)buffer;

    strcpy(parameters->label, label);
    parameters->color = color;
}

void Module::setProfileParameters(uint8_t *buffer, bool mismatch, int recallOptions) {
    auto parameters = (ProfileParameters *)buffer;

    strcpy(label, parameters->label);
    color = parameters->color;
}

bool Module::writeProfileProperties(psu::profile::WriteContext &ctx, const uint8_t *buffer) {
    auto parameters = (ProfileParameters *)buffer;

    WRITE_PROPERTY("label", parameters->label);
    WRITE_PROPERTY("color", parameters->color);

    return true;
}

bool Module::readProfileProperties(psu::profile::ReadContext &ctx, uint8_t *buffer) {
    auto parameters = (ProfileParameters *)buffer;

    READ_STRING_PROPERTY("label", parameters->label, SLOT_LABEL_MAX_LENGTH);
    READ_PROPERTY("color", parameters->color);

    return false;
}

void Module::resetConfiguration() {
    *label = 0;
    color = 0;
}

int Module::getNumSubchannels() {
    return numPowerChannels + numOtherChannels;
}

bool Module::isValidSubchannelIndex(int subchannelIndex) {
    return subchannelIndex >= 0 && subchannelIndex < getNumSubchannels();
}

int Module::getSubchannelIndexFromRelativeChannelIndex(int relativeChannelIndex) {
    return relativeChannelIndex;
}

const char *Module::getLabel() {
    return label;
}

const char *Module::getDefaultLabel() {
    return moduleName;
}

void Module::setLabel(const char *value, int length) {
    if (length == -1) {
        length = strlen(value);
    }
    if (length > (int)SLOT_LABEL_MAX_LENGTH) {
        length = SLOT_LABEL_MAX_LENGTH;
    }
    strncpy(label, value, length);
    label[length] = 0;
}

uint8_t Module::getColor() {
    return color;
}

void Module::setColor(uint8_t value) {
    color = value;
}

size_t Module::getChannelLabelMaxLength(int subchannelIndex) {
    return 0;
}

const char *Module::getChannelLabel(int subchannelIndex) {
    return "";
}

const char *Module::getDefaultChannelLabel(int subchannelIndex) {
    return "";
}

eez_err_t Module::getChannelLabel(int subchannelIndex, const char *&label) {
    return SCPI_ERROR_HARDWARE_MISSING;
}

eez_err_t Module::setChannelLabel(int subchannelIndex, const char *label, int length) {
    return SCPI_ERROR_HARDWARE_MISSING;
}

size_t Module::getChannelPinLabelMaxLength(int subchannelIndex, int pin) {
    return 0;
}

const char *Module::getDefaultChannelPinLabel(int subchannelIndex, int pin) {
    return "";
}

eez_err_t Module::getChannelPinLabel(int subchannelIndex, int pin, const char *&label) {
    return SCPI_ERROR_HARDWARE_MISSING;
}

eez_err_t Module::setChannelPinLabel(int subchannelIndex, int pin, const char *label, int length) {
    return SCPI_ERROR_HARDWARE_MISSING;
}

uint8_t Module::getChannelColor(int subchannelIndex) {
    return 0;
}

eez_err_t Module::getChannelColor(int subchannelIndex, uint8_t &color) {
    return SCPI_ERROR_HARDWARE_MISSING;
}

eez_err_t Module::setChannelColor(int subchannelIndex, uint8_t color) {
    return SCPI_ERROR_HARDWARE_MISSING;
}

bool Module::getDigitalInputData(int subchannelIndex, uint8_t &data, int *err) {
    if (err) {
        *err = SCPI_ERROR_HARDWARE_MISSING;
    }
    return false;
}

#ifdef EEZ_PLATFORM_SIMULATOR
bool Module::setDigitalInputData(int subchannelIndex, uint8_t data, int *err) {
    if (err) {
        *err = SCPI_ERROR_HARDWARE_MISSING;
    }
    return false;    
}
#endif

bool Module::getDigitalInputRange(int subchannelIndex, uint8_t pin, uint8_t &range, int *err) {
    if (err) {
        *err = SCPI_ERROR_HARDWARE_MISSING;
    }
    return false;
}

bool Module::setDigitalInputRange(int subchannelIndex, uint8_t pin, uint8_t range, int *err) {
    if (err) {
        *err = SCPI_ERROR_HARDWARE_MISSING;
    }
    return false;
}

bool Module::getDigitalInputSpeed(int subchannelIndex, uint8_t pin, uint8_t &speed, int *err) {
    if (err) {
        *err = SCPI_ERROR_HARDWARE_MISSING;
    }
    return false;
}

bool Module::setDigitalInputSpeed(int subchannelIndex, uint8_t pin, uint8_t speed, int *err) {
    if (err) {
        *err = SCPI_ERROR_HARDWARE_MISSING;
    }
    return false;
}

bool Module::getDigitalOutputData(int subchannelIndex, uint8_t &data, int *err) {
    if (err) {
        *err = SCPI_ERROR_HARDWARE_MISSING;
    }
    return false;
}

bool Module::setDigitalOutputData(int subchannelIndex, uint8_t data, int *err) {
    if (err) {
        *err = SCPI_ERROR_HARDWARE_MISSING;
    }
    return false;
}

bool Module::getSourceMode(int subchannelIndex, SourceMode &mode, int *err) {
    if (err) {
        *err = SCPI_ERROR_HARDWARE_MISSING;
    }
    return false;
}

bool Module::setSourceMode(int subchannelIndex, SourceMode mode, int *err) {
    if (err) {
        *err = SCPI_ERROR_HARDWARE_MISSING;
    }
    return false;
}

bool Module::getSourceCurrentRange(int subchannelIndex, uint8_t &range, int *err) {
    if (err) {
        *err = SCPI_ERROR_HARDWARE_MISSING;
    }
    return false;
}

bool Module::setSourceCurrentRange(int subchannelIndex, uint8_t range, int *err) {
    if (err) {
        *err = SCPI_ERROR_HARDWARE_MISSING;
    }
    return false;
}

bool Module::getSourceVoltageRange(int subchannelIndex, uint8_t &range, int *err) {
    if (err) {
        *err = SCPI_ERROR_HARDWARE_MISSING;
    }
    return false;
}

bool Module::setSourceVoltageRange(int subchannelIndex, uint8_t range, int *err) {
    if (err) {
        *err = SCPI_ERROR_HARDWARE_MISSING;
    }
    return false;
}

bool Module::getMeasureMode(int subchannelIndex, MeasureMode &mode, int *err) {
    if (err) {
        *err = SCPI_ERROR_HARDWARE_MISSING;
    }
    return false;
}

bool Module::setMeasureMode(int subchannelIndex, MeasureMode mode, int *err) {
    if (err) {
        *err = SCPI_ERROR_HARDWARE_MISSING;
    }
    return false;
}

bool Module::getMeasureCurrentRange(int subchannelIndex, uint8_t &range, int *err) {
    if (err) {
        *err = SCPI_ERROR_HARDWARE_MISSING;
    }
    return false;
}

bool Module::setMeasureCurrentRange(int subchannelIndex, uint8_t range, int *err) {
    if (err) {
        *err = SCPI_ERROR_HARDWARE_MISSING;
    }
    return false;
}

bool Module::getMeasureVoltageRange(int subchannelIndex, uint8_t &range, int *err) {
	if (err) {
		*err = SCPI_ERROR_HARDWARE_MISSING;
	}
	return false;
}

bool Module::setMeasureVoltageRange(int subchannelIndex, uint8_t range, int *err) {
	if (err) {
		*err = SCPI_ERROR_HARDWARE_MISSING;
	}
	return false;
}

bool Module::getMeasureCurrentNPLC(int subchannelIndex, float &nplc, int *err) {
	if (err) {
		*err = SCPI_ERROR_HARDWARE_MISSING;
	}
	return false;
}

bool Module::setMeasureCurrentNPLC(int subchannelIndex, float nplc, int *err) {
	if (err) {
		*err = SCPI_ERROR_HARDWARE_MISSING;
	}
	return false;
}

bool Module::getMeasureVoltageNPLC(int subchannelIndex, float &nplc, int *err) {
	if (err) {
		*err = SCPI_ERROR_HARDWARE_MISSING;
	}
	return false;
}

bool Module::setMeasureVoltageNPLC(int subchannelIndex, float nplc, int *err) {
	if (err) { 
		*err = SCPI_ERROR_HARDWARE_MISSING;
	}
	return false;
}

bool Module::isRouteOpen(int subchannelIndex, bool &isRouteOpen, int *err) {
    if (err) {
        *err = SCPI_ERROR_HARDWARE_MISSING;
    }
    return false;
}

bool Module::routeOpen(ChannelList channelList, int *err) {
    if (err) {
        *err = SCPI_ERROR_HARDWARE_MISSING;
    }
    return false;
}

bool Module::routeClose(ChannelList channelList, int *err) {
    if (err) {
        *err = SCPI_ERROR_HARDWARE_MISSING;
    }
    return false;
}

bool Module::getSwitchMatrixNumRows(int &numRows, int *err) {
    if (err) {
        *err = SCPI_ERROR_HARDWARE_MISSING;
    }
    return false;
}

bool Module::getSwitchMatrixNumColumns(int &numColumns, int *err) {
    if (err) {
        *err = SCPI_ERROR_HARDWARE_MISSING;
    }
    return false;
}

bool Module::setSwitchMatrixRowLabel(int subchannelIndex, const char *label, int *err) {
    if (err) {
        *err = SCPI_ERROR_HARDWARE_MISSING;
    }
    return false;
}

bool Module::getSwitchMatrixRowLabel(int subchannelIndex, char *label, int *err) {
    if (err) {
        *err = SCPI_ERROR_HARDWARE_MISSING;
    }
    return false;
}

bool Module::setSwitchMatrixColumnLabel(int subchannelIndex, const char *label, int *err) {
    if (err) {
        *err = SCPI_ERROR_HARDWARE_MISSING;
    }
    return false;
}

bool Module::getSwitchMatrixColumnLabel(int subchannelIndex, char *label, int *err) {
    if (err) {
        *err = SCPI_ERROR_HARDWARE_MISSING;
    }
    return false;
}

bool Module::getRelayCycles(int subchannelIndex, uint32_t &relayCycles, int *err) {
    if (err) {
        *err = SCPI_ERROR_HARDWARE_MISSING;
    }
    return false;
}

bool Module::outputEnable(int subchannelIndex, bool enable, int *err) {
    if (err) {
        *err = SCPI_ERROR_HARDWARE_MISSING;
    }
    return false;
}

bool Module::isOutputEnabled(int subchannelIndex, bool &enabled, int *err) {
    if (err) {
        *err = SCPI_ERROR_HARDWARE_MISSING;
    }
    return false;
}

bool Module::getVoltage(int subchannelIndex, float &value, int *err) {
    if (err) {
        *err = SCPI_ERROR_HARDWARE_MISSING;
    }
    return false;
}

bool Module::setVoltage(int subchannelIndex, float value, int *err) {
    if (err) {
        *err = SCPI_ERROR_HARDWARE_MISSING;
    }
    return false;
}

bool Module::getMeasuredVoltage(int subchannelIndex, float &value, int *err) {
    if (err) {
        *err = SCPI_ERROR_HARDWARE_MISSING;
    }
    return false;
}

void Module::getVoltageStepValues(int subchannelIndex, StepValues *stepValues, bool calibrationMode) {
    stepValues->values = nullptr;
    stepValues->count = 0;
    stepValues->unit = UNIT_VOLT;

    stepValues->encoderSettings.accelerationEnabled = false;
    stepValues->encoderSettings.range = 40.0f;
    stepValues->encoderSettings.step = 0.005f;
}

float Module::getVoltageResolution(int subchannelIndex) {
    return NAN;
}

float Module::getVoltageMinValue(int subchannelIndex) {
    return NAN;
}

float Module::getVoltageMaxValue(int subchannelIndex) {
    return NAN;
}

bool Module::isConstantVoltageMode(int subchannelIndex) {
    return false;
}

bool Module::isVoltageCalibrationExists(int subchannelIndex) {
    return false;
}

bool Module::isVoltageCalibrationEnabled(int subchannelIndex) {
    return false;
}

void Module::enableVoltageCalibration(int subchannelIndex, bool enable) {
}

bool Module::getCurrent(int subchannelIndex, float &value, int *err) {
    if (err) {
        *err = SCPI_ERROR_HARDWARE_MISSING;
    }
    return false;
}

bool Module::setCurrent(int subchannelIndex, float value, int *err) {
    if (err) {
        *err = SCPI_ERROR_HARDWARE_MISSING;
    }
    return false;
}

bool Module::getMeasuredCurrent(int subchannelIndex, float &value, int *err) {
    if (err) {
        *err = SCPI_ERROR_HARDWARE_MISSING;
    }
    return false;
}

void Module::getCurrentStepValues(int subchannelIndex, StepValues *stepValues, bool calibrationMode) {
    stepValues->values = nullptr;
    stepValues->count = 0;
    stepValues->unit = UNIT_AMPER;

    stepValues->encoderSettings.accelerationEnabled = false;
    stepValues->encoderSettings.range = 5.0f;
    stepValues->encoderSettings.step = 0.0005f;
}

float Module::getCurrentResolution(int subchannelIndex) {
    return NAN;
}

float Module::getCurrentMinValue(int subchannelIndex) {
    return NAN;
}

float Module::getCurrentMaxValue(int subchannelIndex) {
    return NAN;
}

bool Module::isCurrentCalibrationExists(int subchannelIndex) {
    return false;
}

bool Module::isCurrentCalibrationEnabled(int subchannelIndex) {
    return false;
}

void Module::enableCurrentCalibration(int subchannelIndex, bool enable) {
}

bool Module::loadChannelCalibration(int subchannelIndex, int *err) {
    return true;
}

bool Module::saveChannelCalibration(int subchannelIndex, int *err) {
    if (err) {
        *err = SCPI_ERROR_HARDWARE_MISSING;
    }
    return false;
}

void Module::initChannelCalibration(int subchannelIndex) {
}

void Module::startChannelCalibration(int subchannelIndex) {
}

bool Module::calibrationReadAdcValue(int subchannelIndex, float &adcValue, int *err) {
    adcValue = 0.0f;
    return true;
}

bool Module::calibrationMeasure(int subchannelIndex, float &measuredValue, int *err) {
    if (err) {
        *err = SCPI_ERROR_BAD_SEQUENCE_OF_CALIBRATION_COMMANDS;
    }
    return false;
}

void Module::stopChannelCalibration(int subchannelIndex) {
}

unsigned int Module::getMaxCalibrationPoints(int m_subchannelIndex) {
    return MAX_CALIBRATION_POINTS;
}

CalibrationValueType Module::getCalibrationValueType(int subchannelIndex) {
    return CALIBRATION_VALUE_U;
}

const char *Module::getCalibrationValueRangeDescription(int subchannelIndex) {
    return nullptr;
}

bool Module::isCalibrationValueSource(int subchannelIndex) {
    return true;
}

void Module::getDefaultCalibrationPoints(int subchannelIndex, CalibrationValueType type, unsigned int &numPoints, float *&points) {
    numPoints = 0;
    points = nullptr;
}

bool Module::getCalibrationConfiguration(int subchannelIndex, CalibrationConfiguration &calConf, int *err) {
    if (err) {
        *err = SCPI_ERROR_HARDWARE_MISSING;
    }
    return false;
}

bool Module::setCalibrationConfiguration(int subchannelIndex, const CalibrationConfiguration &calConf, int *err) {
    if (err) {
        *err = SCPI_ERROR_HARDWARE_MISSING;
    }
    return false;
}

bool Module::getCalibrationRemark(int subchannelIndex, const char *&calibrationRemark, int *err) {
    if (err) {
        *err = SCPI_ERROR_HARDWARE_MISSING;
    }
    return false;
}

bool Module::getCalibrationDate(int subchannelIndex, uint32_t &calibrationDate, int *err) {
    if (err) {
        *err = SCPI_ERROR_HARDWARE_MISSING;
    }
    return false;
}

int Module::getNumDlogResources(int subchannelIndex) {
    return 0;
}

DlogResourceType Module::getDlogResourceType(int subchannelIndex, int resourceIndex) {
    return DLOG_RESOURCE_TYPE_NONE;
}

const char *Module::getDlogResourceLabel(int subchannelIndex, int resourceIndex) {
    return "";
}

float Module::getDlogResourceMinPeriod(int subchannelIndex, int resourceIndex) {
    return NAN;
}

void Module::onStartDlog() {
}

void Module::onStopDlog() {
}

int Module::diskDriveInitialize() {
    return 1; // STA_NOINIT
}

int Module::diskDriveStatus() {
    return 1; // STA_NOINIT
}

int Module::diskDriveRead(uint8_t *buff, uint32_t sector) {
    return 1; // RES_ERROR
}

int Module::diskDriveWrite(uint8_t *buff, uint32_t sector) {
    return 1; // RES_ERROR
}

int Module::diskDriveIoctl(uint8_t cmd, void *buff) {
    return 1; // RES_ERROR
}

////////////////////////////////////////////////////////////////////////////////

struct NoneModule : public Module {
public:
    NoneModule() {
        moduleType = MODULE_TYPE_NONE;
        moduleName = "None";
        moduleBrand = "None";
        latestModuleRevision = 0;
        flashMethod = FLASH_METHOD_NONE;
        spiBaudRatePrescaler = 0;
        spiCrcCalculationEnable = false;
        numPowerChannels = 0;
        numOtherChannels = 0;
        isResyncSupported = false;
    }

    Module *createModule() {
        return new NoneModule();
    }
};

static NoneModule noneModule;

////////////////////////////////////////////////////////////////////////////////

static Module *g_modules[] = {
    dcp405::g_module,
    dcm220::g_module,
    dcm224::g_module,
    dib_mio168::g_module,
    dib_prel6::g_module,
    dib_smx46::g_module
};

Module *g_slots[NUM_SLOTS];

////////////////////////////////////////////////////////////////////////////////

Module *getModule(uint16_t moduleType) {
    for (unsigned int i = 0; i < sizeof(g_modules) / sizeof(Module *); i++) {
        if (g_modules[i]->moduleType == moduleType) {
            return g_modules[i];
        }
    }
    return &noneModule;
}

void getModuleSerialInfo(uint8_t slotIndex, char *serialStr) {
    auto &module = *g_slots[slotIndex];
    if (module.idw0 != 0 || module.idw1 != 0 || module.idw2 != 0) {
        sprintf(serialStr, "%08X", (unsigned int)module.idw0);
        sprintf(serialStr + 8, "%08X", (unsigned int)module.idw1);
        sprintf(serialStr + 16, "%08X", (unsigned int)module.idw2);
    } else {
        strcpy(serialStr, "N/A");
    }
}

bool hexStringToNumber(const char *str, uint32_t &num, int *err) {
    num = 0;
    for (int i = 0; i < 8; i++) {
        if (str[i] >= '0' && str[i] <= '9') {
            num = num * 16 + (str[i] - '0');
        } else if (str[i] >= 'A' && str[i] <= 'F') {
            num = num * 16 + (10 + str[i] - 'F');
        } else if (str[i] >= 'a' && str[i] <= 'f') {
            num = num * 16 + (10 + str[i] - 'a');
        } else {
            if (err) {
                *err = SCPI_ERROR_ILLEGAL_PARAMETER_VALUE;
            }
            return false;
        }
    }
    return true;
}

bool setModuleSerialInfo(uint8_t slotIndex, const char *serialStr, size_t serialStrLen, int *err) {
    Module *module = g_slots[slotIndex];

    if (module->moduleType != MODULE_TYPE_DCP405) {
        if (err) {
            *err = SCPI_ERROR_EXECUTION_ERROR;
        }
        return false;
    }

    if (serialStrLen != 24) {
        if (err) {
            *err = SCPI_ERROR_ILLEGAL_PARAMETER_VALUE;
        }
        return false;
    }

    uint32_t idw0;
    if (!hexStringToNumber(serialStr, idw0, err)) {
        return false;
    }

    uint32_t idw1;
    if (!hexStringToNumber(serialStr + 8, idw1, err)) {
        return false;
    }

    uint32_t idw2;
    if (!hexStringToNumber(serialStr + 16, idw2, err)) {
        return false;
    }

    g_slots[slotIndex]->idw0 = idw0;
    g_slots[slotIndex]->idw1 = idw1;
    g_slots[slotIndex]->idw2 = idw2;

    psu::persist_conf::saveSerialNo(slotIndex);

    return true;
}

} // namespace eez
