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

#ifdef DEBUG

#include <cstdio>
#include <stdarg.h>
#include <string.h>

#include <eez/debug.h>
#include <eez/memory.h>
#include <eez/system.h>
#include <eez/util.h>

// TODO these includes should not be inside apps/psu
#include <eez/modules/psu/psu.h>
#include <eez/modules/psu/datetime.h>

using namespace eez::psu;

namespace eez {
namespace debug {

void Trace(const char *format, ...) {
    va_list args;
    va_start(args, format);

    static const size_t BUFFER_SIZE = 256;
    char buffer[BUFFER_SIZE + 1];

	vsnprintf(buffer, BUFFER_SIZE, format, args);
	buffer[BUFFER_SIZE] = 0;

    va_end(args);
}

////////////////////////////////////////////////////////////////////////////////

DebugVariable::DebugVariable(const char *name) : m_name(name) {
}

const char *DebugVariable::name() {
    return m_name;
}

////////////////////////////////////////////////////////////////////////////////

DebugValueVariable::DebugValueVariable(const char *name) : DebugVariable(name) {
}

void DebugValueVariable::tick1secPeriod() {
}

void DebugValueVariable::tick10secPeriod() {
}

void DebugValueVariable::dump(char *buffer) {
    strcatInt32(buffer, m_value);
}

////////////////////////////////////////////////////////////////////////////////

DebugDurationForPeriod::DebugDurationForPeriod()
    : m_min(4294967295UL), m_max(0), m_total(0), m_count(0) {
}

void DebugDurationForPeriod::tick(uint32_t duration) {
    if (duration < m_min) {
        m_min = duration;
    }

    if (duration > m_max) {
        m_max = duration;
    }

    m_total += duration;
    ++m_count;
}

void DebugDurationForPeriod::tickPeriod() {
    if (m_count > 0) {
        m_minLast = m_min;
        m_maxLast = m_max;
        m_avgLast = m_total / m_count;
    } else {
        m_minLast = 0;
        m_maxLast = 0;
        m_avgLast = 0;
    }

    m_min = 4294967295UL;
    m_max = 0;
    m_total = 0;
    m_count = 0;
}

void DebugDurationForPeriod::dump(char *buffer) {
    strcatUInt32(buffer, m_minLast);
    strcat(buffer, " ");
    strcatUInt32(buffer, m_avgLast);
    strcat(buffer, " ");
    strcatUInt32(buffer, m_maxLast);
}

////////////////////////////////////////////////////////////////////////////////

DebugDurationVariable::DebugDurationVariable(const char *name)
    : DebugVariable(name), m_minTotal(4294967295UL), m_maxTotal(0) {
}

void DebugDurationVariable::start() {
    m_lastTickCount = micros();
}

void DebugDurationVariable::finish() {
    tick(micros());
}

void DebugDurationVariable::tick(uint32_t tickCount) {
    uint32_t duration = tickCount - m_lastTickCount;

    duration1sec.tick(duration);
    duration10sec.tick(duration);

    if (duration < m_minTotal) {
        m_minTotal = duration;
    }

    if (duration > m_maxTotal) {
        m_maxTotal = duration;
    }

    m_lastTickCount = tickCount;
}

void DebugDurationVariable::tick1secPeriod() {
    duration1sec.tickPeriod();
}

void DebugDurationVariable::tick10secPeriod() {
    duration10sec.tickPeriod();
}

void DebugDurationVariable::dump(char *buffer) {
    duration1sec.dump(buffer);

    strcat(buffer, " / ");

    duration10sec.dump(buffer);

    strcat(buffer, " / ");

    strcatUInt32(buffer, m_minTotal);
    strcat(buffer, " ");
    strcatUInt32(buffer, m_maxTotal);
}

////////////////////////////////////////////////////////////////////////////////

DebugCounterForPeriod::DebugCounterForPeriod() : m_counter(0) {
}

void DebugCounterForPeriod::inc() {
    ++m_counter;
}

void DebugCounterForPeriod::tickPeriod() {
    m_lastCounter = m_counter;
    m_counter = 0;
}

void DebugCounterForPeriod::dump(char *buffer) {
    strcatUInt32(buffer, m_lastCounter);
}

////////////////////////////////////////////////////////////////////////////////

DebugCounterVariable::DebugCounterVariable(const char *name) : DebugVariable(name) {
}

void DebugCounterVariable::inc() {
    counter1sec.inc();
    counter10sec.inc();
    ++m_totalCounter;
}

void DebugCounterVariable::tick1secPeriod() {
    counter1sec.tickPeriod();
}

void DebugCounterVariable::tick10secPeriod() {
    counter10sec.tickPeriod();
}

void DebugCounterVariable::dump(char *buffer) {
    counter1sec.dump(buffer);

    strcat(buffer, " / ");

    counter10sec.dump(buffer);

    strcat(buffer, " / ");

    strcatUInt32(buffer, m_totalCounter);
}

} // namespace debug
} // namespace eez

extern "C" void debug_trace(const char *str, size_t len) {
    DebugTrace("%.*s", len, str);
}

#endif // DEBUG
