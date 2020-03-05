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

#include <eez/file_type.h>
#include <eez/util.h>

#include <eez/modules/psu/psu.h>
#include <eez/modules/psu/profile.h>
#include <eez/modules/psu/list_program.h>

namespace eez {

static const char *fileTypeExtension[] = {
    nullptr,
    nullptr,
    LIST_EXT,
    PROFILE_EXT,
    ".dlog",
    ".jpg",
    ".py",
    ".hex",
    nullptr
};

static const char *scpiFileTypeNames[] = {
    nullptr,
    "FOLD",
    "LIST",
    "PROF",
    "DLOG",
    "IMG",
    "MP",
    "HEX",
    "BIN",
};

static const char *smallIcons[] = {
    nullptr,
    "\x46",
    "\x85",
    "\x30",
    "\x27",
    "\x5b",
    "\x7d",
    "\x6a",
    "\x6a",
};

static const char *largeIcons[] = {
    nullptr,
    "\x80",
    "\x81",
    "\x82",
    "\x83",
    "\x84",
    "\x85",
    "\x86",
    "\x86",
};

static const char *rootDirectoryIcons[] = {
    "\x80",
    "\x87",
    "\x88",
    "\x89",
    "\x8A",
    "\x8B",
    "\x8C",
    "\x8D"
};

FileType getFileTypeFromExtension(const char *filePath) {
    for (int fileType = 0; fileType < FILE_TYPE_OTHER; fileType++) {
        if (fileTypeExtension[fileType] && endsWithNoCase(filePath, fileTypeExtension[fileType])) {
            return (FileType)fileType;
        }    
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