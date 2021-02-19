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

#include <math.h>

#include <eez/index.h>
#include <eez/sound.h>
#include <eez/system.h>

#include <eez/modules/psu/psu.h>
#include <eez/modules/psu/channel_dispatcher.h>
#include <eez/modules/psu/datetime.h>
#include <eez/modules/psu/sd_card.h>
#include <eez/modules/psu/io_pins.h>
#include <eez/modules/psu/dlog_record.h>
#include <eez/modules/psu/event_queue.h>
#include <eez/modules/psu/gui/psu.h>

#include <eez/modules/psu/scpi/psu.h>

#include <eez/gui/widgets/yt_graph.h>

#include <eez/libs/sd_fat/sd_fat.h>

#include <eez/memory.h>

volatile extern uint32_t g_debugVarBufferDiff;

namespace eez {

using namespace scpi;

namespace psu {
namespace dlog_record {

#define CHUNK_SIZE (64 * 1024)

#define CONF_DLOG_SYNC_FILE_TIME_MS 10000

#define CONF_WRITE_TIMEOUT_MS 1000
#define CONF_WRITE_FLUSH_TIMEOUT_MS 10000

enum Event {
    EVENT_INITIATE,
    EVENT_INITIATE_TRACE,
    EVENT_START,
    EVENT_TRIGGER,
    EVENT_TOGGLE_START,
    EVENT_TOGGLE_STOP,
    EVENT_FINISH,
    EVENT_ABORT,
    EVENT_ABORT_AFTER_BUFFER_OVERFLOW_ERROR,
    EVENT_ABORT_AFTER_MASS_STORAGE_ERROR,
    EVENT_RESET
};

dlog_view::Parameters g_parameters;

trigger::Source g_triggerSource = trigger::SOURCE_IMMEDIATE;

dlog_view::Recording g_recording; 

State g_state = STATE_IDLE;
bool g_inStateTransition;
int g_stateTransitionError;

// This variable is true if data logging is initiated
// with the SCPI command `INITiate:DLOG:TRACe`.
bool g_traceInitiated;

static uint32_t g_countingStarted;
static double g_currentTime;
static double g_nextTime;

static unsigned int g_lastSavedBufferIndex;
static uint32_t g_lastSavedBufferTickCount;

static dlog_file::Writer g_writer(DLOG_RECORD_BUFFER, DLOG_RECORD_BUFFER_SIZE);

static uint32_t g_fileLength;
static uint32_t g_numSamples;

////////////////////////////////////////////////////////////////////////////////

static float getValue(uint32_t rowIndex, uint8_t columnIndex, float *max) {
	auto p = DLOG_RECORD_BUFFER + (
			g_recording.dataOffset + (
				rowIndex * g_recording.numFloatsPerRow
				+ g_recording.columnFloatIndexes[columnIndex]
			) * 4
		) % DLOG_RECORD_BUFFER_SIZE;

    float value;

    if (g_recording.parameters.yAxes[columnIndex].unit == UNIT_BIT) {
        auto intValue = *(uint32_t*)p;
        if (intValue & 0x8000) {
            if (intValue & (0x4000 >> g_recording.parameters.yAxes[columnIndex].channelIndex)) {
                value = 1.0f;
            } else {
                value = 0.0f;
            }
        } else {
            value = NAN;
        }
	} else {
		value = *(float *)p;
	}

    if (g_recording.parameters.yAxisScale == dlog_file::SCALE_LOGARITHMIC) {
        float logOffset = 1 - g_recording.parameters.yAxes[columnIndex].range.min;
        value = log10f(logOffset + value);
    }

    *max = value;
    return value;
}

////////////////////////////////////////////////////////////////////////////////

static File file;

static int fileTruncate() {
    if (!file.open(g_parameters.filePath, FILE_OPEN_APPEND | FILE_WRITE)) {
        event_queue::pushEvent(event_queue::EVENT_ERROR_DLOG_FILE_OPEN_ERROR);
        // TODO replace with more specific error
        return SCPI_ERROR_MASS_STORAGE_ERROR;
    }

    bool result = file.truncate(0);
    // file.close();
    if (!result) {
        event_queue::pushEvent(event_queue::EVENT_ERROR_DLOG_TRUNCATE_ERROR);
        // TODO replace with more specific error
        return SCPI_ERROR_MASS_STORAGE_ERROR;
    }

    return SCPI_RES_OK;
}

bool getNextWriteBuffer(const uint8_t *&buffer, uint32_t &bufferSize, bool flush) {
    static uint8_t g_saveBuffer[CHUNK_SIZE];

    buffer = nullptr;
    bufferSize = 0;

// #if defined(EEZ_PLATFORM_STM32)
//     taskENTER_CRITICAL();
// #endif
    int32_t timeDiff = millis() - g_lastSavedBufferTickCount;
    uint32_t alignedBufferIndex = (g_writer.getBufferIndex() / 4) * 4;
    uint32_t indexDiff = alignedBufferIndex - g_lastSavedBufferIndex;
    g_debugVarBufferDiff = indexDiff;
    if (indexDiff > 0 && (flush || timeDiff >= CONF_DLOG_SYNC_FILE_TIME_MS || indexDiff >= CHUNK_SIZE)) {
        bufferSize = MIN(indexDiff, CHUNK_SIZE);
        buffer = g_saveBuffer;

        int32_t i = g_writer.getBufferIndex() - (g_lastSavedBufferIndex + DLOG_RECORD_BUFFER_SIZE);

        if (i <= 0) {
            i = 0;
        } else {
// #if defined(EEZ_PLATFORM_STM32)
//             taskEXIT_CRITICAL();
// #endif
            abortAfterBufferOverflowError();
            return false;
        }

        if ((uint32_t)i < bufferSize) {
            uint32_t tail = (g_lastSavedBufferIndex + i) % DLOG_RECORD_BUFFER_SIZE;
            uint32_t head = (g_lastSavedBufferIndex + bufferSize) % DLOG_RECORD_BUFFER_SIZE;
            if (tail < head) {
                memcpy(g_saveBuffer + i, DLOG_RECORD_BUFFER + tail, head - tail);
            } else {
                uint32_t n = DLOG_RECORD_BUFFER_SIZE - tail;
                memcpy(g_saveBuffer, DLOG_RECORD_BUFFER + tail, n);
                if (head > 0) {
                    memcpy(g_saveBuffer + n, DLOG_RECORD_BUFFER, head);
                }
            }
        } 
    }
// #if defined(EEZ_PLATFORM_STM32)
//     taskEXIT_CRITICAL();
// #endif

    return true;
}

// returns true if there is more data to write
void fileWrite(bool flush) {
    if (!flush && g_state != STATE_EXECUTING) {
        return;
    }

    if (isModuleLocalRecording()) {
        return;
    }

    uint32_t timeout = millis() + CONF_WRITE_TIMEOUT_MS;
    while (millis() < timeout) {
        const uint8_t *buffer = nullptr;
        uint32_t bufferSize = 0;

        if (!getNextWriteBuffer(buffer, bufferSize, flush)) {
        	return;
        }

        if (!buffer) {
            return;
        }

        int err = 0;

        // File file;
        // if (file.open(g_recording.parameters.filePath, FILE_OPEN_APPEND | FILE_WRITE)) {
        //     if (file.seek(g_lastSavedBufferIndex)) {
                size_t written = file.write(buffer, bufferSize);

                if (written != bufferSize) {
                    err = event_queue::EVENT_ERROR_DLOG_WRITE_ERROR;
                }

                // if (!file.close()) {
                //     err = event_queue::EVENT_ERROR_DLOG_WRITE_ERROR;
                // }

                if (!err) {
                    g_lastSavedBufferIndex += bufferSize;
                    g_lastSavedBufferTickCount = millis();
                }
        //     } else {
        //         err = event_queue::EVENT_ERROR_DLOG_SEEK_ERROR;
        //     }
        // } else {
        //     err = event_queue::EVENT_ERROR_DLOG_FILE_REOPEN_ERROR;
        // }

        if (err) {
            //DebugTrace("write error\n");
            sd_card::reinitialize();
            return;
        }
    }
}

////////////////////////////////////////////////////////////////////////////////

static void flushData() {
    //DebugTrace("flush before: %d\n", g_bufferIndex - g_lastSavedBufferIndex);

	g_writer.flushBits();

    uint32_t timeout = millis() + CONF_WRITE_FLUSH_TIMEOUT_MS;
    while (g_lastSavedBufferIndex < g_writer.getBufferIndex() && millis() < timeout) {
        fileWrite(true);
    }

    //DebugTrace("flush after: %d\n", g_bufferIndex - g_lastSavedBufferIndex);
}

////////////////////////////////////////////////////////////////////////////////

static void initRecordingStart() {
    g_countingStarted = false;
    g_currentTime = 0;
    g_nextTime = 0;
    g_lastSavedBufferIndex = 0;

    memcpy(&g_recording.parameters, &g_parameters, sizeof(dlog_view::Parameters));

    if (isModuleLocalRecording() || isModuleControlledRecording()) {
        g_recording.parameters.period = dlog_view::PERIOD_MIN;
    }

    g_recording.size = 0;
    g_recording.pageSize = 480;

    g_recording.xAxisOffset = 0.0f;
    g_recording.xAxisDiv = g_recording.pageSize * g_recording.parameters.period / dlog_view::NUM_HORZ_DIVISIONS;

    for (int8_t dlogItemIndex = 0; dlogItemIndex < g_recording.parameters.numDlogItems; ++dlogItemIndex) {
        auto &dlogItem = g_recording.parameters.dlogItems[dlogItemIndex];
        dlogItem.resourceType = g_slots[dlogItem.slotIndex]->getDlogResourceType(dlogItem.subchannelIndex, dlogItem.resourceIndex);
    }

    if (!g_traceInitiated) {
        dlog_view::initAxis(g_recording);
    }

    dlog_view::initDlogValues(g_recording);
    dlog_view::calcColumnIndexes(g_recording);

    g_recording.getValue = getValue;

    g_writer.reset();
}

////////////////////////////////////////////////////////////////////////////////

static void setState(State newState) {
    if (g_state != newState) {
        if (newState == STATE_EXECUTING) {
            setOperBits(OPER_DLOG, true);
            event_queue::pushEvent(event_queue::EVENT_INFO_DLOG_START);
        } else if (g_state == STATE_EXECUTING) {
            setOperBits(OPER_DLOG, false);
            event_queue::pushEvent(event_queue::EVENT_INFO_DLOG_FINISH);
        }

        g_state = newState;
    }
}

static void log() {
    uint32_t tickCountMs = millis();

    static uint32_t g_seconds;
    static uint32_t g_millis;
    static uint32_t g_iSample;
    static uint32_t g_lastTickCountMs;

    if (!g_countingStarted) {
        g_seconds = 0;
        g_millis = 0;
        g_iSample = 0;
        g_countingStarted = true;
        g_lastTickCountMs = tickCountMs;
        return;
    }
	
    g_millis += tickCountMs - g_lastTickCountMs;
    g_lastTickCountMs = tickCountMs;

    if (g_millis >= 1000) {
        ++g_seconds;
        g_millis -= 1000;
    }

    g_currentTime = g_seconds + g_millis * 1E-3;

    if (g_traceInitiated) {
        // data is logged with the SCPI command `SENSe:DLOG:TRACe[:DATA]`
        return;
    }

    if (isModuleControlledRecording()) {
        // data is logged by module
        if (g_currentTime >= g_recording.parameters.duration) {
            stateTransition(EVENT_FINISH);
        }
        return;
    }

    while (g_currentTime >= g_nextTime) {
        // write sample
        for (int i = 0; i < g_recording.parameters.numDlogItems; i++) {
            auto &dlogItem = g_recording.parameters.dlogItems[i];
            if (dlogItem.resourceType == DLOG_RESOURCE_TYPE_U) {
                g_writer.writeFloat(channel_dispatcher::getUMonLast(dlogItem.slotIndex, dlogItem.subchannelIndex));
            } else if (dlogItem.resourceType == DLOG_RESOURCE_TYPE_I) {
                g_writer.writeFloat(channel_dispatcher::getIMonLast(dlogItem.slotIndex, dlogItem.subchannelIndex));
            } else if (dlogItem.resourceType == DLOG_RESOURCE_TYPE_P)  {
                g_writer.writeFloat(
                    channel_dispatcher::getUMonLast(dlogItem.slotIndex, dlogItem.subchannelIndex) *
                    channel_dispatcher::getIMonLast(dlogItem.slotIndex, dlogItem.subchannelIndex)
                );
            } else  if (dlogItem.resourceType >= DLOG_RESOURCE_TYPE_DIN0 && dlogItem.resourceType <= DLOG_RESOURCE_TYPE_DIN7) {
                if (g_writer.getBitMask() == 0) {
                    g_writer.writeBit(1); // mark as valid sample
                }                    

                uint8_t data;
                channel_dispatcher::getDigitalInputData(dlogItem.slotIndex, dlogItem.subchannelIndex, data, nullptr);
                if (data & (1 << (dlogItem.resourceType - DLOG_RESOURCE_TYPE_DIN0))) {
                    g_writer.writeBit(1);
                } else {
                    g_writer.writeBit(0);
                }
            }
        }

        g_writer.flushBits();

        ++g_recording.size;

        g_nextTime = ++g_iSample * g_recording.parameters.period;
        
        if (g_nextTime > g_recording.parameters.duration) {
            stateTransition(EVENT_FINISH);
            break;
        }
    }
}

static int doStartImmediately() {
    int err;

    err = checkDlogParameters(g_parameters, false, g_traceInitiated);
    if (err != SCPI_RES_OK) {
        return err;
    }

    if (!isModuleLocalRecording()) {
        err = fileTruncate();
        if (err != SCPI_RES_OK) {
            return err;
        }
    }

    initRecordingStart();

	for (uint8_t channelIndex = 0; channelIndex < dlog_file::MAX_NUM_OF_CHANNELS; channelIndex++) {
        if (channelIndex < CH_NUM) {
		    Channel &channel = Channel::get(channelIndex);
		    g_parameters.channels[channelIndex].moduleType = g_slots[channel.slotIndex]->moduleType;
		    g_parameters.channels[channelIndex].moduleRevision = g_slots[channel.slotIndex]->moduleRevision;
        } else {
		    g_parameters.channels[channelIndex].moduleType = 0;
		    g_parameters.channels[channelIndex].moduleRevision = 0;
        }
	}

    if (!isModuleLocalRecording()) {
        g_writer.writeFileHeaderAndMetaFields(g_recording.parameters);
        fileWrite(true);
    }

    for (int slotIndex = 0; slotIndex < NUM_SLOTS; slotIndex++) {
        g_slots[slotIndex]->onStartDlog();
    }

    g_recording.dataOffset = g_writer.getDataOffset();

    g_lastSavedBufferTickCount = millis();

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

static void resetAllParameters() {
    memset(&g_parameters, 0, sizeof(g_parameters));
    g_parameters.period = dlog_view::PERIOD_DEFAULT;
    g_parameters.duration = dlog_view::DURATION_DEFAULT;
    setTriggerSource(trigger::SOURCE_IMMEDIATE);
}

static void resetFilePath() {
    *g_parameters.filePath = 0;
}

static void doFinish(bool afterError) {
    setState(STATE_IDLE);

    if (!afterError) {
        if (!isModuleLocalRecording()) {        
            flushData();
            onSdCardFileChangeHook(g_parameters.filePath);
        }
    }

    file.close();

    resetFilePath();

    for (int slotIndex = 0; slotIndex < NUM_SLOTS; slotIndex++) {
        g_slots[slotIndex]->onStopDlog();
    }
}


////////////////////////////////////////////////////////////////////////////////

bool isModuleLocalRecording() {
    // TODO not supported for now
    return false;
}

bool isModuleControlledRecording() {
    return !g_traceInitiated && g_parameters.period < dlog_view::PERIOD_MIN;
}

int getModuleLocalRecordingSlotIndex() {
    if (isModuleLocalRecording()) {
        if (g_parameters.numDlogItems > 0) {
            return g_parameters.dlogItems[0].slotIndex;
        }
    }
    return -1;
}

bool isModuleAtSlotRecording(int slotIndex) {
	for (int i = 0; i < g_parameters.numDlogItems; i++) {
		if (g_parameters.dlogItems[i].slotIndex == slotIndex) {
			return true;
		}
	}
	return false;
}

double getCurrentTime() {
    return isModuleLocalRecording() ? (g_numSamples - 1) * g_parameters.period * 1.0 : g_currentTime;
}

uint32_t getFileLength() {
	return isModuleLocalRecording() ? g_fileLength : g_writer.getFileLength();
}

void setFileLength(uint32_t fileLength) {
    g_fileLength = fileLength;
}

void setNumSamples(uint32_t numSamples) {
    g_numSamples = numSamples;
}

////////////////////////////////////////////////////////////////////////////////

void setTriggerSource(trigger::Source source) {
    g_parameters.triggerSource = source;

    if (source == trigger::SOURCE_PIN1) {
        if (io_pins::g_ioPins[0].function != io_pins::FUNCTION_DLOGTRIG) {
            io_pins::setPinFunction(0, io_pins::FUNCTION_DLOGTRIG);
        }
    } else if (source == trigger::SOURCE_PIN2) {
        if (io_pins::g_ioPins[1].function != io_pins::FUNCTION_DLOGTRIG) {
            io_pins::setPinFunction(1, io_pins::FUNCTION_DLOGTRIG);
        }
    } else {
        if (io_pins::g_ioPins[0].function == io_pins::FUNCTION_DLOGTRIG) {
            io_pins::setPinFunction(0, io_pins::FUNCTION_NONE);
        } else if (io_pins::g_ioPins[1].function == io_pins::FUNCTION_DLOGTRIG) {
            io_pins::setPinFunction(1, io_pins::FUNCTION_NONE);
        }
    }
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
        bool somethingToLog = parameters.numDlogItems > 0;
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

    if (!isLowPriorityThread()) {
        sendMessageToLowPriorityThread(THREAD_MESSAGE_DLOG_STATE_TRANSITION, event);
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
        } else if (event == EVENT_TOGGLE_START) {
            err = doInitiate(false);
        } else if (event == EVENT_TOGGLE_STOP) {
            err = SCPI_RES_OK;
        } else if (event == EVENT_ABORT || event == EVENT_RESET) {
            resetFilePath();
            err = SCPI_RES_OK;
        } else if (event == EVENT_ABORT_AFTER_MASS_STORAGE_ERROR || event == EVENT_ABORT_AFTER_BUFFER_OVERFLOW_ERROR) {
            err = SCPI_RES_OK;
        }

    } else if (g_state == STATE_INITIATED) {
        if (event == EVENT_START || event == EVENT_TRIGGER || event == EVENT_TOGGLE_START) {
            err = doStartImmediately();
        } else if (event == EVENT_TOGGLE_START) {
            err = doInitiate(false);
        } else if (event == EVENT_ABORT || event == EVENT_RESET) {
            resetFilePath();
            setState(STATE_IDLE);
            err = SCPI_RES_OK;
        } else if (event == EVENT_ABORT_AFTER_MASS_STORAGE_ERROR) {
            resetFilePath();
            setState(STATE_IDLE);
            err = SCPI_ERROR_BUFFER_OVERFLOW;
        } else if (event == EVENT_ABORT_AFTER_BUFFER_OVERFLOW_ERROR) {
            resetFilePath();
            setState(STATE_IDLE);
            err = SCPI_ERROR_MASS_STORAGE_ERROR;
        }
    } else if (g_state == STATE_EXECUTING) {
        if (event == EVENT_TOGGLE_STOP || event == EVENT_FINISH || event == EVENT_ABORT || event == EVENT_RESET) {
            doFinish(false);
            err = SCPI_RES_OK;
        } else if (event == EVENT_TOGGLE_START) {
            err = SCPI_RES_OK;
        } else if (event == EVENT_ABORT_AFTER_BUFFER_OVERFLOW_ERROR) {
            doFinish(true);
            err = SCPI_ERROR_BUFFER_OVERFLOW;
        } else if (event == EVENT_ABORT_AFTER_MASS_STORAGE_ERROR) {
            doFinish(true);
            err = SCPI_ERROR_MASS_STORAGE_ERROR;
        }
    }

    if (event == EVENT_RESET) {
        resetAllParameters();
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

void toggleStart() {
    stateTransition(EVENT_TOGGLE_START);
}

void toggleStop() {
    stateTransition(EVENT_TOGGLE_STOP);
}

void abort() {
    stateTransition(EVENT_ABORT);
}

void abortAfterBufferOverflowError() {
    stateTransition(EVENT_ABORT_AFTER_BUFFER_OVERFLOW_ERROR);
}

void abortAfterMassStorageError() {
    stateTransition(EVENT_ABORT_AFTER_MASS_STORAGE_ERROR);
}

void reset() {
    stateTransition(EVENT_RESET);
}

////////////////////////////////////////////////////////////////////////////////

void tick() {
    if (g_state == STATE_EXECUTING && g_nextTime <= g_recording.parameters.duration && !g_inStateTransition) {
        log();
    }
}

void log(float *values, uint32_t bits) {
    if (g_state == STATE_EXECUTING) {
        for (int yAxisIndex = 0; yAxisIndex < g_recording.parameters.numYAxes; yAxisIndex++) {
            if (g_recording.parameters.yAxes[yAxisIndex].unit == UNIT_BIT) {
                if (g_writer.getBitMask() == 0) {
                    g_writer.writeBit(1); // mark as valid sample
                }                    
                if (bits & (1 << g_recording.parameters.yAxes[yAxisIndex].channelIndex)) {
                    g_writer.writeBit(1);
                } else {
                    g_writer.writeBit(0);
                }
            } else {
			    g_writer.writeFloat(*values++);
            }
        }
        g_writer.flushBits();
        ++g_recording.size;
    }
}

void logInvalid() {
    if (g_state == STATE_EXECUTING) {
        for (int yAxisIndex = 0; yAxisIndex < g_recording.parameters.numYAxes; yAxisIndex++) {
            if (g_recording.parameters.yAxes[yAxisIndex].unit == UNIT_BIT) {
                if (g_writer.getBitMask() == 0) {
                    g_writer.writeBit(0); // mark as invalid sample
                }                    
                g_writer.writeBit(0);
            } else {
			    g_writer.writeFloat(NAN);
            }
        }
        g_writer.flushBits();
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
