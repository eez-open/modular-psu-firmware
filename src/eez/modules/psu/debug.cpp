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

#include <eez/modules/psu/psu.h>

#include <eez/modules/psu/datetime.h>
#include <eez/modules/psu/serial_psu.h>
#include <eez/system.h>

namespace eez {
namespace psu {
namespace debug {

#define CHANNELS CHANNEL(1),  CHANNEL(2),  CHANNEL(3),  CHANNEL(4),  CHANNEL(5),  CHANNEL(6)

DebugCounterVariable g_adcCounter("ADC_COUNTER");
DebugValueVariable g_encoderCounter("ENC_COUNTER", 100);

#undef CHANNEL
#define CHANNEL(N) DebugValueVariable("CH"#N" U_DAC")
DebugValueVariable g_uDac[CH_MAX] = { CHANNELS };

#undef CHANNEL
#define CHANNEL(N) DebugValueVariable("CH"#N" U_MON")
DebugValueVariable g_uMon[CH_MAX] = { CHANNELS };

#undef CHANNEL
#define CHANNEL(N) DebugValueVariable("CH"#N" U_MON_DAC")
DebugValueVariable g_uMonDac[CH_MAX] = { CHANNELS };

#undef CHANNEL
#define CHANNEL(N) DebugValueVariable("CH"#N" I_DAC")
DebugValueVariable g_iDac[CH_MAX] = { CHANNELS };

#undef CHANNEL
#define CHANNEL(N) DebugValueVariable("CH"#N" I_MON")
DebugValueVariable g_iMon[CH_MAX] = { CHANNELS };

#undef CHANNEL
#define CHANNEL(N) DebugValueVariable("CH"#N" I_MON_DAC")
DebugValueVariable g_iMonDac[CH_MAX] = { CHANNELS };

#undef CHANNEL
#define CHANNEL(N) &g_uDac[N-1], &g_uMon[N-1], &g_uMonDac[N-1], &g_iDac[N-1], &g_iMon[N-1], &g_iMonDac[N-1]
DebugVariable *g_variables[] = { 
    &g_adcCounter,
    &g_encoderCounter,
    CHANNELS
};

static uint32_t g_previousTickCount1sec;
static uint32_t g_previousTickCount10sec;

void dumpVariables(char *buffer) {
    buffer[0] = 0;

    for (unsigned i = 0; i < getNumVariables(); ++i) {
        strcat(buffer, g_variables[i]->name());
        strcat(buffer, " = ");
        g_variables[i]->dump(buffer);
        strcat(buffer, "\n");
    }
}

uint32_t getNumVariables() {
    return sizeof(g_variables) / sizeof(DebugVariable *) - (CH_MAX - CH_NUM) * 6;
}

const char *getVariableName(int variableIndex) {
    return g_variables[variableIndex]->name();
}

void getVariableValue(int variableIndex, char *buffer) {
    g_variables[variableIndex]->dump(buffer);
}

uint32_t getVariableRefreshRateMs(int variableIndex) {
    return g_variables[variableIndex]->getRefreshRateMs();
}

void tick(uint32_t tickCount) {
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
