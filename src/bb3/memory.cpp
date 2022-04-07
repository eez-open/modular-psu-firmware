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

#include <bb3/memory.h>
#include <bb3/uart.h>
#include <bb3/psu/psu.h>
#include <bb3/psu/dlog_view.h>
#include <bb3/psu/dlog_record.h>

namespace eez {

uint8_t *DLOG_RECORD_BUFFER;
uint8_t *DLOG_RECORD_SAVE_BUFFER;
uint8_t *FILE_VIEW_BUFFER;
uint8_t *SOUND_TUNES_MEMORY;
uint8_t *FILE_MANAGER_MEMORY;
uint8_t *UART_BUFFER_MEMORY;
uint8_t *VRAM_SCREENSHOOT_JPEG_OUT_BUFFER;
    
namespace bb3 {

void initMemory() {
    eez::initMemory();

    DLOG_RECORD_BUFFER = allocBuffer(DLOG_RECORD_BUFFER_SIZE);
    DLOG_RECORD_SAVE_BUFFER = allocBuffer(DLOG_RECORD_SAVE_BUFFER_SIZE);
    FILE_VIEW_BUFFER = allocBuffer(FILE_VIEW_BUFFER_SIZE);
    SOUND_TUNES_MEMORY = allocBuffer(SOUND_TUNES_MEMORY_SIZE);
    FILE_MANAGER_MEMORY = allocBuffer(FILE_MANAGER_MEMORY_SIZE);
    UART_BUFFER_MEMORY = allocBuffer(UART_BUFFER_MEMORY_SIZE);
    VRAM_SCREENSHOOT_JPEG_OUT_BUFFER = allocBuffer(VRAM_SCREENSHOOT_JPEG_OUT_BUFFER_SIZE);

    eez::uart::init();
    eez::psu::dlog_view::init();
    eez::psu::dlog_record::init();
}

} // bb3
} // eez
