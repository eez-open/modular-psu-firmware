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

#include <eez/modules/psu/psu.h>

#include <string.h>
#include <stdlib.h>

#include <eez/system.h>
#include <eez/mp.h>
#include <eez/memory.h>

#include <eez/gui/gui.h>

#include <eez/scpi/scpi.h>

#include <eez/modules/psu/psu.h>
#include <eez/modules/psu/serial_psu.h>
#if OPTION_ETHERNET
#include <eez/modules/psu/ethernet.h>
#endif
#include <eez/modules/psu/sd_card.h>
#include <eez/modules/psu/persist_conf.h>
#include <eez/modules/psu/datetime.h>
#include <eez/modules/psu/event_queue.h>
#include <eez/modules/psu/persist_conf.h>
#include <eez/modules/psu/scpi/psu.h>
#include <eez/modules/psu/gui/file_manager.h>
#include <eez/modules/psu/gui/keypad.h>
#include <eez/modules/psu/dlog_view.h>


#include <eez/libs/sd_fat/sd_fat.h>

#include <eez/libs/image/jpeg.h>

namespace eez {
namespace gui {
namespace file_manager {

static State g_state;
static uint32_t g_loadingStartTickCount;

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

int32_t g_selectedFileIndex = -1;

bool g_imageLoadFailed;
uint8_t *g_openedImagePixels;

bool g_fileBrowserMode;
const char *g_fileBrowserTitle;
FileType g_fileBrowserFileType;
static void (*g_fileBrowserOnFileSelected)(const char *filePath);

void catalogCallback(void *param, const char *name, FileType type, size_t size) {
    if (g_fileBrowserMode && type != FILE_TYPE_DIRECTORY && type != g_fileBrowserFileType) {
        return;
    }

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
    qsort(FILE_MANAGER_MEMORY, g_filesCount, sizeof(FileItem), compareFunc);
}

void loadDirectory() {
    if (g_state == STATE_LOADING) {
        return;
    }

    g_state = STATE_LOADING;
    g_filesCount = 0;
    g_selectedFileIndex = -1;
    g_filesStartPosition = 0;
    g_loadingStartTickCount = millis();

    if (osThreadGetId() != scpi::g_scpiTaskHandle) {
        osMessagePut(scpi::g_scpiMessageQueueId, SCPI_QUEUE_MESSAGE(SCPI_QUEUE_MESSAGE_TARGET_NONE, SCPI_QUEUE_MESSAGE_TYPE_FILE_MANAGER_LOAD_DIRECTORY, 0), osWaitForever);
    } else {
        doLoadDirectory();
    }
}

void doLoadDirectory() {
    if (g_state != STATE_LOADING) {
        return;
    }

    g_frontBufferPosition = FILE_MANAGER_MEMORY;
    g_backBufferPosition = FILE_MANAGER_MEMORY + FILE_MANAGER_MEMORY_SIZE;

    int numFiles;
    int err;
    psu::sd_card::catalog(g_currentDirectory, 0, catalogCallback, &numFiles, &err);

    sort();

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

    return (FileItem *)(FILE_MANAGER_MEMORY + fileIndex * sizeof(FileItem));
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
    if (g_filesStartPosition + getFilesPageSize() > getFilesCount()) {
        if (getFilesCount() > getFilesPageSize()) {
            g_filesStartPosition = getFilesCount() - getFilesPageSize();
        } else {
            g_filesStartPosition = 0;
        }
    }
}

uint32_t getFilesPageSize() {
    static const uint32_t FILE_BROWSER_FILES_PAGE_SIZE = 5;
    static const uint32_t FILE_MANAGER_FILES_PAGE_SIZE = 6;

    return g_fileBrowserMode ? FILE_BROWSER_FILES_PAGE_SIZE : FILE_MANAGER_FILES_PAGE_SIZE;
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

bool isFileSelected(uint32_t fileIndex) {
    return (isPageOnStack(PAGE_ID_FILE_BROWSER) || isPageOnStack(PAGE_ID_FILE_MENU)) && g_selectedFileIndex == (int32_t)fileIndex;
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
        if (!g_fileBrowserMode) {
            pushPage(PAGE_ID_FILE_MENU);
        }
    }
}

bool isOpenFileEnabled() {
    auto fileItem = getFileItem(g_selectedFileIndex);
    if (!fileItem) {
        return false;
    }

    if (fileItem->type == FILE_TYPE_DLOG) {
        return true;
    }

    if (fileItem->type == FILE_TYPE_IMAGE) {
        return true;
    }

    if (fileItem->type == FILE_TYPE_MICROPYTHON) {
        return mp::isIdle();
    }

    return false;
}

static void checkImageLoadingStatus() {
    if (g_openedImagePixels) {
        gui::popPage();
        gui::pushPage(gui::PAGE_ID_IMAGE_VIEW);
    } else if (g_imageLoadFailed) {
        gui::popPage();
        gui::errorMessage("Failed to load image!");
    }
}

void openFile() {
    popPage();

    auto fileItem = getFileItem(g_selectedFileIndex);
    if (!fileItem) {
        return;
    }

    if (strlen(g_currentDirectory) + 1 + strlen(fileItem->name) > MAX_PATH_LENGTH) {
        errorMessage(Value(SCPI_ERROR_FILE_NAME_ERROR, VALUE_TYPE_SCPI_ERROR));
        return;
    }

    char filePath[MAX_PATH_LENGTH + 1];
    strcpy(filePath, g_currentDirectory);
    strcat(filePath, "/");
    strcat(filePath, fileItem->name);

    if (fileItem->type == FILE_TYPE_DLOG) {
        psu::dlog_view::g_showLatest = false;
        psu::dlog_view::openFile(filePath);
        gui::pushPage(gui::PAGE_ID_DLOG_VIEW);
    } else if (fileItem->type == FILE_TYPE_IMAGE) {
        g_imageLoadFailed = false;
        g_openedImagePixels = nullptr;
        osMessagePut(scpi::g_scpiMessageQueueId, SCPI_QUEUE_MESSAGE(SCPI_QUEUE_MESSAGE_TARGET_NONE, SCPI_QUEUE_MESSAGE_TYPE_FILE_MANAGER_OPEN_IMAGE_FILE, 0), osWaitForever);
        gui::showAsyncOperationInProgress("Loading...", checkImageLoadingStatus);
    } else if (fileItem->type == FILE_TYPE_MICROPYTHON) {
        mp::startScript(filePath);
    }
}

void openImageFile() {
    auto fileItem = getFileItem(g_selectedFileIndex);
    if (fileItem) {
        if (strlen(g_currentDirectory) + 1 + strlen(fileItem->name) > MAX_PATH_LENGTH) {
            errorMessage(Value(SCPI_ERROR_FILE_NAME_ERROR, VALUE_TYPE_SCPI_ERROR));
            return;
        }

        char filePath[MAX_PATH_LENGTH + 1];
        strcpy(filePath, g_currentDirectory);
        strcat(filePath, "/");
        strcat(filePath, fileItem->name);
        
        g_openedImagePixels = jpegDecode(filePath);
        if (!g_openedImagePixels) {
            g_imageLoadFailed = true;
        }
    }
}

uint8_t *getOpenedImagePixels() {
    return g_openedImagePixels;
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
        osMessagePut(scpi::g_scpiMessageQueueId, SCPI_QUEUE_MESSAGE(SCPI_QUEUE_MESSAGE_TARGET_NONE, SCPI_QUEUE_MESSAGE_TYPE_FILE_MANAGER_UPLOAD_FILE, 0), osWaitForever);
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

    if (strlen(g_currentDirectory) + 1 + strlen(fileItem->name) > MAX_PATH_LENGTH) {
        errorMessage(Value(SCPI_ERROR_FILE_NAME_ERROR, VALUE_TYPE_SCPI_ERROR));
        return;
    }

    char filePath[MAX_PATH_LENGTH + 1];
    strcpy(filePath, g_currentDirectory);
    strcat(filePath, "/");
    strcat(filePath, fileItem->name);

    int err;
    if (!psu::scpi::mmemUpload(filePath, context, &err)) {
        errorMessage(Value(err, VALUE_TYPE_SCPI_ERROR));
    }
}

bool isRenameFileEnabled() {
    return true;
}

static char g_fileNameWithoutExtension[MAX_PATH_LENGTH + 1];

void onRenameFileOk(char *fileNameWithoutExtension) {
    strcpy(g_fileNameWithoutExtension, fileNameWithoutExtension);

    if (osThreadGetId() != scpi::g_scpiTaskHandle) {
        popPage();
        osMessagePut(scpi::g_scpiMessageQueueId, SCPI_QUEUE_MESSAGE(SCPI_QUEUE_MESSAGE_TARGET_NONE, SCPI_QUEUE_MESSAGE_TYPE_FILE_MANAGER_RENAME_FILE, 0), osWaitForever);
    } else {
        doRenameFile();
    }
}

void doRenameFile() {
    auto fileItem = getFileItem(g_selectedFileIndex);
    if (!fileItem) {
        return;
    }

    // source file path
    if (strlen(g_currentDirectory) + 1 + strlen(fileItem->name) > MAX_PATH_LENGTH) {
        errorMessage(Value(SCPI_ERROR_FILE_NAME_ERROR, VALUE_TYPE_SCPI_ERROR));
        return;
    }
    char srcFilePath[MAX_PATH_LENGTH + 1];
    strcpy(srcFilePath, g_currentDirectory);
    strcat(srcFilePath, "/");
    strcat(srcFilePath, fileItem->name);

    // destination file path
    const char *extension = strrchr(fileItem->name, '.');
    if (strlen(g_currentDirectory) + 1 + strlen(g_fileNameWithoutExtension) + (extension ? strlen(extension) : 0) > MAX_PATH_LENGTH) {
        errorMessage(Value(SCPI_ERROR_FILE_NAME_ERROR, VALUE_TYPE_SCPI_ERROR));
        return;
    }
    char dstFilePath[MAX_PATH_LENGTH + 1];
    strcpy(dstFilePath, g_currentDirectory);
    strcat(dstFilePath, "/");
    strcat(dstFilePath, g_fileNameWithoutExtension);
    if (extension) {
        strcat(dstFilePath, extension);
    }

    int err;
    if (!psu::sd_card::moveFile(srcFilePath, dstFilePath, &err)) {
        errorMessage(Value(err, VALUE_TYPE_SCPI_ERROR));
    }
}

void renameFile() {
    popPage();

    auto fileItem = getFileItem(g_selectedFileIndex);
    if (!fileItem) {
        return;
    }

    char fileNameWithoutExtension[MAX_PATH_LENGTH + 1];
    
    const char *str = strrchr(fileItem->name, '.');
    if (str) {
        auto n = str - fileItem->name;
        strncpy(fileNameWithoutExtension, fileItem->name, n);
        fileNameWithoutExtension[n] = 0;
    } else {
        strcpy(fileNameWithoutExtension, fileItem->name);
    }

    psu::gui::Keypad::startPush(0, fileNameWithoutExtension, 1, MAX_PATH_LENGTH, false, onRenameFileOk, popPage);
}

bool isDeleteFileEnabled() {
    return true;
}

void deleteFile() {
    if (osThreadGetId() != scpi::g_scpiTaskHandle) {
        popPage();
        osMessagePut(scpi::g_scpiMessageQueueId, SCPI_QUEUE_MESSAGE(SCPI_QUEUE_MESSAGE_TARGET_NONE, SCPI_QUEUE_MESSAGE_TYPE_FILE_MANAGER_DELETE_FILE, 0), osWaitForever);
        return;
    }

    auto fileItem = getFileItem(g_selectedFileIndex);
    if (!fileItem) {
        return;
    }

    if (strlen(g_currentDirectory) + 1 + strlen(fileItem->name) > MAX_PATH_LENGTH) {
        errorMessage(Value(SCPI_ERROR_FILE_NAME_ERROR, VALUE_TYPE_SCPI_ERROR));
        return;
    }

    char filePath[MAX_PATH_LENGTH + 1];
    strcpy(filePath, g_currentDirectory);
    strcat(filePath, "/");
    strcat(filePath, fileItem->name);

    int err;
    if (!psu::sd_card::deleteFile(filePath, &err)) {
        errorMessage(Value(err, VALUE_TYPE_SCPI_ERROR));
    }
}

void onEncoder(int counter) {
#if defined(EEZ_PLATFORM_SIMULATOR)
    counter = -counter;
#endif
    int32_t newPosition = getFilesStartPosition() + counter;
    if (newPosition < 0) {
        newPosition = 0;
    }
    setFilesStartPosition(newPosition);
}

void openFileManager() {
    g_fileBrowserMode = false;
    loadDirectory();
    showPage(PAGE_ID_FILE_MANAGER);
}

int FileBrowserPage::getDirty() {
    return g_selectedFileIndex != -1;
}

bool FileBrowserPage::showAreYouSureOnDiscard() {
    return false;
}

void FileBrowserPage::set() {
    popPage();

    auto fileItem = getFileItem(g_selectedFileIndex);
    if (!fileItem) {
        return;
    }

    if (strlen(g_currentDirectory) + 1 + strlen(fileItem->name) > MAX_PATH_LENGTH) {
        errorMessage(Value(SCPI_ERROR_FILE_NAME_ERROR, VALUE_TYPE_SCPI_ERROR));
        return;
    }

    char filePath[MAX_PATH_LENGTH + 1];
    strcpy(filePath, g_currentDirectory);
    strcat(filePath, "/");
    strcat(filePath, fileItem->name);

    g_fileBrowserOnFileSelected(filePath);
}

void browseForFile(const char *title, const char *directory, FileType fileType, void(*onFileSelected)(const char *filePath)) {
    g_fileBrowserMode = true;
    g_fileBrowserTitle = title;
    g_fileBrowserFileType = fileType;
    g_fileBrowserOnFileSelected = onFileSelected;

    strcpy(g_currentDirectory, directory);
    loadDirectory();

    pushPage(PAGE_ID_FILE_BROWSER);
}

}
}

using namespace gui::file_manager;

void onSdCardFileChangeHook(const char *filePath1, const char *filePath2) {
	if (g_fileBrowserMode) {
		return;
	}

	char parentDirPath1[MAX_PATH_LENGTH + 1];
    getParentDir(filePath1, parentDirPath1);
    if (strcmp(parentDirPath1, g_currentDirectory) == 0) {
        loadDirectory();
        return;
    }

    if (filePath2) {
        onSdCardFileChangeHook(filePath2);
    }
}

} // namespace eez::gui::file_manager
