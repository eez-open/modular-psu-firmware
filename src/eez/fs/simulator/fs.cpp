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

#include <eez/fs/fs.h>

#include <stdio.h>
#include <time.h>

#ifdef EEZ_PLATFORM_SIMULATOR_WIN32

#undef INPUT
#undef OUTPUT

#include <direct.h>
#include <io.h>
#include <sys/stat.h>

#else

#include <dirent.h>
#include <errno.h>
#include <sys/stat.h> // mkdir
#include <sys/statvfs.h>
#include <unistd.h>

#endif

#include <eez/core/util.h>

#if OPTION_SCPI
#include <scpi/scpi.h>
#else
#define SCPI_ERROR_MASS_STORAGE_ERROR -321
#define SCPI_RES_OK -161
#endif


namespace eez {

////////////////////////////////////////////////////////////////////////////////

FileInfo::FileInfo() {
#if defined(EEZ_PLATFORM_SIMULATOR_WIN32)
    memset(&m_ffd, 0, sizeof(WIN32_FIND_DATAA));
#else
    m_dirent = 0;
#endif
}

SdFatResult FileInfo::fstat(const char *filePath) {
    Directory dir;
    return dir.findFirst(filePath, *this);
}

std::string getRealPath(const char *path) {
    std::string realPath;

    realPath = "sd_card";

    if (path[0] >= '0' && path[0] <= '9' && path[1] == ':') {
        if (path[0] >= '1') {
            realPath += "_slot_";
            realPath += path[0];
        }
        path += 2;
    }

    if (path[0]) {
        if (path[0] != '/') {
            realPath += "/";
        }

        realPath += path;
    }

    // remove trailing slash
    if (realPath[realPath.size() - 1] == '/') {
        realPath = realPath.substr(0, realPath.size() - 1);
    }

    return getConfFilePath(realPath.c_str());
}

bool pathExists(const char *path) {
    std::string realPath = getRealPath(path);
    struct stat path_stat;
    int result = stat(realPath.c_str(), &path_stat);
    return result == 0;
}

////////////////////////////////////////////////////////////////////////////////

FileInfo::operator bool() {
#ifdef EEZ_PLATFORM_SIMULATOR_WIN32
    return m_ffd.cFileName[0] != 0;
#else
    return m_dirent->d_name[0] != 0;
#endif
}

bool FileInfo::isDirectory() {
#ifdef EEZ_PLATFORM_SIMULATOR_WIN32
    return m_ffd.dwFileAttributes == FILE_ATTRIBUTE_DIRECTORY;
#else
    struct stat stbuf;
    stat((m_parentPath + "/" + m_dirent->d_name).c_str(), &stbuf);
    return S_ISDIR(stbuf.st_mode);
#endif
}

void FileInfo::getName(char *name, size_t size) {
    const char *str;
#ifdef EEZ_PLATFORM_SIMULATOR_WIN32
    str = strrchr(m_ffd.cFileName, '\\');
    if (!str) {
        str = m_ffd.cFileName;
    }
#else
    str = strrchr(m_dirent->d_name, '/');
    if (!str) {
        str = m_dirent->d_name;
    }
#endif
    stringCopy(name, size, str);
}

size_t FileInfo::getSize() {
#ifdef EEZ_PLATFORM_SIMULATOR_WIN32
    return (size_t)((m_ffd.nFileSizeHigh * ((uint64_t)MAXDWORD + 1)) + m_ffd.nFileSizeLow);
#else
    struct stat stbuf;
    stat((m_parentPath + "/" + m_dirent->d_name).c_str(), &stbuf);
    return stbuf.st_size;
#endif
}

bool FileInfo::isHiddenOrSystemFile() {
    return false;
}

struct tm *getGmTime(FileInfo &fileInfo) {
    time_t mtime;

#ifdef EEZ_PLATFORM_SIMULATOR_WIN32
    ULARGE_INTEGER ull;

    ull.LowPart = fileInfo.m_ffd.ftLastWriteTime.dwLowDateTime;
    ull.HighPart = fileInfo.m_ffd.ftLastWriteTime.dwHighDateTime;

    mtime = ull.QuadPart / 10000000ULL - 11644473600ULL;
#else
    struct stat stbuf;
    stat((fileInfo.m_parentPath + "/" + fileInfo.m_dirent->d_name).c_str(), &stbuf);
    mtime = stbuf.st_mtime;
#endif

    return gmtime(&mtime);
}

int FileInfo::getModifiedYear() {
    struct tm *tmmtime = getGmTime(*this);
    return tmmtime->tm_year + 1900;
}

int FileInfo::getModifiedMonth() {
    struct tm *tmmtime = getGmTime(*this);
    return tmmtime->tm_mon + 1;
}

int FileInfo::getModifiedDay() {
    struct tm *tmmtime = getGmTime(*this);
    return tmmtime->tm_mday;
}

int FileInfo::getModifiedHour() {
    struct tm *tmmtime = getGmTime(*this);
    return tmmtime->tm_hour;
}

int FileInfo::getModifiedMinute() {
    struct tm *tmmtime = getGmTime(*this);
    return tmmtime->tm_min;
}

int FileInfo::getModifiedSecond() {
    struct tm *tmmtime = getGmTime(*this);
    return tmmtime->tm_sec;
}

////////////////////////////////////////////////////////////////////////////////

Directory::Directory()
#ifdef EEZ_PLATFORM_SIMULATOR_WIN32
    : m_handle(INVALID_HANDLE_VALUE)
#else
    : m_handle(0)
#endif
{
}

Directory::~Directory() {
    close();
}

void Directory::close() {
#ifdef EEZ_PLATFORM_SIMULATOR_WIN32
    if (m_handle != INVALID_HANDLE_VALUE) {
        FindClose(m_handle);
        m_handle = INVALID_HANDLE_VALUE;
    }
#else
    if (m_handle) {
        closedir((DIR *)m_handle);
        m_handle = 0;
    }
#endif
}

SdFatResult Directory::findFirst(const char *path, const char *pattern, FileInfo &fileInfo) {
    std::string temp = path;

    if (pattern) {
        temp += "/";
        temp += pattern;
    }
#ifdef EEZ_PLATFORM_SIMULATOR_WIN32
    else {
        temp += "/*.*";
    }   
#endif 

    temp = getRealPath(temp.c_str());

#ifdef EEZ_PLATFORM_SIMULATOR_WIN32
    m_handle = FindFirstFileA(temp.c_str(), &fileInfo.m_ffd);
    if (m_handle == INVALID_HANDLE_VALUE) {
        temp = getRealPath(path);
        m_handle = FindFirstFileA(temp.c_str(), &fileInfo.m_ffd);
        if (m_handle == INVALID_HANDLE_VALUE) {
            // TODO check FatFs what he returns
            return SD_FAT_RESULT_NO_PATH;
        }
    }
#else
    m_handle = opendir(temp.c_str());
    if (!m_handle) {
        // TODO check FatFs what he returns
        return SD_FAT_RESULT_NO_PATH;
    }
    struct dirent *ep = readdir((DIR *)m_handle);
    if (!ep) {
        // TODO check FatFs what he returns
        return SD_FAT_RESULT_NO_PATH;
    }
    fileInfo.m_parentPath = temp;
    fileInfo.m_dirent = ep;
#endif
    return SD_FAT_RESULT_OK;
}

SdFatResult Directory::findFirst(const char *path, FileInfo &fileInfo) {
    return findFirst(path, nullptr, fileInfo);
}

SdFatResult Directory::findNext(FileInfo &fileInfo) {
#ifdef EEZ_PLATFORM_SIMULATOR_WIN32
    if (!FindNextFileA(m_handle, &fileInfo.m_ffd)) {
        // TODO check FatFs what he returns
        return SD_FAT_RESULT_NO_FILE;
    }
#else
    struct dirent *ep = readdir((DIR *)m_handle);
    if (!ep) {
        // TODO check FatFs what he returns
        return SD_FAT_RESULT_NO_FILE;
    }
    fileInfo.m_dirent = ep;
#endif
    return SD_FAT_RESULT_OK;
}

////////////////////////////////////////////////////////////////////////////////

File::File() {}

bool File::open(const char *path, uint8_t mode) {
    const char *fmode;

    fmode = "";

    if (mode == (FILE_OPEN_EXISTING | FILE_READ)) {
        fmode = "rb";
    } else if (mode == (FILE_OPEN_ALWAYS | FILE_WRITE)) {
        fmode = "r+b";
        m_fp = fopen(getRealPath(path).c_str(), fmode);
        if (m_fp) {
            return true;
        }
        fmode = "wb";
    } else if (mode == (FILE_OPEN_APPEND | FILE_WRITE)) {
        fmode = "ab";
    } else if (mode == (FILE_CREATE_ALWAYS | FILE_WRITE)) {
        fmode = "wb";
    }

    m_fp = fopen(getRealPath(path).c_str(), fmode);

    m_isOpen = m_fp != nullptr;

    return m_fp != nullptr;
}

File::~File() {
    close();
}

bool File::close() {
    int result = 0;
    if (m_fp) {
        result = fclose(m_fp);
        m_fp = NULL;
    }
    m_isOpen = false;
    return !result;
}

bool File::isOpen() {
    return m_isOpen;
}

bool File::truncate(uint32_t length) {
#ifdef EEZ_PLATFORM_SIMULATOR_WIN32
    return _chsize(_fileno(m_fp), length) == 0;
#else
    return ftruncate(fileno(m_fp), length) == 0;
#endif
}

size_t File::size() {
    uint32_t curpos = ftell(m_fp);
    fseek(m_fp, 0, SEEK_END);
    uint32_t size = ftell(m_fp);
    fseek(m_fp, curpos, SEEK_SET);
    return size;
}

bool File::available() {
    return peek() != EOF;
}

bool File::seek(uint32_t pos) {
    return fseek(m_fp, pos, SEEK_SET) == 0;
}

size_t File::tell() {
    return ftell(m_fp);
}

int File::peek() {
    int c = getc(m_fp);

    if (c == EOF) {
        return EOF;
    }

    ungetc(c, m_fp);

    return c;
}

int File::read() {
    return getc(m_fp);
}

size_t File::read(void *buf, uint32_t nbyte) {
    return fread(buf, 1, nbyte, m_fp);
}

size_t File::write(const void *buf, size_t size) {
    return fwrite(buf, 1, size, m_fp);
}

bool File::sync() {
    return !fflush(m_fp);
}

void File::print(float value, int numDecimalDigits) {
    fprintf(m_fp, "%.*f", numDecimalDigits, value);
}

void File::print(char value) {
    fputc(value, m_fp);
}

////////////////////////////////////////////////////////////////////////////////

bool SdFat::mount(int *err) {
    // make sure SD card root path exists
    mkdir("/");
	mkdir("1:/");
	mkdir("2:/");
	mkdir("3:/");
    
    if (!pathExists("/")) {
        *err = SCPI_ERROR_MASS_STORAGE_ERROR;
        return false;
    } 
    
    *err = SCPI_RES_OK;
    return true;
}

void SdFat::unmount() {
}

bool SdFat::exists(const char *path) {
    return pathExists(path);
}

bool SdFat::rename(const char *sourcePath, const char *destinationPath) {
    std::string realSourcePath = getRealPath(sourcePath);
    std::string realDestinationPath = getRealPath(destinationPath);
    return ::rename(realSourcePath.c_str(), realDestinationPath.c_str()) == 0;
}

bool SdFat::remove(const char *path) {
    std::string realPath = getRealPath(path);
    return ::remove(realPath.c_str()) == 0;
}

bool SdFat::mkdir(const char *path) {
    if (pathExists(path)) {
        return true;
    }

    char parentDir[205];
    getParentDir(path, parentDir);

    if (parentDir[0]) {
        if (!mkdir(parentDir)) {
            return false;
        }
    }

    std::string realPath = getRealPath(path);

    int result;
#ifdef EEZ_PLATFORM_SIMULATOR_WIN32
    result = ::_mkdir(realPath.c_str());
#else
    result = ::mkdir(realPath.c_str(), 0700);
#endif
    return result == 0 || result == EEXIST;
}

bool SdFat::rmdir(const char *path) {
    std::string realPath = getRealPath(path);
#ifdef EEZ_PLATFORM_SIMULATOR_WIN32
    int result = ::_rmdir(realPath.c_str());
#else
    int result = ::rmdir(realPath.c_str());
#endif
    return result == 0;
}

bool SdFat::getInfo(int diskDriveIndex, uint64_t &usedSpace, uint64_t &freeSpace) {
#ifdef EEZ_PLATFORM_SIMULATOR_WIN32
    DWORD sectorsPerCluster, bytesPerSector, numberOfFreeClusters, totalNumberOfClusters;
    GetDiskFreeSpaceA(getRealPath("").c_str(), &sectorsPerCluster, &bytesPerSector,
                      &numberOfFreeClusters, &totalNumberOfClusters);
    usedSpace = uint64_t(bytesPerSector) * sectorsPerCluster *
                (totalNumberOfClusters - numberOfFreeClusters);
    freeSpace = uint64_t(bytesPerSector) * sectorsPerCluster * numberOfFreeClusters;
    return true;
#else
    struct statvfs buf;
    statvfs(getRealPath("").c_str(), &buf);
    usedSpace = uint64_t(buf.f_bsize) * (buf.f_blocks - buf.f_bfree);
    freeSpace = uint64_t(buf.f_bsize) * buf.f_bfree;
    return true;
#endif
}

} // namespace eez