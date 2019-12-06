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
#include <eez/modules/psu/psu.h>
#include <eez/libs/sd_fat/sd_fat.h>

#include <math.h>

#include <eez/index.h>
#include <eez/scpi/scpi.h>

#include <eez/modules/psu/channel_dispatcher.h>
#include <eez/modules/psu/datetime.h>
#include <eez/modules/psu/scpi/psu.h>
#include <eez/modules/psu/sd_card.h>
#include <eez/system.h>
#include <eez/modules/psu/dlog_record.h>
#include <eez/modules/psu/event_queue.h>
#include <eez/gui/widgets/yt_graph.h>

#include <eez/memory.h>

namespace eez {

using namespace scpi;

namespace psu {
namespace dlog_record {

dlog_view::Parameters g_parameters = {
    { 0 },
    { },
    0,
    {{}, {}, {}, {}, {}, {}},
    {false, false, false, false, false, false},
    {false, false, false, false, false, false},
    {false, false, false, false, false, false},
    PERIOD_DEFAULT,
    TIME_DEFAULT,
    trigger::SOURCE_IMMEDIATE
};

dlog_view::Parameters g_guiParameters = {
    { 0 },
    { },
    0,
    {{}, {}, {}, {}, {}, {}},
    {true, false, false, false, false, false},
    {true, false, false, false, false, false},
    {false, false, false, false, false, false},
    PERIOD_DEFAULT,
    TIME_DEFAULT,
    trigger::SOURCE_IMMEDIATE
};

trigger::Source g_triggerSource = trigger::SOURCE_IMMEDIATE;

dlog_view::Recording g_recording; 

static State g_state = STATE_IDLE;
static bool g_traceInitiated;

static uint32_t g_lastTickCount;
static uint32_t g_seconds;
static uint32_t g_micros;
static uint32_t g_iSample;
double g_currentTime;
static double g_nextTime;
static uint32_t g_lastSyncTickCount;

uint32_t g_fileLength;

#define CHUNK_SIZE 4096

static unsigned int g_bufferIndex;

static unsigned int g_lastSavedBufferIndex;
static unsigned int g_saveUpToBufferIndex;

int fileOpen() {
    File file;
    if (!file.open(g_parameters.filePath, FILE_OPEN_APPEND | FILE_WRITE)) {
        event_queue::pushEvent(event_queue::EVENT_ERROR_DLOG_FILE_OPEN_ERROR);
        // TODO replace with more specific error
        return SCPI_ERROR_MASS_STORAGE_ERROR;
    }

    bool result = file.truncate(0);
    file.close();
    if (!result) {
        event_queue::pushEvent(event_queue::EVENT_ERROR_DLOG_TRUNCATE_ERROR);
        // TODO replace with more specific error
        return SCPI_ERROR_MASS_STORAGE_ERROR;
    }

    return SCPI_RES_OK;
}

void fileWrite() {
    auto saveUpToBufferIndex = g_saveUpToBufferIndex;
    size_t length = saveUpToBufferIndex - g_lastSavedBufferIndex;
    if (length > 0) {
        File file;
        if (!file.open(g_recording.parameters.filePath, FILE_OPEN_APPEND | FILE_WRITE)) {
            event_queue::pushEvent(event_queue::EVENT_ERROR_DLOG_FILE_REOPEN_ERROR);
            abort(false);
            return;
        }

        int i = g_lastSavedBufferIndex % DLOG_RECORD_BUFFER_SIZE;
        int j = saveUpToBufferIndex % DLOG_RECORD_BUFFER_SIZE;

        size_t written;
        if (i < j || j == 0) {
            written = file.write(DLOG_RECORD_BUFFER + i, length);
        } else {
            written = file.write(DLOG_RECORD_BUFFER + i, DLOG_RECORD_BUFFER_SIZE - i) + file.write(DLOG_RECORD_BUFFER, j);
        }

        g_lastSavedBufferIndex = saveUpToBufferIndex;

        file.close();

        if (written != length) {
            event_queue::pushEvent(event_queue::EVENT_ERROR_DLOG_WRITE_ERROR);
            abort(false);
        }
    }
}

void flushData() {
    if (osThreadGetId() != g_scpiTaskHandle) {
        osMessagePut(g_scpiMessageQueueId, SCPI_QUEUE_MESSAGE(SCPI_QUEUE_MESSAGE_TARGET_NONE, SCPI_QUEUE_MESSAGE_DLOG_FILE_WRITE, 0), osWaitForever);
    } else {
        fileWrite();
    }
}

void writeUint8(uint8_t value) {
    *(DLOG_RECORD_BUFFER + (g_bufferIndex % DLOG_RECORD_BUFFER_SIZE)) = value;

    g_bufferIndex++;

    if (g_state == STATE_EXECUTING && (g_bufferIndex - g_saveUpToBufferIndex) >= CHUNK_SIZE) {
        g_lastSyncTickCount = micros();
        g_saveUpToBufferIndex = g_bufferIndex;
        flushData();
    }

    ++g_fileLength;
}

void writeUint16(uint16_t value) {
    writeUint8(value & 0xFF);
    writeUint8((value >> 8) & 0xFF);
}

void writeUint32(uint32_t value) {
    writeUint8(value & 0xFF);
    writeUint8((value >> 8) & 0xFF);
    writeUint8((value >> 16) & 0xFF);
    writeUint8(value >> 24);
}

void writeFloat(float value) {
    writeUint32(*((uint32_t *)&value));
}

void writeUint8Field(uint8_t id, uint8_t value) {
    writeUint16(sizeof(uint16_t) + sizeof(uint8_t) + sizeof(uint8_t));
    writeUint8(id);
    writeUint8(value);
}

void writeUint8FieldWithIndex(uint8_t id, uint8_t value, uint8_t index) {
    writeUint16(sizeof(uint16_t) + sizeof(uint8_t) + sizeof(uint8_t) + sizeof(uint8_t));
    writeUint8(id);
    writeUint8(index);
    writeUint8(value);
}

void writeUint16Field(uint8_t id, uint16_t value) {
    writeUint16(sizeof(uint16_t) + sizeof(uint8_t) + sizeof(uint16_t));
    writeUint8(id);
    writeUint16(value);
}

void writeUint16FieldWithIndex(uint8_t id, uint16_t value, uint8_t index) {
    writeUint16(sizeof(uint16_t) + sizeof(uint8_t) + sizeof(uint8_t) + sizeof(uint16_t));
    writeUint8(id);
    writeUint8(index);
    writeUint16(value);
}

void writeFloatField(uint8_t id, float value) {
    writeUint16(sizeof(uint16_t) + sizeof(uint8_t) + sizeof(float));
    writeUint8(id);
    writeFloat(value);
}

void writeFloatFieldWithIndex(uint8_t id, float value, uint8_t index) {
    writeUint16(sizeof(uint16_t) + sizeof(uint8_t) + sizeof(uint8_t) + sizeof(float));
    writeUint8(id);
    writeUint8(index);
    writeFloat(value);
}

void writeStringField(uint8_t id, const char *str) {
    writeUint16(sizeof(uint16_t) + sizeof(uint8_t) + (uint16_t)strlen(str));
    writeUint8(id);
    while (*str) {
        writeUint8(*str++);
    }
}

void writeStringFieldWithIndex(uint8_t id, const char *str, uint8_t index) {
    writeUint16(sizeof(uint16_t) + sizeof(uint8_t) + sizeof(uint8_t) + (uint16_t)strlen(str));
    writeUint8(id);
    writeUint8(index);
    while (*str) {
        writeUint8(*str++);
    }
}

void setState(State newState) {
    if (g_state != newState) {
        if (newState == STATE_EXECUTING) {
            setOperBits(OPER_DLOG, true);
        } else if (g_state == STATE_EXECUTING) {
            setOperBits(OPER_DLOG, false);
        }

        g_state = newState;
    }
}

State getState() {
    return g_state;
}

int checkDlogParameters(dlog_view::Parameters &parameters, bool doNotCheckFilePath) {
    if (g_traceInitiated) {
        if (parameters.xAxis.step <= 0) {
            // TODO replace with more specific error
            return SCPI_ERROR_EXECUTION_ERROR;
        }

        if (parameters.numYAxes == 0) {
            // TODO replace with more specific error
            return SCPI_ERROR_EXECUTION_ERROR;
        }
    } else {
        bool somethingToLog = false;
        for (int i = 0; i < CH_NUM; ++i) {
            if (parameters.logVoltage[i] || parameters.logCurrent[i] || parameters.logPower[i]) {
                somethingToLog = true;
                break;
            }
        }

        if (!somethingToLog) {
            // TODO replace with more specific error
            return SCPI_ERROR_EXECUTION_ERROR;
        }

        if (!doNotCheckFilePath) {
            if (!parameters.filePath[0]) {
                // TODO replace with more specific error
                return SCPI_ERROR_EXECUTION_ERROR;
            }
        }
    }

    return SCPI_RES_OK;
}

bool isIdle() {
    return g_state == STATE_IDLE;
}

bool isInitiated() {
    return g_state == STATE_INITIATED;
}

bool isExecuting() {
    return g_state == STATE_EXECUTING;
}

static int doInitiate() {
    int error = SCPI_RES_OK;

    if (g_parameters.triggerSource == trigger::SOURCE_IMMEDIATE) {
        error = startImmediately();
    } else {
        error = checkDlogParameters(g_parameters);
        if (error == SCPI_RES_OK) {
            setState(STATE_INITIATED);
        }
    }

    if (error != SCPI_RES_OK) {
        g_parameters.filePath[0] = 0;
    }

    return error;
}

int initiate() {
    g_traceInitiated = false;
    return doInitiate();
}

int initiateTrace() {
    g_traceInitiated = true;
    return doInitiate();
}

float getValue(int rowIndex, int columnIndex, float *max) {
    float value = *(float *)(DLOG_RECORD_BUFFER + (g_recording.dataOffset + (rowIndex * g_recording.parameters.numYAxes + columnIndex) * 4) % DLOG_RECORD_BUFFER_SIZE);
    *max = value;
    return value;
}

void log(uint32_t tickCount);

int startImmediately() {
    int err;

    err = checkDlogParameters(g_parameters);
    if (err != SCPI_RES_OK) {
        return err;
    }

    err = fileOpen();
    if (err != SCPI_RES_OK) {
        return err;
    }

    g_bufferIndex = 0;
    g_lastSavedBufferIndex = 0;
    g_saveUpToBufferIndex = 0;
    g_lastSyncTickCount = micros();
    g_fileLength = 0;
    g_lastTickCount = micros();
    g_seconds = 0;
    g_micros = 0;
    g_iSample = 0;
    g_currentTime = 0;
    g_nextTime = 0;

    memcpy(&g_recording.parameters, &g_parameters, sizeof(dlog_view::Parameters));

    g_recording.size = 0;
    g_recording.pageSize = 480;

    g_recording.timeOffset = 0.0f;
    g_recording.timeDiv = g_recording.pageSize * g_recording.parameters.period / dlog_view::NUM_HORZ_DIVISIONS;

    if (!g_traceInitiated) {
        dlog_view::initAxis(g_recording);
    }

    dlog_view::initDlogValues(g_recording);

    g_recording.getValue = getValue;

    // header
    writeUint32(dlog_view::MAGIC1);
    writeUint32(dlog_view::MAGIC2);
    writeUint16(dlog_view::VERSION2);
    writeUint16(g_recording.parameters.numYAxes);
    uint32_t savedBufferIndex = g_bufferIndex;
    writeUint32(0);

    // meta fields
    writeUint8Field(dlog_view::FIELD_ID_X_UNIT, g_recording.parameters.xAxis.unit);
    writeFloatField(dlog_view::FIELD_ID_X_STEP, g_recording.parameters.xAxis.step);
    writeFloatField(dlog_view::FIELD_ID_X_RANGE_MIN, g_recording.parameters.xAxis.range.min);
    writeFloatField(dlog_view::FIELD_ID_X_RANGE_MAX, g_recording.parameters.xAxis.range.max);
    writeStringField(dlog_view::FIELD_ID_X_LABEL, g_recording.parameters.xAxis.label);

    bool writeChannelFields[CH_MAX];
    for (uint8_t channelIndex = 0; channelIndex < CH_MAX; channelIndex++) {
        writeChannelFields[channelIndex] = false;
    }

    for (uint8_t yAxisIndex = 0; yAxisIndex < g_recording.parameters.numYAxes; yAxisIndex++) {
        writeUint8FieldWithIndex(dlog_view::FIELD_ID_Y_UNIT, g_recording.parameters.yAxes[yAxisIndex].unit, yAxisIndex + 1);
        writeFloatFieldWithIndex(dlog_view::FIELD_ID_Y_RANGE_MIN, g_recording.parameters.yAxes[yAxisIndex].range.min, yAxisIndex + 1);
        writeFloatFieldWithIndex(dlog_view::FIELD_ID_Y_RANGE_MAX, g_recording.parameters.yAxes[yAxisIndex].range.max, yAxisIndex + 1);
        writeStringFieldWithIndex(dlog_view::FIELD_ID_Y_LABEL, g_recording.parameters.yAxes[yAxisIndex].label, yAxisIndex + 1);
        writeUint8FieldWithIndex(dlog_view::FIELD_ID_Y_CHANNEL_INDEX, g_recording.parameters.yAxes[yAxisIndex].channelIndex + 1, yAxisIndex + 1);
        
        writeChannelFields[g_recording.parameters.yAxes[yAxisIndex].channelIndex] = true;
    }

    for (uint8_t channelIndex = 0; channelIndex < CH_MAX; channelIndex++) {
        if (writeChannelFields[channelIndex]) {
            Channel &channel = Channel::get(channelIndex);
            writeUint16FieldWithIndex(dlog_view::FIELD_ID_CHANNEL_MODULE_TYPE, g_slots[channel.slotIndex].moduleInfo->moduleType, channelIndex + 1);
            writeUint16FieldWithIndex(dlog_view::FIELD_ID_CHANNEL_MODULE_REVISION, g_slots[channel.slotIndex].moduleRevision, channelIndex + 1);
        }
    }

    writeUint16(0); // end of meta fields section

    // write beginning of data offset
    g_recording.dataOffset = 4 * ((g_bufferIndex + 3) / 4);
    g_bufferIndex = savedBufferIndex;
    writeUint32(g_recording.dataOffset);
    g_bufferIndex = g_recording.dataOffset;

    setState(STATE_EXECUTING);

    log(g_lastTickCount);

    return SCPI_RES_OK;
}

void triggerGenerated() {
    int err = startImmediately();
    if (err != SCPI_RES_OK) {
        generateError(err);
    }
}

void toggle() {
    if (osThreadGetId() != g_scpiTaskHandle) {
        osMessagePut(g_scpiMessageQueueId, SCPI_QUEUE_MESSAGE(SCPI_QUEUE_MESSAGE_TARGET_NONE, SCPI_QUEUE_MESSAGE_DLOG_TOGGLE, 0), osWaitForever);
        return;
    }

    if (isExecuting()) {
        abort();
    } else if (isInitiated()) {
        triggerGenerated();
    } else {
        initiate();
    }
}

void resetParameters() {
    memset(&g_parameters.xAxis, 0, sizeof(dlog_view::XAxis));
    g_parameters.numYAxes = 0;
    memset(&g_parameters.yAxes[0], 0, dlog_view::MAX_NUM_OF_Y_AXES * sizeof(dlog_view::YAxis));

    for (int i = 0; i < CH_NUM; ++i) {
        g_parameters.logVoltage[i] = 0;
        g_parameters.logCurrent[i] = 0;
        g_parameters.logPower[i] = 0;
    }

    g_parameters.period = PERIOD_DEFAULT;
    g_parameters.time = TIME_DEFAULT;
    g_parameters.triggerSource = trigger::SOURCE_IMMEDIATE;
    g_parameters.filePath[0] = 0;
}

void finishLogging(bool flush) {
	setState(STATE_IDLE);

	if (flush) {
        g_saveUpToBufferIndex = g_bufferIndex;
        flushData();
	}

    reset();
}

void abort(bool flush) {
    if (g_state == STATE_EXECUTING) {
        finishLogging(flush);
    } else {
        setState(STATE_IDLE);
    }
}

void log(uint32_t tickCount) {
    g_micros += tickCount - g_lastTickCount;
    g_lastTickCount = tickCount;

    if (g_micros >= 1000000) {
        ++g_seconds;
        g_micros -= 1000000;
    }

    g_currentTime = g_seconds + g_micros * 1E-6;

    if (g_traceInitiated) {
        return;
    }

    if (g_currentTime >= g_nextTime) {
        while (1) {
            g_nextTime = ++g_iSample * g_recording.parameters.period;
            if (g_currentTime < g_nextTime || g_nextTime > g_recording.parameters.time) {
                break;
            }

#if defined(EEZ_PLATFORM_SIMULATOR)
            for (int i = 0; i < CH_NUM; ++i) {
                Channel &channel = Channel::get(i);

                float uMon = 0;
                float iMon = 0;

                if (g_recording.parameters.logVoltage[i]) {
                    uMon = channel_dispatcher::getUMonLast(channel);
                    writeFloat(uMon);
                }

                if (g_recording.parameters.logCurrent[i]) {
                    iMon = channel_dispatcher::getIMonLast(channel);
                    writeFloat(iMon);
                }

                if (g_recording.parameters.logPower[i]) {
                    if (!g_recording.parameters.logVoltage[i]) {
                        uMon = channel_dispatcher::getUMonLast(channel);
                    }
                    if (!g_recording.parameters.logCurrent[i]) {
                        iMon = channel_dispatcher::getIMonLast(channel);
                    }
                    writeFloat(uMon * iMon);
                }
            }

            ++g_recording.size;
#else
            // we missed a sample, write NAN's
            for (int i = 0; i < CH_NUM; ++i) {
                if (g_recording.parameters.logVoltage[i]) {
                    writeFloat(NAN);
                }
                if (g_recording.parameters.logCurrent[i]) {
                    writeFloat(NAN);
                }
                if (g_recording.parameters.logPower[i]) {
                    writeFloat(NAN);
                }
            }

            ++g_recording.size;
#endif
        }

        // write sample
        for (int i = 0; i < CH_NUM; ++i) {
            Channel &channel = Channel::get(i);

            float uMon = 0;
            float iMon = 0;

            if (g_recording.parameters.logVoltage[i]) {
                uMon = channel_dispatcher::getUMonLast(channel);
                writeFloat(uMon);
            }

            if (g_recording.parameters.logCurrent[i]) {
                iMon = channel_dispatcher::getIMonLast(channel);
                writeFloat(iMon);
            }

            if (g_recording.parameters.logPower[i]) {
                if (!g_recording.parameters.logVoltage[i]) {
                    uMon = channel_dispatcher::getUMonLast(channel);
                }
                if (!g_recording.parameters.logCurrent[i]) {
                    iMon = channel_dispatcher::getIMonLast(channel);
                }
                writeFloat(uMon * iMon);
            }
        }
        
        ++g_recording.size;

        if (g_nextTime > g_recording.parameters.time) {
            finishLogging(true);
        } else {
            int32_t diff = tickCount - g_lastSyncTickCount;
            if (diff > CONF_DLOG_SYNC_FILE_TIME * 1000000L) {
                g_lastSyncTickCount = tickCount;
                g_saveUpToBufferIndex = g_bufferIndex;
                flushData();
            }
        }
    }
}

void log(float *values) {
    for (int yAxisIndex = 0; yAxisIndex < dlog_record::g_recording.parameters.numYAxes; yAxisIndex++) {
        writeFloat(values[yAxisIndex]);
    }
    ++g_recording.size;
}

void tick(uint32_t tickCount) {
    if (g_state == STATE_EXECUTING) {
        log(tickCount);
    }
}

void reset() {
    abort(false);
    resetParameters();
}

const char *getLatestFilePath() {
    return g_recording.parameters.filePath[0] != 0 ? g_recording.parameters.filePath : nullptr;
}

} // namespace dlog_record
} // namespace psu
} // namespace eez

#endif // OPTION_SD_CARD
