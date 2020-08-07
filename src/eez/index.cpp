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
#include <eez/modules/dib-mio168/dib-mio168.h>
#include <eez/modules/dib-prel6/dib-prel6.h>
#include <eez/modules/dib-smx46/dib-smx46.h>

#include <eez/gui/gui.h>
#include <eez/modules/psu/psu.h>
#include <eez/modules/psu/persist_conf.h>

namespace eez {

bool g_isCol2Mode = false;
int g_slotIndexes[3] = { 0, 1, 2 };

ModuleInfo::ModuleInfo(uint16_t moduleType_, const char *moduleName_, const char *moduleBrand_, uint16_t latestModuleRevision_, FlashMethod flashMethod_, uint32_t flashDuration_, uint32_t spiBaudRatePrescaler_, bool spiCrcCalculationEnable_, uint8_t numChannels_)
    : moduleType(moduleType_)
    , moduleName(moduleName_)
    , moduleBrand(moduleBrand_)
    , latestModuleRevision(latestModuleRevision_)
    , flashMethod(flashMethod_)
    , flashDuration(flashDuration_)
    , spiBaudRatePrescaler(spiBaudRatePrescaler_)
    , spiCrcCalculationEnable(spiCrcCalculationEnable_)
    , numChannels(numChannels_)
{
}

using namespace gui;

psu::Channel *ModuleInfo::createChannel(int slotIndex, int channelIndex, int subchannelIndex) {
    return nullptr;
}

int ModuleInfo::getSlotView(SlotViewType slotViewType, int slotIndex, int cursor) {
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

////////////////////////////////////////////////////////////////////////////////

Module::Module(uint8_t slotIndex_, ModuleInfo *moduleInfo_, uint16_t moduleRevision_, bool firmwareInstalled_) 
    : slotIndex(slotIndex_)
    , moduleInfo(moduleInfo_)
    , moduleRevision(moduleRevision_)
    , firmwareInstalled(firmwareInstalled_)
    , firmwareMajorVersion(0)
    , firmwareMinorVersion(0)
    , idw0(0)
    , idw1(0)
    , idw2(0)
{
}

TestResult Module::getTestResult() {
    return TEST_SKIPPED;
}

void Module::boot() {
    using namespace psu;

    Channel::g_slotIndexToChannelIndex[slotIndex] = CH_NUM;

    for (int subchannelIndex = 0; subchannelIndex < moduleInfo->numChannels; subchannelIndex++) {
        Channel::g_channels[CH_NUM] = moduleInfo->createChannel(slotIndex, CH_NUM, subchannelIndex);
        Channel::g_channels[CH_NUM]->initParams(moduleRevision);
        CH_NUM++;
    }
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


////////////////////////////////////////////////////////////////////////////////

struct NoneModuleInfo : public ModuleInfo {
public:
    NoneModuleInfo()
        : ModuleInfo(MODULE_TYPE_NONE, "None", "None", 0, FLASH_METHOD_NONE, 0, 0, false, 0)
    {
    }

    Module *createModule(uint8_t slotIndex, uint16_t moduleRevision, bool firmwareInstalled) override;
};

struct NoneModule : public Module {
public:
    NoneModule(uint8_t slotIndex, ModuleInfo *moduleInfo, uint16_t moduleRevision, bool firmwareInstalled)
        : Module(slotIndex, moduleInfo, moduleRevision, firmwareInstalled)
    {
    }
};

Module *NoneModuleInfo::createModule(uint8_t slotIndex, uint16_t moduleRevision, bool firmwareInstalled) {
    return new NoneModule(slotIndex, this, moduleRevision, firmwareInstalled);
}

static NoneModuleInfo noneModuleInfo;

////////////////////////////////////////////////////////////////////////////////

static ModuleInfo *g_modules[] = {
    dcp405::g_moduleInfo,
    dcm220::g_dcm220ModuleInfo,
    dcm220::g_dcm224ModuleInfo,
    dib_mio168::g_moduleInfo,
    dib_prel6::g_moduleInfo,
    dib_smx46::g_moduleInfo
};

Module *g_slots[NUM_SLOTS];

////////////////////////////////////////////////////////////////////////////////

ModuleInfo *getModuleInfo(uint16_t moduleType) {
    for (unsigned int i = 0; i < sizeof(g_modules) / sizeof(ModuleInfo *); i++) {
        if (g_modules[i]->moduleType == moduleType) {
            return g_modules[i];
        }
    }
    return &noneModuleInfo;
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
