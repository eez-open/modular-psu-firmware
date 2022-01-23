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

#define INFINITY_SYMBOL "\x91"
#define DEGREE_SYMBOL "\x8a"

namespace eez {

// order of units should not be changed since it is used in DLOG files
enum Unit {
    UNIT_UNKNOWN = 255,
    UNIT_NONE = 0,
    UNIT_VOLT,
    UNIT_MILLI_VOLT,
    UNIT_AMPER,
    UNIT_MILLI_AMPER,
    UNIT_MICRO_AMPER,
    UNIT_WATT,
    UNIT_MILLI_WATT,
    UNIT_SECOND,
    UNIT_MILLI_SECOND,
    UNIT_CELSIUS,
    UNIT_RPM,
    UNIT_OHM,
    UNIT_KOHM,
    UNIT_MOHM,
    UNIT_PERCENT,
    UNIT_HERTZ,
    UNIT_MILLI_HERTZ,
    UNIT_KHERTZ,
    UNIT_MHERTZ,
    UNIT_JOULE,
    UNIT_FARAD,
    UNIT_MILLI_FARAD,
    UNIT_MICRO_FARAD,
    UNIT_NANO_FARAD,
    UNIT_PICO_FARAD,
    UNIT_MINUTE,
    UNIT_VOLT_AMPERE,
    UNIT_VOLT_AMPERE_REACTIVE,
	UNIT_DEGREE,
	UNIT_VOLT_PP,
	UNIT_MILLI_VOLT_PP,
	UNIT_AMPER_PP,
	UNIT_MILLI_AMPER_PP,
	UNIT_MICRO_AMPER_PP,
};

extern const char *g_unitNames[];

inline const char *getUnitName(Unit unit) {
	if (unit == UNIT_UNKNOWN) {
		return "";
	}
    return g_unitNames[unit];
}

Unit getUnitFromName(const char *unitName);

#if OPTION_SCPI
int getScpiUnit(Unit unit);
#endif

// for UNIT_MILLI_VOLT returns UNIT_VOLT, etc...
Unit getBaseUnit(Unit unit);

// returns 1.0 form UNIT_VOLT, returns 1E-3 for UNIT_MILLI_VOLT, 1E-6 for UNIT_MICRO_AMPER, 1E3 for UNIT_KOHM, etc
float getUnitFactor(Unit unit);

// if value is 0.01 and unit is UNIT_VOLT returns UNIT_MILLI_VOLT, etc
Unit findDerivedUnit(float value, Unit unit);

Unit getSmallerUnit(Unit unit, float min, float precision);
Unit getBiggestUnit(Unit unit, float max);
Unit getSmallestUnit(Unit unit, float min, float precision);

} // namespace eez