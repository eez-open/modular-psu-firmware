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

#include <bb3/psu/psu.h>

#include <string.h>
#include <stdlib.h>

#include <bb3/system.h>
#include <bb3/scripting/scripting.h>
#include <eez/core/memory.h>
#include <bb3/fs_driver.h>

#include <eez/gui/gui.h>
#include <eez/gui/widgets/containers/container.h>

#include <bb3/psu/psu.h>
#include <bb3/psu/serial_psu.h>
#if OPTION_ETHERNET
#include <bb3/psu/ethernet_scpi.h>
#endif
#include <bb3/psu/sd_card.h>
#include <bb3/psu/persist_conf.h>
#include <bb3/psu/datetime.h>
#include <bb3/psu/event_queue.h>
#include <bb3/psu/persist_conf.h>
#include <bb3/psu/dlog_view.h>

#include <bb3/psu/scpi/psu.h>

#include <bb3/psu/gui/psu.h>
#include <bb3/psu/gui/animations.h>
#include <bb3/psu/gui/file_manager.h>
#include <bb3/psu/gui/keypad.h>

#include <eez/fs/fs.h>

#include <eez/gui/image.h>
#include <bb3/fpga/prog.h>

using namespace eez::psu::gui;

namespace eez {
namespace gui {
namespace file_manager {

static const size_t MAX_FILE_DESCRIPTION_LENGTH = 80;

static State g_state;
static uint32_t g_loadingStartTickCount;

static int g_currentDiskDrive;
static char g_currentDirectory[MAX_PATH_LENGTH + 1];

static uint32_t g_currentDiskDriveTitleVersion;

static bool g_showDiskDrives;

struct FileItem {
    FileType type;
    const char *name;
    uint32_t size;
    uint32_t dateTime; // if type is FILE_TYPE_DISK_DRIVE this field containes disk drive index
    const char *description;
};

static uint8_t *g_frontBufferPosition;
static uint8_t *g_backBufferPosition;

uint32_t g_filesCount;
uint32_t g_filesStartPosition;
uint32_t g_savedFilesStartPosition;

int32_t g_selectedFileIndex = -1;

uint32_t g_imageLoadStartTime;
ImageDecodeResult g_imageLoadResult;
Image g_openedImage;

bool g_fileBrowserMode;
DialogType g_dialogType;
const char *g_fileBrowserTitle;
FileType g_fileBrowserFileType;
bool (*g_fileBrowserFileTypeFilter)(FileType type);
static bool (*g_fileBrowserNameFilter)(const char *fileName);
static void (*g_fileBrowserOnFileSelected)(const char *filePath);

static ListViewOption g_rootDirectoryListViewOption = LIST_VIEW_LARGE_ICONS;
static ListViewOption g_scriptsDirectoryListViewOption = LIST_VIEW_SCRIPTS;

bool makeAbsolutePath(const char *relativePath, char *dest) {
    if (2 + strlen(g_currentDirectory) + (relativePath ? 1 + strlen(relativePath) : 0) > MAX_PATH_LENGTH) {
        return false;
    }

	if (fs_driver::getDiskDrivesNum() > 1) {
		dest[0] = '0' + g_currentDiskDrive;
		dest[1] = ':';
		stringCopy(dest + 2, MAX_PATH_LENGTH - 2, g_currentDirectory);
	} else {
		stringCopy(dest, MAX_PATH_LENGTH, g_currentDirectory);
	}

    if (relativePath) {
    	stringAppendString(dest, MAX_PATH_LENGTH, "/");
    	stringAppendString(dest, MAX_PATH_LENGTH, relativePath);
    }
    return true;
}

void catalogCallback(void *param, const char *name, FileType type, size_t size, bool isHiddenOrSystemFile) {
    if (isHiddenOrSystemFile) {
        return;
    }
    
    if (name[0] == '.') {
        return;
    }

    if (g_fileBrowserMode) {
        if (type != FILE_TYPE_DIRECTORY) {
            if (!(g_fileBrowserFileTypeFilter ? g_fileBrowserFileTypeFilter(type) : type == g_fileBrowserFileType)) {
                return;
            }
        }
    }

    if (g_fileBrowserMode && g_fileBrowserNameFilter) {
        if (!g_fileBrowserNameFilter(name)) {
            return;
        }
    }

    auto fileInfo = (FileInfo *)param;
 
    char fileNameWithoutExtension[MAX_PATH_LENGTH + 1];

    char description[MAX_FILE_DESCRIPTION_LENGTH + 1];
    size_t descriptionLen = 0;
    size_t descriptionLenWithPadding = 0;

    if (isScriptsDirectory() && (getListViewOption() == LIST_VIEW_SCRIPTS || getListViewOption() == LIST_VIEW_LARGE_ICONS)) {
        if (type != FILE_TYPE_MICROPYTHON && type != FILE_TYPE_APP) {
            return;
        }

        description[0] = 0;

        if (getListViewOption() == LIST_VIEW_SCRIPTS) {
            char filePath[MAX_PATH_LENGTH + 1];
            if (makeAbsolutePath(name, filePath)) {
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
            }
        }

        descriptionLen = strlen(description);
        descriptionLenWithPadding = descriptionLen;
        if (descriptionLenWithPadding > 0) {
            descriptionLenWithPadding = 4 * ((descriptionLenWithPadding + 1 + 3) / 4);
        }

        const char *str = strrchr(name, '.');
        if (str) {
            auto n = str - name;
            memcpy(fileNameWithoutExtension, name, n);
            fileNameWithoutExtension[n] = 0;
            name = fileNameWithoutExtension;
        }
    }

    size_t nameLen = strlen(name);
    size_t nameLenWithPadding = 4 * ((nameLen + 1 + 3) / 4);

    if (g_frontBufferPosition + sizeof(FileItem) > g_backBufferPosition - nameLenWithPadding - descriptionLenWithPadding) {
        return;
    }

    auto fileItem = (FileItem *)g_frontBufferPosition;
    g_frontBufferPosition += sizeof(FileItem);

    fileItem->type = type;

    g_backBufferPosition -= nameLenWithPadding;
    stringCopy((char *)g_backBufferPosition, nameLen + 1, name);
    fileItem->name = (const char *)g_backBufferPosition;

    if (descriptionLenWithPadding > 0) {
        g_backBufferPosition -= descriptionLenWithPadding;
        stringCopy((char *)g_backBufferPosition, descriptionLen + 1, description);
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

    if (isScriptsDirectory() && (getListViewOption() == LIST_VIEW_SCRIPTS || getListViewOption() == LIST_VIEW_LARGE_ICONS)) {
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

    g_currentDiskDriveTitleVersion++;

    if (!isLowPriorityThread()) {
        using namespace scpi;
        sendMessageToLowPriorityThread(THREAD_MESSAGE_FILE_MANAGER_LOAD_DIRECTORY);
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

    if (g_showDiskDrives) {
        int diskDrivesNum = fs_driver::getDiskDrivesNum();

        for (int iterationIndex = 0; iterationIndex < diskDrivesNum; iterationIndex++) {
			int diskDriveIndex = fs_driver::getDiskDriveIndex(iterationIndex);
			Value value = Value(diskDriveIndex, VALUE_TYPE_MASS_STORAGE_DEVICE_LABEL);

            char name[MAX_PATH_LENGTH + 1];
            value.toText(name, sizeof(name));

            size_t nameLen = strlen(name);
            size_t nameLenWithPadding = 4 * ((nameLen + 1 + 3) / 4);

            if (g_frontBufferPosition + sizeof(FileItem) > g_backBufferPosition - nameLenWithPadding) {
                break;
            }

            auto fileItem = (FileItem *)g_frontBufferPosition;
            g_frontBufferPosition += sizeof(FileItem);

            fileItem->type = FILE_TYPE_DISK_DRIVE;

            g_backBufferPosition -= nameLenWithPadding;
            stringCopy((char *)g_backBufferPosition, nameLen + 1, name);
            fileItem->name = (const char *)g_backBufferPosition;

            fileItem->description = nullptr;
            fileItem->size = 0;
            fileItem->dateTime = diskDriveIndex;

            g_filesCount++;
        }

        setFilesStartPosition(0);
        g_state = STATE_READY;
    } else {
        static char g_loadDirectoryPath[MAX_PATH_LENGTH + 1];
        if (makeAbsolutePath(nullptr, g_loadDirectoryPath) && psu::sd_card::catalog(g_loadDirectoryPath, 0, catalogCallback, &numFiles, &err)) {
            sort();
            setFilesStartPosition(g_savedFilesStartPosition);
            g_state = STATE_READY;
        } else {
            g_state = STATE_NOT_PRESENT;
        }
    }
}

void onSdCardMountedChange() {
    if (g_showDiskDrives) {
        g_state = STATE_STARTING;
    } else {
        if (g_currentDiskDrive == 0) {
            if (psu::sd_card::isMounted(nullptr, nullptr)) {
                g_state = STATE_STARTING;
            } else {
                g_state = STATE_NOT_PRESENT;
            }
        } else {
            if (fs_driver::isDriverLinked(g_currentDiskDrive - 1)) {
                g_state = STATE_STARTING;
            } else {
                g_state = STATE_NOT_PRESENT;
            }
        }
    }
}

ListViewOption getListViewOption() {
    if (g_showDiskDrives) {
        return LIST_VIEW_LARGE_ICONS;
    }
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
        return PAGE_ID_FILE_MANAGER_SCRIPTS_ALTER_VIEW;
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
        // if (g_scriptsDirectoryListViewOption == LIST_VIEW_DETAILS) {
        //     g_scriptsDirectoryListViewOption = LIST_VIEW_SCRIPTS;
        // } else if (g_scriptsDirectoryListViewOption == LIST_VIEW_SCRIPTS) {
        //     g_scriptsDirectoryListViewOption = LIST_VIEW_LARGE_ICONS;
        // } else {
        //     g_scriptsDirectoryListViewOption = LIST_VIEW_DETAILS;
        // }

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

void getCurrentDirectoryTitle(char *text, int count) {
    if (g_showDiskDrives) {
        stringCopy(text, count, "Mass Storage Devices");
    } else {
        const char *str = *g_currentDirectory == 0 ? "/<Root directory>" : g_currentDirectory;

    	if (psu::sd_card::isMounted(nullptr, nullptr) && fs_driver::getDiskDrivesNum() == 1 && g_currentDiskDrive == 0) {
            stringCopy(text, count, str);
        } else {
        	text[0] = '0' + g_currentDiskDrive;
        	text[1] = ':';
    		stringCopy(text + 2, count - 2, str);
        }
    }
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

static bool hasParent() {
    if (g_showDiskDrives) {
        return false;
    }

    if (!isRootDirectory()) {
        return true;
    }

    return fs_driver::getDiskDrivesNum() > 1 || !psu::sd_card::isMounted(nullptr, nullptr) || g_currentDiskDrive != 0;
}

bool isRootDirectory() {
    return g_currentDirectory[0] == 0 || strcmp(g_currentDirectory, "/") == 0;
}

bool isScriptsDirectory() {
    return strcmp(g_currentDirectory, SCRIPTS_DIR) == 0;
}

static void animateLoadDirectory() {
    static const Rect g_rectManager = { 0, 0, 480, 272 };
    static const Rect g_rectBrowser = { 10, 17, 460, 238 };

    animateFadeOutFadeIn(g_fileBrowserMode ? g_rectBrowser : g_rectManager);
}

static void goToParent() {
    if (g_state != STATE_READY && g_state != STATE_NOT_PRESENT) {
        return;
    }

    if (isRootDirectory()) {
        g_showDiskDrives = true;
    } else {
        char *p = g_currentDirectory + strlen(g_currentDirectory);

        while (p != g_currentDirectory && *p != '/') {
            p--;
        }

        *p = 0;
    }

    g_filesStartPosition = 0;
    loadDirectory();
    animateLoadDirectory();
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

static const int NUM_COLUMNS_IN_SCRIPTS_ALTER_VIEW = 2;
static const int NUM_ROWS_IN_SCRIPTS_ALTER_VIEW = 3;

uint32_t getFilesPositionIncrement() {
    if (getListViewOption() == LIST_VIEW_LARGE_ICONS) {
        return NUM_COLUMNS_IN_LARGE_ICONS_VIEW;
    }

    if (getListViewOption() == LIST_VIEW_SCRIPTS) {
        if (getListViewLayout() == PAGE_ID_FILE_MANAGER_SCRIPTS_ALTER_VIEW) {
            return NUM_COLUMNS_IN_SCRIPTS_ALTER_VIEW;
        } else {
            return 1;
        }
    }

    return 1;
}

uint32_t getFilesPageSize() {
    static const uint32_t FILE_BROWSER_FILES_PAGE_SIZE = 5;
    static const uint32_t FILE_MANAGER_FILES_PAGE_SIZE_IN_DETAILS_VIEW = 6;
    static const uint32_t FILE_MANAGER_FILES_PAGE_SIZE_IN_LARGE_ICONS_VIEW = NUM_COLUMNS_IN_LARGE_ICONS_VIEW * NUM_ROWS_IN_LARGE_ICONS_VIEW;
    static const uint32_t FILE_MANAGER_FILES_PAGE_SIZE_IN_SCRIPTS_VIEW = 3;
    static const uint32_t FILE_MANAGER_FILES_PAGE_SIZE_IN_SCRIPTS_ALTER_VIEW = NUM_COLUMNS_IN_SCRIPTS_ALTER_VIEW * NUM_ROWS_IN_SCRIPTS_ALTER_VIEW;

    if (g_fileBrowserMode) {
        return FILE_BROWSER_FILES_PAGE_SIZE;
    }

    if (getListViewOption() == LIST_VIEW_LARGE_ICONS) {
        return FILE_MANAGER_FILES_PAGE_SIZE_IN_LARGE_ICONS_VIEW;
    }
    
    if (getListViewOption() == LIST_VIEW_SCRIPTS) {
        if (getListViewLayout() == PAGE_ID_FILE_MANAGER_SCRIPTS_ALTER_VIEW) {
            return FILE_MANAGER_FILES_PAGE_SIZE_IN_SCRIPTS_ALTER_VIEW;
        } else {
            return FILE_MANAGER_FILES_PAGE_SIZE_IN_SCRIPTS_VIEW;
        }
    }

    return FILE_MANAGER_FILES_PAGE_SIZE_IN_DETAILS_VIEW;
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
    if (fileType == FILE_TYPE_NONE) {
        return nullptr;
    }

    if (getListViewOption() == LIST_VIEW_DETAILS) {
        return getFileTypeSmallIcon(fileType);
    }

    if (getListViewOption() == LIST_VIEW_SCRIPTS) {
        return fileType == FILE_TYPE_APP ? "\x95" : "\x94";
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
        if (fileItem && fileItem->type == FILE_TYPE_DISK_DRIVE) {
            g_showDiskDrives = false;
            g_currentDiskDrive = fileItem->dateTime;
            *g_currentDirectory = 0;
            g_filesStartPosition = 0;
            loadDirectory();
            animateLoadDirectory();
        } else if (fileItem && fileItem->type == FILE_TYPE_DIRECTORY) {
            if (2 + strlen(g_currentDirectory) + 1 + strlen(fileItem->name) <= MAX_PATH_LENGTH) {
                stringAppendString(g_currentDirectory, MAX_PATH_LENGTH, "/");
                stringAppendString(g_currentDirectory, MAX_PATH_LENGTH, fileItem->name);
                g_filesStartPosition = 0;
                loadDirectory();
                animateLoadDirectory();
            }
        } else {
            g_selectedFileIndex = fileIndex;
            if (!g_fileBrowserMode) {
                if (isScriptsDirectory() && (getListViewOption() == LIST_VIEW_SCRIPTS || getListViewOption() == LIST_VIEW_LARGE_ICONS)) {
                    if (scripting::isIdle()) {
                        if (strlen(fileItem->name) + 3 <= MAX_PATH_LENGTH) {
                            char fileName[MAX_PATH_LENGTH + 1];
                            stringCopy(fileName, sizeof(fileName), fileItem->name);
							if (fileItem->type == FILE_TYPE_MICROPYTHON) {
								stringAppendString(fileName, MAX_PATH_LENGTH, ".py");
							} else {
								stringAppendString(fileName, MAX_PATH_LENGTH, ".app");
							}
                            char filePath[MAX_PATH_LENGTH + 1];
                            if (makeAbsolutePath(fileName, filePath)) {
                                scripting::startScript(filePath);
                            } else {
								g_psuAppContext.errorMessage(Value(SCPI_ERROR_FILE_NAME_ERROR, VALUE_TYPE_SCPI_ERROR));
                            }
                        } else {
							g_psuAppContext.errorMessage(Value(SCPI_ERROR_FILE_NAME_ERROR, VALUE_TYPE_SCPI_ERROR));
                        }
                    } else {
						g_psuAppContext.infoMessage("Script is already running!");
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
        return scripting::isIdle();
    }

    if (fileItem->type == FILE_TYPE_OTHER) {
        if (endsWithNoCase(fileItem->name, ".bit")) {
            return true;
        }
    }

    return false;
}

void openFile() {
    popPage();

    auto fileItem = getFileItem(g_selectedFileIndex);
    if (!fileItem) {
        return;
    }

    char filePath[MAX_PATH_LENGTH + 1];
    if (!makeAbsolutePath(fileItem->name, filePath)) {
		g_psuAppContext.errorMessage(Value(SCPI_ERROR_FILE_NAME_ERROR, VALUE_TYPE_SCPI_ERROR));
        return;
    }

    if (fileItem->type == FILE_TYPE_DLOG) {
        psu::dlog_view::g_showLatest = false;
        psu::dlog_view::openFile(filePath);
        pushPage(gui::PAGE_ID_DLOG_VIEW);
    } else if (fileItem->type == FILE_TYPE_IMAGE) {
        char filePath[MAX_PATH_LENGTH + 1];
		makeAbsolutePath(fileItem->name, filePath);
        openImageFile(filePath, gui::PAGE_ID_IMAGE_VIEW);
    } else if (fileItem->type == FILE_TYPE_MICROPYTHON) {
        scripting::startScript(filePath);
    } else if (fileItem->type == FILE_TYPE_OTHER) {
        if (endsWithNoCase(filePath, ".bit")) {
            sendMessageToLowPriorityThread(THREAD_MESSAGE_FILE_MANAGER_OPEN_BIT_FILE);
        }
    }
}

static char g_imageFilePath[MAX_PATH_LENGTH + 1];

void openImageFile(const char *filePath, int pageId) {
    stringCopy(g_imageFilePath, MAX_PATH_LENGTH + 1, filePath);
    g_imageLoadStartTime = millis();
    g_imageLoadResult = IMAGE_DECODE_OK;
    g_openedImage.pixels = nullptr;
    pushPage(pageId);
    sendMessageToLowPriorityThread(THREAD_MESSAGE_FILE_MANAGER_OPEN_IMAGE_FILE);
}

void openImageFile() {
    g_imageLoadResult = imageDecode(g_imageFilePath, &g_openedImage);
}

void openBitFile() {
    auto fileItem = getFileItem(g_selectedFileIndex);
    if (fileItem) {
        char filePath[MAX_PATH_LENGTH + 1];
        if (makeAbsolutePath(fileItem->name, filePath)) {
            fpga::prog(filePath);
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
    if (!isLowPriorityThread()) {
        popPage();
        using namespace scpi;
        sendMessageToLowPriorityThread(THREAD_MESSAGE_FILE_MANAGER_UPLOAD_FILE);
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
    if (!makeAbsolutePath(fileItem->name, filePath)) {
		g_psuAppContext.errorMessage(Value(SCPI_ERROR_FILE_NAME_ERROR, VALUE_TYPE_SCPI_ERROR));
        return;
    }

    int err;
    if (!psu::scpi::mmemUpload(filePath, context, &err)) {
		g_psuAppContext.errorMessage(Value(err, VALUE_TYPE_SCPI_ERROR));
    }
}

bool isRenameFileEnabled() {
    return true;
}

static char g_fileNameWithoutExtension[MAX_PATH_LENGTH + 1];

void onRenameFileOk(char *fileNameWithoutExtension) {
    stringCopy(g_fileNameWithoutExtension, sizeof(g_fileNameWithoutExtension), fileNameWithoutExtension);

    if (!isLowPriorityThread()) {
        popPage();
        using namespace scpi;
        sendMessageToLowPriorityThread(THREAD_MESSAGE_FILE_MANAGER_RENAME_FILE);
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
    char srcFilePath[MAX_PATH_LENGTH + 1];
    if (!makeAbsolutePath(fileItem->name, srcFilePath)) {
		g_psuAppContext.errorMessage(Value(SCPI_ERROR_FILE_NAME_ERROR, VALUE_TYPE_SCPI_ERROR));
        return;
    }

    // destination file path
    const char *extension = strrchr(fileItem->name, '.');
    
    char dstFileName[MAX_PATH_LENGTH + 1];
    
    if (strlen(g_fileNameWithoutExtension) + (extension ? strlen(extension) : 0) > MAX_PATH_LENGTH) {
		g_psuAppContext.errorMessage(Value(SCPI_ERROR_FILE_NAME_ERROR, VALUE_TYPE_SCPI_ERROR));
        return;
    }
    
    stringCopy(dstFileName, sizeof(dstFileName), g_fileNameWithoutExtension);
    if (extension) {
        stringAppendString(dstFileName, MAX_PATH_LENGTH, extension);
    }
    
    char dstFilePath[MAX_PATH_LENGTH + 1];
    if (!makeAbsolutePath(dstFileName, dstFilePath)) {
		g_psuAppContext.errorMessage(Value(SCPI_ERROR_FILE_NAME_ERROR, VALUE_TYPE_SCPI_ERROR));
        return;
    }

    int err;
    if (!psu::sd_card::moveFile(srcFilePath, dstFilePath, &err)) {
		g_psuAppContext.errorMessage(Value(err, VALUE_TYPE_SCPI_ERROR));
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
        memcpy(fileNameWithoutExtension, fileItem->name, n);
        fileNameWithoutExtension[n] = 0;
    } else {
        stringCopy(fileNameWithoutExtension, MAX_PATH_LENGTH, fileItem->name);
    }

    startTextKeypad(0, fileNameWithoutExtension, 1, MAX_PATH_LENGTH, false, onRenameFileOk, popPage);
}

bool isDeleteFileEnabled() {
    return true;
}

void deleteFile() {
    if (!isLowPriorityThread()) {
        popPage();
        using namespace scpi;
        sendMessageToLowPriorityThread(THREAD_MESSAGE_FILE_MANAGER_DELETE_FILE);
        return;
    }

    auto fileItem = getFileItem(g_selectedFileIndex);
    if (!fileItem) {
        return;
    }

    char filePath[MAX_PATH_LENGTH + 1];
    if (!makeAbsolutePath(fileItem->name, filePath)) {
		g_psuAppContext.errorMessage(Value(SCPI_ERROR_FILE_NAME_ERROR, VALUE_TYPE_SCPI_ERROR));
        return;
    }

    int err;
    if (!psu::sd_card::deleteFile(filePath, &err)) {
		g_psuAppContext.errorMessage(Value(err, VALUE_TYPE_SCPI_ERROR));
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

    char filePath[MAX_PATH_LENGTH + 1];
    if (!makeAbsolutePath(fileItem->name, filePath)) {
		g_psuAppContext.errorMessage(Value(SCPI_ERROR_FILE_NAME_ERROR, VALUE_TYPE_SCPI_ERROR));
        return;
    }

    g_fileBrowserOnFileSelected(filePath);
}

void browseForFile(const char *title, const char *directory, FileType fileType, DialogType dialogType, void(*onFileSelected)(const char *filePath), bool (*nameFilter)(const char *), bool (*fileTypeFilter)(FileType)) {
    g_fileBrowserMode = true;
    g_dialogType = dialogType;
    g_fileBrowserTitle = title;
    g_fileBrowserFileType = fileType;
    g_fileBrowserFileTypeFilter = fileTypeFilter;
    g_fileBrowserNameFilter = nameFilter;
    g_fileBrowserOnFileSelected = onFileSelected;

	g_showDiskDrives = false;
	g_currentDiskDrive = 0;
    stringCopy(g_currentDirectory, sizeof(g_currentDirectory), directory);
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

    char fileName[MAX_PATH_LENGTH + 1];
    if (strlen(fileNameWithoutExtension) + strlen(extension) > MAX_PATH_LENGTH) {
		g_psuAppContext.errorMessage(Value(SCPI_ERROR_FILE_NAME_ERROR, VALUE_TYPE_SCPI_ERROR));
        return;
    }

    stringCopy(fileName, sizeof(fileName), fileNameWithoutExtension);
    stringAppendString(fileName, MAX_PATH_LENGTH, extension);

    char filePath[MAX_PATH_LENGTH + 1];
    if (!makeAbsolutePath(fileName, filePath)) {
		g_psuAppContext.errorMessage(Value(SCPI_ERROR_FILE_NAME_ERROR, VALUE_TYPE_SCPI_ERROR));
        return;
    }

    g_fileBrowserOnFileSelected(filePath);
}

void newFile() {
    if (isSaveDialog()) {
        popPage();
        startTextKeypad(0, 0, 1, MAX_PATH_LENGTH, false, onNewFileOk, popPage);
    }
}

bool isStorageAlarm() {
    uint64_t usedSpace;
    uint64_t freeSpace;
    if (psu::sd_card::getInfo(0, usedSpace, freeSpace, true)) { // "true" means get storage info from cache
        auto totalSpace = usedSpace + freeSpace;
        auto percent = (int)floor(100.0 * freeSpace / totalSpace);
        if (percent >= 10) {
            return false;
        }
    }
    return true;
}

void getStorageInfo(Value& value) {
    if (!g_fileBrowserMode && g_state == STATE_READY && !g_showDiskDrives) {
        if (isStorageAlarm() || isRootDirectory()) {
            value = Value(((uint32_t)psu::sd_card::getInfoVersion(g_currentDiskDrive) << 16) | g_currentDiskDrive, VALUE_TYPE_STORAGE_INFO);
        } else {
            value = Value(g_filesCount, VALUE_TYPE_FOLDER_INFO);
        }
    }
}

} // namespace file_manager

using namespace file_manager;

void data_file_manager_current_directory(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = Value(g_currentDiskDriveTitleVersion, VALUE_TYPE_CURRENT_DIRECTORY_TITLE);
    }
}

void data_file_manager_state(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = getState();
    }
}

void data_file_manager_has_parent(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = hasParent() ? 1 : 0;
    }
}

void data_file_manager_layout(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = getListViewLayout();
    }
}

void data_file_manager_files(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_COUNT) {
        value = (int)MAX(getFilesCount(), getFilesPageSize());
    } else if (operation == DATA_OPERATION_YT_DATA_GET_SIZE) {
        value = Value(getFilesCount(), VALUE_TYPE_UINT32);
    } else if (operation == DATA_OPERATION_YT_DATA_GET_POSITION) {
        value = Value(getFilesStartPosition(), VALUE_TYPE_UINT32);
    } else if (operation == DATA_OPERATION_YT_DATA_SET_POSITION) {
        setFilesStartPosition(value.getUInt32());
    } else if (operation == DATA_OPERATION_YT_DATA_GET_POSITION_INCREMENT) {
        value = Value(getFilesPositionIncrement(), VALUE_TYPE_UINT32);
    } else if (operation == DATA_OPERATION_YT_DATA_GET_PAGE_SIZE) {
        value = Value(getFilesPageSize(), VALUE_TYPE_UINT32);
    }
}

void data_file_manager_file_type(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    auto cursor = widgetCursor.cursor;
    if (operation == DATA_OPERATION_GET) {
        value = getFileType(cursor);
    }
}

void data_file_manager_file_icon(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    auto cursor = widgetCursor.cursor;
    if (operation == DATA_OPERATION_GET) {
        value = getFileIcon(cursor);
    }
}

void data_file_manager_file_name(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    auto cursor = widgetCursor.cursor;
    if (operation == DATA_OPERATION_GET) {
        auto fileType = getFileType(cursor);
        if (fileType != FILE_TYPE_NONE) {
            value = Value(getFileName(cursor), VALUE_TYPE_STRING, STRING_OPTIONS_FILE_ELLIPSIS);
        }
    }
}

void data_file_manager_file_has_description(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    auto cursor = widgetCursor.cursor;
    if (operation == DATA_OPERATION_GET) {
        auto fileType = getFileType(cursor);
        if (fileType != FILE_TYPE_NONE) {
            value = strlen(getFileDescription(cursor)) > 0;
        }
    }
}

void data_file_manager_file_description(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    auto cursor = widgetCursor.cursor;
    if (operation == DATA_OPERATION_GET) {
        auto fileType = getFileType(cursor);
        if (fileType != FILE_TYPE_NONE) {
            value = getFileDescription(cursor);
        }
    }
}

void data_file_manager_file_size(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    auto cursor = widgetCursor.cursor;
    if (operation == DATA_OPERATION_GET) {
        auto fileType = getFileType(cursor);
        if (fileType == FILE_TYPE_DIRECTORY) {
            value = "<dir>";
        } else if (fileType != FILE_TYPE_NONE) {
            value = Value(getFileSize(cursor), VALUE_TYPE_FILE_LENGTH);
        }
    }
}

void data_file_manager_file_date_time(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    auto cursor = widgetCursor.cursor;
    if (operation == DATA_OPERATION_GET) {
        auto fileType = getFileType(cursor);
        if (fileType != FILE_TYPE_NONE) {
            value = Value(getFileDataTime(cursor), VALUE_TYPE_FILE_DATE_TIME);
        }
    }
}

void data_file_manager_file_selected(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    auto cursor = widgetCursor.cursor;
    if (operation == DATA_OPERATION_GET) {
        value = isFileSelected(cursor);
    }
}


void data_file_manager_open_file_enabled(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = isOpenFileEnabled();
    }
}

void data_file_manager_upload_file_enabled(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = isUploadFileEnabled();
    }
}

void data_file_manager_rename_file_enabled(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = isRenameFileEnabled();
    }
}

void data_file_manager_delete_file_enabled(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = isDeleteFileEnabled();
    }
}

void data_file_manager_opened_image(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET_BITMAP_IMAGE) {
        if (g_openedImage.pixels) {
            value = Value(&g_openedImage, VALUE_TYPE_POINTER);
        }
    }
}

void data_file_manager_browser_title(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = g_fileBrowserTitle;
    }
}

void data_file_manager_browser_is_save_dialog(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = isSaveDialog();
    }
}

void data_file_manager_storage_alarm(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = isStorageAlarm();
    }
}

void data_file_manager_storage_info(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        getStorageInfo(value);
    }
}

void data_file_manager_is_list_view_option_available(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = !g_showDiskDrives && (isRootDirectory() || isScriptsDirectory());
    }
}

void data_file_manager_is_sort_files_option_available(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = !g_showDiskDrives;
    }
}

void data_file_manager_list_view(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = getListViewOption();
    }
}

void data_file_manager_sort_files_option(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = getSortFilesOption();
    }
}

void data_file_manager_image_open_state(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        if (g_openedImage.pixels) {
            value = 1;
        } else if (g_imageLoadResult != IMAGE_DECODE_OK) {
            value = 1 + g_imageLoadResult;
        } else {
            value = 0;
        }
    }
}

void data_file_manager_image_open_progress(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        int progress = ((millis() / 40) * 40 - g_imageLoadStartTime) / 5;
        if (progress > 140) {
            progress = 95 + (progress - 140) / 100;
        } else if (progress > 90) {
            progress = 90 + (progress - 90) / 10;
        }
        
        if (progress > 100) {
            progress = 100;
        } else if (progress < 0) {
            progress = 0;
        }

        value = progress;
    }
}

void action_file_manager_go_to_parent() {
    goToParent();
}

void action_file_manager_select_file() {
    selectFile(getFoundWidgetAtDown().cursor);
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
	pushSelectFromEnumPage(ENUM_DEFINITION_FILE_MANAGER_SORT_BY, getSortFilesOption(), NULL, onSetFileManagerSortBy, true);
}

void action_file_manager_new_file() {
    newFile();
}

} // namespace gui

using namespace gui::file_manager;

void onSdCardFileChangeHook(const char *filePath1, const char *filePath2) {
    if (!isPageOnStack(PAGE_ID_FILE_MANAGER) && !isPageOnStack(PAGE_ID_FILE_BROWSER)) {
        return;
    }

	char parentDirPath1[MAX_PATH_LENGTH + 1];
    getParentDir(filePath1, parentDirPath1);

    int diskDrive = 0;
	const char *dirPath = parentDirPath1;
    
    if (parentDirPath1[0] >= '0' && parentDirPath1[0] <= '9' && parentDirPath1[1] == ':') {
        diskDrive = parentDirPath1[0] - '0';
		dirPath += 2;
	}

    if (diskDrive != g_currentDiskDrive) {
        return;
    }

    if (strcmp(dirPath, g_currentDirectory) == 0) {
        loadDirectory();
        return;
    }

    if (filePath2) {
        onSdCardFileChangeHook(filePath2);
    }
}

} // namespace eez
