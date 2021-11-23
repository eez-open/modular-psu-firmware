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

#include <bb3/psu/psu.h>

#include <bb3/psu/timer.h>
#include <bb3/system.h>

namespace eez {
namespace psu {

Interval::Interval(uint32_t interval) : m_interval(interval) {
    reset();
}

void Interval::reset() {
    m_nextTick = millis() + m_interval;
}

bool Interval::test() {
    uint32_t tick = millis();
    int32_t diff = tick - m_nextTick;
    if (diff > 0) {
        do {
            m_nextTick += m_interval;
            diff = tick - m_nextTick;
        } while (diff > 0);

        return true;
    }

    return false;
}

////////////////////////////////////////////////////////////////////////////////

void Timer::start(uint32_t untilTickCount) {
    m_isRunning = true;
    m_untilTickCount = untilTickCount;
}

bool Timer::isRunning(uint32_t tickCount) {
    if (m_isRunning) {
        m_isRunning = (int32_t)(m_untilTickCount - tickCount) > 0;
    }
    return m_isRunning;
}

} // namespace psu
} // namespace eez
