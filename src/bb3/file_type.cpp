/*
* EEZ Generic Firmware
* Copyright (C) 2018-present, Envox d.o.o.
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

#include <bb3/file_type.h>
#include <eez/util.h>

#include <bb3/psu/psu.h>
#include <bb3/psu/profile.h>
#include <bb3/psu/list_program.h>

namespace eez {

static const char *fileTypeExtension[] = {
    nullptr, // FILE_TYPE_NONE
    nullptr, // FILE_TYPE_DIRECTORY
    LIST_EXT, // FILE_TYPE_LIST
    PROFILE_EXT, // FILE_TYPE_PROFILE
    ".dlog", // FILE_TYPE_DLOG
    ".jpg", // FILE_TYPE_IMAGE
    ".py", // FILE_TYPE_MICROPYTHON
    ".app", // FILE_TYPE_APP
    ".hex", // FILE_TYPE_HEX
    nullptr, // FILE_TYPE_OTHER
    nullptr // FILE_TYPE_DISK_DRIVE
};

static const char *scpiFileTypeNames[] = {
    nullptr, // FILE_TYPE_NONE 
    "FOLD", // FILE_TYPE_DIRECTORY
    "LIST", // FILE_TYPE_LIST
    "PROF", // FILE_TYPE_PROFILE
    "DLOG", // FILE_TYPE_DLOG
    "IMG", // FILE_TYPE_IMAGE
    "MP", // FILE_TYPE_MICROPYTHON
    "APP", // FILE_TYPE_APP
    "HEX", // FILE_TYPE_HEX
    "BIN", // FILE_TYPE_OTHER
    nullptr // FILE_TYPE_DISK_DRIVE
};

static const char *smallIcons[] = {
    nullptr, // FILE_TYPE_NONE
    "\x46", // FILE_TYPE_DIRECTORY
    "\x85", // FILE_TYPE_LIST
    "\x30", // FILE_TYPE_PROFILE
    "\x27", // FILE_TYPE_DLOG
    "\x5b", // FILE_TYPE_IMAGE
    "\x7d", // FILE_TYPE_MICROPYTHON
    "\x92", // FILE_TYPE_APP
    "\x6a", // FILE_TYPE_HEX
    "\x6a", // FILE_TYPE_OTHER
    "\x89", // FILE_TYPE_DISK_DRIVE
};

static const char *largeIcons[] = {
    nullptr, // FILE_TYPE_NONE
    "\x80", // FILE_TYPE_DIRECTORY
    "\x81", // FILE_TYPE_LIST
    "\x82", // FILE_TYPE_PROFILE
    "\x83", // FILE_TYPE_DLOG
    "\x84", // FILE_TYPE_IMAGE
    "\x85", // FILE_TYPE_MICROPYTHON
    "\x8F", // FILE_TYPE_APP
    "\x86", // FILE_TYPE_HEX
    "\x86", // FILE_TYPE_OTHER
    "\x8E", // FILE_TYPE_DISK_DRIVE
};

static const char *rootDirectoryIcons[] = {
    "\x80", // ROOT_DIRECTORY_TYPE_NONE
    "\x87", // ROOT_DIRECTORY_TYPE_SCRIPTS
    "\x88", // ROOT_DIRECTORY_TYPE_SCREENSHOTS
    "\x89", // ROOT_DIRECTORY_TYPE_RECORDINGS
    "\x8A", // ROOT_DIRECTORY_TYPE_LISTS
    "\x8B", // ROOT_DIRECTORY_TYPE_PROFILES
    "\x8C", // ROOT_DIRECTORY_TYPE_LOGS
    "\x8D" // ROOT_DIRECTORY_TYPE_UPDATES
};

FileType getFileTypeFromExtension(const char *filePath) {
    for (int fileType = 0; fileType < FILE_TYPE_OTHER; fileType++) {
        if (fileTypeExtension[fileType] && endsWithNoCase(filePath, fileTypeExtension[fileType])) {
            return (FileType)fileType;
        }    
    }

    if (endsWithNoCase(filePath, ".bmp")) {
        return FILE_TYPE_IMAGE;
    }

    return FILE_TYPE_OTHER;
}

const char *getExtensionFromFileType(FileType fileType) {
    return fileTypeExtension[fileType];
}

const char *getFileTypeScpiName(FileType fileType) {
    return scpiFileTypeNames[fileType];
}

const char *getFileTypeSmallIcon(FileType fileType) {
    return smallIcons[fileType];
}

const char *getFileTypeLargeIcon(FileType fileType) {
    return largeIcons[fileType];
}

const char *getRootDirectoryIcon(RootDirectoryType fileType) {
    return rootDirectoryIcons[fileType];
}


} // eez