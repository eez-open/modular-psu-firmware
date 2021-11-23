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

namespace eez {
namespace debug {

void Trace(TraceType traceType, const char *format, ...) {
    va_list args;
    va_start(args, format);

    static const size_t BUFFER_SIZE = 256;
    char buffer[BUFFER_SIZE + 1];

	vsnprintf(buffer, BUFFER_SIZE, format, args);
	buffer[BUFFER_SIZE] = 0;

    va_end(args);

    if (traceType == TRACE_TYPE_DEBUG) {
        pushDebugTraceHook(buffer, strlen(buffer));
    } else if (traceType == TRACE_TYPE_INFO) {
        pushInfoTraceHook(buffer, strlen(buffer));
    } else {
        pushErrorTraceHook(buffer, strlen(buffer));
    }
}

} // namespace debug
} // namespace eez

extern "C" void debug_trace(const char *str, size_t len) {
    eez::debug::pushDebugTraceHook(str, len);
}

#endif // DEBUG
