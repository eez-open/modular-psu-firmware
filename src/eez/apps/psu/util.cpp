/*
 * EEZ Modular Firmware
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

#include <eez/apps/psu/psu.h>

#include <math.h>
#include <string.h>

#include <eez/apps/psu/channel_dispatcher.h>

namespace eez {
namespace psu {

void strcatVoltage(char *str, float value) {
    strcatFloat(str, value);
    strcat(str, "V");
}

void strcatCurrent(char *str, float value) {
    strcatFloat(str, value);
    strcat(str, "A");
}

void strcatPower(char *str, float value) {
    strcatFloat(str, value);
    strcat(str, "W");
}

void strcatDuration(char *str, float value) {
    if (value > 0.1) {
        strcatFloat(str, value);
        strcat(str, " s");
    } else {
        strcatFloat(str, value * 1000);
        strcat(str, " ms");
    }
}

void strcatLoad(char *str, float value) {
    if (value < 1000) {
        strcatFloat(str, value);
        strcat(str, " ohm");
    } else if (value < 1000000) {
        strcatFloat(str, value / 1000);
        strcat(str, " Kohm");
    } else {
        strcatFloat(str, value / 1000000);
        strcat(str, " Mohm");
    }
}

} // namespace psu
} // namespace eez
