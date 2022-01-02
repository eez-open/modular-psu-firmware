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
#include <eez/os.h>

namespace eez {
namespace hmi {

#define MAX_GUI_OR_ENCODER_INACTIVITY_TIME_MS 60 * 1000

static uint32_t g_timeOfLastActivity;
static bool g_inactivityTimeMaxed = true;

void tick() {
    if (!g_inactivityTimeMaxed) {
        uint32_t inactivityPeriod = getInactivityPeriodMs();
        if (inactivityPeriod >= MAX_GUI_OR_ENCODER_INACTIVITY_TIME_MS) {
            g_inactivityTimeMaxed = true;
        }
    }
}

void noteActivity() {
    g_timeOfLastActivity = millis();
    g_inactivityTimeMaxed = false;
}

uint32_t getInactivityPeriodMs() {
    if (g_inactivityTimeMaxed) {
        return MAX_GUI_OR_ENCODER_INACTIVITY_TIME_MS;
    } else {
        return millis() - g_timeOfLastActivity;
    }
}

uint32_t getTimeOfLastActivity() {
    if (g_inactivityTimeMaxed) {
        return millis() - MAX_GUI_OR_ENCODER_INACTIVITY_TIME_MS;
    } else {
        return g_timeOfLastActivity;
    }
}

} // namespace hmi
} // namespace eez