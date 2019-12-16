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
};

extern ConnectionState g_connectionState;
    
void tick(uint32_t tickCount);

bool connect(const char *addr, int port, const char *user, const char *pass, int16_t *err);
bool disconnect(int16_t *err);

} // mqtt
} // eez