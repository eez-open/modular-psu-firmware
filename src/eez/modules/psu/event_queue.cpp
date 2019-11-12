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

#include <eez/modules/mcu/eeprom.h>

#include <eez/modules/psu/datetime.h>
#include <eez/modules/psu/event_queue.h>
#include <eez/sound.h>

namespace eez {
namespace psu {
namespace event_queue {

static const uint32_t MAGIC = 0xD8152FC3L;
static const uint16_t VERSION = 6;

static const uint16_t MAX_EVENTS = 200;
static const uint16_t NULL_INDEX = MAX_EVENTS;

static EventQueueHeader g_eventQueue;

static Event g_events[MAX_EVENTS];

static int16_t g_eventsToPush[6];
static uint8_t g_eventsToPushHead = 0;
static const int MAX_EVENTS_TO_PUSH = sizeof(g_eventsToPush) / sizeof(int16_t);

static uint8_t g_pageIndex = 0;

void readHeader() {
    mcu::eeprom::read((uint8_t *)&g_eventQueue, sizeof(EventQueueHeader), mcu::eeprom::EEPROM_EVENT_QUEUE_START_ADDRESS);
}

void writeHeader() {
    if (mcu::eeprom::g_testResult == TEST_OK) {
        mcu::eeprom::write((uint8_t *)&g_eventQueue, sizeof(EventQueueHeader), mcu::eeprom::EEPROM_EVENT_QUEUE_START_ADDRESS);
    }
}

Event *readEvent(uint16_t eventIndex) {
    return &g_events[eventIndex];
}

void writeEvent(uint16_t eventIndex, Event *e) {
    memcpy(&g_events[eventIndex], e, sizeof(Event));

    if (mcu::eeprom::g_testResult == TEST_OK) {
        mcu::eeprom::write((uint8_t *)e, sizeof(Event), mcu::eeprom::EEPROM_EVENT_QUEUE_START_ADDRESS + sizeof(EventQueueHeader) + eventIndex * sizeof(Event));
    }
}

void init() {
    readHeader();

    if (g_eventQueue.magicNumber != MAGIC || g_eventQueue.version != VERSION ||
        g_eventQueue.head >= MAX_EVENTS || g_eventQueue.size > MAX_EVENTS) {
        g_eventQueue.magicNumber = MAGIC;
        g_eventQueue.version = VERSION;
        g_eventQueue.head = 0;
        g_eventQueue.size = 0;
        g_eventQueue.lastErrorEventIndex = NULL_INDEX;

        pushEvent(EVENT_INFO_WELCOME);
    } else {
        // read all events
        mcu::eeprom::read((uint8_t *)g_events, sizeof(g_events), mcu::eeprom::EEPROM_EVENT_QUEUE_START_ADDRESS + sizeof(EventQueueHeader));
    }
}

void doPushEvent(int16_t eventId) {
    Event e;

    e.dateTime = datetime::now();
    e.eventId = eventId;

    writeEvent(g_eventQueue.head, &e);

    if (g_eventQueue.lastErrorEventIndex == g_eventQueue.head) {
        // this event overwrote last error event, therefore:
        g_eventQueue.lastErrorEventIndex = NULL_INDEX;
    }

    int eventType = getEventType(&e);
    if (eventType == EVENT_TYPE_ERROR ||
        (eventType == EVENT_TYPE_WARNING && g_eventQueue.lastErrorEventIndex == NULL_INDEX)) {
        g_eventQueue.lastErrorEventIndex = g_eventQueue.head;
    }

    g_eventQueue.head = (g_eventQueue.head + 1) % MAX_EVENTS;
    if (g_eventQueue.size < MAX_EVENTS) {
        ++g_eventQueue.size;
    }

    writeHeader();

    if (getEventType(&e) == EVENT_TYPE_ERROR) {
        sound::playBeep();
    }
}

void tick() {
    for (int i = 0; i < g_eventsToPushHead; ++i) {
        doPushEvent(g_eventsToPush[i]);
    }
    g_eventsToPushHead = 0;
}

int getNumEvents() {
    return g_eventQueue.size;
}

Event *getEvent(uint16_t index) {
    uint16_t eventIndex = (g_eventQueue.head - (index + 1) + MAX_EVENTS) % MAX_EVENTS;
    return readEvent(eventIndex);
}

Event *getLastErrorEvent() {
    return g_eventQueue.lastErrorEventIndex != NULL_INDEX ? readEvent(g_eventQueue.lastErrorEventIndex) : nullptr;
}

int getEventType(Event *e) {
    if (e->eventId >= EVENT_INFO_START_ID) {
        return EVENT_TYPE_INFO;
    } else if (e->eventId >= EVENT_WARNING_START_ID) {
        return EVENT_TYPE_WARNING;
    } else if (e->eventId != EVENT_TYPE_NONE) {
        return EVENT_TYPE_ERROR;
    } else {
        return EVENT_TYPE_NONE;
    }
}

const char *getEventMessage(Event *e) {
    static char message[35];

    const char *p_message = 0;

    if (e->eventId >= EVENT_INFO_START_ID) {
        switch (e->eventId) {
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
    } else if (e->eventId >= EVENT_WARNING_START_ID) {
        switch (e->eventId) {
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
        switch (e->eventId) {
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
            return SCPI_ErrorTranslate(e->eventId);
        }
    }

    if (p_message) {
        strncpy(message, p_message, sizeof(message) - 1);
        message[sizeof(message) - 1] = 0;
        return message;
    }

    return 0;
}

void pushEvent(int16_t eventId) {
    if (g_eventsToPushHead < MAX_EVENTS_TO_PUSH) {
        g_eventsToPush[g_eventsToPushHead] = eventId;
        ++g_eventsToPushHead;
    } else {
        DebugTrace("MAX_EVENTS_TO_PUSH exceeded");
    }
}

void markAsRead() {
    if (g_eventQueue.lastErrorEventIndex != NULL_INDEX) {
        g_eventQueue.lastErrorEventIndex = NULL_INDEX;
        writeHeader();
    }
}

int getNumPages() {
    return (getNumEvents() + EVENTS_PER_PAGE - 1) / EVENTS_PER_PAGE;
}

int getActivePageNumEvents() {
    if (g_pageIndex < getNumPages() - 1) {
        return EVENTS_PER_PAGE;
    } else {
        return getNumEvents() - (getNumPages() - 1) * EVENTS_PER_PAGE;
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
    if (g_pageIndex < getNumPages() - 1) {
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

} // namespace event_queue
} // namespace psu
} // namespace eez
