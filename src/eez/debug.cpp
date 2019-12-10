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

// TODO these includes should not be inside apps/psu
#include <eez/modules/psu/psu.h>

#include <eez/modules/psu/datetime.h>

#include <eez/system.h>
#include <eez/util.h>

using namespace eez::psu;

namespace eez {
namespace debug {

static char *g_log = (char *)DEBUG_TRACE_LOG;
static uint32_t g_head;
static uint32_t g_tail;
static bool g_full;

static uint32_t g_numLines;
static uint32_t g_changed;

static uint32_t g_lastLineIndex;
static uint32_t g_lastLineCharPosition;

static uint32_t g_startPosition = 0;
static const uint32_t TRACE_LOG_PAGE_SIZE = 10;

void addCharToLog(char ch) {
    *(g_log + g_head) = ch;

    // advance pointer
    if (g_full) {
        g_tail = (g_tail + 1) % DEBUG_TRACE_LOG_SIZE;
    }
    g_head = (g_head + 1) % DEBUG_TRACE_LOG_SIZE;
    g_full = g_head == g_tail;
}

void Trace(const char *format, ...) {
    va_list args;
    va_start(args, format);

    char buffer[896];
    vsnprintf(buffer, 896, format, args);

    va_end(args);

    for (char *p = buffer; *p; p++) {
        if (*p == '\r') {
            continue;
        }
        if (*p == '\n') {
            addCharToLog(0);
        } else if (*p == '\t') {
            static const int TAB_SIZE = 4;
            for (int i = 0; i < TAB_SIZE; i++) {
                addCharToLog(' ');
            }
        } else {
            addCharToLog(*p);
        }
    }

    g_changed = true;

    g_lastLineIndex = 0;
    g_lastLineCharPosition = g_tail;
}

uint32_t getNumTraceLogLines() {
    if (g_changed) {
        g_changed = false;
        g_numLines = 1;
        uint32_t from = g_tail;
        uint32_t to = g_head > 0 ? g_head - 1 : DEBUG_TRACE_LOG_SIZE - 1;
        for (uint32_t i = from; i != to; i = (i + 1) % DEBUG_TRACE_LOG_SIZE) {
            if (!g_log[i]) {
                g_numLines++;
            }
        }
    }
    return g_numLines;
}

const char *getTraceLogLine(uint32_t lineIndex) {
    uint32_t lastLineIndex = g_lastLineIndex;
    uint32_t lastLineCharPosition = g_lastLineCharPosition;
    uint32_t tail = g_tail;

    if (lineIndex > lastLineIndex) {
        do {
            if (g_log[lastLineCharPosition] == 0) {
                lastLineIndex++;
            }

            lastLineCharPosition = (lastLineCharPosition + 1) % DEBUG_TRACE_LOG_SIZE;
        } while (lastLineIndex != lineIndex);
    } else if (lineIndex < lastLineIndex) {
        if (lineIndex == 0) {
            lastLineIndex = 0;
            lastLineCharPosition = tail;
        } else {
            lastLineCharPosition = (lastLineCharPosition + DEBUG_TRACE_LOG_SIZE - 2) % DEBUG_TRACE_LOG_SIZE;

            do {
                if (g_log[lastLineCharPosition] == 0) {
                    lastLineIndex--;
                }

                lastLineCharPosition = (lastLineCharPosition + DEBUG_TRACE_LOG_SIZE - 1) % DEBUG_TRACE_LOG_SIZE;
            } while (lastLineIndex != lineIndex);
                
            lastLineCharPosition = (lastLineCharPosition + 2) % DEBUG_TRACE_LOG_SIZE;
        }
    }

    return g_log + lastLineCharPosition;
}

uint32_t getTraceLogStartPosition() {
    uint32_t position = g_startPosition;
    uint32_t count = eez::debug::getNumTraceLogLines();
    if (count <= TRACE_LOG_PAGE_SIZE) {
        position = 0;
    } else if (position > count - TRACE_LOG_PAGE_SIZE) {
        position = count - TRACE_LOG_PAGE_SIZE;
    }
    return position;
}

void setTraceLogStartPosition(uint32_t position) {
    g_startPosition = position;
}

void resetTraceLogStartPosition() {
    uint32_t count = eez::debug::getNumTraceLogLines();
    if (count <= TRACE_LOG_PAGE_SIZE) {
        g_startPosition = 0;
    } else {
        g_startPosition = count - TRACE_LOG_PAGE_SIZE;
    }
}

uint32_t getTraceLogPageSize() {
    return TRACE_LOG_PAGE_SIZE;
}

void onEncoder(int counter) {
#if defined(EEZ_PLATFORM_SIMULATOR)
    counter = -counter;
#endif
    int32_t newPosition = getTraceLogStartPosition() + counter;
    if (newPosition < 0) {
        newPosition = 0;
    }
    setTraceLogStartPosition(newPosition);
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

#endif // DEBUG
