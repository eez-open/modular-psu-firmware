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

#include <eez/scpi/scpi.h>

#include <eez/modules/psu/channel_dispatcher.h>
#include <eez/modules/psu/datetime.h>
#include <eez/modules/psu/scpi/psu.h>
#include <eez/modules/psu/sd_card.h>
#include <eez/system.h>
#include <eez/modules/psu/dlog.h>
#include <eez/modules/psu/event_queue.h>

#ifdef EEZ_PLATFORM_STM32
#include <eez/platform/stm32/defines.h>
#endif

namespace eez {

using namespace scpi;

namespace psu {
namespace dlog {

Options g_nextOptions;

trigger::Source g_triggerSource = trigger::SOURCE_IMMEDIATE;
static char g_filePath[MAX_PATH_LENGTH + 1];

Options g_lastOptions;
uint8_t *g_lastBufferStart;
uint32_t g_size;

uint8_t g_totalDlogValues;
uint8_t g_numVisibleDlogValues;
DlogValueParams g_dlogValues[MAX_DLOG_VALUES];

eez::gui::data::Value g_timeOffset;

uint32_t g_pageSize = 480;
uint32_t g_cursorOffset = 240;

static State g_state = STATE_IDLE;

static uint32_t g_lastTickCount;
static uint32_t g_seconds;
static uint32_t g_micros;
static uint32_t g_iSample;
double g_currentTime;
static double g_nextTime;
static uint32_t g_lastSyncTickCount;

uint32_t g_fileLength;

#ifdef EEZ_PLATFORM_STM32
static uint8_t *g_buffer = (uint8_t *)DLOG_BUFFER;
#endif

#ifdef EEZ_PLATFORM_SIMULATOR
#define DLOG_BUFFER_SIZE (1024 * 1024)
static uint8_t g_bufferMemory[DLOG_BUFFER_SIZE];
static uint8_t *g_buffer = g_bufferMemory;
#endif

static const unsigned int CHUNK_SIZE = 4096;
static const unsigned int NUM_CHUNKS = DLOG_BUFFER_SIZE / CHUNK_SIZE;

static unsigned int g_selectedChunkIndex;
static unsigned int g_bufferIndex;

static unsigned int g_lastChunkIndex;
static unsigned int g_lastChunkSize;

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

int checkDlogParameters() {
    bool somethingToLog = false;
    for (int i = 0; i < CH_NUM; ++i) {
        if (g_nextOptions.logVoltage[i] || g_nextOptions.logCurrent[i] || g_nextOptions.logPower[i]) {
            somethingToLog = true;
            break;
        }
    }

    if (!somethingToLog) {
        // TODO replace with more specific error
        return SCPI_ERROR_EXECUTION_ERROR;
    }

    if (!*g_filePath) {
        // TODO replace with more specific error
        return SCPI_ERROR_EXECUTION_ERROR;
    }

    return SCPI_RES_OK;
}

State getState() {
    return g_state;
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

int initiate(const char *filePath) {
    int error = SCPI_RES_OK;

    strcpy(g_filePath, filePath);

    if (g_triggerSource == trigger::SOURCE_IMMEDIATE) {
        error = startImmediately();
    } else {
        error = checkDlogParameters();
        if (error == SCPI_RES_OK) {
            setState(STATE_INITIATED);
        }
    }

    if (error != SCPI_RES_OK) {
        g_filePath[0] = 0;
    }

    return error;
}

void triggerGenerated(bool startImmediatelly) {
    if (startImmediatelly) {
        int err = startImmediately();
        if (err != SCPI_RES_OK) {
            generateError(err);
        }
    } else {
        setState(STATE_TRIGGERED);
    }
}

#define MAGIC1 0x2D5A4545L
#define MAGIC2 0x474F4C44L
#define VERSION 0x00000001L

int fileOpen() {
	File file;
    if (!file.open(g_filePath, FILE_OPEN_APPEND | FILE_WRITE)) {
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
	File file;
    if (!file.open(g_filePath, FILE_OPEN_APPEND | FILE_WRITE)) {
    	event_queue::pushEvent(event_queue::EVENT_ERROR_DLOG_FILE_REOPEN_ERROR);
    	abort(false);
        return;
    }

    size_t written = file.write(g_buffer + g_lastChunkIndex * CHUNK_SIZE, g_lastChunkSize);
	file.close();
	if (written != g_lastChunkSize) {
		event_queue::pushEvent(event_queue::EVENT_ERROR_DLOG_WRITE_ERROR);
		abort(false);
	}
}

void flushData() {
    g_lastChunkIndex = g_selectedChunkIndex;
    g_lastChunkSize = g_bufferIndex;

    g_selectedChunkIndex = (g_selectedChunkIndex + 1) % NUM_CHUNKS;
    g_bufferIndex = 0;

    if (osThreadGetId() != g_scpiTaskHandle) {
        osMessagePut(g_scpiMessageQueueId, SCPI_QUEUE_MESSAGE(SCPI_QUEUE_MESSAGE_TARGET_NONE, SCPI_QUEUE_MESSAGE_DLOG_FILE_WRITE, 0), osWaitForever);
    } else {
        fileWrite();
    }
}

void writeUint8(uint8_t value) {
    *(g_buffer + g_selectedChunkIndex * CHUNK_SIZE + g_bufferIndex) = value;

    if (++g_bufferIndex == CHUNK_SIZE) {
        g_lastSyncTickCount = micros();
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

int startImmediately() {
	int err;

    err = checkDlogParameters();
    if (err != SCPI_RES_OK) {
        return err;
    }

    err = fileOpen();
    if (err != SCPI_RES_OK) {
        return err;
    }

    g_selectedChunkIndex = 0;
    g_bufferIndex = 0;
    g_lastSyncTickCount = micros();
    g_fileLength = 0;

    memcpy(&g_lastOptions, &g_nextOptions, sizeof(Options));

    dlog::g_timeOffset = gui::data::Value(0.0f, UNIT_SECOND);

    g_totalDlogValues = 0;
    g_numVisibleDlogValues = 0;

    static const int MAX_VISIBLE_DLOG_VALUES = 2;

    uint32_t columns = 0;
    for (int iChannel = 0; iChannel < CH_NUM; ++iChannel) {
        if (g_lastOptions.logVoltage[iChannel]) {
            columns |= 1 << (4 * iChannel);
            ++g_totalDlogValues;
            if (g_numVisibleDlogValues < MAX_VISIBLE_DLOG_VALUES) {
                dlog::g_dlogValues[g_numVisibleDlogValues].dlogValueType = (DlogValueType)(3 * g_numVisibleDlogValues + dlog::DLOG_VALUE_CH1_U);
                float perDiv = channel_dispatcher::getUMax(Channel::get(iChannel)) / dlog::NUM_VERT_DIVISIONS;
                dlog::g_dlogValues[g_numVisibleDlogValues].perDiv = gui::data::Value(roundPrec(perDiv, 0.01f), UNIT_VOLT);
                dlog::g_dlogValues[g_numVisibleDlogValues].offset = gui::data::Value(roundPrec(-perDiv * dlog::NUM_VERT_DIVISIONS / 2, 0.01f), UNIT_VOLT);

                ++g_numVisibleDlogValues;
            }
        }
        if (g_lastOptions.logCurrent[iChannel]) {
            columns |= 2 << (4 * iChannel);
            ++g_totalDlogValues;
            if (g_numVisibleDlogValues < MAX_VISIBLE_DLOG_VALUES) {
                dlog::g_dlogValues[g_numVisibleDlogValues].dlogValueType = (DlogValueType)(3 * g_numVisibleDlogValues + dlog::DLOG_VALUE_CH1_I);
                float perDiv = channel_dispatcher::getIMax(Channel::get(iChannel)) / dlog::NUM_VERT_DIVISIONS;
                dlog::g_dlogValues[g_numVisibleDlogValues].perDiv = gui::data::Value(roundPrec(perDiv, 0.01f), UNIT_AMPER);
                dlog::g_dlogValues[g_numVisibleDlogValues].offset = gui::data::Value(roundPrec(-perDiv * dlog::NUM_VERT_DIVISIONS / 2, 0.01f), UNIT_AMPER);

                ++g_numVisibleDlogValues;
            }
        }
        if (g_lastOptions.logPower[iChannel]) {
            columns |= 4 << (4 * iChannel);
            ++g_totalDlogValues;
            if (g_numVisibleDlogValues < MAX_VISIBLE_DLOG_VALUES) {
                dlog::g_dlogValues[g_numVisibleDlogValues].dlogValueType = (DlogValueType)(3 * g_numVisibleDlogValues + dlog::DLOG_VALUE_CH1_P);
                float perDiv = channel_dispatcher::getPowerMaxLimit(Channel::get(iChannel)) / dlog::NUM_VERT_DIVISIONS;
                dlog::g_dlogValues[g_numVisibleDlogValues].perDiv = gui::data::Value(roundPrec(perDiv, 0.01f), UNIT_WATT);
                dlog::g_dlogValues[g_numVisibleDlogValues].offset = gui::data::Value(roundPrec(-perDiv * dlog::NUM_VERT_DIVISIONS / 2, 0.01f), UNIT_WATT);

                ++g_numVisibleDlogValues;
            }
        }
    }

    g_lastTickCount = micros();
    g_seconds = 0;
    g_micros = 0;
    g_iSample = 0;
    g_currentTime = 0;
    g_nextTime = 0;

    writeUint32(MAGIC1);
    writeUint32(MAGIC2);

    writeUint16(VERSION);

    if (CONF_DLOG_JITTER) {
        writeUint16(1);
    } else {
        writeUint16(0);
    }

    writeUint32(columns);

    writeFloat(g_lastOptions.period);
    writeFloat(g_lastOptions.time);
    writeUint32(datetime::nowUtc());

    g_lastBufferStart = g_buffer + g_selectedChunkIndex * CHUNK_SIZE + g_bufferIndex;
    g_size = 0;

    setState(STATE_EXECUTING);

    log(g_lastTickCount);

    return SCPI_RES_OK;
}

void finishLogging(bool flush) {
	setState(STATE_IDLE);

	if (flush) {
        flushData();
	}

    for (int i = 0; i < CH_NUM; ++i) {
        g_nextOptions.logVoltage[i] = 0;
        g_nextOptions.logCurrent[i] = 0;
        g_nextOptions.logPower[i] = 0;
    }
}

void abort(bool flush) {
    if (g_state == STATE_EXECUTING) {
        finishLogging(flush);
    } else if (g_state == STATE_INITIATED || g_state == STATE_TRIGGERED) {
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

    if (g_currentTime >= g_nextTime) {
        while (1) {
            g_nextTime = ++g_iSample * g_lastOptions.period;
            if (g_currentTime < g_nextTime || g_nextTime > g_lastOptions.time) {
                break;
            }

#if defined(EEZ_PLATFORM_SIMULATOR)
            for (int i = 0; i < CH_NUM; ++i) {
                Channel &channel = Channel::get(i);

                float uMon = 0;
                float iMon = 0;

                if (g_lastOptions.logVoltage[i]) {
                    uMon = channel_dispatcher::getUMonLast(channel);
                    writeFloat(uMon);
                }

                if (g_lastOptions.logCurrent[i]) {
                    iMon = channel_dispatcher::getIMonLast(channel);
                    writeFloat(iMon);
                }

                if (g_lastOptions.logPower[i]) {
                    if (!g_lastOptions.logVoltage[i]) {
                        uMon = channel_dispatcher::getUMonLast(channel);
                    }
                    if (!g_lastOptions.logCurrent[i]) {
                        iMon = channel_dispatcher::getIMonLast(channel);
                    }
                    writeFloat(uMon * iMon);
                }
            }

            ++g_size;
#else
            // we missed a sample, write NAN
#ifdef DLOG_JITTER
            writeFloat(NAN);
#endif
            for (int i = 0; i < CH_NUM; ++i) {
                if (g_lastOptions.logVoltage[i]) {
                    writeFloat(NAN);
                }
                if (g_lastOptions.logCurrent[i]) {
                    writeFloat(NAN);
                }
                if (g_lastOptions.logPower[i]) {
                    writeFloat(NAN);
                }
            }

            ++g_size;
#endif
        }

        // write sample
#ifdef DLOG_JITTER
        writeFloat(dt);
#endif
        for (int i = 0; i < CH_NUM; ++i) {
            Channel &channel = Channel::get(i);

            float uMon = 0;
            float iMon = 0;

            if (g_lastOptions.logVoltage[i]) {
                uMon = channel_dispatcher::getUMonLast(channel);
                writeFloat(uMon);
            }

            if (g_lastOptions.logCurrent[i]) {
                iMon = channel_dispatcher::getIMonLast(channel);
                writeFloat(iMon);
            }

            if (g_lastOptions.logPower[i]) {
                if (!g_lastOptions.logVoltage[i]) {
                    uMon = channel_dispatcher::getUMonLast(channel);
                }
                if (!g_lastOptions.logCurrent[i]) {
                    iMon = channel_dispatcher::getIMonLast(channel);
                }
                writeFloat(uMon * iMon);
            }
        }
        
        ++g_size;

        if (g_nextTime > g_lastOptions.time) {
            finishLogging(true);
        } else {
            int32_t diff = tickCount - g_lastSyncTickCount;
            if (diff > CONF_DLOG_SYNC_FILE_TIME * 1000000L) {
                g_lastSyncTickCount = tickCount;
                flushData();
            }
        }
    }
}

void tick(uint32_t tickCount) {
    if (g_state == STATE_TRIGGERED) {
        int err = startImmediately();
        if (err != SCPI_RES_OK) {
            generateError(err);
        }
    } else if (g_state == STATE_EXECUTING) {
        log(tickCount);
    }
}

void reset() {
    abort(false);

    for (int i = 0; i < CH_NUM; ++i) {
        g_nextOptions.logVoltage[i] = 0;
        g_nextOptions.logCurrent[i] = 0;
        g_nextOptions.logPower[i] = 0;
    }

    g_nextOptions.period = PERIOD_DEFAULT;
    g_nextOptions.time = TIME_DEFAULT;
    g_triggerSource = trigger::SOURCE_IMMEDIATE;
    g_filePath[0] = 0;
}

uint32_t getSize() {
    return g_size;
}

} // namespace dlog
} // namespace psu
} // namespace eez

#endif // OPTION_SD_CARD
