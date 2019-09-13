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

#ifdef DEBUG

#include <eez/apps/psu/psu.h>

#include <eez/apps/psu/datetime.h>
#include <eez/apps/psu/serial_psu.h>
#include <eez/system.h>

#ifndef EEZ_PLATFORM_SIMULATOR
#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>

extern char _end;
extern "C" char *sbrk(int i);
char *ramstart = (char *)0x20070000;
char *ramend = (char *)0x20088000;
#endif

#if OPTION_SD_CARD
#include <eez/apps/psu/sd_card.h>
#endif

namespace eez {
namespace psu {
namespace debug {

DebugValueVariable g_uDac[CH_MAX] = { DebugValueVariable("CH1 U_DAC"), DebugValueVariable("CH2 U_DAC"), DebugValueVariable("CH3 U_DAC"), DebugValueVariable("CH4 U_DAC"), DebugValueVariable("CH5 U_DAC"), DebugValueVariable("CH6 U_DAC") };
DebugValueVariable g_uMon[CH_MAX] = { DebugValueVariable("CH1 U_MON"), DebugValueVariable("CH2 U_MON"), DebugValueVariable("CH3 U_MON"), DebugValueVariable("CH4 U_MON"), DebugValueVariable("CH5 U_MON"), DebugValueVariable("CH6 U_MON") };
DebugValueVariable g_uMonDac[CH_MAX] = { DebugValueVariable("CH1 U_MON_DAC"), DebugValueVariable("CH2 U_MON_DAC"), DebugValueVariable("CH3 U_MON_DAC"), DebugValueVariable("CH4 U_MON_DAC"), DebugValueVariable("CH5 U_MON_DAC"), DebugValueVariable("CH6 U_MON_DAC") };
DebugValueVariable g_iDac[CH_MAX] = { DebugValueVariable("CH1 I_DAC"), DebugValueVariable("CH2 I_DAC"), DebugValueVariable("CH3 I_DAC"), DebugValueVariable("CH4 I_DAC"), DebugValueVariable("CH5 I_DAC"), DebugValueVariable("CH6 I_DAC") };
DebugValueVariable g_iMon[CH_MAX] = { DebugValueVariable("CH1 I_MON"), DebugValueVariable("CH2 I_MON"), DebugValueVariable("CH3 I_MON"), DebugValueVariable("CH4 I_MON"), DebugValueVariable("CH5 I_MON"), DebugValueVariable("CH6 I_MON") };
DebugValueVariable g_iMonDac[CH_MAX] = { DebugValueVariable("CH1 I_MON_DAC"), DebugValueVariable("CH2 I_MON_DAC"), DebugValueVariable("CH3 I_MON_DAC"), DebugValueVariable("CH4 I_MON_DAC"), DebugValueVariable("CH5 I_MON_DAC"), DebugValueVariable("CH6 I_MON_DAC") };

DebugDurationVariable g_mainLoopDuration("MAIN_LOOP_DURATION");
#if CONF_DEBUG_VARIABLES
DebugDurationVariable g_listTickDuration("LIST_TICK_DURATION");
#endif
DebugCounterVariable g_adcCounter("ADC_COUNTER");

DebugVariable *g_variables[] = { &g_uDac[0],          &g_uDac[1],    &g_uMon[0],    &g_uMon[1],
                                 &g_uMonDac[0],       &g_uMonDac[1], &g_iDac[0],    &g_iDac[1],
                                 &g_iMon[0],          &g_iMon[1],    &g_iMonDac[0], &g_iMonDac[1],

                                 &g_mainLoopDuration,
#if CONF_DEBUG_VARIABLES
                                 &g_listTickDuration,
#endif
                                 &g_adcCounter };

bool g_debugWatchdog = true;

static uint32_t g_previousTickCount1sec;
static uint32_t g_previousTickCount10sec;

void dumpVariables(char *buffer) {
    buffer[0] = 0;

    for (unsigned i = 0; i < sizeof(g_variables) / sizeof(DebugVariable *); ++i) {
        strcat(buffer, g_variables[i]->name());
        strcat(buffer, " = ");
        g_variables[i]->dump(buffer);
        strcat(buffer, "\n");
    }

#ifndef EEZ_PLATFORM_SIMULATOR
    char *heapend = sbrk(0);
    register char *stack_ptr asm("sp");
    struct mallinfo mi = mallinfo();
    sprintf(buffer + strlen(buffer), "Dynamic ram used: %d\n", mi.uordblks);
    sprintf(buffer + strlen(buffer), "Program static ram used %d\n", &_end - ramstart);
    sprintf(buffer + strlen(buffer), "Stack ram used %d\n", ramend - stack_ptr);
    sprintf(buffer + strlen(buffer), "My guess at free mem: %d\n",
            stack_ptr - heapend + mi.fordblks);
#endif

#if OPTION_SD_CARD
    sd_card::dumpInfo(buffer);
#endif
}

void tick(uint32_t tickCount) {
    debug::g_mainLoopDuration.tick(tickCount);

    if (g_previousTickCount1sec != 0) {
        if (tickCount - g_previousTickCount1sec >= 1000000L) {
            for (unsigned i = 0; i < sizeof(g_variables) / sizeof(DebugVariable *); ++i) {
                g_variables[i]->tick1secPeriod();
            }
            g_previousTickCount1sec = tickCount;
        }
    } else {
        g_previousTickCount1sec = tickCount;
    }

    if (g_previousTickCount10sec != 0) {
        if (tickCount - g_previousTickCount10sec >= 10 * 1000000L) {
            for (unsigned i = 0; i < sizeof(g_variables) / sizeof(DebugVariable *); ++i) {
                g_variables[i]->tick10secPeriod();
            }
            g_previousTickCount10sec = tickCount;
        }
    } else {
        g_previousTickCount10sec = tickCount;
    }
}

} // namespace debug
} // namespace psu
} // namespace eez

#endif // DEBUG
