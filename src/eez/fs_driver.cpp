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

#if defined(EEZ_PLATFORM_STM32)
#include <ff_gen_drv.h>
#include <usbd_msc.h>
#include <usbd_def.h>
#endif

#include <eez/index.h>
#include <eez/debug.h>
#include <eez/usb.h>
#include <eez/modules/psu/sd_card.h>

#if OPTION_DISPLAY
#include <eez/modules/psu/gui/psu.h>
#include <eez/modules/psu/gui/file_manager.h>
#endif

#include "./fs_driver.h"

#if defined(EEZ_PLATFORM_STM32)
extern Disk_drvTypeDef disk;
#endif

namespace eez {
namespace fs_driver {

#if defined(EEZ_PLATFORM_STM32)
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
#endif

#if defined(EEZ_PLATFORM_SIMULATOR)
static bool g_diskDriverLinked[NUM_SLOTS];
#endif

////////////////////////////////////////////////////////////////////////////////

bool isDriverLinked(int slotIndex) {
#if defined(EEZ_PLATFORM_STM32) 
    int driverIndex = slotIndex + 1;
    return disk.drv[driverIndex] != 0;
#endif

#if defined(EEZ_PLATFORM_SIMULATOR)
    return g_diskDriverLinked[slotIndex];
#endif
}

void LinkDriver(int slotIndex) {
    if (isDriverLinked(slotIndex)) {
        // already linked
        return;
    }

#if defined(EEZ_PLATFORM_STM32)
    int driverIndex = slotIndex + 1;

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
        DebugTrace("Slot %d disk mount failed\n", slotIndex + 1);
        return;
    }
#endif

#if defined(EEZ_PLATFORM_SIMULATOR)
    g_diskDriverLinked[slotIndex] = true;
#endif

#if OPTION_DISPLAY
    eez::gui::file_manager::onSdCardMountedChange();
#endif

    DebugTrace("Slot %d disk driver linked\n", slotIndex + 1);
}

void UnLinkDriver(int slotIndex) {
    if (!isDriverLinked(slotIndex)) {
        // already unlinked
        return;
    }

#if defined(EEZ_PLATFORM_STM32)
    int driverIndex = slotIndex + 1;

    // unmount
    char path[4];
    path[0] = '0' + driverIndex;
    path[1] = ':';
    path[2] = '/';
    path[2] = '/';
    f_mount(0, path, 0);

    disk.drv[driverIndex] = 0;
#endif

#if defined(EEZ_PLATFORM_SIMULATOR)
    g_diskDriverLinked[slotIndex] = false;
#endif

#if OPTION_DISPLAY
    eez::gui::file_manager::onSdCardMountedChange();
#endif

    DebugTrace("Slot %d disk driver unlinked\n", slotIndex + 1);
}

////////////////////////////////////////////////////////////////////////////////

int getDiskDrivesNum(bool includeUsbMassStorageDevice) {
    int diskDrivesNum = 0;

    if (psu::sd_card::isMounted(nullptr, nullptr)) {
        diskDrivesNum++;
    	if (g_selectedMassStorageDevice == 0) {
    		includeUsbMassStorageDevice = false;
    	}
    }

    for (int slotIndex = 0; slotIndex < NUM_SLOTS; slotIndex++) {
        if (isDriverLinked(slotIndex) && !(usb::isMassStorageActive() && g_selectedMassStorageDevice == slotIndex + 1)) {
            diskDrivesNum++;
        	if (g_selectedMassStorageDevice == slotIndex + 1) {
        		includeUsbMassStorageDevice = false;
        	}
        }
    }

    if (includeUsbMassStorageDevice) {
    	diskDrivesNum++;
    }

    return diskDrivesNum;
}

int getDiskDriveIndex(int iterationIndex, bool includeUsbMassStorageDevice) {
    if (psu::sd_card::isMounted(nullptr, nullptr) || (includeUsbMassStorageDevice && g_selectedMassStorageDevice == 0)) {
        if (iterationIndex == 0) {
            return 0;
        }
        --iterationIndex;
    }

    int slotIndex;
    for (slotIndex = 0; slotIndex < NUM_SLOTS; slotIndex++) {
        if (fs_driver::isDriverLinked(slotIndex) || (includeUsbMassStorageDevice && g_selectedMassStorageDevice == slotIndex + 1)) {
            if (iterationIndex == 0) {
                return 1 + slotIndex;
            }
            --iterationIndex;
        }
    }

    return -1;
}

////////////////////////////////////////////////////////////////////////////////

#if defined(EEZ_PLATFORM_STM32)

DSTATUS DiskDriver_initialize(BYTE lun) {
    ExecuteDiskDriveOperationParams params;

    params.operation = DISK_DRIVER_OPERATION_INITIALIZE;

    g_slots[lun]->executeDiskDriveOperation(&params);

    return (DSTATUS)params.result;
}

DSTATUS DiskDriver_status(BYTE lun) {
    ExecuteDiskDriveOperationParams params;

    params.operation = DISK_DRIVER_OPERATION_STATUS;

    g_slots[lun]->executeDiskDriveOperation(&params);

    return (DSTATUS)params.result;
}

DRESULT DiskDriver_read(BYTE lun, BYTE* buff, DWORD sector, UINT count) {
    ExecuteDiskDriveOperationParams params;

    params.operation = DISK_DRIVER_OPERATION_READ;

    for (UINT i = 0; i < count; i++) {
        params.buff = buff + i * 512;
        params.sector = sector + i;

        g_slots[lun]->executeDiskDriveOperation(&params);

        if (params.result != RES_OK) {
            return (DRESULT)params.result;
        }
    }

    return RES_OK;
}

DRESULT DiskDriver_write(BYTE lun, const BYTE* buff, DWORD sector, UINT count) {
    ExecuteDiskDriveOperationParams params;

    params.operation = DISK_DRIVER_OPERATION_WRITE;
    
    for (UINT i = 0; i < count; i++) {
        params.buff = (BYTE *)buff + i * 512;
        params.sector = sector + i;

        g_slots[lun]->executeDiskDriveOperation(&params);

        if (params.result != RES_OK) {
            return (DRESULT)params.result;
        }
    }

    return RES_OK;
}

DRESULT DiskDriver_ioctl(BYTE lun, BYTE cmd, void *buff) {
    ExecuteDiskDriveOperationParams params;

    params.operation = DISK_DRIVER_OPERATION_IOCTL;

    params.cmd = cmd;
    params.buff = (BYTE *)buff;

    g_slots[lun]->executeDiskDriveOperation(&params);

    return (DRESULT)params.result;
}

////////////////////////////////////////////////////////////////////////////////

static int8_t UsbStorageFS_Init(uint8_t lun) {
    return USBD_OK;
}

static int8_t UsbStorageFS_GetCapacity(uint8_t lun, uint32_t *block_num, uint16_t *block_size) {
    int slotIndex = g_selectedMassStorageDevice - 1;

    ExecuteDiskDriveOperationParams params;

    params.operation = DISK_DRIVER_OPERATION_IOCTL;

    // get block_num
    params.cmd = GET_SECTOR_COUNT;
    params.buff = (uint8_t *)block_num;

    g_slots[slotIndex]->executeDiskDriveOperation(&params);

    if ((DRESULT)params.result != RES_OK) {
        return -1;
    }

    // get block_size
    params.cmd = GET_SECTOR_SIZE;
    params.buff = (uint8_t *)block_size;

    g_slots[slotIndex]->executeDiskDriveOperation(&params);
    
    if ((DRESULT)params.result != RES_OK) {
        return -1;
    }
    
    return USBD_OK;
}

static int8_t UsbStorageFS_IsReady(uint8_t lun) {
    int slotIndex = g_selectedMassStorageDevice - 1;

    ExecuteDiskDriveOperationParams params;

    params.operation = DISK_DRIVER_OPERATION_STATUS;

    g_slots[slotIndex]->executeDiskDriveOperation(&params);

    return (DSTATUS)params.result & STA_NOINIT ? -1 : USBD_OK;
}

static int8_t UsbStorageFS_IsWriteProtected(uint8_t lun) {
    return USBD_OK;
}

static int8_t UsbStorageFS_Read(uint8_t lun, uint8_t *buf, uint32_t blk_addr, uint16_t blk_len) {
    if (blk_len > 1) {
        return -1;
    }

    int slotIndex = g_selectedMassStorageDevice - 1;

    ExecuteDiskDriveOperationParams params;

    params.operation = DISK_DRIVER_OPERATION_READ;

    params.buff = buf;
    params.sector = blk_addr;

    g_slots[slotIndex]->executeDiskDriveOperation(&params);

    return (DRESULT)params.result == RES_OK ? USBD_OK : -1;
}

static int8_t UsbStorageFS_Write(uint8_t lun, uint8_t *buf, uint32_t blk_addr, uint16_t blk_len) {
    if (blk_len > 1) {
        return -1;
    }

    int slotIndex = g_selectedMassStorageDevice - 1;

    ExecuteDiskDriveOperationParams params;

    params.operation = DISK_DRIVER_OPERATION_WRITE;

    params.buff = buf;
    params.sector = blk_addr;

    g_slots[slotIndex]->executeDiskDriveOperation(&params);

    return (DRESULT)params.result == RES_OK ? USBD_OK : -1;
}

static int8_t UsbStorageFS_GetMaxLun() {
    return 0;
}

#endif

} // namespace fs_driver
} // namespace eez

////////////////////////////////////////////////////////////////////////////////

#if defined(EEZ_PLATFORM_STM32)

extern const int8_t STORAGE_Inquirydata_FS[];

USBD_StorageTypeDef g_fsDriver_USBD_Storage_Interface = {
    eez::fs_driver::UsbStorageFS_Init,
	eez::fs_driver::UsbStorageFS_GetCapacity,
	eez::fs_driver::UsbStorageFS_IsReady,
	eez::fs_driver::UsbStorageFS_IsWriteProtected,
	eez::fs_driver::UsbStorageFS_Read,
	eez::fs_driver::UsbStorageFS_Write,
	eez::fs_driver::UsbStorageFS_GetMaxLun,
    (int8_t *)STORAGE_Inquirydata_FS
};

#endif
