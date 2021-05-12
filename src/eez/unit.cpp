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

#include <eez/unit.h>

#include <scpi/types.h>

namespace eez {


const char *g_unitNames[] = {
    "",
    "V",
    "mV",
    "A",
    "mA",
    "uA",
    "W",
    "mW",
    "s",
    "ms",
    DEGREE_SYMBOL"C",
    "rpm",
    "\xb4",
    "K\xb4",
    "M\xb4",
    "%",
    "Hz",
    "mHz",
    "KHz",
    "MHz",
    "J",
    "F",
    "mF",
    "uF",
    "nF",
    "pF",
    "minutes",
    "VA",
    "VAR",
};

static const int g_scpiUnits[] = {
    SCPI_UNIT_NONE,
    SCPI_UNIT_NONE,
    SCPI_UNIT_VOLT,
    SCPI_UNIT_VOLT,
    SCPI_UNIT_AMPER,
    SCPI_UNIT_AMPER,
    SCPI_UNIT_AMPER,
    SCPI_UNIT_WATT,
    SCPI_UNIT_WATT,
    SCPI_UNIT_SECOND,
    SCPI_UNIT_SECOND,
    SCPI_UNIT_SECOND,
    SCPI_UNIT_CELSIUS,
    SCPI_UNIT_NONE,
    SCPI_UNIT_OHM,
    SCPI_UNIT_OHM,
    SCPI_UNIT_OHM,
    SCPI_UNIT_NONE,
    SCPI_UNIT_HERTZ,
    SCPI_UNIT_HERTZ,
    SCPI_UNIT_HERTZ,
    SCPI_UNIT_HERTZ,
    SCPI_UNIT_JOULE,
    SCPI_UNIT_FARAD,
    SCPI_UNIT_FARAD,
    SCPI_UNIT_FARAD,
    SCPI_UNIT_FARAD,
    SCPI_UNIT_FARAD
};

int getScpiUnit(Unit unit) {
    return g_scpiUnits[unit];
}

} // namespace eez