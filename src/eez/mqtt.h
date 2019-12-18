/*
* EEZ Generic Firmware
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

#pragma once

namespace eez {
namespace mqtt {

enum ConnectionState {
    CONNECTION_STATE_ERROR = -1,
    CONNECTION_STATE_IDLE,
    CONNECTION_STATE_CONNECTED,
    CONNECTION_STATE_TRANSIENT,
    CONNECTION_STATE_CONNECT = CONNECTION_STATE_TRANSIENT,
    CONNECTION_STATE_CONNECTING,
    CONNECTION_STATE_DNS_IN_PROGRESS,
    CONNECTION_STATE_DNS_FOUND,
    CONNECTION_STATE_DISCONNECT,
    CONNECTION_STATE_RECONNECT
};

static const float PERIOD_MIN = 0.1f;
static const float PERIOD_MAX = 120.0f;
static const float PERIOD_DEFAULT = 1.0f;

extern ConnectionState g_connectionState;
    
void tick(uint32_t tickCount);
void reconnect();
void pushEvent(int16_t eventId);

} // mqtt
} // eez
