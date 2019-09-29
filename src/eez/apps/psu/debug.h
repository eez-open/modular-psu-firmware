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

#include <eez/debug.h>

#ifdef DEBUG

using eez::debug::DebugCounterVariable;
using eez::debug::DebugDurationVariable;
using eez::debug::DebugValueVariable;
using eez::debug::DebugVariable;

namespace eez {
namespace psu {
/// Everything used for the debugging purposes.
namespace debug {

void tick(uint32_t tickCount);

extern DebugValueVariable g_uDac[CH_MAX];
extern DebugValueVariable g_uMon[CH_MAX];
extern DebugValueVariable g_uMonDac[CH_MAX];
extern DebugValueVariable g_iDac[CH_MAX];
extern DebugValueVariable g_iMon[CH_MAX];
extern DebugValueVariable g_iMonDac[CH_MAX];

extern DebugDurationVariable g_mainLoopDuration;
#if CONF_DEBUG_VARIABLES
extern DebugDurationVariable g_listTickDuration;
#endif
extern DebugCounterVariable g_adcCounter;

void dumpVariables(char *buffer);

} // namespace debug
} // namespace psu
} // namespace eez

#endif
