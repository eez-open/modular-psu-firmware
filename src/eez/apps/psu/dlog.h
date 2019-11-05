/*
* EEZ PSU Firmware
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

#include <eez/apps/psu/trigger.h>

namespace eez {
namespace psu {
namespace dlog {

enum DlogValueType {
    DLOG_VALUE_CH1_U,
    DLOG_VALUE_CH1_I,
    DLOG_VALUE_CH1_P,
    DLOG_VALUE_CH2_U,
    DLOG_VALUE_CH2_I,
    DLOG_VALUE_CH2_P,
    DLOG_VALUE_CH3_U,
    DLOG_VALUE_CH3_I,
    DLOG_VALUE_CH3_P,
    DLOG_VALUE_CH4_U,
    DLOG_VALUE_CH4_I,
    DLOG_VALUE_CH4_P,
    DLOG_VALUE_CH5_U,
    DLOG_VALUE_CH5_I,
    DLOG_VALUE_CH5_P,
    DLOG_VALUE_CH6_U,
    DLOG_VALUE_CH6_I,
    DLOG_VALUE_CH6_P,
};

static const float PERIOD_MIN = 0.005f;
static const float PERIOD_MAX = 120.0f;
static const float PERIOD_DEFAULT = 0.02f;

static const float TIME_MIN = 1.0f;
static const float TIME_MAX = 86400000.0f;
static const float TIME_DEFAULT = 60.0f;

struct Options {
    bool logVoltage[CH_MAX];
    bool logCurrent[CH_MAX];
    bool logPower[CH_MAX];
    float period;
    float time;
};

extern Options g_nextOptions;

extern trigger::Source g_triggerSource;

extern Options g_lastOptions;
extern uint8_t *g_lastBufferStart;
extern uint8_t *g_lastBufferEnd;

struct DlogValueParams {
    DlogValueType type;
    eez::gui::data::Value perDiv;
    eez::gui::data::Value offset;
};

static const int MAX_DLOG_VALUES = 6;

static const int NUM_HORZ_DIVISIONS = 12;
static const int NUM_VERT_DIVISIONS = 6;

extern int g_numDlogValues;
extern DlogValueParams g_dlogValues[MAX_DLOG_VALUES];

extern eez::gui::Value g_timeOffset;

extern double g_currentTime;

extern uint32_t g_pageSize;
extern uint32_t g_cursorOffset;

bool isIdle();
bool isInitiated();
bool isExecuting();
int initiate(const char *filePath);
void triggerGenerated(bool startImmediatelly = true);
int startImmediately();
void abort();

void tick(uint32_t tick_usec);
void reset();

void executeDiskOperation(int diskOperation);

uint32_t getSize();

} // namespace dlog
} // namespace psu
} // namespace eez
