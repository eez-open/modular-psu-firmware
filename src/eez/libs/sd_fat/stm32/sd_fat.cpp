/*
 * EEZ PSU Firmware
 * Copyright (C) 2017-present, Envox d.o.o.
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

#include <eez/libs/sd_fat/sd_fat.h>

#include <stdio.h>
#include <string.h>

namespace eez {

////////////////////////////////////////////////////////////////////////////////

FileInfo::FileInfo() {
    memset(&m_fno, 0, sizeof(m_fno));
}

FileInfo::operator bool() {
    return m_fno.fname[0] ? true : false;
}

bool FileInfo::isDirectory() {
    return m_fno.fattrib & AM_DIR ? true : false;
}

void FileInfo::getName(char *name, size_t size) {
    const char *str1 = strrchr(m_fno.fname, '\\');
    if (!str1) {
        str1 = m_fno.fname;
    }

    const char *str2 = strrchr(str1, '/');
    if (!str2) {
        str2 = str1;
    }

    strncpy(name, str2, size);
    name[size] = 0;
}

size_t FileInfo::getSize() {
    return m_fno.fsize;
}

#define FAT_YEAR(date) (1980 + ((date) >> 9))
#define FAT_MONTH(date) (((date) >> 5) & 0XF)
#define FAT_DAY(date) ((date)&0X1F)

#define FAT_HOUR(time) ((time) >> 11)
#define FAT_MINUTE(time) (((time) >> 5) & 0X3F)
#define FAT_SECOND(time) (2 * ((time)&0X1F))

int FileInfo::getModifiedYear() {
    return FAT_YEAR(m_fno.fdate);
}

int FileInfo::getModifiedMonth() {
    return FAT_MONTH(m_fno.fdate);
}

int FileInfo::getModifiedDay() {
    return FAT_DAY(m_fno.fdate);
}

int FileInfo::getModifiedHour() {
    return FAT_HOUR(m_fno.ftime);
}

int FileInfo::getModifiedMinute() {
    return FAT_MINUTE(m_fno.ftime);
}

int FileInfo::getModifiedSecond() {
    return FAT_SECOND(m_fno.ftime);
}

////////////////////////////////////////////////////////////////////////////////

Directory::Directory() {
    memset(&m_dj, 0, sizeof(m_dj));
}

Directory::~Directory() {
    close();
}

void Directory::close() {
    f_closedir(&m_dj);
}

SdFatResult Directory::findFirst(const char *path, const char *pattern, FileInfo &fileInfo) {
    return (SdFatResult)f_findfirst(&m_dj, &fileInfo.m_fno, path, pattern);
}

SdFatResult Directory::findFirst(const char *path, FileInfo &fileInfo) {
    return findFirst(path, nullptr, fileInfo);
}

SdFatResult Directory::findNext(FileInfo &fileInfo) {
    return (SdFatResult)f_findnext(&m_dj, &fileInfo.m_fno);
}

////////////////////////////////////////////////////////////////////////////////

File::File() {
    memset(&m_file, 0, sizeof(m_file));
}

bool File::open(const char *path, uint8_t mode) {
    FRESULT result = f_open(&m_file, path, mode);
    return result == FR_OK;
}

File::~File() {
    close();
}

void File::close() {
    f_close(&m_file);
}

bool File::truncate(uint32_t length) {
    return f_lseek(&m_file, length) == FR_OK && f_truncate(&m_file) == FR_OK;
}

size_t File::size() {
    return f_size(&m_file);
}

bool File::available() {
    return peek() != EOF;
}

bool File::seek(uint32_t pos) {
    return f_lseek(&m_file, pos) == FR_OK;
}

int File::peek() {
    auto pos = f_tell(&m_file);
    int result = read();
    f_lseek(&m_file, pos);
    return result;
}

int File::read() {
    uint8_t value;
    UINT br;
    auto result = f_read(&m_file, &value, 1, &br);
    return result != FR_OK || br != 1 ? EOF : (int)value;
}

int File::read(void *buf, uint16_t nbyte) {
    UINT br;
    auto result = f_read(&m_file, buf, nbyte, &br);
    return result == FR_OK ? br : 0;
}

size_t File::write(const uint8_t *buf, size_t size) {
    UINT bw;
    auto result = f_write(&m_file, buf, size, &bw);
    return result == FR_OK ? bw : 0;
}

void File::sync() {
    f_sync(&m_file);
}

void File::print(float value, int numDecimalDigits) {
    // TODO: test this
    f_printf(&m_file, "%.*f", numDecimalDigits, value);
}

void File::print(char value) {
    f_printf(&m_file, "%c", value);
}

////////////////////////////////////////////////////////////////////////////////

bool SdFat::begin() {
    return f_mount(&SDFatFS, SDPath, 1) == FR_OK;
}

bool SdFat::exists(const char *path) {
    FILINFO fno;
    return f_stat(path, &fno) == FR_OK;
}

bool SdFat::rename(const char *sourcePath, const char *destinationPath) {
    return f_rename(sourcePath, destinationPath) == FR_OK;
}

bool SdFat::remove(const char *path) {
    return f_unlink(path) == FR_OK;
}

bool SdFat::mkdir(const char *path) {
    return f_mkdir(path) == FR_OK;
}

bool SdFat::rmdir(const char *path) {
    return f_unlink(path) == FR_OK;
}

bool SdFat::getInfo(uint64_t &usedSpace, uint64_t &freeSpace) {
    DWORD freeClusters;
    FATFS *fs;
    if (f_getfree(SDPath, &freeClusters, &fs) != FR_OK) {
        return false;
    }

    DWORD totalSector = (fs->n_fatent - 2) * fs->csize;
    DWORD freeSector = freeClusters * fs->csize;

    uint64_t totalSpace = totalSector * uint64_t(512);
    freeSpace = freeSector * uint64_t(512);
    usedSpace = totalSpace - freeSpace;

    return true;
}

} // namespace eez
