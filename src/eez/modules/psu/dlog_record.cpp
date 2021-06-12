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
#include <assert.h>

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


#if defined(EEZ_PLATFORM_STM32)
extern RTC_HandleTypeDef hrtc;
#endif

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
    EVENT_ABORT_AFTER_BUFFER_OVERFLOW_ERROR_MASTER,
    EVENT_ABORT_AFTER_BUFFER_OVERFLOW_ERROR_SLAVE,
    EVENT_ABORT_AFTER_MASS_STORAGE_ERROR,
    EVENT_RESET
};

dlog_view::Parameters g_recordingParameters;

dlog_view::Recording g_activeRecording; 

State g_state = STATE_IDLE;
bool g_inStateTransition;
static int g_stateTransitionError;

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

static uint8_t *g_saveBuffer = DLOG_RECORD_SAVE_BUFFER;

////////////////////////////////////////////////////////////////////////////////

static float getValue(uint32_t rowIndex, uint8_t columnIndex, float *max) {
	auto rowData = DLOG_RECORD_BUFFER + (g_activeRecording.dataOffset + 
            rowIndex * g_activeRecording.numBytesPerRow
		) % DLOG_RECORD_BUFFER_SIZE;

	float value = dlog_view::isValidSample(g_activeRecording, rowData) ?
		dlog_view::getSample(g_activeRecording, rowData, columnIndex) : NAN;

    if (g_activeRecording.parameters.yAxisScaleType == dlog_file::SCALE_LOGARITHMIC) {
        float logOffset = 1 - g_activeRecording.parameters.yAxes[columnIndex].range.min;
        value = log10f(logOffset + value);
    }

    *max = value;
    return value;
}

////////////////////////////////////////////////////////////////////////////////

static File g_file;
static bool g_bookmarksFileOpened;
static File g_bookmarksIndexFile;
static File g_bookmarksTextFile;

static int fileOpen() {
    if (!g_file.open(g_recordingParameters.filePath, FILE_CREATE_ALWAYS | FILE_WRITE)) {
        event_queue::pushEvent(event_queue::EVENT_ERROR_DLOG_FILE_OPEN_ERROR);
        // TODO replace with more specific error
        return SCPI_ERROR_MASS_STORAGE_ERROR;
    }

    g_bookmarksFileOpened = false;

    return SCPI_RES_OK;
}

bool getNextWriteBuffer(const uint8_t *&buffer, uint32_t &bufferSize, bool flush) {
    buffer = nullptr;
    bufferSize = 0;

// #if defined(EEZ_PLATFORM_STM32)
//     taskENTER_CRITICAL();
// #endif
    int32_t timeDiff = millis() - g_lastSavedBufferTickCount;
    uint32_t indexDiff = g_writer.getBufferIndex() - g_lastSavedBufferIndex;
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
            abortAfterBufferOverflowError(0);
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

        // File g_file;
        // if (g_file.open(g_activeRecording.parameters.filePath, FILE_OPEN_APPEND | FILE_WRITE)) {
        //     if (g_file.seek(g_lastSavedBufferIndex)) {
                size_t written = g_file.write(buffer, bufferSize);

                if (written != bufferSize) {
                    err = event_queue::EVENT_ERROR_DLOG_WRITE_ERROR;
                }

                // if (!g_file.close()) {
                //     err = event_queue::EVENT_ERROR_DLOG_WRITE_ERROR;
                // }

                if (!err) {
                    g_file.sync();
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
	static uint32_t g_seconds;
	static double g_subsecond;
	static uint32_t g_iSample;
	static uint32_t g_lastTickCountMs;

#if defined(EEZ_PLATFORM_STM32)
    uint32_t tickCountMs = 1024 - 4 * hrtc.Instance->SSR;
#endif

#if defined(EEZ_PLATFORM_SIMULATOR)
	uint32_t tickCountMs = millis();
#endif

    if (!g_countingStarted) {
        g_seconds = 0;
        g_subsecond = 0;
        g_iSample = 0;
        g_countingStarted = true;
        g_lastTickCountMs = tickCountMs;
    }

#if defined(EEZ_PLATFORM_STM32)
	int32_t diff = tickCountMs - g_lastTickCountMs;
	if (diff < 0) {
    	diff += 1024;
    }
	g_subsecond += diff / 1024.0;
#endif
	
#if defined(EEZ_PLATFORM_SIMULATOR)
	uint32_t diff = tickCountMs - g_lastTickCountMs;
	g_subsecond += diff / 1000.0;
#endif

	g_lastTickCountMs = tickCountMs;

    if (g_subsecond >= 1.0) {
        ++g_seconds;
        g_subsecond -= 1.0;
    }

    g_currentTime = g_seconds + g_subsecond;

    if (g_traceInitiated) {
        // data is logged with the SCPI command `SENSe:DLOG:TRACe[:DATA]`
        return;
    }

    if (isModuleControlledRecording()) {
        // data is logged by module
        if (g_currentTime >= g_activeRecording.parameters.duration) {
            stateTransition(EVENT_FINISH);
        }
        return;
    }

    while (g_currentTime >= g_nextTime) {
        if (g_activeRecording.parameters.dataContainsSampleValidityBit) {
            g_writer.writeBit(1); // mark as valid sample
        }

        // write sample
        for (int i = 0; i < g_activeRecording.parameters.numDlogItems; i++) {
            auto &dlogItem = g_activeRecording.parameters.dlogItems[i];
            if (dlogItem.resourceType == DLOG_RESOURCE_TYPE_U) {
                g_writer.flushBits();
                g_writer.writeFloat(channel_dispatcher::getUMonLast(dlogItem.slotIndex, dlogItem.subchannelIndex));
            } else if (dlogItem.resourceType == DLOG_RESOURCE_TYPE_I) {
                g_writer.flushBits();
                g_writer.writeFloat(channel_dispatcher::getIMonLast(dlogItem.slotIndex, dlogItem.subchannelIndex));
            } else if (dlogItem.resourceType == DLOG_RESOURCE_TYPE_P)  {
                g_writer.flushBits();
                g_writer.writeFloat(
                    channel_dispatcher::getUMonLast(dlogItem.slotIndex, dlogItem.subchannelIndex) *
                    channel_dispatcher::getIMonLast(dlogItem.slotIndex, dlogItem.subchannelIndex)
                );
            } else if (dlogItem.resourceType >= DLOG_RESOURCE_TYPE_DIN0 && dlogItem.resourceType <= DLOG_RESOURCE_TYPE_DIN7) {
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

        ++g_activeRecording.size;

        g_nextTime = ++g_iSample * g_activeRecording.parameters.period;
        
        if (g_nextTime > g_activeRecording.parameters.duration) {
            stateTransition(EVENT_FINISH);
            break;
        }
    }
}

static int doStartImmediately() {
    int err;

    err = checkDlogParameters(g_recordingParameters, false, g_traceInitiated);
    if (err != SCPI_RES_OK) {
        return err;
    }

    if (!isModuleLocalRecording()) {
        err = fileOpen();
        if (err != SCPI_RES_OK) {
            return err;
        }
    }

    g_countingStarted = false;
    g_currentTime = 0;
    g_nextTime = 0;
    g_lastSavedBufferIndex = 0;

    memcpy(&g_activeRecording.parameters, &g_recordingParameters, sizeof(dlog_view::Parameters));

    //if (isModuleLocalRecording() || isModuleControlledRecording()) {
    //    g_activeRecording.parameters.period = dlog_view::PERIOD_MIN;
    //}

    g_activeRecording.size = 0;

    g_activeRecording.xAxisOffset = 0.0;
    g_activeRecording.xAxisDiv = 1.0 * g_activeRecording.parameters.period * dlog_view::WIDTH_PER_DIVISION;

    for (int8_t dlogItemIndex = 0; dlogItemIndex < g_activeRecording.parameters.numDlogItems; ++dlogItemIndex) {
        auto &dlogItem = g_activeRecording.parameters.dlogItems[dlogItemIndex];
        dlogItem.resourceType = g_slots[dlogItem.slotIndex]->getDlogResourceType(dlogItem.subchannelIndex, dlogItem.resourceIndex);
    }

    if (!g_traceInitiated) {
        g_activeRecording.parameters.xAxis.unit = UNIT_SECOND;
        g_activeRecording.parameters.xAxis.step = g_activeRecording.parameters.period;
        g_activeRecording.parameters.xAxis.range.min = 0;
        g_activeRecording.parameters.xAxis.range.max = g_activeRecording.parameters.duration;

        for (int8_t dlogItemIndex = 0; dlogItemIndex < g_activeRecording.parameters.numDlogItems; ++dlogItemIndex) {
            auto &dlogItem = g_activeRecording.parameters.dlogItems[dlogItemIndex];
            auto &yAxis = g_activeRecording.parameters.yAxes[dlogItemIndex];

            yAxis.dataType = g_slots[dlogItem.slotIndex]->getDlogResourceDataType(dlogItem.subchannelIndex, dlogItem.resourceIndex);
            yAxis.transformOffset = g_slots[dlogItem.slotIndex]->getDlogResourceTransformOffset(dlogItem.subchannelIndex, dlogItem.resourceIndex);
			yAxis.transformScale = g_slots[dlogItem.slotIndex]->getDlogResourceTransformScale(dlogItem.subchannelIndex, dlogItem.resourceIndex);

            if (dlogItem.resourceType == DLOG_RESOURCE_TYPE_U) {
                yAxis.unit = UNIT_VOLT;
                yAxis.range.min = channel_dispatcher::getUMin(dlogItem.slotIndex, dlogItem.subchannelIndex);
                yAxis.range.max = channel_dispatcher::getUMax(dlogItem.slotIndex, dlogItem.subchannelIndex);
                Channel *channel = Channel::getBySlotIndex(dlogItem.slotIndex, dlogItem.subchannelIndex);
                yAxis.channelIndex = channel ? channel->channelIndex : -1;
            } else if (dlogItem.resourceType == DLOG_RESOURCE_TYPE_I) {
                yAxis.unit = UNIT_AMPER;
                yAxis.range.min = channel_dispatcher::getIMin(dlogItem.slotIndex, dlogItem.subchannelIndex);
                yAxis.range.max = channel_dispatcher::getIMaxLimit(dlogItem.slotIndex, dlogItem.subchannelIndex);
                Channel *channel = Channel::getBySlotIndex(dlogItem.slotIndex, dlogItem.subchannelIndex);
                yAxis.channelIndex = channel ? channel->channelIndex : -1;
            } else if (dlogItem.resourceType == DLOG_RESOURCE_TYPE_P) {
                yAxis.unit = UNIT_WATT;
                Channel *channel = Channel::getBySlotIndex(dlogItem.slotIndex, dlogItem.subchannelIndex);
                yAxis.range.min = channel_dispatcher::getPowerMinLimit(*channel);
                yAxis.range.max = channel_dispatcher::getPowerMaxLimit(*channel);
                yAxis.channelIndex = channel->channelIndex;
            } else if (dlogItem.resourceType >= DLOG_RESOURCE_TYPE_DIN0 && dlogItem.resourceType <= DLOG_RESOURCE_TYPE_DIN7) {
                yAxis.unit = UNIT_NONE;
                yAxis.range.min = 0;
                yAxis.range.max = 1;
                yAxis.channelIndex = dlogItem.resourceType - DLOG_RESOURCE_TYPE_DIN0;
            }

            stringCopy(yAxis.label, sizeof(yAxis.label), g_slots[dlogItem.slotIndex]->getDlogResourceLabel(dlogItem.subchannelIndex, dlogItem.resourceIndex));
        }

        g_activeRecording.parameters.numYAxes = g_activeRecording.parameters.numDlogItems;
    }

    dlog_view::initDlogValues(g_activeRecording);

    // do we need dataContainsSampleValidityBit?
    g_activeRecording.parameters.dataContainsSampleValidityBit = false;
    for (int yAxisIndex = 0; yAxisIndex < g_activeRecording.parameters.numYAxes; yAxisIndex++) {
        auto &yAxis = g_activeRecording.parameters.yAxes[yAxisIndex];
        if (yAxis.dataType != dlog_file::DATA_TYPE_FLOAT) {
            // yes!
            g_activeRecording.parameters.dataContainsSampleValidityBit = true;
            break;
        }
    }

    dlog_view::calcColumnIndexes(g_activeRecording);

    g_activeRecording.getValue = getValue;

    g_writer.reset();

	for (uint8_t channelIndex = 0; channelIndex < dlog_file::MAX_NUM_OF_CHANNELS; channelIndex++) {
        if (channelIndex < CH_NUM) {
		    Channel &channel = Channel::get(channelIndex);
			g_recordingParameters.channels[channelIndex].moduleType = g_slots[channel.slotIndex]->moduleType;
			g_recordingParameters.channels[channelIndex].moduleRevision = g_slots[channel.slotIndex]->moduleRevision;
        } else {
			g_recordingParameters.channels[channelIndex].moduleType = 0;
			g_recordingParameters.channels[channelIndex].moduleRevision = 0;
        }
	}

    g_activeRecording.parameters.startTime = datetime::nowUtc();
    g_activeRecording.parameters.finalDuration = 0;

    if (!isModuleLocalRecording()) {
        g_writer.writeFileHeaderAndMetaFields(g_activeRecording.parameters);
        fileWrite(true);
    }

    for (int slotIndex = 0; slotIndex < NUM_SLOTS; slotIndex++) {
        g_slots[slotIndex]->onStartDlog();
    }

    g_activeRecording.dataOffset = g_writer.getDataOffset();

    g_lastSavedBufferTickCount = millis();

    g_activeRecording.parameters.bookmarksSize = 0;

    setState(STATE_EXECUTING);

    return SCPI_RES_OK;
}

static int doInitiate(bool traceInitiated) {
    int err;

    g_traceInitiated = traceInitiated;

    if (g_recordingParameters.triggerSource == trigger::SOURCE_IMMEDIATE) {
        err = doStartImmediately();
    } else {
        err = checkDlogParameters(g_recordingParameters, false, g_traceInitiated);
        if (err == SCPI_RES_OK) {
            setState(STATE_INITIATED);
        }
    }

    if (err != SCPI_RES_OK) {
		g_recordingParameters.filePath[0] = 0;
        g_traceInitiated = false;
    }

    return err;
}

static void resetAllParameters() {
	#if defined(EEZ_PLATFORM_STM32)
	#pragma GCC diagnostic push
	#pragma GCC diagnostic ignored "-Wclass-memaccess"
	#endif

	memset(&g_recordingParameters, 0, sizeof(g_recordingParameters));

	#if defined(EEZ_PLATFORM_STM32)
	#pragma GCC diagnostic pop
	#endif

    g_recordingParameters.yAxis.unit = UNIT_UNKNOWN;
    g_recordingParameters.yAxis.dataType = dlog_file::DATA_TYPE_FLOAT;
	g_recordingParameters.yAxis.transformOffset = 0;
	g_recordingParameters.yAxis.transformScale = 1.0;
	g_recordingParameters.period = dlog_view::PERIOD_DEFAULT;
	g_recordingParameters.duration = dlog_view::DURATION_DEFAULT;
    setTriggerSource(trigger::SOURCE_IMMEDIATE);
}

static void resetFilePath() {
    *g_recordingParameters.filePath = 0;
}

static void finalizeBookmarks();

static void doFinish(bool afterError) {
    setState(STATE_IDLE);

    if (!afterError) {
        if (!isModuleLocalRecording()) {        
            flushData();
            onSdCardFileChangeHook(g_recordingParameters.filePath);
        }
    }

    // write final duration
	g_activeRecording.parameters.finalDuration = g_currentTime;
	g_file.seek(g_writer.getFinishTimeFieldOffset());
	g_file.write(&g_activeRecording.parameters.finalDuration, 8);

    if (!g_traceInitiated) {
        // write X axis step
	    g_file.seek(g_writer.getXAxisStepFieldOffset());
        float xAxisStep = g_activeRecording.parameters.finalDuration / (g_activeRecording.size - 1);
	    g_file.write(&xAxisStep, 4);
    }

    // write data size
	g_file.seek(g_writer.getDataSizeFieldOffset());
	g_file.write(&g_activeRecording.size, 4);

    if (g_bookmarksFileOpened) {
        finalizeBookmarks();
    }

    g_file.close();

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
    return !g_traceInitiated && g_recordingParameters.period < dlog_view::PERIOD_MIN;
}

int getModuleLocalRecordingSlotIndex() {
    if (isModuleLocalRecording()) {
        if (g_recordingParameters.numDlogItems > 0) {
            return g_recordingParameters.dlogItems[0].slotIndex;
        }
    }
    return -1;
}

bool isModuleAtSlotRecording(int slotIndex) {
	for (int i = 0; i < g_recordingParameters.numDlogItems; i++) {
		if (g_recordingParameters.dlogItems[i].slotIndex == slotIndex) {
			return true;
		}
	}
	return false;
}

double getCurrentTime() {
    return isModuleLocalRecording() ? (g_numSamples - 1) * g_recordingParameters.period * 1.0 : g_currentTime;
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
	g_recordingParameters.triggerSource = source;

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
        } else if (event == EVENT_ABORT_AFTER_MASS_STORAGE_ERROR || event == EVENT_ABORT_AFTER_BUFFER_OVERFLOW_ERROR_MASTER || event == EVENT_ABORT_AFTER_BUFFER_OVERFLOW_ERROR_SLAVE) {
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
            err = SCPI_ERROR_MASS_STORAGE_ERROR;
        } else if (event == EVENT_ABORT_AFTER_BUFFER_OVERFLOW_ERROR_MASTER || event == EVENT_ABORT_AFTER_BUFFER_OVERFLOW_ERROR_SLAVE) {
            resetFilePath();
            setState(STATE_IDLE);
            err = event == EVENT_ABORT_AFTER_BUFFER_OVERFLOW_ERROR_MASTER ? SCPI_ERROR_BUFFER_OVERFLOW : SCPI_ERROR_BUFFER_OVERFLOW_SLAVE;
        }
    } else if (g_state == STATE_EXECUTING) {
        if (event == EVENT_TOGGLE_STOP || event == EVENT_FINISH || event == EVENT_ABORT || event == EVENT_RESET) {
            doFinish(false);
			err = SCPI_RES_OK;
        } else if (event == EVENT_TOGGLE_START) {
            err = SCPI_RES_OK;
        } else if (event == EVENT_ABORT_AFTER_BUFFER_OVERFLOW_ERROR_MASTER || event == EVENT_ABORT_AFTER_BUFFER_OVERFLOW_ERROR_SLAVE) {
            doFinish(true);
            err = event == EVENT_ABORT_AFTER_BUFFER_OVERFLOW_ERROR_MASTER ? SCPI_ERROR_BUFFER_OVERFLOW : SCPI_ERROR_BUFFER_OVERFLOW_SLAVE;
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

void abortAfterBufferOverflowError(int detectionSource) {
    stateTransition(detectionSource ? EVENT_ABORT_AFTER_BUFFER_OVERFLOW_ERROR_SLAVE : EVENT_ABORT_AFTER_BUFFER_OVERFLOW_ERROR_MASTER);
}

void abortAfterMassStorageError() {
    stateTransition(EVENT_ABORT_AFTER_MASS_STORAGE_ERROR);
}

void reset() {
    stateTransition(EVENT_RESET);
}

////////////////////////////////////////////////////////////////////////////////

void tick() {
    if (g_state == STATE_EXECUTING && g_nextTime <= g_activeRecording.parameters.duration && !g_inStateTransition) {
        log();
    }
}

void log(float *values) {
	if (g_state == STATE_EXECUTING && g_nextTime < g_activeRecording.parameters.duration && !g_inStateTransition) {
        for (int yAxisIndex = 0; yAxisIndex < g_activeRecording.parameters.numYAxes; yAxisIndex++) {
            g_writer.writeFloat(*values++);
        }
        ++g_activeRecording.size;
    }
}

void log(uint32_t bits) {
	if (g_state == STATE_EXECUTING && g_nextTime < g_activeRecording.parameters.duration && !g_inStateTransition) {
        g_writer.writeBit(1); // mark as valid sample

        for (int yAxisIndex = 0; yAxisIndex < g_activeRecording.parameters.numYAxes; yAxisIndex++) {
            assert (g_activeRecording.parameters.yAxes[yAxisIndex].dataType == dlog_file::DATA_TYPE_BIT);
            if (bits & (1 << g_activeRecording.parameters.yAxes[yAxisIndex].channelIndex)) {
                g_writer.writeBit(1);
            } else {
                g_writer.writeBit(0);
            }
        }

        g_writer.flushBits();

        ++g_activeRecording.size;
    }
}

void logInt16(uint8_t *values, uint32_t bits) {
	if (g_state == STATE_EXECUTING && g_nextTime < g_activeRecording.parameters.duration && !g_inStateTransition) {
        g_writer.writeBit(1); // mark as valid sample

        for (int yAxisIndex = 0; yAxisIndex < g_activeRecording.parameters.numYAxes; yAxisIndex++) {
            if (g_activeRecording.parameters.yAxes[yAxisIndex].dataType == dlog_file::DATA_TYPE_BIT) {
                if (bits & (1 << g_activeRecording.parameters.yAxes[yAxisIndex].channelIndex)) {
                    g_writer.writeBit(1);
                } else {
                    g_writer.writeBit(0);
                }
            } else {
                g_writer.flushBits();
			    g_writer.writeInt16(values);
                values += 2;
            }
        }

        g_writer.flushBits();

        ++g_activeRecording.size;
    }
}

void logInt24(uint8_t *values, uint32_t bits) {
	if (g_state == STATE_EXECUTING && g_nextTime < g_activeRecording.parameters.duration && !g_inStateTransition) {
        g_writer.writeBit(1); // mark as valid sample

        for (int yAxisIndex = 0; yAxisIndex < g_activeRecording.parameters.numYAxes; yAxisIndex++) {
            if (g_activeRecording.parameters.yAxes[yAxisIndex].dataType == dlog_file::DATA_TYPE_BIT) {
                if (bits & (1 << g_activeRecording.parameters.yAxes[yAxisIndex].channelIndex)) {
                    g_writer.writeBit(1);
                } else {
                    g_writer.writeBit(0);
                }
            } else {
                g_writer.flushBits();
			    g_writer.writeInt24(values);
                values += 3;
            }
        }

        g_writer.flushBits();

        ++g_activeRecording.size;
    }
}

void logInvalid() {
	if (g_state == STATE_EXECUTING && g_nextTime < g_activeRecording.parameters.duration && !g_inStateTransition) {
        if (g_activeRecording.parameters.dataContainsSampleValidityBit) {
            g_writer.writeBit(0); // mark as invalid sample
        }

        for (int yAxisIndex = 0; yAxisIndex < g_activeRecording.parameters.numYAxes; yAxisIndex++) {
            auto dataType = g_activeRecording.parameters.yAxes[yAxisIndex].dataType;
            
            if (dataType == dlog_file::DATA_TYPE_BIT) {
                g_writer.writeBit(0);
            } else {
                g_writer.flushBits();

                if (dataType == dlog_file::DATA_TYPE_INT16_BE) {
                    g_writer.writeUint16(0);
                } else if (dataType == dlog_file::DATA_TYPE_INT24_BE) {
                    g_writer.writeInt24(0);
                } else if (dataType == dlog_file::DATA_TYPE_FLOAT) {
                    g_writer.writeFloat(NAN);
                } else {
                    assert(false);
                }
            }
        }

        g_writer.flushBits();
        ++g_activeRecording.size;
    }
}

void logBookmark(const char *text, size_t textLen) {
    if (isModuleLocalRecording()) {
        // TODO report error
        return;
    }

    if (g_state == STATE_EXECUTING && g_nextTime < g_activeRecording.parameters.duration && !g_inStateTransition) {
        if (!g_bookmarksFileOpened) {
            char filePath[MAX_PATH_LENGTH];

			stringCopy(filePath, MAX_PATH_LENGTH, g_recordingParameters.filePath);
			stringAppendString(filePath, MAX_PATH_LENGTH, ".dlog-bi");
			if (!g_bookmarksIndexFile.open(filePath, FILE_CREATE_ALWAYS | FILE_WRITE)) {
				// TODO report error
				return;
			}
			
			stringCopy(filePath, MAX_PATH_LENGTH, g_recordingParameters.filePath);
			stringAppendString(filePath, MAX_PATH_LENGTH, ".dlog-bt");
			if (!g_bookmarksTextFile.open(filePath, FILE_CREATE_ALWAYS | FILE_WRITE)) {
				// TODO report error
				return;
			}

            g_bookmarksFileOpened = true;
        }

        uint8_t buffer[8];

        uint32_t positon = g_activeRecording.size;

		((uint32_t *)buffer)[0] = positon;
		((uint32_t *)buffer)[1] = g_bookmarksTextFile.size();

        if (g_bookmarksIndexFile.write(buffer, sizeof(buffer)) != sizeof(buffer)) {
            // TODO report error
        }

		g_bookmarksIndexFile.sync();

		if (textLen > dlog_file::MAX_BOOKMARK_TEXT_LEN) {
			textLen = dlog_file::MAX_BOOKMARK_TEXT_LEN;
		}
		
		if (g_bookmarksTextFile.write(text, textLen) != textLen) {
            // TODO report error
        }

		g_bookmarksTextFile.sync();

		dlog_view::appendLiveBookmark(positon, text, textLen);
    }
}

static void finalizeBookmarks() {
	char filePath[MAX_PATH_LENGTH];
	uint32_t fileSize;
	
	//
	// append bookmarks index file to the end of dlog file
    //
	g_bookmarksIndexFile.close();

	stringCopy(filePath, MAX_PATH_LENGTH, g_recordingParameters.filePath);
	stringAppendString(filePath, MAX_PATH_LENGTH, ".dlog-bi");

	if (!g_bookmarksIndexFile.open(filePath, FILE_OPEN_EXISTING | FILE_READ)) {
		// TODO report error
		return;
	}

    fileSize = g_bookmarksIndexFile.size();

    if (!g_file.seek(g_writer.getBookmarksSizeFieldOffset())) {
		// TODO report error
		return;
    }
    uint32_t bookmarksSize = fileSize / dlog_file::NUM_BYTES_PER_BOOKMARK_IN_INDEX;
    if (g_file.write(&bookmarksSize, sizeof(bookmarksSize)) != sizeof(bookmarksSize)) {
        // TODO report error
		return;
    }

    if (!g_file.seek(g_writer.getDataOffset() + g_activeRecording.size * g_activeRecording.numBytesPerRow)) {
        // TODO report error
		return;
    }

    for (uint32_t i = 0; i < fileSize; i += CHUNK_SIZE) {
        uint32_t n = MIN(CHUNK_SIZE, fileSize - i);
		if (g_bookmarksIndexFile.read(DLOG_RECORD_BUFFER, n) != n) {
            // TODO report error
            return;
        }
        if (g_file.write(DLOG_RECORD_BUFFER, n) != n) {
            // TODO report error
            return;
        }
    }

	uint8_t buffer[8];

	((uint32_t *)buffer)[0] = 0;
	((uint32_t *)buffer)[1] = g_bookmarksTextFile.size();

	if (g_file.write(buffer, sizeof(buffer)) != sizeof(buffer)) {
		// TODO report error
	}

    g_bookmarksIndexFile.close();
    sd_card::deleteFile(filePath, nullptr);

    //
    // append bookmarks text file to the end of dlog file
    //
	g_bookmarksTextFile.close();

	stringCopy(filePath, MAX_PATH_LENGTH, g_recordingParameters.filePath);
	stringAppendString(filePath, MAX_PATH_LENGTH, ".dlog-bt");

	if (!g_bookmarksTextFile.open(filePath, FILE_OPEN_EXISTING | FILE_READ)) {
		// TODO report error
		return;
	}

    fileSize = g_bookmarksTextFile.size();

    for (uint32_t i = 0; i < fileSize; i += CHUNK_SIZE) {
        uint32_t n = MIN(CHUNK_SIZE, fileSize - i);
		if (g_bookmarksTextFile.read(DLOG_RECORD_BUFFER, n) != n) {
            // TODO report error
            return;
        }
        if (g_file.write(DLOG_RECORD_BUFFER, n) != n) {
            // TODO report error
            return;
        }
    }

    g_bookmarksTextFile.close();
    sd_card::deleteFile(filePath, nullptr);
}

////////////////////////////////////////////////////////////////////////////////

const char *getLatestFilePath() {
    return g_activeRecording.parameters.filePath[0] != 0 ? g_activeRecording.parameters.filePath : nullptr;
}

} // namespace dlog_record
} // namespace psu
} // namespace eez
