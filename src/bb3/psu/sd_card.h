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

//#include <bb3/fs/fs.h>

#include <bb3/file_type.h>

namespace eez {

class File;

//extern SdFat SD;

namespace psu {
namespace sd_card {

extern TestResult g_testResult;
extern int g_lastError;

void init();
bool test();

void tick();

#if defined(EEZ_PLATFORM_STM32)
void onSdDetectInterrupt();
void onSdDetectInterruptHandler();
#endif

void reinitialize();

int getDiskDriveIndexFromPath(const char *filePath);

bool isMounted(const char *filePath, int *err);
bool isBusy();

bool makeParentDir(const char *filePath, int *err);

bool exists(const char *dirPath, int *err);
bool catalog(const char *dirPath, void *param, void (*callback)(void *param, const char *name, FileType type, size_t size, bool isHiddenOrSystemFile), int *numFiles, int *err);
bool catalogLength(const char *dirPath, size_t *length, int *err);
bool upload(const char *filePath, void *param, void (*callback)(void *param, const void *buffer, int size), int *err);
bool download(const char *filePath, bool truncate, const void *buffer, size_t size, int *err);
void downloadFinished();
bool moveFile(const char *sourcePath, const char *destinationPath, int *err);
bool copyFile(const char *sourcePath, const char *destinationPath, bool showProgress, int *err);
bool deleteFile(const char *filePath, int *err);
bool makeDir(const char *dirPath, int *err);
bool removeDir(const char *dirPath, int *err);
bool getDate(const char *filePath, uint8_t &year, uint8_t &month, uint8_t &day, int *err);
bool getTime(const char *filePath, uint8_t &hour, uint8_t &minute, uint8_t &second, int *err);

uint16_t getInfoVersion(int diskDriveIndex);
bool getInfo(int diskDriveIndex, uint64_t &usedSpace, uint64_t &freeSpace, bool fromCache);

////////////////////////////////////////////////////////////////////////////////

class BufferedFileRead {
public:
    BufferedFileRead(File &file, size_t bufferSize = BUFFER_SIZE);

    int peek();
    int read();
    int read(void *buf, uint32_t nbyte);
    bool available();

    size_t size();
    size_t tell();

private:
    File &file;
    static const size_t BUFFER_SIZE = 512;
    uint8_t buffer[BUFFER_SIZE];
    uint32_t bufferSize;
    size_t position;
    size_t end;

    void readNextChunk();
};

class BufferedFileWrite {
public:
    BufferedFileWrite(File &file);

    bool write(const uint8_t *buf, size_t size);

    bool print(float value, int numDecimalDigits);
    bool print(char value);

    bool flush();

private:
    File &file;
    static const size_t BUFFER_SIZE = 512;
    uint8_t buffer[BUFFER_SIZE];
    size_t position;
};

void matchZeroOrMoreSpaces(BufferedFileRead &file);
bool match(BufferedFileRead &file, char c);
bool match(BufferedFileRead &file, const char *str);
bool matchUntil(BufferedFileRead &file, char c, char *result, int count = -1);
void skipUntil(BufferedFileRead &file, const char *str);
void skipUntilEOL(BufferedFileRead &file);
bool matchQuotedString(BufferedFileRead &file, char *str, unsigned int strLength);
bool match(BufferedFileRead &file, int &result);
bool match(BufferedFileRead &file, unsigned int &result);
bool match(BufferedFileRead &file, float &result);

} // namespace sd_card
} // namespace psu
} // namespace eez
