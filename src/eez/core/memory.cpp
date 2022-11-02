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

#include <assert.h>

#include <eez/core/memory.h>

#if defined(EEZ_FOR_LVGL)
#ifdef LV_LVGL_H_INCLUDE_SIMPLE
#include "lvgl.h"
#else
#include "lvgl/lvgl.h"
#endif
#endif

namespace eez {

#if defined(EEZ_PLATFORM_SIMULATOR) && !defined(EEZ_FOR_LVGL)
uint8_t g_memory[MEMORY_SIZE] = { 0 };
#endif

uint8_t *DECOMPRESSED_ASSETS_START_ADDRESS;
uint8_t *FLOW_TO_DEBUGGER_MESSAGE_BUFFER;

#if OPTION_GUI || !defined(OPTION_GUI)
uint8_t *VRAM_BUFFER1_START_ADDRESS;
uint8_t *VRAM_BUFFER2_START_ADDRESS;

uint8_t *VRAM_ANIMATION_BUFFER1_START_ADDRESS;
uint8_t *VRAM_ANIMATION_BUFFER2_START_ADDRESS;

uint8_t *VRAM_AUX_BUFFER1_START_ADDRESS;
uint8_t *VRAM_AUX_BUFFER2_START_ADDRESS;
uint8_t *VRAM_AUX_BUFFER3_START_ADDRESS;
uint8_t *VRAM_AUX_BUFFER4_START_ADDRESS;
uint8_t *VRAM_AUX_BUFFER5_START_ADDRESS;
uint8_t *VRAM_AUX_BUFFER6_START_ADDRESS;

uint8_t *SCREENSHOOT_BUFFER_START_ADDRESS;

uint8_t *GUI_STATE_BUFFER;
#endif

uint8_t *ALLOC_BUFFER = 0;
uint32_t ALLOC_BUFFER_SIZE = 0;

void initMemory() {
    initAssetsMemory();
    initOtherMemory();
}

void initAssetsMemory() {
#if defined(EEZ_FOR_LVGL)
#if defined(LV_MEM_SIZE)
    ALLOC_BUFFER_SIZE = LV_MEM_SIZE;
#endif
#else
    ALLOC_BUFFER = MEMORY_BEGIN;
    ALLOC_BUFFER_SIZE = MEMORY_SIZE;
    DECOMPRESSED_ASSETS_START_ADDRESS = allocBuffer(MAX_DECOMPRESSED_ASSETS_SIZE);
#endif
}

void initOtherMemory() {
#if !defined(EEZ_FOR_LVGL)
    FLOW_TO_DEBUGGER_MESSAGE_BUFFER = allocBuffer(FLOW_TO_DEBUGGER_MESSAGE_BUFFER_SIZE);
#endif

#if OPTION_GUI || !defined(OPTION_GUI)
    uint32_t VRAM_BUFFER_SIZE = DISPLAY_WIDTH * DISPLAY_HEIGHT * DISPLAY_BPP / 8;

    VRAM_BUFFER1_START_ADDRESS = allocBuffer(VRAM_BUFFER_SIZE);
    VRAM_BUFFER2_START_ADDRESS = allocBuffer(VRAM_BUFFER_SIZE);

    VRAM_ANIMATION_BUFFER1_START_ADDRESS = allocBuffer(VRAM_BUFFER_SIZE);
    VRAM_ANIMATION_BUFFER2_START_ADDRESS = allocBuffer(VRAM_BUFFER_SIZE);

    VRAM_AUX_BUFFER1_START_ADDRESS = allocBuffer(VRAM_BUFFER_SIZE);
    VRAM_AUX_BUFFER2_START_ADDRESS = allocBuffer(VRAM_BUFFER_SIZE);
    VRAM_AUX_BUFFER3_START_ADDRESS = allocBuffer(VRAM_BUFFER_SIZE);
    VRAM_AUX_BUFFER4_START_ADDRESS = allocBuffer(VRAM_BUFFER_SIZE);
    VRAM_AUX_BUFFER5_START_ADDRESS = allocBuffer(VRAM_BUFFER_SIZE);
    VRAM_AUX_BUFFER6_START_ADDRESS = allocBuffer(VRAM_BUFFER_SIZE);

    GUI_STATE_BUFFER = allocBuffer(GUI_STATE_BUFFER_SIZE);

    SCREENSHOOT_BUFFER_START_ADDRESS = VRAM_ANIMATION_BUFFER1_START_ADDRESS;
#endif
}

uint8_t *allocBuffer(uint32_t size) {
#if defined(EEZ_FOR_LVGL)
    return (uint8_t *)lv_mem_alloc(size);
#else
    size = ((size + 1023) / 1024) * 1024;

    auto buffer = ALLOC_BUFFER;

    assert(ALLOC_BUFFER_SIZE > size);
    ALLOC_BUFFER += size;
    ALLOC_BUFFER_SIZE -= size;

    return buffer;
#endif
}

} // eez
