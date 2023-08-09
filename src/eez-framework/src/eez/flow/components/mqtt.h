/*
* EEZ Framework
* Copyright (C) 2023-present, Envox d.o.o.
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

#ifdef __cplusplus
extern "C" {
#endif

//
// These functions must be implemented by the MQTT library adapter.
// Define EEZ_MQTT_ADAPTER if implementation is available.
//

#define MQTT_ERROR_OK 0
#define MQTT_ERROR_OTHER 1
#define MQTT_ERROR_NOT_IMPLEMENTED 2

int eez_mqtt_init(const char *protocol, const char *host, int port, const char *username, const char *password, void **handle);
int eez_mqtt_deinit(void *handle);
int eez_mqtt_connect(void *handle);
int eez_mqtt_disconnect(void *handle);
int eez_mqtt_subscribe(void *handle, const char *topic);
int eez_mqtt_unsubscribe(void *handle, const char *topic);
int eez_mqtt_publish(void *handle, const char *topic, const char *payload);

//
// The following function is implemented inside EEZ Framework and should be called by the MQTT library adapter
//

typedef enum {
    EEZ_MQTT_EVENT_CONNECT = 0, // eventData is null
    EEZ_MQTT_EVENT_RECONNECT = 1, // eventData is null
    EEZ_MQTT_EVENT_CLOSE = 2, // eventData is null
    EEZ_MQTT_EVENT_DISCONNECT = 3, // eventData is null
    EEZ_MQTT_EVENT_OFFLINE = 4, // eventData is null
    EEZ_MQTT_EVENT_END = 5, // eventData is null
    EEZ_MQTT_EVENT_ERROR = 6, // eventData is char *, i.e. error message
    EEZ_MQTT_EVENT_MESSAGE = 7 // eventData is EEZ_MQTT_MessageEvent *
} EEZ_MQTT_Event;

typedef struct {
    const char *topic;
    const char *payload;
} EEZ_MQTT_MessageEvent;

void eez_mqtt_on_event_callback(void *handle, EEZ_MQTT_Event event, void *eventData);

#ifdef __cplusplus
}
#endif
