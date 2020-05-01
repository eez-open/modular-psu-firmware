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

#include <eez/index.h>

#include <eez/modules/dcp405/channel.h>
#include <eez/modules/dcm220/channel.h>

namespace eez {

ModuleInfo::ModuleInfo(uint16_t moduleType_, uint16_t moduleCategory_, const char *moduleName_, uint16_t latestModuleRevision_)
    : moduleType(moduleType_)
    , moduleCategory(moduleCategory_)
    , moduleName(moduleName_)
    , latestModuleRevision(latestModuleRevision_)
{
}

////////////////////////////////////////////////////////////////////////////////

ModuleInfo noneModuleInfo(MODULE_TYPE_NONE, MODULE_CATEGORY_NONE, "None", 0);

static ModuleInfo *g_modules[] = {
    dcp405::g_moduleInfo,
    dcm220::g_dcm220ModuleInfo,
    dcm220::g_dcm224ModuleInfo
};

ModuleInfo *getModuleInfo(uint16_t moduleType) {
    for (unsigned int i = 0; i < sizeof(g_modules) / sizeof(ModuleInfo *); i++) {
        if (g_modules[i]->moduleType == moduleType) {
            return g_modules[i];
        }
    }
    return &noneModuleInfo;
}

SlotInfo g_slots[NUM_SLOTS + 1] = {
    { &noneModuleInfo },
    { &noneModuleInfo },
    { &noneModuleInfo },
    // invalid slot
    { &noneModuleInfo }
};

} // namespace eez
