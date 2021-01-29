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

static State g_state;
static uint32_t g_loadingStartTickCount;
bool g_showLatest = true;
char g_filePath[MAX_PATH_LENGTH + 1];
Recording g_recording;

struct CacheBlock {
    unsigned valid: 1;
    uint32_t loadedValues;
    uint32_t startAddress;
};

struct BlockElement {
    float min;
    float max;
};

static const uint32_t NUM_ELEMENTS_PER_BLOCKS = 480 * MAX_NUM_OF_Y_VALUES;
static const uint32_t BLOCK_SIZE = NUM_ELEMENTS_PER_BLOCKS * sizeof(BlockElement);
static const uint32_t NUM_BLOCKS = FILE_VIEW_BUFFER_SIZE / (BLOCK_SIZE + sizeof(CacheBlock));

CacheBlock *g_cacheBlocks = (CacheBlock *)FILE_VIEW_BUFFER;

static bool g_isLoading;
static bool g_interruptLoading;
static uint32_t g_blockIndexToLoad;
static float g_loadScale;
static bool g_refreshed;
static bool g_wasExecuting;

////////////////////////////////////////////////////////////////////////////////

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

////////////////////////////////////////////////////////////////////////////////

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

inline BlockElement *getCacheBlock(unsigned blockIndex) {
    return (BlockElement *)(FILE_VIEW_BUFFER + NUM_BLOCKS * sizeof(CacheBlock) + blockIndex * BLOCK_SIZE);
}

inline unsigned getNumElementsPerRow() {
    return MIN(g_recording.parameters.numYAxes, MAX_NUM_OF_Y_VALUES);
}

void invalidateAllBlocks() {
    g_interruptLoading = true;

    while (g_isLoading) {
        osDelay(1);
    }

    for (unsigned blockIndex = 0; blockIndex < NUM_BLOCKS; blockIndex++) {
        g_cacheBlocks[blockIndex].valid = false;
    }
}

void loadBlock() {
    static const int NUM_VALUES_ROWS = 16;
    float values[18 * NUM_VALUES_ROWS];

    auto numSamplesPerValue = (unsigned)round(g_loadScale);
    if (numSamplesPerValue > 0) {
        File file;
        if (file.open(g_filePath, FILE_OPEN_EXISTING | FILE_READ)) {
            auto numElementsPerRow = getNumElementsPerRow();

            BlockElement *blockElements = getCacheBlock(g_blockIndexToLoad);

            uint32_t totalBytesRead = 0;

            uint32_t i = g_cacheBlocks[g_blockIndexToLoad].loadedValues;
            while (i < NUM_ELEMENTS_PER_BLOCKS) {
                auto offset = (uint32_t)roundf(
                    (g_blockIndexToLoad * NUM_ELEMENTS_PER_BLOCKS + i) / numElementsPerRow
                        * g_loadScale * g_recording.numFloatsPerRow
                );

                offset = g_recording.numFloatsPerRow * (
                    (offset + g_recording.numFloatsPerRow - 1) / g_recording.numFloatsPerRow
                );

                size_t filePosition = g_recording.dataOffset + offset * sizeof(float);
                if (!file.seek(filePosition)) {
                    i = NUM_ELEMENTS_PER_BLOCKS;
                    goto closeFile;
                }

                unsigned iStart = i;

                for (unsigned j = 0; j < numSamplesPerValue; j++) {
                    i = iStart;

                    auto valuesRow = j % NUM_VALUES_ROWS;

                    if (valuesRow == 0) {
                        if (g_interruptLoading) {
                            i = NUM_ELEMENTS_PER_BLOCKS;
                            goto closeFile;
                        }

                        // read up to NUM_VALUES_ROWS
                        uint32_t bytesToRead = MIN(NUM_VALUES_ROWS, numSamplesPerValue - j) * g_recording.numFloatsPerRow * sizeof(float);
                        uint32_t bytesRead = file.read(values, bytesToRead);
                        if (bytesToRead != bytesRead) {
                            i = NUM_ELEMENTS_PER_BLOCKS;
                            goto closeFile;
                        }

                        totalBytesRead += bytesRead;
                    }

					unsigned valuesOffset = valuesRow * g_recording.numFloatsPerRow;

                    uint32_t bitMask = 0;
                    uint32_t bits = 0;
                    uint32_t m = 0;

                    for (unsigned k = 0; k < numElementsPerRow; k++) {
                        BlockElement *blockElement = blockElements + i++;

                        float value;

                        auto &yAxis = g_recording.parameters.yAxes[k];
                        if (yAxis.unit == UNIT_BIT) {
                            if (bitMask == 0) {
                                bits = *(uint32_t *)&values[valuesOffset + m++];
                                bitMask = 0x4000;
                            } else {
                                bitMask >>= 1;
                            }

                            value = bits & 0x8000 ? ((bits & bitMask) ? 1.0f : 0.0f) : NAN;
                        } else {
                            bitMask = 0;
                            value = values[valuesOffset + m++];
                        }

                        if (j == 0) {
                            blockElement->min = blockElement->max = value;
                        } else if (value < blockElement->min) {
                            blockElement->min = value;
                        } else if (value > blockElement->max) {
                            blockElement->max = value;
                        }
                    }
                }

                if (totalBytesRead > NUM_ELEMENTS_PER_BLOCKS * sizeof(BlockElement)) {
                    break;
                }

                g_refreshed = true;
            }

        closeFile:
            g_cacheBlocks[g_blockIndexToLoad].loadedValues = i;
            file.close();
        }
    }

    g_isLoading = false;
    g_refreshed = true;
}

void stateManagment() {
    auto isExecuting = dlog_record::isExecuting();
    if (!isExecuting && g_wasExecuting && g_showLatest && psu::gui::isPageOnStack(PAGE_ID_DLOG_VIEW)) {
        if (psu::gui::isPageOnStack(getExternalAssetsFirstPageId())) {
            psu::gui::removePageFromStack(PAGE_ID_DLOG_VIEW);
            return;
        } else {
            openFile(dlog_record::getLatestFilePath());
        }
    }
    g_wasExecuting = isExecuting;

    if (g_refreshed) {
        ++g_recording.refreshCounter;
        g_refreshed = false;
    }
}

float getValue(uint32_t rowIndex, uint8_t columnIndex, float *max) {
    uint32_t blockElementAddress = (rowIndex * getNumElementsPerRow() + columnIndex) * sizeof(BlockElement);

    uint32_t blockIndex = blockElementAddress / BLOCK_SIZE;
    uint32_t blockStartAddress = blockIndex * BLOCK_SIZE;

    blockIndex %= NUM_BLOCKS;

    BlockElement *blockElements = getCacheBlock(blockIndex);

    if (!(g_cacheBlocks[blockIndex].valid && g_cacheBlocks[blockIndex].startAddress == blockStartAddress)) {
        for (unsigned i = 0; i < NUM_ELEMENTS_PER_BLOCKS; i++) {
            blockElements[i].min = NAN;
            blockElements[i].max = NAN;
        }

        g_cacheBlocks[blockIndex].valid = 1;
        g_cacheBlocks[blockIndex].loadedValues = 0;
        g_cacheBlocks[blockIndex].startAddress = blockStartAddress;
    }

    if (g_cacheBlocks[blockIndex].loadedValues < NUM_ELEMENTS_PER_BLOCKS && !g_isLoading) {
        g_isLoading = true;
        g_interruptLoading = false;
        g_blockIndexToLoad = blockIndex;
        g_loadScale = g_recording.xAxisDiv / g_recording.xAxisDivMin;

        sendMessageToLowPriorityThread(THREAD_MESSAGE_DLOG_LOAD_BLOCK);
    }

    uint32_t blockElementIndex = (blockElementAddress % BLOCK_SIZE) / sizeof(BlockElement);

    BlockElement *blockElement = blockElements + blockElementIndex;

    if (g_recording.parameters.yAxisScale == dlog_file::SCALE_LOGARITHMIC) {
        float logOffset = 1 - g_recording.parameters.yAxes[columnIndex].range.min;
        *max = log10f(logOffset + blockElement->max);
        return log10f(logOffset + blockElement->min);
    }

    *max = blockElement->max;
    return blockElement->min;
}

void adjustXAxisOffset(Recording &recording) {
    auto duration = getDuration(recording);
    if (recording.xAxisOffset + recording.pageSize * recording.parameters.period > duration) {
        recording.xAxisOffset = duration - recording.pageSize * recording.parameters.period;
    }
    if (recording.xAxisOffset < 0) {
        recording.xAxisOffset = 0;
    }
}

Unit getXAxisUnit(Recording& recording) {
    return recording.parameters.xAxis.scale == dlog_file::SCALE_LINEAR ? recording.parameters.xAxis.unit : UNIT_UNKNOWN;
}

Unit getYAxisUnit(Recording& recording, int dlogValueIndex) {
    return recording.parameters.yAxisScale == dlog_file::SCALE_LINEAR ? recording.parameters.yAxes[dlogValueIndex].unit : UNIT_UNKNOWN;
}

uint32_t getPosition(Recording& recording) {
    if (&recording == &dlog_record::g_recording) {
        return recording.size > recording.pageSize ? recording.size - recording.pageSize : 0;
    } else {
        float position = recording.xAxisOffset / recording.parameters.period;
        if (position < 0) {
            return 0;
        } else if (position > recording.size - recording.pageSize) {
            return recording.size - recording.pageSize;
        } else {
            return (uint32_t)roundf(position);
        }
    }
}

void changeXAxisOffset(Recording &recording, float xAxisOffset) {
    if (&g_recording == &recording) {
        float newXAxisOffset = xAxisOffset;
        if (newXAxisOffset != recording.xAxisOffset) {
            recording.xAxisOffset = newXAxisOffset;
            adjustXAxisOffset(recording);
        }
    } else {
        recording.xAxisOffset = xAxisOffset;
    }
}

void changeXAxisDiv(Recording &recording, float xAxisDiv) {
    if (recording.xAxisDiv != xAxisDiv) {
        recording.xAxisDiv = xAxisDiv;

        if (recording.xAxisDiv == recording.xAxisDivMin) {
            recording.parameters.period = recording.parameters.xAxis.step;
            recording.size = recording.numSamples;
        } else {
            recording.parameters.period = recording.parameters.xAxis.step * recording.xAxisDiv / recording.xAxisDivMin;
            recording.size = (uint32_t)round(recording.numSamples * recording.xAxisDivMin / recording.xAxisDiv);
        }
        
        adjustXAxisOffset(recording);

        invalidateAllBlocks();
    }
}

float getDuration(Recording &recording) {
    if (&recording == &g_recording) {
        return (recording.numSamples - 1) * recording.parameters.xAxis.step;
    }

    return (recording.size - 1) * recording.parameters.xAxis.step;
}

void getLabel(Recording& recording, int valueIndex, char *text, int count) {
    if (recording.parameters.yAxes[valueIndex].label[0]) {
        strncpy(text, recording.parameters.yAxes[valueIndex].label, count - 1);
        text[count - 1] = 0;
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

void initAxis(Recording &recording) {
    recording.parameters.xAxis.unit = UNIT_SECOND;
    recording.parameters.xAxis.step = recording.parameters.period;
    recording.parameters.xAxis.range.min = 0;
    recording.parameters.xAxis.range.max = recording.parameters.duration;

    for (int8_t dlogItemIndex = 0; dlogItemIndex < recording.parameters.numDlogItems; ++dlogItemIndex) {
        auto &dlogItem = recording.parameters.dlogItems[dlogItemIndex];
        auto &yAxis = recording.parameters.yAxes[dlogItemIndex];
        if (dlogItem.resourceType == DLOG_RESOURCE_TYPE_U) {
            yAxis.unit = UNIT_VOLT;
            if (dlogItem.slotIndex != 255) {
                yAxis.range.min = channel_dispatcher::getUMin(dlogItem.slotIndex, dlogItem.subchannelIndex);
                yAxis.range.max = channel_dispatcher::getUMax(dlogItem.slotIndex, dlogItem.subchannelIndex);
                Channel *channel = Channel::getBySlotIndex(dlogItem.slotIndex, dlogItem.subchannelIndex);
                yAxis.channelIndex = channel ? channel->channelIndex : -1;
            } else {
                yAxis.range.min = 0;
                yAxis.range.max = 40.0f;
                yAxis.channelIndex = dlogItem.subchannelIndex;
            }
        } else if (dlogItem.resourceType == DLOG_RESOURCE_TYPE_I) {
            yAxis.unit = UNIT_AMPER;
            if (dlogItem.slotIndex != 255) {
                yAxis.range.min = channel_dispatcher::getIMin(dlogItem.slotIndex, dlogItem.subchannelIndex);
                yAxis.range.max = channel_dispatcher::getIMaxLimit(dlogItem.slotIndex, dlogItem.subchannelIndex);
                Channel *channel = Channel::getBySlotIndex(dlogItem.slotIndex, dlogItem.subchannelIndex);
                yAxis.channelIndex = channel ? channel->channelIndex : -1;
            } else {
                yAxis.range.min = 0;
                yAxis.range.max = 5.0f;
                yAxis.channelIndex = dlogItem.subchannelIndex;
            }
        } else if (dlogItem.resourceType == DLOG_RESOURCE_TYPE_P) {
            yAxis.unit = UNIT_WATT;
            if (dlogItem.slotIndex != 255) {
                Channel *channel = Channel::getBySlotIndex(dlogItem.slotIndex, dlogItem.subchannelIndex);
                yAxis.range.min = channel_dispatcher::getPowerMinLimit(*channel);
                yAxis.range.max = channel_dispatcher::getPowerMaxLimit(*channel);
                yAxis.channelIndex = channel->channelIndex;
            } else {
                yAxis.range.min = 0;
                yAxis.range.max = 200.0f;
                yAxis.channelIndex = dlogItem.subchannelIndex;
            }
        } else if (dlogItem.resourceType >= DLOG_RESOURCE_TYPE_DIN0 && dlogItem.resourceType <= DLOG_RESOURCE_TYPE_DIN7) {
            yAxis.unit = UNIT_BIT;
            yAxis.range.min = 0;
            yAxis.range.max = 1;
            yAxis.channelIndex = dlogItem.resourceType - DLOG_RESOURCE_TYPE_DIN0;
        }
        strcpy(yAxis.label, g_slots[dlogItem.slotIndex]->getDlogResourceLabel(dlogItem.subchannelIndex, dlogItem.resourceIndex));
    }

    recording.parameters.numYAxes = recording.parameters.numDlogItems;
}

void initDlogValues(Recording &recording) {
    uint8_t yAxisIndex;

    int numBitValues = 0;
    for (yAxisIndex = 0; yAxisIndex < MIN(recording.parameters.numYAxes, MAX_NUM_OF_Y_VALUES); yAxisIndex++) {
        if (recording.parameters.yAxes[yAxisIndex].unit == UNIT_BIT) {
            numBitValues++;
        }
    }

    int bitValueIndex = 0;

    for (yAxisIndex = 0; yAxisIndex < MIN(recording.parameters.numYAxes, MAX_NUM_OF_Y_VALUES); yAxisIndex++) {
        recording.dlogValues[yAxisIndex].isVisible = true;

        float rangeMin = recording.parameters.yAxes[yAxisIndex].range.min;
        float rangeMax = recording.parameters.yAxes[yAxisIndex].range.max;

        if (recording.parameters.yAxes[yAxisIndex].unit == UNIT_BIT) {
            float numDivisions = 1.0f * NUM_VERT_DIVISIONS / numBitValues;
            recording.dlogValues[yAxisIndex].div = 1.1f / numDivisions;
            recording.dlogValues[yAxisIndex].offset = recording.dlogValues[yAxisIndex].div * (NUM_VERT_DIVISIONS / 2 - (bitValueIndex++ + 1) * numDivisions);
        } else {
            if (recording.parameters.yAxisScale == dlog_file::SCALE_LOGARITHMIC) {
                float logOffset = 1 - rangeMin;
                rangeMin = 0;
                rangeMax = log10f(logOffset + rangeMax);
            }

            float div = (rangeMax - rangeMin) / NUM_VERT_DIVISIONS;
            recording.dlogValues[yAxisIndex].div = div;
            recording.dlogValues[yAxisIndex].offset = -div * NUM_VERT_DIVISIONS / 2;
        }
    }

    for (; yAxisIndex < MAX_NUM_OF_Y_VALUES; yAxisIndex++) {
        recording.dlogValues[yAxisIndex].isVisible = false;
    }

    recording.selectedVisibleValueIndex = 0;
}

void calcColumnIndexes(Recording &recording) {
    uint32_t columnIndex = 0;
    uint32_t bitMask = 0;

    for (int yAxisIndex = 0; yAxisIndex < recording.parameters.numYAxes; yAxisIndex++) {
        auto &yAxis = recording.parameters.yAxes[yAxisIndex];
        
        recording.columnFloatIndexes[yAxisIndex] = columnIndex;
        
        if (yAxis.unit == UNIT_BIT) {
            if (bitMask == 0) {
                bitMask = 0x4000;
            } else {
                bitMask >>= 1;
            }

            if (bitMask == 1) {
                columnIndex++;
                bitMask = 0;
            }
        } else {
            if (bitMask != 0) {
                columnIndex++;
                bitMask = 0;
            }

            columnIndex++;
        }
    }

    if (bitMask != 0) {
        columnIndex++;
    }

    recording.numFloatsPerRow = columnIndex;
}

int getNumVisibleDlogValues(const Recording &recording) {
    int count = 0;
    for (int dlogValueIndex = 0; dlogValueIndex < MAX_NUM_OF_Y_VALUES; dlogValueIndex++) {
        if (recording.dlogValues[dlogValueIndex].isVisible) {
            count++;
        }
    }
    return count;
}

int getDlogValueIndex(Recording &recording, int visibleDlogValueIndex) {
    int i = 0;
    for (int dlogValueIndex = 0; dlogValueIndex < MAX_NUM_OF_Y_VALUES; dlogValueIndex++) {
        if (recording.dlogValues[dlogValueIndex].isVisible) {
            if (i == visibleDlogValueIndex) {
                return dlogValueIndex;
            }
            i++;
        }
    }
    return -1;
}

int getVisibleDlogValueIndex(Recording &recording, int fromDlogValueIndex) {
    int visibleDlogValueIndex = 0;
    for (int dlogValueIndex = 0; dlogValueIndex < MAX_NUM_OF_Y_VALUES; dlogValueIndex++) {
        if (dlogValueIndex == fromDlogValueIndex) {
            return visibleDlogValueIndex;
        }
        if (recording.dlogValues[dlogValueIndex].isVisible) {
            ++visibleDlogValueIndex;
        }
    }
    return -1;
}

DlogValueParams *getVisibleDlogValueParams(Recording &recording, int visibleDlogValueIndex) {
    int dlogValueIndex = getDlogValueIndex(recording, visibleDlogValueIndex);
    if (dlogValueIndex != -1) {
        return &recording.dlogValues[dlogValueIndex];
    }
    return nullptr;
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

void autoScale(Recording &recording) {
    auto numVisibleDlogValues = getNumVisibleDlogValues(recording);

    for (auto visibleDlogValueIndex = 0; visibleDlogValueIndex < numVisibleDlogValues; visibleDlogValueIndex++) {
        int dlogValueIndex = getDlogValueIndex(recording, visibleDlogValueIndex);
        DlogValueParams &dlogValueParams = recording.dlogValues[dlogValueIndex];

        float numDivisions = 1.0f * NUM_VERT_DIVISIONS / numVisibleDlogValues;

        float rangeMin = recording.parameters.yAxes[dlogValueIndex].range.min;
        float rangeMax = recording.parameters.yAxes[dlogValueIndex].range.max;

        if (recording.parameters.yAxisScale == dlog_file::SCALE_LOGARITHMIC) {
            float logOffset = 1 - rangeMin;
            rangeMin = 0;
            rangeMax = log10f(logOffset + rangeMax);
        }

        if (recording.parameters.yAxes[dlogValueIndex].unit == UNIT_BIT) {
            dlogValueParams.div = 1.1f / numDivisions;
            dlogValueParams.offset = dlogValueParams.div * (NUM_VERT_DIVISIONS / 2 - (visibleDlogValueIndex + 1) * numDivisions);
        } else {
            dlogValueParams.div = (rangeMax - rangeMin) / numDivisions;
            dlogValueParams.offset = dlogValueParams.div * (NUM_VERT_DIVISIONS / 2 - (visibleDlogValueIndex + 1) * numDivisions);
        }
    }
}

void scaleToFit(Recording &recording) {
    auto numVisibleDlogValues = getNumVisibleDlogValues(recording);
    auto startPosition = getPosition(recording);

    float totalMin = FLT_MAX;
    float totalMax = -FLT_MAX;

    for (auto position = startPosition; position < startPosition + recording.pageSize; position++) {
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

    for (auto visibleDlogValueIndex = 0; visibleDlogValueIndex < numVisibleDlogValues; visibleDlogValueIndex++) {
        int dlogValueIndex = getDlogValueIndex(recording, visibleDlogValueIndex);
        DlogValueParams &dlogValueParams = recording.dlogValues[dlogValueIndex];
        float numDivisions = 1.0f * NUM_VERT_DIVISIONS;
        dlogValueParams.div = (totalMax - totalMin) / numDivisions;
        dlogValueParams.offset = - dlogValueParams.div * NUM_VERT_DIVISIONS / 2;
    }
}

bool openFile(const char *filePath, int *err) {
    if (!isLowPriorityThread()) {
        g_state = STATE_LOADING;
        g_loadingStartTickCount = millis();

        strcpy(g_filePath, filePath);
        memset(&g_recording, 0, sizeof(Recording));

        sendMessageToLowPriorityThread(THREAD_MESSAGE_DLOG_SHOW_FILE);
        return true;
    }

    g_state = STATE_LOADING;

    File file;
    if (file.open(filePath != nullptr ? filePath : g_filePath, FILE_OPEN_EXISTING | FILE_READ)) {
        uint8_t * buffer = FILE_VIEW_BUFFER;
        uint32_t read = file.read(buffer, dlog_file::DLOG_VERSION1_HEADER_SIZE);
        if (read == dlog_file::DLOG_VERSION1_HEADER_SIZE) {
			dlog_file::Reader reader(buffer);
		
			uint32_t headerRemaining;
			bool result = reader.readFileHeaderAndMetaFields(g_recording.parameters, headerRemaining);
			if (result) {
				if (reader.getVersion() == dlog_file::VERSION1) {
					uint32_t columns = reader.getColumns();

					for (int channelIndex = 0; channelIndex < CH_MAX; ++channelIndex) {
						if (columns & (1 << (4 * channelIndex))) {
							g_recording.parameters.enableDlogItem(255, channelIndex, 0, true);
						}

						if (columns & (2 << (4 * channelIndex))) {
							g_recording.parameters.enableDlogItem(255, channelIndex, 1, true);
						}

						if (columns & (4 << (4 * channelIndex))) {
							g_recording.parameters.enableDlogItem(255, channelIndex, 2, true);
						}
					}

					initAxis(g_recording);
				}
				else {
					if (headerRemaining > 0) {
						uint32_t read = file.read(buffer + dlog_file::DLOG_VERSION1_HEADER_SIZE, headerRemaining);
						if (read != headerRemaining) {
							result = false;
						}
					}

					if (result)	{
						result = reader.readRemainingFileHeaderAndMetaFields(g_recording.parameters);
					}
				}

				if (result) {
					g_recording.dataOffset = reader.getDataOffset();

					initDlogValues(g_recording);
					calcColumnIndexes(g_recording);

					g_recording.pageSize = VIEW_WIDTH;

					g_recording.numSamples = (file.size() - g_recording.dataOffset) / (g_recording.numFloatsPerRow * sizeof(float));
					g_recording.xAxisDivMin = g_recording.pageSize * g_recording.parameters.period / NUM_HORZ_DIVISIONS;
					g_recording.xAxisDivMax = MAX(g_recording.numSamples, g_recording.pageSize) * g_recording.parameters.period / NUM_HORZ_DIVISIONS;

					g_recording.size = g_recording.numSamples;

					g_recording.xAxisOffset = 0.0f;
					g_recording.xAxisDiv = g_recording.xAxisDivMin;

					g_recording.cursorOffset = VIEW_WIDTH / 2;

					g_recording.getValue = getValue;
					g_isLoading = false;

					if (isMulipleValuesOverlayHeuristic(g_recording)) {
						autoScale(g_recording);
					}

					g_state = STATE_READY;
					invalidateAllBlocks();
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

    if (g_state != STATE_READY) {
        g_state = STATE_ERROR;
    }

    return g_state != STATE_ERROR;
}

Recording &getRecording() {
    return g_showLatest && g_wasExecuting ? dlog_record::g_recording : g_recording;
}

float roundValue(float value) {
    return roundPrec(value, powf(10.0f, floorf(log10f(fabs(value)))) / 1000.0f);
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

class DlogParamsPage : public SetPage {
public:
    static Parameters g_parameters;
    static const int PAGE_SIZE = 6;
    static int g_scrollPosition;
	static float g_minPeriod;

    void pageAlloc() {
        g_numDlogResources = countDlogResources();

        memcpy(&g_parameters, &dlog_record::g_parameters, sizeof(Parameters));
        memcpy(&g_parametersOrig, &dlog_record::g_parameters, sizeof(Parameters));

		g_minPeriod = getMinPeriod();
    }

    int getDirty() {
        return memcmp(&g_parameters, &g_parametersOrig, sizeof(Parameters)) != 0;
    }

    void set() {
        gui::popPage();

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

        memcpy(&dlog_record::g_parameters, &DlogParamsPage::g_parameters, sizeof(DlogParamsPage::g_parameters));
        strcpy(dlog_record::g_parameters.filePath, filePath);

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
		float minPeriod = g_slots[slotIndex]->getDlogResourceMinPeriod(subchannelIndex, resourceIndex);
		if (isNaN(minPeriod)) {
			return PERIOD_MIN;
		}
		return minPeriod;
	}

	static void setPeriod(float value) {
		g_parameters.period = value;

		int numUnchecked = 0;

		for (int slotIndex = 0; slotIndex < NUM_SLOTS; slotIndex++) {
			int numSubchannels = g_slots[slotIndex]->getNumSubchannels();
			for (int subchannelIndex = 0; subchannelIndex < numSubchannels; subchannelIndex++) {
				int numResources = g_slots[slotIndex]->getNumDlogResources(subchannelIndex);
				for (int resourceIndex = 0; resourceIndex < numResources; resourceIndex++) {
					if (g_parameters.isDlogItemEnabled(slotIndex, subchannelIndex, resourceIndex)) {
						if (getResourceMinPeriod(slotIndex, subchannelIndex, resourceIndex) - g_parameters.period >= 1E-6) {
							g_parameters.enableDlogItem(slotIndex, subchannelIndex, resourceIndex, false);
							numUnchecked++;
						}
					}
				}
			}
		}

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

	static int getModuleLocalRecordingSlotIndex() {
		if (g_parameters.period < dlog_view::PERIOD_MIN) {
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
        strcpy(g_parameters.filePath, value);
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

void data_dlog_state(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = dlog_record::getState();
    }
}

void data_dlog_toggle_state(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
    	if (DlogParamsPage::getNumDlogResources() == 0) {
            value = 5;
        } else if (dlog_record::isInStateTransition()) {
    		value = 4;
    	} else if (dlog_record::isIdle()) {
            value = 0;
        } else if (dlog_record::isExecuting()) {
            value = 1;
        } else if (dlog_record::isInitiated() && dlog_record::g_parameters.triggerSource == trigger::SOURCE_MANUAL) {
            value = 2;
        } else {
            value = 3;
        }
    }
}

void data_dlog_period(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = MakeValue(DlogParamsPage::g_parameters.period, UNIT_SECOND);
    } else if (operation == DATA_OPERATION_GET_UNIT) {
        value = UNIT_SECOND;
    } else if (operation == DATA_OPERATION_GET_MIN) {
        value = MakeValue(DlogParamsPage::g_minPeriod, UNIT_SECOND);
    } else if (operation == DATA_OPERATION_GET_MAX) {
        value = MakeValue(PERIOD_MAX, UNIT_SECOND);
    } else if (operation == DATA_OPERATION_SET) {
        DlogParamsPage::setPeriod(value.getFloat());
    }
}

void data_dlog_duration(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = MakeValue(DlogParamsPage::g_parameters.duration, UNIT_SECOND);
    } else if (operation == DATA_OPERATION_GET_UNIT) {
        value = UNIT_SECOND;
    } else if (operation == DATA_OPERATION_GET_MIN) {
        value = MakeValue(DURATION_MIN, UNIT_SECOND);
    } else if (operation == DATA_OPERATION_GET_MAX) {
        value = MakeValue(INFINITY, UNIT_SECOND);
    } else if (operation == DATA_OPERATION_SET) {
        DlogParamsPage::g_parameters.duration = value.getFloat();
    }
}

void data_dlog_file_name(DataOperationEnum operation, Cursor cursor, Value &value) {
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

void data_dlog_start_enabled(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = checkDlogParameters(DlogParamsPage::g_parameters, true, false) == SCPI_RES_OK ? 1 : 0;
    }
}

void data_dlog_view_state(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = dlog_view::getState();
    }
}

void data_recording_ready(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = isExecuting() || getLatestFilePath() ? 1 : 0;
    }
}

void data_dlog_items_scrollbar_enabled(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = DlogParamsPage::getNumDlogResources() > DlogParamsPage::PAGE_SIZE;
    }
}

void data_dlog_items_num_selected(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value.type_ = VALUE_TYPE_NUM_SELECTED;
        value.pairOfUint16_.first = DlogParamsPage::g_parameters.numDlogItems;
        value.pairOfUint16_.second = DlogParamsPage::getNumDlogResources();
    }
}

void data_dlog_items(DataOperationEnum operation, Cursor cursor, Value &value) {
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

void data_dlog_item_is_available(DataOperationEnum operation, Cursor cursor, Value &value) {
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
                value = 1;
            }
        } else {
            value = 0;
        }
    }
}

void data_dlog_item_is_checked(DataOperationEnum operation, Cursor cursor, Value &value) {
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

void data_dlog_item_channel(DataOperationEnum operation, Cursor cursor, Value &value) {
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

void data_dlog_item_label(DataOperationEnum operation, Cursor cursor, Value &value) {
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
    toggleStop();
}

void data_dlog_trigger_source(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = MakeEnumDefinitionValue(DlogParamsPage::g_parameters.triggerSource, ENUM_DEFINITION_TRIGGER_SOURCE);
    }
}

void action_dlog_trigger_select_source() {
    DlogParamsPage::selectTriggerSource();
}

void action_show_dlog_params() {
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
    dlog_view::g_showLatest = true;
    if (!dlog_record::isExecuting()) {
        dlog_view::openFile(dlog_record::getLatestFilePath());
    }
    showPage(PAGE_ID_DLOG_VIEW);
}

void action_dlog_view_show_overlay_options() {
    pushPage(PAGE_ID_DLOG_VIEW_OVERLAY_OPTIONS);
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
    recording.selectedVisibleValueIndex = (recording.selectedVisibleValueIndex + 1) % dlog_view::getNumVisibleDlogValues(recording);
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

void data_recording(DataOperationEnum operation, Cursor cursor, Value &value) {
    auto &recording = dlog_view::getRecording();

    if (operation == DATA_OPERATION_YT_DATA_GET_REFRESH_COUNTER) {
        value = Value(recording.refreshCounter, VALUE_TYPE_UINT32);
    } else if (operation == DATA_OPERATION_YT_DATA_GET_GET_VALUE_FUNC) {
        value = recording.getValue;
    } else if (operation == DATA_OPERATION_YT_DATA_VALUE_IS_VISIBLE) {
        value = Value(recording.dlogValues[value.getUInt8()].isVisible);
    } else if (operation == DATA_OPERATION_YT_DATA_GET_SHOW_LABELS) {
        value = persist_conf::devConf.viewFlags.dlogViewShowLabels ? 1 : 0;
    } else if (operation == DATA_OPERATION_YT_DATA_GET_SELECTED_VALUE_INDEX) {
        value = persist_conf::devConf.viewFlags.dlogViewLegendViewOption != persist_conf::DLOG_VIEW_LEGEND_VIEW_OPTION_HIDDEN ? dlog_view::getDlogValueIndex(recording, recording.selectedVisibleValueIndex) : -1;
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
            if (newPosition + recording.pageSize > recording.size) {
                newPosition = recording.size - recording.pageSize;
            }
        }
        dlog_view::changeXAxisOffset(recording, newPosition * recording.parameters.period);
    } else if (operation == DATA_OPERATION_YT_DATA_GET_PAGE_SIZE) {
        value = Value(recording.pageSize, VALUE_TYPE_UINT32);
    } else if (operation == DATA_OPERATION_YT_DATA_GET_STYLE) {
        uint8_t dlogValueIndex = value.getUInt8();
        uint8_t visibleDlogValueIndex = dlog_view::getVisibleDlogValueIndex(recording, dlogValueIndex);
        uint8_t numYtGraphStyles = sizeof(g_ytGraphStyles) / sizeof(uint16_t);
        if (psu::dlog_view::isMulipleValuesOverlayHeuristic(recording)) {
            value = Value(g_ytGraphStyles[visibleDlogValueIndex % numYtGraphStyles], VALUE_TYPE_UINT16);
        } else {
            if (persist_conf::devConf.viewFlags.dlogViewLegendViewOption != persist_conf::DLOG_VIEW_LEGEND_VIEW_OPTION_HIDDEN && visibleDlogValueIndex == recording.selectedVisibleValueIndex) {
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
    } else if (operation == DATA_OPERATION_YT_DATA_IS_CURSOR_VISIBLE) {
        value = &recording != &dlog_record::g_recording;
    } else if (operation == DATA_OPERATION_YT_DATA_GET_CURSOR_OFFSET) {
        value = Value(recording.cursorOffset, VALUE_TYPE_UINT32);
    } else if (operation == DATA_OPERATION_YT_DATA_TOUCH_DRAG) {
        TouchDrag *touchDrag = (TouchDrag *)value.getVoidPointer();
        
        static float valueAtTouchDown;
        static int xAtTouchDown;
        static int yAtTouchDown;
        
        if (g_focusDataId == DATA_ID_DLOG_VISIBLE_VALUE_OFFSET) {
            dlog_view::DlogValueParams *dlogValueParams = dlog_view::getVisibleDlogValueParams(recording,
                !dlog_view::isMulipleValuesOverlayHeuristic(recording) || persist_conf::devConf.viewFlags.dlogViewLegendViewOption == persist_conf::DLOG_VIEW_LEGEND_VIEW_OPTION_DOCK
                ? recording.selectedVisibleValueIndex : g_focusCursor);

            if (touchDrag->type == EVENT_TYPE_TOUCH_DOWN) {
                valueAtTouchDown = dlogValueParams->offset;
                yAtTouchDown = touchDrag->y;
            } else {
                float newOffset = valueAtTouchDown + dlogValueParams->div * (yAtTouchDown - touchDrag->y) * dlog_view::NUM_VERT_DIVISIONS / dlog_view::VIEW_HEIGHT;

                float min = getMin(g_focusCursor, DATA_ID_DLOG_VISIBLE_VALUE_OFFSET).getFloat();
                float max = getMax(g_focusCursor, DATA_ID_DLOG_VISIBLE_VALUE_OFFSET).getFloat();

                newOffset = clamp(newOffset, min, max);
                
                dlogValueParams->offset = newOffset;
            }
        } else if (g_focusDataId == DATA_ID_DLOG_VISIBLE_VALUE_DIV) {
            dlog_view::DlogValueParams *dlogValueParams = dlog_view::getVisibleDlogValueParams(recording,
                !dlog_view::isMulipleValuesOverlayHeuristic(recording)  || persist_conf::devConf.viewFlags.dlogViewLegendViewOption == persist_conf::DLOG_VIEW_LEGEND_VIEW_OPTION_DOCK
                ? recording.selectedVisibleValueIndex : g_focusCursor);

            if (touchDrag->type == EVENT_TYPE_TOUCH_DOWN) {
                valueAtTouchDown = dlogValueParams->div;

                //uint32_t position = ytDataGetPosition(cursor, DATA_ID_RECORDING);
                //Value::YtDataGetValueFunctionPointer ytDataGetValue = ytDataGetGetValueFunc(cursor, DATA_ID_RECORDING);
                //for (int i = position + touchDrag->x - 10; i < position + touchDrag->x + 10; )

                yAtTouchDown = touchDrag->y;
            } else {
                float scale = 1.0f * (dlog_view::VIEW_HEIGHT - yAtTouchDown) / (dlog_view::VIEW_HEIGHT - touchDrag->y);

                float newDiv = valueAtTouchDown * scale;

                float min = getMin(g_focusCursor, DATA_ID_DLOG_VISIBLE_VALUE_DIV).getFloat();
                float max = getMax(g_focusCursor, DATA_ID_DLOG_VISIBLE_VALUE_DIV).getFloat();

                newDiv = clamp(newDiv, min, max);

                dlogValueParams->offset = dlogValueParams->offset * newDiv / dlogValueParams->div;
                dlogValueParams->div = newDiv;
            }
        } else if (g_focusDataId == DATA_ID_DLOG_X_AXIS_DIV) {
            if (touchDrag->type == EVENT_TYPE_TOUCH_DOWN) {
                valueAtTouchDown = recording.xAxisDiv;
                xAtTouchDown = touchDrag->x;
            } else {
                float scale = 1.0f * (dlog_view::VIEW_WIDTH - touchDrag->x) / (dlog_view::VIEW_WIDTH - xAtTouchDown);

                float newDiv = valueAtTouchDown * scale;

                float min = getMin(g_focusCursor, DATA_ID_DLOG_X_AXIS_DIV).getFloat();
                float max = getMax(g_focusCursor, DATA_ID_DLOG_X_AXIS_DIV).getFloat();

                newDiv = clamp(newDiv, min, max);

                dlog_view::changeXAxisDiv(recording, newDiv);
            }
        } else {
            recording.cursorOffset = MIN(dlog_view::VIEW_WIDTH - 1, MAX(touchDrag->x, 0));
        }
    } else if (operation == DATA_OPERATION_YT_DATA_GET_CURSOR_X_VALUE) {
        float xValue = (dlog_view::getPosition(recording) + recording.cursorOffset) * recording.parameters.period;
        if (recording.parameters.xAxis.scale == dlog_file::SCALE_LOGARITHMIC) {
            xValue = powf(10, recording.parameters.xAxis.range.min + xValue);
        }
        value = Value(roundPrec(xValue, 0.001f), recording.parameters.xAxis.unit);
    } else if (operation == DATA_OPERATION_GET) {
        value = Value((dlog_view::getPosition(recording) + recording.cursorOffset) * recording.parameters.period, dlog_view::getXAxisUnit(recording));
    } else if (operation == DATA_OPERATION_GET_MIN) {
        value = Value(0.0f, dlog_view::getXAxisUnit(recording));
    } else if (operation == DATA_OPERATION_GET_MAX) {
        value = Value(recording.parameters.xAxis.range.max - recording.parameters.xAxis.range.min, dlog_view::getXAxisUnit(recording));
    } else if (operation == DATA_OPERATION_GET_DEF) {
        value = Value((recording.parameters.xAxis.range.max - recording.parameters.xAxis.range.min) / 2, dlog_view::getXAxisUnit(recording));
    } else if (operation == DATA_OPERATION_SET) {
        float cursorOffset = value.getFloat() / recording.parameters.period - dlog_view::getPosition(recording);
        if (cursorOffset < 0.0f) {
            dlog_view::changeXAxisOffset(recording, recording.xAxisOffset + cursorOffset * recording.parameters.period);
            recording.cursorOffset = 0;
        } else if (cursorOffset > dlog_view::VIEW_WIDTH - 1) {
            dlog_view::changeXAxisOffset(recording, recording.xAxisOffset + (cursorOffset - (dlog_view::VIEW_WIDTH - 1)) * recording.parameters.period);
            recording.cursorOffset = dlog_view::VIEW_WIDTH - 1;
        } else {
            recording.cursorOffset = (uint32_t)roundf(cursorOffset);
        }
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

        value = 1;
    }
}

void data_dlog_multiple_values_overlay(DataOperationEnum operation, Cursor cursor, Value &value) {
    static const int NUM_WIDGETS = 2;

    static const int LABELS_CONTAINER_WIDGET = 0;
    static const int DLOG_VALUES_LIST_WIDGET = 1;

    static Overlay overlay;
    static WidgetOverride widgetOverrides[NUM_WIDGETS];

    if (operation == DATA_OPERATION_GET_OVERLAY_DATA) {
        value = Value(&overlay, VALUE_TYPE_POINTER);
    } else if (operation == DATA_OPERATION_UPDATE_OVERLAY_DATA) {
        auto &recording = dlog_view::getRecording();
        
        int state = 0;
        int numVisibleDlogValues = dlog_view::getNumVisibleDlogValues(recording);
        
        if (persist_conf::devConf.viewFlags.dlogViewLegendViewOption == persist_conf::DLOG_VIEW_LEGEND_VIEW_OPTION_FLOAT && dlog_view::isMulipleValuesOverlayHeuristic(recording)) {
            state = numVisibleDlogValues;
        }

        if (overlay.state != state) {
            overlay.state = state;

            if (state != 0) {
                overlay.widgetOverrides = widgetOverrides;

                WidgetCursor &widgetCursor = *(WidgetCursor *)value.getVoidPointer();

                const ContainerWidget *containerWidget = GET_WIDGET_PROPERTY(widgetCursor.widget, specific, const ContainerWidget *);

                const Widget *labelsContainerWidget = GET_WIDGET_LIST_ELEMENT(containerWidget->widgets, LABELS_CONTAINER_WIDGET);
                const Widget *dlogValuesListWidget = GET_WIDGET_LIST_ELEMENT(containerWidget->widgets, DLOG_VALUES_LIST_WIDGET);

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

void data_dlog_single_value_overlay(DataOperationEnum operation, Cursor cursor, Value &value) {
    static Overlay overlay;

    if (operation == DATA_OPERATION_GET_OVERLAY_DATA) {
        value = Value(&overlay, VALUE_TYPE_POINTER);
    } else if (operation == DATA_OPERATION_UPDATE_OVERLAY_DATA) {
        auto &recording = dlog_view::getRecording();
        overlay.state = persist_conf::devConf.viewFlags.dlogViewLegendViewOption == persist_conf::DLOG_VIEW_LEGEND_VIEW_OPTION_FLOAT && !dlog_view::isMulipleValuesOverlayHeuristic(recording);
        
        WidgetCursor &widgetCursor = *(WidgetCursor *)value.getVoidPointer();
        overlay.width = widgetCursor.widget->w;
        overlay.height = widgetCursor.widget->h;
        
        overlay.x = 40;
        overlay.y = 8;

        value = Value(&overlay, VALUE_TYPE_POINTER);
    }
}

void data_dlog_visible_values(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_COUNT) {
        auto &recording = dlog_view::getRecording();
        value = getNumVisibleDlogValues(recording);
    }
}

void data_dlog_visible_value_label(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        auto &recording = dlog_view::getRecording();
        int dlogValueIndex = dlog_view::getDlogValueIndex(recording,
            !dlog_view::isMulipleValuesOverlayHeuristic(recording) || persist_conf::devConf.viewFlags.dlogViewLegendViewOption == persist_conf::DLOG_VIEW_LEGEND_VIEW_OPTION_DOCK
            ? recording.selectedVisibleValueIndex : cursor);
        value = Value(dlogValueIndex, VALUE_TYPE_DLOG_VALUE_LABEL);
    }
}

void guessStepValues(StepValues *stepValues, Unit unit) {
    if (unit == UNIT_VOLT) {
        static float values[] = { 0.001f, 0.01f, 0.1f, 1.0f };
        stepValues->values = values;
        stepValues->count = sizeof(values) / sizeof(float);
    } else if (unit == UNIT_AMPER) {
        static float values[] = { 0.00001f, 0.0001f, 0.001f, 0.01f, 0.1f };
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

void data_dlog_visible_value_div(DataOperationEnum operation, Cursor cursor, Value &value) {
    auto &recording = dlog_view::getRecording();
    int dlogValueIndex = dlog_view::getDlogValueIndex(recording,
        !dlog_view::isMulipleValuesOverlayHeuristic(recording) || persist_conf::devConf.viewFlags.dlogViewLegendViewOption == persist_conf::DLOG_VIEW_LEGEND_VIEW_OPTION_DOCK
        ? recording.selectedVisibleValueIndex : cursor);

    if (operation == DATA_OPERATION_GET) {
        bool focused = g_focusCursor == cursor && g_focusDataId == DATA_ID_DLOG_VISIBLE_VALUE_DIV;
        if (focused && g_focusEditValue.getType() != VALUE_TYPE_NONE) {
            value = g_focusEditValue;
        } else {
            value = Value(dlog_view::roundValue(recording.dlogValues[dlogValueIndex].div), getYAxisUnit(recording, dlogValueIndex));
        }
    } else if (operation == DATA_OPERATION_GET_EDIT_VALUE) {
        bool focused = g_focusCursor == cursor && g_focusDataId == DATA_ID_DLOG_VISIBLE_VALUE_DIV;
        if (focused && g_focusEditValue.getType() != VALUE_TYPE_NONE) {
            value = g_focusEditValue;
        } else {
            value = Value(recording.dlogValues[dlogValueIndex].div, getYAxisUnit(recording, dlogValueIndex));
        }
    } else if (operation == DATA_OPERATION_GET_MIN) {
        value = Value(0.000001f, getYAxisUnit(recording, dlogValueIndex));
    } else if (operation == DATA_OPERATION_GET_MAX) {
        value = Value(100.0f, getYAxisUnit(recording, dlogValueIndex));
    } else if (operation == DATA_OPERATION_SET) {
        recording.dlogValues[dlogValueIndex].offset = recording.dlogValues[dlogValueIndex].offset * value.getFloat() / recording.dlogValues[dlogValueIndex].div;
        recording.dlogValues[dlogValueIndex].div = value.getFloat();
    } else if (operation == DATA_OPERATION_GET_NAME) {
        value = "Div";
    } else if (operation == DATA_OPERATION_GET_UNIT) {
        value = getYAxisUnit(recording, dlogValueIndex);
    } else if (operation == DATA_OPERATION_GET_IS_CHANNEL_DATA) {
        value = 0;
    } else if (operation == DATA_OPERATION_GET_ENCODER_STEP_VALUES) {
        guessStepValues(value.getStepValues(), getYAxisUnit(recording, dlogValueIndex));
        value = 1;
    }
}

void data_dlog_visible_value_offset(DataOperationEnum operation, Cursor cursor, Value &value) {
    auto &recording = dlog_view::getRecording();
    int dlogValueIndex = dlog_view::getDlogValueIndex(recording,
        !dlog_view::isMulipleValuesOverlayHeuristic(recording) || persist_conf::devConf.viewFlags.dlogViewLegendViewOption == persist_conf::DLOG_VIEW_LEGEND_VIEW_OPTION_DOCK
        ? recording.selectedVisibleValueIndex : cursor);

    if (operation == DATA_OPERATION_GET) {
        bool focused = g_focusCursor == cursor && g_focusDataId == DATA_ID_DLOG_VISIBLE_VALUE_OFFSET;
        if (focused && g_focusEditValue.getType() != VALUE_TYPE_NONE) {
            value = g_focusEditValue;
        } else {
            value = Value(dlog_view::roundValue(recording.dlogValues[dlogValueIndex].offset), getYAxisUnit(recording, dlogValueIndex));
        }
    } else if (operation == DATA_OPERATION_GET_EDIT_VALUE) {
        bool focused = g_focusCursor == cursor && g_focusDataId == DATA_ID_DLOG_VISIBLE_VALUE_OFFSET;
        if (focused && g_focusEditValue.getType() != VALUE_TYPE_NONE) {
            value = g_focusEditValue;
        } else {
            value = Value(recording.dlogValues[dlogValueIndex].offset, getYAxisUnit(recording, dlogValueIndex));
        }
    } else if (operation == DATA_OPERATION_GET_MIN) {
        value = Value(-100.0f, getYAxisUnit(recording, dlogValueIndex));
    } else if (operation == DATA_OPERATION_GET_MAX) {
        value = Value(100.0f, getYAxisUnit(recording, dlogValueIndex));
    } else if (operation == DATA_OPERATION_SET) {
        recording.dlogValues[dlogValueIndex].offset = value.getFloat();
    } else if (operation == DATA_OPERATION_GET_NAME) {
        value = "Offset";
    } else if (operation == DATA_OPERATION_GET_UNIT) {
        value = getYAxisUnit(recording, dlogValueIndex);
    } else if (operation == DATA_OPERATION_GET_IS_CHANNEL_DATA) {
        value = 0;
    } else if (operation == DATA_OPERATION_GET_ENCODER_STEP_VALUES) {
        guessStepValues(value.getStepValues(), getYAxisUnit(recording, dlogValueIndex));
        value = 1;
    }
}

void data_dlog_x_axis_offset(DataOperationEnum operation, Cursor cursor, Value &value) {
    auto &recording = dlog_view::getRecording();

    if (operation == DATA_OPERATION_GET) {
        bool focused = g_focusCursor == cursor && g_focusDataId == DATA_ID_DLOG_X_AXIS_OFFSET;
        if (focused && g_focusEditValue.getType() != VALUE_TYPE_NONE) {
            value = g_focusEditValue;
        } else {
            value = Value(recording.xAxisOffset, dlog_view::getXAxisUnit(recording));
        }
    } else if (operation == DATA_OPERATION_GET_MIN) {
        value = Value(recording.parameters.xAxis.range.min, dlog_view::getXAxisUnit(recording));
    } else if (operation == DATA_OPERATION_GET_MAX) {
        value = Value(recording.parameters.xAxis.range.max - recording.pageSize * recording.parameters.period, dlog_view::getXAxisUnit(recording));
    } else if (operation == DATA_OPERATION_SET) {
        dlog_view::changeXAxisOffset(recording, value.getFloat());
    } else if (operation == DATA_OPERATION_GET_NAME) {
        value = "Offset";
    } else if (operation == DATA_OPERATION_GET_UNIT) {
        value = dlog_view::getXAxisUnit(recording);
    } else if (operation == DATA_OPERATION_GET_IS_CHANNEL_DATA) {
        value = 0;
    } else if (operation == DATA_OPERATION_GET_ENCODER_STEP_VALUES) {
        guessStepValues(value.getStepValues(), dlog_view::getXAxisUnit(recording));
        value = 1;
    }
}

void data_dlog_x_axis_div(DataOperationEnum operation, Cursor cursor, Value &value) {
    auto &recording = dlog_view::getRecording();

    if (operation == DATA_OPERATION_GET) {
        bool focused = g_focusCursor == cursor && g_focusDataId == DATA_ID_DLOG_X_AXIS_DIV;
        if (focused && g_focusEditValue.getType() != VALUE_TYPE_NONE) {
            value = g_focusEditValue;
        } else {
            value = Value(recording.xAxisDiv, dlog_view::getXAxisUnit(recording));
        }
    } else if (operation == DATA_OPERATION_GET_MIN || operation == DATA_OPERATION_GET_DEF) {
        value = Value(recording.xAxisDivMin, dlog_view::getXAxisUnit(recording));
    } else if (operation == DATA_OPERATION_GET_MAX) {
        value = Value(recording.xAxisDivMax, dlog_view::getXAxisUnit(recording));
    } else if (operation == DATA_OPERATION_SET) {
        dlog_view::changeXAxisDiv(recording, value.getFloat());
    } else if (operation == DATA_OPERATION_GET_NAME) {
        value = "Div";
    } else if (operation == DATA_OPERATION_GET_UNIT) {
        value = dlog_view::getXAxisUnit(recording);
    } else if (operation == DATA_OPERATION_GET_IS_CHANNEL_DATA) {
        value = 0;
    } else if (operation == DATA_OPERATION_GET_ENCODER_STEP_VALUES) {
        guessStepValues(value.getStepValues(), dlog_view::getXAxisUnit(recording));
        value = 1;
    }
}

void data_dlog_x_axis_max_value(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        auto &recording = dlog_view::getRecording();

        float maxValue = dlog_view::getDuration(recording);

        if (recording.parameters.xAxis.scale == dlog_file::SCALE_LOGARITHMIC) {
            maxValue = powf(10, recording.parameters.xAxis.range.min + maxValue);
        }

        value = Value(maxValue, recording.parameters.xAxis.unit);
    }
}

void data_dlog_x_axis_max_value_label(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        auto &recording = dlog_view::getRecording();
        if (recording.parameters.xAxis.unit == UNIT_SECOND) {
            value = "Total duration";
        } else {
            value = "Max.";
        }
    }
}

void data_dlog_visible_value_cursor(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        auto &recording = dlog_view::getRecording();
        int dlogValueIndex = dlog_view::getDlogValueIndex(recording,
            !dlog_view::isMulipleValuesOverlayHeuristic(recording) || persist_conf::devConf.viewFlags.dlogViewLegendViewOption == persist_conf::DLOG_VIEW_LEGEND_VIEW_OPTION_DOCK
            ? recording.selectedVisibleValueIndex : cursor);
        auto ytDataGetValue = ytDataGetGetValueFunc(cursor, DATA_ID_RECORDING);
        float max;
        float min = ytDataGetValue(ytDataGetPosition(cursor, DATA_ID_RECORDING) + recording.cursorOffset, dlogValueIndex, &max);
        float cursorValue = (min + max) / 2;
        if (recording.parameters.yAxisScale == dlog_file::SCALE_LOGARITHMIC) {
            float logOffset = 1 - recording.parameters.yAxes[dlogValueIndex].range.min;
            cursorValue = powf(10, cursorValue) - logOffset;
        }
        value = Value(cursorValue, recording.parameters.yAxes[dlogValueIndex].unit);

    }
}

void data_dlog_current_time(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = Value(0, VALUE_TYPE_DLOG_CURRENT_TIME);
    }
}

void data_dlog_file_length(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = Value(dlog_record::getFileLength(), VALUE_TYPE_FILE_LENGTH);
    }
}

void data_dlog_value_label(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = Value(cursor, VALUE_TYPE_DLOG_VALUE_LABEL);
    }
}

void data_dlog_view_legend_view_option(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = (int)persist_conf::devConf.viewFlags.dlogViewLegendViewOption;
    }
}

void data_dlog_view_show_labels(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = (int)persist_conf::devConf.viewFlags.dlogViewShowLabels;
    }
}

void data_dlog_preview_overlay(DataOperationEnum operation, Cursor cursor, Value &value) {
    static Overlay overlay;

    if (operation == DATA_OPERATION_GET_OVERLAY_DATA) {
        value = Value(&overlay, VALUE_TYPE_POINTER);
    } else if (operation == DATA_OPERATION_UPDATE_OVERLAY_DATA) {
        overlay.state = dlog_record::isExecuting() && dlog_record::isModuleLocalRecording() ? 1 : 0;
        
        WidgetCursor &widgetCursor = *(WidgetCursor *)value.getVoidPointer();
        overlay.width = widgetCursor.widget->w;
        overlay.height = widgetCursor.widget->h;
        
        overlay.x = widgetCursor.widget->x;
        overlay.y = widgetCursor.widget->y;

        value = Value(&overlay, VALUE_TYPE_POINTER);
    }
}

} // namespace gui

} // namespace eez
