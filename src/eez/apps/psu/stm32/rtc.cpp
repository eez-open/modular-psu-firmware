/*
 * EEZ PSU Firmware
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

#include <rtc.h>

#include <eez/apps/psu/psu.h>

#include <scpi/scpi.h>

#include <eez/apps/psu/rtc.h>

namespace eez {
namespace psu {
namespace rtc {

TestResult g_testResult = TEST_FAILED;

////////////////////////////////////////////////////////////////////////////////

void init() {
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

bool readDate(uint8_t &year, uint8_t &month, uint8_t &day) {
    if (g_testResult != TEST_OK) {
        return false;
    }

    RTC_DateTypeDef date;
    HAL_RTC_GetDate(&hrtc, &date, FORMAT_BIN);
    
    day = date.Date;
    month = date.Month;
    year = date.Year;

    return true;
}

bool writeDate(uint8_t year, uint8_t month, uint8_t day) {
    if (g_testResult != TEST_OK) {
        return false;
    }

	RTC_DateTypeDef date;

	date.Year = year;
	date.Month = month;
	date.Date = day;
	
    // TODO ???
    date.WeekDay = RTC_WEEKDAY_MONDAY;

	HAL_RTC_SetDate(&hrtc, &date, FORMAT_BIN);

    return true;
}

bool readTime(uint8_t &hour, uint8_t &minute, uint8_t &second) {
    if (g_testResult != TEST_OK) {
        return false;
    }

	RTC_TimeTypeDef time;
    HAL_RTC_GetTime(&hrtc, &time, FORMAT_BIN);

    second = time.Seconds;
    minute = time.Minutes;
    hour = time.Hours;

    return true;
}

bool writeTime(uint8_t hour, uint8_t minute, uint8_t second) {
    if (g_testResult != TEST_OK) {
        return false;
    }

	RTC_TimeTypeDef time;

	time.Hours = hour;
	time.Minutes = minute;
	time.Seconds = second;

	time.SubSeconds = 0;
	time.TimeFormat = RTC_HOURFORMAT_24;
	time.DayLightSaving = RTC_DAYLIGHTSAVING_NONE;
	time.StoreOperation = RTC_STOREOPERATION_RESET;

	HAL_RTC_SetTime(&hrtc, &time, FORMAT_BIN);

    return true;
}

bool readDateTime(uint8_t &year, uint8_t &month, uint8_t &day, uint8_t &hour, uint8_t &minute, uint8_t &second) {
    if (g_testResult != TEST_OK) {
        return false;
    }

    readDate(year, month, day);
    readTime(hour, minute, second);

    return true;
}

bool writeDateTime(uint8_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t minute, uint8_t second) {
    if (g_testResult != TEST_OK) {
        return false;
    }

    writeDate(year, month, day);
    writeTime(hour, minute, second);

    return true;
}

} // namespace rtc
} // namespace psu
} // namespace eez
