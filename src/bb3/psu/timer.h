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
 
#pragma once

namespace eez {
namespace psu {

class Interval {
public:
	Interval(uint32_t interval);

    void reset();

    bool test();

private:
	uint32_t m_interval;
	uint32_t m_nextTick;
};

class Timer {
public:
    void start(uint32_t untilTickCount);

    bool isRunning(uint32_t tickCount);

private:
	uint32_t m_untilTickCount;
	bool m_isRunning;
};


}
} // namespace eez::psu
