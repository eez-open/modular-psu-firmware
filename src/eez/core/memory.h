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

static uint8_t * const DECOMPRESSED_ASSETS_START_ADDRESS = MEMORY_BEGIN;
#if defined(EEZ_PLATFORM_STM32)
static const uint32_t MAX_DECOMPRESSED_ASSETS_SIZE = (3 * 512 + 128) * 1024;
#endif
#if defined(EEZ_PLATFORM_SIMULATOR)
static const uint32_t MAX_DECOMPRESSED_ASSETS_SIZE = 8 * 1024 * 1024;
#endif

static uint8_t * const DLOG_RECORD_BUFFER = DECOMPRESSED_ASSETS_START_ADDRESS + MAX_DECOMPRESSED_ASSETS_SIZE;
static const uint32_t DLOG_RECORD_BUFFER_SIZE = 512 * 1024;

static uint8_t * const DLOG_RECORD_SAVE_BUFFER = DLOG_RECORD_BUFFER + DLOG_RECORD_BUFFER_SIZE;
static const uint32_t DLOG_RECORD_SAVE_BUFFER_SIZE = 64 * 1024;

static uint8_t * const FILE_VIEW_BUFFER = DLOG_RECORD_SAVE_BUFFER + DLOG_RECORD_SAVE_BUFFER_SIZE;
#if defined(EEZ_PLATFORM_STM32)
static const uint32_t FILE_VIEW_BUFFER_SIZE = 768 * 1024;
#endif
#if defined(EEZ_PLATFORM_SIMULATOR)
static const uint32_t FILE_VIEW_BUFFER_SIZE = 3 * 512 * 1024;
#endif

static uint8_t * const ALLOC_BUFFER = FILE_VIEW_BUFFER + FILE_VIEW_BUFFER_SIZE;
static const uint32_t ALLOC_BUFFER_SIZE = (1024 + 512) * 1024;

static uint8_t * const FLOW_TO_DEBUGGER_MESSAGE_BUFFER = ALLOC_BUFFER + ALLOC_BUFFER_SIZE;
#if defined(EEZ_PLATFORM_STM32)
static const uint32_t FLOW_TO_DEBUGGER_MESSAGE_BUFFER_SIZE = 32 * 1024;
#endif
#if defined(EEZ_PLATFORM_SIMULATOR)
static const uint32_t FLOW_TO_DEBUGGER_MESSAGE_BUFFER_SIZE = 1024 * 1024;
#endif

static uint8_t * const SOUND_TUNES_MEMORY = FLOW_TO_DEBUGGER_MESSAGE_BUFFER + FLOW_TO_DEBUGGER_MESSAGE_BUFFER_SIZE;
static const uint32_t SOUND_TUNES_MEMORY_SIZE = 32 * 1024;

static uint8_t * const FILE_MANAGER_MEMORY = SOUND_TUNES_MEMORY + SOUND_TUNES_MEMORY_SIZE;
static const uint32_t FILE_MANAGER_MEMORY_SIZE = 256 * 1024;

static uint8_t * const UART_BUFFER_MEMORY = FILE_MANAGER_MEMORY + FILE_MANAGER_MEMORY_SIZE;
static const uint32_t UART_BUFFER_MEMORY_SIZE = 256 * 1024;

static uint8_t * const VRAM_SCREENSHOOT_JPEG_OUT_BUFFER = UART_BUFFER_MEMORY + UART_BUFFER_MEMORY_SIZE;
static const uint32_t VRAM_SCREENSHOOT_JPEG_OUT_BUFFER_SIZE = 256 * 1024;

static const uint32_t VRAM_BUFFER_SIZE = DISPLAY_WIDTH * DISPLAY_HEIGHT * DISPLAY_BPP / 8;

static uint8_t * const VRAM_BUFFER1_START_ADDRESS = VRAM_SCREENSHOOT_JPEG_OUT_BUFFER + VRAM_SCREENSHOOT_JPEG_OUT_BUFFER_SIZE;
static uint8_t * const VRAM_BUFFER2_START_ADDRESS = VRAM_BUFFER1_START_ADDRESS + VRAM_BUFFER_SIZE;

// used for animation and screenshot
static uint8_t * const VRAM_ANIMATION_BUFFER1_START_ADDRESS = VRAM_BUFFER2_START_ADDRESS + VRAM_BUFFER_SIZE;
static uint8_t * const VRAM_ANIMATION_BUFFER2_START_ADDRESS = VRAM_ANIMATION_BUFFER1_START_ADDRESS + VRAM_BUFFER_SIZE;
static uint8_t * const SCREENSHOOT_BUFFER_START_ADDRESS = VRAM_ANIMATION_BUFFER1_START_ADDRESS;

static uint8_t * const VRAM_AUX_BUFFER1_START_ADDRESS = VRAM_ANIMATION_BUFFER2_START_ADDRESS + VRAM_BUFFER_SIZE;
static uint8_t * const VRAM_AUX_BUFFER2_START_ADDRESS = VRAM_AUX_BUFFER1_START_ADDRESS + VRAM_BUFFER_SIZE;
static uint8_t * const VRAM_AUX_BUFFER3_START_ADDRESS = VRAM_AUX_BUFFER2_START_ADDRESS + VRAM_BUFFER_SIZE;
static uint8_t * const VRAM_AUX_BUFFER4_START_ADDRESS = VRAM_AUX_BUFFER3_START_ADDRESS + VRAM_BUFFER_SIZE;
static uint8_t * const VRAM_AUX_BUFFER5_START_ADDRESS = VRAM_AUX_BUFFER4_START_ADDRESS + VRAM_BUFFER_SIZE;
static uint8_t * const VRAM_AUX_BUFFER6_START_ADDRESS = VRAM_AUX_BUFFER5_START_ADDRESS + VRAM_BUFFER_SIZE;

static uint8_t * const MEMORY_END = VRAM_AUX_BUFFER6_START_ADDRESS + VRAM_BUFFER_SIZE;
