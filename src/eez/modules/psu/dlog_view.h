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

#include <eez/gui/data.h>
#include <eez/modules/psu/trigger.h>
#include <eez/modules/psu/dlog_view.h>

/* DLOG File Format

OFFSET    TYPE    WIDTH    DESCRIPTION
----------------------------------------------------------------------
0               U32     4        MAGIC1 = 0x2D5A4545L

4               U32     4        MAGIC2 = 0x474F4C44L

8               U16     2        VERSION = 0x0001L

10              U16     2        Flags: RRRR RRRR RRRR RRRJ
                                    J - Jitter enabled
                                    R - Reserverd

12              U32     4        Columns: RRRR RRRR RPIU RPIU RPIU RPIU RPIU RPIU
                                                     Ch6  Ch5  Ch4  Ch3  Ch2  Ch1
                                    U - voltage
                                    I - current
                                    P - power
                                    R - reserved

16              Float   4        Period

20              Float   4        Duration

24              U32     4        Start time, timestamp

28+(n*N+m)*4    Float   4        n-th row and m-th column value, N - number of columns
*/

namespace eez {
namespace psu {
namespace dlog_view {

static const uint32_t MAGIC1 = 0x2D5A4545;
static const uint32_t MAGIC2 = 0x474F4C44;
static const uint16_t VERSION = 1;
static const uint32_t DLOG_HEADER_SIZE = 28;

static const int NUM_HORZ_DIVISIONS = 12;
static const int NUM_VERT_DIVISIONS = 6;

static const int MAX_DLOG_VALUES = 6;

static const int MAX_VISIBLE_DLOG_VALUES = 2;

enum State {
    STATE_STARTING,
    STATE_LOADING,
    STATE_ERROR,
    STATE_READY
};

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

struct DlogValueParams {
    DlogValueType dlogValueType;
    eez::gui::data::Value perDiv;
    eez::gui::data::Value offset;
};

struct Options {
    bool logVoltage[CH_MAX];
    bool logCurrent[CH_MAX];
    bool logPower[CH_MAX];
    float period;
    float time;
};


struct Recording {
    uint32_t refreshCounter;

    uint32_t size;

    uint8_t totalDlogValues;
    uint8_t numVisibleDlogValues;
    DlogValueParams dlogValues[MAX_DLOG_VALUES];

    eez::gui::Value timeOffset;
    Options lastOptions;

    uint32_t pageSize;
    uint32_t cursorOffset;

    eez::gui::data::Value (*getValue)(int rowIndex, int columnIndex);
};

extern bool g_showLatest;

// open dlog file for viewing
void openFile(const char *filePath);

extern State getState();

// this is called from the thread that owns SD card
void loadBlock();

// this should be called during GUI state managment phase
void stateManagment();

Recording &getRecording();


} // namespace dlog_view
} // namespace psu
} // namespace eez
