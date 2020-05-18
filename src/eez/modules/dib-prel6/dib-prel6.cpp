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
#include <stdlib.h>

#include "eez/debug.h"
#include "eez/firmware.h"
#include "eez/gui/document.h"
#include "eez/modules/psu/event_queue.h"
#include "eez/modules/bp3c/comm.h"

#include "./dib-prel6.h"

namespace eez {
namespace dib_prel6 {

struct Prel6ModuleInfo : public ModuleInfo {
public:
    Prel6ModuleInfo() 
        : ModuleInfo(MODULE_TYPE_DIB_PREL6, MODULE_CATEGORY_OTHER, "PREL6", "Envox", MODULE_REVISION_R1B2, FLASH_METHOD_STM32_BOOTLOADER_UART)
    {}
    
    Module *createModule(uint8_t slotIndex, uint16_t moduleRevision) override;

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

#define BUFFER_SIZE 20

struct Prel6Module : public Module {
public:
    TestResult testResult = TEST_NONE;
    bool synchronized = false;
    int numCrcErrors = 0;
    uint8_t input[BUFFER_SIZE];
    uint8_t output[BUFFER_SIZE];

    Prel6Module(uint8_t slotIndex, ModuleInfo *moduleInfo, uint16_t moduleRevision)
        : Module(slotIndex, moduleInfo, moduleRevision)
    {
    }

    TestResult getTestResult() override {
        return testResult;
    }

    void initChannels() override {
        if (!synchronized) {
            if (bp3c::comm::masterSynchro(slotIndex)) {
                synchronized = true;
                numCrcErrors = 0;
                testResult = TEST_OK;
            } else {
                psu::event_queue::pushEvent(psu::event_queue::EVENT_ERROR_SLOT1_SYNC_ERROR + slotIndex);
                testResult = TEST_FAILED;
            }
        }
    }

    int cnt1 = 0;
    int cnt2 = 0;

    void tick() override {
        if (!synchronized) {
            return;
        }

        cnt1++;

        if (cnt1 % 1000) {
            cnt2++;
            return;
        }

        if (bp3c::comm::transfer(slotIndex, output, input, BUFFER_SIZE)) {
            cnt2++;

            if (cnt2 % 10000 == 0) {
                uint32_t scnt1 = *(uint32_t *)(input + 0);
                uint32_t scnt2 = *(uint32_t *)(input + 4);
                uint32_t scnt3 = *(uint32_t *)(input + 8);
                DebugTrace("%d, %d, %d, %d, %d\n", cnt1, cnt2, scnt1, scnt2, scnt3);
            }

            numCrcErrors = 0;
        } else {
            if (++numCrcErrors >= 4) {
                psu::event_queue::pushEvent(psu::event_queue::EVENT_ERROR_SLOT1_CRC_CHECK_ERROR + slotIndex);
                synchronized = false;
                testResult = TEST_FAILED;
            } else {
                DebugTrace("Slot %d CRC %d\n", slotIndex + 1, numCrcErrors);
            }
        }
    }

    void onPowerDown() override {
        synchronized = false;
    }
};

Module *Prel6ModuleInfo::createModule(uint8_t slotIndex, uint16_t moduleRevision) {
    return new Prel6Module(slotIndex, this, moduleRevision);
}

static Prel6ModuleInfo g_prel6ModuleInfo;
ModuleInfo *g_moduleInfo = &g_prel6ModuleInfo;

} // namespace dib_prel6
} // namespace eez
