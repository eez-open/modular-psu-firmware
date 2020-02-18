/* / mcu / sound.h
 * EEZ Modular Firmware
 * Copyright (C) 2015-present, Envox d.o.o.
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

#include <scpi/scpi.h>

#include <eez/firmware.h>
#include <eez/sound.h>

#include <eez/scpi/scpi.h>

#include <eez/modules/mcu/eeprom.h>

#include <eez/modules/psu/datetime.h>
#include <eez/modules/psu/event_queue.h>
#include <eez/modules/psu/sd_card.h>

#include <eez/libs/sd_fat/sd_fat.h>

#if OPTION_ETHERNET
#include <eez/modules/mcu/ethernet.h>
#endif

#if OPTION_DISPLAY
#include <eez/modules/psu/gui/psu.h>
#include <eez/modules/psu/gui/data.h>
#endif

namespace eez {
namespace psu {
namespace event_queue {

static const char *LOG_FILE_NAME = "log.txt";

static const char *LOG_DEBUG_INDEX_FILE_NAME   = "index1";
static const char *LOG_INFO_INDEX_FILE_NAME    = "index2";
static const char *LOG_WARNING_INDEX_FILE_NAME = "index3";
static const char *LOG_ERROR_INDEX_FILE_NAME   = "index4";

static const char *EVENT_TYPE_NAMES[] = {
    "NONE",
    "DEBUG",
    "INFO",
    "WARNING",
    "ERROR",
    "UNKNOWN"
};

static const size_t EVENT_MESSAGE_MAX_SIZE = 128;

struct Event {
    uint32_t eventIndex;
    uint32_t dateTime;
    int eventType;
    char message[EVENT_MESSAGE_MAX_SIZE];
};

static const int WRITE_QUEUE_MAX_SIZE = 50;
struct QueueEvent {
    uint32_t dateTime;
    int16_t eventId;
};
static QueueEvent g_writeQueue[WRITE_QUEUE_MAX_SIZE];
static uint8_t g_writeQueueSize = 0;

static uint32_t g_numEvents = 0;
static uint32_t g_pageIndex = 0;

static int16_t g_lastErrorEventId;

static const int EVENTS_CACHE_SIZE = EVENTS_PER_PAGE;
static Event g_eventsCache[EVENTS_CACHE_SIZE];

////////////////////////////////////////////////////////////////////////////////

static void writeEvent(QueueEvent *event);
static void readEvent(Event &event);

static Event *getEvent(uint32_t eventIndex);

////////////////////////////////////////////////////////////////////////////////

void init() {
    for (int i = 0; i < EVENTS_CACHE_SIZE; i++) {
        g_eventsCache[i].eventIndex = 0xFFFFFFFF;
    }

    g_numEvents = 0;

    char filePath[MAX_PATH_LENGTH];

    strcpy(filePath, LOGS_DIR);
    strcat(filePath, PATH_SEPARATOR);
    strcat(filePath, LOG_DEBUG_INDEX_FILE_NAME);

    File file;
    if (file.open(filePath, FILE_OPEN_EXISTING | FILE_READ)) {
        g_numEvents = file.size() / 4;
        file.close();
    }

    if (g_numEvents == 0) {
        pushEvent(EVENT_INFO_WELCOME);
    }
}

void tick() {
    if (g_writeQueueSize > 0) {
        for (int i = 0; i < g_writeQueueSize; ++i) {
            writeEvent(&g_writeQueue[i]);
            ++g_numEvents;
        }
        g_writeQueueSize = 0;
        
        // invalidate cache
        for (int i = 0; i < EVENTS_CACHE_SIZE; ++i) {
            g_eventsCache[i].eventIndex = 0xFFFFFFFF;
        }
    } else {
        for (int i = 0; i < EVENTS_CACHE_SIZE; ++i) {
            if (g_eventsCache[i].eventType == EVENT_TYPE_UNKNOWN) {
                readEvent(g_eventsCache[i]);
            }
        }
    }
}

int16_t getLastErrorEventId() {
    return g_lastErrorEventId;
}

uint32_t getEventDateTime(Event *e) {
    return e->dateTime;
}

int getEventType(int16_t eventId) {
    if (eventId >= EVENT_INFO_START_ID) {
        return EVENT_TYPE_INFO;
    } else if (eventId >= EVENT_WARNING_START_ID) {
        return EVENT_TYPE_WARNING;
    } else if (eventId != EVENT_TYPE_NONE) {
        return EVENT_TYPE_ERROR;
    } else {
        return EVENT_TYPE_NONE;
    }
}

int getEventType(Event *e) {
    if (!e) {
        return EVENT_TYPE_NONE;
    }
    return e->eventType;
}

const char *getEventMessage(int16_t eventId) {
    static char message[35];

    const char *p_message = 0;

    if (eventId >= EVENT_INFO_START_ID) {
        switch (eventId) {
#define EVENT_SCPI_ERROR(ID, TEXT)
#define EVENT_ERROR(NAME, ID, TEXT)
#define EVENT_WARNING(NAME, ID, TEXT)
#define EVENT_INFO(NAME, ID, TEXT)                                                                 \
    case EVENT_INFO_START_ID + ID:                                                                 \
        p_message = TEXT;                                                                          \
        break;
            LIST_OF_EVENTS
#undef EVENT_SCPI_ERROR
#undef EVENT_INFO
#undef EVENT_WARNING
#undef EVENT_ERROR
        }
    } else if (eventId >= EVENT_WARNING_START_ID) {
        switch (eventId) {
#define EVENT_SCPI_ERROR(ID, TEXT)
#define EVENT_ERROR(NAME, ID, TEXT)
#define EVENT_WARNING(NAME, ID, TEXT)                                                              \
    case EVENT_WARNING_START_ID + ID:                                                              \
        p_message = TEXT;                                                                          \
        break;
#define EVENT_INFO(NAME, ID, TEXT)
            LIST_OF_EVENTS
#undef EVENT_SCPI_ERROR
#undef EVENT_INFO
#undef EVENT_WARNING
#undef EVENT_ERROR
        default:
            p_message = 0;
        }
    } else {
        switch (eventId) {
#define EVENT_SCPI_ERROR(ID, TEXT)                                                                 \
    case ID:                                                                                       \
        p_message = TEXT;                                                                          \
        break;
#define EVENT_INFO(NAME, ID, TEXT)
#define EVENT_WARNING(NAME, ID, TEXT)
#define EVENT_ERROR(NAME, ID, TEXT)                                                                \
    case EVENT_ERROR_START_ID + ID:                                                                \
        p_message = TEXT;                                                                          \
        break;
            LIST_OF_EVENTS
#undef EVENT_SCPI_ERROR
#undef EVENT_INFO
#undef EVENT_WARNING
#undef EVENT_ERROR
        default:
            return SCPI_ErrorTranslate(eventId);
        }
    }

    if (p_message) {
        strncpy(message, p_message, sizeof(message) - 1);
        message[sizeof(message) - 1] = 0;
        return message;
    }

    return 0;
}

const char *getEventMessage(Event *e) {
    if (!e) {
        return nullptr;
    }
    return e->message;
}

bool compareEvents(Event *aEvent, Event *bEvent) {
    return aEvent->eventIndex == bEvent->eventIndex;
}

void pushEvent(int16_t eventId) {
    if (g_writeQueueSize < WRITE_QUEUE_MAX_SIZE) {
        g_writeQueue[g_writeQueueSize].dateTime = datetime::now();
        g_writeQueue[g_writeQueueSize].eventId = eventId;
        ++g_writeQueueSize;
    } else {
        g_writeQueue[g_writeQueueSize - 1].dateTime = datetime::now();
        g_writeQueue[g_writeQueueSize - 1].eventId = EVENT_ERROR_TOO_MANY_LOG_EVENTS;
    }

#if OPTION_ETHERNET
    eez::mcu::ethernet::pushEvent(eventId);
#endif

    if (getEventType(eventId) == EVENT_TYPE_ERROR) {
        g_lastErrorEventId  = g_writeQueue[g_writeQueueSize - 1].eventId;

        sound::playBeep();

#if OPTION_DISPLAY
        if (g_isBooted) {
            eez::psu::gui::psuErrorMessage(0, MakeEventMessageValue(eventId));
        }
#endif
    }

}

void markAsRead() {
    g_lastErrorEventId = EVENT_TYPE_NONE;
}

int getNumPages() {
    return (g_numEvents + EVENTS_PER_PAGE - 1) / EVENTS_PER_PAGE;
}

int getActivePageNumEvents() {
    if ((int)g_pageIndex < getNumPages() - 1) {
        return EVENTS_PER_PAGE;
    } else {
        return g_numEvents - (getNumPages() - 1) * EVENTS_PER_PAGE;
    }
}

Event *getActivePageEvent(int i) {
    int n = event_queue::getActivePageNumEvents();
    if (i < n) {
        return getEvent(g_pageIndex * EVENTS_PER_PAGE + i);
    }
    return nullptr;
}

void moveToFirstPage() {
    g_pageIndex = 0;
}

void moveToNextPage() {
    if ((int)g_pageIndex < getNumPages() - 1) {
        ++g_pageIndex;
    }
}

void moveToPreviousPage() {
    if (g_pageIndex > 0) {
        --g_pageIndex;
    }
}

int getActivePageIndex() {
    return g_pageIndex;
}

////////////////////////////////////////////////////////////////////////////////

static bool writeToLog(QueueEvent *event, uint32_t &logOffset, int &eventType) {
    char filePath[MAX_PATH_LENGTH];

    strcpy(filePath, LOGS_DIR);
    strcat(filePath, PATH_SEPARATOR);
    strcat(filePath, LOG_FILE_NAME);

    File file;
    if (!file.open(filePath, FILE_OPEN_APPEND | FILE_WRITE)) {
        return false;
    }

    logOffset = file.size();

    using namespace sd_card;
    BufferedFileWrite bufferedFile(file);

    int year, month, day, hour, minute, second;
    datetime::breakTime(event->dateTime, year, month, day, hour, minute, second);

    eventType = getEventType(event->eventId);

    char dateTimeAndEventTypeStr[32];
    sprintf(dateTimeAndEventTypeStr, "%04d-%02d-%02d %02d:%02d:%02d %s ", year, month, day, hour, minute, second, EVENT_TYPE_NAMES[eventType]);
    bufferedFile.write((const uint8_t *)dateTimeAndEventTypeStr, strlen(dateTimeAndEventTypeStr));

    const char *message = getEventMessage(event->eventId);
    bufferedFile.write((const uint8_t *)message, strlen(message));

    bufferedFile.write((const uint8_t *)"\n", 1);

    bufferedFile.flush();

    file.close();

    return true;
}

static void writeToIndex(const char *indexFileName, uint32_t logOffset) {
    char filePath[MAX_PATH_LENGTH];

    strcpy(filePath, LOGS_DIR);
    strcat(filePath, PATH_SEPARATOR);
    strcat(filePath, indexFileName);

    File file;
    if (file.open(filePath, FILE_OPEN_APPEND | FILE_WRITE)) {
        file.write((const uint8_t *)&logOffset, sizeof(uint32_t));
        file.close();
    }
}

static void writeEvent(QueueEvent *event) {
    uint32_t logOffset;
    int eventType;
    if (!writeToLog(event, logOffset, eventType)) {
        return;
    }

    writeToIndex(LOG_DEBUG_INDEX_FILE_NAME, logOffset);

    if (eventType == EVENT_TYPE_DEBUG) {
        return;
    }

    writeToIndex(LOG_INFO_INDEX_FILE_NAME, logOffset);

    if (eventType == EVENT_TYPE_INFO) {
        return;
    }

    writeToIndex(LOG_WARNING_INDEX_FILE_NAME, logOffset);

    if (eventType == EVENT_TYPE_WARNING) {
        return;
    }

    writeToIndex(LOG_ERROR_INDEX_FILE_NAME, logOffset);
}

static void readEvent(Event &event) {
    char filePath[MAX_PATH_LENGTH];

    uint32_t logOffset;

    {
        strcpy(filePath, LOGS_DIR);
        strcat(filePath, PATH_SEPARATOR);
        strcat(filePath, LOG_DEBUG_INDEX_FILE_NAME);

        File file;
        if (!file.open(filePath, FILE_OPEN_EXISTING | FILE_READ)) {
            return;
        }

        file.seek(4 * (g_numEvents - 1 - event.eventIndex));

        bool result = file.read(&logOffset, sizeof(uint32_t)) == sizeof(uint32_t);

        file.close();

        if (!result) {
            return;
        }
    }

    int eventType = EVENT_TYPE_NONE;

    strcpy(filePath, LOGS_DIR);
    strcat(filePath, PATH_SEPARATOR);
    strcat(filePath, LOG_FILE_NAME);

    File file;
    if (!file.open(filePath, FILE_OPEN_EXISTING | FILE_READ)) {
        return;
    }

    file.seek(logOffset);

    using namespace sd_card;
    BufferedFileRead bufferedFile(file, 64);

    unsigned int year;
    if (!match(bufferedFile, year)) {
        goto Error;
    }
    if (!match(bufferedFile, '-')) {
        goto Error;
    }
    unsigned int month;
    if (!match(bufferedFile, month)) {
        goto Error;
    }
    if (!match(bufferedFile, '-')) {
        goto Error;
    }
    unsigned int day;
    if (!match(bufferedFile, day)) {
        goto Error;
    }

    unsigned int hour;
    if (!match(bufferedFile, hour)) {
        goto Error;
    }
    if (!match(bufferedFile, ':')) {
        goto Error;
    }
    unsigned int minute;
    if (!match(bufferedFile, minute)) {
        goto Error;
    }
    if (!match(bufferedFile, ':')) {
        goto Error;
    }
    unsigned int second;
    if (!match(bufferedFile, second)) {
        goto Error;
    }

    matchZeroOrMoreSpaces(bufferedFile);

    char eventTypeStr[9];
    matchUntil(bufferedFile, ' ', eventTypeStr, sizeof(eventTypeStr) - 1);
    eventTypeStr[sizeof(eventTypeStr) - 1] = 0;

    for (int i = EVENT_TYPE_DEBUG; i <= EVENT_TYPE_ERROR; i++) {
        if (strcmp(eventTypeStr, EVENT_TYPE_NAMES[i]) == 0) {
            eventType = i;
            break;
        }
    }

    if (eventType == EVENT_TYPE_NONE) {
        goto Error;
    }

    char message[EVENT_MESSAGE_MAX_SIZE];
    matchUntil(bufferedFile, '\n', message, sizeof(message) - 1);
    message[sizeof(message) - 1] = 0;

    event.dateTime = datetime::makeTime(year, month, day, hour, minute, second);
    event.eventType = eventType;
    strcpy(event.message, message);

Error:
    file.close();
}

static Event *getEvent(uint32_t eventIndex) {
    int i = eventIndex % EVENTS_CACHE_SIZE;

    auto &event = g_eventsCache[i];

    if (event.eventIndex != eventIndex) {
        event.eventIndex = eventIndex;
        event.eventType = EVENT_TYPE_UNKNOWN;
        event.message[0] = 0;
    }

    return &event;
}

} // namespace event_queue
} // namespace psu
} // namespace eez
