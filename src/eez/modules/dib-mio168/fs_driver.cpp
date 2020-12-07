    /*
 * EEZ DIB MIO168
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

#include <ff_gen_drv.h>

#include <eez/debug.h>

#include "./fs_driver.h"

extern Disk_drvTypeDef disk;

namespace eez {
namespace dib_mio168 {

void executeDiskDriveOperation(int slotIndex);

namespace fs {

DiskDriverOperation g_operation;
int g_slotIndex;
uint32_t g_sector;
uint8_t* g_buff;
uint8_t g_cmd;
bool g_finished;
uint32_t g_result;

static DSTATUS DiskDriver_initialize(BYTE);
static DSTATUS DiskDriver_status(BYTE);
static DRESULT DiskDriver_read(BYTE, BYTE*, DWORD, UINT);
static DRESULT DiskDriver_write(BYTE, const BYTE*, DWORD, UINT);
static DRESULT DiskDriver_ioctl(BYTE, BYTE, void*);

static const Diskio_drvTypeDef g_diskDriver = {
    DiskDriver_initialize,
    DiskDriver_status,
    DiskDriver_read,
    DiskDriver_write,
    DiskDriver_ioctl,
};

static FATFS g_fatFS[3];

bool isDriverLinked(int slotIndex) {
    int driverIndex = slotIndex + 1;
    return disk.drv[driverIndex] != 0;
}

void LinkDriver(int slotIndex) {
    int driverIndex = slotIndex + 1;
    if (disk.drv[driverIndex]) {
        // already linked
        return;
    }

    disk.is_initialized[driverIndex] = 0;
    disk.drv[driverIndex] = &g_diskDriver;
    disk.lun[driverIndex] = slotIndex;

    // mount
    char path[4];
    path[0] = '0' + driverIndex;
    path[1] = ':';
    path[2] = '/';
    path[2] = '/';
    auto result = f_mount(&g_fatFS[slotIndex], path, 1);
    if (result != FR_OK) {
        DebugTrace("MIO168 disk mount failed\n");
        return;
    }

    DebugTrace("MIO168 disk driver linked\n");
}

void UnLinkDriver(int slotIndex) {
    int driverIndex = slotIndex + 1;
    if (!disk.drv[driverIndex]) {
        // already unlinked
        return;
    }

    // unmount
    char path[4];
    path[0] = '0' + driverIndex;
    path[1] = ':';
    path[2] = '/';
    path[2] = '/';
    f_mount(0, path, 0);

    disk.drv[driverIndex] = 0;
    
    DebugTrace("MIO168 disk driver unlinked\n");
}

DSTATUS DiskDriver_initialize(BYTE lun) {
    g_operation = DISK_DRIVER_OPERATION_INITIALIZE;
    g_slotIndex = lun;

    executeDiskDriveOperation(g_slotIndex);

    return (DSTATUS)g_result;
}

DSTATUS DiskDriver_status(BYTE lun) {
    g_operation = DISK_DRIVER_OPERATION_STATUS;
    g_slotIndex = lun;

    executeDiskDriveOperation(g_slotIndex);

    return (DSTATUS)g_result;
}

DRESULT DiskDriver_read(BYTE lun, BYTE* buff, DWORD sector, UINT count) {
    g_operation = DISK_DRIVER_OPERATION_READ;
    g_slotIndex = lun;

    for (UINT i = 0; i < count; i++) {
        g_buff = buff + i * 512;
        g_sector = sector + i;

        executeDiskDriveOperation(g_slotIndex);

        if (g_result != RES_OK) {
            return (DRESULT)g_result;
        }
    }

    return RES_OK;
}

DRESULT DiskDriver_write(BYTE lun, const BYTE* buff, DWORD sector, UINT count) {
    g_operation = DISK_DRIVER_OPERATION_WRITE;
    g_slotIndex = lun;
    
    for (UINT i = 0; i < count; i++) {
        g_buff = (BYTE *)buff + i * 512;
        g_sector = sector + i;

        executeDiskDriveOperation(g_slotIndex);

        if (g_result != RES_OK) {
            return (DRESULT)g_result;
        }
    }

    return RES_OK;
}

DRESULT DiskDriver_ioctl(BYTE lun, BYTE cmd, void *buff) {
    g_operation = DISK_DRIVER_OPERATION_IOCTL;
    g_slotIndex = lun;

    g_cmd = cmd;
    g_buff = (BYTE *)buff;

    executeDiskDriveOperation(g_slotIndex);

    return (DRESULT)g_result;
}

} // namespace fs
} // namespace dib_mio168
} // namespace eez
