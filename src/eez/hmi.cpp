/*
* EEZ PSU Firmware
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

#include <eez/hmi.h>
#include <eez/system.h>

#include <eez/modules/psu/psu.h>
#include <eez/modules/psu/gui/psu.h>
#include <eez/modules/psu/event_queue.h>
#include <eez/modules/psu/rtc.h>
#include <eez/modules/psu/scpi/psu.h>

using namespace eez::psu;
using namespace eez::psu::gui;

namespace eez {
namespace hmi {

#define MAX_GUI_OR_ENCODER_INACTIVITY_TIME 60 * 1000

static uint32_t g_timeOfLastActivity;
static bool g_inactivityTimeMaxed = true;

int g_selectedSlotIndex;
int g_selectedSubchannelIndex;

int g_colorIndex;

void tick(uint32_t tickCount) {
    if (!g_inactivityTimeMaxed) {
        uint32_t inactivityPeriod = getInactivityPeriod();
        if (inactivityPeriod >= MAX_GUI_OR_ENCODER_INACTIVITY_TIME) {
            g_inactivityTimeMaxed = true;
        }
    }
}

void noteActivity() {
    g_timeOfLastActivity = micros();
    g_inactivityTimeMaxed = false;
}

uint32_t getInactivityPeriod() {
    if (g_inactivityTimeMaxed) {
        return MAX_GUI_OR_ENCODER_INACTIVITY_TIME;
    } else {
        return (micros() - g_timeOfLastActivity) / 1000;
    }
}

void selectSlot(int slotIndex) {
    g_selectedSlotIndex = slotIndex;
    if (!Channel::getBySlotIndex(slotIndex)) {
        selectChannel(nullptr);
    }
}

} // namespace hmi
} // namespace eez