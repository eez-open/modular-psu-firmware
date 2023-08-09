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

#include <stdio.h>

#include <eez/conf-internal.h>

#include <eez/core/debug.h>

#include <eez/flow/flow.h>
#include <eez/flow/components.h>
#include <eez/flow/flow_defs_v3.h>
#include <eez/flow/expression.h>
#include <eez/flow/debugger.h>
#include <eez/flow/queue.h>

#include <eez/flow/components/mqtt.h>

namespace eez {
namespace flow {

////////////////////////////////////////////////////////////////////////////////

struct MQTTEventActionComponenent : public Component {
    int16_t connectEventOutputIndex;
    int16_t reconnectEventOutputIndex;
    int16_t closeEventOutputIndex;
    int16_t disconnectEventOutputIndex;
    int16_t offlineEventOutputIndex;
    int16_t endEventOutputIndex;
    int16_t errorEventOutputIndex;
    int16_t messageEventOutputIndex;
};

struct MQTTEvent {
    int16_t outputIndex;
    Value value;
    MQTTEvent *next;
};

struct MQTTEventActionComponenentExecutionState : public ComponenentExecutionState {
	FlowState *flowState;
    unsigned componentIndex;
    MQTTEvent *firstEvent;
    MQTTEvent *lastEvent;

    MQTTEventActionComponenentExecutionState() : firstEvent(nullptr), lastEvent(nullptr) {}

    virtual ~MQTTEventActionComponenentExecutionState() override;

    void addEvent(int16_t outputIndex, Value value = Value(VALUE_TYPE_NULL)) {
        auto event = ObjectAllocator<MQTTEvent>::allocate(0xe1b95933);
        event->outputIndex = outputIndex;
        event->value = value;
        event->next = nullptr;
        if (!firstEvent) {
            firstEvent = event;
            lastEvent = event;
        } else {
            lastEvent->next = event;
            lastEvent = event;
        }
    }

    MQTTEvent *removeEvent() {
        auto event = firstEvent;
        if (event) {
            firstEvent = event->next;
            if (!firstEvent) {
                lastEvent = nullptr;
            }
        }
        return event;
    }
};

////////////////////////////////////////////////////////////////////////////////

struct MQTTConnectionEventHandler {
    MQTTEventActionComponenentExecutionState *componentExecutionState;

    MQTTConnectionEventHandler *next;
    MQTTConnectionEventHandler *prev;
};

struct MQTTConnection {
    void *handle;

    MQTTConnectionEventHandler *firstEventHandler;
    MQTTConnectionEventHandler *lastEventHandler;

    MQTTConnection *next;
    MQTTConnection *prev;
};

MQTTConnection *g_firstMQTTConnection = nullptr;
MQTTConnection *g_lastMQTTConnection = nullptr;

////////////////////////////////////////////////////////////////////////////////

static MQTTConnection *addConnection(void *handle) {
    auto connection = ObjectAllocator<MQTTConnection>::allocate(0x95d9f5d1);
    if (!connection) {
        return nullptr;
    }

    connection->handle = handle;
    connection->firstEventHandler = nullptr;
    connection->lastEventHandler = nullptr;

    if (!g_firstMQTTConnection) {
        g_firstMQTTConnection = connection;
        g_lastMQTTConnection = connection;
        connection->prev = nullptr;
        connection->next = nullptr;
    } else {
        g_lastMQTTConnection->next = connection;
        connection->prev = g_lastMQTTConnection;
        connection->next = nullptr;
        g_lastMQTTConnection = connection;
    }

    return connection;
}

static MQTTConnection *findConnection(void *handle) {
    for (auto connection = g_firstMQTTConnection; connection; connection = connection->next) {
        if (connection->handle == handle) {
            return connection;
        }
    }
    return nullptr;
}

static void deleteConnection(void *handle) {
    auto connection = findConnection(handle);
    if (!connection) {
        return;
    }

    while (connection->firstEventHandler) {
        deallocateComponentExecutionState(
            connection->firstEventHandler->componentExecutionState->flowState,
            connection->firstEventHandler->componentExecutionState->componentIndex
        );
    }

    eez_mqtt_deinit(connection->handle);

    if (connection->prev) {
        connection->prev->next = connection->next;
    } else {
        g_firstMQTTConnection = connection->next;
    }
    if (connection->next) {
        connection->next->prev = connection->prev;
    } else {
        g_lastMQTTConnection = connection->prev;
    }

    ObjectAllocator<MQTTConnection>::deallocate(connection);
}

MQTTConnectionEventHandler *addConnectionEventHandler(void *handle, MQTTEventActionComponenentExecutionState *componentExecutionState) {
    auto connection = findConnection(handle);
    if (!connection) {
        return nullptr;
    }

    auto eventHandler = ObjectAllocator<MQTTConnectionEventHandler>::allocate(0x75ccf1eb);
    if (!eventHandler) {
        return nullptr;
    }

    eventHandler->componentExecutionState = componentExecutionState;

    if (!connection->firstEventHandler) {
        connection->firstEventHandler = eventHandler;
        connection->lastEventHandler = eventHandler;
        eventHandler->prev = nullptr;
        eventHandler->next = nullptr;
    } else {
        connection->lastEventHandler->next = eventHandler;
        eventHandler->prev = connection->lastEventHandler;
        eventHandler->next = nullptr;
        connection->lastEventHandler = eventHandler;
    }

    return eventHandler;
}

static void removeEventHandler(MQTTEventActionComponenentExecutionState *componentExecutionState) {
    for (auto connection = g_firstMQTTConnection; connection; connection = connection->next) {
        for (auto eventHandler = connection->firstEventHandler; eventHandler; eventHandler = eventHandler->next) {
            if (eventHandler->componentExecutionState == componentExecutionState) {
                if (eventHandler->prev) {
                    eventHandler->prev->next = eventHandler->next;
                } else {
                    connection->firstEventHandler = eventHandler->next;
                }

                if (eventHandler->next) {
                    eventHandler->next->prev = eventHandler->prev;
                } else {
                    connection->lastEventHandler = eventHandler->prev;
                }

                ObjectAllocator<MQTTConnectionEventHandler>::deallocate(eventHandler);
                return;
            }
        }
    }
}

void eez_mqtt_on_event_callback(void *handle, EEZ_MQTT_Event event, void *eventData) {
    auto connection = findConnection(handle);
    if (!connection) {
        return;
    }

    for (auto eventHandler = connection->firstEventHandler; eventHandler; eventHandler = eventHandler->next) {
        auto componentExecutionState = eventHandler->componentExecutionState;

        auto flowState = componentExecutionState->flowState;
        auto componentIndex = componentExecutionState->componentIndex;

        auto component = (MQTTEventActionComponenent *)flowState->flow->components[componentIndex];

        if (event == EEZ_MQTT_EVENT_CONNECT) {
            if (component->connectEventOutputIndex >= 0) {
                componentExecutionState->addEvent(component->connectEventOutputIndex);
            }
        } else if (event == EEZ_MQTT_EVENT_RECONNECT) {
            if (component->reconnectEventOutputIndex >= 0) {
                componentExecutionState->addEvent(component->reconnectEventOutputIndex);
            }
        } else if (event == EEZ_MQTT_EVENT_CLOSE) {
            if (component->closeEventOutputIndex >= 0) {
                componentExecutionState->addEvent(component->closeEventOutputIndex);
            }
        } else if (event == EEZ_MQTT_EVENT_DISCONNECT) {
            if (component->disconnectEventOutputIndex >= 0) {
                componentExecutionState->addEvent(component->disconnectEventOutputIndex);
            }
        } else if (event == EEZ_MQTT_EVENT_OFFLINE) {
            if (component->offlineEventOutputIndex >= 0) {
                componentExecutionState->addEvent(component->offlineEventOutputIndex);
            }
        } else if (event == EEZ_MQTT_EVENT_END) {
            if (component->endEventOutputIndex >= 0) {
                componentExecutionState->addEvent(component->endEventOutputIndex);
            }
        } else if (event == EEZ_MQTT_EVENT_ERROR) {
            if (component->errorEventOutputIndex >= 0) {
                componentExecutionState->addEvent(component->errorEventOutputIndex, Value::makeStringRef((const char *)eventData, -1, 0x2b7ac31a));
            }
        } else if (event == EEZ_MQTT_EVENT_MESSAGE) {
            if (component->messageEventOutputIndex >= 0) {
                auto messageEvent = (EEZ_MQTT_MessageEvent *)eventData;

                Value messageValue = Value::makeArrayRef(defs_v3::SYSTEM_STRUCTURE_MQTT_MESSAGE_NUM_FIELDS, defs_v3::SYSTEM_STRUCTURE_MQTT_MESSAGE, 0xe256716a);
                auto messageArray = messageValue.getArray();
                messageArray->values[defs_v3::SYSTEM_STRUCTURE_MQTT_MESSAGE_FIELD_TOPIC] = Value::makeStringRef(messageEvent->topic, -1, 0x5bdff567);
                messageArray->values[defs_v3::SYSTEM_STRUCTURE_MQTT_MESSAGE_FIELD_PAYLOAD] = Value::makeStringRef(messageEvent->payload, -1, 0xcfa25e4f);

                componentExecutionState->addEvent(component->messageEventOutputIndex, messageValue);
            }
        }
    }
}

void onFreeMQTTConnection(ArrayValue *mqttConnectionValue) {
    void *handle = mqttConnectionValue->values[defs_v3::OBJECT_TYPE_MQTT_CONNECTION_FIELD_ID].getVoidPointer();
    deleteConnection(handle);
}

MQTTEventActionComponenentExecutionState::~MQTTEventActionComponenentExecutionState() {
    removeEventHandler(this);

    while (firstEvent) {
        auto event = removeEvent();
        ObjectAllocator<MQTTEvent>::deallocate(event);
    }
}

////////////////////////////////////////////////////////////////////////////////

void executeMQTTInitComponent(FlowState *flowState, unsigned componentIndex) {
    Value connectionDstValue;
    if (!evalAssignableProperty(flowState, componentIndex, defs_v3::MQTT_INIT_ACTION_COMPONENT_PROPERTY_CONNECTION, connectionDstValue, "Failed to evaluate Connection in MQTTInit")) {
        return;
    }

    Value protocolValue;
    if (!evalProperty(flowState, componentIndex, defs_v3::MQTT_INIT_ACTION_COMPONENT_PROPERTY_PROTOCOL, protocolValue, "Failed to evaluate Protocol in MQTTInit")) {
        return;
    }
    if (!protocolValue.isString()) {
        throwError(flowState, componentIndex, "Protocol must be a string");
        return;
    }

    Value hostValue;
    if (!evalProperty(flowState, componentIndex, defs_v3::MQTT_INIT_ACTION_COMPONENT_PROPERTY_HOST, hostValue, "Failed to evaluate Host in MQTTInit")) {
        return;
    }
    if (!hostValue.isString()) {
        throwError(flowState, componentIndex, "Host must be a string");
        return;
    }

    Value portValue;
    if (!evalProperty(flowState, componentIndex, defs_v3::MQTT_INIT_ACTION_COMPONENT_PROPERTY_PORT, portValue, "Failed to evaluate Port in MQTTInit")) {
        return;
    }
    if (portValue.getType() != VALUE_TYPE_INT32) {
        throwError(flowState, componentIndex, "Port must be an integer");
        return;
    }

    Value usernameValue;
    if (!evalProperty(flowState, componentIndex, defs_v3::MQTT_INIT_ACTION_COMPONENT_PROPERTY_USER_NAME, usernameValue, "Failed to evaluate Username in MQTTInit")) {
        return;
    }
    if (usernameValue.getType() != VALUE_TYPE_UNDEFINED && !usernameValue.isString()) {
        throwError(flowState, componentIndex, "Username must be a string");
        return;
    }

    Value passwordValue;
    if (!evalProperty(flowState, componentIndex, defs_v3::MQTT_INIT_ACTION_COMPONENT_PROPERTY_PASSWORD, passwordValue, "Failed to evaluate Password in MQTTInit")) {
        return;
    }
    if (passwordValue.getType() != VALUE_TYPE_UNDEFINED && !passwordValue.isString()) {
        throwError(flowState, componentIndex, "Password must be a string");
        return;
    }

    void *handle;
    auto result = eez_mqtt_init(protocolValue.getString(), hostValue.getString(), portValue.getInt32(), usernameValue.getString(), passwordValue.getString(), &handle);
    if (result != MQTT_ERROR_OK) {
        char errorMessage[256];
        snprintf(errorMessage, sizeof(errorMessage), "Failed to initialize MQTT connection with error code: %d", (int)result);
        throwError(flowState, componentIndex, errorMessage);
        return;
    }

    addConnection(handle);

    Value connectionValue = Value::makeArrayRef(defs_v3::OBJECT_TYPE_MQTT_CONNECTION_NUM_FIELDS, defs_v3::OBJECT_TYPE_MQTT_CONNECTION, 0x51ba2203);
    auto connectionArray = connectionValue.getArray();
    connectionArray->values[defs_v3::OBJECT_TYPE_MQTT_CONNECTION_FIELD_PROTOCOL] = protocolValue;
    connectionArray->values[defs_v3::OBJECT_TYPE_MQTT_CONNECTION_FIELD_HOST] = hostValue;
    connectionArray->values[defs_v3::OBJECT_TYPE_MQTT_CONNECTION_FIELD_PORT] = portValue;
    connectionArray->values[defs_v3::OBJECT_TYPE_MQTT_CONNECTION_FIELD_USER_NAME] = usernameValue;
    connectionArray->values[defs_v3::OBJECT_TYPE_MQTT_CONNECTION_FIELD_PASSWORD] = passwordValue;
    connectionArray->values[defs_v3::OBJECT_TYPE_MQTT_CONNECTION_FIELD_IS_CONNECTED] = Value(false, VALUE_TYPE_BOOLEAN);
    connectionArray->values[defs_v3::OBJECT_TYPE_MQTT_CONNECTION_FIELD_ID] = Value(handle, VALUE_TYPE_POINTER);

    Value statusValue = Value::makeArrayRef(defs_v3::SYSTEM_STRUCTURE_OBJECT_VARIABLE_STATUS_NUM_FIELDS, defs_v3::SYSTEM_STRUCTURE_OBJECT_VARIABLE_STATUS, 0x51ba2203);
    auto statusArray = statusValue.getArray();
    statusArray->values[defs_v3::SYSTEM_STRUCTURE_OBJECT_VARIABLE_STATUS_FIELD_LABEL] = Value("", VALUE_TYPE_STRING);
    statusArray->values[defs_v3::SYSTEM_STRUCTURE_OBJECT_VARIABLE_STATUS_FIELD_IMAGE] = Value("data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAABgAAAAYCAYAAADgdz34AAAAAXNSR0IArs4c6QAAAARnQU1BAACxjwv8YQUAAAAJcEhZcwAADsIAAA7CARUoSoAAAAItSURBVEhL1dZNSBVRGMbxmcjCiFz14aJVlKtcVFjUJghCIlq5aBHoQiwoyJaRCzdRUUhQUIuIyKiEdoLuolCQEgQzyk8KjTZhidEnwe3/nJl3ermc8m5c9MCPmTtn5rz3zMw596alUilZzqzIt8uWYgRpmp5ncxQ/4YelL/EJ9/AQ87CkOIMT4VOS/MIq9NDvuXBEBfIiL/VxCW/Qjmr4tMG+mMwU/boCW7ETB3EKdzEFu8h7jr3w0XUandrHYwViWYdDeAT/DeUbTsPnAL5jOlZAw9b9+1v2oB++iFyGzzFER9CHEfTiAhpRg/KcxGf4Ilfh0xIrMAN/kUyjE7Xw2Yc5+HM7UCRWQJ35C7x3aIVPPWbhzzuCkFgBPczj6MIQyh+q3MEaWBqgOWLteo03IVrAR5NrF26jvJCe1VpYmuHbr2PJAj778Qq+kx74ZeYBrO0r6mMF9CbcQAs264CLHvIgrBM5C8s2LMDabsUK6P7ZCR9wCf413YgXsHMWsR2Wa7C2iViBcXeCGcYWWHbjC6y9G5Yd0CzW8ehEixWQUWyA5QqsTROuDopW1gHoeFGgkt8Dve8Xs92Qm9DtUfQ2Hc52Q8ePs90/qfQHR+uLXltFE/JpthuiBc7yLN9qNCGVFqhCU7Yb8iTfKrpFWnWVt/iBaAF18q9o/bFM5FtFr/D6bDf5CBUoVmVf4D40fD3s12V0XPd9NZT30OSbzK2Eokk2Bk28kP/9X0WS/AaVCm1sgeHGuwAAAABJRU5ErkJggg==", VALUE_TYPE_STRING);
    statusArray->values[defs_v3::SYSTEM_STRUCTURE_OBJECT_VARIABLE_STATUS_FIELD_COLOR] = Value("gray", VALUE_TYPE_STRING);
    statusArray->values[defs_v3::SYSTEM_STRUCTURE_OBJECT_VARIABLE_STATUS_FIELD_ERROR] = Value();

    connectionArray->values[defs_v3::OBJECT_TYPE_MQTT_CONNECTION_FIELD_STATUS] = statusValue;

    assignValue(flowState, componentIndex, connectionDstValue, connectionValue);

	propagateValueThroughSeqout(flowState, componentIndex);
}

////////////////////////////////////////////////////////////////////////////////

void executeMQTTConnectComponent(FlowState *flowState, unsigned componentIndex) {
    Value connectionValue;
    if (!evalProperty(flowState, componentIndex, defs_v3::MQTT_CONNECT_ACTION_COMPONENT_PROPERTY_CONNECTION, connectionValue, "Failed to evaluate Connection in MQTTEvent")) {
        return;
    }
    if (!connectionValue.isArray() || connectionValue.getArray()->arrayType != defs_v3::OBJECT_TYPE_MQTT_CONNECTION) {
        throwError(flowState, componentIndex, "Connection must be a object:MQTTConnection");
        return;
    }

    auto connectionArray = connectionValue.getArray();
    void *handle = connectionArray->values[defs_v3::OBJECT_TYPE_MQTT_CONNECTION_FIELD_ID].getVoidPointer();

    auto result = eez_mqtt_connect(handle);
    if (result != MQTT_ERROR_OK) {
        char errorMessage[256];
        snprintf(errorMessage, sizeof(errorMessage), "Failed to connect to MQTT broker with error code: %d", (int)result);
        throwError(flowState, componentIndex, errorMessage);
        return;
    }

	propagateValueThroughSeqout(flowState, componentIndex);
}

////////////////////////////////////////////////////////////////////////////////

void executeMQTTDisconnectComponent(FlowState *flowState, unsigned componentIndex) {
    Value connectionValue;
    if (!evalProperty(flowState, componentIndex, defs_v3::MQTT_DISCONNECT_ACTION_COMPONENT_PROPERTY_CONNECTION, connectionValue, "Failed to evaluate Connection in MQTTDisconnect")) {
        return;
    }
    if (!connectionValue.isArray() || connectionValue.getArray()->arrayType != defs_v3::OBJECT_TYPE_MQTT_CONNECTION) {
        throwError(flowState, componentIndex, "Connection must be a object:MQTTConnection");
        return;
    }

    auto connectionArray = connectionValue.getArray();
    void *handle = connectionArray->values[defs_v3::OBJECT_TYPE_MQTT_CONNECTION_FIELD_ID].getVoidPointer();

    auto result = eez_mqtt_disconnect(handle);
    if (result != MQTT_ERROR_OK) {
        char errorMessage[256];
        snprintf(errorMessage, sizeof(errorMessage), "Failed to disconnect from MQTT broker with error code: %d", (int)result);
        throwError(flowState, componentIndex, errorMessage);
        return;
    }

	propagateValueThroughSeqout(flowState, componentIndex);
}

////////////////////////////////////////////////////////////////////////////////

void executeMQTTEventComponent(FlowState *flowState, unsigned componentIndex) {
    Value connectionValue;
    if (!evalProperty(flowState, componentIndex, defs_v3::MQTT_EVENT_ACTION_COMPONENT_PROPERTY_CONNECTION, connectionValue, "Failed to evaluate Connection in MQTTEvent")) {
        return;
    }
    if (!connectionValue.isArray() || connectionValue.getArray()->arrayType != defs_v3::OBJECT_TYPE_MQTT_CONNECTION) {
        throwError(flowState, componentIndex, "Connection must be a object:MQTTConnection");
        return;
    }

    auto componentExecutionState = (MQTTEventActionComponenentExecutionState *)flowState->componenentExecutionStates[componentIndex];
    if (!componentExecutionState) {
        componentExecutionState = allocateComponentExecutionState<MQTTEventActionComponenentExecutionState>(flowState, componentIndex);
        componentExecutionState->flowState = flowState;
        componentExecutionState->componentIndex = componentIndex;

        auto connectionArray = connectionValue.getArray();
        void *handle = connectionArray->values[defs_v3::OBJECT_TYPE_MQTT_CONNECTION_FIELD_ID].getVoidPointer();
        addConnectionEventHandler(handle, componentExecutionState);

	    propagateValueThroughSeqout(flowState, componentIndex);

        addToQueue(flowState, componentIndex, -1, -1, -1, true);
    } else {
        auto event = componentExecutionState->removeEvent();
        if (event) {
            propagateValue(flowState, componentIndex, event->outputIndex, event->value);
            ObjectAllocator<MQTTEvent>::deallocate(event);
        } else {
            addToQueue(flowState, componentIndex, -1, -1, -1, true);
        }
    }
}

////////////////////////////////////////////////////////////////////////////////

void executeMQTTSubscribeComponent(FlowState *flowState, unsigned componentIndex) {
    Value connectionValue;
    if (!evalProperty(flowState, componentIndex, defs_v3::MQTT_SUBSCRIBE_ACTION_COMPONENT_PROPERTY_CONNECTION, connectionValue, "Failed to evaluate Connection in MQTTSubscribe")) {
        return;
    }
    if (!connectionValue.isArray() || connectionValue.getArray()->arrayType != defs_v3::OBJECT_TYPE_MQTT_CONNECTION) {
        throwError(flowState, componentIndex, "Connection must be a object:MQTTConnection");
        return;
    }

    Value topicValue;
    if (!evalProperty(flowState, componentIndex, defs_v3::MQTT_SUBSCRIBE_ACTION_COMPONENT_PROPERTY_TOPIC, topicValue, "Failed to evaluate Topic in MQTTSubscribe")) {
        return;
    }
    if (!topicValue.isString()) {
        throwError(flowState, componentIndex, "Topic must be a string");
        return;
    }

    auto connectionArray = connectionValue.getArray();
    void *handle = connectionArray->values[defs_v3::OBJECT_TYPE_MQTT_CONNECTION_FIELD_ID].getVoidPointer();

    auto result = eez_mqtt_subscribe(handle, topicValue.getString());
    if (result != MQTT_ERROR_OK) {
        char errorMessage[256];
        snprintf(errorMessage, sizeof(errorMessage), "Failed to subscribe to MQTT topic with error code: %d", (int)result);
        throwError(flowState, componentIndex, errorMessage);
        return;
    }

    propagateValueThroughSeqout(flowState, componentIndex);
}

////////////////////////////////////////////////////////////////////////////////

void executeMQTTUnsubscribeComponent(FlowState *flowState, unsigned componentIndex) {
    Value connectionValue;
    if (!evalProperty(flowState, componentIndex, defs_v3::MQTT_UNSUBSCRIBE_ACTION_COMPONENT_PROPERTY_CONNECTION, connectionValue, "Failed to evaluate Connection in MQTTUnsubscribe")) {
        return;
    }
    if (!connectionValue.isArray() || connectionValue.getArray()->arrayType != defs_v3::OBJECT_TYPE_MQTT_CONNECTION) {
        throwError(flowState, componentIndex, "Connection must be a object:MQTTConnection");
        return;
    }

    Value topicValue;
    if (!evalProperty(flowState, componentIndex, defs_v3::MQTT_UNSUBSCRIBE_ACTION_COMPONENT_PROPERTY_TOPIC, topicValue, "Failed to evaluate Topic in MQTTUnsubscribe")) {
        return;
    }
    if (!topicValue.isString()) {
        throwError(flowState, componentIndex, "Topic must be a string");
        return;
    }

    auto connectionArray = connectionValue.getArray();
    void *handle = connectionArray->values[defs_v3::OBJECT_TYPE_MQTT_CONNECTION_FIELD_ID].getVoidPointer();

    auto result = eez_mqtt_unsubscribe(handle, topicValue.getString());
    if (result != MQTT_ERROR_OK) {
        char errorMessage[256];
        snprintf(errorMessage, sizeof(errorMessage), "Failed to unsubscribe from MQTT topic with error code: %d", (int)result);
        throwError(flowState, componentIndex, errorMessage);
        return;
    }

    propagateValueThroughSeqout(flowState, componentIndex);
}

////////////////////////////////////////////////////////////////////////////////

void executeMQTTPublishComponent(FlowState *flowState, unsigned componentIndex) {
	Value connectionValue;
    if (!evalProperty(flowState, componentIndex, defs_v3::MQTT_PUBLISH_ACTION_COMPONENT_PROPERTY_CONNECTION, connectionValue, "Failed to evaluate Connection in MQTTPublish")) {
        return;
    }
    if (!connectionValue.isArray() || connectionValue.getArray()->arrayType != defs_v3::OBJECT_TYPE_MQTT_CONNECTION) {
        throwError(flowState, componentIndex, "Connection must be a object:MQTTConnection");
        return;
    }

    Value topicValue;
    if (!evalProperty(flowState, componentIndex, defs_v3::MQTT_PUBLISH_ACTION_COMPONENT_PROPERTY_TOPIC, topicValue, "Failed to evaluate Topic in MQTTPublish")) {
        return;
    }
    if (!topicValue.isString()) {
        throwError(flowState, componentIndex, "Topic must be a string");
        return;
    }

    Value payloadValue;
    if (!evalProperty(flowState, componentIndex, defs_v3::MQTT_PUBLISH_ACTION_COMPONENT_PROPERTY_PAYLOAD, payloadValue, "Failed to evaluate Payload in MQTTPublish")) {
        return;
    }
    if (!payloadValue.isString()) {
        throwError(flowState, componentIndex, "Topic must be a string");
        return;
    }

    auto connectionArray = connectionValue.getArray();
    void *handle = connectionArray->values[defs_v3::OBJECT_TYPE_MQTT_CONNECTION_FIELD_ID].getVoidPointer();

    auto result = eez_mqtt_publish(handle, topicValue.getString(), payloadValue.getString());
    if (result != MQTT_ERROR_OK) {
        char errorMessage[256];
        snprintf(errorMessage, sizeof(errorMessage), "Failed to subscribe to MQTT topic with error code: %d", (int)result);
        throwError(flowState, componentIndex, errorMessage);
        return;
    }

    propagateValueThroughSeqout(flowState, componentIndex);
}

////////////////////////////////////////////////////////////////////////////////

} // namespace flow
} // namespace eez

////////////////////////////////////////////////////////////////////////////////

#ifdef EEZ_STUDIO_FLOW_RUNTIME

#include <emscripten.h>

extern "C" {

int eez_mqtt_init(const char *protocol, const char *host, int port, const char *username, const char *password, void **handle) {
    int id = EM_ASM_INT({
        return eez_mqtt_init($0, UTF8ToString($1), UTF8ToString($2), $3, UTF8ToString($4), UTF8ToString($5));
    }, eez::flow::g_wasmModuleId, protocol, host, port, username, password);

    if (id == 0) {
        return 1;
    }

    *handle = (void *)id;

    return MQTT_ERROR_OK;
}

int eez_mqtt_deinit(void *handle) {
    return EM_ASM_INT({
        return eez_mqtt_deinit($0, $1);
    }, eez::flow::g_wasmModuleId, handle);
}

int eez_mqtt_connect(void *handle) {
    return EM_ASM_INT({
        return eez_mqtt_connect($0, $1);
    }, eez::flow::g_wasmModuleId, handle);
}

int eez_mqtt_disconnect(void *handle) {
    return EM_ASM_INT({
        return eez_mqtt_disconnect($0, $1);
    }, eez::flow::g_wasmModuleId, handle);
}

int eez_mqtt_subscribe(void *handle, const char *topic) {
    return EM_ASM_INT({
        return eez_mqtt_subscribe($0, $1, UTF8ToString($2));
    }, eez::flow::g_wasmModuleId, handle, topic);
}

int eez_mqtt_unsubscribe(void *handle, const char *topic) {
    return EM_ASM_INT({
        return eez_mqtt_unsubscribe($0, $1, UTF8ToString($2));
    }, eez::flow::g_wasmModuleId, handle, topic);
}

int eez_mqtt_publish(void *handle, const char *topic, const char *payload) {
    return EM_ASM_INT({
        return eez_mqtt_publish($0, $1, UTF8ToString($2), UTF8ToString($3));
    }, eez::flow::g_wasmModuleId, handle, topic, payload);
}

}

EM_PORT_API(void) onMqttEvent(void *handle, EEZ_MQTT_Event event, void *eventDataPtr1, void *eventDataPtr2) {
    void *eventData;
    if (eventDataPtr1 && eventDataPtr2)  {
        EEZ_MQTT_MessageEvent eventData;
        eventData.topic = (const char *)eventDataPtr1;
        eventData.payload = (const char *)eventDataPtr2;
        eez::flow::eez_mqtt_on_event_callback(handle, event, &eventData);
    } else if (eventDataPtr1) {
        eez::flow::eez_mqtt_on_event_callback(handle, event, eventDataPtr1);
    } else {
        eez::flow::eez_mqtt_on_event_callback(handle, event, nullptr);
    }
}

#else

#ifndef EEZ_MQTT_ADAPTER

int eez_mqtt_init(const char *protocol, const char *host, int port, const char *username, const char *password, void **handle) {
    return MQTT_ERROR_NOT_IMPLEMENTED;
}

int eez_mqtt_deinit(void *handle) {
    return MQTT_ERROR_NOT_IMPLEMENTED;
}

int eez_mqtt_connect(void *handle) {
    return MQTT_ERROR_NOT_IMPLEMENTED;
}

int eez_mqtt_disconnect(void *handle) {
    return MQTT_ERROR_NOT_IMPLEMENTED;
}

int eez_mqtt_subscribe(void *handle, const char *topic) {
    return MQTT_ERROR_NOT_IMPLEMENTED;
}

int eez_mqtt_unsubscribe(void *handle, const char *topic) {
    return MQTT_ERROR_NOT_IMPLEMENTED;
}

int eez_mqtt_publish(void *handle, const char *topic, const char *payload) {
    return MQTT_ERROR_NOT_IMPLEMENTED;
}

#endif

#endif

////////////////////////////////////////////////////////////////////////////////
