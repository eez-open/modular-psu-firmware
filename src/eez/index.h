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

#include <eez/unit.h>

namespace eez {

struct StepValues {
    int count;
    const float *values;
    Unit unit;
};

enum TestResult {
    TEST_NONE,
    TEST_FAILED,
    TEST_OK,
    TEST_CONNECTING,
    TEST_SKIPPED,
    TEST_WARNING
};

static const uint16_t MODULE_TYPE_NONE    = 0;
static const uint16_t MODULE_TYPE_DCP405  = 405;
static const uint16_t MODULE_TYPE_DCM220  = 220;
static const uint16_t MODULE_TYPE_DCM224  = 224;

static const uint16_t MODULE_CATEGORY_NONE = 0;
static const uint16_t MODULE_CATEGORY_DCPSUPPLY = 1;
static const uint16_t MODULE_CATEGORY_OTHER = 2;

static const uint16_t MODULE_REVISION_DCP405_R1B1  = 0x0101;
static const uint16_t MODULE_REVISION_DCP405_R2B5  = 0x0205;
static const uint16_t MODULE_REVISION_DCP405_R2B7  = 0x0207;
static const uint16_t MODULE_REVISION_DCP405_R2B11 = 0x020B;
static const uint16_t MODULE_REVISION_DCP405_R3B1  = 0x0301;

static const uint16_t MODULE_REVISION_DCM220_R2B4  = 0x0204;

static const uint16_t MODULE_REVISION_DCM224_R1B1  = 0x0101;

struct ModuleInfo {
    uint16_t moduleType;
    uint16_t moduleCategory;
    const char *moduleName;
    uint16_t latestModuleRevision;

    ModuleInfo(uint16_t moduleType, uint16_t moduleCategory, const char *moduleName, uint16_t latestModuleRevision);
};

ModuleInfo *getModuleInfo(uint16_t moduleType);

struct SlotInfo {
    ModuleInfo *moduleInfo;
    uint16_t moduleRevision;
    uint8_t channelIndex;
};

static const int NUM_SLOTS = 3;
static const int INVALID_SLOT_INDEX = NUM_SLOTS;
extern SlotInfo g_slots[NUM_SLOTS + 1]; // one more for invalid slot

} // namespace eez
