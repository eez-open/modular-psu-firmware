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

#if OPTION_SD_CARD

#include <string.h>
#include <assert.h>

#include <eez/system.h>

#include <eez/scpi/scpi.h>

#include <eez/modules/psu/psu.h>
#include <eez/modules/psu/channel_dispatcher.h>
#include <eez/modules/psu/dlog_view.h>
#include <eez/modules/psu/dlog_record.h>

#include <eez/gui/gui.h>

#include <eez/libs/sd_fat/sd_fat.h>

#ifdef EEZ_PLATFORM_STM32
#include <eez/platform/stm32/defines.h>
#endif

namespace eez {

using namespace scpi;

namespace psu {
namespace dlog_view {

static State g_state;
static uint32_t g_loadingStartTickCount;
bool g_showLatest = true;
bool g_overlayMinimized;
char g_filePath[MAX_PATH_LENGTH + 1];
Recording g_recording;

#ifdef EEZ_PLATFORM_STM32
static uint8_t *g_buffer = (uint8_t *)DLOG_VIEW_BUFFER;
#endif

#ifdef EEZ_PLATFORM_SIMULATOR
#define DLOG_VIEW_BUFFER_SIZE (896 * 1024)
static uint8_t g_bufferMemory[DLOG_VIEW_BUFFER_SIZE];
static uint8_t *g_buffer = g_bufferMemory;
#endif

struct CacheBlock {
    unsigned valid: 1;
    unsigned loaded: 1;
    uint32_t startAddress;
};

struct BlockElement {
    float min;
    float max;
};

static const uint32_t NUM_ELEMENTS_PER_BLOCKS = 480 * MAX_NUM_OF_Y_VALUES;
static const uint32_t BLOCK_SIZE = NUM_ELEMENTS_PER_BLOCKS * sizeof(BlockElement);
static const uint32_t NUM_BLOCKS = DLOG_VIEW_BUFFER_SIZE / (BLOCK_SIZE + sizeof(CacheBlock));

CacheBlock *g_cacheBlocks = (CacheBlock *)g_buffer;

static bool g_isLoading;
static bool g_interruptLoading;
static uint32_t g_blockIndexToLoad;
static float g_loadScale;
static bool g_refreshed;
static bool g_wasExecuting;

State getState() {
    if (g_showLatest) {
        if (dlog_record::isExecuting()) {
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

uint16_t readUint16(uint8_t *buffer, uint32_t *offset) {
    uint32_t i = *offset;
    *offset += 2;
    return (buffer[i + 1] << 8) | buffer[i];
}

uint32_t readUint32(uint8_t *buffer, uint32_t *offset) {
    uint32_t i = *offset;
    *offset += 4;
    return (buffer[i + 3] << 24) | (buffer[i + 2] << 16) | (buffer[i + 1] << 8) | buffer[i];
}

float readFloat(uint8_t *buffer, uint32_t *offset) {
    uint32_t value = readUint32(buffer, offset);
    return *((float *)&value);
}

BlockElement *getCacheBlock(unsigned blockIndex) {
    return (BlockElement *)(g_buffer + NUM_BLOCKS * sizeof(CacheBlock) + blockIndex * BLOCK_SIZE);
}

unsigned getNumElementsPerRow() {
    return MIN(g_recording.totalDlogValues, MAX_NUM_OF_Y_VALUES);
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

    File file;
    if (file.open(g_filePath, FILE_OPEN_EXISTING | FILE_READ)) {
        auto numSamplesPerValue = (unsigned)round(g_loadScale);
        auto numElementsPerRow = getNumElementsPerRow();

        BlockElement *blockElements = getCacheBlock(g_blockIndexToLoad);
            
        for (unsigned i = 0; i < NUM_ELEMENTS_PER_BLOCKS && !g_interruptLoading;) {
            auto offset = (uint32_t)roundf((g_blockIndexToLoad * NUM_ELEMENTS_PER_BLOCKS + i) / numElementsPerRow * g_loadScale * g_recording.totalDlogValues);

            offset = g_recording.totalDlogValues *((offset + g_recording.totalDlogValues - 1) / g_recording.totalDlogValues);

            file.seek(DLOG_HEADER_SIZE + offset * sizeof(float));

            unsigned iStart = i;

            for (unsigned j = 0; j < numSamplesPerValue; j++) {
                i = iStart;

                auto valuesRow = j % NUM_VALUES_ROWS;
                if (valuesRow == 0) {
                    // read up to NUM_VALUES_ROWS
                    file.read(values, MIN(NUM_VALUES_ROWS, numSamplesPerValue - j) * g_recording.totalDlogValues * sizeof(float));
                }

                unsigned valuesOffset = valuesRow * g_recording.totalDlogValues;

                for (unsigned k = 0; k < g_recording.totalDlogValues; k++) {
                    if (k < numElementsPerRow) {
                        BlockElement *blockElement = blockElements + i;
                        i++;

                        float value = values[valuesOffset + k];

                        if (isNaN(blockElement->min) || value < blockElement->min) {
                            blockElement->min = value;
                        }

                        if (isNaN(blockElement->max) || value > blockElement->max) {
                            blockElement->max = value;
                        }
                    }
                }
            }

            g_refreshed = true;
        }

        g_cacheBlocks[g_blockIndexToLoad].loaded = 1;
    }

    file.close();

    g_isLoading = false;
    g_refreshed = true;
}

void stateManagment() {
    auto isExecuting = dlog_record::isExecuting();
    if (!isExecuting && g_wasExecuting && g_showLatest && gui::getActivePageId() == gui::PAGE_ID_DLOG_VIEW) {
        openFile(dlog_record::getLatestFilePath());
    }
    g_wasExecuting = isExecuting;

    if (g_refreshed) {
        ++g_recording.refreshCounter;
        g_refreshed = false;
    }
}

float getValue(int rowIndex, int columnIndex, float *max) {
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
        g_cacheBlocks[blockIndex].loaded = 0;
        g_cacheBlocks[blockIndex].startAddress = blockStartAddress;
    }

    if (!g_cacheBlocks[blockIndex].loaded && !g_isLoading) {
        g_isLoading = true;
        g_interruptLoading = false;
        g_blockIndexToLoad = blockIndex;
        g_loadScale = g_recording.timeDiv / g_recording.timeDivMin;

        osMessagePut(g_scpiMessageQueueId, SCPI_QUEUE_MESSAGE(SCPI_QUEUE_MESSAGE_TARGET_NONE, SCPI_QUEUE_MESSAGE_DLOG_LOAD_BLOCK, 0), osWaitForever);
    }

    uint32_t blockElementIndex = (blockElementAddress % BLOCK_SIZE) / sizeof(BlockElement);

    BlockElement *blockElement = blockElements + blockElementIndex;
    *max = blockElement->max;
    return blockElement->min;
}

void adjustTimeOffset(Recording &recording) {
    auto duration = getDuration(recording);
    if (recording.timeOffset + recording.pageSize * recording.parameters.period > duration) {
        recording.timeOffset = roundPrec(duration - recording.pageSize * recording.parameters.period, TIME_PREC);
        if (recording.timeOffset < 0) {
            recording.timeOffset = 0;
        }
    }
}

float getMaxTimeOffset(Recording &recording) {
    return roundPrec((recording.size - recording.pageSize) * recording.parameters.period, TIME_PREC);
}

void changeTimeOffset(Recording &recording, float timeOffset) {
    if (&dlog_view::g_recording == &recording) {
        float newTimeOffset = roundPrec(timeOffset, TIME_PREC);
        if (newTimeOffset != recording.timeOffset) {
            recording.timeOffset = newTimeOffset;
            adjustTimeOffset(recording);
        }
    } else {
        recording.timeOffset = timeOffset;
    }
}

void changeTimeDiv(Recording &recording, float timeDiv) {
    float newTimeDiv = timeDiv != recording.timeDivMin ? roundPrec(timeDiv, TIME_PREC) : timeDiv;

    if (recording.timeDiv != newTimeDiv) {
        recording.timeDiv = newTimeDiv;

        if (recording.timeDiv == recording.timeDivMin) {
            recording.parameters.period = recording.minPeriod;
            recording.size = recording.numSamples;
        } else {
            recording.parameters.period = recording.minPeriod * recording.timeDiv / recording.timeDivMin;
            recording.size = (uint32_t)round(recording.numSamples * recording.timeDivMin / recording.timeDiv);
        }
        
        adjustTimeOffset(recording);

        invalidateAllBlocks();
    }
}

float getDuration(Recording &recording) {
    if (&recording == &g_recording) {
        return recording.numSamples * recording.minPeriod;
    }

    return recording.size * recording.parameters.period;
}

void setDlogValue(int dlogValueIndex, int channelIndex, DlogValueType valueType) {
    g_recording.dlogValues[dlogValueIndex].isVisible = true;
    g_recording.dlogValues[dlogValueIndex].dlogValueType = (DlogValueType)(3 * channelIndex + valueType);
    g_recording.dlogValues[dlogValueIndex].channelIndex = channelIndex;
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

int getVisibleDlogValueIndex(Recording &recording, int visibleDlogValueIndex) {
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

DlogValueParams *getVisibleDlogValueParams(Recording &recording, int visibleDlogValueIndex) {
    int dlogValueIndex = getVisibleDlogValueIndex(recording, visibleDlogValueIndex);
    if (dlogValueIndex != -1) {
        return &recording.dlogValues[dlogValueIndex];
    }
    return nullptr;
}

void autoScale(Recording &recording) {
    auto numVisibleDlogValues = getNumVisibleDlogValues(recording);

    for (auto visibleDlogValueIndex = 0; visibleDlogValueIndex < numVisibleDlogValues; visibleDlogValueIndex++) {
        DlogValueParams *dlogValueParams = getVisibleDlogValueParams(recording, visibleDlogValueIndex);

        float div;

        float numDivisions = 1.0f * NUM_VERT_DIVISIONS / numVisibleDlogValues;

        if (dlogValueParams->dlogValueType == DLOG_VALUE_CH1_U) {
            // TODO this must be read from the file        
            if (Channel::get(dlogValueParams->channelIndex).isInstalled()) {
                div = channel_dispatcher::getUMax(Channel::get(dlogValueParams->channelIndex)) / numDivisions;
            } else {
                div = 40.0f / numDivisions;
            }

            dlogValueParams->div = gui::data::Value(ceilPrec(div, VALUE_PREC), UNIT_VOLT);
        } else if (dlogValueParams->dlogValueType == DLOG_VALUE_CH1_I) {
            // TODO this must be read from the file
            if (Channel::get(dlogValueParams->channelIndex).isInstalled()) {
                div = channel_dispatcher::getIMax(Channel::get(dlogValueParams->channelIndex)) / numDivisions;
            } else {
                div = 5.0f / numDivisions;
            }

            dlogValueParams->div = gui::data::Value(ceilPrec(div, VALUE_PREC), UNIT_AMPER);
        } else {
            // TODO this must be read from the file
            if (Channel::get(dlogValueParams->channelIndex).isInstalled()) {
                div = channel_dispatcher::getPowerMaxLimit(Channel::get(dlogValueParams->channelIndex)) / numDivisions;
            } else {
                div = 155.0f / numDivisions;
            }

            dlogValueParams->div = gui::data::Value(ceilPrec(div, VALUE_PREC), UNIT_WATT);
        }

        dlogValueParams->offset = gui::data::Value(roundPrec(div * (NUM_VERT_DIVISIONS / 2 - (visibleDlogValueIndex + 1) * numDivisions), VALUE_PREC), dlogValueParams->div.getUnit());
    }
}

void openFile(const char *filePath) {
    if (osThreadGetId() != g_scpiTaskHandle) {
        g_state = STATE_LOADING;
        g_loadingStartTickCount = millis();

        strcpy(g_filePath, filePath);
        memset(&g_recording, 0, sizeof(Recording));

        osMessagePut(g_scpiMessageQueueId, SCPI_QUEUE_MESSAGE(SCPI_QUEUE_MESSAGE_TARGET_NONE, SCPI_QUEUE_MESSAGE_DLOG_SHOW_FILE, 0), osWaitForever);
        return;
    }

    File file;
    if (file.open(g_filePath, FILE_OPEN_EXISTING | FILE_READ)) {
        uint8_t buffer[DLOG_HEADER_SIZE];
        uint32_t read = file.read(buffer, sizeof(buffer));
        if (read == DLOG_HEADER_SIZE) {
            uint32_t offset = 0;
            uint32_t magic1 = readUint32(buffer, &offset);
            uint32_t magic2 = readUint32(buffer, &offset);
            uint16_t version = readUint16(buffer, &offset);

            if (magic1 == MAGIC1 && magic2 == MAGIC2 && version == VERSION) {
                readUint16(buffer, &offset); // flags
                uint32_t columns = readUint32(buffer, &offset);
                float period = readFloat(buffer, &offset);
                float duration = readFloat(buffer, &offset);
                readUint32(buffer, &offset); // startTime

                g_recording.parameters.period = period;
                g_recording.parameters.time = duration;

                g_recording.totalDlogValues = 0;
                unsigned int dlogValueIndex = 0;

                for (int channelIndex = 0; channelIndex < CH_MAX; ++channelIndex) {
                    if (columns & (1 << (4 * channelIndex))) {
                        ++g_recording.totalDlogValues;
                        g_recording.parameters.logVoltage[channelIndex] = 1;
                        if (dlogValueIndex < MAX_NUM_OF_Y_VALUES) {
                            setDlogValue(dlogValueIndex, channelIndex, DLOG_VALUE_CH1_U);
                            dlogValueIndex++;
                        }
                    }

                    if (columns & (2 << (4 * channelIndex))) {
                        ++g_recording.totalDlogValues;
                        g_recording.parameters.logCurrent[channelIndex] = 1;
                        if (dlogValueIndex < MAX_NUM_OF_Y_VALUES) {
                            setDlogValue(dlogValueIndex, channelIndex, DLOG_VALUE_CH1_I);
                            dlogValueIndex++;
                        }
                    }

                    if (columns & (4 << (4 * channelIndex))) {
                        ++g_recording.totalDlogValues;
                        g_recording.parameters.logPower[channelIndex] = 1;
                        if (dlogValueIndex < MAX_NUM_OF_Y_VALUES) {
                            setDlogValue(dlogValueIndex, channelIndex, DLOG_VALUE_CH1_P);
                            dlogValueIndex++;
                        }
                    }
                }

                g_recording.pageSize = VIEW_WIDTH;

                g_recording.numSamples = (file.size() - DLOG_HEADER_SIZE) / (g_recording.totalDlogValues * 4);
                g_recording.minPeriod = g_recording.parameters.period;
                g_recording.timeDivMin = g_recording.pageSize * g_recording.parameters.period / dlog_view::NUM_HORZ_DIVISIONS;
                g_recording.timeDivMax = roundPrec(MAX(g_recording.numSamples, g_recording.pageSize) * g_recording.parameters.period / dlog_view::NUM_HORZ_DIVISIONS, TIME_PREC);

                g_recording.size = g_recording.numSamples;

                g_recording.timeOffset = 0.0f;
                g_recording.timeDiv = g_recording.timeDivMin;

                g_recording.cursorOffset = VIEW_WIDTH / 2;

                g_recording.getValue = getValue;
                g_overlayMinimized = false;
                g_isLoading = false;

                autoScale(g_recording);

                g_state = STATE_READY;

                invalidateAllBlocks();
            }
        }
    }

    file.close();

    if (g_state == STATE_LOADING) {
        g_state = STATE_ERROR;
    }
}

Recording &getRecording() {
    return g_showLatest && dlog_record::isExecuting() ? dlog_record::g_recording : g_recording;
}

} // namespace dlog_view
} // namespace psu
} // namespace eez

#endif // OPTION_SD_CARD
