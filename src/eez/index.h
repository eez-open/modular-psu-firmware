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

#pragma once

#include <stdint.h>

namespace eez {

static const uint16_t MODULE_TYPE_NONE = 0;
static const uint16_t MODULE_TYPE_DCP405 = 405;
static const uint16_t MODULE_TYPE_DCM220 = 220;
static const uint16_t MODULE_TYPE_DCM224 = 224;
static const uint16_t MODULE_TYPE_DIB_MIO168 = 168;
static const uint16_t MODULE_TYPE_DIB_PREL6 = 6;
static const uint16_t MODULE_TYPE_DIB_SMX46 = 46;

static const uint16_t MODULE_CATEGORY_NONE = 0;
static const uint16_t MODULE_CATEGORY_DCPSUPPLY = 1;
static const uint16_t MODULE_CATEGORY_OTHER = 2;

enum SlotViewType {
    SLOT_VIEW_TYPE_DEFAULT,
    SLOT_VIEW_TYPE_MAX,
    SLOT_VIEW_TYPE_MIN,
    SLOT_VIEW_TYPE_MICRO,
};

struct ModuleInfo {
    uint16_t moduleType;
    uint16_t moduleCategory;
    const char *moduleName;
    uint16_t latestModuleRevision;

    ModuleInfo(uint16_t moduleType, uint16_t moduleCategory, const char *moduleName, uint16_t latestModuleRevision);

    virtual int getSlotView(SlotViewType slotViewType, int slotIndex, int cursor);
};

ModuleInfo *getModuleInfo(uint16_t moduleType);

struct SlotInfo {
    ModuleInfo *moduleInfo;
    uint16_t moduleRevision;
};

static const int NUM_SLOTS = 3;
extern SlotInfo g_slots[NUM_SLOTS];

} // namespace eez
