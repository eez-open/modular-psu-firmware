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

#include "./dib-smx46.h"

#include "eez/gui/document.h"

namespace eez {
namespace dib_smx46 {

struct Smx46ModuleInfo : public ModuleInfo {
public:
    Smx46ModuleInfo() 
        : ModuleInfo(MODULE_TYPE_DIB_SMX46, MODULE_CATEGORY_OTHER, "SMX46", "Envox", MODULE_REVISION_R1B2)
    {}
    
    int getSlotView(SlotViewType slotViewType, int slotIndex, int cursor) override {
        if (slotViewType == SLOT_VIEW_TYPE_DEFAULT) {
            return gui::PAGE_ID_DIB_SMX46_SLOT_VIEW_DEF;
        }
        if (slotViewType == SLOT_VIEW_TYPE_MAX) {
            return gui::PAGE_ID_DIB_SMX46_SLOT_VIEW_MAX;
        }
        if (slotViewType == SLOT_VIEW_TYPE_MIN) {
            return gui::PAGE_ID_DIB_SMX46_SLOT_VIEW_MIN;
        }
        assert(slotViewType == SLOT_VIEW_TYPE_MICRO);
        return gui::PAGE_ID_DIB_SMX46_SLOT_VIEW_MICRO;
    }
};

static Smx46ModuleInfo g_smx46ModuleInfo;
ModuleInfo *g_moduleInfo = &g_smx46ModuleInfo;

} // namespace dib_smx46
} // namespace eez