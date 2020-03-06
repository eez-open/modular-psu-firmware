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
#include <eez/gui/widgets/container.h>

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
#include <eez/modules/psu/sd_card.h>
#include <eez/modules/psu/dlog_view.h>

#include <eez/modules/psu/scpi/psu.h>

#include <eez/modules/psu/gui/psu.h>
#include <eez/modules/psu/gui/data.h>
#include <eez/modules/psu/gui/file_manager.h>
#include <eez/modules/psu/gui/keypad.h>

#include <eez/libs/sd_fat/sd_fat.h>

#include <eez/libs/image/image.h>

namespace eez {
namespace gui {
namespace file_manager {

static const int CONF_FILE_DESCRIPTION_WIDTH_PX = 243;
static const size_t MAX_FILE_DESCRIPTION_LENGTH = 80;

static State g_state;
static uint32_t g_loadingStartTickCount;

static char g_currentDirectory[MAX_PATH_LENGTH + 1];

struct FileItem {
    FileType type;
    const char *name;
    uint32_t size;
    uint32_t dateTime;
    const char *description;
};

static uint8_t *g_frontBufferPosition;
static uint8_t *g_backBufferPosition;

uint32_t g_filesCount;
uint32_t g_filesStartPosition;
uint32_t g_savedFilesStartPosition;

int32_t g_selectedFileIndex = -1;

bool g_imageLoadFailed;
Image g_openedImage;

bool g_fileBrowserMode;
DialogType g_dialogType;
const char *g_fileBrowserTitle;
FileType g_fileBrowserFileType;
static void (*g_fileBrowserOnFileSelected)(const char *filePath);

static ListViewOption g_rootDirectoryListViewOption = LIST_VIEW_LARGE_ICONS;
static ListViewOption g_scriptsDirectoryListViewOption = LIST_VIEW_SCRIPTS;

void catalogCallback(void *param, const char *name, FileType type, size_t size) {
    if (g_fileBrowserMode && type != FILE_TYPE_DIRECTORY && type != g_fileBrowserFileType) {
        return;
    }

    auto fileInfo = (FileInfo *)param;
 
    char fileNameWithoutExtension[MAX_PATH_LENGTH + 1];

    char description[MAX_FILE_DESCRIPTION_LENGTH + 1];
    size_t descriptionLen = 0;

    if (getListViewOption() == LIST_VIEW_SCRIPTS) {
        if (type != FILE_TYPE_MICROPYTHON) {
            return;
        }

        description[0] = 0;

        char filePath[MAX_PATH_LENGTH + 1];
        strcpy(filePath, g_currentDirectory);
        strcat(filePath, "/");
        strcat(filePath, name);
        File file;
        if (file.open(filePath, FILE_OPEN_EXISTING | FILE_READ)) {
            psu::sd_card::BufferedFileRead bufferedFile(file);

            psu::sd_card::matchZeroOrMoreSpaces(bufferedFile);
            if (psu::sd_card::match(bufferedFile, '#')) {
                psu::sd_card::matchZeroOrMoreSpaces(bufferedFile);
                psu::sd_card::matchUntil(bufferedFile, '\n', description, MAX_FILE_DESCRIPTION_LENGTH);
                description[MAX_FILE_DESCRIPTION_LENGTH] = 0;
            }

            file.close();
        }

        descriptionLen = strlen(description);
        if (descriptionLen > 0) {
            descriptionLen = 4 * ((descriptionLen + 1 + 3) / 4);
        }

        const char *str = strrchr(name, '.');
        if (str) {
            auto n = str - name;
            strncpy(fileNameWithoutExtension, name, n);
            fileNameWithoutExtension[n] = 0;
            name = fileNameWithoutExtension;
        }
    }

    size_t nameLen = 4 * ((strlen(name) + 1 + 3) / 4);

    if (g_frontBufferPosition + sizeof(FileItem) > g_backBufferPosition - nameLen - descriptionLen) {
        return;
    }

    auto fileItem = (FileItem *)g_frontBufferPosition;
    g_frontBufferPosition += sizeof(FileItem);

    fileItem->type = type;

    g_backBufferPosition -= nameLen;
    strcpy((char *)g_backBufferPosition, name);
    fileItem->name = (const char *)g_backBufferPosition;

    if (descriptionLen > 0) {
        g_backBufferPosition -= descriptionLen;
        strcpy((char *)g_backBufferPosition, description);
        fileItem->description = (const char *)g_backBufferPosition;
    } else {
        fileItem->description = nullptr;
    }

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

RootDirectoryType getRootDirectoryType(FileItem *item) {
	if (strcmp(item->name, "Scripts") == 0) {
		return ROOT_DIRECTORY_TYPE_SCRIPTS;
	}
	if (strcmp(item->name, "Screenshots") == 0) {
		return ROOT_DIRECTORY_TYPE_SCREENSHOTS;
	}
	if (strcmp(item->name, "Recordings") == 0) {
		return ROOT_DIRECTORY_TYPE_RECORDINGS;
	}
	if (strcmp(item->name, "Lists") == 0) {
		return ROOT_DIRECTORY_TYPE_LISTS;
	}
	if (strcmp(item->name, "Profiles") == 0) {
		return ROOT_DIRECTORY_TYPE_PROFILES;
	}
	if (strcmp(item->name, "Logs") == 0) {
		return ROOT_DIRECTORY_TYPE_LOGS;
	}
	if (strcmp(item->name, "Updates") == 0) {
		return ROOT_DIRECTORY_TYPE_UPDATES;
	}
	return ROOT_DIRECTORY_TYPE_NONE;
}

int compareFunc(const void *p1, const void *p2, SortFilesOption sortFilesOption) {
    FileItem *item1 = (FileItem *)p1;
    FileItem *item2 = (FileItem *)p2;

    if (getListViewOption() == LIST_VIEW_LARGE_ICONS) {
    	// root directory has special sort order
    	int sortOrder1 = getRootDirectoryType(item1);
    	int sortOrder2 = getRootDirectoryType(item2);
    	if (sortOrder1 && !sortOrder2) {
            return -1;
        }
        if (!sortOrder1 && sortOrder2) {
            return 1;
        }
        if (sortOrder1 && sortOrder2) {
        	return sortOrder1 - sortOrder2;
        }

        if (item1->type == FILE_TYPE_DIRECTORY && item2->type != FILE_TYPE_DIRECTORY) {
            return -1;
        }
        if (item1->type != FILE_TYPE_DIRECTORY && item2->type == FILE_TYPE_DIRECTORY) {
            return 1;
        }
    }

    if (getListViewOption() == LIST_VIEW_SCRIPTS) {
        sortFilesOption = SORT_FILES_BY_NAME_ASC;
    }

    if (sortFilesOption == SORT_FILES_BY_NAME_ASC) {
        return strcicmp(item1->name, item2->name);
    } else if (sortFilesOption == SORT_FILES_BY_NAME_DESC) {
        return -strcicmp(item1->name, item2->name);
    } else if (sortFilesOption == SORT_FILES_BY_SIZE_ASC) {
        return item1->size - item2->size;
    } else if (sortFilesOption == SORT_FILES_BY_SIZE_DESC) {
        return item2->size - item1->size;
    } else if (sortFilesOption == SORT_FILES_BY_TIME_ASC) {
        return item1->dateTime - item2->dateTime;
    } else {
        return item2->dateTime - item1->dateTime;
    }
} 


int compareFunc(const void *p1, const void *p2) {
    int result = compareFunc(p1, p2, psu::persist_conf::devConf.sortFilesOption);
    if (result != 0) {
        return result;
    }

    if (psu::persist_conf::devConf.sortFilesOption == SORT_FILES_BY_NAME_ASC || psu::persist_conf::devConf.sortFilesOption == SORT_FILES_BY_NAME_DESC) {
        int result = compareFunc(p1, p2, SORT_FILES_BY_SIZE_ASC);
        if (result != 0) {
            return result;
        }
        return compareFunc(p1, p2, SORT_FILES_BY_TIME_ASC);
    } else if (psu::persist_conf::devConf.sortFilesOption == SORT_FILES_BY_SIZE_ASC || psu::persist_conf::devConf.sortFilesOption == SORT_FILES_BY_SIZE_DESC) {
        int result = compareFunc(p1, p2, SORT_FILES_BY_NAME_ASC);
        if (result != 0) {
            return result;
        }
        return compareFunc(p1, p2, SORT_FILES_BY_TIME_ASC);
    } else {
        int result = compareFunc(p1, p2, SORT_FILES_BY_NAME_ASC);
        if (result != 0) {
            return result;
        }
        return compareFunc(p1, p2, SORT_FILES_BY_SIZE_ASC);
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
    g_savedFilesStartPosition = g_filesStartPosition;
    g_filesStartPosition = 0;
    g_loadingStartTickCount = millis();

    if (osThreadGetId() != scpi::g_scpiTaskHandle) {
        using namespace scpi;
        osMessagePut(g_scpiMessageQueueId, SCPI_QUEUE_MESSAGE(SCPI_QUEUE_MESSAGE_TARGET_NONE, SCPI_QUEUE_MESSAGE_TYPE_FILE_MANAGER_LOAD_DIRECTORY, 0), osWaitForever);
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
    if (psu::sd_card::catalog(g_currentDirectory, 0, catalogCallback, &numFiles, &err)) {
        sort();
        setFilesStartPosition(g_savedFilesStartPosition);
        g_state = STATE_READY;
    } else {
    	g_state = STATE_NOT_PRESENT;
    }

}

void onSdCardMountedChange() {
	if (psu::sd_card::isMounted(nullptr)) {
		g_state = STATE_STARTING;
	} else {
		g_state = STATE_NOT_PRESENT;
	}
}

bool isListViewOptionAvailable() {
    return isRootDirectory() || isScriptsDirectory();
}

ListViewOption getListViewOption() {
    if (g_fileBrowserMode) {
        return LIST_VIEW_DETAILS;
    }
    if (isRootDirectory()) {
        return g_rootDirectoryListViewOption;
    }
    if (isScriptsDirectory()) {
        return g_scriptsDirectoryListViewOption;
    }
    return LIST_VIEW_DETAILS;
}

int getListViewLayout() {
    if (getListViewOption() == LIST_VIEW_LARGE_ICONS) {
        return PAGE_ID_FILE_MANAGER_LARGE_ICONS_VIEW;
    }
    if (getListViewOption() == LIST_VIEW_SCRIPTS) {
        return PAGE_ID_FILE_MANAGER_SCRIPTS_VIEW;
    }
    return PAGE_ID_FILE_MANAGER_DETAILS_VIEW;
}

void toggleListViewOption() {
    if (isRootDirectory()) {
        if (g_rootDirectoryListViewOption == LIST_VIEW_DETAILS) {
            g_rootDirectoryListViewOption = LIST_VIEW_LARGE_ICONS;
        } else {
            g_rootDirectoryListViewOption = LIST_VIEW_DETAILS;
        }
    } else {
        if (g_scriptsDirectoryListViewOption == LIST_VIEW_DETAILS) {
            g_scriptsDirectoryListViewOption = LIST_VIEW_SCRIPTS;
        } else {
            g_scriptsDirectoryListViewOption = LIST_VIEW_DETAILS;
        }
    }

    loadDirectory();
}

SortFilesOption getSortFilesOption() {
    return psu::persist_conf::devConf.sortFilesOption;
}

void setSortFilesOption(SortFilesOption sortFilesOption) {
    psu::persist_conf::setSortFilesOption(sortFilesOption);

    sort();

    g_filesStartPosition = 0;

    if (!g_fileBrowserMode) {
        g_selectedFileIndex = -1;
    }
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
    
    return g_state;
}

bool isRootDirectory() {
    return g_currentDirectory[0] == 0 || strcmp(g_currentDirectory, "/") == 0;
}

bool isScriptsDirectory() {
    return strcmp(g_currentDirectory, SCRIPTS_DIR) == 0;
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

    g_filesStartPosition = 0;
    loadDirectory();
}

uint32_t getFilesCount() {
    return (g_filesCount + getFilesPositionIncrement() - 1) / getFilesPositionIncrement() * getFilesPositionIncrement();
}

uint32_t getFilesStartPosition() {
    return g_filesStartPosition;
}

void setFilesStartPosition(uint32_t position) {
    g_filesStartPosition = (position + getFilesPositionIncrement() - 1) / getFilesPositionIncrement() * getFilesPositionIncrement();
    if (g_filesStartPosition + getFilesPageSize() > getFilesCount()) {
        if (getFilesCount() > getFilesPageSize()) {
            g_filesStartPosition = getFilesCount() - getFilesPageSize();
        } else {
            g_filesStartPosition = 0;
        }
    }

    if (!g_fileBrowserMode) {
        g_selectedFileIndex = -1;
    }
}

static const int NUM_COLUMNS_IN_LARGE_ICONS_VIEW = 4;
static const int NUM_ROWS_IN_LARGE_ICONS_VIEW = 2;

uint32_t getFilesPositionIncrement() {
    return getListViewOption() == LIST_VIEW_LARGE_ICONS ? NUM_COLUMNS_IN_LARGE_ICONS_VIEW : 1;
}

uint32_t getFilesPageSize() {
    static const uint32_t FILE_BROWSER_FILES_PAGE_SIZE = 5;
    static const uint32_t FILE_MANAGER_FILES_PAGE_SIZE_IN_DETAILS_VIEW = 6;
    static const uint32_t FILE_MANAGER_FILES_PAGE_SIZE_IN_LARGE_ICONS_VIEW = NUM_COLUMNS_IN_LARGE_ICONS_VIEW * NUM_ROWS_IN_LARGE_ICONS_VIEW;
    static const uint32_t FILE_MANAGER_FILES_PAGE_SIZE_IN_SCRIPTS_VIEW = 3;

    if (g_fileBrowserMode) {
        return FILE_BROWSER_FILES_PAGE_SIZE;
    }

    if (getListViewOption() == LIST_VIEW_LARGE_ICONS) {
        return FILE_MANAGER_FILES_PAGE_SIZE_IN_LARGE_ICONS_VIEW;
    }
    
    if (getListViewOption() == LIST_VIEW_SCRIPTS) {
        return FILE_MANAGER_FILES_PAGE_SIZE_IN_SCRIPTS_VIEW;
    }

    return FILE_MANAGER_FILES_PAGE_SIZE_IN_DETAILS_VIEW;
}

bool isDirectory(uint32_t fileIndex) {
    auto fileItem = getFileItem(fileIndex);
    return fileItem ? fileItem->type == FILE_TYPE_DIRECTORY : false;
}

FileType getFileType(uint32_t fileIndex) {
    auto fileItem = getFileItem(fileIndex);
    return fileItem ? fileItem->type : FILE_TYPE_NONE;
}

RootDirectoryType getRootDirectoryType(uint32_t fileIndex) {
    auto fileItem = getFileItem(fileIndex);
    return fileItem ? getRootDirectoryType(fileItem) : ROOT_DIRECTORY_TYPE_NONE;
}

const char *getFileIcon(uint32_t fileIndex) {
    auto fileType = getFileType(fileIndex);

    if (getListViewOption() == LIST_VIEW_DETAILS || getListViewOption() == LIST_VIEW_SCRIPTS) {
        return getFileTypeSmallIcon(fileType);
    } 
    
    if (fileType == FILE_TYPE_DIRECTORY) {
        return getRootDirectoryIcon(getRootDirectoryType(fileIndex));
    }

    return getFileTypeLargeIcon(fileType);
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

const char *getFileDescription(uint32_t fileIndex) {
    auto fileItem = getFileItem(fileIndex);
    return fileItem && fileItem->description ? fileItem->description : "";
}

bool isFileSelected(uint32_t fileIndex) {
    return (isPageOnStack(PAGE_ID_FILE_BROWSER) || isPageOnStack(PAGE_ID_FILE_MENU)) && g_selectedFileIndex == (int32_t)fileIndex;
}

bool isSelectFileActionEnabled(uint32_t fileIndex) {
    return fileIndex < g_filesCount;
}

void selectFile(uint32_t fileIndex) {
    if (g_state != STATE_READY) {
        return;
    }

    if (fileIndex < g_filesCount) {
        auto fileItem = getFileItem(fileIndex);
        if (fileItem && fileItem->type == FILE_TYPE_DIRECTORY) {
            if (strlen(g_currentDirectory) + 1 + strlen(fileItem->name) <= MAX_PATH_LENGTH) {
                strcat(g_currentDirectory, "/");
                strcat(g_currentDirectory, fileItem->name);
                g_filesStartPosition = 0;
                loadDirectory();
            }
        } else {
            g_selectedFileIndex = fileIndex;
            if (!g_fileBrowserMode) {
                if (getListViewOption() == LIST_VIEW_SCRIPTS) {
                    if (mp::isIdle()) {
                        char filePath[MAX_PATH_LENGTH + 1];
                        strcpy(filePath, g_currentDirectory);
                        strcat(filePath, "/");
                        strcat(filePath, fileItem->name);
                        strcat(filePath, ".py");

                        mp::startScript(filePath);
                    } else {
                        infoMessage("Script is already running!");
                    }
                } else {
                    pushPage(PAGE_ID_FILE_MENU);
                }
            }
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
    if (g_openedImage.pixels) {
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
        g_openedImage.pixels = nullptr;
        using namespace scpi;
        osMessagePut(g_scpiMessageQueueId, SCPI_QUEUE_MESSAGE(SCPI_QUEUE_MESSAGE_TARGET_NONE, SCPI_QUEUE_MESSAGE_TYPE_FILE_MANAGER_OPEN_IMAGE_FILE, 0), osWaitForever);
        psu::gui::showAsyncOperationInProgress("Loading...", checkImageLoadingStatus);
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

        if (!imageDecode(filePath, &g_openedImage)) {
            g_imageLoadFailed = true;
        }            
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
        using namespace scpi;
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
        using namespace scpi;
        osMessagePut(g_scpiMessageQueueId, SCPI_QUEUE_MESSAGE(SCPI_QUEUE_MESSAGE_TARGET_NONE, SCPI_QUEUE_MESSAGE_TYPE_FILE_MANAGER_RENAME_FILE, 0), osWaitForever);
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
        using namespace scpi;
        osMessagePut(g_scpiMessageQueueId, SCPI_QUEUE_MESSAGE(SCPI_QUEUE_MESSAGE_TARGET_NONE, SCPI_QUEUE_MESSAGE_TYPE_FILE_MANAGER_DELETE_FILE, 0), osWaitForever);
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
    counter *= getFilesPositionIncrement();
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

void browseForFile(const char *title, const char *directory, FileType fileType, DialogType dialogType, void(*onFileSelected)(const char *filePath)) {
    g_fileBrowserMode = true;
    g_dialogType = dialogType;
    g_fileBrowserTitle = title;
    g_fileBrowserFileType = fileType;
    g_fileBrowserOnFileSelected = onFileSelected;

    strcpy(g_currentDirectory, directory);
    loadDirectory();

    pushPage(PAGE_ID_FILE_BROWSER);
}

bool isSaveDialog() {
    return g_fileBrowserMode && g_dialogType == DIALOG_TYPE_SAVE;
}

void onNewFileOk(char *fileNameWithoutExtension) {
    popPage();

    const char *extension = getExtensionFromFileType(g_fileBrowserFileType);
    if (endsWithNoCase(fileNameWithoutExtension, extension)) {
        extension = "";
    }

    if (strlen(g_currentDirectory) + 1 + strlen(fileNameWithoutExtension) + strlen(extension) > MAX_PATH_LENGTH) {
        errorMessage(Value(SCPI_ERROR_FILE_NAME_ERROR, VALUE_TYPE_SCPI_ERROR));
        return;
    }

    char filePath[MAX_PATH_LENGTH + 1];
    strcpy(filePath, g_currentDirectory);
    strcat(filePath, "/");
    strcat(filePath, fileNameWithoutExtension);
    strcat(filePath, extension);

    g_fileBrowserOnFileSelected(filePath);
}

void newFile() {
    if (isSaveDialog()) {
        popPage();
        psu::gui::Keypad::startPush(0, 0, 1, MAX_PATH_LENGTH, false, onNewFileOk, popPage);
    }
}

bool isStorageAlarm() {
    uint64_t usedSpace;
    uint64_t freeSpace;
    if (psu::sd_card::getInfo(usedSpace, freeSpace, true)) { // "true" means get storage info from cache
        auto totalSpace = usedSpace + freeSpace;
        auto percent = (int)floor(100.0 * freeSpace / totalSpace);
        if (percent >= 10) {
            return false;
        }
    }
    return true;
}

void getStorageInfo(Value& value) {
    if (!g_fileBrowserMode && g_state == STATE_READY) {
        if (isStorageAlarm() || isRootDirectory()) {
            value = Value(psu::sd_card::getInfoVersion(), VALUE_TYPE_STORAGE_INFO);
        } else {
            value = Value(g_filesCount, VALUE_TYPE_FOLDER_INFO);
        }
    }
}

} // namespace file_manager

using namespace file_manager;

void data_file_manager_current_directory(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        value = getCurrentDirectory();
    }
}

void data_file_manager_state(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        value = getState();
    }
}

void data_file_manager_is_root_directory(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        value = isRootDirectory() ? 1 : 0;
    }
}

void data_file_manager_layout(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        value = getListViewLayout();
    }
}

void data_file_manager_files(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_COUNT) {
        value = (int)MAX(getFilesCount(), getFilesPageSize());
    } else if (operation == data::DATA_OPERATION_YT_DATA_GET_SIZE) {
        value = Value(getFilesCount(), VALUE_TYPE_UINT32);
    } else if (operation == data::DATA_OPERATION_YT_DATA_GET_POSITION) {
        value = Value(getFilesStartPosition(), VALUE_TYPE_UINT32);
    } else if (operation == data::DATA_OPERATION_YT_DATA_SET_POSITION) {
        setFilesStartPosition(value.getUInt32());
    } else if (operation == data::DATA_OPERATION_YT_DATA_GET_POSITION_INCREMENT) {
        value = Value(getFilesPositionIncrement(), VALUE_TYPE_UINT32);
    } else if (operation == data::DATA_OPERATION_YT_DATA_GET_PAGE_SIZE) {
        value = Value(getFilesPageSize(), VALUE_TYPE_UINT32);
    }
}

void data_file_manager_is_directory(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        value = isDirectory(cursor.i) ? 1 : 0;
    }
}

void data_file_manager_file_type(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        value = getFileType(cursor.i);
    }
}

void data_file_manager_file_icon(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        value = getFileIcon(cursor.i);
    }
}

void data_file_manager_file_name(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        auto fileType = getFileType(cursor.i);
        if (fileType != FILE_TYPE_NONE) {
            value = Value(getFileName(cursor.i), VALUE_TYPE_STR, STRING_OPTIONS_FILE_ELLIPSIS);
        }
    }
}

void data_file_manager_file_description(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        auto fileType = getFileType(cursor.i);
        if (fileType != FILE_TYPE_NONE) {
            value = getFileDescription(cursor.i);
        }
    }
}

void data_file_manager_file_size(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        auto fileType = getFileType(cursor.i);
        if (fileType == FILE_TYPE_DIRECTORY) {
            value = "<dir>";
        } else if (fileType != FILE_TYPE_NONE) {
            value = Value(getFileSize(cursor.i), VALUE_TYPE_FILE_LENGTH);
        }
    }
}

void data_file_manager_file_date_time(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        auto fileType = getFileType(cursor.i);
        if (fileType != FILE_TYPE_NONE) {
            value = Value(getFileDataTime(cursor.i), VALUE_TYPE_FILE_DATE_TIME);
        }
    }
}

void data_file_manager_file_selected(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        value = isFileSelected(cursor.i);
    }
}


void data_file_manager_open_file_enabled(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        value = isOpenFileEnabled();
    }
}

void data_file_manager_upload_file_enabled(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        value = isUploadFileEnabled();
    }
}

void data_file_manager_rename_file_enabled(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        value = isRenameFileEnabled();
    }
}

void data_file_manager_delete_file_enabled(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        value = isDeleteFileEnabled();
    }
}

void data_file_manager_opened_image(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET_BITMAP_IMAGE) {
        if (g_openedImage.pixels) {
            value = Value(&g_openedImage, VALUE_TYPE_POINTER);
        }
    }
}

void data_file_manager_browser_title(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        value = g_fileBrowserTitle;
    }
}

void data_file_manager_browser_is_save_dialog(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        value = isSaveDialog();
    }
}

void data_file_manager_storage_alarm(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        value = isStorageAlarm();
    }
}

void data_file_manager_storage_info(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        getStorageInfo(value);
    }
}

void data_file_manager_is_list_view_option_available(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        value = isListViewOptionAvailable();
    }
}

void data_file_manager_list_view(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        value = getListViewOption();
    }
}

void data_file_manager_sort_files_option(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        value = getSortFilesOption();
    }
}


void action_file_manager_go_to_parent_directory() {
    goToParentDirectory();
}

void action_file_manager_select_file() {
    selectFile(getFoundWidgetAtDown().cursor.i);
}

void action_file_manager_open_file() {
    openFile();
}

void action_file_manager_upload_file() {
    uploadFile();
}

void action_file_manager_rename_file() {
    renameFile();
}

void action_file_manager_delete_file() {
    deleteFile();
}

void action_file_manager_select_list_view() {
    toggleListViewOption();
}

void onSetFileManagerSortBy(uint16_t value) {
    popPage();
    setSortFilesOption((SortFilesOption)value);
}

void action_file_manager_sort_by() {
    pushSelectFromEnumPage(g_fileManagerSortByEnumDefinition, getSortFilesOption(), NULL, onSetFileManagerSortBy, true);
}

void action_file_manager_new_file() {
    newFile();
}

} // namespace gui

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

    // invalidate storage info cache
    uint64_t usedSpace;
    uint64_t freeSpace;
    psu::sd_card::getInfo(usedSpace, freeSpace, false); // "false" means **do not** get storage info from cache
}

} // namespace eez
