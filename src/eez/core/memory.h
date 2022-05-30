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
#include <eez/conf.h>

namespace eez {

#if defined(EEZ_PLATFORM_STM32)
static uint8_t * const MEMORY_BEGIN = (uint8_t *)0xc0000000u;
#if CONF_OPTION_FPGA
static const uint32_t MEMORY_SIZE = 32 * 1024 * 1024;
#else
static const uint32_t MEMORY_SIZE = 8 * 1024 * 1024;
#endif
#endif

#if defined(EEZ_PLATFORM_SIMULATOR)
extern uint8_t g_memory[];
static uint8_t * const MEMORY_BEGIN = g_memory;
static const uint32_t MEMORY_SIZE = 64 * 1024 * 1024;
#endif

extern uint8_t *ALLOC_BUFFER;
extern uint32_t ALLOC_BUFFER_SIZE;

extern uint8_t *DECOMPRESSED_ASSETS_START_ADDRESS;
#if defined(EEZ_PLATFORM_STM32)
static const uint32_t MAX_DECOMPRESSED_ASSETS_SIZE = (3 * 512 + 128) * 1024;
#endif
#if defined(EEZ_PLATFORM_SIMULATOR)
static const uint32_t MAX_DECOMPRESSED_ASSETS_SIZE = 8 * 1024 * 1024;
#endif

extern uint8_t *FLOW_TO_DEBUGGER_MESSAGE_BUFFER;
#if defined(EEZ_PLATFORM_STM32)
static const uint32_t FLOW_TO_DEBUGGER_MESSAGE_BUFFER_SIZE = 32 * 1024;
#endif
#if defined(EEZ_PLATFORM_SIMULATOR)
static const uint32_t FLOW_TO_DEBUGGER_MESSAGE_BUFFER_SIZE = 1024 * 1024;
#endif

extern uint8_t *VRAM_BUFFER1_START_ADDRESS;
extern uint8_t *VRAM_BUFFER2_START_ADDRESS;

extern uint8_t *VRAM_ANIMATION_BUFFER1_START_ADDRESS;
extern uint8_t *VRAM_ANIMATION_BUFFER2_START_ADDRESS;

extern uint8_t *VRAM_AUX_BUFFER1_START_ADDRESS;
extern uint8_t *VRAM_AUX_BUFFER2_START_ADDRESS;
extern uint8_t *VRAM_AUX_BUFFER3_START_ADDRESS;
extern uint8_t *VRAM_AUX_BUFFER4_START_ADDRESS;
extern uint8_t *VRAM_AUX_BUFFER5_START_ADDRESS;
extern uint8_t *VRAM_AUX_BUFFER6_START_ADDRESS;

extern uint8_t* SCREENSHOOT_BUFFER_START_ADDRESS;

extern uint8_t* GUI_STATE_BUFFER;

void initMemory();
uint8_t *allocBuffer(uint32_t size);

} // eez