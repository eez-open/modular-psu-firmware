/*
 * EEZ DIB PREL6
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

#include "./dib-prel6.h"

#include "eez/gui/document.h"

namespace eez {
namespace dib_prel6 {

struct Prel6ModuleInfo : public ModuleInfo {
public:
    Prel6ModuleInfo() 
        : ModuleInfo(MODULE_TYPE_DIB_PREL6, MODULE_CATEGORY_OTHER, "PREL6", "Envox", MODULE_REVISION_R1B2)
    {}
    
    int getSlotView(SlotViewType slotViewType, int slotIndex, int cursor) override {
        if (slotViewType == SLOT_VIEW_TYPE_DEFAULT) {
            return gui::PAGE_ID_DIB_PREL6_SLOT_VIEW_DEF;
        }
        if (slotViewType == SLOT_VIEW_TYPE_MAX) {
            return gui::PAGE_ID_DIB_PREL6_SLOT_VIEW_MAX;
        }
        if (slotViewType == SLOT_VIEW_TYPE_MIN) {
            return gui::PAGE_ID_DIB_PREL6_SLOT_VIEW_MIN;
        }
        assert(slotViewType == SLOT_VIEW_TYPE_MICRO);
        return gui::PAGE_ID_DIB_PREL6_SLOT_VIEW_MICRO;
    }
};

static Prel6ModuleInfo g_prel6ModuleInfo;
ModuleInfo *g_moduleInfo = &g_prel6ModuleInfo;

} // namespace dib_prel6
} // namespace eez