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

#include <eez/modules/psu/psu.h>

#include <string.h>
#include <stdlib.h>

#include <eez/system.h>

#include <eez/scpi/scpi.h>

#include <eez/modules/psu/psu.h>
#include <eez/modules/psu/serial_psu.h>
#if OPTION_ETHERNET
#include <eez/modules/psu/ethernet.h>
#endif
#include <eez/modules/psu/sd_card.h>
#include <eez/modules/psu/persist_conf.h>
#include <eez/modules/psu/datetime.h>
#include <eez/modules/psu/persist_conf.h>
#include <eez/modules/psu/scpi/psu.h>
#include <eez/modules/psu/gui/file_manager.h>
#include <eez/modules/psu/dlog_view.h>

#if defined(EEZ_PLATFORM_STM32)
#include <eez/platform/stm32/defines.h>
#endif

#include <eez/libs/sd_fat/sd_fat.h>


namespace eez {
namespace gui {
namespace file_manager {

static State g_state;
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

uint32_t g_filesCount;
uint32_t g_filesStartPosition;

uint32_t g_selectedFileIndex;

void catalogCallback(void *param, const char *name, FileType type, size_t size) {
    auto fileInfo = (FileInfo *)param;

    size_t nameLen = 4 * ((strlen(name) + 1 + 3) / 4);

    if (g_frontBufferPosition + sizeof(FileItem) > g_backBufferPosition - nameLen) {
        return;
    }

    auto fileItem = (FileItem *)g_frontBufferPosition;
    g_frontBufferPosition += sizeof(FileItem);

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

    fileItem->dateTime = psu::datetime::makeTime(year, month, day, hour, minute, second);

    g_filesCount++;
}

int compareFunc(const void *p1, const void *p2) {
    FileItem *item1 = (FileItem *)p1;
    FileItem *item2 = (FileItem *)p2;
    if (psu::persist_conf::devConf.sortFilesOption == SORT_FILES_BY_NAME_ASC) {
        return strcicmp(item1->name, item2->name);
    } else if (psu::persist_conf::devConf.sortFilesOption == SORT_FILES_BY_NAME_DESC) {
        return -strcicmp(item1->name, item2->name);
    } else if (psu::persist_conf::devConf.sortFilesOption == SORT_FILES_BY_SIZE_ASC) {
        return item1->size - item2->size;
    } else if (psu::persist_conf::devConf.sortFilesOption == SORT_FILES_BY_SIZE_DESC) {
        return item2->size - item1->size;
    } else if (psu::persist_conf::devConf.sortFilesOption == SORT_FILES_BY_TIME_ASC) {
        return item1->dateTime - item2->dateTime;
    } else {
        return item2->dateTime - item1->dateTime;
    }
} 

void sort() {
    qsort(g_buffer, g_filesCount, sizeof(FileItem), compareFunc);
}

void loadDirectory() {
    if (osThreadGetId() != scpi::g_scpiTaskHandle) {
        if (g_state == STATE_LOADING) {
            return;
        }
        g_state = STATE_LOADING;
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

    sort();
    g_filesStartPosition = 0;

    g_state = STATE_READY;
}

SortFilesOption getSortFilesOption() {
    return psu::persist_conf::devConf.sortFilesOption;
}

void setSortFilesOption(SortFilesOption sortFilesOption) {
    psu::persist_conf::setSortFilesOption(sortFilesOption);
    sort();
    g_filesStartPosition = 0;
}

const char *getCurrentDirectory() {
    return *g_currentDirectory == 0 ? "/<Root directory>" : g_currentDirectory;
}

static FileItem *getFileItem(uint32_t fileIndex) {
    if (g_state != STATE_READY) {
        return nullptr;
    }

    if (fileIndex >= g_filesCount) {
        return nullptr;
    }

    return (FileItem *)(g_buffer + fileIndex * sizeof(FileItem));
}

State getState() {
    if (g_state == STATE_STARTING) {
        loadDirectory();
        return STATE_STARTING;
    }

    if (g_state == STATE_LOADING) {
        if (millis() - g_loadingStartTickCount < 1000) {
            return STATE_STARTING; // during 1st second of loading
        }
        return STATE_LOADING;
    }
    
    return STATE_READY;
}

bool isRootDirectory() {
    return g_currentDirectory[0] == 0;
}

void goToParentDirectory() {
    if (g_state != STATE_READY) {
        return;
    }

    char *p = g_currentDirectory + strlen(g_currentDirectory);

    while (p != g_currentDirectory && *p != '/') {
        p--;
    }

    *p = 0;

    loadDirectory();
}

uint32_t getFilesCount() {
    return g_filesCount;
}

uint32_t getFilesStartPosition() {
    return g_filesStartPosition;
}

void setFilesStartPosition(uint32_t position) {
    g_filesStartPosition = position;
    if (g_filesStartPosition + FILES_PAGE_SIZE > getFilesCount()) {
        if (getFilesCount() > FILES_PAGE_SIZE) {
            g_filesStartPosition = getFilesCount() - FILES_PAGE_SIZE;
        } else {
            g_filesStartPosition = 0;
        }
    }
}

bool isDirectory(uint32_t fileIndex) {
    auto fileItem = getFileItem(fileIndex);
    return fileItem ? fileItem->type == FILE_TYPE_DIRECTORY : false;
}

FileType getFileType(uint32_t fileIndex) {
    auto fileItem = getFileItem(fileIndex);
    return fileItem ? fileItem->type : FILE_TYPE_OTHER;
}

const char *getFileName(uint32_t fileIndex) {
    auto fileItem = getFileItem(fileIndex);
    return fileItem ? fileItem->name : "";
}

const uint32_t getFileSize(uint32_t fileIndex) {
    auto fileItem = getFileItem(fileIndex);
    return fileItem ? fileItem->size : 0;
}

const uint32_t getFileDataTime(uint32_t fileIndex) {
    auto fileItem = getFileItem(fileIndex);
    return fileItem ? fileItem->dateTime : 0;
}

void selectFile(uint32_t fileIndex) {
    if (g_state != STATE_READY) {
        return;
    }

    auto fileItem = getFileItem(fileIndex);
    if (fileItem && fileItem->type == FILE_TYPE_DIRECTORY) {
        if (strlen(g_currentDirectory) + 1 + strlen(fileItem->name) <= MAX_PATH_LENGTH) {
            strcat(g_currentDirectory, "/");
            strcat(g_currentDirectory, fileItem->name);
            loadDirectory();
        }
    } else {
        g_selectedFileIndex = fileIndex;
        pushPage(PAGE_ID_FILE_MENU);
    }
}

bool isOpenFileEnabled() {
    auto fileItem = getFileItem(g_selectedFileIndex);
    return fileItem && fileItem->type == FILE_TYPE_DLOG;
}

void openFile() {
    popPage();

    auto fileItem = getFileItem(g_selectedFileIndex);
    if (fileItem && fileItem->type == FILE_TYPE_DLOG) {
        char filePath[MAX_PATH_LENGTH + 1];
        strcpy(filePath, g_currentDirectory);
        strcat(filePath, "/");
        strcat(filePath, fileItem->name);
        psu::dlog_view::g_showLatest = false;
        psu::dlog_view::openFile(filePath);
        gui::pushPage(gui::PAGE_ID_DLOG_VIEW);
    }
}

bool isUploadFileEnabled() {
#if !defined(EEZ_PLATFORM_SIMULATOR)
    if (psu::serial::isConnected()) {
        return true;
    }
#endif

#if OPTION_ETHERNET
    if (psu::ethernet::isConnected()) {
        return true;
    }
#endif
    return false;
}

void uploadFile() {
    if (osThreadGetId() != scpi::g_scpiTaskHandle) {
        popPage();
        osMessagePut(scpi::g_scpiMessageQueueId, SCPI_QUEUE_MESSAGE(SCPI_QUEUE_MESSAGE_TARGET_NONE, SCPI_QUEUE_MESSAGE_FILE_MANAGER_UPLOAD_FILE, 0), osWaitForever);
        return;
    }

    auto fileItem = getFileItem(g_selectedFileIndex);
    if (!fileItem) {
        return;
    }

    scpi_t *context = nullptr;

#if !defined(EEZ_PLATFORM_SIMULATOR)
    if (psu::serial::isConnected()) {
        context = &psu::serial::g_scpiContext;
    }
#endif

#if OPTION_ETHERNET
    if (!context && psu::ethernet::isConnected()) {
        context = &psu::ethernet::g_scpiContext;
    }
#endif

    if (!context) {
        return;
    }

    char filePath[MAX_PATH_LENGTH + 1];
    strcpy(filePath, g_currentDirectory);
    strcat(filePath, "/");
    strcat(filePath, fileItem->name);

    int err;
    psu::scpi::mmemUpload(filePath, context, &err);
}

bool isRenameFileEnabled() {
    return false;
}

void renameFile() {
}

bool isDeleteFileEnabled() {
    return false;
}

void deleteFile() {
}

void onEncoder(int counter) {
    int32_t newPosition = getFilesStartPosition() + counter;
    if (newPosition < 0) {
        newPosition = 0;
    }
    setFilesStartPosition(newPosition);
}

}
}
} // namespace eez::gui::file_manager

#endif
