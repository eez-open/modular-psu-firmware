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

#if OPTION_SD_CARD

#include <string.h>

#include <eez/system.h>

#include <eez/libs/sd_fat/sd_fat.h>

#include <eez/modules/psu/gui/file_manager.h>
#include <eez/modules/psu/psu.h>
#include <eez/modules/psu/sd_card.h>
#include <eez/modules/psu/persist_conf.h>
#include <eez/modules/psu/datetime.h>

#if defined(EEZ_PLATFORM_STM32)
#include <eez/platform/stm32/defines.h>
#endif

#include <eez/scpi/scpi.h>

namespace eez {
namespace gui {
namespace file_manager {

static int g_loading;
static uint32_t g_loadingStartTickCount;

#if defined(EEZ_PLATFORM_STM32)
static uint8_t *g_buffer = (uint8_t *)FILE_MANAGER_MEMORY;
#else
static const int FILE_MANAGER_MEMORY_SIZE = 256 * 1024;
static uint8_t g_buffer[FILE_MANAGER_MEMORY_SIZE];
#endif

static char g_currentDirectory[MAX_PATH_LENGTH + 1];

struct FileItem {
    FileType type;
    const char *name;
    uint32_t size;
    uint32_t dateTime;
};

static uint8_t *g_frontBufferPosition;
static uint8_t *g_backBufferPosition;

int g_filesCount;

void catalogCallback(void *param, const char *name, FileType type, size_t size) {
    auto fileInfo = (FileInfo *)param;

    size_t nameLen = 4 * ((strlen(name) + 1 + 3) / 4);

    if (g_frontBufferPosition + sizeof(FileItem*) > g_backBufferPosition - sizeof(FileItem) - nameLen) {
        return;
    }

    g_backBufferPosition -= sizeof(FileItem);
    auto fileItem = (FileItem *)g_backBufferPosition;

    fileItem->type = type;

    g_backBufferPosition -= nameLen;
    strcpy((char *)g_backBufferPosition, name);
    fileItem->name = (const char *)g_backBufferPosition;

    fileItem->size = size;

    int year = fileInfo->getModifiedYear();
    int month = fileInfo->getModifiedMonth();
    int day = fileInfo->getModifiedDay();

    int hour = fileInfo->getModifiedHour();
    int minute = fileInfo->getModifiedMinute();
    int second = fileInfo->getModifiedSecond();

    uint32_t utc = psu::datetime::makeTime(year, month, day, hour, minute, second);
    uint32_t local = psu::datetime::utcToLocal(utc, psu::persist_conf::devConf.time_zone, (psu::datetime::DstRule)psu::persist_conf::devConf.dstRule);

    fileItem->dateTime = local;

    *((FileItem **)g_frontBufferPosition) = fileItem;
    g_frontBufferPosition += sizeof(FileItem *);

    g_filesCount++;
}

void loadDirectory() {
    if (osThreadGetId() != scpi::g_scpiTaskHandle) {
        if (g_loading == 1) {
            return;
        }
        g_loading = 1;
        g_loadingStartTickCount = millis();
        osMessagePut(scpi::g_scpiMessageQueueId, SCPI_QUEUE_MESSAGE(SCPI_QUEUE_MESSAGE_TARGET_NONE, SCPI_QUEUE_MESSAGE_FILE_MANAGER_LOAD_DIRECTORY, 0), osWaitForever);
        return;
    }

    g_frontBufferPosition = g_buffer;
    g_backBufferPosition = g_buffer + FILE_MANAGER_MEMORY_SIZE;

    g_filesCount = 0;

    int numFiles;
    int err;
    psu::sd_card::catalog(g_currentDirectory, 0, catalogCallback, &numFiles, &err);

    g_loading = 2;
}

const char *getCurrentDirectory() {
    return *g_currentDirectory == 0 ? "/<Root directory>" : g_currentDirectory;
}

static FileItem *getFileItem(int fileIndex) {
    if (g_loading != 2) {
        return nullptr;
    }

    if (fileIndex >= g_filesCount) {
        return nullptr;
    }

    return ((FileItem **)g_buffer)[fileIndex];
}

int getStatus() {
    if (g_loading == 0) {
        loadDirectory();
        return 0;
    }

    if (g_loading == 1) {
        if (millis() - g_loadingStartTickCount < 1000) {
            return 0; // STARTING (during first second)
        }
        return 1; // LOADING
    }
    
    return 2; // READY
}

bool isRootDirectory() {
    return g_currentDirectory[0] == 0;
}

void goToParentDirectory() {
    if (g_loading != 2) {
        return;
    }

    char *p = g_currentDirectory + strlen(g_currentDirectory);

    while (p != g_currentDirectory && *p != '/') {
        p--;
    }

    *p = 0;

    loadDirectory();
}

int getFilesCount() {
    return g_filesCount;
}

bool isDirectory(int fileIndex) {
    auto fileItem = getFileItem(fileIndex);
    return fileItem ? fileItem->type == FILE_TYPE_DIRECTORY : false;
}

FileType getFileType(int fileIndex) {
    auto fileItem = getFileItem(fileIndex);
    return fileItem ? fileItem->type : FILE_TYPE_OTHER;
}

const char *getFileName(int fileIndex) {
    auto fileItem = getFileItem(fileIndex);
    return fileItem ? fileItem->name : "";
}

const uint32_t getFileSize(int fileIndex) {
    auto fileItem = getFileItem(fileIndex);
    return fileItem ? fileItem->size : 0;
}

const uint32_t getFileDataTime(int fileIndex) {
    auto fileItem = getFileItem(fileIndex);
    return fileItem ? fileItem->dateTime : 0;
}

void selectFile(int fileIndex) {
    if (g_loading != 2) {
        return;
    }

    auto fileItem = getFileItem(fileIndex);
    if (fileItem && fileItem->type == FILE_TYPE_DIRECTORY) {
        if (strlen(g_currentDirectory) + 1 + strlen(fileItem->name) <= MAX_PATH_LENGTH) {
            strcat(g_currentDirectory, "/");
            strcat(g_currentDirectory, fileItem->name);
            loadDirectory();
        }
    }
}

}
}
} // namespace eez::gui::file_manager

#endif