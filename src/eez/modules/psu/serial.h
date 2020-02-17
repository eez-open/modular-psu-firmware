/*
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

#pragma once

#include <stdint.h>

class UARTClass {
public:
#if defined(EEZ_PLATFORM_SIMULATOR)
    void put(int ch);
#endif

    void getInputBuffer(int bufferPosition, uint8_t **buffer, uint32_t *length);
    void releaseInputBuffer();

    int write(const char *buffer, int size);
    int print(const char *data);
    int println(const char *data);
    int print(int value);
    int println(int value);
    int print(float value, int numDigits);
    int println(float value, int numDigits);
};


extern UARTClass Serial;
