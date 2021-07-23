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

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <float.h>
#include <assert.h>

#include <eez/system.h>
#include <eez/hmi.h>

#include <eez/modules/psu/psu.h>
#include <eez/modules/psu/channel_dispatcher.h>
#include <eez/modules/psu/datetime.h>
#include <eez/modules/psu/dlog_view.h>
#include <eez/modules/psu/dlog_record.h>
#include <eez/modules/psu/scpi/psu.h>
#include <eez/modules/psu/serial_psu.h>
#include <eez/modules/psu/gui/psu.h>
#include <eez/modules/psu/gui/animations.h>
#include <eez/modules/psu/gui/edit_mode.h>
#if OPTION_ETHERNET
#include <eez/modules/psu/ethernet.h>
#endif

#include <eez/gui/gui.h>
#include <eez/gui/widgets/container.h>

#include <eez/libs/sd_fat/sd_fat.h>

#include <eez/memory.h>

namespace eez {

using namespace scpi;

namespace psu {
namespace dlog_view {

////////////////////////////////////////////////////////////////////////////////

static int VIEW_WIDTH = 480;
static int VIEW_HEIGHT = 240;

static int NUM_HORZ_DIVISIONS = 12;
static int NUM_VERT_DIVISIONS = 6;

static State g_state;
static uint32_t g_loadingStartTickCount;
bool g_showLatest = true;
char g_filePath[MAX_PATH_LENGTH + 1];
static Recording g_dlogFile;
static Recording g_previousDlogFile;
static uint32_t g_refreshCounter;

static uint8_t *g_rowDataStart = FILE_VIEW_BUFFER;
static const uint32_t ROW_DATA_BUFFER_SIZE = 65536;

static uint8_t *g_bookmarksDataStart = g_rowDataStart + ROW_DATA_BUFFER_SIZE;
struct Bookmark {
	uint32_t position;
	const char *text;
};
static Bookmark *g_bookmarks = (Bookmark *)g_bookmarksDataStart;
static const uint32_t BOOKMARKS_PER_PAGE = 8;
static const uint32_t BOOKMARKS_DATA_SIZE = 2 * (BOOKMARKS_PER_PAGE * (sizeof(Bookmark) + dlog_file::MAX_BOOKMARK_TEXT_LEN + 1));
static uint32_t g_selectedBookmarkIndex = -1;
static uint32_t g_bookmarksScrollPosition = 0;
static uint32_t g_loadedBookmarksScrollPosition = 0;
uint8_t *g_visibleBookmarksBuffer = g_bookmarksDataStart + BOOKMARKS_DATA_SIZE;
uint8_t *g_visibleBookmarks;
uint32_t *g_visibleBookmarkIndexesBuffer = (uint32_t *)(g_visibleBookmarksBuffer + 2 * 480);
uint32_t *g_visibleBookmarkIndexes;

struct BlockElement {
    float min;
    float max;
};
static BlockElement *g_blockElements = (BlockElement *)(g_visibleBookmarkIndexesBuffer + 2 * 480);
static uint32_t g_rowIndexStart;
static uint32_t g_rowIndexEnd;
static double g_loadScale;
static uint32_t g_loadedRowIndex;
static void loadBlockElements();

static bool g_fileIsOpen;
File g_file;
uint32_t g_cacheRowIndexStart;
uint32_t g_cacheRowIndexEnd;

static bool g_isLoading;
static bool g_refreshed;
static bool g_wasExecuting;

static enum {
    TAB_Y_VALUES,
	TAB_BOOKMARKS,
	TAB_OPTIONS
} g_selectedTab = TAB_Y_VALUES;

static const uint32_t Y_VALUES_PER_PAGE = 5;
static uint32_t g_yValuesScrollPosition = 0;

static Overlay g_multipleValuesOverlay;
static Overlay g_singleValueOverlay;

static Recording *g_cacheRecording;
static int g_cacheDlogValueIndex;
static float g_cacheMin;
static float g_cacheMax;

////////////////////////////////////////////////////////////////////////////////

static uint32_t getPosition(Recording& recording);
static void adjustXAxisOffset(Recording &recording);
static void loadVisibleBookmarks(uint32_t positionStart, uint32_t positionEnd);
static void changeXAxisDiv(Recording &recording, double xAxisDiv);

////////////////////////////////////////////////////////////////////////////////

void tick() {
	if (psu::gui::isPageOnStack(PAGE_ID_DLOG_VIEW) && &getRecording() == &g_dlogFile && g_state == STATE_READY) {
		loadSamples();

		if (g_bookmarksScrollPosition != g_loadedBookmarksScrollPosition) {
			loadBookmarks();
		}
	}
}

void loadSamples() {
    uint32_t rowIndexStart = dlog_view::getPosition(getRecording());
    uint32_t rowIndexEnd = rowIndexStart + VIEW_WIDTH;
    if (rowIndexStart > 0) {
        rowIndexStart = rowIndexStart - 1;
    }
    auto loadScale = g_dlogFile.xAxisDiv / g_dlogFile.xAxisDivMin;

    uint32_t numRows = rowIndexEnd - rowIndexStart;

    if (rowIndexStart != g_rowIndexStart || rowIndexEnd != g_rowIndexEnd || loadScale != g_loadScale) {
        uint32_t n = numRows * g_dlogFile.parameters.numYAxes;
        for (unsigned i = 0; i < n; i++) {
            g_blockElements[i].min = NAN;
            g_blockElements[i].max = NAN;
        }
        g_rowIndexStart = rowIndexStart;
        g_rowIndexEnd = rowIndexEnd;
        g_loadScale = loadScale;
        g_loadedRowIndex = 0;

        auto positionStart = (uint32_t)floorf(rowIndexStart * loadScale);
        auto positionEnd = (uint32_t)ceilf(rowIndexEnd * loadScale);
        loadVisibleBookmarks(positionStart, positionEnd);
    }

    if (g_loadedRowIndex < numRows) {
        loadBlockElements();

		g_cacheRecording = nullptr;
    }
}

State getState() {
    if (g_showLatest) {
        if (g_wasExecuting) {
            return STATE_READY;
        }
    }

    if (g_state == STATE_LOADING) {
        if (millis() - g_loadingStartTickCount < 1000) {
            return STATE_STARTING; // during 1st second of loading
        }
        return STATE_LOADING;
    }

    return g_state;
}

void stateManagment() {
    auto newViewWidth = persist_conf::devConf.viewFlags.dlogViewDrawerIsOpen ? 320 : 480;
    if (newViewWidth != VIEW_WIDTH) {
        VIEW_WIDTH = newViewWidth;
        NUM_HORZ_DIVISIONS = VIEW_WIDTH / WIDTH_PER_DIVISION;

        auto &recording = getRecording();
        if (&recording == &g_dlogFile) {
            g_dlogFile.xAxisDivMin = VIEW_WIDTH * g_dlogFile.parameters.xAxis.step / NUM_HORZ_DIVISIONS;
            g_dlogFile.xAxisDivMax = MAX(g_dlogFile.numSamples, (uint32_t)VIEW_WIDTH) * g_dlogFile.parameters.xAxis.step / NUM_HORZ_DIVISIONS;
            if (g_dlogFile.xAxisDiv < g_dlogFile.xAxisDivMin) {
                changeXAxisDiv(g_dlogFile, g_dlogFile.xAxisDivMin);
            } else if (g_dlogFile.xAxisDiv > g_dlogFile.xAxisDivMax) {
                changeXAxisDiv(g_dlogFile, g_dlogFile.xAxisDivMax);
            }
        }

        if (recording.cursorOffset >= (VIEW_WIDTH - 1) * recording.parameters.period) {
            recording.cursorOffset = (VIEW_WIDTH - 1) * recording.parameters.period;
        }
    }

    auto newViewHeight = persist_conf::devConf.viewFlags.dlogViewLegendViewOption == persist_conf::DLOG_VIEW_LEGEND_VIEW_OPTION_DOCK && !persist_conf::devConf.viewFlags.dlogViewDrawerIsOpen ? 204 : 240;
    if (newViewHeight != VIEW_HEIGHT) {
        VIEW_HEIGHT = newViewHeight;
    }

	auto isExecuting = dlog_record::isExecuting();
	if (!isExecuting && g_wasExecuting) {
		if (psu::gui::isPageOnStack(PAGE_ID_DLOG_VIEW)) {
			if (psu::gui::isPageOnStack(getExternalAssetsFirstPageId())) {
				psu::gui::removePageFromStack(PAGE_ID_DLOG_VIEW);
			} else {
				openFile(dlog_record::getLatestFilePath());
			}
		} else {
			if (psu::gui::isPageOnStack(getExternalAssetsFirstPageId())) {
			} else {
				gui::showPage(PAGE_ID_DLOG_VIEW);
				openFile(dlog_record::getLatestFilePath());
			}
		}
	} else if (isExecuting && !g_wasExecuting) {
		if (!psu::gui::isPageOnStack(getExternalAssetsFirstPageId())) {
            g_showLatest = true;
            if (!psu::gui::isPageOnStack(PAGE_ID_DLOG_VIEW)) {
                gui::showPage(PAGE_ID_DLOG_VIEW);
            } else {
                eez::gui::refreshScreen();
            }
		}
    }
	g_wasExecuting = isExecuting;

	if (g_refreshed) {
		++g_refreshCounter;
		g_refreshed = false;
	}
}

#define IS_VALID_SAMPLE(recording, rowData) !recording.parameters.dataContainsSampleValidityBit || *rowData & 0x80

bool isValidSample(Recording &recording, uint8_t *rowData) {
	return IS_VALID_SAMPLE(recording, rowData);
}

#define GET_SAMPLE(recording, rowData, columnIndex) \
    float value; \
	auto &yAxis = recording.parameters.yAxes[columnIndex]; \
	auto dataType = yAxis.dataType; \
	uint32_t columnDataIndex = recording.columnDataIndexes[columnIndex]; \
	auto columnData = rowData + columnDataIndex; \
	if (dataType == dlog_file::DATA_TYPE_BIT) { \
		value = *columnData & recording.columnBitMask[columnIndex] ? 1.0f : 0.0f; \
	} else if (dataType == dlog_file::DATA_TYPE_INT16_BE) { \
		auto iValue = int16_t((columnData[0] << 8) | columnData[1]); \
		value = float(yAxis.transformOffset + yAxis.transformScale * iValue); \
	} else if (dataType == dlog_file::DATA_TYPE_INT24_BE) { \
		auto iValue = ((int32_t)((columnData[0] << 24) | (columnData[1] << 16) | (columnData[2] << 8))) >> 8; \
		value = float(yAxis.transformOffset + yAxis.transformScale * iValue); \
	} else if (yAxis.dataType == dlog_file::DATA_TYPE_FLOAT) { \
		uint8_t *p = (uint8_t *)&value; \
		*p++ = *columnData++; \
		*p++ = *columnData++; \
		*p++ = *columnData++; \
		*p = *columnData; \
	} else { \
		assert(false); \
		value = NAN; \
	} \

float getSample(Recording &recording, uint8_t *rowData, unsigned columnIndex) {
	GET_SAMPLE(recording, rowData, columnIndex);
	return value;
}

static uint8_t *readMoreSamples(uint32_t rowIndex) {
	if (!g_fileIsOpen) {
		if (!g_file.open(g_filePath, FILE_OPEN_EXISTING | FILE_READ)) {
			return nullptr;
		}
		g_fileIsOpen = true;
	}

    auto filePosition = g_dlogFile.dataOffset + rowIndex * g_dlogFile.numBytesPerRow;

	if (!g_file.seek(filePosition)) {
		return nullptr;
	}
	
	auto numSamplesPerValue = (unsigned)round(g_loadScale);
    auto remaining = g_file.size() - filePosition;
    auto maxRequiredPerView = (g_rowIndexEnd - g_rowIndexStart) * numSamplesPerValue * g_dlogFile.numBytesPerRow;
	uint32_t bytesToRead = MIN(MIN(ROW_DATA_BUFFER_SIZE, remaining), maxRequiredPerView);
    bytesToRead = (bytesToRead / g_dlogFile.numBytesPerRow) * g_dlogFile.numBytesPerRow;

	uint32_t bytesRead = g_file.read(g_rowDataStart, bytesToRead);
	if (bytesRead != bytesToRead) {
		return nullptr;
	}

	g_cacheRowIndexStart = rowIndex;
	g_cacheRowIndexEnd = rowIndex + bytesRead / g_dlogFile.numBytesPerRow;

	return g_rowDataStart;
}

uint32_t g_liveBookmarkPoisition[480];

void appendLiveBookmark(uint32_t position, const char *bookmarkText, size_t bookmarkTextLen) {
	auto &recording = getRecording();

	auto bookmarks = g_bookmarks == (Bookmark *)g_bookmarksDataStart ? (Bookmark *)(g_bookmarksDataStart + BOOKMARKS_DATA_SIZE / 2) : (Bookmark *)g_bookmarksDataStart;

	recording.parameters.bookmarksSize++;

    int i;
    char *text = (char *)(bookmarks + BOOKMARKS_PER_PAGE);

    if (recording.parameters.bookmarksSize > BOOKMARKS_PER_PAGE) {
		for (uint32_t i = 1; i < BOOKMARKS_PER_PAGE; i++) {
			bookmarks[i-1].position = g_bookmarks[i].position;
			bookmarks[i-1].text = text;
			auto len = strlen(g_bookmarks[i].text);
			memcpy(text, g_bookmarks[i].text, len + 1);
			text += len + 1;
		}
		i = BOOKMARKS_PER_PAGE - 1;
	} else {
		memcpy(bookmarks, g_bookmarks, (recording.parameters.bookmarksSize - 1) * sizeof(Bookmark));

        i = recording.parameters.bookmarksSize - 1;
        if (i > 0) {
            text = (char *)(bookmarks[i-1].text + strlen(bookmarks[i-1].text) + 1);
        }
    }

    bookmarks[i].position = position;
    bookmarks[i].text = text;
    memcpy(text, bookmarkText, bookmarkTextLen);
    text[bookmarkTextLen] = 0;

	g_bookmarks = bookmarks;

    if (recording.parameters.bookmarksSize <= 480) {
        g_liveBookmarkPoisition[recording.parameters.bookmarksSize - 1] = position;
    } else {
        for (int i = 1; i < 480; i++) {
            g_liveBookmarkPoisition[i-1] = g_liveBookmarkPoisition[i];
        }
        g_liveBookmarkPoisition[480 - 1] = position;
    }
}

uint8_t *getLiveVisibleBookmarks() {
	auto &recording = getRecording();

	uint8_t *visibleBookmarks = g_visibleBookmarksBuffer;

	for (int i = 0; i < VIEW_WIDTH; i++) {
		visibleBookmarks[i] = 0;
	}

    for (int i = MIN(480, recording.parameters.bookmarksSize) - 1; i >= 0; i--) {
        int x = MIN((uint32_t)VIEW_WIDTH, recording.size) - (recording.size - g_liveBookmarkPoisition[i]);
        if (x < 0) {
            break;
        }
		visibleBookmarks[x] = 1;
    }

    return visibleBookmarks;
}

static void loadBlockElements() {
	auto numSamplesPerBlock = (unsigned)round(g_loadScale);

	uint32_t startTime = millis();

	if (g_loadedRowIndex == 0) {
		g_cacheRowIndexStart = 0;
		g_cacheRowIndexEnd = 0;
	}

	uint32_t n = g_rowIndexEnd - g_rowIndexStart;
	while (g_loadedRowIndex < n) {
		BlockElement *blockElementStart = g_blockElements + g_loadedRowIndex * g_dlogFile.parameters.numYAxes;

		auto rowIndex = (uint32_t)round((g_rowIndexStart + g_loadedRowIndex) * g_loadScale);

        uint8_t *rowData = g_rowDataStart + (rowIndex - g_cacheRowIndexStart) * g_dlogFile.numBytesPerRow;

		for (unsigned blockSampleIndex = 0; blockSampleIndex < numSamplesPerBlock; blockSampleIndex++) {
			if (rowIndex < g_cacheRowIndexStart || rowIndex >= g_cacheRowIndexEnd) {
				rowData = readMoreSamples(rowIndex);
				if (!rowData) {
					goto Exit;
				}
			}

            if (IS_VALID_SAMPLE(g_dlogFile, rowData)) {
				BlockElement *blockElement = blockElementStart;
				
				for (uint32_t columnIndex = 0; columnIndex < g_dlogFile.parameters.numYAxes; columnIndex++, blockElement++) {
					GET_SAMPLE(g_dlogFile, rowData, columnIndex);
                    if (blockSampleIndex == 0) {
                        blockElement->min = blockElement->max = value;
                    } else if (value < blockElement->min) {
                        blockElement->min = value;
                    } else if (value > blockElement->max) {
                        blockElement->max = value;
                    }
                }
            }

			rowIndex++;
            rowData += g_dlogFile.numBytesPerRow;
		}

		g_loadedRowIndex++;
		g_refreshed = true;

		if (millis() - startTime > 50) {
			break;
		}
	}

Exit:
	if (g_fileIsOpen) {
		g_file.close();
		g_fileIsOpen = false;
	}
}

static float getValue(uint32_t rowIndex, uint8_t columnIndex, float *max) {
	if (rowIndex >= g_rowIndexStart && rowIndex < g_rowIndexEnd) {
		BlockElement *blockElement = g_blockElements + (rowIndex - g_rowIndexStart) * g_dlogFile.parameters.numYAxes + columnIndex;

		if (g_dlogFile.parameters.yAxisScaleType == dlog_file::SCALE_LOGARITHMIC) {
			float logOffset = 1 - g_dlogFile.parameters.yAxes[columnIndex].range.min;
			*max = log10f(logOffset + blockElement->max);
			return log10f(logOffset + blockElement->min);
		}

		*max = blockElement->max;
		return blockElement->min;

	} else {
		*max = NAN;
		return NAN;
	}
}

static float getDuration(Recording &recording) {
	if (&recording == &g_dlogFile) {
		return (recording.numSamples - 1) * recording.parameters.xAxis.step;
	}

	return recording.size > 0 ? (recording.size - 1) * recording.parameters.xAxis.step : 0;
}

////////////////////////////////////////////////////////////////////////////////

static void getRangeOfScreenData(Recording &recording, int dlogValueIndex, float &totalMin, float &totalMax) {
	if (g_cacheRecording == &recording && g_cacheDlogValueIndex == dlogValueIndex) {
		totalMin = g_cacheMin;
		totalMax = g_cacheMax;
		return;
	}

	totalMin = FLT_MAX;
	totalMax = -FLT_MAX;
	auto startPosition = getPosition(recording);
	for (auto position = startPosition; position < startPosition + VIEW_WIDTH; position++) {
		float max;
		float min = recording.getValue(position, dlogValueIndex, &max);
		if (min < totalMin) {
			totalMin = min;
		}
		if (max > totalMax) {
			totalMax = max;
		}
	}

	g_cacheRecording = &recording;
	g_cacheDlogValueIndex = dlogValueIndex;
	g_cacheMin = totalMin;
	g_cacheMax = totalMax;
}

////////////////////////////////////////////////////////////////////////////////

static void adjustXAxisOffset(Recording &recording) {
    auto duration = getDuration(recording);
    if (recording.xAxisOffset + VIEW_WIDTH * recording.parameters.period > duration) {
        recording.xAxisOffset = duration - VIEW_WIDTH * recording.parameters.period;
    }
    if (recording.xAxisOffset < 0) {
        recording.xAxisOffset = 0;
    }
    sendMessageToLowPriorityThread(THREAD_MESSAGE_DLOG_LOAD_SAMPLES);
}

static Unit getXAxisUnit(Recording& recording) {
    return recording.parameters.xAxis.scaleType == dlog_file::SCALE_LINEAR ? recording.parameters.xAxis.unit : UNIT_UNKNOWN;
}

static Unit getYAxisUnit(Recording& recording, int dlogValueIndex) {
    return recording.parameters.yAxisScaleType == dlog_file::SCALE_LINEAR ? recording.parameters.yAxes[dlogValueIndex].unit : UNIT_UNKNOWN;
}

float getYAxisRangeMax(Recording& recording, int dlogValueIndex) {
    return recording.parameters.yAxes[dlogValueIndex].range.max;
}

float getYAxisResolution(Recording& recording, int dlogValueIndex) {
    if (recording.parameters.yAxes[dlogValueIndex].unit == UNIT_AMPER) {
        if (getYAxisRangeMax(recording, dlogValueIndex) <= 0.05f) {
            return 0.00001f;
        }
    }
    return 0.001f;
}

static uint32_t getPosition(Recording& recording) {
    if (&recording == &dlog_record::g_activeRecording) {
        return recording.size > (uint32_t)VIEW_WIDTH ? recording.size - VIEW_WIDTH : 0;
    } else {
        auto position = recording.xAxisOffset / recording.parameters.period;
        if (position < 0) {
            return 0;
        } else if (position > recording.size - VIEW_WIDTH) {
            return recording.size - VIEW_WIDTH;
        } else {
            return (uint32_t)round(position);
        }
    }
}

static void changeXAxisOffset(Recording &recording, double xAxisOffset) {
    if (&g_dlogFile == &recording) {
        auto newXAxisOffset = xAxisOffset;
        if (newXAxisOffset != recording.xAxisOffset) {
            recording.xAxisOffset = newXAxisOffset;
            adjustXAxisOffset(recording);
        }
    } else {
        recording.xAxisOffset = xAxisOffset;
    }
}

static void changeXAxisDiv(Recording &recording, double xAxisDiv) {
    if (recording.xAxisDiv != xAxisDiv) {
        recording.xAxisDiv = xAxisDiv;

        auto periodBefore = recording.parameters.period;

        if (recording.xAxisDiv == recording.xAxisDivMin) {
            recording.parameters.period = recording.parameters.xAxis.step;
            recording.size = recording.numSamples;
        } else {
            recording.parameters.period = recording.parameters.xAxis.step * recording.xAxisDiv / recording.xAxisDivMin;
            recording.size = (uint32_t)round(recording.numSamples * recording.xAxisDivMin / recording.xAxisDiv);
        }

        recording.cursorOffset *= recording.parameters.period / periodBefore;

        adjustXAxisOffset(recording);
    }
}

uint32_t getCursorPosition(Recording &recording) {
    int32_t cursorPosition = (int32_t)round(recording.cursorOffset / recording.parameters.period);
    if (cursorPosition < 0) {
        return 0;
    }
    if (cursorPosition > VIEW_WIDTH - 1) {
        return VIEW_WIDTH - 1;
    }
    return (uint32_t)cursorPosition;
}

void getLabel(Recording& recording, int valueIndex, char *text, int count) {
    if (recording.parameters.yAxes[valueIndex].label[0]) {
        stringCopy(text, count, recording.parameters.yAxes[valueIndex].label);
    } else {
        char dlogValueType;
        if (recording.parameters.yAxes[valueIndex].unit == UNIT_VOLT) {
            dlogValueType = 'U';
        } else if (recording.parameters.yAxes[valueIndex].unit == UNIT_AMPER) {
            dlogValueType = 'I';
        } else if (recording.parameters.yAxes[valueIndex].unit == UNIT_WATT) {
            dlogValueType = 'P';
        } else {
            dlogValueType = '?';
        }
        
        snprintf(text, count - 1, "%c%d", dlogValueType, recording.parameters.yAxes[valueIndex].channelIndex + 1);
    }
}

void initDlogValues(Recording &recording) {
    uint8_t yAxisIndex;

    int numBitValues = 0;
    for (yAxisIndex = 0; yAxisIndex < recording.parameters.numYAxes; yAxisIndex++) {
        if (recording.parameters.yAxes[yAxisIndex].dataType == dlog_file::DATA_TYPE_BIT) {
            numBitValues++;
        }
    }

    int bitValueIndex = 0;

    for (yAxisIndex = 0; yAxisIndex < recording.parameters.numYAxes; yAxisIndex++) {
        recording.dlogValues[yAxisIndex].isVisible = yAxisIndex < MAX_NUM_OF_Y_VALUES;

        float rangeMin = recording.parameters.yAxes[yAxisIndex].range.min;
        float rangeMax = recording.parameters.yAxes[yAxisIndex].range.max;

        if (recording.parameters.yAxes[yAxisIndex].dataType == dlog_file::DATA_TYPE_BIT) {
            float numDivisions = 1.0f * NUM_VERT_DIVISIONS / numBitValues;
            recording.dlogValues[yAxisIndex].div = 1.1f / numDivisions;
            recording.dlogValues[yAxisIndex].offset = recording.dlogValues[yAxisIndex].div * (NUM_VERT_DIVISIONS / 2 - (bitValueIndex++ + 1) * numDivisions);
        } else {
            if (recording.parameters.yAxisScaleType == dlog_file::SCALE_LOGARITHMIC) {
                float logOffset = 1 - rangeMin;
                rangeMin = 0;
                rangeMax = log10f(logOffset + rangeMax);
            }

            float div = (rangeMax - rangeMin) / NUM_VERT_DIVISIONS;
            recording.dlogValues[yAxisIndex].div = div;
            recording.dlogValues[yAxisIndex].offset = -rangeMin - div * NUM_VERT_DIVISIONS / 2;
        }
    }

    for (; yAxisIndex < dlog_file::MAX_NUM_OF_Y_AXES; yAxisIndex++) {
        recording.dlogValues[yAxisIndex].isVisible = false;
    }

    recording.selectedValueIndex = 0;
}

void calcColumnIndexes(Recording &recording) {
    uint32_t columnDataIndex = 0;
    uint32_t bitMask = 0;

    if (recording.parameters.dataContainsSampleValidityBit) {
        bitMask = 0x80;
    }

    for (int yAxisIndex = 0; yAxisIndex < recording.parameters.numYAxes; yAxisIndex++) {
        auto &yAxis = recording.parameters.yAxes[yAxisIndex];
        
        if (yAxis.dataType == dlog_file::DATA_TYPE_BIT) {
            if (bitMask == 0) {
                bitMask = 0x80;
            } else {
                bitMask >>= 1;
            }

            recording.columnDataIndexes[yAxisIndex] = columnDataIndex;
            recording.columnBitMask[yAxisIndex] = bitMask;

            if (bitMask == 1) {
                columnDataIndex += 1;
                bitMask = 0;
            }
        } else {
            if (bitMask != 0) {
                columnDataIndex += 1;
                bitMask = 0;
            }

            recording.columnDataIndexes[yAxisIndex] = columnDataIndex;

            if (yAxis.dataType == dlog_file::DATA_TYPE_INT16_BE) {
                columnDataIndex += 2;
            } else if (yAxis.dataType == dlog_file::DATA_TYPE_INT24_BE) {
                columnDataIndex += 3;
            } else if (yAxis.dataType == dlog_file::DATA_TYPE_FLOAT) {
                columnDataIndex += 4;
            } else {
                assert(false);
            }
        }
    }

    if (bitMask != 0) {
        columnDataIndex += 1;
    }

    recording.numBytesPerRow = columnDataIndex;
}

static int getNumVisibleDlogValues(const Recording &recording) {
    int count = 0;
    for (int dlogValueIndex = 0; dlogValueIndex < dlog_file::MAX_NUM_OF_Y_AXES; dlogValueIndex++) {
        if (recording.dlogValues[dlogValueIndex].isVisible) {
            count++;
        }
    }
    return count;
}

int getDlogValueIndex(Recording &recording, int visibleDlogValueIndex) {
    int i = 0;
    for (int dlogValueIndex = 0; dlogValueIndex < dlog_file::MAX_NUM_OF_Y_AXES; dlogValueIndex++) {
        if (recording.dlogValues[dlogValueIndex].isVisible) {
            if (i == visibleDlogValueIndex) {
                return dlogValueIndex;
            }
            i++;
        }
    }
    return -1;
}

static int getSelectedDlogValueIndex(Recording &recording, Cursor cursor) {
    return (
        cursor == -1 ||
        !dlog_view::isMulipleValuesOverlayHeuristic(recording) ||
        persist_conf::devConf.viewFlags.dlogViewLegendViewOption == persist_conf::DLOG_VIEW_LEGEND_VIEW_OPTION_DOCK
    ) ? recording.selectedValueIndex : dlog_view::getDlogValueIndex(recording, cursor);
}

bool compareWithPreviousDlogFile() {
    if (
        g_previousDlogFile.parameters.xAxis.range.min != g_dlogFile.parameters.xAxis.range.min ||
		g_previousDlogFile.parameters.xAxis.range.max != g_dlogFile.parameters.xAxis.range.max ||
        g_previousDlogFile.parameters.xAxis.scaleType != g_dlogFile.parameters.xAxis.scaleType ||
        fabs(g_previousDlogFile.parameters.xAxis.step - g_dlogFile.parameters.xAxis.step) > 0.05 * g_dlogFile.parameters.xAxis.step ||
        g_previousDlogFile.parameters.xAxis.unit != g_dlogFile.parameters.xAxis.unit
    ) {
        return false;
    }

    if (g_previousDlogFile.parameters.numYAxes != g_dlogFile.parameters.numYAxes) {
        return false;
    }

    for (int i = 0; i < g_previousDlogFile.parameters.numYAxes; i++) {
        if (
            g_previousDlogFile.parameters.yAxes[i].channelIndex != g_dlogFile.parameters.yAxes[i].channelIndex ||
            g_previousDlogFile.parameters.yAxes[i].dataType != g_dlogFile.parameters.yAxes[i].dataType ||
            g_previousDlogFile.parameters.yAxes[i].range.min != g_dlogFile.parameters.yAxes[i].range.min ||
			g_previousDlogFile.parameters.yAxes[i].range.max != g_dlogFile.parameters.yAxes[i].range.max ||
            g_previousDlogFile.parameters.yAxes[i].unit != g_dlogFile.parameters.yAxes[i].unit
        ) {
			return false;
        }
    }

    return true;
}

void copyParamsFromPreviousDlogFile() {
	for (int dlogValueIndex = 0; dlogValueIndex < g_previousDlogFile.parameters.numYAxes; dlogValueIndex++) {
		DlogValueParams &previousDlogValueParams = g_previousDlogFile.dlogValues[dlogValueIndex];
        DlogValueParams &dlogValueParams = g_dlogFile.dlogValues[dlogValueIndex];

        dlogValueParams.isVisible = previousDlogValueParams.isVisible;
        dlogValueParams.div = previousDlogValueParams.div;
        dlogValueParams.offset = previousDlogValueParams.offset;
	}

    changeXAxisDiv(g_dlogFile, g_previousDlogFile.xAxisDiv);
    g_dlogFile.xAxisOffset = g_previousDlogFile.xAxisOffset;
    adjustXAxisOffset(g_dlogFile);
    g_dlogFile.cursorOffset = g_previousDlogFile.cursorOffset;
}

bool isMulipleValuesOverlayHeuristic(Recording &recording) {
    // return true if X axis is time
    if (recording.parameters.xAxis.unit == UNIT_SECOND) {
        return true;
    }

    // return true if Y axis values have different units
    for (auto yAxisIndex = 1; yAxisIndex < recording.parameters.numYAxes; yAxisIndex++) {
        if (recording.parameters.yAxes[yAxisIndex].unit != recording.parameters.yAxes[0].unit) {
            return true;
        }
    }

    return false;
}

static void getRange(Recording &recording, int dlogValueIndex, float &rangeMin, float &rangeMax) {
    rangeMin = recording.parameters.yAxes[dlogValueIndex].range.min;
    rangeMax = recording.parameters.yAxes[dlogValueIndex].range.max;

    if (recording.parameters.yAxisScaleType == dlog_file::SCALE_LOGARITHMIC) {
        float logOffset = 1 - rangeMin;
        rangeMin = 0;
        rangeMax = log10f(logOffset + rangeMax);
    }
}

static void autoScale(Recording &recording) {
    auto numVisibleDlogValues = getNumVisibleDlogValues(recording);

    for (auto visibleDlogValueIndex = 0; visibleDlogValueIndex < numVisibleDlogValues; visibleDlogValueIndex++) {
        int dlogValueIndex = getDlogValueIndex(recording, visibleDlogValueIndex);
        DlogValueParams &dlogValueParams = recording.dlogValues[dlogValueIndex];

        float numDivisions = 1.0f * NUM_VERT_DIVISIONS / numVisibleDlogValues;

        float rangeMin;
        float rangeMax;
        getRange(recording, dlogValueIndex, rangeMin, rangeMax);

        if (recording.parameters.yAxes[dlogValueIndex].dataType == dlog_file::DATA_TYPE_BIT) {
            dlogValueParams.div = 1.1f / numDivisions;
            dlogValueParams.offset = dlogValueParams.div * (NUM_VERT_DIVISIONS / 2 - (visibleDlogValueIndex + 1) * numDivisions);
        } else {
            dlogValueParams.div = (rangeMax - rangeMin) / numDivisions;
            dlogValueParams.offset = -rangeMin + dlogValueParams.div * (NUM_VERT_DIVISIONS / 2 - (visibleDlogValueIndex + 1) * numDivisions);
        }
    }
}

static void scaleToFit(Recording &recording) {
    auto numVisibleDlogValues = getNumVisibleDlogValues(recording);
    auto startPosition = getPosition(recording);

    float totalMin = FLT_MAX;
    float totalMax = -FLT_MAX;

    for (auto position = startPosition; position < startPosition + VIEW_WIDTH; position++) {
        for (auto visibleDlogValueIndex = 0; visibleDlogValueIndex < numVisibleDlogValues; visibleDlogValueIndex++) {
            int dlogValueIndex = getDlogValueIndex(recording, visibleDlogValueIndex);

            float max;
            float min = recording.getValue(position, dlogValueIndex, &max);
            if (min < totalMin) {
                totalMin = min;
            }
            if (max > totalMax) {
                totalMax = max;
            }
        }
    }

	if (totalMin == totalMax) {
		int dlogValueIndex = getDlogValueIndex(recording, 0);

		totalMin = recording.parameters.yAxes[dlogValueIndex].range.min;
		totalMax = recording.parameters.yAxes[dlogValueIndex].range.max;
	}

    for (auto visibleDlogValueIndex = 0; visibleDlogValueIndex < numVisibleDlogValues; visibleDlogValueIndex++) {
        int dlogValueIndex = getDlogValueIndex(recording, visibleDlogValueIndex);
        DlogValueParams &dlogValueParams = recording.dlogValues[dlogValueIndex];
        float numDivisions = 1.0f * NUM_VERT_DIVISIONS;
        dlogValueParams.div = (totalMax - totalMin) / numDivisions;
        dlogValueParams.offset = -totalMin - dlogValueParams.div * NUM_VERT_DIVISIONS / 2;
    }
}

void setBookmarksScrollPosition(uint32_t scrollPosition) {
	auto &recording = getRecording();

	if (scrollPosition + BOOKMARKS_PER_PAGE > recording.parameters.bookmarksSize) {
		scrollPosition = recording.parameters.bookmarksSize - BOOKMARKS_PER_PAGE;
	}

    g_bookmarksScrollPosition = scrollPosition;
}

void loadBookmarks() {
	auto &recording = getRecording();

    auto scrollPosition = g_bookmarksScrollPosition;
	auto bookmarks = g_bookmarks == (Bookmark *)g_bookmarksDataStart ? (Bookmark *)(g_bookmarksDataStart + BOOKMARKS_DATA_SIZE / 2) : (Bookmark *)g_bookmarksDataStart;

	int n = MIN(recording.parameters.bookmarksSize - scrollPosition, BOOKMARKS_PER_PAGE);

	if (n <= 0) {
		return;
	}

    File file;
    if (!file.open(g_filePath, FILE_OPEN_EXISTING | FILE_READ)) {
        // TODO report error
        return;
    }

    uint32_t bookmarksIndexOffset = recording.dataOffset + recording.parameters.dataSize * recording.numBytesPerRow;

    if (!file.seek(bookmarksIndexOffset + scrollPosition * dlog_file::NUM_BYTES_PER_BOOKMARK_IN_INDEX)) {
        // TODO report error
    	file.close();
    	return;
    }

    uint8_t buffer[(BOOKMARKS_PER_PAGE + 1) * dlog_file::NUM_BYTES_PER_BOOKMARK_IN_INDEX];
    if (file.read(buffer, (n + 1) * dlog_file::NUM_BYTES_PER_BOOKMARK_IN_INDEX) != (n + 1) * dlog_file::NUM_BYTES_PER_BOOKMARK_IN_INDEX) {
        // TODO report error
    	file.close();
    	return;
    }

    auto text = (char *)(bookmarks + BOOKMARKS_PER_PAGE);

	uint32_t bookmarksTextOffset = bookmarksIndexOffset + (recording.parameters.bookmarksSize + 1) * dlog_file::NUM_BYTES_PER_BOOKMARK_IN_INDEX;
	
	uint8_t textLen[BOOKMARKS_PER_PAGE];
	uint32_t previous_textOffset = 0;
    for (int i = 0; i <= n; i++) {
		uint32_t textOffset = ((uint32_t *)(buffer + i * dlog_file::NUM_BYTES_PER_BOOKMARK_IN_INDEX))[1];
		if (i == 0) {
			bookmarksTextOffset += textOffset;
		} else {
			textLen[i - 1] = textOffset - previous_textOffset;
			text += textLen[i - 1] + 1;
		}

		if (i < n) {
			bookmarks[i].position = ((uint32_t *)(buffer + i * dlog_file::NUM_BYTES_PER_BOOKMARK_IN_INDEX))[0];
			bookmarks[i].text = text;
		}

		previous_textOffset = textOffset;
	}

    if (!file.seek(bookmarksTextOffset)) {
        // TODO report error
    	file.close();
    	return;
    }

    text = (char *)(bookmarks + BOOKMARKS_PER_PAGE);

    for (int i = 0; i < n; i++) {
        if (file.read(text, textLen[i]) != textLen[i]) {
            // TODO report error
        	file.close();
        	return;
        }
		text[textLen[i]] = 0;
        text += textLen[i] + 1;
    }

    g_loadedBookmarksScrollPosition = scrollPosition;
	g_bookmarks = bookmarks;

    file.close();
}

void loadVisibleBookmarks(uint32_t positionStart, uint32_t positionEnd) {
	auto &recording = getRecording();

    File file;
    if (!file.open(g_filePath, FILE_OPEN_EXISTING | FILE_READ)) {
        // TODO report error
        return;
    }

    uint32_t bookmarksIndexOffset = recording.dataOffset + recording.parameters.dataSize * recording.numBytesPerRow;

    if (!file.seek(bookmarksIndexOffset)) {
        // TODO report error
    	file.close();
    	return;
    }

	uint32_t a = 0;
	uint32_t b = recording.parameters.bookmarksSize - 1;
	while (a < b) {
		uint32_t c = (a + b) / 2;

		if (!file.seek(bookmarksIndexOffset + c * dlog_file::NUM_BYTES_PER_BOOKMARK_IN_INDEX)) {
			// TODO report error
			file.close();
			return;
		}

		uint32_t position;
		if (file.read(&position, sizeof(uint32_t)) != sizeof(uint32_t)) {
			// TODO report error
			file.close();
			return;
		}

		if (position < positionStart) {
			if (a == c) {
				break;
			}
			a = c;
		} else {
			if (b == c) {
				break;
			}
			b = c;
		}
	}

	uint8_t *visibleBookmarks = g_visibleBookmarks == g_visibleBookmarksBuffer ? g_visibleBookmarksBuffer + 480 : g_visibleBookmarksBuffer;
    uint32_t *visibleBookmarkIndexes = g_visibleBookmarkIndexes == g_visibleBookmarkIndexesBuffer ? g_visibleBookmarkIndexesBuffer + 480 : g_visibleBookmarkIndexesBuffer;

	for (int i = 0; i < VIEW_WIDTH; i++) {
		visibleBookmarks[i] = 0;
        visibleBookmarkIndexes[i] = 0;
	}

	for (; a < recording.parameters.bookmarksSize; a++) {
		if (!file.seek(bookmarksIndexOffset + a * dlog_file::NUM_BYTES_PER_BOOKMARK_IN_INDEX)) {
			// TODO report error
			file.close();
			return;
		}
		uint32_t position;
		if (file.read(&position, sizeof(uint32_t)) != sizeof(uint32_t)) {
			// TODO report error
			file.close();
			return;
		}
		
		if (position >= positionEnd) {
			break;
		}

		auto i = (int)roundf((position - positionStart) * recording.parameters.xAxis.step / recording.parameters.period);

        if (i >= 0 && i < 480) {
			visibleBookmarks[i] = 1;
            visibleBookmarkIndexes[i] = a;
        }
	}

	g_visibleBookmarks = visibleBookmarks;
    g_visibleBookmarkIndexes = visibleBookmarkIndexes;

    file.close();
}

void findNearestBookmark() {
    auto &recording = getRecording();
    auto i =  (uint32_t)round(recording.cursorOffset / g_dlogFile.parameters.period);
    if (i < 0) i = 0;
    if (i >= 480) i = 480 - 1;
	if (g_visibleBookmarks) {
		uint32_t bookmarkIndex = 0;
		bool found = false;
		for (int j = i, k = i; j < 480 || k >= 0; j++, k--) {
			if (j < 480 && g_visibleBookmarks[j]) {
				bookmarkIndex = g_visibleBookmarkIndexes[j];
				found = true;
				break;
			}
			if (k >= 0 && g_visibleBookmarks[k]) {
				bookmarkIndex = g_visibleBookmarkIndexes[k];
				found = true;
				break;
			}
		}
		if (found) {
			if (bookmarkIndex < g_bookmarksScrollPosition || bookmarkIndex >= g_bookmarksScrollPosition + BOOKMARKS_PER_PAGE) {
				g_bookmarksScrollPosition = bookmarkIndex - BOOKMARKS_PER_PAGE / 2;
				if (g_bookmarksScrollPosition < 0) {
					g_bookmarksScrollPosition = 0;
				} else if (g_bookmarksScrollPosition > recording.parameters.bookmarksSize - BOOKMARKS_PER_PAGE) {
					g_bookmarksScrollPosition = recording.parameters.bookmarksSize - BOOKMARKS_PER_PAGE;
				}
			}
			g_selectedBookmarkIndex = bookmarkIndex;
		}
	}
}

bool openFile(const char *filePath, int *err) {
    g_state = STATE_LOADING;
    g_loadingStartTickCount = millis();

	if (filePath) {
		stringCopy(g_filePath, sizeof(g_filePath), filePath);
	}

    if (!isLowPriorityThread()) {
        sendMessageToLowPriorityThread(THREAD_MESSAGE_DLOG_SHOW_FILE);
        return true;
    }

	g_previousDlogFile = g_dlogFile;

#if defined(EEZ_PLATFORM_STM32)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wclass-memaccess"
#endif

	memset(&g_dlogFile, 0, sizeof(Recording));

#if defined(EEZ_PLATFORM_STM32)
#pragma GCC diagnostic pop
#endif

	g_selectedBookmarkIndex = -1;
	g_bookmarksScrollPosition = 0;
	g_visibleBookmarks = 0;

    File file;
    if (file.open(g_filePath, FILE_OPEN_EXISTING | FILE_READ)) {
        uint8_t *buffer = FILE_VIEW_BUFFER;
        uint32_t read = file.read(buffer, dlog_file::DLOG_VERSION1_HEADER_SIZE);
        if (read == dlog_file::DLOG_VERSION1_HEADER_SIZE) {
			dlog_file::Reader reader(buffer);
		
			uint32_t headerRemaining;
			bool result = reader.readFileHeaderAndMetaFields(g_dlogFile.parameters, headerRemaining);
			if (result) {
				if (reader.getVersion() == dlog_file::VERSION1) {
					uint32_t columns = reader.getColumns();

					for (int channelIndex = 0; channelIndex < CH_MAX; ++channelIndex) {
						if (columns & (1 << (4 * channelIndex))) {
							g_dlogFile.parameters.enableDlogItem(255, channelIndex, 0, true);
						}

						if (columns & (2 << (4 * channelIndex))) {
							g_dlogFile.parameters.enableDlogItem(255, channelIndex, 1, true);
						}

						if (columns & (4 << (4 * channelIndex))) {
							g_dlogFile.parameters.enableDlogItem(255, channelIndex, 2, true);
						}
					}

                    auto initAxis = [] () {
                        Recording &recording = g_dlogFile;

                        recording.parameters.xAxis.unit = UNIT_SECOND;
                        recording.parameters.xAxis.step = recording.parameters.period;
                        recording.parameters.xAxis.range.min = 0;
                        recording.parameters.xAxis.range.max = recording.parameters.duration;

                        for (int8_t dlogItemIndex = 0; dlogItemIndex < recording.parameters.numDlogItems; ++dlogItemIndex) {
                            auto &dlogItem = recording.parameters.dlogItems[dlogItemIndex];
                            auto &yAxis = recording.parameters.yAxes[dlogItemIndex];
                            if (dlogItem.resourceType == DLOG_RESOURCE_TYPE_U) {
                                yAxis.unit = UNIT_VOLT;
                                yAxis.dataType = dlog_file::DATA_TYPE_FLOAT;
                                yAxis.range.min = 0;
                                yAxis.range.max = 40.0f;
                                yAxis.channelIndex = dlogItem.subchannelIndex;
                            } else if (dlogItem.resourceType == DLOG_RESOURCE_TYPE_I) {
                                yAxis.unit = UNIT_AMPER;
                                yAxis.dataType = dlog_file::DATA_TYPE_FLOAT;
                                yAxis.range.min = 0;
                                yAxis.range.max = 5.0f;
                                yAxis.channelIndex = dlogItem.subchannelIndex;
                            } else if (dlogItem.resourceType == DLOG_RESOURCE_TYPE_P) {
                                yAxis.unit = UNIT_WATT;
                                yAxis.dataType = dlog_file::DATA_TYPE_FLOAT;
                                yAxis.range.min = 0;
                                yAxis.range.max = 200.0f;
                                yAxis.channelIndex = dlogItem.subchannelIndex;
                            }
							*yAxis.label = 0;
                        }

                        recording.parameters.numYAxes = recording.parameters.numDlogItems;
					};

					initAxis();
				}
				else {
					if (headerRemaining > 0) {
						uint32_t read = file.read(buffer + dlog_file::DLOG_VERSION1_HEADER_SIZE, headerRemaining);
						if (read != headerRemaining) {
							result = false;
						}
					}
					if (result) {
						result = reader.readRemainingFileHeaderAndMetaFields(g_dlogFile.parameters);
					}
				}

				if (result) {
					g_dlogFile.dataOffset = reader.getDataOffset();

					initDlogValues(g_dlogFile);
					calcColumnIndexes(g_dlogFile);

					g_dlogFile.numSamples = g_dlogFile.parameters.dataSize ?
						g_dlogFile.parameters.dataSize :
						(file.size() - g_dlogFile.dataOffset) / g_dlogFile.numBytesPerRow;

					g_dlogFile.xAxisDivMin = VIEW_WIDTH * g_dlogFile.parameters.period / NUM_HORZ_DIVISIONS;
					g_dlogFile.xAxisDivMax = MAX(g_dlogFile.numSamples, (uint32_t)VIEW_WIDTH) * g_dlogFile.parameters.period / NUM_HORZ_DIVISIONS;

					g_dlogFile.size = g_dlogFile.numSamples;

					g_dlogFile.xAxisOffset = 0.0;
					g_dlogFile.xAxisDiv = g_dlogFile.xAxisDivMin;

					g_dlogFile.cursorOffset = VIEW_WIDTH * g_dlogFile.parameters.period / 2;

					g_dlogFile.getValue = getValue;
					g_isLoading = false;

                    if (compareWithPreviousDlogFile()) {
                        copyParamsFromPreviousDlogFile();
                    } else if (isMulipleValuesOverlayHeuristic(g_dlogFile)) {
						autoScale(g_dlogFile);
					}

					g_state = STATE_READY;

					// DebugTrace("Duration: %f\n", (float)g_dlogFile.parameters.finalDuration);
					// DebugTrace("No. samples: %d\n", g_dlogFile.numSamples);
					// DebugTrace("Sample rate: %f\n", (float)((g_dlogFile.numSamples - 1) / g_dlogFile.parameters.finalDuration));
				}
			}
        }

        if (g_state != STATE_READY) {
            if (err) {
                // TODO
                *err = SCPI_ERROR_MASS_STORAGE_ERROR;
            }
        }
    } else {
        if (err) {
            *err = SCPI_ERROR_FILE_NOT_FOUND;
        }
    }

    file.close();

    if (g_state == STATE_READY) {
		g_rowIndexStart = 0;
		g_rowIndexEnd = 0;
		g_loadScale = 1.0;

        loadBookmarks();
    } else {
        g_state = STATE_ERROR;
    }

    return g_state != STATE_ERROR;
}

Recording &getRecording() {
    return g_showLatest && g_wasExecuting ? dlog_record::g_activeRecording : g_dlogFile;
}

float roundValueOnXAxis(Recording &recording, float value) {
    return roundPrec(value, recording.parameters.xAxis.step);
}

float roundValueOnYAxis(Recording &recording, int yAxisIndex, float value) {
	float prec;
	if (fabs(value) < 1E-5f) {
		prec = 1E-5f;
	} else {
		prec = powf(10.0f, floorf(log10f(fabs(value)))) / 1000.0f;
	}
    return roundPrec(value, prec);
}

void uploadFile() {
    if (!isLowPriorityThread()) {
        sendMessageToLowPriorityThread(THREAD_MESSAGE_DLOG_UPLOAD_FILE);
        return;
    }

    scpi_t *context = nullptr;

#if !defined(EEZ_PLATFORM_SIMULATOR)
    if (psu::serial::isConnected()) {
        context = &psu::serial::g_scpiContext;
    }
#endif

#if OPTION_ETHERNET
    if (!context && psu::ethernet::isConnected()) {
        context = &psu::ethernet::g_scpiContext;
    }
#endif

    if (!context) {
        return;
    }

    int err;
    psu::scpi::mmemUpload(g_filePath, context, &err);
}

////////////////////////////////////////////////////////////////////////////////

bool Parameters::isDlogItemAvailable(int slotIndex, int subchannelIndex, int resourceIndex) {
    if (getResourceMinPeriod(slotIndex, subchannelIndex, resourceIndex) > period) {
        return false;
    }

	if (isDlogItemEnabled(slotIndex, subchannelIndex, resourceIndex)) {
		return true;
	}

	if (numDlogItems >= dlog_file::MAX_NUM_OF_Y_AXES) {
		return false;
	}

	// If period is less then PERIOD_MIN (in which case, logging is controlled by the module)
	// then do not allow logging from different modules.
	if (period < PERIOD_MIN) {
		if (!g_slots[slotIndex]->isDlogPeriodAllowed(subchannelIndex, resourceIndex, period)) {
			return false;
		}

		if (numDlogItems > 0 && dlogItems[0].slotIndex != slotIndex) {
			return false;
		}
	}

	return true;
}

bool Parameters::isDlogItemAvailable(int slotIndex, int subchannelIndex, DlogResourceType resourceType) {
	int numResources = g_slots[slotIndex]->getNumDlogResources(subchannelIndex);
	for (int resourceIndex = 0; resourceIndex < numResources; resourceIndex++) {
		if (g_slots[slotIndex]->getDlogResourceType(subchannelIndex, resourceIndex) == resourceType) {
			return isDlogItemAvailable(slotIndex, subchannelIndex, resourceIndex);
		}
	}
	return false;
}

eez_err_t Parameters::enableDlogItem(int slotIndex, int subchannelIndex, int resourceIndex, bool enable) {
	int dlogItemIndex;
	bool enabled = findDlogItemIndex(slotIndex, subchannelIndex, resourceIndex, dlogItemIndex);
	if (enable) {
		if (!enabled) {
			if (numDlogItems == dlog_file::MAX_NUM_OF_Y_AXES) {
				return SCPI_ERROR_TOO_MUCH_DATA;
			}

			int numResources = g_slots[slotIndex]->getNumDlogResources(subchannelIndex);
			if (resourceIndex >= numResources) {
				return SCPI_ERROR_HARDWARE_MISSING;
			}

			if (!isDlogItemAvailable(slotIndex, subchannelIndex, resourceIndex)) {
				return SCPI_ERROR_HARDWARE_MISSING;
			}

			if (dlogItemIndex < numDlogItems) {
				for (int i = numDlogItems - 1; i >= dlogItemIndex; i--) {
					memcpy(dlogItems + i + 1, dlogItems + i, sizeof(DlogItem));
				}
			}

			dlogItems[dlogItemIndex].slotIndex = slotIndex;
			dlogItems[dlogItemIndex].subchannelIndex = subchannelIndex;
			dlogItems[dlogItemIndex].resourceIndex = resourceIndex;
			dlogItems[dlogItemIndex].resourceType = g_slots[slotIndex]->getDlogResourceType(subchannelIndex, resourceIndex);

			numDlogItems++;

			refreshScreen();
		}
	} else {
		if (enabled) {
			for (int i = dlogItemIndex + 1; i < numDlogItems; i++) {
				memcpy(dlogItems + i - 1, dlogItems + i, sizeof(DlogItem));
			}

			numDlogItems--;

			dlogItems[numDlogItems].slotIndex = 0;
			dlogItems[numDlogItems].subchannelIndex = 0;
			dlogItems[numDlogItems].resourceIndex = 0;
			dlogItems[numDlogItems].resourceType = 0;

			refreshScreen();
		}
	}

	return SCPI_RES_OK;
}

eez_err_t Parameters::enableDlogItem(int slotIndex, int subchannelIndex, DlogResourceType resourceType, bool enable) {
	int numResources = g_slots[slotIndex]->getNumDlogResources(subchannelIndex);
	for (int resourceIndex = 0; resourceIndex < numResources; resourceIndex++) {
		if (g_slots[slotIndex]->getDlogResourceType(subchannelIndex, resourceIndex) == resourceType) {
			return enableDlogItem(slotIndex, subchannelIndex, resourceIndex, enable);
		}
	}
	return SCPI_ERROR_HARDWARE_MISSING;
}

bool Parameters::isDlogItemEnabled(int slotIndex, int subchannelIndex, int resourceIndex) {
	int dlogItemIndex;
	bool enabled = findDlogItemIndex(slotIndex, subchannelIndex, resourceIndex, dlogItemIndex);
	(void)dlogItemIndex;
	return enabled;
}

bool Parameters::isDlogItemEnabled(int slotIndex, int subchannelIndex, DlogResourceType resourceType) {
	int dlogItemIndex;
	bool enabled = findDlogItemIndex(slotIndex, subchannelIndex, resourceType, dlogItemIndex);
	(void)dlogItemIndex;
	return enabled;
}

bool Parameters::findDlogItemIndex(int slotIndex, int subchannelIndex, int resourceIndex, int &dlogItemIndex) {
	for (dlogItemIndex = 0; dlogItemIndex < numDlogItems; dlogItemIndex++) {
		if (slotIndex < dlogItems[dlogItemIndex].slotIndex) {
			return false;
		}

		if (slotIndex == dlogItems[dlogItemIndex].slotIndex) {
			if (subchannelIndex < dlogItems[dlogItemIndex].subchannelIndex) {
				return false;
			}

			if (subchannelIndex == dlogItems[dlogItemIndex].subchannelIndex) {
				if (resourceIndex < dlogItems[dlogItemIndex].resourceIndex) {
					return false;
				}

				if (resourceIndex == dlogItems[dlogItemIndex].resourceIndex) {
					return true;
				}
			}
		}
	}
	return false;
}

bool Parameters::findDlogItemIndex(int slotIndex, int subchannelIndex, DlogResourceType resourceType, int &dlogItemIndex) {
	for (dlogItemIndex = 0; dlogItemIndex < numDlogItems; dlogItemIndex++) {
		if (slotIndex < dlogItems[dlogItemIndex].slotIndex) {
			return false;
		}

		if (slotIndex == dlogItems[dlogItemIndex].slotIndex) {
			if (subchannelIndex < dlogItems[dlogItemIndex].subchannelIndex) {
				return false;
			}

			if (subchannelIndex == dlogItems[dlogItemIndex].subchannelIndex) {
				if (resourceType < dlogItems[dlogItemIndex].resourceType) {
					return false;
				}

				if (resourceType == dlogItems[dlogItemIndex].resourceType) {
					return true;
				}
			}
		}
	}
	return false;
}

float Parameters::getResourceMinPeriod(int slotIndex, int subchannelIndex, int resourceIndex) {
	float minPeriod = g_slots[slotIndex]->getDlogResourceMinPeriod(subchannelIndex, resourceIndex);
	if (isNaN(minPeriod)) {
		return PERIOD_MIN;
	}
	return minPeriod;
}

unsigned Parameters::setPeriod(float value) {
	period = value;

	int numDisabled = 0;

	for (int slotIndex = 0; slotIndex < NUM_SLOTS; slotIndex++) {
		int numSubchannels = g_slots[slotIndex]->getNumSubchannels();
		for (int subchannelIndex = 0; subchannelIndex < numSubchannels; subchannelIndex++) {
			int numResources = g_slots[slotIndex]->getNumDlogResources(subchannelIndex);
			for (int resourceIndex = 0; resourceIndex < numResources; resourceIndex++) {
				if (isDlogItemEnabled(slotIndex, subchannelIndex, resourceIndex)) {
					if (!isDlogItemAvailable(slotIndex, subchannelIndex, resourceIndex)) {
						enableDlogItem(slotIndex, subchannelIndex, resourceIndex, false);
						numDisabled++;
					}
				}
			}
		}
	}

	return numDisabled;
}

////////////////////////////////////////////////////////////////////////////////

class DlogParamsPage : public SetPage {
public:
    static Parameters g_parameters;
    static const int PAGE_SIZE = 6;
    static int g_scrollPosition;
	static float g_minPeriod;

    void pageAlloc() {
        g_numDlogResources = countDlogResources();

        memcpy(&g_parameters, &dlog_record::g_recordingParameters, sizeof(Parameters));
        memcpy(&g_parametersOrig, &dlog_record::g_recordingParameters, sizeof(Parameters));

		g_minPeriod = getMinPeriod();
    }

    int getDirty() {
        return memcmp(&g_parameters, &g_parametersOrig, sizeof(Parameters)) != 0;
    }

    void set() {
        gui::popPage();

        memcpy(&dlog_record::g_recordingParameters, &DlogParamsPage::g_parameters, sizeof(DlogParamsPage::g_parameters));
    }

    void start() {
        char filePath[MAX_PATH_LENGTH + 1];

        int slotIndex = getModuleLocalRecordingSlotIndex();

        int n;

        if (isStringEmpty(DlogParamsPage::g_parameters.filePath)) {
            uint8_t year, month, day, hour, minute, second;
            datetime::getDateTime(year, month, day, hour, minute, second);

            if (persist_conf::devConf.dateTimeFormat == datetime::FORMAT_DMY_24) {
                if (slotIndex != -1) {
                    n = snprintf(filePath, sizeof(filePath), "%d:%s/%s%02d_%02d_%02d-%02d_%02d_%02d.dlog",
                        slotIndex + 1,
                        RECORDINGS_DIR,
                        DlogParamsPage::g_parameters.filePath,
                        (int)day, (int)month, (int)year,
                        (int)hour, (int)minute, (int)second);
                } else {
                    n = snprintf(filePath, sizeof(filePath), "%s/%s%02d_%02d_%02d-%02d_%02d_%02d.dlog",
                        RECORDINGS_DIR,
                        DlogParamsPage::g_parameters.filePath,
                        (int)day, (int)month, (int)year,
                        (int)hour, (int)minute, (int)second);
                }
            } else if (persist_conf::devConf.dateTimeFormat == datetime::FORMAT_MDY_24) {
                if (slotIndex != -1) {
                    n = snprintf(filePath, sizeof(filePath), "%d:%s/%s%02d_%02d_%02d-%02d_%02d_%02d.dlog",
                        slotIndex + 1,
                        RECORDINGS_DIR,
                        DlogParamsPage::g_parameters.filePath,
                        (int)month, (int)day, (int)year,
                        (int)hour, (int)minute, (int)second);
                } else {
                    n = snprintf(filePath, sizeof(filePath), "%s/%s%02d_%02d_%02d-%02d_%02d_%02d.dlog",
                        RECORDINGS_DIR,
                        DlogParamsPage::g_parameters.filePath,
                        (int)month, (int)day, (int)year,
                        (int)hour, (int)minute, (int)second);
                }
            } else if (persist_conf::devConf.dateTimeFormat == datetime::FORMAT_DMY_12) {
                bool am;
                datetime::convertTime24to12(hour, am);
                if (slotIndex != -1) {
                    n = snprintf(filePath, sizeof(filePath), "%d:%s/%s%02d_%02d_%02d-%02d_%02d_%02d_%s.dlog",
                        slotIndex + 1,
                        RECORDINGS_DIR,
                        DlogParamsPage::g_parameters.filePath,
                        (int)day, (int)month, (int)year,
                        (int)hour, (int)minute, (int)second, am ? "AM" : "PM");
                } else {
                    n = snprintf(filePath, sizeof(filePath), "%s/%s%02d_%02d_%02d-%02d_%02d_%02d_%s.dlog",
                        RECORDINGS_DIR,
                        DlogParamsPage::g_parameters.filePath,
                        (int)day, (int)month, (int)year,
                        (int)hour, (int)minute, (int)second, am ? "AM" : "PM");
                }
            } else {
				// persist_conf::devConf.dateTimeFormat == datetime::FORMAT_MDY_12
                bool am;
                datetime::convertTime24to12(hour, am);
                if (slotIndex != -1) {
                    n = snprintf(filePath, sizeof(filePath), "%d:%s/%s%02d_%02d_%02d-%02d_%02d_%02d_%s.dlog",
                        slotIndex + 1,
                        RECORDINGS_DIR,
                        DlogParamsPage::g_parameters.filePath,
                        (int)month, (int)day, (int)year,
                        (int)hour, (int)minute, (int)second, am ? "AM" : "PM");
                } else {
                    n = snprintf(filePath, sizeof(filePath), "%s/%s%02d_%02d_%02d-%02d_%02d_%02d_%s.dlog",
                        RECORDINGS_DIR,
                        DlogParamsPage::g_parameters.filePath,
                        (int)month, (int)day, (int)year,
                        (int)hour, (int)minute, (int)second, am ? "AM" : "PM");
                }
            }
        } else {
            int slotIndex = getModuleLocalRecordingSlotIndex();
            if (slotIndex != -1) {
                n = snprintf(filePath, sizeof(filePath), "%d:%s/%s.dlog", slotIndex + 1, RECORDINGS_DIR, DlogParamsPage::g_parameters.filePath);
            } else {
                n = snprintf(filePath, sizeof(filePath), "%s/%s.dlog", RECORDINGS_DIR, DlogParamsPage::g_parameters.filePath);
            }
        }

        if (n >= MAX_PATH_LENGTH + 1) {
            psu::gui::errorMessage("File name too big!");
            return;
        }

        gui::popPage();

        DlogParamsPage::g_parameters.comment[0] = 0;
        DlogParamsPage::g_parameters.xAxis.label[0] = 0;
        for (int i = 0; i < DlogParamsPage::g_parameters.numYAxes; i++) {
            DlogParamsPage::g_parameters.yAxes[i].label[0] = 0;
        }

        memcpy(&dlog_record::g_recordingParameters, &DlogParamsPage::g_parameters, sizeof(DlogParamsPage::g_parameters));

        stringCopy(dlog_record::g_recordingParameters.filePath, sizeof(dlog_record::g_recordingParameters.filePath), filePath);

        dlog_record::toggleStart();
    }

    static void setScrollPosition(int scrollPosition) {
        if (g_numDlogResources <= PAGE_SIZE) {
            return;
        }

        if (scrollPosition < 0) {
            scrollPosition = 0;
        } else if (scrollPosition > g_numDlogResources - PAGE_SIZE) {
            scrollPosition = g_numDlogResources - PAGE_SIZE;
        }
        
        if (scrollPosition != DlogParamsPage::g_scrollPosition) {
            DlogParamsPage::g_scrollPosition = scrollPosition;
            refreshScreen();
        }
	}

    void onEncoder(int counter) {
#if defined(EEZ_PLATFORM_SIMULATOR)
        counter = -counter;
#endif
        setScrollPosition(g_scrollPosition + counter);
    }

    static int getNumDlogResources() {
        if (g_numDlogResources == -1) {
            g_numDlogResources = countDlogResources();
        }
        return g_numDlogResources;
    }

    static bool findResource(int absoluteResourceIndex, int &slotIndex, int &subchannelIndex, int &resourceIndex) {
        int index = 0;
        for (slotIndex = 0; slotIndex < NUM_SLOTS; slotIndex++) {
            int numSubchannels = g_slots[slotIndex]->getNumSubchannels();
            for (subchannelIndex = 0; subchannelIndex < numSubchannels; subchannelIndex++) {
                int numResources = g_slots[slotIndex]->getNumDlogResources(subchannelIndex);
                for (resourceIndex = 0; resourceIndex < numResources; resourceIndex++) {
                    if (index == absoluteResourceIndex) {
                        return true;
                    }
                    index++;
                }
            }
        }

        return false;
    }

	static float getResourceMinPeriod(int slotIndex, int subchannelIndex, int resourceIndex) {
		return Parameters::getResourceMinPeriod(slotIndex, subchannelIndex, resourceIndex);
	}

	static void setPeriod(float value) {
		g_parameters.period = value;
		int numUnchecked = g_parameters.setPeriod(value);
		if (numUnchecked > 0) {
			psu::gui::infoMessage("Some resources unchecked.");
		}
	}

    static void editFileName() {
        psu::gui::Keypad::startPush(0, DlogParamsPage::g_parameters.filePath, 0, MAX_PATH_LENGTH, false, onSetFileName, 0);
    }

    static void selectTriggerSource() {
        psu::gui::pushSelectFromEnumPage(ENUM_DEFINITION_TRIGGER_SOURCE, g_parameters.triggerSource, 0, onSelectTriggerSource);
    }

    static bool isModuleLocalRecording() {
        // TODO not supported for now
        return false;
    }

	static int getModuleLocalRecordingSlotIndex() {
		if (isModuleLocalRecording()) {
			if (g_parameters.numDlogItems > 0) {
				return g_parameters.dlogItems[0].slotIndex;
			}
		}
		
		return -1;
	}

private:
    static Parameters g_parametersOrig;
    static int g_numDlogResources;

    static int countDlogResources() {
        int count = 0;
        for (int slotIndex = 0; slotIndex < NUM_SLOTS; slotIndex++) {
            int numSubchannels = g_slots[slotIndex]->getNumSubchannels();
            for (int subchannelIndex = 0; subchannelIndex < numSubchannels; subchannelIndex++) {
                count += g_slots[slotIndex]->getNumDlogResources(subchannelIndex);
            }
        }
        return count;
    }

    static void onSetFileName(char *value) {
        psu::gui::popPage();
        stringCopy(g_parameters.filePath, sizeof(g_parameters.filePath), value);
    }

    static void onSelectTriggerSource(uint16_t value) {
        psu::gui::popPage();
        g_parameters.triggerSource = (trigger::Source)value;
    }

	static float getMinPeriod() {
		float minMinPeriod = PERIOD_MIN;
		for (int slotIndex = 0; slotIndex < NUM_SLOTS; slotIndex++) {
			int numSubchannels = g_slots[slotIndex]->getNumSubchannels();
			for (int subchannelIndex = 0; subchannelIndex < numSubchannels; subchannelIndex++) {
				int numResources = g_slots[slotIndex]->getNumDlogResources(subchannelIndex);
				for (int resourceIndex = 0; resourceIndex < numResources; resourceIndex++) {
					minMinPeriod = MIN(getResourceMinPeriod(slotIndex, subchannelIndex, resourceIndex), minMinPeriod);
				}
			}
		}

		return minMinPeriod;
	}
};

Parameters DlogParamsPage::g_parameters;
Parameters DlogParamsPage::g_parametersOrig;
int DlogParamsPage::g_numDlogResources = -1;
int DlogParamsPage::g_scrollPosition;
float DlogParamsPage::g_minPeriod;

static DlogParamsPage g_dlogParamsPage;

SetPage *getParamsPage() {
    return &g_dlogParamsPage;
}

} // namespace dlog_view
} // namespace psu

namespace gui {

using namespace psu;
using namespace psu::gui;
using namespace dlog_record;
using namespace dlog_view;

void data_dlog_state(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = dlog_record::getState();
    }
}

void data_dlog_toggle_state(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
    	if (DlogParamsPage::getNumDlogResources() == 0) {
            value = 5;
        } else if (dlog_record::isInStateTransition()) {
    		value = 4;
    	} else if (dlog_record::isIdle()) {
            value = 0;
        } else if (dlog_record::isExecuting()) {
            value = 1;
        } else if (dlog_record::isInitiated() && dlog_record::g_recordingParameters.triggerSource == trigger::SOURCE_MANUAL) {
            value = 2;
        } else {
            value = 3;
        }
    }
}

static EnumItem g_predefinedDlogPeriodsEnumDefinition[] = {
    { 0, "2 KSPS (24-bit)", "2 KSPS" },
    { 1, "4 KSPS (24-bit)", "4 KSPS" },
    { 2, "8 KSPS (24-bit)", "8 KSPS" },
    { 3, "16 KSPS (24-bit)", "16 KSPS" },
    { 4, "32 KSPS (16-bit)", "32 KSPS" },
    { 0, 0 }
};

float g_predefinedDlogPeriods[] = {
    1.0f / 2000,
    1.0f / 4000,
    1.0f / 8000,
    1.0f / 16000,
    1.0f / 32000
};

void data_dlog_period(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = MakeValue(DlogParamsPage::g_parameters.period, UNIT_SECOND);
    } else if (operation == DATA_OPERATION_GET_UNIT) {
        value = UNIT_SECOND;
    } else if (operation == DATA_OPERATION_GET_MIN) {
        value = MakeValue(DlogParamsPage::g_minPeriod, UNIT_SECOND);
    } else if (operation == DATA_OPERATION_GET_MAX) {
        value = MakeValue(MIN(DlogParamsPage::g_parameters.duration, PERIOD_MAX), UNIT_SECOND);
    } else if (operation == DATA_OPERATION_SET) {
        DlogParamsPage::setPeriod(value.getFloat());
    }
}

void data_dlog_period_spec(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
	data_dlog_period(operation, widgetCursor, value);
    if (operation == DATA_OPERATION_GET) {
        for (size_t i = 0; i < sizeof(g_predefinedDlogPeriods) / sizeof(float); i++) {
            if (DlogParamsPage::g_parameters.period == g_predefinedDlogPeriods[i]) {
                value = g_predefinedDlogPeriodsEnumDefinition[i].widgetLabel;
            }
        }
    }
}

void data_dlog_period_has_predefined_values(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = DlogParamsPage::g_minPeriod < PERIOD_MIN;
    }
}

void action_dlog_period_select_predefined_value() {
    pushSelectFromEnumPage(g_predefinedDlogPeriodsEnumDefinition, 0, nullptr, [] (uint16_t value) {
		popPage();
		DlogParamsPage::setPeriod(g_predefinedDlogPeriods[value]);
    }, true, false);
}

void data_dlog_duration(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = MakeValue(DlogParamsPage::g_parameters.duration, UNIT_SECOND);
    } else if (operation == DATA_OPERATION_GET_UNIT) {
        value = UNIT_SECOND;
    } else if (operation == DATA_OPERATION_GET_MIN) {
        value = MakeValue(MAX(DURATION_MIN, DlogParamsPage::g_parameters.period), UNIT_SECOND);
    } else if (operation == DATA_OPERATION_GET_MAX) {
        value = MakeValue(INFINITY, UNIT_SECOND);
    } else if (operation == DATA_OPERATION_SET) {
        DlogParamsPage::g_parameters.duration = value.getFloat();
    }
}

void data_dlog_file_name(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        static char filePath[MAX_PATH_LENGTH + 1 + 12];

        const char *fileName = *DlogParamsPage::g_parameters.filePath ? DlogParamsPage::g_parameters.filePath : "<time>";

        int slotIndex = DlogParamsPage::getModuleLocalRecordingSlotIndex();
        if (slotIndex != -1) {
            snprintf(filePath, sizeof(filePath), "%d:%s", slotIndex + 1, fileName);
			value = filePath;
		} else {
            value = fileName;
        }
    }
}

void data_dlog_start_enabled(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = checkDlogParameters(DlogParamsPage::g_parameters, true, false) == SCPI_RES_OK ? 1 : 0;
    }
}

void data_dlog_view_state(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = dlog_view::getState();
    }
}

void data_recording_ready(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = isExecuting() || getLatestFilePath() ? 1 : 0;
    }
}

void data_dlog_items_scrollbar_enabled(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = DlogParamsPage::getNumDlogResources() > DlogParamsPage::PAGE_SIZE;
    }
}

void data_dlog_items_num_selected(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value.type_ = VALUE_TYPE_NUM_SELECTED;
        value.pairOfUint16_.first = DlogParamsPage::g_parameters.numDlogItems;
        value.pairOfUint16_.second = DlogParamsPage::getNumDlogResources();
    }
}

void data_dlog_items(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    auto cursor = widgetCursor.cursor;
    if (operation == DATA_OPERATION_COUNT) {
        value = DlogParamsPage::getNumDlogResources();
    } else if (operation == DATA_OPERATION_SELECT) {
        int slotIndex;
        int subchannelIndex;
        int resourceIndex;
        if (DlogParamsPage::findResource(cursor, slotIndex, subchannelIndex, resourceIndex)) {
            value.type_ = VALUE_TYPE_CHANNEL_ID;
            value.pairOfUint16_.first = slotIndex;
            value.pairOfUint16_.second = subchannelIndex;
            
            hmi::g_selectedSlotIndex = slotIndex;
            hmi::g_selectedSubchannelIndex = subchannelIndex;
        }
    } else if (operation == DATA_OPERATION_DESELECT) {
        if (value.getType() == VALUE_TYPE_CHANNEL_ID) {
            hmi::g_selectedSlotIndex = value.pairOfUint16_.first;
            hmi::g_selectedSubchannelIndex = value.pairOfUint16_.second;
        }
    } else if (operation == DATA_OPERATION_YT_DATA_GET_SIZE) {
        value = Value(DlogParamsPage::getNumDlogResources(), VALUE_TYPE_UINT32);
    } else if (operation == DATA_OPERATION_YT_DATA_GET_POSITION) {
        value = Value(DlogParamsPage::g_scrollPosition, VALUE_TYPE_UINT32);
    } else if (operation == DATA_OPERATION_YT_DATA_SET_POSITION) {
        DlogParamsPage::setScrollPosition((int)value.getUInt32());
    } else if (operation == DATA_OPERATION_YT_DATA_GET_POSITION_INCREMENT) {
        value = 1;
    } else if (operation == DATA_OPERATION_YT_DATA_GET_PAGE_SIZE) {
        value = DlogParamsPage::PAGE_SIZE;
    }
}

void data_dlog_item_is_available(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    auto cursor = widgetCursor.cursor;
    if (operation == DATA_OPERATION_GET) {
        int slotIndex;
        int subchannelIndex;
        int resourceIndex;
        if (DlogParamsPage::findResource(cursor, slotIndex, subchannelIndex, resourceIndex)) {
            value = DlogParamsPage::g_parameters.isDlogItemAvailable(slotIndex, subchannelIndex, resourceIndex);
        } else {
            value = 0;
        }
    }
}

void data_dlog_item_is_checked(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    auto cursor = widgetCursor.cursor;
    if (operation == DATA_OPERATION_GET) {
        int slotIndex;
        int subchannelIndex;
        int resourceIndex;
        if (DlogParamsPage::findResource(cursor, slotIndex, subchannelIndex, resourceIndex)) {
            bool enabled = DlogParamsPage::g_parameters.isDlogItemEnabled(slotIndex, subchannelIndex, resourceIndex);
            if (!enabled && (
				DlogParamsPage::g_parameters.numDlogItems >= dlog_file::MAX_NUM_OF_Y_AXES || 
				DlogParamsPage::getResourceMinPeriod(slotIndex, subchannelIndex, resourceIndex)
					> DlogParamsPage::g_parameters.period
			)) {
                value = 0;
            } else {
                value = enabled;
            }
        } else {
            value = 0;
        }
    }
}

void data_dlog_item_channel(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    auto cursor = widgetCursor.cursor;
    if (operation == DATA_OPERATION_GET) {
        int slotIndex;
        int subchannelIndex;
        int resourceIndex;
        if (DlogParamsPage::findResource(cursor, slotIndex, subchannelIndex, resourceIndex)) {
            value.type_ = VALUE_TYPE_CHANNEL_ID;
            value.pairOfUint16_.first = slotIndex;
            value.pairOfUint16_.second = subchannelIndex;
        }
    }
}

void data_dlog_item_label(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    auto cursor = widgetCursor.cursor;
    if (operation == DATA_OPERATION_GET) {
        int slotIndex;
        int subchannelIndex;
        int resourceIndex;
        if (DlogParamsPage::findResource(cursor, slotIndex, subchannelIndex, resourceIndex)) {
            value = g_slots[slotIndex]->getDlogResourceLabel(subchannelIndex, resourceIndex);
        }
    }
}

void action_dlog_item_toggle() {
    int slotIndex;
    int subchannelIndex;
    int resourceIndex;
    if (DlogParamsPage::findResource(getFoundWidgetAtDown().cursor, slotIndex, subchannelIndex, resourceIndex)) {
        DlogParamsPage::g_parameters.enableDlogItem(slotIndex, subchannelIndex, resourceIndex,
            !DlogParamsPage::g_parameters.isDlogItemEnabled(slotIndex, subchannelIndex, resourceIndex));
    }
}

void action_dlog_toggle() {
    if (psu::gui::isPageOnStack(PAGE_ID_DLOG_PARAMS)) {
        g_dlogParamsPage.start();
    } else {
        toggleStop();
    }
}

void data_dlog_trigger_source(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = MakeEnumDefinitionValue(DlogParamsPage::g_parameters.triggerSource, ENUM_DEFINITION_TRIGGER_SOURCE);
    }
}

void action_dlog_trigger_select_source() {
    DlogParamsPage::selectTriggerSource();
}

void action_show_dlog_params() {
	selectChannel(nullptr);
    pushPage(PAGE_ID_DLOG_PARAMS);
}

void action_dlog_edit_period() {
    editValue(DATA_ID_DLOG_PERIOD);
}

void action_dlog_edit_duration() {
    editValue(DATA_ID_DLOG_DURATION);
}

void action_dlog_edit_file_name() {
    DlogParamsPage::editFileName();
}

void action_show_dlog_view() {
    NUM_HORZ_DIVISIONS = VIEW_WIDTH / WIDTH_PER_DIVISION;
    dlog_view::g_showLatest = true;
    if (!dlog_record::isExecuting()) {
        dlog_view::openFile(dlog_record::getLatestFilePath());
    }
    showPage(PAGE_ID_DLOG_VIEW);
}

void data_dlog_view_is_drawer_open(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
	if (operation == DATA_OPERATION_GET) {
		value = persist_conf::devConf.viewFlags.dlogViewDrawerIsOpen ? 1 : 0;
	}
}

bool isMultipleValuesOverlay(Recording recording) {
	return persist_conf::devConf.viewFlags.dlogViewLegendViewOption == persist_conf::DLOG_VIEW_LEGEND_VIEW_OPTION_FLOAT && dlog_view::isMulipleValuesOverlayHeuristic(recording);
}

bool isSingleValueOverlay(Recording recording) {
	return persist_conf::devConf.viewFlags.dlogViewLegendViewOption == persist_conf::DLOG_VIEW_LEGEND_VIEW_OPTION_FLOAT && !dlog_view::isMulipleValuesOverlayHeuristic(recording);
}

static const Rect g_dockedLegend = { 0, 0, 480, 32 };
static const Rect g_chartWithDockedLegend = { 0, 32, 480, 208 };
static const Rect g_chartWithoutDockedLegend = { 0, 0, 320, 240 };
static const Rect g_rightDrawerOpened = { 320, 0, 160, 272 };
static const Rect g_rightDrawerClosed = { 480, 0, 160, 272 };
static Rect g_overlayRect;

void animateRightDrawerOpen() {
	int i = 0;

    auto &recording = dlog_view::getRecording();

	if (persist_conf::devConf.viewFlags.dlogViewLegendViewOption == persist_conf::DLOG_VIEW_LEGEND_VIEW_OPTION_HIDDEN) {
	} else if (persist_conf::devConf.viewFlags.dlogViewLegendViewOption == persist_conf::DLOG_VIEW_LEGEND_VIEW_OPTION_DOCK) {
		g_animRects[i++] = { BUFFER_SOLID_COLOR, g_workingAreaRect, g_workingAreaRect, 0, OPACITY_SOLID, POSITION_TOP_LEFT };
		g_animRects[i++] = { BUFFER_OLD, g_dockedLegend, g_dockedLegend, 0, OPACITY_FADE_OUT, POSITION_TOP_LEFT };
		g_animRects[i++] = { BUFFER_OLD, g_chartWithDockedLegend, g_chartWithDockedLegend, 0, OPACITY_FADE_OUT, POSITION_TOP_LEFT };
		g_animRects[i++] = { BUFFER_NEW, g_chartWithoutDockedLegend, g_chartWithoutDockedLegend, 0, OPACITY_FADE_IN, POSITION_TOP_LEFT };
	} else if (isMultipleValuesOverlay(recording) || isSingleValueOverlay(recording)) {
		if (isMultipleValuesOverlay(recording)) {
			g_overlayRect.x = g_multipleValuesOverlay.x;
			g_overlayRect.y = g_multipleValuesOverlay.y;
			g_overlayRect.w = g_multipleValuesOverlay.width;
			g_overlayRect.h = g_multipleValuesOverlay.height;
		} else {
			g_overlayRect.x = g_singleValueOverlay.x;
			g_overlayRect.y = g_singleValueOverlay.y;
			g_overlayRect.w = g_singleValueOverlay.width;
			g_overlayRect.h = g_singleValueOverlay.height;
		}
		g_animRects[i++] = { BUFFER_NEW, g_overlayRect, g_overlayRect, 0, OPACITY_SOLID, POSITION_TOP_LEFT };
		g_animRects[i++] = { BUFFER_OLD, g_overlayRect, g_overlayRect, 0, OPACITY_FADE_OUT, POSITION_TOP_LEFT };
	}

	g_animRects[i++] = { BUFFER_NEW, g_rightDrawerClosed, g_rightDrawerOpened, 0, OPACITY_SOLID, POSITION_TOP_LEFT };

	animateRects(&g_psuAppContext, BUFFER_OLD, i);
	g_animationState.easingRects = remapOutCubic;
	g_animationState.easingOpacity = remapCubic;
}

void animateRightDrawerClose() {
	int i = 0;

	auto &recording = dlog_view::getRecording();

	if (persist_conf::devConf.viewFlags.dlogViewLegendViewOption == persist_conf::DLOG_VIEW_LEGEND_VIEW_OPTION_HIDDEN) {
	} else if (persist_conf::devConf.viewFlags.dlogViewLegendViewOption == persist_conf::DLOG_VIEW_LEGEND_VIEW_OPTION_DOCK) {
		g_animRects[i++] = { BUFFER_SOLID_COLOR, g_workingAreaRect, g_workingAreaRect, 0, OPACITY_SOLID, POSITION_TOP_LEFT };
		g_animRects[i++] = { BUFFER_OLD, g_chartWithoutDockedLegend, g_chartWithoutDockedLegend, 0, OPACITY_FADE_OUT, POSITION_TOP_LEFT };
		g_animRects[i++] = { BUFFER_NEW, g_dockedLegend, g_dockedLegend, 0, OPACITY_FADE_IN, POSITION_TOP_LEFT };
		g_animRects[i++] = { BUFFER_NEW, g_chartWithDockedLegend, g_chartWithDockedLegend, 0, OPACITY_FADE_IN, POSITION_TOP_LEFT };
	} else if (isMultipleValuesOverlay(recording) || isSingleValueOverlay(recording)) {
		if (isMultipleValuesOverlay(recording)) {
			g_overlayRect.x = g_multipleValuesOverlay.x;
			g_overlayRect.y = g_multipleValuesOverlay.y;
			g_overlayRect.w = g_multipleValuesOverlay.width;
			g_overlayRect.h = g_multipleValuesOverlay.height;
		} else {
			g_overlayRect.x = g_singleValueOverlay.x;
			g_overlayRect.y = g_singleValueOverlay.y;
			g_overlayRect.w = g_singleValueOverlay.width;
			g_overlayRect.h = g_singleValueOverlay.height;
		}
		g_animRects[i++] = { BUFFER_OLD, g_overlayRect, g_overlayRect, 0, OPACITY_SOLID, POSITION_TOP_LEFT };
		g_animRects[i++] = { BUFFER_NEW, g_overlayRect, g_overlayRect, 0, OPACITY_FADE_IN, POSITION_TOP_LEFT };
	}
	g_animRects[i++] = { BUFFER_OLD, g_rightDrawerOpened, g_rightDrawerClosed, 0, OPACITY_SOLID, POSITION_TOP_LEFT };

	animateRects(&g_psuAppContext, BUFFER_NEW, i);
	g_animationState.easingRects = remapCubic;
	g_animationState.easingOpacity = remapOutCubic;
}

void action_dlog_view_toggle_drawer() {
	if (persist_conf::devConf.viewFlags.dlogViewDrawerIsOpen) {
		persist_conf::setDlogViewDrawerIsOpen(false);
		animateRightDrawerClose();
	} else {
		persist_conf::setDlogViewDrawerIsOpen(true);
		animateRightDrawerOpen();
	}
}

void onSelectDlogViewLegendViewOption(uint16_t value) {
    popPage();
    persist_conf::setDlogViewLegendViewOption((persist_conf::DlogViewLegendViewOption)value);
}

void action_dlog_view_select_legend_view_option() {
    pushSelectFromEnumPage(ENUM_DEFINITION_DLOG_VIEW_LEGEND_VIEW_OPTION, persist_conf::devConf.viewFlags.dlogViewLegendViewOption, nullptr, onSelectDlogViewLegendViewOption);
}

void action_dlog_view_toggle_labels() {
    persist_conf::setDlogViewShowLabels(!persist_conf::devConf.viewFlags.dlogViewShowLabels);
}

void action_dlog_view_select_visible_value() {
    dlog_view::Recording &recording = dlog_view::getRecording();
    for (int i = 0; i < recording.parameters.numYAxes; i++) {
        int valueIndex = (recording.selectedValueIndex + 1 + i) % recording.parameters.numYAxes;
        if (recording.dlogValues[valueIndex].isVisible) {
            recording.selectedValueIndex = valueIndex;
            return;
        }
    }
}

void action_dlog_auto_scale() {
    dlog_view::autoScale(dlog_view::getRecording());
}

void action_dlog_scale_to_fit() {
    dlog_view::scaleToFit(dlog_view::getRecording());
}

void action_dlog_upload() {
    dlog_view::uploadFile();
}

void data_recording(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    auto &recording = dlog_view::getRecording();

    if (operation == DATA_OPERATION_YT_DATA_GET_REFRESH_COUNTER) {
        value = Value(g_refreshCounter, VALUE_TYPE_UINT32);
    } else if (operation == DATA_OPERATION_YT_DATA_GET_GET_VALUE_FUNC) {
        value = recording.getValue;
    } else if (operation == DATA_OPERATION_YT_DATA_VALUE_IS_VISIBLE) {
        value = Value(recording.dlogValues[value.getUInt8()].isVisible);
    } else if (operation == DATA_OPERATION_YT_DATA_GET_SHOW_LABELS) {
        value = persist_conf::devConf.viewFlags.dlogViewShowLabels ? 1 : 0;
    } else if (operation == DATA_OPERATION_YT_DATA_GET_SELECTED_VALUE_INDEX) {
        value = persist_conf::devConf.viewFlags.dlogViewLegendViewOption != persist_conf::DLOG_VIEW_LEGEND_VIEW_OPTION_HIDDEN ? recording.selectedValueIndex : -1;
    } else if (operation == DATA_OPERATION_YT_DATA_GET_LABEL) {
        YtDataGetLabelParams *params = (YtDataGetLabelParams *)value.getVoidPointer();
        dlog_view::getLabel(dlog_view::getRecording(), params->valueIndex, params->text, params->count);
    } else if (operation == DATA_OPERATION_YT_DATA_GET_SIZE) {
        value = Value(recording.size, VALUE_TYPE_UINT32);
    } else if (operation == DATA_OPERATION_YT_DATA_GET_POSITION) {
        value = Value(dlog_view::getPosition(recording), VALUE_TYPE_UINT32);
    } else if (operation == DATA_OPERATION_YT_DATA_SET_POSITION) {
        int32_t newPosition = value.getUInt32();
        if (newPosition < 0) {
            newPosition = 0;
        } else {
            if (newPosition + (uint32_t)VIEW_WIDTH > recording.size) {
                newPosition = recording.size - VIEW_WIDTH;
            }
        }
        dlog_view::changeXAxisOffset(recording, newPosition * recording.parameters.period);
    } else if (operation == DATA_OPERATION_YT_DATA_GET_PAGE_SIZE) {
        value = Value(VIEW_WIDTH, VALUE_TYPE_UINT32);
    } else if (operation == DATA_OPERATION_YT_DATA_GET_STYLE) {
        uint8_t dlogValueIndex = value.getUInt8();
        uint8_t numYtGraphStyles = sizeof(g_ytGraphStyles) / sizeof(uint16_t);
        if (psu::dlog_view::isMulipleValuesOverlayHeuristic(recording)) {
            value = Value(g_ytGraphStyles[dlogValueIndex % numYtGraphStyles], VALUE_TYPE_UINT16);
        } else {
            if (
                persist_conf::devConf.viewFlags.dlogViewLegendViewOption != persist_conf::DLOG_VIEW_LEGEND_VIEW_OPTION_HIDDEN &&
                dlogValueIndex == recording.selectedValueIndex
            ) {
                value = Value(STYLE_ID_YT_GRAPH_Y1, VALUE_TYPE_UINT16);
            } else {
                value = Value(STYLE_ID_YT_GRAPH_Y5, VALUE_TYPE_UINT16);
            }
        }
    } else if (operation == DATA_OPERATION_YT_DATA_GET_HORZ_DIVISIONS) {
        value = dlog_view::NUM_HORZ_DIVISIONS;
    } else if (operation == DATA_OPERATION_YT_DATA_GET_VERT_DIVISIONS) {
        value = dlog_view::NUM_VERT_DIVISIONS;
    } else if (operation == DATA_OPERATION_YT_DATA_GET_DIV) {
        value = Value(recording.dlogValues[value.getUInt8()].div, recording.parameters.yAxes[value.getUInt8()].unit);
    } else if (operation == DATA_OPERATION_YT_DATA_GET_OFFSET) {
        value = Value(recording.dlogValues[value.getUInt8()].offset, recording.parameters.yAxes[value.getUInt8()].unit);
    } else if (operation == DATA_OPERATION_YT_DATA_GET_GRAPH_UPDATE_METHOD) {
        value = YT_GRAPH_UPDATE_METHOD_STATIC;
    } else if (operation == DATA_OPERATION_YT_DATA_GET_PERIOD) {
        value = Value(recording.parameters.period, UNIT_SECOND);
    } else if (operation == DATA_OPERATION_YT_DATA_GET_BOOKMARKS) {
        value = Value(&getRecording() == &g_dlogFile ? g_visibleBookmarks : getLiveVisibleBookmarks(), VALUE_TYPE_POINTER);
	} else if (operation == DATA_OPERATION_YT_DATA_IS_CURSOR_VISIBLE) {
        value = &recording != &dlog_record::g_activeRecording;
    } else if (operation == DATA_OPERATION_YT_DATA_GET_CURSOR_OFFSET) {
        value = Value(getCursorPosition(recording), VALUE_TYPE_UINT32);
    } else if (operation == DATA_OPERATION_YT_DATA_TOUCH_DRAG) {
		enum DragObject {
            DRAG_NONE,
			DRAG_CURSOR,
			DRAG_VALUE
		};

        static struct {
			DragObject dragObject;
			float valueAtTouchDown;
			int positionAtTouchDown;
    	} g_dragState;

        TouchDrag *touchDrag = (TouchDrag *)value.getVoidPointer();

		dlog_view::DlogValueParams *dlogValueParams = recording.dlogValues + getSelectedDlogValueIndex(recording, g_focusCursor);

		WidgetCursor focusWidgetCursor;
		focusWidgetCursor.cursor = g_focusCursor;

		if (touchDrag->type == EVENT_TYPE_TOUCH_DOWN) {
			static const int CURSOR_REGION_SIZE_WIDTH = 80;
			static const int CURSOR_REGION_SIZE_HEIGHT = 40;
			auto cursorPosition = getCursorPosition(recording);
			if (cursorPosition < CURSOR_REGION_SIZE_WIDTH / 2) {
				cursorPosition = CURSOR_REGION_SIZE_WIDTH / 2;
			} else if (cursorPosition >= (uint32_t)VIEW_WIDTH - CURSOR_REGION_SIZE_WIDTH / 2) {
				cursorPosition = VIEW_WIDTH - CURSOR_REGION_SIZE_WIDTH / 2;
			}
			g_dragState.positionAtTouchDown = cursorPosition - touchDrag->x;
			bool dragCursor = (abs((long)g_dragState.positionAtTouchDown) < CURSOR_REGION_SIZE_WIDTH / 2) && (touchDrag->y > (touchDrag->widgetCursor.widget->h - CURSOR_REGION_SIZE_HEIGHT));
			if (dragCursor) {
				g_dragState.dragObject = DRAG_CURSOR;
				setFocusCursor(-1, DATA_ID_RECORDING);
			} else {
				if (g_focusDataId == DATA_ID_DLOG_VISIBLE_VALUE_OFFSET || g_focusDataId == DATA_ID_DLOG_Y_VALUE_OFFSET || g_focusDataId == DATA_ID_DLOG_VISIBLE_VALUE_DIV || g_focusDataId == DATA_ID_DLOG_Y_VALUE_DIV) {
                    if (dlogValueParams && dlogValueParams->isVisible) {
					    g_dragState.dragObject = DRAG_VALUE;
					    g_dragState.valueAtTouchDown = get(focusWidgetCursor, g_focusDataId).getFloat();
					    g_dragState.positionAtTouchDown = touchDrag->y;
                    } else {
                        g_dragState.dragObject = DRAG_NONE;
                    }
				} else {
					if (g_focusDataId != DATA_ID_DLOG_X_AXIS_DIV) {
						setFocusCursor(-1, DATA_ID_DLOG_X_AXIS_OFFSET);
					}

					g_dragState.dragObject = DRAG_VALUE;
					g_dragState.valueAtTouchDown = get(focusWidgetCursor, g_focusDataId).getFloat();
					g_dragState.positionAtTouchDown = touchDrag->x;
				}
			}
		} else {
			if (g_dragState.dragObject == DRAG_CURSOR) {
				recording.cursorOffset = MIN(dlog_view::VIEW_WIDTH - 1, MAX(touchDrag->x + g_dragState.positionAtTouchDown, 0)) * recording.parameters.period;
				findNearestBookmark();
			} else if (g_dragState.dragObject == DRAG_VALUE) {
				float scale;
				float value;
				Unit unit;
				if (g_focusDataId == DATA_ID_DLOG_VISIBLE_VALUE_OFFSET || g_focusDataId == DATA_ID_DLOG_Y_VALUE_OFFSET) {
					value = g_dragState.valueAtTouchDown + dlogValueParams->div * (g_dragState.positionAtTouchDown - touchDrag->y) * dlog_view::NUM_VERT_DIVISIONS / dlog_view::VIEW_HEIGHT;
					unit = getYAxisUnit(recording, g_focusCursor);
				} else if (g_focusDataId == DATA_ID_DLOG_VISIBLE_VALUE_DIV || g_focusDataId == DATA_ID_DLOG_Y_VALUE_DIV) {
					if (dlog_view::VIEW_HEIGHT - touchDrag->y <= 0) {
						return;
					}
					scale = 1.0f * (dlog_view::VIEW_HEIGHT - g_dragState.positionAtTouchDown) / (dlog_view::VIEW_HEIGHT - touchDrag->y);
					value = g_dragState.valueAtTouchDown * scale;
					unit = getYAxisUnit(recording, g_focusCursor);
				} else if (g_focusDataId == DATA_ID_DLOG_X_AXIS_DIV) {
					if (dlog_view::VIEW_WIDTH - touchDrag->x <= 0) {
						return;
					}
					scale = 1.0f * (dlog_view::VIEW_WIDTH - touchDrag->x) / (dlog_view::VIEW_WIDTH - g_dragState.positionAtTouchDown);
					value = g_dragState.valueAtTouchDown * scale;
					unit = getXAxisUnit(recording);
				} else {
					// g_focusDataId == DATA_ID_DLOG_X_AXIS_OFFSET
					value = g_dragState.valueAtTouchDown + recording.xAxisDiv * (g_dragState.positionAtTouchDown - touchDrag->x) * dlog_view::NUM_HORZ_DIVISIONS / dlog_view::VIEW_WIDTH;
					unit = getXAxisUnit(recording);
				}
				float min = getMin(focusWidgetCursor, g_focusDataId).getFloat();
				float max = getMax(focusWidgetCursor, g_focusDataId).getFloat();
				float value1 = clamp(value, min, max);
				set(focusWidgetCursor, g_focusDataId, Value(value1, unit));
			}
		}
    } else if (operation == DATA_OPERATION_YT_DATA_GET_CURSOR_X_VALUE) {
        float xValue = dlog_view::getPosition(recording) * recording.parameters.period + recording.cursorOffset;
        if (recording.parameters.xAxis.scaleType == dlog_file::SCALE_LOGARITHMIC) {
            xValue = powf(10, recording.parameters.xAxis.range.min + xValue);
        }
        value = Value(roundValueOnXAxis(recording, xValue), recording.parameters.xAxis.unit);
    } else if (operation == DATA_OPERATION_GET) {
        value = Value(dlog_view::getPosition(recording) * recording.parameters.period + recording.cursorOffset, dlog_view::getXAxisUnit(recording));
    } else if (operation == DATA_OPERATION_GET_MIN) {
        value = Value(0.0f, dlog_view::getXAxisUnit(recording));
    } else if (operation == DATA_OPERATION_GET_MAX) {
        value = Value(recording.parameters.xAxis.range.max - recording.parameters.xAxis.range.min, dlog_view::getXAxisUnit(recording));
    } else if (operation == DATA_OPERATION_GET_DEF) {
        value = Value((recording.parameters.xAxis.range.max - recording.parameters.xAxis.range.min) / 2, dlog_view::getXAxisUnit(recording));
    } else if (operation == DATA_OPERATION_SET) {
        float cursorPosition = value.getFloat() / recording.parameters.period - dlog_view::getPosition(recording);
        if (cursorPosition < 0.0f) {
            dlog_view::changeXAxisOffset(recording, recording.xAxisOffset + cursorPosition * recording.parameters.period);
            recording.cursorOffset = 0;
        } else if (cursorPosition > dlog_view::VIEW_WIDTH - 1) {
            dlog_view::changeXAxisOffset(recording, recording.xAxisOffset + (cursorPosition - (dlog_view::VIEW_WIDTH - 1)) * recording.parameters.period);
            recording.cursorOffset = (dlog_view::VIEW_WIDTH - 1) * recording.parameters.period;
        } else {
            recording.cursorOffset = cursorPosition * recording.parameters.period;
        }
		findNearestBookmark();
    } else if (operation == DATA_OPERATION_GET_ENCODER_STEP_VALUES) {
        static const int COUNT = 4;

        static float values[COUNT];

        values[0] = recording.parameters.xAxis.step;
        values[1] = recording.parameters.xAxis.step * 10;
        values[2] = recording.parameters.xAxis.step * 100;
        values[3] = recording.parameters.xAxis.step * 1000;

        StepValues *stepValues = value.getStepValues();

        stepValues->values = values;
        stepValues->count = COUNT;
        stepValues->unit = dlog_view::getXAxisUnit(recording);

        stepValues->encoderSettings.accelerationEnabled = true;
        stepValues->encoderSettings.range = 100.0f * values[0];
        stepValues->encoderSettings.step = values[0];

        stepValues->encoderSettings.mode = edit_mode_step::g_recordingEncoderMode;

        value = 1;
    } else if (operation == DATA_OPERATION_SET_ENCODER_MODE) {
		edit_mode_step::g_recordingEncoderMode = (EncoderMode)value.getInt();
    }
}

static const int NUM_VISIBLE_DLOG_VALUES_IN_OVERLAY = 4;

void data_dlog_multiple_values_overlay(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    static const int NUM_WIDGETS = 2;

    static const int LABELS_CONTAINER_WIDGET = 0;
    static const int DLOG_VALUES_LIST_WIDGET = 1;

    static WidgetOverride widgetOverrides[NUM_WIDGETS];
    Overlay &overlay = g_multipleValuesOverlay;

    if (operation == DATA_OPERATION_GET_OVERLAY_DATA) {
        value = Value(&overlay, VALUE_TYPE_POINTER);
    } else if (operation == DATA_OPERATION_UPDATE_OVERLAY_DATA) {
        auto &recording = dlog_view::getRecording();
        
        int state = 0;
        int numVisibleDlogValues = MIN(dlog_view::getNumVisibleDlogValues(recording), NUM_VISIBLE_DLOG_VALUES_IN_OVERLAY);
        
        if (!persist_conf::devConf.viewFlags.dlogViewDrawerIsOpen && isMultipleValuesOverlay(recording)) {
            state = numVisibleDlogValues;
        }

        if (overlay.state != state) {
            overlay.state = state;

            if (state != 0) {
                overlay.widgetOverrides = widgetOverrides;

                WidgetCursor &widgetCursor = *(WidgetCursor *)value.getVoidPointer();

                auto containerWidget = (ContainerWidget *)(widgetCursor.widget);

                Widget *labelsContainerWidget = containerWidget->widgets.item(widgetCursor.assets, LABELS_CONTAINER_WIDGET);
                Widget *dlogValuesListWidget = containerWidget->widgets.item(widgetCursor.assets, DLOG_VALUES_LIST_WIDGET);

                widgetOverrides[LABELS_CONTAINER_WIDGET].isVisible = true;
                widgetOverrides[LABELS_CONTAINER_WIDGET].x = labelsContainerWidget->x;
                widgetOverrides[LABELS_CONTAINER_WIDGET].y = labelsContainerWidget->y;
                widgetOverrides[LABELS_CONTAINER_WIDGET].w = labelsContainerWidget->w;
                widgetOverrides[LABELS_CONTAINER_WIDGET].h = labelsContainerWidget->h;

                widgetOverrides[DLOG_VALUES_LIST_WIDGET].isVisible = true;
                widgetOverrides[DLOG_VALUES_LIST_WIDGET].x = dlogValuesListWidget->x;
                widgetOverrides[DLOG_VALUES_LIST_WIDGET].y = dlogValuesListWidget->y;
                widgetOverrides[DLOG_VALUES_LIST_WIDGET].w = dlogValuesListWidget->w;
                widgetOverrides[DLOG_VALUES_LIST_WIDGET].h = numVisibleDlogValues * dlogValuesListWidget->h;

                overlay.width = widgetCursor.widget->w;
                overlay.height = widgetOverrides[LABELS_CONTAINER_WIDGET].h + widgetOverrides[DLOG_VALUES_LIST_WIDGET].h;

                overlay.x = 40;
                overlay.y = 8;
            }
        }

        value = Value(&overlay, VALUE_TYPE_POINTER);
    }
}

void data_dlog_single_value_overlay(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    Overlay &overlay = g_singleValueOverlay;

    if (operation == DATA_OPERATION_GET_OVERLAY_DATA) {
        value = Value(&overlay, VALUE_TYPE_POINTER);
    } else if (operation == DATA_OPERATION_UPDATE_OVERLAY_DATA) {
        auto &recording = dlog_view::getRecording();
        overlay.state = !persist_conf::devConf.viewFlags.dlogViewDrawerIsOpen && isSingleValueOverlay(recording);
        
        WidgetCursor &widgetCursor = *(WidgetCursor *)value.getVoidPointer();
        overlay.width = widgetCursor.widget->w;
        overlay.height = widgetCursor.widget->h;
        
        overlay.x = 40;
        overlay.y = 8;

        value = Value(&overlay, VALUE_TYPE_POINTER);
    }
}

void data_dlog_visible_values(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_COUNT) {
        auto &recording = dlog_view::getRecording();
        value = MIN(getNumVisibleDlogValues(recording), NUM_VISIBLE_DLOG_VALUES_IN_OVERLAY);
    }
}

void data_dlog_visible_value_label(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    auto cursor = widgetCursor.cursor;
    if (operation == DATA_OPERATION_GET) {
        auto &recording = dlog_view::getRecording();
        int dlogValueIndex = getSelectedDlogValueIndex(recording, cursor);
        value = Value(dlogValueIndex, VALUE_TYPE_DLOG_VALUE_LABEL);
    }
}

void guessStepValues(StepValues *stepValues, Unit unit) {
    if (unit == UNIT_VOLT) {
        static float values[] = { 0.001f, 0.01f, 0.1f, 1.0f };
        stepValues->values = values;
        stepValues->count = sizeof(values) / sizeof(float);
    } else if (unit == UNIT_AMPER) {
        static float values[] = { 0.0001f, 0.001f, 0.01f, 0.1f };
        stepValues->values = values;
        stepValues->count = sizeof(values) / sizeof(float);
    } else if (unit == UNIT_WATT) {
        static float values[] = { 0.5f, 1.0f, 2.0f, 5.0f };
        stepValues->values = values;
        stepValues->count = sizeof(values) / sizeof(float);
    } else if (unit == UNIT_SECOND) {
        static float values[] = { 1.0f, 5.0f, 10.0f, 20.0f };
        stepValues->values = values;
        stepValues->count = sizeof(values) / sizeof(float);
    } else {
        static float values[] = { 0.001f, 0.01f, 0.1f, 1.0f };
        stepValues->values = values;
        stepValues->count = sizeof(values) / sizeof(float);
    }

    stepValues->encoderSettings.accelerationEnabled = true;
    stepValues->encoderSettings.range = stepValues->values[0] * 10.0f;
    stepValues->encoderSettings.step = stepValues->values[0];

    stepValues->unit = unit;
}

void data_dlog_value_div(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    auto cursor = widgetCursor.cursor;
	auto &recording = dlog_view::getRecording();

	float minValue;
	float maxValue;
	getRangeOfScreenData(recording, cursor, minValue, maxValue);
	float range = (recording.parameters.yAxes[cursor].range.max - recording.parameters.yAxes[cursor].range.min);
	if (maxValue - minValue < range / 100.0f) {
		minValue -= range / 200.0f;
		maxValue += range / 200.0f;
	}

	float minDiv = (maxValue - minValue) / 100;
	float maxDiv = (maxValue - minValue) * 3;

	if (operation == DATA_OPERATION_GET) {
		bool focused = g_focusCursor == cursor && g_focusDataId == DATA_ID_DLOG_VISIBLE_VALUE_DIV;
		if (focused && g_focusEditValue.getType() != VALUE_TYPE_UNDEFINED) {
			value = g_focusEditValue;
		} else {
            value = Value(dlog_view::roundValueOnYAxis(recording, cursor, recording.dlogValues[cursor].div), getYAxisUnit(recording, cursor));
		}
	} else if (operation == DATA_OPERATION_GET_EDIT_VALUE) {
		bool focused = g_focusCursor == cursor && g_focusDataId == DATA_ID_DLOG_VISIBLE_VALUE_DIV;
		if (focused && g_focusEditValue.getType() != VALUE_TYPE_UNDEFINED) {
			value = g_focusEditValue;
		} else {
            value = Value(recording.dlogValues[cursor].div, getYAxisUnit(recording, cursor));
		}
	} else if (operation == DATA_OPERATION_GET_MIN) {
        value = Value(minDiv, getYAxisUnit(recording, cursor));
	} else if (operation == DATA_OPERATION_GET_MAX) {
		value = Value(maxDiv, getYAxisUnit(recording, cursor));
	} else if (operation == DATA_OPERATION_SET) {
		float mid = (minValue + maxValue) / 2;
		float oldDiv = recording.dlogValues[cursor].div;
		float newDiv = value.getFloat();
		float oldOffset = recording.dlogValues[cursor].offset;

		float newOffset = (mid + oldOffset) * newDiv / oldDiv - mid;

        recording.dlogValues[cursor].offset = newOffset;
        recording.dlogValues[cursor].div = value.getFloat();
	} else if (operation == DATA_OPERATION_GET_NAME) {
		value = "Div";
	} else if (operation == DATA_OPERATION_GET_UNIT) {
		value = getYAxisUnit(recording, cursor);
	} else if (operation == DATA_OPERATION_GET_IS_CHANNEL_DATA) {
		value = 0;
	} else if (operation == DATA_OPERATION_GET_ENCODER_STEP_VALUES) {
		StepValues *stepValues = value.getStepValues();

        stepValues->values = nullptr;
        stepValues->count = 0;

		stepValues->encoderSettings.range = 100 * minDiv;
        stepValues->encoderSettings.step = minDiv;

		stepValues->encoderSettings.mode = ENCODER_MODE_AUTO;
		value = 1;
	} else if (operation == DATA_OPERATION_SET_ENCODER_MODE) {
        edit_mode_step::g_visibleValueDivEncoderMode = (EncoderMode)value.getInt();
	}
}

void data_dlog_visible_value_div(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    auto cursor = widgetCursor.cursor;
    auto &recording = dlog_view::getRecording();
	WidgetCursor dlogValueDivWidgetCursor = widgetCursor;
	dlogValueDivWidgetCursor.cursor = getSelectedDlogValueIndex(recording, cursor);
	data_dlog_value_div(operation, dlogValueDivWidgetCursor, value);
}

void data_dlog_value_offset(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    auto cursor = widgetCursor.cursor;
	auto &recording = dlog_view::getRecording();

	float minValue;
	float maxValue;
	getRangeOfScreenData(recording, cursor, minValue, maxValue);
	float range = (recording.parameters.yAxes[cursor].range.max - recording.parameters.yAxes[cursor].range.min);
	if (maxValue - minValue < range / 100.0f) {
		minValue -= range / 200.0f;
		maxValue += range / 200.0f;
	}

	float minOffset = -recording.dlogValues[cursor].div * NUM_VERT_DIVISIONS / 2 - maxValue;
	float maxOffset = recording.dlogValues[cursor].div * NUM_VERT_DIVISIONS / 2 - minValue;

	if (recording.dlogValues[cursor].offset < minOffset) {
		minOffset = recording.dlogValues[cursor].offset;
	}

	if (recording.dlogValues[cursor].offset > maxOffset) {
		maxOffset = recording.dlogValues[cursor].offset;
	}

	if (operation == DATA_OPERATION_GET) {
		bool focused = g_focusCursor == cursor && g_focusDataId == DATA_ID_DLOG_VISIBLE_VALUE_OFFSET;
		if (focused && g_focusEditValue.getType() != VALUE_TYPE_UNDEFINED) {
			value = g_focusEditValue;
		} else {
            value = Value(dlog_view::roundValueOnYAxis(recording, cursor, recording.dlogValues[cursor].offset), getYAxisUnit(recording, cursor));
		}
	} else if (operation == DATA_OPERATION_GET_EDIT_VALUE) {
		bool focused = g_focusCursor == cursor && g_focusDataId == DATA_ID_DLOG_VISIBLE_VALUE_OFFSET;
		if (focused && g_focusEditValue.getType() != VALUE_TYPE_UNDEFINED) {
			value = g_focusEditValue;
		} else {
            value = Value(recording.dlogValues[cursor].offset, getYAxisUnit(recording, cursor));
		}
	} else if (operation == DATA_OPERATION_GET_MIN) {
		value = Value(minOffset, getYAxisUnit(recording, cursor));
	} else if (operation == DATA_OPERATION_GET_MAX) {
		float rangeMin;
		float rangeMax;
		getRange(recording, cursor, rangeMin, rangeMax);
        value = Value(maxOffset, getYAxisUnit(recording, cursor));
	} else if (operation == DATA_OPERATION_SET) {
        recording.dlogValues[cursor].offset = value.getFloat();
	} else if (operation == DATA_OPERATION_GET_NAME) {
		value = "Offset";
	} else if (operation == DATA_OPERATION_GET_UNIT) {
		value = getYAxisUnit(recording, cursor);
	} else if (operation == DATA_OPERATION_GET_IS_CHANNEL_DATA) {
		value = 0;
	} else if (operation == DATA_OPERATION_GET_ENCODER_STEP_VALUES) {
		StepValues *stepValues = value.getStepValues();
		
        stepValues->values = nullptr;
        stepValues->count = 0;

		stepValues->encoderSettings.range = maxOffset - minOffset;
		stepValues->encoderSettings.step = (maxOffset - minOffset) / VIEW_HEIGHT;

		stepValues->encoderSettings.mode = ENCODER_MODE_AUTO;
		value = 1;
	}
}

void data_dlog_visible_value_offset(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    auto cursor = widgetCursor.cursor;
    auto &recording = dlog_view::getRecording();
	WidgetCursor dlogValueOffsetWidgetCursor = widgetCursor;
	dlogValueOffsetWidgetCursor = getSelectedDlogValueIndex(recording, cursor);
	data_dlog_value_offset(operation, dlogValueOffsetWidgetCursor, value);
}

void data_dlog_x_axis_offset(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    auto cursor = widgetCursor.cursor;
    auto &recording = dlog_view::getRecording();

    if (operation == DATA_OPERATION_GET) {
        bool focused = g_focusCursor == cursor && g_focusDataId == DATA_ID_DLOG_X_AXIS_OFFSET;
        if (focused && g_focusEditValue.getType() != VALUE_TYPE_UNDEFINED) {
            value = g_focusEditValue;
        } else {
            value = Value(recording.xAxisOffset, dlog_view::getXAxisUnit(recording));
        }
    } else if (operation == DATA_OPERATION_GET_MIN) {
        value = Value(recording.parameters.xAxis.range.min, dlog_view::getXAxisUnit(recording));
    } else if (operation == DATA_OPERATION_GET_MAX) {
        value = Value(getDuration(recording) - VIEW_WIDTH * recording.parameters.period, dlog_view::getXAxisUnit(recording));
    } else if (operation == DATA_OPERATION_SET) {
        dlog_view::changeXAxisOffset(recording, value.getFloat());
    } else if (operation == DATA_OPERATION_GET_NAME) {
        value = "Offset";
    } else if (operation == DATA_OPERATION_GET_UNIT) {
        value = dlog_view::getXAxisUnit(recording);
    } else if (operation == DATA_OPERATION_GET_IS_CHANNEL_DATA) {
        value = 0;
    } else if (operation == DATA_OPERATION_GET_ENCODER_STEP_VALUES) {
        StepValues *stepValues = value.getStepValues();
        guessStepValues(stepValues, dlog_view::getXAxisUnit(recording));
        stepValues->encoderSettings.mode = edit_mode_step::g_xAxisOffsetEncoderMode;
        value = 1;
    } else if (operation == DATA_OPERATION_SET_ENCODER_MODE) {
        edit_mode_step::g_xAxisOffsetEncoderMode = (EncoderMode)value.getInt();
    }
}

void data_dlog_x_axis_div(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    auto cursor = widgetCursor.cursor;
    auto &recording = dlog_view::getRecording();

    if (operation == DATA_OPERATION_GET) {
        bool focused = g_focusCursor == cursor && g_focusDataId == DATA_ID_DLOG_X_AXIS_DIV;
        if (focused && g_focusEditValue.getType() != VALUE_TYPE_UNDEFINED) {
            value = g_focusEditValue;
        } else {
            value = Value(recording.xAxisDiv, dlog_view::getXAxisUnit(recording));
        }
    } else if (operation == DATA_OPERATION_GET_MIN || operation == DATA_OPERATION_GET_DEF) {
        value = Value(recording.xAxisDivMin, dlog_view::getXAxisUnit(recording));
    } else if (operation == DATA_OPERATION_GET_MAX) {
        value = Value(recording.xAxisDivMax, dlog_view::getXAxisUnit(recording));
    } else if (operation == DATA_OPERATION_SET) {
        dlog_view::changeXAxisDiv(recording, roundPrec(value.getFloat(), 0.001f));
    } else if (operation == DATA_OPERATION_GET_NAME) {
        value = "Div";
    } else if (operation == DATA_OPERATION_GET_UNIT) {
        value = dlog_view::getXAxisUnit(recording);
    } else if (operation == DATA_OPERATION_GET_IS_CHANNEL_DATA) {
        value = 0;
    } else if (operation == DATA_OPERATION_GET_ENCODER_STEP_VALUES) {
        StepValues *stepValues = value.getStepValues();
		
        stepValues->values = nullptr;
        stepValues->count = 0;

		stepValues->encoderSettings.range = recording.xAxisDivMax - recording.xAxisDivMin;
		stepValues->encoderSettings.step = (recording.xAxisDivMax - recording.xAxisDivMin) / 100;

		stepValues->encoderSettings.mode = ENCODER_MODE_AUTO;
		value = 1;
    }
}

void data_dlog_x_axis_max_value(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        auto &recording = dlog_view::getRecording();

        float maxValue = dlog_view::getDuration(recording);

        if (recording.parameters.xAxis.scaleType == dlog_file::SCALE_LOGARITHMIC) {
            maxValue = powf(10, recording.parameters.xAxis.range.min + maxValue);
        }

        value = Value(maxValue, recording.parameters.xAxis.unit);
    }
}

void data_dlog_x_axis_max_value_label(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        auto &recording = dlog_view::getRecording();
        if (recording.parameters.xAxis.unit == UNIT_SECOND) {
            value = "Total duration";
        } else {
            value = "Max.";
        }
    }
}

void data_dlog_value_cursor(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
	if (operation == DATA_OPERATION_GET) {
		auto &recording = dlog_view::getRecording();
		auto ytDataGetValue = ytDataGetGetValueFunc(widgetCursor, DATA_ID_RECORDING);
		float max;
		float min = ytDataGetValue(ytDataGetPosition(widgetCursor, DATA_ID_RECORDING) + recording.cursorOffset / recording.parameters.period, widgetCursor.cursor, &max);
		float cursorValue = (min + max) / 2;
		if (recording.parameters.yAxisScaleType == dlog_file::SCALE_LOGARITHMIC) {
			float logOffset = 1 - recording.parameters.yAxes[widgetCursor.cursor].range.min;
			cursorValue = powf(10, cursorValue) - logOffset;
		}
		value = Value(cursorValue, recording.parameters.yAxes[widgetCursor.cursor].unit);

	}
}

void data_dlog_visible_value_cursor(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    auto cursor = widgetCursor.cursor;
    auto &recording = dlog_view::getRecording();
	WidgetCursor dlogValueCursorWidgetCursor = widgetCursor;
	dlogValueCursorWidgetCursor.cursor = getSelectedDlogValueIndex(recording, cursor);
	data_dlog_value_cursor(operation, dlogValueCursorWidgetCursor, value);
}

void data_dlog_current_time(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = Value(0, VALUE_TYPE_DLOG_CURRENT_TIME);
    }
}

void data_dlog_file_length(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = Value(dlog_record::getFileLength(), VALUE_TYPE_FILE_LENGTH);
    }
}

void data_dlog_value_label(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    auto cursor = widgetCursor.cursor;
    if (operation == DATA_OPERATION_GET) {
        value = Value(cursor, VALUE_TYPE_DLOG_VALUE_LABEL);
    }
}

void data_dlog_view_legend_view_option(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = (int)persist_conf::devConf.viewFlags.dlogViewLegendViewOption;
    }
}

void data_dlog_view_show_labels(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = (int)persist_conf::devConf.viewFlags.dlogViewShowLabels;
    }
}

void data_dlog_preview_overlay(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    static Overlay overlay;

    if (operation == DATA_OPERATION_GET_OVERLAY_DATA) {
        value = Value(&overlay, VALUE_TYPE_POINTER);
    } else if (operation == DATA_OPERATION_UPDATE_OVERLAY_DATA) {
        overlay.state = dlog_record::isExecuting() && (
                dlog_record::isModuleLocalRecording() || dlog_record::isModuleControlledRecording()
            ) ? 1 : 0;
        
        WidgetCursor &widgetCursor = *(WidgetCursor *)value.getVoidPointer();
        overlay.width = widgetCursor.widget->w;
        overlay.height = widgetCursor.widget->h;
        
        overlay.x = widgetCursor.widget->x;
        overlay.y = widgetCursor.widget->y;

        value = Value(&overlay, VALUE_TYPE_POINTER);
    }
}

void data_dlog_view_selected_tab(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
	if (operation == DATA_OPERATION_GET) {
		value = g_selectedTab;
	}
}

void action_dlog_view_select_y_values_tab() {
	g_selectedTab = TAB_Y_VALUES;

	setFocusCursor(Cursor(-1), DATA_ID_DLOG_Y_VALUE_OFFSET);
}

void action_dlog_view_select_bookmarks_tab() {
	g_selectedTab = TAB_BOOKMARKS;

	if (getRecording().parameters.bookmarksSize > BOOKMARKS_PER_PAGE) {
		setFocusCursor(Cursor(-1), DATA_ID_DLOG_BOOKMARKS);
	}
}

void action_dlog_view_select_options_tab() {
	g_selectedTab = TAB_OPTIONS;
}

void data_dlog_has_bookmark(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
	if (operation == DATA_OPERATION_GET) {
        value = getRecording().parameters.bookmarksSize > 0;
    }
}

void data_dlog_bookmarks_is_scrollbar_visible(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
	if (operation == DATA_OPERATION_GET) {
        value = &getRecording() == &g_dlogFile ? getRecording().parameters.bookmarksSize > BOOKMARKS_PER_PAGE : 0;
    }
}

void data_dlog_bookmarks(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
	auto bookmarksSize = getRecording().parameters.bookmarksSize;
    if (&getRecording() != &g_dlogFile) {
        bookmarksSize = MIN(bookmarksSize, BOOKMARKS_PER_PAGE);
    }

	if (operation == DATA_OPERATION_COUNT) {
		value = (int)(bookmarksSize);
	} else if (operation == DATA_OPERATION_YT_DATA_GET_SIZE) {
        value = Value(bookmarksSize, VALUE_TYPE_UINT32);
    } else if (operation == DATA_OPERATION_YT_DATA_GET_POSITION) {
        value = Value(g_bookmarksScrollPosition, VALUE_TYPE_UINT32);
    } else if (operation == DATA_OPERATION_YT_DATA_SET_POSITION) {
		setBookmarksScrollPosition(value.getUInt32());
    } else if (operation == DATA_OPERATION_YT_DATA_GET_PAGE_SIZE) {
        value = Value(BOOKMARKS_PER_PAGE, VALUE_TYPE_UINT32);
	} else if (operation == DATA_OPERATION_GET) {
		value = MakeValue(1.0f * g_bookmarksScrollPosition, UNIT_NONE);
	} else if (operation == DATA_OPERATION_GET_MIN) {
		value = MakeValue(0, UNIT_NONE);
	} else if (operation == DATA_OPERATION_GET_MAX) {
		value = MakeValue(1.0f * (bookmarksSize - BOOKMARKS_PER_PAGE), UNIT_NONE);
	} else if (operation == DATA_OPERATION_GET_ENCODER_STEP_VALUES) {
 		auto stepValues = value.getStepValues();

		static float values[] = { 1.0f, 1.0f * BOOKMARKS_PER_PAGE, 2.0f * BOOKMARKS_PER_PAGE, 5.0f * BOOKMARKS_PER_PAGE, 10.0f * BOOKMARKS_PER_PAGE };

		stepValues->values = values;
		stepValues->count = sizeof(values) / sizeof(float);
		stepValues->unit = UNIT_NONE;

		stepValues->encoderSettings.accelerationEnabled = true;
		stepValues->encoderSettings.range = 10.0f * BOOKMARKS_PER_PAGE;
		stepValues->encoderSettings.step = 1.0f;
		stepValues->encoderSettings.mode = edit_mode_step::g_scrollBarEncoderMode;

		value = 1;
	} else if (operation == DATA_OPERATION_SET_ENCODER_MODE) {
        psu::gui::edit_mode_step::g_scrollBarEncoderMode = (EncoderMode)value.getInt();
    } else if (operation == DATA_OPERATION_SET) {
		setBookmarksScrollPosition((uint32_t)value.getFloat());
	}
}

void data_dlog_bookmark_x_value(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    auto cursor = widgetCursor.cursor;
	if (operation == DATA_OPERATION_GET) {
		auto i = cursor - g_bookmarksScrollPosition;
		if (i >= 0 && i < BOOKMARKS_PER_PAGE) {
			auto &recording = getRecording();
			value = Value(g_bookmarks[i].position * recording.parameters.xAxis.step, dlog_view::getXAxisUnit(recording));
		}
	}
}

void data_dlog_bookmark_text(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    auto cursor = widgetCursor.cursor;
	if (operation == DATA_OPERATION_GET) {
		auto i = cursor - g_bookmarksScrollPosition;
		if (i >= 0 && i < BOOKMARKS_PER_PAGE) {
			value = g_bookmarks[i].text;
		}
	}
}

void data_dlog_bookmark_is_selected(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    auto cursor = widgetCursor.cursor;
	if (operation == DATA_OPERATION_GET) {
		value = (uint32_t)cursor == g_selectedBookmarkIndex;
	}
}

void action_dlog_view_select_bookmark() {
	g_selectedBookmarkIndex = getFoundWidgetAtDown().cursor;

	if (g_bookmarksScrollPosition == g_loadedBookmarksScrollPosition && g_selectedBookmarkIndex >= g_bookmarksScrollPosition && g_selectedBookmarkIndex < g_bookmarksScrollPosition + BOOKMARKS_PER_PAGE) {
		// position cursor at the bookmark position
		auto bookmarkIndex = g_selectedBookmarkIndex - g_bookmarksScrollPosition;

		auto cursorPosition = g_bookmarks[bookmarkIndex].position * g_dlogFile.parameters.xAxis.step / g_dlogFile.parameters.period - getPosition(g_dlogFile);
		if (cursorPosition < 0 || cursorPosition >= (uint32_t)VIEW_WIDTH) {
			cursorPosition = VIEW_WIDTH / 2.0f;

            g_dlogFile.xAxisOffset = g_bookmarks[bookmarkIndex].position * g_dlogFile.parameters.xAxis.step - cursorPosition * g_dlogFile.parameters.period;
            adjustXAxisOffset(g_dlogFile);
        }

        g_dlogFile.cursorOffset = cursorPosition * g_dlogFile.parameters.period;
	}

	setFocusCursor(Cursor(-1), DATA_ID_DLOG_BOOKMARKS);
}

void data_dlog_view_is_zoom_in_enabled(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
	if (operation == DATA_OPERATION_GET) {
		if (&getRecording() == &g_dlogFile) {
			value = g_dlogFile.xAxisDiv > g_dlogFile.xAxisDivMin;
		} else {
			value = 0;
		}
	}
}

void action_dlog_view_zoom_in() {
	if (&getRecording() == &g_dlogFile) {
		changeXAxisDiv(g_dlogFile, MAX(g_dlogFile.xAxisDiv / 2, g_dlogFile.xAxisDivMin));
	}
}

void data_dlog_view_is_zoom_out_enabled(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
	if (operation == DATA_OPERATION_GET) {
		if (&getRecording() == &g_dlogFile) {
			value = g_dlogFile.xAxisDiv < g_dlogFile.xAxisDivMax;
		} else {
			value = 0;
		}
	}
}

void action_dlog_view_zoom_out() {
	if (&getRecording() == &g_dlogFile) {
		changeXAxisDiv(g_dlogFile, MIN(g_dlogFile.xAxisDiv * 2, g_dlogFile.xAxisDivMax));
	}
}

void data_dlog_y_values_is_scrollbar_visible(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
	if (operation == DATA_OPERATION_GET) {
        value = getRecording().parameters.numYAxes > Y_VALUES_PER_PAGE;
    }
}

void setYValuesScrollPosition(uint32_t scrollPosition) {
	auto &recording = getRecording();

	if (scrollPosition + Y_VALUES_PER_PAGE > recording.parameters.numYAxes) {
		scrollPosition = recording.parameters.numYAxes - Y_VALUES_PER_PAGE;
	}

    g_yValuesScrollPosition = scrollPosition;
}

void data_dlog_y_values(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
	auto &recording = getRecording();
	if (operation == DATA_OPERATION_COUNT) {
		value = (int)(recording.parameters.numYAxes);
	} else if (operation == DATA_OPERATION_YT_DATA_GET_SIZE) {
        value = Value((uint32_t)recording.parameters.numYAxes, VALUE_TYPE_UINT32);
    } else if (operation == DATA_OPERATION_YT_DATA_GET_POSITION) {
        value = Value(g_yValuesScrollPosition, VALUE_TYPE_UINT32);
    } else if (operation == DATA_OPERATION_YT_DATA_SET_POSITION) {
		setYValuesScrollPosition(value.getUInt32());
    } else if (operation == DATA_OPERATION_YT_DATA_GET_PAGE_SIZE) {
        value = Value(Y_VALUES_PER_PAGE, VALUE_TYPE_UINT32);
	} else if (operation == DATA_OPERATION_GET) {
		value = MakeValue(1.0f * g_yValuesScrollPosition, UNIT_NONE);
	} else if (operation == DATA_OPERATION_GET_MIN) {
		value = MakeValue(0, UNIT_NONE);
	} else if (operation == DATA_OPERATION_GET_MAX) {
		value = MakeValue(1.0f * (recording.parameters.numYAxes - Y_VALUES_PER_PAGE), UNIT_NONE);
	} else if (operation == DATA_OPERATION_GET_ENCODER_STEP_VALUES) {
 		auto stepValues = value.getStepValues();

		static float values[] = { 1.0f, 1.0f * Y_VALUES_PER_PAGE, 2.0f * Y_VALUES_PER_PAGE, 5.0f * Y_VALUES_PER_PAGE, 10.0f * Y_VALUES_PER_PAGE };

		stepValues->values = values;
		stepValues->count = sizeof(values) / sizeof(float);
		stepValues->unit = UNIT_NONE;

		stepValues->encoderSettings.accelerationEnabled = true;
		stepValues->encoderSettings.range = 10.0f * Y_VALUES_PER_PAGE;
		stepValues->encoderSettings.step = 1.0f;
		stepValues->encoderSettings.mode = edit_mode_step::g_scrollBarEncoderMode;

		value = 1;
	} else if (operation == DATA_OPERATION_SET_ENCODER_MODE) {
        psu::gui::edit_mode_step::g_scrollBarEncoderMode = (EncoderMode)value.getInt();
    } else if (operation == DATA_OPERATION_SET) {
		setYValuesScrollPosition((uint32_t)value.getFloat());
	}
}

void data_dlog_y_value_label(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    auto cursor = widgetCursor.cursor;
    if (operation == DATA_OPERATION_GET) {
        value = Value(cursor, VALUE_TYPE_DLOG_VALUE_LABEL);
    }
}

void data_dlog_y_value_is_checked(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    auto cursor = widgetCursor.cursor;
    if (operation == DATA_OPERATION_GET) {
	    value = getRecording().dlogValues[cursor].isVisible;
    }
}

void data_dlog_y_value_is_selected(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    auto cursor = widgetCursor.cursor;
    if (operation == DATA_OPERATION_GET) {
	    value = getRecording().selectedValueIndex == cursor;
    }
}

void data_dlog_y_value_offset(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
	WidgetCursor dlogValueOffsetWidgetCursor = widgetCursor;
	dlogValueOffsetWidgetCursor = getRecording().selectedValueIndex;
	data_dlog_value_offset(operation, dlogValueOffsetWidgetCursor, value);
}

void data_dlog_y_value_div(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
	WidgetCursor dlogValueDivWidgetCursor = widgetCursor;
	dlogValueDivWidgetCursor = getRecording().selectedValueIndex;
	data_dlog_value_div(operation, dlogValueDivWidgetCursor, value);
}

void data_dlog_y_value_cursor(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
	WidgetCursor dlogValueCursorWidgetCursor = widgetCursor;
	dlogValueCursorWidgetCursor.cursor = getRecording().selectedValueIndex;
	data_dlog_value_cursor(operation, dlogValueCursorWidgetCursor, value);
}

void action_dlog_view_select_y_value() {
    auto &recording = getRecording();
    auto valueIndex = getFoundWidgetAtDown().cursor;
    if (recording.dlogValues[valueIndex].isVisible) {
        getRecording().selectedValueIndex = getFoundWidgetAtDown().cursor;
    }
    setFocusCursor(Cursor(-1), DATA_ID_DLOG_Y_VALUES);
}

void data_dlog_y_value_is_enabled(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    auto cursor = widgetCursor.cursor;
	if (operation == DATA_OPERATION_GET) {
        auto &recording = getRecording();
        value = getNumVisibleDlogValues(recording) < MAX_NUM_OF_Y_VALUES || recording.dlogValues[cursor].isVisible;
    }
}

void action_dlog_view_toggle_y_value_visible() {
    auto &recording = getRecording();
	auto valueIndex = getFoundWidgetAtDown().cursor;
	if (recording.dlogValues[valueIndex].isVisible) {
        recording.dlogValues[valueIndex].isVisible = false;
	} else {
		recording.dlogValues[valueIndex].isVisible = true;
	}
}

void action_scroll() {
}

} // namespace gui

} // namespace eez
