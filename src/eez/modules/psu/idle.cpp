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

#include <eez/modules/psu/psu.h>

#include <eez/modules/psu/event_queue.h>
#include <eez/modules/psu/idle.h>
#include <eez/modules/psu/rtc.h>
#include <eez/modules/psu/scpi/psu.h>
#include <eez/system.h>

namespace eez {
namespace psu {
namespace idle {

enum ActivityType {
    ACTIVITY_TYPE_NONE,
    ACTIVITY_TYPE_HMI
};
static ActivityType g_lastActivityType = ACTIVITY_TYPE_NONE;
static uint32_t g_timeOfLastActivity;

#define MAX_GUI_OR_ENCODER_INACTIVITY_TIME 60 * 1000
static bool g_hmiInactivityTimeMaxed = true;
static uint32_t g_timeOfLastHmiActivity;

void tick(uint32_t tickCount) {
    if (!g_hmiInactivityTimeMaxed) {
        uint32_t hmiInactivityPeriod = getHmiInactivityPeriod();
        if (hmiInactivityPeriod >= MAX_GUI_OR_ENCODER_INACTIVITY_TIME) {
            g_hmiInactivityTimeMaxed = true;
        }
    }
}

void noteHmiActivity() {
    g_lastActivityType = ACTIVITY_TYPE_HMI;
    g_timeOfLastActivity = micros();
    g_hmiInactivityTimeMaxed = false;
    g_timeOfLastHmiActivity = g_timeOfLastActivity;
}

uint32_t getHmiInactivityPeriod() {
    if (g_hmiInactivityTimeMaxed) {
        return MAX_GUI_OR_ENCODER_INACTIVITY_TIME;
    } else {
        return (micros() - g_timeOfLastHmiActivity) / 1000;
    }
}

} // namespace idle
} // namespace psu
} // namespace eez