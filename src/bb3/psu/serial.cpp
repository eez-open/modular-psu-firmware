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

#if defined(EEZ_PLATFORM_STM32)
#include "usbd_cdc_if.h"
#endif

#if defined(EEZ_PLATFORM_SIMULATOR)
#include <stdio.h>
#include <string.h>
#endif

#include <eez/system.h>
#include <eez/tasks.h>

#include <bb3/psu/psu.h>
#include <bb3/psu/serial_psu.h>

UARTClass Serial;

#if defined(EEZ_PLATFORM_STM32)
uint8_t *g_buffer;
uint32_t g_length;
#endif

#if defined(EEZ_PLATFORM_SIMULATOR)
uint8_t g_serialInputChars[LOW_PRIORITY_THREAD_QUEUE_SIZE + 1];
int g_serialInputCharPosition = 0;
#endif

////////////////////////////////////////////////////////////////////////////////

#if defined(EEZ_PLATFORM_STM32)
extern "C" void notifySerialLineStateChanged(uint8_t serialLineState) {
	using namespace eez;
    sendMessageToLowPriorityThread(SERIAL_LINE_STATE_CHANGED, serialLineState);
}

extern "C" void notifySerialInput(uint8_t *buffer, uint32_t length) {
	g_buffer = buffer;
	g_length = length;
	using namespace eez;
	sendMessageToLowPriorityThread(SERIAL_INPUT_AVAILABLE);
	return;
}
#endif

////////////////////////////////////////////////////////////////////////////////

#if defined(EEZ_PLATFORM_STM32)
extern USBD_HandleTypeDef hUsbDeviceFS;
#endif

#if defined(EEZ_PLATFORM_SIMULATOR)
void UARTClass::put(int ch) {
    g_serialInputChars[g_serialInputCharPosition] = ch;
    
    using namespace eez;
    sendMessageToLowPriorityThread(SERIAL_INPUT_AVAILABLE, g_serialInputCharPosition);
    
    g_serialInputCharPosition = (g_serialInputCharPosition + 1) % (LOW_PRIORITY_THREAD_QUEUE_SIZE + 1);
}
#endif

void UARTClass::getInputBuffer(int bufferPosition, uint8_t **buffer, uint32_t *length) {
#if defined(EEZ_PLATFORM_STM32)
    *buffer = g_buffer;
    *length = g_length;
#endif

#if defined(EEZ_PLATFORM_SIMULATOR)
    *buffer = &g_serialInputChars[bufferPosition];
    *length = 1;
#endif
}

void UARTClass::releaseInputBuffer() {
#if defined(EEZ_PLATFORM_STM32)
	USBD_CDC_ReceivePacket(&hUsbDeviceFS);
#endif
}

int UARTClass::write(const char *buffer, int size) {
#if defined(EEZ_PLATFORM_STM32)    
    CDC_Transmit_FS((uint8_t *)buffer, (uint16_t)size);
    return size;
#endif

#if defined(EEZ_PLATFORM_SIMULATOR)
    return fwrite(buffer, 1, size, stdout);
#endif
}

int UARTClass::print(const char *data) {
    return write(data, strlen(data));
}

int UARTClass::println(const char *data) {
#if defined(EEZ_PLATFORM_STM32)    
    char buffer[1024];
    int size = snprintf(buffer, sizeof(buffer), "%s\n", data);
    return write(buffer, size);
#endif

#if defined(EEZ_PLATFORM_SIMULATOR)
    return printf("%s\n", data);
#endif
}

int UARTClass::print(int value) {
#if defined(EEZ_PLATFORM_STM32)    
    char buffer[1024];
    int size = snprintf(buffer, sizeof(buffer), "%d", value);
    return write(buffer, size);
#endif

#if defined(EEZ_PLATFORM_SIMULATOR)
    return printf("%d", value);
#endif
}

int UARTClass::println(int value) {
#if defined(EEZ_PLATFORM_STM32)    
    char buffer[1024];
    int size = snprintf(buffer, sizeof(buffer), "%d\n", value);
    return write(buffer, size);
#endif

#if defined(EEZ_PLATFORM_SIMULATOR)
    return printf("%d\n", value);
#endif
}

int UARTClass::print(float value, int numDigits) {
#if defined(EEZ_PLATFORM_STM32)    
    // TODO numDigits
    char buffer[1024];
    int size = snprintf(buffer, sizeof(buffer), "%f", value);
    return write(buffer, size);
#endif

#if defined(EEZ_PLATFORM_SIMULATOR)
    return printf("%f", value);
#endif
}

int UARTClass::println(float value, int numDigits) {
#if defined(EEZ_PLATFORM_STM32)    
    // TODO numDigits
    char buffer[1024];
    int size = snprintf(buffer, sizeof(buffer), "%f\n", value);
    return write(buffer, size);
#endif

#if defined(EEZ_PLATFORM_SIMULATOR)
    return printf("%f\n", value);
#endif
}
