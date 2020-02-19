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

#include <eez/modules/psu/psu.h>
#include <eez/libs/sd_fat/sd_fat.h>

#include <math.h>

#include <eez/index.h>
#include <eez/scpi/scpi.h>

#include <eez/modules/psu/psu.h>
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

#define CHUNK_SIZE 4096

enum Event {
    EVENT_INITIATE,
    EVENT_INITIATE_TRACE,
    EVENT_START,
    EVENT_TRIGGER,
    EVENT_TOGGLE,
    EVENT_FINISH,
    EVENT_ABORT,
    EVENT_RESET
};

dlog_view::Parameters g_parameters = {
    { 0 },
    { 0 },
    { },
    { },
    dlog_view::SCALE_LINEAR,
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
    { 0 },
    { },
    { },
    dlog_view::SCALE_LINEAR,
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

State g_state = STATE_IDLE;
bool g_inStateTransition;
int g_stateTransitionError;
bool g_traceInitiated;

static uint32_t g_lastSyncTickCount;
static uint32_t g_lastTickCount;
static uint32_t g_seconds;
static uint32_t g_micros;
static uint32_t g_iSample;
double g_currentTime;
static double g_nextTime;
uint32_t g_fileLength;
static unsigned int g_bufferIndex;
static unsigned int g_lastSavedBufferIndex;
static unsigned int g_saveUpToBufferIndex;

////////////////////////////////////////////////////////////////////////////////

static float getValue(uint32_t rowIndex, uint8_t columnIndex, float *max) {
    float value = *(float *)(DLOG_RECORD_BUFFER + (g_recording.dataOffset + (rowIndex * g_recording.parameters.numYAxes + columnIndex) * 4) % DLOG_RECORD_BUFFER_SIZE);

    if (g_recording.parameters.yAxisScale == dlog_view::SCALE_LOGARITHMIC) {
        float logOffset = 1 - g_recording.parameters.yAxes[columnIndex].range.min;
        value = log10f(logOffset + value);
    }

    *max = value;
    return value;
}

////////////////////////////////////////////////////////////////////////////////

static int fileTruncate() {
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
    if (g_state != STATE_EXECUTING) {
        return;
    }

    auto saveUpToBufferIndex = g_saveUpToBufferIndex;
    size_t length = saveUpToBufferIndex - g_lastSavedBufferIndex;
    if (length > 0) {
        File file;
        if (!file.open(g_recording.parameters.filePath, FILE_OPEN_APPEND | FILE_WRITE)) {
            event_queue::pushEvent(event_queue::EVENT_ERROR_DLOG_FILE_REOPEN_ERROR);
            abort();
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
            abort();
        }
    }
}

////////////////////////////////////////////////////////////////////////////////

static void flushData() {
    if (osThreadGetId() != g_scpiTaskHandle) {
        osMessagePut(g_scpiMessageQueueId, SCPI_QUEUE_MESSAGE(SCPI_QUEUE_MESSAGE_TARGET_NONE, SCPI_QUEUE_MESSAGE_TYPE_DLOG_FILE_WRITE, 0), osWaitForever);
    } else {
        fileWrite();
    }
}

////////////////////////////////////////////////////////////////////////////////

static void writeUint8(uint8_t value) {
    *(DLOG_RECORD_BUFFER + (g_bufferIndex % DLOG_RECORD_BUFFER_SIZE)) = value;

    g_bufferIndex++;

    if (g_state == STATE_EXECUTING && (g_bufferIndex - g_saveUpToBufferIndex) >= CHUNK_SIZE) {
        g_lastSyncTickCount = micros();
        g_saveUpToBufferIndex = g_bufferIndex;
        flushData();
    }

    ++g_fileLength;
}

static void writeUint16(uint16_t value) {
    writeUint8(value & 0xFF);
    writeUint8((value >> 8) & 0xFF);
}

static void writeUint32(uint32_t value) {
    writeUint8(value & 0xFF);
    writeUint8((value >> 8) & 0xFF);
    writeUint8((value >> 16) & 0xFF);
    writeUint8(value >> 24);
}

static void writeFloat(float value) {
    writeUint32(*((uint32_t *)&value));
}

static void writeUint8Field(uint8_t id, uint8_t value) {
    writeUint16(sizeof(uint16_t) + sizeof(uint8_t) + sizeof(uint8_t));
    writeUint8(id);
    writeUint8(value);
}

static void writeUint8FieldWithIndex(uint8_t id, uint8_t value, uint8_t index) {
    writeUint16(sizeof(uint16_t) + sizeof(uint8_t) + sizeof(uint8_t) + sizeof(uint8_t));
    writeUint8(id);
    writeUint8(index);
    writeUint8(value);
}

//static void writeUint16Field(uint8_t id, uint16_t value) {
//    writeUint16(sizeof(uint16_t) + sizeof(uint8_t) + sizeof(uint16_t));
//    writeUint8(id);
//    writeUint16(value);
//}

static void writeUint16FieldWithIndex(uint8_t id, uint16_t value, uint8_t index) {
    writeUint16(sizeof(uint16_t) + sizeof(uint8_t) + sizeof(uint8_t) + sizeof(uint16_t));
    writeUint8(id);
    writeUint8(index);
    writeUint16(value);
}

static void writeFloatField(uint8_t id, float value) {
    writeUint16(sizeof(uint16_t) + sizeof(uint8_t) + sizeof(float));
    writeUint8(id);
    writeFloat(value);
}

static void writeFloatFieldWithIndex(uint8_t id, float value, uint8_t index) {
    writeUint16(sizeof(uint16_t) + sizeof(uint8_t) + sizeof(uint8_t) + sizeof(float));
    writeUint8(id);
    writeUint8(index);
    writeFloat(value);
}

static void writeStringField(uint8_t id, const char *str) {
    writeUint16(sizeof(uint16_t) + sizeof(uint8_t) + (uint16_t)strlen(str));
    writeUint8(id);
    while (*str) {
        writeUint8(*str++);
    }
}

static void writeStringFieldWithIndex(uint8_t id, const char *str, uint8_t index) {
    writeUint16(sizeof(uint16_t) + sizeof(uint8_t) + sizeof(uint8_t) + (uint16_t)strlen(str));
    writeUint8(id);
    writeUint8(index);
    while (*str) {
        writeUint8(*str++);
    }
}

////////////////////////////////////////////////////////////////////////////////

static void initRecordingStart() {
    g_lastSyncTickCount = micros();
    g_lastTickCount = g_lastSyncTickCount;
    g_seconds = 0;
    g_micros = 0;
    g_iSample = 0;
    g_currentTime = 0;
    g_nextTime = 0;
    g_fileLength = 0;
    g_bufferIndex = 0;
    g_lastSavedBufferIndex = 0;
    g_saveUpToBufferIndex = 0;

    memcpy(&g_recording.parameters, &g_parameters, sizeof(dlog_view::Parameters));

    g_recording.size = 0;
    g_recording.pageSize = 480;

    g_recording.xAxisOffset = 0.0f;
    g_recording.xAxisDiv = g_recording.pageSize * g_recording.parameters.period / dlog_view::NUM_HORZ_DIVISIONS;

    if (!g_traceInitiated) {
        dlog_view::initAxis(g_recording);
    }

    dlog_view::initDlogValues(g_recording);

    g_recording.getValue = getValue;
}

static void writeFileHeaderAndMetaFields() {
    // header
    writeUint32(dlog_view::MAGIC1);
    writeUint32(dlog_view::MAGIC2);
    writeUint16(dlog_view::VERSION2);
    writeUint16(g_recording.parameters.numYAxes);
    uint32_t savedBufferIndex = g_bufferIndex;
    writeUint32(0);

    // meta fields
    writeStringField(dlog_view::FIELD_ID_COMMENT, g_recording.parameters.comment);

    writeUint8Field(dlog_view::FIELD_ID_X_UNIT, g_recording.parameters.xAxis.unit);
    writeFloatField(dlog_view::FIELD_ID_X_STEP, g_recording.parameters.xAxis.step);
    writeUint8Field(dlog_view::FIELD_ID_X_SCALE, g_recording.parameters.xAxis.scale);
    writeFloatField(dlog_view::FIELD_ID_X_RANGE_MIN, g_recording.parameters.xAxis.range.min);
    writeFloatField(dlog_view::FIELD_ID_X_RANGE_MAX, g_recording.parameters.xAxis.range.max);
    writeStringField(dlog_view::FIELD_ID_X_LABEL, g_recording.parameters.xAxis.label);

    bool writeChannelFields[CH_MAX];
    for (uint8_t channelIndex = 0; channelIndex < CH_MAX; channelIndex++) {
        writeChannelFields[channelIndex] = false;
    }

    if (g_recording.parameters.yAxis.unit != UNIT_UNKNOWN) {
        writeUint8FieldWithIndex(dlog_view::FIELD_ID_Y_UNIT, g_recording.parameters.yAxis.unit, 0);
        writeFloatFieldWithIndex(dlog_view::FIELD_ID_Y_RANGE_MIN, g_recording.parameters.yAxis.range.min, 0);
        writeFloatFieldWithIndex(dlog_view::FIELD_ID_Y_RANGE_MAX, g_recording.parameters.yAxis.range.max, 0);
        writeStringFieldWithIndex(dlog_view::FIELD_ID_Y_LABEL, g_recording.parameters.yAxis.label, 0);
        writeUint8FieldWithIndex(dlog_view::FIELD_ID_Y_CHANNEL_INDEX, 0, 0);
    }

    for (uint8_t yAxisIndex = 0; yAxisIndex < g_recording.parameters.numYAxes; yAxisIndex++) {
        if (g_recording.parameters.yAxis.unit == UNIT_UNKNOWN || g_recording.parameters.yAxes[yAxisIndex].unit != g_recording.parameters.yAxis.unit) {
            writeUint8FieldWithIndex(dlog_view::FIELD_ID_Y_UNIT, g_recording.parameters.yAxes[yAxisIndex].unit, yAxisIndex + 1);
        }

        if (g_recording.parameters.yAxis.unit == UNIT_UNKNOWN || g_recording.parameters.yAxes[yAxisIndex].range.min != g_recording.parameters.yAxis.range.min) {
            writeFloatFieldWithIndex(dlog_view::FIELD_ID_Y_RANGE_MIN, g_recording.parameters.yAxes[yAxisIndex].range.min, yAxisIndex + 1);
        }

        if (g_recording.parameters.yAxis.unit == UNIT_UNKNOWN || g_recording.parameters.yAxes[yAxisIndex].range.max != g_recording.parameters.yAxis.range.max) {
            writeFloatFieldWithIndex(dlog_view::FIELD_ID_Y_RANGE_MAX, g_recording.parameters.yAxes[yAxisIndex].range.max, yAxisIndex + 1);
        }

        if (g_recording.parameters.yAxis.unit == UNIT_UNKNOWN || strcmp(g_recording.parameters.yAxes[yAxisIndex].label, g_recording.parameters.yAxis.label) != 0) {
            writeStringFieldWithIndex(dlog_view::FIELD_ID_Y_LABEL, g_recording.parameters.yAxes[yAxisIndex].label, yAxisIndex + 1);
        }

        if (g_recording.parameters.yAxis.unit == UNIT_UNKNOWN || g_recording.parameters.yAxes[yAxisIndex].channelIndex >= 0) {
            writeUint8FieldWithIndex(dlog_view::FIELD_ID_Y_CHANNEL_INDEX, g_recording.parameters.yAxes[yAxisIndex].channelIndex + 1, yAxisIndex + 1);
            if (g_recording.parameters.yAxes[yAxisIndex].channelIndex >= 0) {
                writeChannelFields[g_recording.parameters.yAxes[yAxisIndex].channelIndex] = true;
            }
        }
    }

    writeUint8Field(dlog_view::FIELD_ID_Y_SCALE, g_recording.parameters.yAxisScale);

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
}

////////////////////////////////////////////////////////////////////////////////

static void setState(State newState) {
    if (g_state != newState) {
        if (newState == STATE_EXECUTING) {
            setOperBits(OPER_DLOG, true);
        } else if (g_state == STATE_EXECUTING) {
            setOperBits(OPER_DLOG, false);
        }

        g_state = newState;
    }
}

static void log(uint32_t tickCount) {
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
            stateTransition(EVENT_FINISH);
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

static int doStartImmediately() {
    int err;

    err = checkDlogParameters(g_parameters, false, g_traceInitiated);
    if (err != SCPI_RES_OK) {
        return err;
    }

    err = fileTruncate();
    if (err != SCPI_RES_OK) {
        return err;
    }

    initRecordingStart();

    writeFileHeaderAndMetaFields();

    setState(STATE_EXECUTING);

    return SCPI_RES_OK;
}

static int doInitiate(bool traceInitiated) {
    int err;

    g_traceInitiated = traceInitiated;

    if (g_parameters.triggerSource == trigger::SOURCE_IMMEDIATE) {
        err = doStartImmediately();
    } else {
        err = checkDlogParameters(g_parameters, false, g_traceInitiated);
        if (err == SCPI_RES_OK) {
            setState(STATE_INITIATED);
        }
    }

    if (err != SCPI_RES_OK) {
        g_parameters.filePath[0] = 0;
        g_traceInitiated = false;
    }

    return err;
}

static void resetParameters() {
    memset(&g_parameters, 0, sizeof(g_parameters));
    g_parameters.period = PERIOD_DEFAULT;
    g_parameters.time = TIME_DEFAULT;
    g_parameters.triggerSource = trigger::SOURCE_IMMEDIATE;
}

static void doFinish() {
    g_saveUpToBufferIndex = g_bufferIndex;
    flushData();
    onSdCardFileChangeHook(g_parameters.filePath);
    resetParameters();
    setState(STATE_IDLE);
}

////////////////////////////////////////////////////////////////////////////////

int checkDlogParameters(dlog_view::Parameters &parameters, bool doNotCheckFilePath, bool forTraceUsage) {
    if (forTraceUsage) {
        if (parameters.xAxis.step <= 0) {
            // TODO replace with more specific error
            return SCPI_ERROR_EXECUTION_ERROR;
        }

        if (parameters.xAxis.range.min >= parameters.xAxis.range.max) {
            return SCPI_ERROR_DATA_OUT_OF_RANGE;
        }

        if (parameters.numYAxes == 0) {
            // TODO replace with more specific error
            return SCPI_ERROR_EXECUTION_ERROR;
        }

        for (int i = 0; i < parameters.numYAxes; i++) {
            if (parameters.yAxes[i].range.min >= parameters.yAxes[i].range.max) {
                return SCPI_ERROR_DATA_OUT_OF_RANGE;
            }
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
    }

    if (!doNotCheckFilePath) {
        if (!parameters.filePath[0]) {
            // TODO replace with more specific error
            return SCPI_ERROR_EXECUTION_ERROR;
        }
    }

    return SCPI_RES_OK;
}

////////////////////////////////////////////////////////////////////////////////

void stateTransition(int event, int* perr) {
    g_inStateTransition = true;

    if (osThreadGetId() != g_scpiTaskHandle) {
        osMessagePut(g_scpiMessageQueueId, SCPI_QUEUE_MESSAGE(SCPI_QUEUE_MESSAGE_TARGET_NONE, SCPI_QUEUE_MESSAGE_TYPE_DLOG_STATE_TRANSITION, event), osWaitForever);
        if (perr) {
            *perr = SCPI_RES_OK;
        }
        return;
    }

    int err = SCPI_ERROR_CANNOT_CHANGE_TRANSIENT_TRIGGER;

    if (g_state == STATE_IDLE) {
        if (event == EVENT_INITIATE || event == EVENT_INITIATE_TRACE) {
            err = doInitiate(event == EVENT_INITIATE_TRACE);
        } else if (event == EVENT_START) {
            err = doStartImmediately();
        } else if (event == EVENT_TOGGLE) {
            err = doInitiate(false);
        } else if (event == EVENT_ABORT || event == EVENT_RESET) {
            resetParameters();
            err = SCPI_RES_OK;
        }
    } else if (g_state == STATE_INITIATED) {
        if (event == EVENT_START || event == EVENT_TRIGGER || event == EVENT_TOGGLE) {
            err = doStartImmediately();
        } else if (event == EVENT_ABORT || event == EVENT_RESET) {
            resetParameters();
            setState(STATE_IDLE);
            err = SCPI_RES_OK;
        }
    } else if (g_state == STATE_EXECUTING) {
        if (event == EVENT_TOGGLE || event == EVENT_FINISH || event == EVENT_ABORT || event == EVENT_RESET) {
            doFinish();
            err = SCPI_RES_OK;
        }
    }

    g_stateTransitionError = err;

    if (perr) {
        *perr = err;
    } else {
        if (err != SCPI_RES_OK) {
            generateError(err);
        }
    }

    g_inStateTransition = false;
}

////////////////////////////////////////////////////////////////////////////////

int initiate() {
    int err;
    stateTransition(EVENT_INITIATE, &err);
    return err;
}

int initiateTrace() {
    stateTransition(EVENT_INITIATE_TRACE, nullptr);
    while (g_inStateTransition) {
        osDelay(1);
    }
    return g_stateTransitionError;
}

int startImmediately() {
    int err;
    stateTransition(EVENT_START, &err);
    return err;
}

void triggerGenerated() {
    stateTransition(EVENT_TRIGGER);
}

void toggle() {
    stateTransition(EVENT_TOGGLE);
}

void abort() {
    stateTransition(EVENT_ABORT);
}

void reset() {
    stateTransition(EVENT_RESET);
}

////////////////////////////////////////////////////////////////////////////////

void tick(uint32_t tickCount) {
    if (g_state == STATE_EXECUTING && g_nextTime <= g_recording.parameters.time) {
        log(tickCount);
    }
}

void log(float *values) {
    if (g_state == STATE_EXECUTING) {
        for (int yAxisIndex = 0; yAxisIndex < dlog_record::g_recording.parameters.numYAxes; yAxisIndex++) {
            writeFloat(values[yAxisIndex]);
        }
        ++g_recording.size;
    }
}

////////////////////////////////////////////////////////////////////////////////

const char *getLatestFilePath() {
    return g_recording.parameters.filePath[0] != 0 ? g_recording.parameters.filePath : nullptr;
}

} // namespace dlog_record
} // namespace psu
} // namespace eez
