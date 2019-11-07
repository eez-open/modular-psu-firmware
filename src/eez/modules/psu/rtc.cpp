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
#include <rtc.h>
#endif

#if defined(EEZ_PLATFORM_SIMULATOR)
#include <stdio.h>
#include <time.h>
#endif

#include <eez/modules/psu/psu.h>
#include <eez/modules/psu/datetime.h>
#include <eez/modules/psu/rtc.h>

#include <scpi/scpi.h>

namespace eez {
namespace psu {
namespace rtc {

TestResult g_testResult = TEST_FAILED;

#if defined(EEZ_PLATFORM_SIMULATOR)
static uint32_t g_offset;

void setOffset(uint32_t offset) {
    g_offset = offset;
    char *file_path = getConfFilePath("RTC.state");
    FILE *fp = fopen(file_path, "wb");
    if (fp != NULL) {
        fseek(fp, 0, SEEK_SET);
        fwrite(&g_offset, sizeof(g_offset), 1, fp);
        fclose(fp);
    }
}
#endif

////////////////////////////////////////////////////////////////////////////////

void init() {
#if defined(EEZ_PLATFORM_SIMULATOR)
    char *file_path = getConfFilePath("RTC.state");
    FILE *fp = fopen(file_path, "r+b");
    if (fp != NULL) {
        fread(&g_offset, sizeof(g_offset), 1, fp);
        fclose(fp);
    } else {
        setOffset(0);
    }
#endif
}

bool test() {
#if OPTION_EXT_RTC
    g_testResult = TEST_OK;
#else
    g_testResult = TEST_SKIPPED;
#endif

    if (g_testResult == TEST_FAILED) {
        generateError(SCPI_ERROR_RTC_TEST_FAILED);
    }

    return g_testResult != TEST_FAILED;
}

#if defined(EEZ_PLATFORM_SIMULATOR)
uint32_t nowUtc() {
    time_t now_time_t = time(0);
    struct tm *now_tm = gmtime(&now_time_t);
    return datetime::makeTime(1900 + now_tm->tm_year, now_tm->tm_mon + 1, now_tm->tm_mday,
                              now_tm->tm_hour, now_tm->tm_min, now_tm->tm_sec);
}
#endif

bool readDate(uint8_t &year, uint8_t &month, uint8_t &day) {
    if (g_testResult != TEST_OK) {
        return false;
    }

#if defined(EEZ_PLATFORM_STM32)
    RTC_DateTypeDef date;
    HAL_RTC_GetDate(&hrtc, &date, FORMAT_BIN);
    
    day = date.Date;
    month = date.Month;
    year = date.Year;
#endif

#if defined(EEZ_PLATFORM_SIMULATOR)
    int year_, month_, day_, hour_, minute_, second_;
    datetime::breakTime(g_offset + nowUtc(), year_, month_, day_, hour_, minute_, second_);

    year = uint8_t(year_ - 2000);
    month = uint8_t(month_);
    day = uint8_t(day_);
#endif

    return true;
}

bool writeDate(uint8_t year, uint8_t month, uint8_t day) {
    if (g_testResult != TEST_OK) {
        return false;
    }

#if defined(EEZ_PLATFORM_STM32)
	RTC_DateTypeDef date;

	date.Year = year;
	date.Month = month;
	date.Date = day;
	
    // TODO ???
    date.WeekDay = RTC_WEEKDAY_MONDAY;

	HAL_RTC_SetDate(&hrtc, &date, FORMAT_BIN);
#endif

#if defined(EEZ_PLATFORM_SIMULATOR)
    int year_, month_, day_, hour_, minute_, second_;
    datetime::breakTime(g_offset + nowUtc(), year_, month_, day_, hour_, minute_, second_);
    setOffset(datetime::makeTime(year + 2000, month, day, hour_, minute_, second_) - nowUtc());
#endif

    return true;
}

bool readTime(uint8_t &hour, uint8_t &minute, uint8_t &second) {
    if (g_testResult != TEST_OK) {
        return false;
    }

#if defined(EEZ_PLATFORM_STM32)
	RTC_TimeTypeDef time;
    HAL_RTC_GetTime(&hrtc, &time, FORMAT_BIN);

    second = time.Seconds;
    minute = time.Minutes;
    hour = time.Hours;
#endif

#if defined(EEZ_PLATFORM_SIMULATOR)
    int year_, month_, day_, hour_, minute_, second_;
    datetime::breakTime(g_offset + nowUtc(), year_, month_, day_, hour_, minute_, second_);

    hour = uint8_t(hour_);
    minute = uint8_t(minute_);
    second = uint8_t(second_);
#endif

    return true;
}

bool writeTime(uint8_t hour, uint8_t minute, uint8_t second) {
    if (g_testResult != TEST_OK) {
        return false;
    }

#if defined(EEZ_PLATFORM_STM32)
	RTC_TimeTypeDef time;

	time.Hours = hour;
	time.Minutes = minute;
	time.Seconds = second;

	time.SubSeconds = 0;
	time.TimeFormat = RTC_HOURFORMAT_24;
	time.DayLightSaving = RTC_DAYLIGHTSAVING_NONE;
	time.StoreOperation = RTC_STOREOPERATION_RESET;

	HAL_RTC_SetTime(&hrtc, &time, FORMAT_BIN);
#endif

#if defined(EEZ_PLATFORM_SIMULATOR)
    int year_, month_, day_, hour_, minute_, second_;
    datetime::breakTime(g_offset + nowUtc(), year_, month_, day_, hour_, minute_, second_);
    setOffset(datetime::makeTime(year_, month_, day_, hour, minute, second) - nowUtc());
#endif

    return true;
}

bool readDateTime(uint8_t &year, uint8_t &month, uint8_t &day, uint8_t &hour, uint8_t &minute, uint8_t &second) {
    if (g_testResult != TEST_OK) {
        return false;
    }

#if defined(EEZ_PLATFORM_STM32)
    readDate(year, month, day);
    readTime(hour, minute, second);
#endif

#if defined(EEZ_PLATFORM_SIMULATOR)
    int year_, month_, day_, hour_, minute_, second_;
    datetime::breakTime(g_offset + nowUtc(), year_, month_, day_, hour_, minute_, second_);

    year = uint8_t(year_ - 2000);
    month = uint8_t(month_);
    day = uint8_t(day_);

    hour = uint8_t(hour_);
    minute = uint8_t(minute_);
    second = uint8_t(second_);
#endif

    return true;
}

bool writeDateTime(uint8_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t minute, uint8_t second) {
    if (g_testResult != TEST_OK) {
        return false;
    }

#if defined(EEZ_PLATFORM_STM32)
    writeDate(year, month, day);
    writeTime(hour, minute, second);
#endif

#if defined(EEZ_PLATFORM_SIMULATOR)
    setOffset(datetime::makeTime(year + 2000, month, day, hour, minute, second) - nowUtc());
#endif

    return true;
}

} // namespace rtc
} // namespace psu
} // namespace eez
