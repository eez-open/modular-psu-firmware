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

#pragma once

namespace eez {

enum FileType {
    FILE_TYPE_NONE,
    FILE_TYPE_DIRECTORY,
    FILE_TYPE_LIST,
    FILE_TYPE_PROFILE,
    FILE_TYPE_DLOG,
    FILE_TYPE_IMAGE,
    FILE_TYPE_MICROPYTHON,
    FILE_TYPE_APP,
    FILE_TYPE_HEX,
    FILE_TYPE_OTHER,
    FILE_TYPE_DISK_DRIVE,
};

enum SortFilesOption {
    SORT_FILES_BY_NAME_ASC,
    SORT_FILES_BY_NAME_DESC,
    SORT_FILES_BY_SIZE_ASC,
    SORT_FILES_BY_SIZE_DESC,
    SORT_FILES_BY_TIME_ASC,
    SORT_FILES_BY_TIME_DESC
};

enum RootDirectoryType {
    ROOT_DIRECTORY_TYPE_NONE,
    ROOT_DIRECTORY_TYPE_SCRIPTS,
    ROOT_DIRECTORY_TYPE_SCREENSHOTS,
    ROOT_DIRECTORY_TYPE_RECORDINGS,
    ROOT_DIRECTORY_TYPE_LISTS,
    ROOT_DIRECTORY_TYPE_PROFILES,
    ROOT_DIRECTORY_TYPE_LOGS,
    ROOT_DIRECTORY_TYPE_UPDATES
};

FileType getFileTypeFromExtension(const char *filePath);
const char *getExtensionFromFileType(FileType fileType);
const char *getFileTypeScpiName(FileType fileType);

const char *getFileTypeSmallIcon(FileType fileType);
const char *getFileTypeLargeIcon(FileType fileType);
const char *getRootDirectoryIcon(RootDirectoryType fileType);

} // eez