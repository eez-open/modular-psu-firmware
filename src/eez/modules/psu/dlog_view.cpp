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

#include <eez/memory.h>

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

BlockElement *getCacheBlock(unsigned blockIndex) {
    return (BlockElement *)(FILE_VIEW_BUFFER + NUM_BLOCKS * sizeof(CacheBlock) + blockIndex * BLOCK_SIZE);
}

unsigned getNumElementsPerRow() {
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

    File file;
    if (file.open(g_filePath, FILE_OPEN_EXISTING | FILE_READ)) {
        auto numSamplesPerValue = (unsigned)round(g_loadScale);
        auto numElementsPerRow = getNumElementsPerRow();

        BlockElement *blockElements = getCacheBlock(g_blockIndexToLoad);

        uint32_t totalBytesRead = 0;

        uint32_t i = g_cacheBlocks[g_blockIndexToLoad].loadedValues;
        while (i < NUM_ELEMENTS_PER_BLOCKS) {
            auto offset = (uint32_t)roundf((g_blockIndexToLoad * NUM_ELEMENTS_PER_BLOCKS + i) / numElementsPerRow * g_loadScale * g_recording.parameters.numYAxes);

            offset = g_recording.parameters.numYAxes *((offset + g_recording.parameters.numYAxes - 1) / g_recording.parameters.numYAxes);

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
        g_cacheBlocks[blockIndex].loadedValues = 0;
        g_cacheBlocks[blockIndex].startAddress = blockStartAddress;
    }

    if (!g_isLoading && g_cacheBlocks[blockIndex].loadedValues < NUM_ELEMENTS_PER_BLOCKS) {
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

void initAxis(Recording &recording) {
    recording.parameters.xAxis.unit = UNIT_SECOND;
    recording.parameters.xAxis.step = recording.parameters.period;
    recording.parameters.xAxis.range.min = 0;
    recording.parameters.xAxis.range.max = recording.parameters.time;

    uint8_t yAxisIndex = 0;
    for (int8_t channelIndex = 0; channelIndex < CH_NUM; ++channelIndex) {
        Channel &channel = Channel::get(channelIndex);

        if (recording.parameters.logVoltage[channelIndex]) {
            recording.parameters.yAxes[yAxisIndex].unit = UNIT_VOLT;
            recording.parameters.yAxes[yAxisIndex].range.min = channel_dispatcher::getUMin(channel);
            recording.parameters.yAxes[yAxisIndex].range.max = channel_dispatcher::getUMax(channel);
            recording.parameters.yAxes[yAxisIndex].channelIndex = channelIndex;
            ++yAxisIndex;
        }

        if (recording.parameters.logCurrent[channelIndex]) {
            recording.parameters.yAxes[yAxisIndex].unit = UNIT_AMPER;
            recording.parameters.yAxes[yAxisIndex].range.min = channel_dispatcher::getIMin(channel);
            recording.parameters.yAxes[yAxisIndex].range.max = channel_dispatcher::getIMax(channel);
            recording.parameters.yAxes[yAxisIndex].channelIndex = channelIndex;
            ++yAxisIndex;
        }

        if (recording.parameters.logPower[channelIndex]) {
            recording.parameters.yAxes[yAxisIndex].unit = UNIT_WATT;
            recording.parameters.yAxes[yAxisIndex].range.min = channel_dispatcher::getPowerMinLimit(channel);
            recording.parameters.yAxes[yAxisIndex].range.max = channel_dispatcher::getPowerMaxLimit(channel);
            recording.parameters.yAxes[yAxisIndex].channelIndex = channelIndex;
            ++yAxisIndex;
        }
    }

    recording.parameters.numYAxes = yAxisIndex;
}

void initDlogValues(Recording &recording) {
    uint8_t dlogValueIndex = 0;
    for (uint8_t yAxisIndex = 0; yAxisIndex < recording.parameters.numYAxes && dlogValueIndex < MAX_NUM_OF_Y_VALUES; yAxisIndex++) {
        int8_t channelIndex = recording.parameters.yAxes[yAxisIndex].channelIndex;

        // TODO this is not logical
        if (channelIndex == -1) {
            channelIndex = 0;
        }

        if (recording.parameters.yAxes[yAxisIndex].unit == UNIT_VOLT) {
            recording.dlogValues[dlogValueIndex].isVisible = true;
            recording.dlogValues[dlogValueIndex].dlogValueType = (dlog_view::DlogValueType)(3 * channelIndex + dlog_view::DLOG_VALUE_CH1_U);
            recording.dlogValues[dlogValueIndex].channelIndex = channelIndex;
            float div = recording.parameters.yAxes[yAxisIndex].range.max / dlog_view::NUM_VERT_DIVISIONS;
            recording.dlogValues[dlogValueIndex].div = gui::data::Value(roundPrec(div, 0.01f), UNIT_VOLT);
            recording.dlogValues[dlogValueIndex].offset = gui::data::Value(roundPrec(-div * dlog_view::NUM_VERT_DIVISIONS / 2, 0.01f), UNIT_VOLT);

            dlogValueIndex++;
        } else if (recording.parameters.yAxes[yAxisIndex].unit == UNIT_AMPER) {
            recording.dlogValues[dlogValueIndex].isVisible = true;
            recording.dlogValues[dlogValueIndex].dlogValueType = (dlog_view::DlogValueType)(3 * channelIndex + dlog_view::DLOG_VALUE_CH1_I);
            recording.dlogValues[dlogValueIndex].channelIndex = channelIndex;
            float div = recording.parameters.yAxes[yAxisIndex].range.max / dlog_view::NUM_VERT_DIVISIONS;
            recording.dlogValues[dlogValueIndex].div = gui::data::Value(roundPrec(div, 0.01f), UNIT_AMPER);
            recording.dlogValues[dlogValueIndex].offset = gui::data::Value(roundPrec(-div * dlog_view::NUM_VERT_DIVISIONS / 2, 0.01f), UNIT_AMPER);

            dlogValueIndex++;
        } else if (recording.parameters.yAxes[yAxisIndex].unit == UNIT_WATT) {
            recording.dlogValues[dlogValueIndex].isVisible = true;
            recording.dlogValues[dlogValueIndex].dlogValueType = (dlog_view::DlogValueType)(3 * channelIndex + dlog_view::DLOG_VALUE_CH1_P);
            recording.dlogValues[dlogValueIndex].channelIndex = channelIndex;
            float div = recording.parameters.yAxes[yAxisIndex].range.max / dlog_view::NUM_VERT_DIVISIONS;
            recording.dlogValues[dlogValueIndex].div = gui::data::Value(roundPrec(div, 0.01f), UNIT_WATT);
            recording.dlogValues[dlogValueIndex].offset = gui::data::Value(roundPrec(-div * dlog_view::NUM_VERT_DIVISIONS / 2, 0.01f), UNIT_WATT);

            dlogValueIndex++;
        }
    }    
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

        if (dlogValueParams->dlogValueType % 3 == DLOG_VALUE_CH1_U) {
            // TODO this must be read from the file        
            if (Channel::get(dlogValueParams->channelIndex).isInstalled()) {
                div = channel_dispatcher::getUMax(Channel::get(dlogValueParams->channelIndex)) / numDivisions;
            } else {
                div = 40.0f / numDivisions;
            }

            dlogValueParams->div = gui::data::Value(ceilPrec(div, VALUE_PREC), UNIT_VOLT);
        } else if (dlogValueParams->dlogValueType % 3 == DLOG_VALUE_CH1_I) {
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
                        g_recording.parameters.logVoltage[channelIndex] = columns & (1 << (4 * channelIndex)) ? 1 : 0;
                        g_recording.parameters.logCurrent[channelIndex] = columns & (2 << (4 * channelIndex)) ? 1 : 0;
                        g_recording.parameters.logPower[channelIndex] = columns & (4 << (4 * channelIndex)) ? 1 : 0;
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

                        if (fieldId == FIELD_ID_X_UNIT) {
                            g_recording.parameters.xAxis.unit = (Unit)readUint8(buffer, offset);
                        } else if (fieldId == FIELD_ID_X_STEP) {
                            g_recording.parameters.xAxis.step = readFloat(buffer, offset);
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
                        } else if (fieldId >= FIELD_ID_Y_UNIT && fieldId <= FIELD_ID_Y_CHANNEL_INDEX) {
                            uint8_t yAxisIndex = readUint8(buffer, offset);
                            if (yAxisIndex < 1 || yAxisIndex > MAX_NUM_OF_Y_AXES) {
                                invalidHeader = true;
                                break;
                            }
                            yAxisIndex--;
                            if (yAxisIndex >= g_recording.parameters.numYAxes) {
                                g_recording.parameters.numYAxes = yAxisIndex + 1;
                            }

                            fieldDataLength -= sizeof(uint8_t);

                            if (fieldId == FIELD_ID_Y_UNIT) {
                                g_recording.parameters.yAxes[yAxisIndex].unit = (Unit)readUint8(buffer, offset);
                            } else if (fieldId == FIELD_ID_Y_RANGE_MIN) {
                                g_recording.parameters.yAxes[yAxisIndex].range.min = readFloat(buffer, offset);
                            } else if (fieldId == FIELD_ID_Y_RANGE_MAX) {
                                g_recording.parameters.yAxes[yAxisIndex].range.max = readFloat(buffer, offset);
                            } else if (fieldId == FIELD_ID_Y_LABEL) {
                                if (fieldDataLength > MAX_LABEL_LENGTH) {
                                    invalidHeader = true;
                                    break;
                                }
                                for (int i = 0; i < fieldDataLength; i++) {
                                    g_recording.parameters.yAxes[yAxisIndex].label[i] = readUint8(buffer, offset);
                                }
                            } else if (fieldId == FIELD_ID_Y_CHANNEL_INDEX) {
                                g_recording.parameters.yAxes[yAxisIndex].channelIndex = (int16_t)(readUint8(buffer, offset)) - 1;
                            } else {
                                // unknown field, skip
                                offset += fieldDataLength;
                            }
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
					g_recording.parameters.time = g_recording.parameters.xAxis.range.max;
                }

                if (!invalidHeader) {
                    initDlogValues(g_recording);

                    g_recording.pageSize = VIEW_WIDTH;

                    g_recording.numSamples = (file.size() - g_recording.dataOffset) / (g_recording.parameters.numYAxes * sizeof(float));
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
