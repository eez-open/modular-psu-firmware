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
#include <eez/system.h>
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
#include <eez/gui/gui.h>
#include <eez/gui/widgets/container.h>

#include <eez/modules/psu/gui/psu.h>
#include <eez/modules/psu/gui/data.h>
#include <eez/modules/psu/gui/animations.h>
#endif

namespace eez {
namespace psu {
namespace event_queue {

static const int EVENTS_PER_PAGE = 8;

static const int CONF_EVENT_LINE_WIDTH_PX = 448;

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
    "ERROR"
};

static const int WRITE_QUEUE_MAX_SIZE = 50;
static const size_t EVENT_MESSAGE_MAX_SIZE = 256;

////////////////////////////////////////////////////////////////////////////////

struct QueueEvent {
    uint32_t dateTime;
    int16_t eventId;
    char message[EVENT_MESSAGE_MAX_SIZE];
};
static QueueEvent g_writeQueue[WRITE_QUEUE_MAX_SIZE];
static uint8_t g_writeQueueHead = 0;
static uint8_t g_writeQueueTail = 0;
static bool g_writeQueueFull;
osMutexId(g_writeQueueMutexId);
osMutexDef(g_writeQueueMutex);

////////////////////////////////////////////////////////////////////////////////

static bool g_refreshEvents;

static int g_filter = EVENT_TYPE_INFO;

static uint32_t g_numEvents;

static uint32_t g_displayFromPosition;
static uint32_t g_previousDisplayFromPosition = -1;

static int16_t g_lastErrorEventId;

struct Event {
    uint32_t dateTime;
    int eventType;
    char message[EVENT_MESSAGE_MAX_SIZE];
    bool isLongMessageText;
    uint32_t logOffset;
};
static Event g_events[EVENTS_PER_PAGE];

static int g_selectedEventIndex = -1;

////////////////////////////////////////////////////////////////////////////////

static void addEventToWriteQueue(int16_t eventId, char *message);
static bool getEventFromWriteQueue(QueueEvent *queueEvent);

static void getIndexFilePath(int indexType, char *filePath);
static void getLogFilePath(char *filePath);

static int getEventType(int16_t eventId);

static void setDisplayFromPosition(uint32_t position);

static void refreshEvents();

static void writeEvent(QueueEvent *event);
static void readEvents(uint32_t fromPosition);

static Event *getEvent(uint32_t eventIndex);

////////////////////////////////////////////////////////////////////////////////

void init() {
    g_refreshEvents = true;
    g_writeQueueMutexId = osMutexCreate(osMutex(g_writeQueueMutex));
}

void tick() {
    if (sd_card::isMounted(nullptr)) {
        QueueEvent queueEvent;
        if (getEventFromWriteQueue(&queueEvent)) {
            writeEvent(&queueEvent);
            g_previousDisplayFromPosition = -1;
        }
#if OPTION_DISPLAY
        else {
            if (gui::g_psuAppContext.getActivePageId() == PAGE_ID_EVENT_QUEUE) {
                if (g_refreshEvents) {
                    refreshEvents();
                    g_refreshEvents = false;
                } else {
                    auto fromPosition = g_displayFromPosition;
                    if (fromPosition != g_previousDisplayFromPosition) {
                        readEvents(fromPosition);
                        g_previousDisplayFromPosition = fromPosition;
                    }
                }
            }
        }
#endif
    } else {
        g_refreshEvents = true;
    }
}

void shutdownSave() {
    QueueEvent queueEvent;
    while (getEventFromWriteQueue(&queueEvent)) {
        writeEvent(&queueEvent);
    }
}

int16_t getLastErrorEventId() {
    return g_lastErrorEventId;
}

const char *getEventTypeName(int16_t eventId) {
    return EVENT_TYPE_NAMES[getEventType(eventId)];
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

void pushEvent(int16_t eventId) {
    addEventToWriteQueue(eventId, nullptr);

#if OPTION_ETHERNET
    eez::mcu::ethernet::pushEvent(eventId);
#endif

    if (getEventType(eventId) == EVENT_TYPE_ERROR) {
        g_lastErrorEventId  = eventId;

        sound::playBeep();

#if OPTION_DISPLAY
        if (g_isBooted) {
            eez::psu::gui::psuErrorMessage(0, MakeEventMessageValue(eventId));
        }
#endif
    }
}

void pushDebugTrace(const char *message, size_t messageLength) {
    static char buffer[EVENT_MESSAGE_MAX_SIZE];
    static int bufferIndex = 0;
    
    const char *messageEnd = message + messageLength;
    for (const char *p = message; p < messageEnd; p++) {
        char ch = *p;
        bool isNewLine = ch == '\n';
        if (!isNewLine) {
            buffer[bufferIndex++] = ch;
        }
        if (isNewLine || bufferIndex == EVENT_MESSAGE_MAX_SIZE - 1) {
            if (bufferIndex > 0) {
                buffer[bufferIndex] = 0;
                bufferIndex = 0;

                addEventToWriteQueue(EVENT_DEBUG_TRACE, buffer);
            }
        }
    }
}

void markAsRead() {
    g_lastErrorEventId = EVENT_TYPE_NONE;
}

void moveToTop() {
    setDisplayFromPosition(0);
}

void onEncoder(int counter) {
#if defined(EEZ_PLATFORM_SIMULATOR)
    counter = -counter;
#endif
    int32_t position = g_displayFromPosition + counter;
    if (position < 0) {
        position = 0;
    }
    setDisplayFromPosition(position);
}

////////////////////////////////////////////////////////////////////////////////

static bool getEventFromWriteQueue(QueueEvent *queueEvent) {
    bool result = false;

    if (osMutexWait(g_writeQueueMutexId, 5) == osOK) {
		if (g_writeQueueFull || g_writeQueueTail != g_writeQueueHead) {
			memcpy(queueEvent, &g_writeQueue[g_writeQueueTail], sizeof(QueueEvent));
			g_writeQueueTail = (g_writeQueueTail + 1) % WRITE_QUEUE_MAX_SIZE;
			g_writeQueueFull = false;
			result = true;
		}

		osMutexRelease(g_writeQueueMutexId);
    }

    return result;
}

static void addEventToWriteQueue(int16_t eventId, char *message) {
    if (osMutexWait(g_writeQueueMutexId, 5) == osOK) {
        g_writeQueue[g_writeQueueHead].dateTime = datetime::now();
        g_writeQueue[g_writeQueueHead].eventId = eventId;

        if (message) {
            strcpy(g_writeQueue[g_writeQueueHead].message, message);
        } else {
            g_writeQueue[g_writeQueueHead].message[0] = 0;
        }

        if (g_writeQueueFull) {
            g_writeQueueTail = (g_writeQueueTail + 1) % WRITE_QUEUE_MAX_SIZE;    
        }

        g_writeQueueHead = (g_writeQueueHead + 1) % WRITE_QUEUE_MAX_SIZE;

        if (g_writeQueueHead == g_writeQueueTail) {
            g_writeQueueFull = true;
        }

        osMutexRelease(g_writeQueueMutexId);
    }
}

static void getIndexFilePath(int indexType, char *filePath) {
    strcpy(filePath, LOGS_DIR);

    strcat(filePath, PATH_SEPARATOR);

    if (indexType == EVENT_TYPE_DEBUG) {
        strcat(filePath, LOG_DEBUG_INDEX_FILE_NAME);
    } else if (indexType == EVENT_TYPE_INFO) {
        strcat(filePath, LOG_INFO_INDEX_FILE_NAME);
    } else if (indexType == EVENT_TYPE_WARNING) {
        strcat(filePath, LOG_WARNING_INDEX_FILE_NAME);
    } else {
        strcat(filePath, LOG_ERROR_INDEX_FILE_NAME);
    }
}

static void getLogFilePath(char *filePath) {
    strcpy(filePath, LOGS_DIR);
    strcat(filePath, PATH_SEPARATOR);
    strcat(filePath, LOG_FILE_NAME);
}

static int getFilter() {
    return g_filter;
}

static void setFilter(int filter) {
    persist_conf::setEventQueueFilter(filter);
    g_refreshEvents = true;
}

static uint32_t getEventDateTime(Event *e) {
    return e->dateTime;
}

static int getEventType(int16_t eventId) {
    if (eventId == EVENT_DEBUG_TRACE) {
        return EVENT_TYPE_DEBUG;
    } else if (eventId >= EVENT_INFO_START_ID) {
        return EVENT_TYPE_INFO;
    } else if (eventId >= EVENT_WARNING_START_ID) {
        return EVENT_TYPE_WARNING;
    } else if (eventId != EVENT_TYPE_NONE) {
        return EVENT_TYPE_ERROR;
    } else {
        return EVENT_TYPE_NONE;
    }
}

static int getEventType(Event *e) {
    if (!e) {
        return EVENT_TYPE_NONE;
    }
    return e->eventType;
}

static const char *getEventMessage(Event *e) {
    if (!e) {
        return nullptr;
    }
    return e->message;
}

static bool isLongMessageText(Event *e) {
    if (e) {
        return e->isLongMessageText;
    }
    return false;
}

static void setDisplayFromPosition(uint32_t position) {
    if (position + EVENTS_PER_PAGE > g_numEvents) {
        if (g_numEvents > EVENTS_PER_PAGE) {
            position = g_numEvents - EVENTS_PER_PAGE;
        } else {
            position = 0;
        }
    }

    if (position != g_displayFromPosition) {
        g_displayFromPosition = position;
        g_previousDisplayFromPosition = -1;
        g_selectedEventIndex = -1;
    }
}

static void refreshEvents() {
    g_filter = persist_conf::devConf.eventQueueFilter;
    if (g_filter < EVENT_TYPE_DEBUG || g_filter > EVENT_TYPE_ERROR) {
        g_filter = EVENT_TYPE_INFO;
    }

    g_numEvents = 0;
    g_displayFromPosition = 0;
    g_previousDisplayFromPosition = -1;
    g_selectedEventIndex = -1;

    char filePath[MAX_PATH_LENGTH];
    getIndexFilePath(g_filter, filePath);

    File file;
    if (file.open(filePath, FILE_OPEN_EXISTING | FILE_READ)) {
        g_numEvents = file.size() / 4;
        file.close();
    }

    if (g_numEvents == 0) {
        pushEvent(EVENT_INFO_WELCOME);
    }
}

static bool writeToLog(QueueEvent *event, uint32_t &logOffset, int &eventType) {
    char filePath[MAX_PATH_LENGTH];
    getLogFilePath(filePath);

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

    if (event->eventId == EVENT_DEBUG_TRACE) {
        bufferedFile.write((const uint8_t *)event->message, strlen(event->message));
    } else {
        const char *message = getEventMessage(event->eventId);
        bufferedFile.write((const uint8_t *)message, strlen(message));
    }

    bufferedFile.write((const uint8_t *)"\n", 1);

    bufferedFile.flush();

    file.close();

    return true;
}

static void writeToIndex(int indexType, uint32_t logOffset) {
    char filePath[MAX_PATH_LENGTH];
    getIndexFilePath(indexType, filePath);

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

    writeToIndex(EVENT_TYPE_DEBUG, logOffset);

    if (eventType == EVENT_TYPE_DEBUG) {
        return;
    }

    writeToIndex(EVENT_TYPE_INFO, logOffset);

    if (eventType == EVENT_TYPE_INFO) {
        return;
    }

    writeToIndex(EVENT_TYPE_WARNING, logOffset);

    if (eventType == EVENT_TYPE_WARNING) {
        return;
    }

    writeToIndex(EVENT_TYPE_ERROR, logOffset);
}

static void getEventInfoText(Event *e, char *text, int count) {
    int year, month, day, hour, minute, second;
    datetime::breakTime(getEventDateTime(e), year, month, day, hour, minute, second);

    int yearNow, monthNow, dayNow, hourNow, minuteNow, secondNow;
    datetime::breakTime(datetime::now(), yearNow, monthNow, dayNow, hourNow, minuteNow, secondNow);

    if (yearNow == year && monthNow == month && dayNow == day) {
        if (persist_conf::devConf.dateTimeFormat == datetime::FORMAT_DMY_24 || persist_conf::devConf.dateTimeFormat == datetime::FORMAT_MDY_24) {
            snprintf(text, count - 1, "%c [%02d:%02d:%02d] %s", 127 + getEventType(e) - EVENT_TYPE_DEBUG, hour, minute, second, getEventMessage(e));
        } else {
            bool am;
            datetime::convertTime24to12(hour, am);
            snprintf(text, count - 1, "%c [%02d:%02d:%02d %s] %s", 127 + getEventType(e) - EVENT_TYPE_DEBUG, hour, minute, second, am ? "AM" : "PM", getEventMessage(e));
        }
    } else {
        if (persist_conf::devConf.dateTimeFormat == datetime::FORMAT_DMY_24 || persist_conf::devConf.dateTimeFormat == datetime::FORMAT_DMY_12) {
            snprintf(text, count - 1, "%c [%02d-%02d-%02d] %s", 127 + getEventType(e) - EVENT_TYPE_DEBUG, day, month, year % 100, getEventMessage(e));
        } else {
            snprintf(text, count - 1, "%c [%02d-%02d-%02d] %s", 127 + getEventType(e) - EVENT_TYPE_DEBUG, month, day, year % 100, getEventMessage(e));
        }
    }

    text[count - 1] = 0;
}

static bool readEvent(File &indexFile, File &logFile, int eventIndex, Event &event) {
    indexFile.seek(4 * (g_numEvents - 1 - eventIndex));
    uint32_t logOffset;
    if (indexFile.read(&logOffset, sizeof(uint32_t)) != sizeof(uint32_t)) {
        return false;
    }

    logFile.seek(logOffset);
    using namespace sd_card;
    BufferedFileRead bufferedFile(logFile, 64);

    unsigned int year;
    if (!match(bufferedFile, year)) {
        return false;
    }
    if (!match(bufferedFile, '-')) {
        return false;
    }
    unsigned int month;
    if (!match(bufferedFile, month)) {
        return false;
    }
    if (!match(bufferedFile, '-')) {
        return false;
    }
    unsigned int day;
    if (!match(bufferedFile, day)) {
        return false;
    }

    unsigned int hour;
    if (!match(bufferedFile, hour)) {
        return false;
    }
    if (!match(bufferedFile, ':')) {
        return false;
    }
    unsigned int minute;
    if (!match(bufferedFile, minute)) {
        return false;
    }
    if (!match(bufferedFile, ':')) {
        return false;
    }
    unsigned int second;
    if (!match(bufferedFile, second)) {
        return false;
    }

    matchZeroOrMoreSpaces(bufferedFile);

    char eventTypeStr[9];
    matchUntil(bufferedFile, ' ', eventTypeStr, sizeof(eventTypeStr) - 1);
    eventTypeStr[sizeof(eventTypeStr) - 1] = 0;

    int eventType = EVENT_TYPE_NONE;

    for (int i = EVENT_TYPE_DEBUG; i <= EVENT_TYPE_ERROR; i++) {
        if (strcmp(eventTypeStr, EVENT_TYPE_NAMES[i]) == 0) {
            eventType = i;
            break;
        }
    }

    if (eventType == EVENT_TYPE_NONE) {
        return false;
    }

    char message[EVENT_MESSAGE_MAX_SIZE];
    matchUntil(bufferedFile, '\n', message, sizeof(message) - 1);
    message[sizeof(message) - 1] = 0;

    event.dateTime = datetime::makeTime(year, month, day, hour, minute, second);
    event.eventType = eventType;
    strcpy(event.message, message);

    char text[256];
    getEventInfoText(&event, text, sizeof(text));
    eez::gui::font::Font font(getFontData(FONT_ID_OSWALD14));
    event.isLongMessageText = mcu::display::measureStr(text, -1, font) > CONF_EVENT_LINE_WIDTH_PX;
    event.logOffset = logOffset;

    return true;
}

static void readEvents(uint32_t fromPosition) {
    char filePath[MAX_PATH_LENGTH];
    getIndexFilePath(g_filter, filePath);
    File indexFile;
    if (indexFile.open(filePath, FILE_OPEN_EXISTING | FILE_READ)) {
        getLogFilePath(filePath);
        File logFile;
        if (logFile.open(filePath, FILE_OPEN_EXISTING | FILE_READ)) {
            for (int i = 0; i < EVENTS_PER_PAGE; i++) {
                auto &event = g_events[i];
                if (fromPosition + i < g_numEvents) {
                    if (readEvent(indexFile, logFile, fromPosition + i, event)) {
                        continue;
                    }
                }
                memset(&event, 0, sizeof(event));
            }
            logFile.close();
        }
        indexFile.close();
    }
}

static Event *getEvent(uint32_t eventIndex) {
    return &g_events[(eventIndex - g_displayFromPosition) % EVENTS_PER_PAGE];
}

static void toggleSelectedEvent(int eventIndex) {
    if (g_selectedEventIndex == eventIndex) {
        g_selectedEventIndex = -1;
    } else {
        g_selectedEventIndex = eventIndex;
    }
}

static Event *getSelectedEvent() {
    return &g_events[g_selectedEventIndex - g_displayFromPosition];
}

} // namespace event_queue
} // namespace psu

namespace gui {

using namespace psu;
using namespace psu::event_queue;
using namespace psu::gui;

static event_queue::Event *getEventFromValue(const Value &value) {
    for (int i = 0; i < EVENTS_PER_PAGE; i++) {
        if (g_events[i].logOffset == value.getUInt32()) {
            return &g_events[i];
        }
    }
    return nullptr;
}

bool compareEventValues(const Value &a, const Value &b) {
    return a.getUInt32() == b.getUInt32();
}

void eventValueToText(const Value &value, char *text, int count) {
    auto event = getEventFromValue(value);
    if (event) {
        getEventInfoText(event, text, count);
    } else {
        text[0] = 0;
    }
}

Value MakeEventValue(event_queue::Event *e) {
    Value value;
    value.type_ = VALUE_TYPE_EVENT;
    value.options_ = 0;
    value.unit_ = UNIT_UNKNOWN;
    value.uint32_ = e->logOffset;
    return value;
}

void data_event_queue_last_event_type(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        value = data::Value(getEventType(getLastErrorEventId()));
    }
}

void data_event_queue_events(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_COUNT) {
        value = (int)g_numEvents;
    } else if (operation == data::DATA_OPERATION_YT_DATA_GET_SIZE) {
        value = Value(g_numEvents, VALUE_TYPE_UINT32);
    } else if (operation == data::DATA_OPERATION_YT_DATA_GET_POSITION) {
        value = Value(g_displayFromPosition, VALUE_TYPE_UINT32);
    } else if (operation == data::DATA_OPERATION_YT_DATA_SET_POSITION) {
        setDisplayFromPosition(value.getUInt32());
    } else if (operation == data::DATA_OPERATION_YT_DATA_GET_PAGE_SIZE) {
        value = Value(EVENTS_PER_PAGE, VALUE_TYPE_UINT32);
    }
}

void data_event_queue_event_type(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        event_queue::Event *event = getEvent(cursor.i);
        value = data::Value(event ? getEventType(event) : EVENT_TYPE_NONE);
    }
}

void data_event_queue_event_message(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        value = MakeEventValue(getEvent(cursor.i));
    }
}

void data_event_queue_is_long_message_text(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        event_queue::Event *event = getEvent(cursor.i);
        value = isLongMessageText(event);
    }
}

void data_event_queue_event_is_selected(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        event_queue::Event *event = getEvent(cursor.i);
        event_queue::Event *selectedEvent = getSelectedEvent();
        value = event == selectedEvent;
    }
}

void data_event_queue_selected_event_message(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        event_queue::Event *selectedEvent = getSelectedEvent();
        if (selectedEvent) {
            value = getEventMessage(selectedEvent);
        }
    }
}

void data_event_queue_event_long_message_overlay(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    static const int NUM_WIDGETS = 1;

    static const int MULTI_LINE_TEXT_WIDGET = 0;

    static Overlay overlay;
    static WidgetOverride widgetOverrides[NUM_WIDGETS];

    if (operation == data::DATA_OPERATION_GET_OVERLAY_DATA) {
        value = data::Value(&overlay, VALUE_TYPE_POINTER);
    } else if (operation == data::DATA_OPERATION_UPDATE_OVERLAY_DATA) {
        overlay.widgetOverrides = widgetOverrides;

        int selectedEventIndexWithinPage = g_selectedEventIndex != -1 ? g_selectedEventIndex - g_displayFromPosition : -1;
        int state = selectedEventIndexWithinPage + 1;

        if (overlay.state != state) {
            overlay.state = state;
            if (state) {
                event_queue::Event *selectedEvent = getSelectedEvent();

                WidgetCursor &widgetCursor = *(WidgetCursor *)value.getVoidPointer();

                const ContainerWidget *containerWidget = GET_WIDGET_PROPERTY(widgetCursor.widget, specific, const ContainerWidget *);

                const Widget *multiLineTextWidget = GET_WIDGET_LIST_ELEMENT(containerWidget->widgets, MULTI_LINE_TEXT_WIDGET);

                static const int CONF_EVENT_LINE_HEIGHT_PX = 30;
                static const int CONF_EVENTS_LIST_HEIGHT_PX = 240;

                auto style = getStyle(multiLineTextWidget->style);
                int height = measureMultilineText(
                    getEventMessage(selectedEvent), 
                    0, 0, multiLineTextWidget->w, CONF_EVENTS_LIST_HEIGHT_PX,
                    style, 0, 0
                ) + style->padding_top + style->padding_bottom + style->border_size_top + style->border_size_bottom;
                
                int y = selectedEventIndexWithinPage * CONF_EVENT_LINE_HEIGHT_PX;
                if (y + height > CONF_EVENTS_LIST_HEIGHT_PX) {
                    y = CONF_EVENTS_LIST_HEIGHT_PX - height;
                }

                overlay.yOffset = y;

                overlay.width = widgetCursor.widget->w;
                overlay.height = height;

                widgetOverrides[MULTI_LINE_TEXT_WIDGET].isVisible = true;
                widgetOverrides[MULTI_LINE_TEXT_WIDGET].x = 0;
                widgetOverrides[MULTI_LINE_TEXT_WIDGET].y = 0;
                widgetOverrides[MULTI_LINE_TEXT_WIDGET].w = overlay.width;
                widgetOverrides[MULTI_LINE_TEXT_WIDGET].h = height;
            }
        }

        value = data::Value(&overlay, VALUE_TYPE_POINTER);
    }
}

void onSetEventQueueFilter(uint16_t value) {
    popPage();
    event_queue::setFilter((int)value);
}

void action_event_queue_filter() {
    pushSelectFromEnumPage(g_eventQueueFilterEnumDefinition, (uint16_t)event_queue::getFilter(), NULL, onSetEventQueueFilter);
}

void action_event_queue_select_event() {
    event_queue::toggleSelectedEvent(getFoundWidgetAtDown().cursor.i);
}

} // namespace gui

} // namespace eez
