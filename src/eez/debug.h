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

#include <stdint.h>

namespace eez {
namespace debug {

enum TraceType {
    TRACE_TYPE_DEBUG,
    TRACE_TYPE_INFO,
    TRACE_TYPE_ERROR
};

void Trace(TraceType traceType, const char *format, ...);

} // namespace debug
} // namespace eez

#define InfoTrace(...) ::eez::debug::Trace(::eez::debug::TRACE_TYPE_INFO, __VA_ARGS__)
#define ErrorTrace(...) ::eez::debug::Trace(::eez::debug::TRACE_TYPE_ERROR, __VA_ARGS__)

#ifdef DEBUG

#define DebugTrace(...) ::eez::debug::Trace(::eez::debug::TRACE_TYPE_DEBUG, __VA_ARGS__)

#else // NO DEBUG

#define DebugTrace(...) 0

#endif
