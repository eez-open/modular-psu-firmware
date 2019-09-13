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
#include <stdlib.h>

#ifdef EEZ_PLATFORM_SIMULATOR
#include <stdio.h>
#ifdef EEZ_PLATFORM_SIMULATOR_WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#else
#include <dirent.h>
#endif
#endif

#ifdef EEZ_PLATFORM_STM32
#include <fatfs.h>
#endif

#define FILE_READ 0x01
#define FILE_WRITE 0x02
#define FILE_OPEN_EXISTING 0x00
#define FILE_CREATE_NEW 0x04
#define FILE_CREATE_ALWAYS 0x08
#define FILE_OPEN_ALWAYS 0x10
#define FILE_OPEN_APPEND 0x30

namespace eez {

// clang-format off
enum SdFatResult {
    SD_FAT_RESULT_OK = 0,          /* (0) Succeeded */
    SD_FAT_RESULT_DISK_ERR,        /* (1) A hard error occurred in the low level disk I/O layer */
    SD_FAT_RESULT_INT_ERR,         /* (2) Assertion failed */
    SD_FAT_RESULT_NOT_READY,       /* (3) The physical drive cannot work */
    SD_FAT_RESULT_NO_FILE,         /* (4) Could not find the file */
    SD_FAT_RESULT_NO_PATH,         /* (5) Could not find the path */
    SD_FAT_RESULT_INVALID_NAME,    /* (6) The path name format is invalid */
    SD_FAT_RESULT_DENIED,          /* (7) Access denied due to prohibited access or directory full */
    SD_FAT_RESULT_EXIST,           /* (8) Access denied due to prohibited access */
    SD_FAT_RESULT_INVALID_OBJECT,  /* (9) The file/directory object is invalid */
    SD_FAT_RESULT_WRITE_PROTECTED, /* (10) The physical drive is write protected */
    SD_FAT_RESULT_INVALID_DRIVE,   /* (11) The logical drive number is invalid */
    SD_FAT_RESULT_NOT_ENABLED,     /* (12) The volume has no work area */
    SD_FAT_RESULT_NO_FILESYSTEM,   /* (13) There is no valid FAT volume */
    SD_FAT_RESULT_MKFS_ABORTED,    /* (14) The f_mkfs() aborted due to any problem */
    SD_FAT_RESULT_TIMEOUT,         /* (15) Could not get a grant to access the volume within defined period */
    SD_FAT_RESULT_LOCKED,          /* (16) The operation is rejected according to the file sharing policy */
    SD_FAT_RESULT_NOT_ENOUGH_CORE, /* (17) LFN working buffer could not be allocated */
    SD_FAT_RESULT_TOO_MANY_OPEN_FILES, /* (18) Number of open files > _FS_LOCK */
    SD_FAT_RESULT_INVALID_PARAMETER,    /* (19) Given parameter is invalid */
};
// clang-format on

struct FileInfo {
    FileInfo();

    SdFatResult fstat(const char *filePath);

    operator bool(); // fname[0]
    bool isDirectory();
    void getName(char *name, size_t size);
    size_t getSize();

    int getModifiedYear();
    int getModifiedMonth();
    int getModifiedDay();

    int getModifiedHour();
    int getModifiedMinute();
    int getModifiedSecond();

#if defined(EEZ_PLATFORM_STM32)
    FILINFO m_fno;
#elif defined(EEZ_PLATFORM_SIMULATOR_WIN32)
    WIN32_FIND_DATAA m_ffd;
#else
    struct dirent *m_dirent;
#endif
};

struct Directory {
    Directory();
    ~Directory();

    void close();

    SdFatResult findFirst(const char *path, const char *pattern, FileInfo &fileInfo);
    SdFatResult findFirst(const char *path, FileInfo &fileInfo);
    SdFatResult findNext(FileInfo &fileInfo);

#if defined(EEZ_PLATFORM_STM32)
    DIR m_dj;
#else
    void *m_handle;
#endif
};

class File {
    File(const File &file);
    const File &operator=(const File &file);

  public:
    File();

    bool open(const char *path, uint8_t mode = FILE_READ);

    ~File();
    void close();

    bool truncate(uint32_t length);

    bool available();
    size_t size();
    bool seek(uint32_t pos);
    int peek();
    int read();
    int read(void *buf, uint16_t nbyte);
    size_t write(const uint8_t *buf, size_t size);
    void sync();

    void print(float value, int numDecimalDigits);
    void print(char value);

  private:
#ifdef EEZ_PLATFORM_SIMULATOR
    FILE *m_fp;
#else
    FIL m_file;
#endif
};

class SdFat {
  public:
    bool begin();
    bool exists(const char *path);
    bool rename(const char *sourcePath, const char *destinationPath);
    bool remove(const char *path);
    bool mkdir(const char *path);
    bool rmdir(const char *path);

    bool getInfo(uint64_t &usedSpace, uint64_t &freeSpace);
};

char *getConfFilePath(const char *file_name);

} // namespace eez
