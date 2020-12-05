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

#include <eez/modules/psu/trigger.h>
#include <eez/modules/psu/dlog_view.h>

namespace eez {
namespace psu {
namespace dlog_record {

static const float PERIOD_MIN = 0.001f;
static const float PERIOD_MAX = 120.0f;
static const float PERIOD_DEFAULT = 0.02f;

static const float TIME_MIN = 1.0f;
static const float TIME_MAX = 86400000.0f;
static const float TIME_DEFAULT = 60.0f;

extern double g_currentTime;
extern uint32_t g_fileLength;
extern dlog_view::Parameters g_parameters;
extern dlog_view::Recording g_recording;

enum State {
    STATE_IDLE,
    STATE_INITIATED,
    STATE_EXECUTING
};

extern State g_state;
extern bool g_inStateTransition;
extern bool g_traceInitiated;

inline State getState() { return g_state; }
inline bool isIdle() { return g_state == STATE_IDLE; }
inline bool isInitiated() { return g_state == STATE_INITIATED; }
inline bool isExecuting() { return g_state == STATE_EXECUTING; }
inline bool isTraceExecuting() { return g_state == STATE_EXECUTING && g_traceInitiated; }
inline bool isInStateTransition() { return g_inStateTransition; }

void setTriggerSource(trigger::Source source);

int checkDlogParameters(dlog_view::Parameters &parameters, bool doNotCheckFilePath, bool forTraceUsage);

int initiate();
int initiateTrace();
int startImmediately();
void triggerGenerated();
void toggleStart();
void toggleStop();
void abort();
void abortAfterSlaveOverflowError();
void reset();

void tick(uint32_t tick_usec);
void log(float *values);
void log(int slotIndex, int subchannelIndex, uint8_t data);

void fileWrite(bool flush = false);
void stateTransition(int event, int *perr = nullptr);

const char *getLatestFilePath();

} // namespace dlog_record
} // namespace psu
} // namespace eez
