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

#if OPTION_SD_CARD

#if defined(EEZ_PLATFORM_STM32)
#include <gpio.h>
#include <sdmmc.h>
#include <fatfs.h>
#endif

#include <eez/apps/psu/psu.h>

#include <scpi/scpi.h>
#include <eez/scpi/scpi.h>

#include <eez/apps/psu/datetime.h>
#include <eez/apps/psu/event_queue.h>
#include <eez/apps/psu/list_program.h>
#include <eez/apps/psu/profile.h>
#include <eez/apps/psu/sd_card.h>
#include <eez/apps/psu/scpi/psu.h>

#if OPTION_DISPLAY
#include <eez/apps/psu/gui/psu.h>
#include <eez/gui/dialogs.h>
#endif

#include <eez/libs/sd_fat/sd_fat.h>

extern "C" int g_sdCardIsPresent;

namespace eez {

SdFat SD;

using namespace scpi;

namespace psu {
namespace sd_card {

bool g_mounted;

TestResult g_testResult = TEST_FAILED;
int g_lastError;

////////////////////////////////////////////////////////////////////////////////

void mount() {
#if defined(EEZ_PLATFORM_STM32)
	MX_FATFS_Init();
#endif
	if (SD.mount(&g_lastError)) {
		g_mounted = true;
		g_testResult = TEST_OK;
		setQuesBits(QUES_MMEM, false);
	} else {
		g_testResult = TEST_FAILED;
	}
}

void unmount() {
    SD.unmount();
#if defined(EEZ_PLATFORM_STM32)
	FATFS_UnLinkDriver(SDPath);
#endif
	g_mounted = false;
    g_lastError = SCPI_ERROR_MISSING_MASS_MEDIA;
	g_testResult = TEST_FAILED;
	setQuesBits(QUES_MMEM, true);
}

void init() {
#if defined(EEZ_PLATFORM_STM32)
    MX_SDMMC1_SD_Init();
	g_sdCardIsPresent = HAL_GPIO_ReadPin(SD_DETECT_GPIO_Port, SD_DETECT_Pin) == GPIO_PIN_RESET ? 1 : 0;
	if (g_sdCardIsPresent) {
		mount();
	} else {
        g_lastError = SCPI_ERROR_MISSING_MASS_MEDIA;
        g_testResult = TEST_FAILED;
	}
#endif

#if defined(EEZ_PLATFORM_SIMULATOR)
	mount();
#endif
}

bool test() {
    return g_testResult != TEST_FAILED;
}

void tick() {
#if defined(EEZ_PLATFORM_STM32)
	g_sdCardIsPresent = HAL_GPIO_ReadPin(SD_DETECT_GPIO_Port, SD_DETECT_Pin) == GPIO_PIN_RESET ? 1 : 0;
	if (g_sdCardIsPresent && !g_mounted && g_lastError == SCPI_ERROR_MISSING_MASS_MEDIA) {
		mount();
	}
#endif
}

#if defined(EEZ_PLATFORM_STM32)
void onSdDetectInterrupt() {
	// push message into SCPI thread queue so onSdDetectInterruptHandler is called inside SCPI thread
	osMessagePut(g_scpiMessageQueueId, SCPI_QUEUE_MESSAGE(SCPI_QUEUE_MESSAGE_TARGET_NONE, SCPI_QUEUE_MESSAGE_TYPE_SD_DETECT_IRQ, 0), 0);
}

void onSdDetectInterruptHandler() {
	if (g_mounted || g_testResult == TEST_FAILED) {
		unmount();
	}
}
#endif

bool isMounted(int *err) {
	if (g_mounted) {
        if (err != nullptr) {
            *err = SCPI_RES_OK;
        }
	    return true;
	}

    if (err != nullptr) {
        *err = g_lastError;
    }
	return false;
}

bool isBusy() {
    return false;
}

void dumpInfo(char *buffer) {
}

#ifndef isSpace
bool isSpace(int c) {
    return c == '\r' || c == '\n' || c == '\t' || c == ' ';
}
#endif

void matchZeroOrMoreSpaces(File &file) {
    while (true) {
        int c = file.peek();
        if (!isSpace(c)) {
            return;
        }
        file.read();
    }
}

bool match(File &file, char c) {
    matchZeroOrMoreSpaces(file);
    if (file.peek() == c) {
        file.read();
        return true;
    }
    return false;
}

bool match(File &file, float &result) {
    matchZeroOrMoreSpaces(file);

    int c = file.peek();
    if (c == -1) {
        return false;
    }

    bool isNegative;
    if (c == '-') {
        file.read();
        isNegative = true;
        c = file.peek();
    } else {
        isNegative = false;
    }

    bool isFraction = false;
    float fraction = 1.0;

    long value = -1;

    while (true) {
        if (c == '.') {
            if (isFraction) {
                return false;
            }
            isFraction = true;
        } else if (c >= '0' && c <= '9') {
            if (value == -1) {
                value = 0;
            }

            value = value * 10 + c - '0';

            if (isFraction) {
                fraction *= 0.1f;
            }
        } else {
            if (value == -1) {
                return false;
            }

            result = (float)value;
            if (isNegative) {
                result = -result;
            }
            if (isFraction) {
                result *= fraction;
            }

            return true;
        }

        file.read();
        c = file.peek();
    }
}

bool makeParentDir(const char *filePath) {
    char dirPath[MAX_PATH_LENGTH];
    getParentDir(filePath, dirPath);
    return SD.mkdir(dirPath);
}

bool exists(const char *dirPath, int *err) {
    if (!sd_card::isMounted(err)) {
        return false;
    }

    if (!SD.exists(dirPath)) {
        if (err)
            *err = SCPI_ERROR_FILE_NAME_NOT_FOUND;
        return false;
    }

    return true;
}

bool catalog(const char *dirPath, void *param,
             void (*callback)(void *param, const char *name, const char *type, size_t size),
             int *numFiles, int *err) {
    *numFiles = 0;

    if (!sd_card::isMounted(err)) {
        return false;
    }

    Directory dir;
    FileInfo fileInfo;
    if (dir.findFirst(dirPath, "*", fileInfo) != SD_FAT_RESULT_OK) {
        // TODO better error handling
        if (err)
            *err = SCPI_ERROR_FILE_NAME_NOT_FOUND;
        return false;
    }

    while (fileInfo) {
        char name[MAX_PATH_LENGTH + 1] = { 0 };
        fileInfo.getName(name, MAX_PATH_LENGTH);

        if (strcmp(name, ".") != 0 && strcmp(name, "..") != 0) {
            (*numFiles)++;
            if (fileInfo.isDirectory()) {
                callback(param, name, "FOLD", fileInfo.getSize());
            } else if (endsWith(name, LIST_EXT)) {
                callback(param, name, "LIST", fileInfo.getSize());
            } else if (endsWith(name, PROFILE_EXT)) {
                callback(param, name, "PROF", fileInfo.getSize());
            } else {
                callback(param, name, "BIN", fileInfo.getSize());
            }
        }

        if (dir.findNext(fileInfo) != SD_FAT_RESULT_OK) {
            break;
        }
    }

    dir.close();

    return true;
}

bool catalogLength(const char *dirPath, size_t *length, int *err) {
    if (!sd_card::isMounted(err)) {
        return false;
    }

    Directory dir;
    FileInfo fileInfo;
    if (dir.findFirst(dirPath, "*", fileInfo) != SD_FAT_RESULT_OK) {
        // TODO better error handling
        if (err)
            *err = SCPI_ERROR_FILE_NAME_NOT_FOUND;
        return false;
    }

    *length = 0;

    while (fileInfo) {
        char name[MAX_PATH_LENGTH + 1] = { 0 };
        fileInfo.getName(name, MAX_PATH_LENGTH);
        if (strcmp(name, ".") != 0 && strcmp(name, "..") != 0) {
            ++(*length);
        }

        if (dir.findNext(fileInfo) != SD_FAT_RESULT_OK) {
            break;
        }
    }

    return true;
}

bool upload(const char *filePath, void *param, void (*callback)(void *param, const void *buffer, int size), int *err) {
    if (!sd_card::isMounted(err)) {
        return false;
    }

    File file;
    if (!file.open(filePath, FILE_OPEN_EXISTING | FILE_READ)) {
        if (err)
            *err = SCPI_ERROR_FILE_NAME_NOT_FOUND;
        return false;
    }

    bool result = true;

    size_t totalSize = file.size();

#if OPTION_DISPLAY
    eez::gui::showProgressPage("Uploading...");
    size_t uploaded = 0;
#endif

    *err = SCPI_RES_OK;

    callback(param, NULL, totalSize);

    const int CHUNK_SIZE = CONF_SERIAL_BUFFER_SIZE;
    uint8_t buffer[CHUNK_SIZE];

    while (true) {
        int size = file.read(buffer, CHUNK_SIZE);

        callback(param, buffer, size);

#if OPTION_DISPLAY
        uploaded += size;
        if (!eez::gui::updateProgressPage(uploaded, totalSize)) {
            eez::gui::hideProgressPage();
            event_queue::pushEvent(event_queue::EVENT_WARNING_FILE_UPLOAD_ABORTED);
            if (err)
                *err = SCPI_ERROR_FILE_TRANSFER_ABORTED;
            result = false;
            break;
        }
#endif

        if (size < CHUNK_SIZE) {
            break;
        }
    }

    file.close();

    callback(param, NULL, -1);

#if OPTION_DISPLAY
    eez::gui::hideProgressPage();
#endif

    return result;
}

File file;

bool download(const char *filePath, bool truncate, const void *buffer, size_t size, int *err) {
    if (!sd_card::isMounted(err)) {
        return false;
    }

	if (truncate) {
	    if (!file.open(filePath, FILE_CREATE_ALWAYS | FILE_WRITE)) {
			if (err)
				*err = SCPI_ERROR_FILE_NAME_NOT_FOUND;
			return false;
		}
	}

    size_t written = file.write((const uint8_t *)buffer, size);

    if (written != size) {
        if (err)
            *err = SCPI_ERROR_MASS_STORAGE_ERROR;
        return false;
    }

    return true;
}

void downloadFinished() {
    file.close();
}

bool moveFile(const char *sourcePath, const char *destinationPath, int *err) {
    if (!sd_card::isMounted(err)) {
        return false;
    }

    if (!SD.exists(sourcePath)) {
        if (err)
            *err = SCPI_ERROR_FILE_NAME_NOT_FOUND;
        return false;
    }

    if (!SD.rename(sourcePath, destinationPath)) {
        if (err)
            *err = SCPI_ERROR_MASS_STORAGE_ERROR;
        return false;
    }

    return true;
}

bool copyFile(const char *sourcePath, const char *destinationPath, int *err) {
    if (!sd_card::isMounted(err)) {
        return false;
    }

    File sourceFile;
    if (!sourceFile.open(sourcePath, FILE_OPEN_EXISTING | FILE_READ)) {
        if (err)
            *err = SCPI_ERROR_FILE_NAME_NOT_FOUND;
        return false;
    }

    File destinationFile;
    if (!destinationFile.open(destinationPath, FILE_CREATE_ALWAYS | FILE_WRITE)) {
        sourceFile.close();
        if (err)
            *err = SCPI_ERROR_FILE_NAME_NOT_FOUND;
        return false;
    }

#if OPTION_DISPLAY
    eez::gui::showProgressPage("Copying...");
#endif

    const int CHUNK_SIZE = 512;
    uint8_t buffer[CHUNK_SIZE];
    size_t totalSize = sourceFile.size();
    size_t totalWritten = 0;

    while (true) {
        int size = sourceFile.read(buffer, CHUNK_SIZE);

        size_t written = destinationFile.write((const uint8_t *)buffer, size);
        if (size < 0 || written != (size_t)size) {
#if OPTION_DISPLAY
            eez::gui::hideProgressPage();
#endif
            sourceFile.close();
            destinationFile.close();
            deleteFile(destinationPath, NULL);
            if (err)
                *err = SCPI_ERROR_MASS_STORAGE_ERROR;
            return false;
        }

        totalWritten += written;

#if OPTION_DISPLAY
        if (!eez::gui::updateProgressPage(totalWritten, totalSize)) {
            sourceFile.close();
            destinationFile.close();

            deleteFile(destinationPath, NULL);
            if (err)
                *err = SCPI_ERROR_MASS_STORAGE_ERROR;

            eez::gui::hideProgressPage();
            return false;
        }
#endif

        if (size < CHUNK_SIZE) {
            break;
        }
    }

    sourceFile.close();
    destinationFile.close();

#if OPTION_DISPLAY
    eez::gui::hideProgressPage();
#endif

    if (totalWritten != totalSize) {
        deleteFile(destinationPath, NULL);
        if (err)
            *err = SCPI_ERROR_MASS_STORAGE_ERROR;
        return false;
    }

    return true;
}

bool deleteFile(const char *filePath, int *err) {
    if (!sd_card::isMounted(err)) {
        return false;
    }

    if (!SD.exists(filePath)) {
        if (err)
            *err = SCPI_ERROR_FILE_NAME_NOT_FOUND;
        return false;
    }

    if (!SD.remove(filePath)) {
        if (err)
            *err = SCPI_ERROR_MASS_STORAGE_ERROR;
        return false;
    }

    return true;
}

bool makeDir(const char *dirPath, int *err) {
    if (!sd_card::isMounted(err)) {
        return false;
    }

    if (!SD.mkdir(dirPath)) {
        if (err)
            *err = SCPI_ERROR_MASS_STORAGE_ERROR;
        return false;
    }

    return true;
}

bool removeDir(const char *dirPath, int *err) {
    if (!sd_card::isMounted(err)) {
        return false;
    }

    if (!SD.rmdir(dirPath)) {
        if (err)
            *err = SCPI_ERROR_MASS_STORAGE_ERROR;
        return false;
    }

    return true;
}

void getDateTime(FileInfo &fileInfo, uint8_t *resultYear, uint8_t *resultMonth, uint8_t *resultDay,
                 uint8_t *resultHour, uint8_t *resultMinute, uint8_t *resultSecond) {
    int year = fileInfo.getModifiedYear();
    int month = fileInfo.getModifiedMonth();
    int day = fileInfo.getModifiedDay();

    int hour = fileInfo.getModifiedHour();
    int minute = fileInfo.getModifiedMinute();
    int second = fileInfo.getModifiedSecond();

    uint32_t utc = datetime::makeTime(year, month, day, hour, minute, second);
    uint32_t local = datetime::utcToLocal(utc, persist_conf::devConf.time_zone,
                                          (datetime::DstRule)persist_conf::devConf.dstRule);
    datetime::breakTime(local, year, month, day, hour, minute, second);

    if (resultYear) {
        *resultYear = (uint8_t)(year - 2000);
        *resultMonth = (uint8_t)month;
        *resultDay = (uint8_t)day;
    }

    if (resultHour) {
        *resultHour = (uint8_t)hour;
        *resultMinute = (uint8_t)minute;
        *resultSecond = (uint8_t)second;
    }
}

bool getDate(const char *filePath, uint8_t &year, uint8_t &month, uint8_t &day, int *err) {
    if (!sd_card::isMounted(err)) {
        return false;
    }

    FileInfo fileInfo;
    if (fileInfo.fstat(filePath) != SD_FAT_RESULT_OK) {
        if (err)
            *err = SCPI_ERROR_FILE_NAME_NOT_FOUND;
        return false;
    }

    getDateTime(fileInfo, &year, &month, &day, NULL, NULL, NULL);

    return true;
}

bool getTime(const char *filePath, uint8_t &hour, uint8_t &minute, uint8_t &second, int *err) {
    if (!sd_card::isMounted(err)) {
        return false;
    }

    FileInfo fileInfo;
    if (fileInfo.fstat(filePath) != SD_FAT_RESULT_OK) {
        if (err)
            *err = SCPI_ERROR_FILE_NAME_NOT_FOUND;
        return false;
    }

    getDateTime(fileInfo, NULL, NULL, NULL, &hour, &minute, &second);

    return true;
}

bool getInfo(uint64_t &usedSpace, uint64_t &freeSpace) {
    return SD.getInfo(usedSpace, freeSpace);
}

bool confRead(uint8_t *buffer, uint16_t buffer_size, uint16_t address) {
    int err;
    if (!sd_card::isMounted(&err)) {
        return false;
    }

    File file;

    // TODO move "/.EEZCONF" to conf file
    if (!file.open("/.EEZCONF", FILE_OPEN_EXISTING | FILE_READ)) {
        return false;
    }

    if (!file.seek(address)) {
        return false;
    }

    return file.read(buffer, buffer_size) == buffer_size;
}

bool confWrite(const uint8_t *buffer, uint16_t buffer_size, uint16_t address) {
    int err;
    if (!sd_card::isMounted(&err)) {
        return false;
    }

    File file;

    if (!file.open("/.EEZCONF", FILE_OPEN_ALWAYS | FILE_WRITE)) {
        return false;
    }

    if (!file.seek(address)) {
        return false;
    }

    return file.write(buffer, buffer_size) == buffer_size;
}

} // namespace sd_card
} // namespace psu
} // namespace eez

#endif
