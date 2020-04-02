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

DebugCounterVariable g_adcCounter("ADC_COUNTER");
DebugValueVariable g_encoderCounter("ENC_COUNTER", 100);
DebugValueVariable g_uDac[CH_MAX] = { DebugValueVariable("CH1 U_DAC"), DebugValueVariable("CH2 U_DAC"), DebugValueVariable("CH3 U_DAC"), DebugValueVariable("CH4 U_DAC"), DebugValueVariable("CH5 U_DAC"), DebugValueVariable("CH6 U_DAC") };
DebugValueVariable g_uMon[CH_MAX] = { DebugValueVariable("CH1 U_MON"), DebugValueVariable("CH2 U_MON"), DebugValueVariable("CH3 U_MON"), DebugValueVariable("CH4 U_MON"), DebugValueVariable("CH5 U_MON"), DebugValueVariable("CH6 U_MON") };
DebugValueVariable g_uMonDac[CH_MAX] = { DebugValueVariable("CH1 U_MON_DAC"), DebugValueVariable("CH2 U_MON_DAC"), DebugValueVariable("CH3 U_MON_DAC"), DebugValueVariable("CH4 U_MON_DAC"), DebugValueVariable("CH5 U_MON_DAC"), DebugValueVariable("CH6 U_MON_DAC") };
DebugValueVariable g_iDac[CH_MAX] = { DebugValueVariable("CH1 I_DAC"), DebugValueVariable("CH2 I_DAC"), DebugValueVariable("CH3 I_DAC"), DebugValueVariable("CH4 I_DAC"), DebugValueVariable("CH5 I_DAC"), DebugValueVariable("CH6 I_DAC") };
DebugValueVariable g_iMon[CH_MAX] = { DebugValueVariable("CH1 I_MON"), DebugValueVariable("CH2 I_MON"), DebugValueVariable("CH3 I_MON"), DebugValueVariable("CH4 I_MON"), DebugValueVariable("CH5 I_MON"), DebugValueVariable("CH6 I_MON") };
DebugValueVariable g_iMonDac[CH_MAX] = { DebugValueVariable("CH1 I_MON_DAC"), DebugValueVariable("CH2 I_MON_DAC"), DebugValueVariable("CH3 I_MON_DAC"), DebugValueVariable("CH4 I_MON_DAC"), DebugValueVariable("CH5 I_MON_DAC"), DebugValueVariable("CH6 I_MON_DAC") };

DebugVariable *g_variables[] = { 
    &g_adcCounter,
    &g_encoderCounter,
    &g_uDac[0], &g_uMon[0], &g_uMonDac[0], &g_iDac[0], &g_iMon[0], &g_iMonDac[0],
    &g_uDac[1], &g_uMon[1], &g_uMonDac[1], &g_iDac[1], &g_iMon[1], &g_iMonDac[1],
    &g_uDac[2], &g_uMon[2], &g_uMonDac[2], &g_iDac[2], &g_iMon[2], &g_iMonDac[2],
    &g_uDac[3], &g_uMon[3], &g_uMonDac[3], &g_iDac[3], &g_iMon[3], &g_iMonDac[3],
    &g_uDac[4], &g_uMon[4], &g_uMonDac[4], &g_iDac[4], &g_iMon[4], &g_iMonDac[4],
    &g_uDac[5], &g_uMon[5], &g_uMonDac[5], &g_iDac[5], &g_iMon[5], &g_iMonDac[5],
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
