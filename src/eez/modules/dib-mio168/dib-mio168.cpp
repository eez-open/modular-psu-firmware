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

#include <assert.h>

#include "./dib-mio168.h"

#include "eez/gui/document.h"

namespace eez {
namespace dib_mio168 {

struct Mio168ModuleInfo : public ModuleInfo {
public:
    Mio168ModuleInfo() 
        : ModuleInfo(MODULE_TYPE_DIB_MIO168, MODULE_CATEGORY_OTHER, "MIO168", "Envox", MODULE_REVISION_R1B2, FLASH_METHOD_STM32_BOOTLOADER_SPI)
    {}

    Module *createModule(uint8_t slotIndex, uint16_t moduleRevision) override;
    
    int getSlotView(SlotViewType slotViewType, int slotIndex, int cursor) override {
        if (slotViewType == SLOT_VIEW_TYPE_DEFAULT) {
            return gui::PAGE_ID_DIB_MIO168_SLOT_VIEW_DEF;
        }
        if (slotViewType == SLOT_VIEW_TYPE_MAX) {
            return gui::PAGE_ID_DIB_MIO168_SLOT_VIEW_MAX;
        }
        if (slotViewType == SLOT_VIEW_TYPE_MIN) {
            return gui::PAGE_ID_DIB_MIO168_SLOT_VIEW_MIN;
        }
        assert(slotViewType == SLOT_VIEW_TYPE_MICRO);
        return gui::PAGE_ID_DIB_MIO168_SLOT_VIEW_MICRO;
    }
};

struct Mio168Module : public Module {
public:
    Mio168Module(uint8_t slotIndex, ModuleInfo *moduleInfo, uint16_t moduleRevision)
        : Module(slotIndex, moduleInfo, moduleRevision)
    {
    }
};

Module *Mio168ModuleInfo::createModule(uint8_t slotIndex, uint16_t moduleRevision) {
    return new Mio168Module(slotIndex, this, moduleRevision);
}

static Mio168ModuleInfo g_mio168ModuleInfo;
ModuleInfo *g_moduleInfo = &g_mio168ModuleInfo;

} // namespace dib_mio168
} // namespace eez
