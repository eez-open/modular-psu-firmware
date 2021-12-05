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

#if defined(EEZ_PLATFORM_STM32)

#include <stdio.h>
#include <string.h>

#include <scpi/scpi.h>

#include <eez/debug.h>
#include <eez/memory.h>
#include <eez/util.h>
#include <bb3/fs/fs.h>

#define CHECK_ERROR(desc, err) (void)(desc); (void)(err);
//#define CHECK_ERROR(desc, err) if (err != FR_OK) DebugTrace("%s: %d\n", desc, (int)err)

namespace eez {

////////////////////////////////////////////////////////////////////////////////

FileInfo::FileInfo() {
    memset(&m_fno, 0, sizeof(m_fno));
}

SdFatResult FileInfo::fstat(const char *filePath) {
    auto result = f_stat(filePath, &m_fno);
    CHECK_ERROR("FileInfo::fstat", result);
    return (SdFatResult)result;
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

    stringCopy(name, size, str2);
}

size_t FileInfo::getSize() {
    return m_fno.fsize;
}

bool FileInfo::isHiddenOrSystemFile() {
    return (m_fno.fattrib & (AM_HID | AM_SYS)) != 0;
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
    auto result = f_closedir(&m_dj);
    CHECK_ERROR("Directory::close", result);
}

SdFatResult Directory::findFirst(const char *path, const char *pattern, FileInfo &fileInfo) {
    auto result = f_findfirst(&m_dj, &fileInfo.m_fno, path, pattern ? pattern : "*");
    CHECK_ERROR("Directory::findFirst", result);
    return (SdFatResult)result;
}

SdFatResult Directory::findFirst(const char *path, FileInfo &fileInfo) {
    return findFirst(path, nullptr, fileInfo);
}

SdFatResult Directory::findNext(FileInfo &fileInfo) {
    auto result = f_findnext(&m_dj, &fileInfo.m_fno);
    CHECK_ERROR("Directory::findNext", result);
    return (SdFatResult)result;
}

////////////////////////////////////////////////////////////////////////////////

File::File() : m_isOpen(false) {
}

bool File::open(const char *path, uint8_t mode) {
	auto result = f_open(&m_file, path, mode);
    CHECK_ERROR("File::open", result);
    m_isOpen = result == FR_OK;
	return m_isOpen;
}

File::~File() {
}

bool File::close() {
    auto result = f_close(&m_file);
    CHECK_ERROR("File::close", result);
    m_isOpen = false;
    return result == FR_OK;
}

bool File::isOpen() {
    return m_isOpen;
}

bool File::truncate(uint32_t length) {
    auto result1 = f_lseek(&m_file, length);
    CHECK_ERROR("File::truncate 1", result1);
    auto result2 = f_truncate(&m_file);
    CHECK_ERROR("File::truncate 2", result2);
    return result1 == FR_OK && result2 == FR_OK;
}

size_t File::size() {
    return f_size(&m_file);
}

bool File::available() {
    return peek() != EOF;
}

bool File::seek(uint32_t pos) {
    auto result = f_lseek(&m_file, pos);
    CHECK_ERROR("File::seek", result);
    return result == FR_OK;
}

size_t File::tell() {
    auto result = f_tell(&m_file);
    CHECK_ERROR("File::tell", result);
    return result;
}

int File::peek() {
    auto pos = f_tell(&m_file);
    int ch = read();
    auto result = f_lseek(&m_file, pos);
    CHECK_ERROR("File::peek", result);
    return ch;
}

int File::read() {
    uint8_t value;
    UINT br;
    auto result = f_read(&m_file, &value, 1, &br);
    CHECK_ERROR("File::read", result);
    return result != FR_OK || br != 1 ? EOF : (int)value;
}

size_t File::read(void *buf, uint32_t size) {
    uint32_t CHUNK_SIZE = 512; // unfortunately, it doesn't work when CHUNK_SIZE is > 512

    UINT brTotal = 0;

	size_t unalignedLength = ((uint32_t)buf) & 3;
	if (unalignedLength > 0) {
		CHUNK_SIZE = 512;
    	unalignedLength = MIN(4 - unalignedLength, size);
		uint8_t unalignedBuffer[4] __attribute__((aligned));
        UINT br;
        auto result = f_read(&m_file, unalignedBuffer, unalignedLength, &br);
        CHECK_ERROR("File::read 1", result);
        if (result != FR_OK) {
            return 0;
        }

		for (size_t i = 0; i < br; i++) {
			((uint8_t *)buf)[i] = unalignedBuffer[i];
		}

		brTotal += br;

        if (br < unalignedLength) {
            return brTotal;
        }
    }
    
    while (brTotal < size) {
        uint32_t btr = MIN(CHUNK_SIZE, size - brTotal);

        UINT br;
        auto result = f_read(&m_file, (uint8_t *)buf + brTotal, btr, &br);
        CHECK_ERROR("File::read 2", result);
        if (result != FR_OK) {
            return brTotal;
        }

        brTotal += br;

        if (br < btr) {
            break;
        }
    }

    return brTotal;    
}

size_t File::write(const void *buf, size_t size) {
	uint32_t CHUNK_SIZE = 64 * 1024;

    UINT bwTotal = 0;

	size_t unalignedLength = ((uint32_t)buf) & 3;
	if (unalignedLength > 0) {
        CHUNK_SIZE = 512;

        unalignedLength = MIN(4 - unalignedLength, size);
		uint8_t unalignedBuffer[4] __attribute__((aligned));
		for (size_t i = 0; i < unalignedLength; i++) {
			unalignedBuffer[i] = ((uint8_t *)buf)[i];
		}

		UINT bw;
		auto result = f_write(&m_file, unalignedBuffer, unalignedLength, &bw);
        CHECK_ERROR("File::write 1", result);
		if (result != FR_OK) {
			return 0;
		}

		bwTotal += bw;

        if (bw < unalignedLength) {
            return bwTotal;
        }        
	}

	while (bwTotal < size) {
		auto btw = MIN(CHUNK_SIZE, size - bwTotal);

		UINT bw;
		auto result = f_write(&m_file, (const uint8_t *)buf + bwTotal, btw, &bw);
        CHECK_ERROR("File::write 2", result);
		if (result != FR_OK) {
			return bwTotal;
		}

        bwTotal += bw;

        if (bw < btw) {
            break;
        }
	}

	return bwTotal;
}

bool File::sync() {
    auto result = f_sync(&m_file);
    CHECK_ERROR("File::sync", result);
    return result == FR_OK;
}

void File::print(float value, int numDecimalDigits) {
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "%.*f", numDecimalDigits, value);
    write((uint8_t *)buffer, strlen(buffer));
}

void File::print(char value) {
    auto result = f_printf(&m_file, "%c", value);
    CHECK_ERROR("File:print", result);
}

////////////////////////////////////////////////////////////////////////////////

bool SdFat::mount(int *err) {
	auto result = f_mount(&SDFatFS, SDPath, 1);
    CHECK_ERROR("SdFat::mount", result);
	if (result != FR_OK) {
		if (result == FR_NO_FILESYSTEM) {
			*err = SCPI_ERROR_MASS_MEDIA_NO_FILESYSTEM;
		} else {
			*err = SCPI_ERROR_MASS_STORAGE_ERROR;
		}
		return false;
	}

	*err = SCPI_RES_OK;
    return true;
}

void SdFat::unmount() {
    auto result = f_mount(0, "", 0);
    CHECK_ERROR("SdFat::unmount", result);
    memset(&SDFatFS, 0, sizeof(SDFatFS));
}

bool SdFat::exists(const char *path) {
    if (strcmp(path, "/") == 0) {
        return true;
    }
    FILINFO fno;
    auto result = f_stat(path, &fno);
    CHECK_ERROR("SdFat::exists", result);
    return result == FR_OK;
}

bool SdFat::rename(const char *sourcePath, const char *destinationPath) {
    auto result = f_rename(sourcePath, destinationPath);
    CHECK_ERROR("SdFat::rename", result);
    return result == FR_OK;
}

bool SdFat::remove(const char *path) {
    auto result = f_unlink(path);
    CHECK_ERROR("SdFat::remove", result);
    return result == FR_OK;
}

bool SdFat::mkdir(const char *path) {
    auto result = f_mkdir(path);
    CHECK_ERROR("SdFat::mkdir", result);
    return result == FR_OK;
}

bool SdFat::rmdir(const char *path) {
    auto result = f_unlink(path);
    CHECK_ERROR("SdFat::rmdir", result);
    return result == FR_OK;
}

bool SdFat::getInfo(int diskDriveIndex, uint64_t &usedSpace, uint64_t &freeSpace) {
    char path[3];

    path[0] = '0' + diskDriveIndex;
    path[1] = ':';
    path[2] = 0;

    DWORD freeClusters;
    FATFS *fs;
    auto result = f_getfree(path, &freeClusters, &fs);
    CHECK_ERROR("SdFat::getInfo", result);
    if (result != FR_OK) {
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

#endif // EEZ_PLATFORM_STM32
