/* / stm32
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

#include "usbd_cdc_if.h"

#include <eez/apps/psu/psu.h>
#include <eez/apps/psu/serial_psu.h>
#include <eez/apps/psu/stm32/serial.h>
#include <eez/system.h>

#include <eez/scpi/scpi.h>
using namespace eez::scpi;

UARTClass Serial;

uint8_t *g_buffer;
uint32_t g_length;

////////////////////////////////////////////////////////////////////////////////

extern "C" void Serial_Write(const char *str, size_t len) {
	Serial.write(str, (int)len);
}

extern "C" void notifySerialLineStateChanged(uint8_t serialLineState) {
    osMessagePut(g_scpiMessageQueueId, SCPI_QUEUE_SERIAL_MESSAGE(SERIAL_LINE_STATE_CHANGED, serialLineState), osWaitForever);
}

extern "C" void notifySerialInput(uint8_t *buffer, uint32_t length) {
	g_buffer = buffer;
	g_length = length;
	osMessagePut(g_scpiMessageQueueId, SCPI_QUEUE_SERIAL_MESSAGE(SERIAL_INPUT_AVAILABLE, 0), osWaitForever);
	return;
}

////////////////////////////////////////////////////////////////////////////////

void UARTClass::begin(unsigned long baud, UARTModes config) {
}

void UARTClass::end() {
}

extern USBD_HandleTypeDef hUsbDeviceFS;

void UARTClass::getInputBuffer(int bufferPosition, uint8_t **buffer, uint32_t *length) {
    *buffer = g_buffer;
    *length = g_length;
}

void UARTClass::releaseInputBuffer() {
	USBD_CDC_ReceivePacket(&hUsbDeviceFS);
}

int UARTClass::write(const char *buffer, int size) {
    CDC_Transmit_FS((uint8_t *)buffer, (uint16_t)size);
    return size;
}

int UARTClass::print(const char *data) {
    return write(data, strlen(data));
}

int UARTClass::println(const char *data) {
    char buffer[1024];
    int size = sprintf(buffer, "%s\n", data);
    return write(buffer, size);
}

int UARTClass::print(int value) {
    char buffer[1024];
    int size = sprintf(buffer, "%d", value);
    return write(buffer, size);
}

int UARTClass::println(int value) {
    char buffer[1024];
    int size = sprintf(buffer, "%d\n", value);
    return write(buffer, size);
}

int UARTClass::print(float value, int numDigits) {
    // TODO numDigits
    char buffer[1024];
    int size = sprintf(buffer, "%f", value);
    return write(buffer, size);
}

int UARTClass::println(float value, int numDigits) {
    // TODO numDigits
    char buffer[1024];
    int size = sprintf(buffer, "%f\n", value);
    return write(buffer, size);
}

int UARTClass::println(IPAddress ipAddress) {
    char buffer[1024];
    int size =
        sprintf(buffer, "%d.%d.%d.%d\n", ipAddress._address.bytes[0], ipAddress._address.bytes[1],
                ipAddress._address.bytes[2], ipAddress._address.bytes[3]);
    return write(buffer, size);
}
