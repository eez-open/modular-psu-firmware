/*
 * EEZ DIB MIO168
 * Copyright (C) 2020-present, Envox d.o.o.
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

namespace eez {
namespace dib_mio168 {
namespace fs {

bool isDriverLinked(int slotIndex);
void LinkDriver(int slotIndex);
void UnLinkDriver(int slotIndex);

enum DiskDriverOperation {
    DISK_DRIVER_OPERATION_NONE,
    DISK_DRIVER_OPERATION_INITIALIZE,
    DISK_DRIVER_OPERATION_STATUS,
    DISK_DRIVER_OPERATION_READ,
    DISK_DRIVER_OPERATION_WRITE,
    DISK_DRIVER_OPERATION_IOCTL
};

extern DiskDriverOperation g_operation;
extern int g_slotIndex;
extern uint32_t g_sector;
extern uint8_t* g_buff;
extern uint8_t g_cmd;
extern uint32_t g_result;

} // namespace fs
} // namespace dib_mio168
} // namespace eez
