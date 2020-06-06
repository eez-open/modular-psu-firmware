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

#include <eez/modules/dib-dcp405/channel.h>
#include <eez/modules/dib-dcm220/channel.h>
#include <eez/modules/dib-mio168/dib-mio168.h>
#include <eez/modules/dib-prel6/dib-prel6.h>
#include <eez/modules/dib-smx46/dib-smx46.h>

#include <eez/gui/gui.h>
#include <eez/modules/psu/psu.h>
#include <eez/modules/psu/persist_conf.h>

namespace eez {

ModuleInfo::ModuleInfo(uint16_t moduleType_, uint16_t moduleCategory_, const char *moduleName_, const char *moduleBrand_, uint16_t latestModuleRevision_, FlashMethod flashMethod_)
    : moduleType(moduleType_)
    , moduleCategory(moduleCategory_)
    , moduleName(moduleName_)
    , moduleBrand(moduleBrand_)
    , latestModuleRevision(latestModuleRevision_)
    , flashMethod(flashMethod_)
{
}

using namespace gui;

static int getDefaultView(int slotIndex, int cursor) {
    int isVert = psu::persist_conf::devConf.channelsViewMode == CHANNELS_VIEW_MODE_NUMERIC || psu::persist_conf::devConf.channelsViewMode == CHANNELS_VIEW_MODE_VERT_BAR;
    return isVert ? PAGE_ID_SLOT_DEF_VERT_NOT_INSTALLED : PAGE_ID_SLOT_DEF_HORZ_NOT_INSTALLED;
}

static int getMaxView(int slotIndex, int cursor) {
    return PAGE_ID_SLOT_MAX_NOT_INSTALLED;
}

static int getMinView(int slotIndex, int cursor) {
    return PAGE_ID_SLOT_MIN_NOT_INSTALLED;
}

static int getMicroView(int slotIndex, int cursor) {
    return PAGE_ID_SLOT_MICRO_NOT_INSTALLED;
}

int ModuleInfo::getSlotView(SlotViewType slotViewType, int slotIndex, int cursor) {
    if (slotViewType == SLOT_VIEW_TYPE_DEFAULT) {
        return getDefaultView(slotIndex, cursor);
    }

    if (slotViewType == SLOT_VIEW_TYPE_MAX) {
        return getMaxView(slotIndex, cursor);
    }

    if (slotViewType == SLOT_VIEW_TYPE_MIN) {
        return getMinView(slotIndex, cursor);
    }

    assert(slotViewType == SLOT_VIEW_TYPE_MICRO);
    return getMicroView(slotIndex, cursor);
}

////////////////////////////////////////////////////////////////////////////////

Module::Module(uint8_t slotIndex_, ModuleInfo *moduleInfo_, uint16_t moduleRevision_) 
    : slotIndex(slotIndex_)
    , moduleInfo(moduleInfo_)
    , moduleRevision(moduleRevision_)
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
}

void Module::initChannels() {
}

void Module::tick() {
}

void Module::onPowerDown() {
}

////////////////////////////////////////////////////////////////////////////////

struct NoneModuleInfo : public ModuleInfo {
public:
    NoneModuleInfo()
        : ModuleInfo(MODULE_TYPE_NONE, MODULE_CATEGORY_NONE, "None", "None", 0, FLASH_METHOD_NONE)
    {
    }

    Module *createModule(uint8_t slotIndex, uint16_t moduleRevision) override;
};

struct NoneModule : public Module {
public:
    NoneModule(uint8_t slotIndex, ModuleInfo *moduleInfo, uint16_t moduleRevision)
        : Module(slotIndex, moduleInfo, moduleRevision)
    {
    }
};

Module *NoneModuleInfo::createModule(uint8_t slotIndex, uint16_t moduleRevision) {
    return new NoneModule(slotIndex, this, moduleRevision);
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
