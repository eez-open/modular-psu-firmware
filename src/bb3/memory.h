/*
* BB3 Firmware
* Copyright (C) 2022-present, Envox d.o.o.
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

#include <eez/core/memory.h>

namespace eez {

namespace bb3 {
void initMemory();
}

extern uint8_t *DLOG_RECORD_BUFFER;
static const uint32_t DLOG_RECORD_BUFFER_SIZE = 512 * 1024;

extern uint8_t *DLOG_RECORD_SAVE_BUFFER;
static const uint32_t DLOG_RECORD_SAVE_BUFFER_SIZE = 64 * 1024;

extern uint8_t *FILE_VIEW_BUFFER;
#if defined(EEZ_PLATFORM_STM32)
static const uint32_t FILE_VIEW_BUFFER_SIZE = 768 * 1024;
#endif
#if defined(EEZ_PLATFORM_SIMULATOR)
static const uint32_t FILE_VIEW_BUFFER_SIZE = 3 * 512 * 1024;
#endif

extern uint8_t *SOUND_TUNES_MEMORY;
static const uint32_t SOUND_TUNES_MEMORY_SIZE = 32 * 1024;

extern uint8_t *FILE_MANAGER_MEMORY;
static const uint32_t FILE_MANAGER_MEMORY_SIZE = 256 * 1024;

extern uint8_t *UART_BUFFER_MEMORY;
static const uint32_t UART_BUFFER_MEMORY_SIZE = 256 * 1024;

extern uint8_t *VRAM_SCREENSHOOT_JPEG_OUT_BUFFER;
static const uint32_t VRAM_SCREENSHOOT_JPEG_OUT_BUFFER_SIZE = 256 * 1024;
} // eez
