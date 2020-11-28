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
            if (numDlogItems == MAX_NUM_OF_Y_AXES) {
                return SCPI_ERROR_TOO_MUCH_DATA;
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

uint8_t readUint8(uint8_t *buffer, uint32_t &offset) {
    uint32_t i = offset;
    offset += 1;
    return buffer[i];
}

uint16_t readUint16(uint8_t *buffer, uint32_t &offset) {
    uint32_t i = offset;
    offset += 2;
    return (buffer[i + 1] << 8) | buffer[i];
}

uint32_t readUint32(uint8_t *buffer, uint32_t &offset) {
    uint32_t i = offset;
    offset += 4;
    return (buffer[i + 3] << 24) | (buffer[i + 2] << 16) | (buffer[i + 1] << 8) | buffer[i];
}

float readFloat(uint8_t *buffer, uint32_t &offset) {
    uint32_t value = readUint32(buffer, offset);
    return *((float *)&value);
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
                auto offset = (uint32_t)roundf((g_blockIndexToLoad * NUM_ELEMENTS_PER_BLOCKS + i) / numElementsPerRow * g_loadScale * g_recording.parameters.numYAxes);

                offset = g_recording.parameters.numYAxes * ((offset + g_recording.parameters.numYAxes - 1) / g_recording.parameters.numYAxes);

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
                        uint32_t bytesToRead = MIN(NUM_VALUES_ROWS, numSamplesPerValue - j) * g_recording.parameters.numYAxes * sizeof(float);
                        uint32_t bytesRead = file.read(values, bytesToRead);
                        if (bytesToRead != bytesRead) {
                            i = NUM_ELEMENTS_PER_BLOCKS;
                            goto closeFile;
                        }

                        totalBytesRead += bytesRead;
                    }

                    unsigned valuesOffset = valuesRow * g_recording.parameters.numYAxes;

                    for (unsigned k = 0; k < g_recording.parameters.numYAxes; k++) {
                        if (k < numElementsPerRow) {
                            BlockElement *blockElement = blockElements + i++;

                            float value = values[valuesOffset + k];

                            if (j == 0) {
                                blockElement->min = blockElement->max = value;
                            } else if (value < blockElement->min) {
                                blockElement->min = value;
                            } else if (value > blockElement->max) {
                                blockElement->max = value;
                            }
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

    if (g_recording.parameters.yAxisScale == SCALE_LOGARITHMIC) {
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
    return recording.parameters.xAxis.scale == SCALE_LINEAR ? recording.parameters.xAxis.unit : UNIT_UNKNOWN;
}

Unit getYAxisUnit(Recording& recording, int dlogValueIndex) {
    return recording.parameters.yAxisScale == SCALE_LINEAR ? recording.parameters.yAxes[dlogValueIndex].unit : UNIT_UNKNOWN;
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
    recording.parameters.xAxis.range.max = recording.parameters.time;

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
            yAxis.unit = UNIT_UNKNOWN;
            yAxis.range.min = 0;
            yAxis.range.max = 1;
            yAxis.channelIndex = -1;
        }
        strcpy(yAxis.label, g_slots[dlogItem.slotIndex]->getDlogResourceLabel(dlogItem.subchannelIndex, dlogItem.resourceIndex));
    }

    recording.parameters.numYAxes = recording.parameters.numDlogItems;
}

void initYAxis(Parameters &parameters, int yAxisIndex) {
    memcpy(&parameters.yAxes[yAxisIndex], &parameters.yAxis, sizeof(parameters.yAxis));
}

void initDlogValues(Recording &recording) {
    uint8_t yAxisIndex;
    for (yAxisIndex = 0; yAxisIndex < MIN(recording.parameters.numYAxes, MAX_NUM_OF_Y_VALUES); yAxisIndex++) {
        recording.dlogValues[yAxisIndex].isVisible = true;

        float rangeMin = recording.parameters.yAxes[yAxisIndex].range.min;
        float rangeMax = recording.parameters.yAxes[yAxisIndex].range.max;

        if (recording.parameters.yAxisScale == SCALE_LOGARITHMIC) {
            float logOffset = 1 - rangeMin;
            rangeMin = 0;
            rangeMax = log10f(logOffset + rangeMax);
        }

        float div = (rangeMax - rangeMin) / NUM_VERT_DIVISIONS;
        recording.dlogValues[yAxisIndex].div = div;
        recording.dlogValues[yAxisIndex].offset = -div * NUM_VERT_DIVISIONS / 2;
    }

    for (; yAxisIndex < MAX_NUM_OF_Y_VALUES; yAxisIndex++) {
        recording.dlogValues[yAxisIndex].isVisible = false;
    }

    recording.selectedVisibleValueIndex = 0;
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

        if (recording.parameters.yAxisScale == SCALE_LOGARITHMIC) {
            float logOffset = 1 - rangeMin;
            rangeMin = 0;
            rangeMax = log10f(logOffset + rangeMax);
        }

        dlogValueParams.div = (rangeMax - rangeMin) / numDivisions;
        dlogValueParams.offset = dlogValueParams.div * (NUM_VERT_DIVISIONS / 2 - (visibleDlogValueIndex + 1) * numDivisions);
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
        uint32_t read = file.read(buffer, DLOG_VERSION1_HEADER_SIZE);
        if (read == DLOG_VERSION1_HEADER_SIZE) {
            uint32_t offset = 0;

            uint32_t magic1 = readUint32(buffer, offset);
            uint32_t magic2 = readUint32(buffer, offset);
            uint16_t version = readUint16(buffer, offset);

            if (magic1 == MAGIC1 && magic2 == MAGIC2 && (version == VERSION1 || version == VERSION2)) {
                bool invalidHeader = false;

                if (version == VERSION1) {
                    g_recording.dataOffset = DLOG_VERSION1_HEADER_SIZE;

                    readUint16(buffer, offset); // flags
                    uint32_t columns = readUint32(buffer, offset);
                    float period = readFloat(buffer, offset);
                    float duration = readFloat(buffer, offset);
                    readUint32(buffer, offset); // startTime

                    g_recording.parameters.period = period;
                    g_recording.parameters.time = duration;

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
                } else {
                    readUint16(buffer, offset); // No. of columns
                    g_recording.dataOffset = readUint32(buffer, offset);

                    // read the rest of the header
                    if (DLOG_VERSION1_HEADER_SIZE < g_recording.dataOffset) {
                        uint32_t headerRemaining = g_recording.dataOffset - DLOG_VERSION1_HEADER_SIZE;
                        uint32_t read = file.read(buffer + DLOG_VERSION1_HEADER_SIZE, headerRemaining);
                        if (read != headerRemaining) {
                            invalidHeader = true;
                        }
                    }

                    while (!invalidHeader && offset < g_recording.dataOffset) {
                        uint16_t fieldLength = readUint16(buffer, offset);
                        if (fieldLength == 0) {
                        	break;
                        }

                        if (offset - sizeof(uint16_t) + fieldLength > g_recording.dataOffset) {
                            invalidHeader = true;
                            break;
                        }

                        uint8_t fieldId = readUint8(buffer, offset);

                        uint16_t fieldDataLength = fieldLength - sizeof(uint16_t) - sizeof(uint8_t);

                        if (fieldId == FIELD_ID_COMMENT) {
                            if (fieldDataLength > MAX_COMMENT_LENGTH) {
                                invalidHeader = true;
                                break;
                            }
                            for (int i = 0; i < fieldDataLength; i++) {
                                g_recording.parameters.comment[i] = readUint8(buffer, offset);
                            }
                            g_recording.parameters.comment[MAX_COMMENT_LENGTH] = 0;
                        } else if (fieldId == FIELD_ID_X_UNIT) {
                            g_recording.parameters.xAxis.unit = (Unit)readUint8(buffer, offset);
                        } else if (fieldId == FIELD_ID_X_STEP) {
                            g_recording.parameters.xAxis.step = readFloat(buffer, offset);
                        } else if (fieldId == FIELD_ID_X_SCALE) {
                            g_recording.parameters.xAxis.scale = (Scale)readUint8(buffer, offset);
                        } else if (fieldId == FIELD_ID_X_RANGE_MIN) {
                            g_recording.parameters.xAxis.range.min = readFloat(buffer, offset);
                        } else if (fieldId == FIELD_ID_X_RANGE_MAX) {
                            g_recording.parameters.xAxis.range.max = readFloat(buffer, offset);
                        } else if (fieldId == FIELD_ID_X_LABEL) {
                            if (fieldDataLength > MAX_LABEL_LENGTH) {
                                invalidHeader = true;
                                break;
                            }
                            for (int i = 0; i < fieldDataLength; i++) {
                                g_recording.parameters.xAxis.label[i] = readUint8(buffer, offset);
                            }
                            g_recording.parameters.xAxis.label[MAX_LABEL_LENGTH] = 0;
                        } else if (fieldId >= FIELD_ID_Y_UNIT && fieldId <= FIELD_ID_Y_CHANNEL_INDEX) {
                            int8_t yAxisIndex = (int8_t)readUint8(buffer, offset);
                            if (yAxisIndex > MAX_NUM_OF_Y_AXES) {
                                invalidHeader = true;
                                break;
                            }

                            fieldDataLength -= sizeof(uint8_t);

                            yAxisIndex--;
                            if (yAxisIndex >= g_recording.parameters.numYAxes) {
                                g_recording.parameters.numYAxes = yAxisIndex + 1;
                                initYAxis(g_recording.parameters, yAxisIndex);
                            }

                            YAxis &destYAxis = yAxisIndex >= 0 ? g_recording.parameters.yAxes[yAxisIndex] : g_recording.parameters.yAxis;

                            if (fieldId == FIELD_ID_Y_UNIT) {
                                destYAxis.unit = (Unit)readUint8(buffer, offset);
                            } else if (fieldId == FIELD_ID_Y_RANGE_MIN) {
                                destYAxis.range.min = readFloat(buffer, offset);
                            } else if (fieldId == FIELD_ID_Y_RANGE_MAX) {
                                destYAxis.range.max = readFloat(buffer, offset);
                            } else if (fieldId == FIELD_ID_Y_LABEL) {
                                if (fieldDataLength > MAX_LABEL_LENGTH) {
                                    invalidHeader = true;
                                    break;
                                }
                                for (int i = 0; i < fieldDataLength; i++) {
                                    destYAxis.label[i] = readUint8(buffer, offset);
                                }
                                destYAxis.label[MAX_LABEL_LENGTH] = 0;
                            } else if (fieldId == FIELD_ID_Y_CHANNEL_INDEX) {
                                destYAxis.channelIndex = (int16_t)(readUint8(buffer, offset)) - 1;
                            } else {
                                // unknown field, skip
                                offset += fieldDataLength;
                            }
                        } else if (fieldId == FIELD_ID_Y_SCALE) {
                            g_recording.parameters.yAxisScale = (Scale)readUint8(buffer, offset);
                        } else if (fieldId == FIELD_ID_CHANNEL_MODULE_TYPE) {
                            readUint8(buffer, offset); // channel index
                            readUint16(buffer, offset); // module type
                        } else if (fieldId == FIELD_ID_CHANNEL_MODULE_REVISION) {
                            readUint8(buffer, offset); // channel index
                            readUint16(buffer, offset); // module revision
                        } else {
                            // unknown field, skip
                            offset += fieldDataLength;
                        }
                    }

					g_recording.parameters.period = g_recording.parameters.xAxis.step;
					g_recording.parameters.time = g_recording.parameters.xAxis.range.max - g_recording.parameters.xAxis.range.min;
                }

                if (!invalidHeader) {
                    initDlogValues(g_recording);

                    g_recording.pageSize = VIEW_WIDTH;

                    g_recording.numSamples = (file.size() - g_recording.dataOffset) / (g_recording.parameters.numYAxes * sizeof(float));
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
    if (value >= 10) {
        return roundPrec(value, 0.1f);
    } else if (value >= 1) {
        return roundPrec(value, 0.01f);
    } else {
        return roundPrec(value, 0.001f);
    }
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
    static Parameters g_guiParameters;
    static const int PAGE_SIZE = 6;
    static int g_scrollPosition;

    void pageAlloc() {
        g_numDlogResources = countDlogResources();

        memcpy(&g_guiParameters, &dlog_record::g_parameters, sizeof(Parameters));
        memcpy(&g_guiParametersOrig, &dlog_record::g_parameters, sizeof(Parameters));
    }

    int getDirty() {
        return memcmp(&g_guiParameters, &g_guiParametersOrig, sizeof(Parameters)) != 0;
    }

    void set() {
        gui::popPage();

        char filePath[MAX_PATH_LENGTH + 50];

        if (isStringEmpty(DlogParamsPage::g_guiParameters.filePath)) {
            uint8_t year, month, day, hour, minute, second;
            datetime::getDateTime(year, month, day, hour, minute, second);

            if (persist_conf::devConf.dateTimeFormat == datetime::FORMAT_DMY_24) {
                snprintf(filePath, sizeof(filePath), "%s/%s%02d_%02d_%02d-%02d_%02d_%02d.dlog",
                    RECORDINGS_DIR,
                    DlogParamsPage::g_guiParameters.filePath,
                    (int)day, (int)month, (int)year,
                    (int)hour, (int)minute, (int)second);
            } else if (persist_conf::devConf.dateTimeFormat == datetime::FORMAT_MDY_24) {
                snprintf(filePath, sizeof(filePath), "%s/%s%02d_%02d_%02d-%02d_%02d_%02d.dlog",
                    RECORDINGS_DIR,
                    DlogParamsPage::g_guiParameters.filePath,
                    (int)month, (int)day, (int)year,
                    (int)hour, (int)minute, (int)second);
            } else if (persist_conf::devConf.dateTimeFormat == datetime::FORMAT_DMY_12) {
                bool am;
                datetime::convertTime24to12(hour, am);
                snprintf(filePath, sizeof(filePath), "%s/%s%02d_%02d_%02d-%02d_%02d_%02d_%s.dlog",
                    RECORDINGS_DIR,
                    DlogParamsPage::g_guiParameters.filePath,
                    (int)day, (int)month, (int)year,
                    (int)hour, (int)minute, (int)second, am ? "AM" : "PM");
            } else if (persist_conf::devConf.dateTimeFormat == datetime::FORMAT_MDY_12) {
                bool am;
                datetime::convertTime24to12(hour, am);
                snprintf(filePath, sizeof(filePath), "%s/%s%02d_%02d_%02d-%02d_%02d_%02d_%s.dlog",
                    RECORDINGS_DIR,
                    DlogParamsPage::g_guiParameters.filePath,
                    (int)month, (int)day, (int)year,
                    (int)hour, (int)minute, (int)second, am ? "AM" : "PM");
            }
        } else {
            snprintf(filePath, sizeof(filePath), "%s/%s.dlog", RECORDINGS_DIR, DlogParamsPage::g_guiParameters.filePath);
        }

        memcpy(&dlog_record::g_parameters, &DlogParamsPage::g_guiParameters, sizeof(DlogParamsPage::g_guiParameters));
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

private:
    static Parameters g_guiParametersOrig;
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
};

Parameters DlogParamsPage::g_guiParameters;
Parameters DlogParamsPage::g_guiParametersOrig;
int DlogParamsPage::g_numDlogResources = -1;
int DlogParamsPage::g_scrollPosition;

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
        value = MakeValue(DlogParamsPage::g_guiParameters.period, UNIT_SECOND);
    } else if (operation == DATA_OPERATION_GET_UNIT) {
        value = UNIT_SECOND;
    } else if (operation == DATA_OPERATION_GET_MIN) {
        value = MakeValue(PERIOD_MIN, UNIT_SECOND);
    } else if (operation == DATA_OPERATION_GET_MAX) {
        value = MakeValue(PERIOD_MAX, UNIT_SECOND);
    } else if (operation == DATA_OPERATION_SET) {
        DlogParamsPage::g_guiParameters.period = value.getFloat();
    }
}

void data_dlog_duration(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = MakeValue(DlogParamsPage::g_guiParameters.time, UNIT_SECOND);
    } else if (operation == DATA_OPERATION_GET_UNIT) {
        value = UNIT_SECOND;
    } else if (operation == DATA_OPERATION_GET_MIN) {
        value = MakeValue(TIME_MIN, UNIT_SECOND);
    } else if (operation == DATA_OPERATION_GET_MAX) {
        value = MakeValue(INFINITY, UNIT_SECOND);
    } else if (operation == DATA_OPERATION_SET) {
        DlogParamsPage::g_guiParameters.time = value.getFloat();
    }
}

void data_dlog_file_name(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = DlogParamsPage::g_guiParameters.filePath;
    } else if (operation == DATA_OPERATION_SET) {
        strcpy(DlogParamsPage::g_guiParameters.filePath, value.getString());
    } else if (operation == DATA_OPERATION_GET_MAX) {
        value = Value(MAX_PATH_LENGTH, VALUE_TYPE_UINT32);
    }
}

void data_dlog_start_enabled(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = checkDlogParameters(DlogParamsPage::g_guiParameters, true, false) == SCPI_RES_OK ? 1 : 0;
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
        value.pairOfUint16_.first = DlogParamsPage::g_guiParameters.numDlogItems;
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

void data_dlog_item_status(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        int slotIndex;
        int subchannelIndex;
        int resourceIndex;
        if (DlogParamsPage::findResource(cursor, slotIndex, subchannelIndex, resourceIndex)) {
            bool enabled = DlogParamsPage::g_guiParameters.isDlogItemEnabled(slotIndex, subchannelIndex, resourceIndex);
            if (!enabled && DlogParamsPage::g_guiParameters.numDlogItems >= MAX_NUM_OF_Y_AXES) {
                value = 2;
            } else {
                value = enabled;
            }
        } else {
            value = 2;
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
        DlogParamsPage::g_guiParameters.enableDlogItem(slotIndex, subchannelIndex, resourceIndex,
            !DlogParamsPage::g_guiParameters.isDlogItemEnabled(slotIndex, subchannelIndex, resourceIndex));
    }
}

void action_dlog_toggle() {
    toggleStop();
}

} // namespace gui

} // namespace eez
