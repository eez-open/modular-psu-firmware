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

////////////////////////////////////////////////////////////////////////////////

static const float PERIOD_MIN = 0.001f;
static const float PERIOD_MAX = 120.0f;
static const float PERIOD_DEFAULT = 0.02f;

static const float DURATION_MIN = 1.0f;
static const float DURATION_MAX = 86400000.0f;
static const float DURATION_DEFAULT = 60.0f;

static const uint32_t WIDTH_PER_DIVISION = 40;

////////////////////////////////////////////////////////////////////////////////

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

    bool isDlogItemAvailable(int slotIndex, int subchannelIndex, int resourceIndex);
    bool isDlogItemAvailable(int slotIndex, int subchannelIndex, DlogResourceType resourceType);

    eez_err_t enableDlogItem(int slotIndex, int subchannelIndex, int resourceIndex, bool enable);
    eez_err_t enableDlogItem(int slotIndex, int subchannelIndex, DlogResourceType resourceType, bool enable);
    bool isDlogItemEnabled(int slotIndex, int subchannelIndex, int resourceIndex);
    bool isDlogItemEnabled(int slotIndex, int subchannelIndex, DlogResourceType resourceType);

    trigger::Source triggerSource = trigger::SOURCE_IMMEDIATE;

	static float getResourceMinPeriod(int slotIndex, int subchannelIndex, int resourceIndex);

	// returns the number of items that are disabled
	// because period is below min. period
	unsigned setPeriod(float value);

private:
    bool findDlogItemIndex(int slotIndex, int subchannelIndex, int resourceIndex, int &dlogItemIndex);
    bool findDlogItemIndex(int slotIndex, int subchannelIndex, DlogResourceType resourceType, int &dlogItemIndex);
};

struct DlogValueParams {
    int isVisible;
    double offset;
    double div;
};

struct Recording {
    Parameters parameters;

    DlogValueParams dlogValues[dlog_file::MAX_NUM_OF_Y_AXES];

    uint32_t size;

    double xAxisOffset;
    double xAxisDiv;

    double cursorOffset;

    float (*getValue)(uint32_t rowIndex, uint8_t columnIndex, float *max);

    uint32_t numSamples;
    double xAxisDivMin;
    double xAxisDivMax;

    uint32_t dataOffset;

    uint8_t selectedValueIndex;

    uint32_t columnDataIndexes[dlog_file::MAX_NUM_OF_Y_AXES];
    uint8_t columnBitMask[dlog_file::MAX_NUM_OF_Y_AXES];
    uint32_t numBytesPerRow;
};

////////////////////////////////////////////////////////////////////////////////

extern bool g_showLatest;

////////////////////////////////////////////////////////////////////////////////

// open dlog file for viewing
bool openFile(const char *filePath, int *err = nullptr);

extern State getState();

void loadSamples();
bool isValidSample(Recording &recording, uint8_t *rowData);
float getSample(Recording &recording, uint8_t *rowData, unsigned columnIndex);

// this is called from the thread that owns SD card
void loadBookmarks();

// this is called from the thread that owns SD card
void tick();

// this should be called during GUI state managment phase
void stateManagment();

Recording &getRecording();

void initDlogValues(Recording &recording);
void calcColumnIndexes(Recording &g_recording);
int getDlogValueIndex(Recording &recording, int visibleDlogValueIndex);
bool isMulipleValuesOverlayHeuristic(Recording &recording);

void getLabel(Recording& recording, int valueIndex, char *text, int count);

void uploadFile();

eez::gui::SetPage *getParamsPage();

float getSample(Recording &recording, uint8_t *rowData, unsigned columnIndex);

void appendLiveBookmark(uint32_t position, const char *bookmarkText, size_t bookmarkTextLen);

////////////////////////////////////////////////////////////////////////////////

} // namespace dlog_view
} // namespace psu
} // namespace eez
