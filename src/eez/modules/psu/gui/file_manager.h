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

#pragma once

#include <stdint.h>

#include <eez/file_type.h>
#include <eez/modules/psu/persist_conf.h>
#include <eez/gui/page.h>

namespace eez {
namespace gui {
namespace file_manager {

enum State {
    STATE_STARTING,
    STATE_LOADING,
    STATE_READY
};

void loadDirectory();
SortFilesOption getSortFilesOption();
void setSortFilesOption(SortFilesOption sortFilesOption);
const char *getCurrentDirectory();
State getState();
bool isRootDirectory();
void goToParentDirectory();
uint32_t getFilesCount();
uint32_t getFilesStartPosition();
void setFilesStartPosition(uint32_t position);
uint32_t getFilesPageSize();
bool isDirectory(uint32_t fileIndex);
FileType getFileType(uint32_t fileIndex);
const char *getFileName(uint32_t fileIndex);
const uint32_t getFileSize(uint32_t fileIndex);
const uint32_t getFileDataTime(uint32_t fileIndex);
bool isFileSelected(uint32_t fileIndex);
void selectFile(uint32_t fileIndex);
bool isOpenFileEnabled();
void openFile();
bool isUploadFileEnabled();
void uploadFile();
bool isRenameFileEnabled();
void renameFile();
bool isDeleteFileEnabled();
void deleteFile();

void openImageFile();
uint8_t *getOpenedImagePixels();

void onEncoder(int couter);

void openFileManager();

class FileBrowserPage : public SetPage {
public:
    int getDirty();
    bool showAreYouSureOnDiscard();
    void set();
};

void browseForFile(const char *title, const char *directory, FileType fileType, void (*onFileSelected)(const char *filePath));

extern const char *g_fileBrowserTitle;

} // namespace file_manager
} // namespace gui
} // namespace eez
