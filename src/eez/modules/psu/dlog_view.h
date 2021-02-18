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

#include <eez/gui/gui.h>
#include <eez/gui/widgets/yt_graph.h>
#include <eez/modules/psu/trigger.h>

#include <eez/dlog_file.h>

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

static const float PERIOD_MIN = 0.001f;
static const float PERIOD_MAX = 120.0f;
static const float PERIOD_DEFAULT = 0.02f;

static const float DURATION_MIN = 1.0f;
static const float DURATION_MAX = 86400000.0f;
static const float DURATION_DEFAULT = 60.0f;

static const int VIEW_WIDTH = 480;
static const int VIEW_HEIGHT = 240;

static const int NUM_HORZ_DIVISIONS = 12;
static const int NUM_VERT_DIVISIONS = 6;

enum State {
    STATE_STARTING,
    STATE_LOADING,
    STATE_ERROR,
    STATE_READY
};

struct DlogItem {
	uint8_t slotIndex;
	uint8_t subchannelIndex;
	uint8_t resourceIndex;
	uint8_t resourceType;
};

struct Parameters : public dlog_file::Parameters {
	char filePath[MAX_PATH_LENGTH + 1] = { 0 };

    DlogItem dlogItems[dlog_file::MAX_NUM_OF_Y_AXES];
    int numDlogItems = 0;

    eez_err_t enableDlogItem(int slotIndex, int subchannelIndex, int resourceIndex, bool enable);
    eez_err_t enableDlogItem(int slotIndex, int subchannelIndex, DlogResourceType resourceType, bool enable);
    bool isDlogItemEnabled(int slotIndex, int subchannelIndex, int resourceIndex);
    bool isDlogItemEnabled(int slotIndex, int subchannelIndex, DlogResourceType resourceType);

    trigger::Source triggerSource = trigger::SOURCE_IMMEDIATE;

private:
    bool findDlogItemIndex(int slotIndex, int subchannelIndex, int resourceIndex, int &dlogItemIndex);
    bool findDlogItemIndex(int slotIndex, int subchannelIndex, DlogResourceType resourceType, int &dlogItemIndex);
};

struct DlogValueParams {
    bool isVisible;
    float offset;
    float div;
};

struct Recording {
    Parameters parameters;

    DlogValueParams dlogValues[MAX_NUM_OF_Y_VALUES];

    uint32_t size;
    uint32_t pageSize;

    float xAxisOffset;
    float xAxisDiv;

    uint32_t cursorOffset;

    float (*getValue)(uint32_t rowIndex, uint8_t columnIndex, float *max);

    uint32_t refreshCounter;

    uint32_t numSamples;
    float xAxisDivMin;
    float xAxisDivMax;

    uint32_t dataOffset;

    uint8_t selectedVisibleValueIndex;

    uint32_t columnFloatIndexes[dlog_file::MAX_NUM_OF_Y_AXES];
    uint32_t numFloatsPerRow;
};

extern bool g_showLatest;

// open dlog file for viewing
bool openFile(const char *filePath, int *err = nullptr);

extern State getState();

// this is called from the thread that owns SD card
void loadBlock();

// this should be called during GUI state managment phase
void stateManagment();

Recording &getRecording();

void initAxis(Recording &recording);
void initDlogValues(Recording &recording);
void calcColumnIndexes(Recording &g_recording);
int getNumVisibleDlogValues(const Recording &recording);
int getDlogValueIndex(Recording &recording, int visibleDlogValueIndex);
int getVisibleDlogValueIndex(Recording &recording, int dlogValueIndex);
DlogValueParams *getVisibleDlogValueParams(Recording &recording, int visibleDlogValueIndex);
bool isMulipleValuesOverlayHeuristic(Recording &recording);
Unit getXAxisUnit(Recording& recording);
Unit getYAxisUnit(Recording& recording, int dlogValueIndex);

uint32_t getPosition(Recording& recording);
void changeXAxisOffset(Recording &recording, float xAxisOffset);
void changeXAxisDiv(Recording &recording, float xAxisDiv);
void getLabel(Recording& recording, int valueIndex, char *text, int count);

void autoScale(Recording &recording);
void scaleToFit(Recording &recording);

void uploadFile();

eez::gui::SetPage *getParamsPage();

} // namespace dlog_view
} // namespace psu
} // namespace eez
