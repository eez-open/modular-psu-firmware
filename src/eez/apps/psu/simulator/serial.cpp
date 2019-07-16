/*
 * EEZ PSU Firmware
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

#include <stdio.h>
#include <string.h>

#include <eez/apps/psu/psu.h>
#include <eez/apps/psu/serial_psu.h>
#include <eez/apps/psu/simulator/serial.h>

#include <eez/scpi/scpi.h>
using namespace eez::scpi;

UARTClass Serial;

uint8_t g_serialInputChars[SCPI_QUEUE_SIZE + 1];
int g_serialInputCharPosition = 0;

void UARTClass::begin(unsigned long baud, UARTModes config) {
}

void UARTClass::end() {
}

void UARTClass::put(int ch) {
    g_serialInputChars[g_serialInputCharPosition] = ch;
    
    osMessagePut(g_scpiMessageQueueId, SCPI_QUEUE_SERIAL_MESSAGE(SERIAL_INPUT_AVAILABLE, g_serialInputCharPosition), osWaitForever);
    
    g_serialInputCharPosition = (g_serialInputCharPosition + 1) % (SCPI_QUEUE_SIZE + 1);
}

void UARTClass::getInputBuffer(int bufferPosition, uint8_t **buffer, uint32_t *length) {
    *buffer = &g_serialInputChars[bufferPosition];
    *length = 1;
}

void UARTClass::releaseInputBuffer() {
}

int UARTClass::write(const char *buffer, int size) {
    return fwrite(buffer, 1, size, stdout);
}

int UARTClass::print(const char *data) {
    return write(data, strlen(data));
}

int UARTClass::println(const char *data) {
    return printf("%s\n", data);
}

int UARTClass::print(int value) {
    return printf("%d", value);
}

int UARTClass::println(int value) {
    return printf("%d\n", value);
}

int UARTClass::print(float value, int numDigits) {
    // TODO numDigits
    return printf("%f", value);
}

int UARTClass::println(float value, int numDigits) {
    // TODO numDigits
    return printf("%f\n", value);
}
