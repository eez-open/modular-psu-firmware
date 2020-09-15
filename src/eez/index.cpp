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

#include <eez/index.h>

#include <eez/modules/dib-dcp405/dib-dcp405.h>
#include <eez/modules/dib-dcm220/dib-dcm220.h>
#include <eez/modules/dib-dcm224/dib-dcm224.h>
#include <eez/modules/dib-mio168/dib-mio168.h>
#include <eez/modules/dib-prel6/dib-prel6.h>
#include <eez/modules/dib-smx46/dib-smx46.h>

#include <eez/gui/gui.h>
#include <eez/modules/psu/psu.h>
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

void Module::onPowerDown() {
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

int Module::getChannelSettingsPageId() {
    return PAGE_ID_NONE;
}

int Module::getSlotSettingsPageId() {
    return PAGE_ID_SLOT_SETTINGS;
}

void Module::getProfileParameters(int channelIndex, uint8_t *buffer) {
}

void Module::setProfileParameters(int channelIndex, uint8_t *buffer, bool mismatch, int recallOptions, int &numTrackingChannels) {
}

bool Module::writeProfileProperties(psu::profile::WriteContext &ctx, const uint8_t *buffer) {
    return true; 
}

bool Module::readProfileProperties(psu::profile::ReadContext &ctx, uint8_t *buffer) {
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

int Module::getNumSubchannels() {
    return numPowerChannels + numOtherChannels;
}

bool Module::isValidSubchannelIndex(int subchannelIndex) {
    return subchannelIndex >= 0 && subchannelIndex < getNumSubchannels();
}

int Module::getSubchannelIndexFromRelativeChannelIndex(int relativeChannelIndex) {
    return relativeChannelIndex;
}

bool Module::getDigitalInputData(int subchannelIndex, uint8_t &data, int *err) {
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

bool Module::getMode(int subchannelIndex, SourceMode &mode, int *err) {
    if (err) {
        *err = SCPI_ERROR_HARDWARE_MISSING;
    }
    return false;
}

bool Module::setMode(int subchannelIndex, SourceMode mode, int *err) {
    if (err) {
        *err = SCPI_ERROR_HARDWARE_MISSING;
    }
    return false;
}

bool Module::getCurrentRange(int subchannelIndex, int8_t &range, int *err) {
    if (err) {
        *err = SCPI_ERROR_HARDWARE_MISSING;
    }
    return false;
}

bool Module::setCurrentRange(int subchannelIndex, int8_t range, int *err) {
    if (err) {
        *err = SCPI_ERROR_HARDWARE_MISSING;
    }
    return false;
}

bool Module::getVoltageRange(int subchannelIndex, int8_t &range, int *err) {
    if (err) {
        *err = SCPI_ERROR_HARDWARE_MISSING;
    }
    return false;
}

bool Module::setVoltageRange(int subchannelIndex, int8_t range, int *err) {
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

////////////////////////////////////////////////////////////////////////////////

struct NoneModule : public Module {
public:
    NoneModule() {
        moduleType = MODULE_TYPE_NONE;
        moduleName = "None";
        moduleBrand = "None";
        latestModuleRevision = 0;
        flashMethod = FLASH_METHOD_NONE;
        flashDuration = 0;
        spiBaudRatePrescaler = 0;
        spiCrcCalculationEnable = false;
        numPowerChannels = 0;
        numOtherChannels = 0;
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

void getModuleSerialInfo(uint8_t slotIndex, char *text) {
    auto &module = *g_slots[slotIndex];
    if (module.idw0 != 0 || module.idw1 != 0 || module.idw2 != 0) {
        sprintf(text, "%08X", (unsigned int)module.idw0);
        sprintf(text + 8, "%08X", (unsigned int)module.idw1);
        sprintf(text + 16, "%08X", (unsigned int)module.idw2);
    } else {
        strcpy(text, "N/A");
    }
}

} // namespace eez
