/*
* EEZ Generic Firmware
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

#include <eez/os.h>
#include <bb3/system.h>

namespace eez {

#define LOW_PRIORITY_THREAD_QUEUE_SIZE 10

enum HighPriorityThreadMessage {
    PSU_MESSAGE_TICK,
    PSU_MESSAGE_CHANGE_POWER_STATE,
    PSU_MESSAGE_RESET,
    PSU_MESSAGE_TEST,
    PSU_MESSAGE_SPI_IRQ,
    PSU_MESSAGE_SPI_DMA_TRANSFER_COMPLETED,
    PSU_MESSAGE_ADC_MEASURE_ALL,
    PSU_MESSAGE_TRIGGER_START_IMMEDIATELY,
    PSU_MESSAGE_TRIGGER_ABORT,
    PSU_MESSAGE_TRIGGER_CHANNEL_SAVE_AND_DISABLE_OE,
    PSU_MESSAGE_TRIGGER_CHANNEL_RESTORE_OE,
    PSU_MESSAGE_SET_COUPLING_TYPE,
    PSU_MESSAGE_SET_TRACKING_CHANNELS,
    PSU_MESSAGE_SYNC_OUTPUT_ENABLE,
    PSU_MESSAGE_HARD_RESET,
    PSU_MESSAGE_SHUTDOWN,
    PSU_MESSAGE_SET_VOLTAGE,
    PSU_MESSAGE_SET_CURRENT,
    PSU_MESSAGE_SET_VOLTAGE_LIMIT,
    PSU_MESSAGE_SET_CURRENT_LIMIT,
    PSU_MESSAGE_RESET_CHANNELS_HISTORY,
    PSU_MESSAGE_CALIBRATION_START,
    PSU_MESSAGE_CALIBRATION_SELECT_CURRENT_RANGE,
    PSU_MESSAGE_SAVE_CHANNEL_CALIBRATION,
    PSU_MESSAGE_CALIBRATION_STOP,
    PSU_MESSAGE_FLASH_SLAVE_START,
    PSU_MESSAGE_FLASH_SLAVE_LEAVE_BOOTLOADER_MODE,
    PSU_MESSAGE_RECALL_STATE,
    PSU_MESSAGE_SLOT_SET_ENABLED,
    PSU_MESSAGE_SET_CURRENT_RANGE_SELECTION_MODE,
    PSU_MESSAGE_CALIBRATION_ENABLE,
    PSU_MESSAGE_REMOTE_SENSING_EANBLE,
    PSU_MESSAGE_READ_CHANNEL_TEMPERATURE,
    PSU_MESSAGE_REMOTE_PROGRAMMING_ENABLE,
    PSU_MESSAGE_SET_DPROG_STATE,
    PSU_MESSAGE_SAVE_SERIAL_NO,
    PSU_MESSAGE_MODULE_RESYNC,
    PSU_MESSAGE_COPY_CHANNEL_TO_CHANNEL,

    // this must be at the end
    PSU_MESSAGE_MODULE_SPECIFIC,
};

enum LowPriorityThreadMessage {
    SERIAL_INPUT_AVAILABLE,
    SERIAL_LINE_STATE_CHANGED,
    
    SERIAL_LAST_MESSAGE_TYPE,

    ETHERNET_CONNECTED,
    ETHERNET_CLIENT_CONNECTED,
    ETHERNET_CLIENT_DISCONNECTED,
    ETHERNET_INPUT_AVAILABLE,

    ETHERNET_LAST_MESSAGE_TYPE,

    MP_LOAD_SCRIPT,
    MP_EXECUTE_SCPI,
    MP_STOP_SCRIPT,

    MP_LAST_MESSAGE_TYPE,

    FLOW_EXECUTE_SCPI,
    FLOW_FLUSH_TO_DEBUGGER_MESSAGE,

    THREAD_MESSAGE_SAVE_LIST,
    THREAD_MESSAGE_SD_DETECT_IRQ,
    THREAD_MESSAGE_DLOG_STATE_TRANSITION,
    THREAD_MESSAGE_DLOG_SHOW_FILE,
    THREAD_MESSAGE_DLOG_LOAD_SAMPLES,
    THREAD_MESSAGE_DLOG_LOAD_BOOKMARKS,
    THREAD_MESSAGE_ABORT_DOWNLOADING,
    THREAD_MESSAGE_SCREENSHOT,
    THREAD_MESSAGE_FILE_MANAGER_LOAD_DIRECTORY,
    THREAD_MESSAGE_FILE_MANAGER_UPLOAD_FILE,
    THREAD_MESSAGE_FILE_MANAGER_OPEN_IMAGE_FILE,
    THREAD_MESSAGE_FILE_MANAGER_OPEN_BIT_FILE,
    THREAD_MESSAGE_FILE_MANAGER_DELETE_FILE,
    THREAD_MESSAGE_FILE_MANAGER_RENAME_FILE,
    THREAD_MESSAGE_DLOG_UPLOAD_FILE,
    THREAD_MESSAGE_FLASH_SLAVE_UPLOAD_HEX_FILE,
    THREAD_MESSAGE_SHUTDOWN,
    THREAD_MESSAGE_PROFILE_SAVE,
    THREAD_MESSAGE_RECALL_PROFILE,
    THREAD_MESSAGE_LISTS_PAGE_IMPORT_LIST,
    THREAD_MESSAGE_LISTS_PAGE_EXPORT_LIST,
    THREAD_MESSAGE_LOAD_PROFILE,
    THREAD_MESSAGE_USER_PROFILES_PAGE_SAVE,
    THREAD_MESSAGE_USER_PROFILES_PAGE_RECALL,
    THREAD_MESSAGE_USER_PROFILES_PAGE_IMPORT,
    THREAD_MESSAGE_USER_PROFILES_PAGE_EXPORT,
    THREAD_MESSAGE_USER_PROFILES_PAGE_DELETE,
    THREAD_MESSAGE_USER_PROFILES_PAGE_EDIT_REMARK,
    THREAD_MESSAGE_EVENT_QUEUE_REFRESH,
    THREAD_MESSAGE_SOUND_TICK,
    THREAD_MESSAGE_SELECT_USB_MODE,
    THREAD_MESSAGE_SELECT_USB_DEVICE_CLASS,
    THREAD_MESSAGE_SELECT_USB_MASS_STORAGE_DEVICE,
    THREAD_MESSAGE_USBD_MSC_DATAIN,
    THREAD_MESSAGE_USBD_MSC_DATAOUT,
    THREAD_MESSAGE_GENERATE_ERROR,
    THREAD_MESSAGE_LOAD_CUSTOM_LOGO,
    THREAD_MESSAGE_FS_DRIVER_LINK,
    THREAD_MESSAGE_FS_DRIVER_UNLINK,
    THREAD_MESSAGE_AUTO_START_SCRIPT,

    // this must be at the end
    THREAD_MESSAGE_MODULE_SPECIFIC
};

extern bool g_screenshotGenerating;

void initHighPriorityMessageQueue();
void startHighPriorityThread();

bool isPsuThread();
void sendMessageToPsu(HighPriorityThreadMessage messageType, uint32_t messageParam = 0, uint32_t timeoutMillisec = osWaitForever);

void initLowPriorityMessageQueue();
void startLowPriorityThread();

bool isLowPriorityThreadAlive();
bool isLowPriorityThread();

void sendMessageToLowPriorityThread(LowPriorityThreadMessage messageType, uint32_t messageParam = 0, uint32_t timeoutMillisec = osWaitForever);

} // namespace eez
