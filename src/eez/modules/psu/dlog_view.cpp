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

static const uint32_t BLOCK_SIZE_WITH_HEADER = 1024;
static const uint32_t BLOCK_HEADER_SIZE = 4;
static const uint32_t BLOCK_SIZE_WITHOUT_HEADER = BLOCK_SIZE_WITH_HEADER - BLOCK_HEADER_SIZE;
static const uint32_t NUM_BLOCKS = DLOG_VIEW_BUFFER_SIZE / BLOCK_SIZE_WITH_HEADER;
static const uint32_t INVALID_BLOCK_ADDRESS = 0x1;

static bool g_isLoading;
static uint32_t g_blockIndexToLoad;
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

void invalidateAllBlocks() {
    for (uint32_t blockIndex = 0; blockIndex < NUM_BLOCKS; blockIndex++) {
        g_buffer[blockIndex * BLOCK_SIZE_WITH_HEADER] = INVALID_BLOCK_ADDRESS;
    }
}

void loadBlock() {
    File file;
    if (file.open(g_filePath, FILE_OPEN_EXISTING | FILE_READ)) {
        uint32_t blockAddress = *((uint32_t *)&g_buffer[g_blockIndexToLoad * BLOCK_SIZE_WITH_HEADER]);
        file.seek(DLOG_HEADER_SIZE + blockAddress);
        uint32_t read = file.read(g_buffer + g_blockIndexToLoad * BLOCK_SIZE_WITH_HEADER + BLOCK_HEADER_SIZE, BLOCK_SIZE_WITHOUT_HEADER);
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

eez::gui::data::Value getValue(int rowIndex, int columnIndex) {
    uint32_t address = (rowIndex * g_recording.totalDlogValues + columnIndex) * 4;

    uint32_t blockAddress = address / BLOCK_SIZE_WITHOUT_HEADER;
    uint32_t blockOffset = address % BLOCK_SIZE_WITHOUT_HEADER;

    uint32_t blockIndex = blockAddress % NUM_BLOCKS;

    blockAddress *= BLOCK_SIZE_WITHOUT_HEADER;

    if (g_buffer[blockIndex * BLOCK_SIZE_WITH_HEADER] == INVALID_BLOCK_ADDRESS || *((uint32_t *)&g_buffer[blockIndex * BLOCK_SIZE_WITH_HEADER]) != blockAddress) {
        float *p = (float *)(g_buffer + blockIndex * BLOCK_SIZE_WITH_HEADER + BLOCK_HEADER_SIZE);
        for (int i = 0; i < BLOCK_SIZE_WITHOUT_HEADER / 4; i += 4) {
            *p = NAN;
        }

        if (!g_isLoading) {
            g_isLoading = true;
            g_blockIndexToLoad = blockIndex;
            *((uint32_t *)&g_buffer[blockIndex * BLOCK_SIZE_WITH_HEADER]) = blockAddress;
            osMessagePut(g_scpiMessageQueueId, SCPI_QUEUE_MESSAGE(SCPI_QUEUE_MESSAGE_TARGET_NONE, SCPI_QUEUE_MESSAGE_DLOG_LOAD_BLOCK, 0), osWaitForever);
        }
    }

    float value = *(float *)(g_buffer + blockIndex * BLOCK_SIZE_WITH_HEADER + BLOCK_HEADER_SIZE + blockOffset);
    return eez::gui::data::Value(value, g_recording.dlogValues[columnIndex].offset.getUnit());
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
                uint16_t flags = readUint16(buffer, &offset);
                uint32_t columns = readUint32(buffer, &offset);
                float period = readFloat(buffer, &offset);
                float duration = readFloat(buffer, &offset);
                uint32_t startTime = readUint32(buffer, &offset);

                g_recording.parameters.period = period;
                g_recording.parameters.time = duration;

                g_recording.totalDlogValues = 0;
                g_recording.numVisibleDlogValues = 0;

                for (int iChannel = 0; iChannel < CH_NUM; ++iChannel) {
                    if (columns & (1 << (4 * iChannel))) {
                        ++g_recording.totalDlogValues;

                        g_recording.parameters.logVoltage[iChannel] = 1;

                        if (g_recording.numVisibleDlogValues < dlog_view::MAX_VISIBLE_DLOG_VALUES) {
                            g_recording.dlogValues[g_recording.numVisibleDlogValues].dlogValueType = (dlog_view::DlogValueType)(3 * g_recording.numVisibleDlogValues + dlog_view::DLOG_VALUE_CH1_U);

                            // TODO this must be read from the file
                            float perDiv = channel_dispatcher::getUMax(Channel::get(iChannel)) / dlog_view::NUM_VERT_DIVISIONS;

                            g_recording.dlogValues[g_recording.numVisibleDlogValues].perDiv = gui::data::Value(roundPrec(perDiv, 0.01f), UNIT_VOLT);
                            g_recording.dlogValues[g_recording.numVisibleDlogValues].offset = gui::data::Value(roundPrec(-perDiv * dlog_view::NUM_VERT_DIVISIONS / 2, 0.01f), UNIT_VOLT);

                            ++g_recording.numVisibleDlogValues;
                        }
                    }

                    if (columns & (2 << (4 * iChannel))) {
                        ++g_recording.totalDlogValues;

                        g_recording.parameters.logCurrent[iChannel] = 1;

                        if (g_recording.numVisibleDlogValues < dlog_view::MAX_VISIBLE_DLOG_VALUES) {
                            g_recording.dlogValues[g_recording.numVisibleDlogValues].dlogValueType = (dlog_view::DlogValueType)(3 * g_recording.numVisibleDlogValues + dlog_view::DLOG_VALUE_CH1_I);

                            // TODO this must be read from the file
                            float perDiv = channel_dispatcher::getIMax(Channel::get(iChannel)) / dlog_view::NUM_VERT_DIVISIONS;

                            g_recording.dlogValues[g_recording.numVisibleDlogValues].perDiv = gui::data::Value(roundPrec(perDiv, 0.01f), UNIT_AMPER);
                            g_recording.dlogValues[g_recording.numVisibleDlogValues].offset = gui::data::Value(roundPrec(-perDiv * dlog_view::NUM_VERT_DIVISIONS / 2, 0.01f), UNIT_AMPER);

                            ++g_recording.numVisibleDlogValues;
                        }
                    }

                    if (columns & (4 << (4 * iChannel))) {
                        ++g_recording.totalDlogValues;

                        g_recording.parameters.logPower[iChannel] = 1;

                        if (g_recording.numVisibleDlogValues < dlog_view::MAX_VISIBLE_DLOG_VALUES) {
                            g_recording.dlogValues[g_recording.numVisibleDlogValues].dlogValueType = (dlog_view::DlogValueType)(3 * g_recording.numVisibleDlogValues + dlog_view::DLOG_VALUE_CH1_P);

                            // TODO this should be stored inside DLOG file
                            float perDiv = channel_dispatcher::getPowerMaxLimit(Channel::get(iChannel)) / dlog_view::NUM_VERT_DIVISIONS;

                            g_recording.dlogValues[g_recording.numVisibleDlogValues].perDiv = gui::data::Value(roundPrec(perDiv, 0.01f), UNIT_WATT);
                            g_recording.dlogValues[g_recording.numVisibleDlogValues].offset = gui::data::Value(roundPrec(-perDiv * dlog_view::NUM_VERT_DIVISIONS / 2, 0.01f), UNIT_WATT);

                            ++g_recording.numVisibleDlogValues;
                        }
                    }
                }

                g_recording.size = (file.size() - DLOG_HEADER_SIZE) / (g_recording.totalDlogValues * 4);

                g_recording.timeOffset = gui::data::Value(0.0f, UNIT_SECOND);

                g_recording.pageSize = 480;
                g_recording.cursorOffset = 240;

                invalidateAllBlocks();

                g_recording.getValue = getValue;

                g_isLoading = false;

                g_state = STATE_READY;
            }
        }
    }

    file.close();

    if (g_state == STATE_LOADING) {
        g_state = STATE_ERROR;
    }
}

dlog_view::Recording &getRecording() {
    return g_showLatest && dlog_record::isExecuting() ? dlog_record::g_recording : g_recording;
}

} // namespace dlog_view
} // namespace psu
} // namespace eez

#endif // OPTION_SD_CARD
