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

#if defined(EEZ_PLATFORM_STM32)
#include <spi.h>
#include <eez/platform/stm32/spi.h>
#endif

#include "eez/debug.h"
#include "eez/firmware.h"
#include "eez/gui/document.h"
#include "eez/modules/psu/event_queue.h"
#include "eez/modules/psu/gui/psu.h"
#include "eez/modules/bp3c/comm.h"

#include "./dib-prel6.h"

namespace eez {
namespace dib_prel6 {

static const uint16_t MODULE_REVISION_R1B2  = 0x0102;

#define BUFFER_SIZE 16

struct Prel6Module : public Module {
public:
    TestResult testResult = TEST_NONE;
    bool synchronized = false;
    int numCrcErrors = 0;
    uint8_t input[BUFFER_SIZE];
    uint8_t output[BUFFER_SIZE];
    bool spiReady;

    Prel6Module() {
        moduleType = MODULE_TYPE_DIB_PREL6;
        moduleName = "PREL6";
        moduleBrand = "Envox";
        latestModuleRevision = MODULE_REVISION_R1B2;
        flashMethod = FLASH_METHOD_STM32_BOOTLOADER_UART;
        flashDuration = 0;
#if defined(EEZ_PLATFORM_STM32)        
        spiBaudRatePrescaler = SPI_BAUDRATEPRESCALER_64;
        spiCrcCalculationEnable = true;
#else
        spiBaudRatePrescaler = 0;
        spiCrcCalculationEnable = false;
#endif
        numPowerChannels = 0;
        numOtherChannels = 0;
    }

    Module *createModule() override {
        return new Prel6Module();
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
                if (g_slots[slotIndex]->firmwareInstalled) {
                    psu::event_queue::pushEvent(psu::event_queue::EVENT_ERROR_SLOT1_SYNC_ERROR + slotIndex);
                }
                testResult = TEST_FAILED;
            }
        }
    }

    void tick() override {
        if (!synchronized) {
            return;
        }

        static int cnt = 0;
        if (++cnt < 25) {
            return;
        }
        cnt = 0;

#if defined(EEZ_PLATFORM_STM32)
        if (spiReady) {
            spiReady = false;
            transfer();
        }
#endif
    }

#if defined(EEZ_PLATFORM_STM32)
    void onSpiIrq() {
        spiReady = true;
    }
#endif

    void transfer() {
        auto status = bp3c::comm::transfer(slotIndex, output, input, BUFFER_SIZE);
        if (status == bp3c::comm::TRANSFER_STATUS_OK) {
            numCrcErrors = 0;
        } else {
            if (status == bp3c::comm::TRANSFER_STATUS_CRC_ERROR) {
                if (++numCrcErrors >= 10) {
                    psu::event_queue::pushEvent(psu::event_queue::EVENT_ERROR_SLOT1_CRC_CHECK_ERROR + slotIndex);
                    synchronized = false;
                    testResult = TEST_FAILED;
                } else {
                    DebugTrace("Slot %d CRC %d\n", slotIndex + 1, numCrcErrors);
                }
            } else {
                DebugTrace("Slot %d SPI transfer error %d\n", slotIndex + 1, status);
            }
        }
    }

    void onPowerDown() override {
        synchronized = false;
    }

    int getSlotView(SlotViewType slotViewType, int slotIndex, int cursor) override {
        if (slotViewType == SLOT_VIEW_TYPE_DEFAULT) {
            return psu::gui::isDefaultViewVertical() ? gui::PAGE_ID_DIB_PREL6_SLOT_VIEW_DEF : gui::PAGE_ID_SLOT_DEF_HORZ_EMPTY;
        }
        if (slotViewType == SLOT_VIEW_TYPE_DEFAULT_2COL) {
            return psu::gui::isDefaultViewVertical() ? gui::PAGE_ID_DIB_PREL6_SLOT_VIEW_DEF_2COL : gui::PAGE_ID_SLOT_DEF_HORZ_EMPTY;
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

static Prel6Module g_prel6Module;
Module *g_module = &g_prel6Module;

} // namespace dib_prel6
} // namespace eez
