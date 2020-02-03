/*
 * EEZ Middleware
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

#pragma once

#if defined(EEZ_PLATFORM_STM32)
#include <api.h>
#endif

namespace eez {
namespace mcu {
namespace ethernet {

void initMessageQueue();
void startThread();

struct IPAddress {
    IPAddress() {
      _address.dword = 0;
    }

    IPAddress(uint32_t dwordAddress) {
      _address.dword = dwordAddress;
    }

    union {
        uint8_t bytes[4]; // IPv4 address
        uint32_t dword;
    } _address;

    operator uint32_t() const {
        return _address.dword;
    };
};

void begin();

IPAddress localIP();
IPAddress subnetMask();
IPAddress gatewayIP();
IPAddress dnsServerIP();

void beginServer(uint16_t port);

void getInputBuffer(int bufferPosition, char **buffer, uint32_t *length);
void releaseInputBuffer();

int writeBuffer(const char *buffer, uint32_t length);

void pushEvent(int16_t eventId);

void ntpStateTransition(int transition);

}
}
} // namespace eez::mcu::ethernet
