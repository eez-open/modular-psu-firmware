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

#include <bb3/psu/psu.h>

#include <stdio.h>

#include <bb3/psu/datetime.h>
#include <bb3/psu/event_queue.h>
#include <bb3/psu/rtc.h>
#include <bb3/psu/scpi/psu.h>

namespace eez {
namespace psu {
namespace datetime {

#define SECONDS_PER_MINUTE 60UL
#define SECONDS_PER_HOUR (SECONDS_PER_MINUTE * 60)
#define SECONDS_PER_DAY (SECONDS_PER_HOUR * 24)

// leap year calulator expects year argument as years offset from 1970
#define LEAP_YEAR(Y)                                                                               \
    (((1970 + Y) > 0) && !((1970 + Y) % 4) && (((1970 + Y) % 100) || !((1970 + Y) % 400)))

////////////////////////////////////////////////////////////////////////////////

TestResult g_testResult = TEST_FAILED;

////////////////////////////////////////////////////////////////////////////////

// API starts months from 1, this array starts from 0
static const uint8_t monthDays[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

////////////////////////////////////////////////////////////////////////////////

enum Week { Last, First, Second, Third, Fourth };
enum DayOfWeek { Sun = 1, Mon, Tue, Wed, Thu, Fri, Sat };
enum Month { Jan = 1, Feb, Mar, Apr, May, Jun, Jul, Aug, Sep, Oct, Nov, Dec };

struct TimeChangeRule {
    Week week;
    DayOfWeek dow;
    uint8_t month;
    uint8_t hour;
};

static struct {
    TimeChangeRule dstStart;
    TimeChangeRule dstEnd;
} g_dstRules[] = {
    { { Last, Sun, Mar, 2 }, { Last, Sun, Oct, 3 } },    // EUROPE
    { { Second, Sun, Mar, 2 }, { First, Sun, Nov, 2 } }, // USA
    { { First, Sun, Oct, 2 }, { First, Sun, Apr, 3 } },  // Australia
};

////////////////////////////////////////////////////////////////////////////////

void init() {
}

int cmp_datetime(uint8_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t minute,
                 uint8_t second, uint8_t rtc_year, uint8_t rtc_month, uint8_t rtc_day,
                 uint8_t rtc_hour, uint8_t rtc_minute, uint8_t rtc_second) {
    if (year < rtc_year)
        return -1;
    if (year > rtc_year)
        return 1;

    if (month < rtc_month)
        return -1;
    if (month > rtc_month)
        return 1;

    if (day < rtc_day)
        return -1;
    if (day > rtc_day)
        return 1;

    if (hour < rtc_hour)
        return -1;
    if (hour > rtc_hour)
        return 1;

    if (minute < rtc_minute)
        return -1;
    if (minute > rtc_minute)
        return 1;

    if (second < rtc_second)
        return -1;
    if (second > rtc_second)
        return 1;

    return 0;
}

bool isValidDate(uint8_t year, uint8_t month, uint8_t day) {
    if (month < 1 || month > 12)
        return false;

    if (day < 1 || day > 31)
        return false;

    if (month == 4 || month == 6 || month == 9 || month == 11) {
        if (day > 30)
            return false;
    } else if (month == 2) {
        bool leap_year = year % 4 == 0;
        if (leap_year) {
            if (day > 29)
                return false;
        } else {
            if (day > 28)
                return false;
        }
    }

    return true;
}

bool isValidTime(uint8_t hour, uint8_t minute, uint8_t second) {
    if (hour > 23)
        return false;
    if (minute > 59)
        return false;
    if (second > 59)
        return false;
    return true;
}

bool dstCheck() {
    uint8_t year, month, day, hour, minute, second;
    rtc::readDateTime(year, month, day, hour, minute, second);
    uint32_t now = makeTime(2000 + year, month, day, hour, minute, second);

    bool dst = isDst(now, (DstRule)persist_conf::devConf.dstRule);

    if (dst != persist_conf::devConf.dst) {
        if (dst) {
            now += SECONDS_PER_HOUR;
            if (!isDst(now, (DstRule)persist_conf::devConf.dstRule)) {
                return false;
            }
        } else {
            now -= SECONDS_PER_HOUR;
        }

        int year, month, day, hour, minute, second;
        breakTime(now, year, month, day, hour, minute, second);
        setDateTime(year - 2000, month, day, hour, minute, second, false, dst ? 1 : 0);
        event_queue::pushEvent(event_queue::EVENT_INFO_SYSTEM_DATE_TIME_CHANGED_DST);

        return true;
    }

    return false;
}

bool test() {
    if (rtc::g_testResult == TEST_OK) {
        uint8_t year, month, day, hour, minute, second;
        if (persist_conf::readSystemDate(year, month, day) && persist_conf::readSystemTime(hour, minute, second)) {
            uint8_t rtc_year, rtc_month, rtc_day, rtc_hour, rtc_minute, rtc_second;
            rtc::readDateTime(rtc_year, rtc_month, rtc_day, rtc_hour, rtc_minute, rtc_second);

            if (!isValidDate(2000 + rtc_year, rtc_month, rtc_day)) {
                g_testResult = TEST_FAILED;
                DebugTrace("RTC test failed, invalid date format detected (%d-%02d-%02d)\n",
                            (int)(2000 + rtc_year), (int)rtc_month, (int)rtc_day);
            } else if (!isValidTime(rtc_hour, rtc_minute, rtc_second)) {
                g_testResult = TEST_FAILED;
                DebugTrace("RTC test failed, invalid time format detected (%02d:%02d:%02d)\n",
                            (int)rtc_hour, (int)rtc_minute, (int)rtc_second);
            } else if (cmp_datetime(year, month, day, hour, minute, second, rtc_year, rtc_month, rtc_day, rtc_hour, rtc_minute, rtc_second) <= 0) {
                g_testResult = TEST_OK;
                if (!dstCheck()) {
                    persist_conf::writeSystemDateTime(rtc_year, rtc_month, rtc_day, rtc_hour,
                                                      rtc_minute, rtc_second, 2);
                }
            } else {
                g_testResult = TEST_FAILED;
                DebugTrace("RTC test failed, RTC time (%d-%02d-%02d %02d:%02d:%02d) older then EEPROM time (%d-%02d-%02d %02d:%02d:%02d)\n",
                            (int)(2000 + rtc_year), (int)rtc_month, (int)rtc_day, (int)rtc_hour, (int)rtc_minute, (int)rtc_second,
							(int)(2000 + year), (int)month, (int)day, (int)hour, (int)minute, (int)second);
            }
        } else {
            g_testResult = TEST_SKIPPED;
        }
    } else {
        g_testResult = TEST_SKIPPED;
    }

    if (g_testResult == TEST_FAILED) {
        generateError(SCPI_ERROR_DATETIME_TEST_FAILED);
    }

    setQuesBits(QUES_TIME, g_testResult != TEST_OK);

    return g_testResult != TEST_FAILED && g_testResult != TEST_WARNING;
}

void tick() {
    static uint32_t g_lastTickCountMs;
	uint32_t tickCountMs = millis();
    int32_t diff = tickCountMs - g_lastTickCountMs;
    if (diff > 1000L) {
        dstCheck();
        g_lastTickCountMs = tickCountMs;
    }
}

bool getDate(uint8_t &year, uint8_t &month, uint8_t &day) {
    return rtc::readDate(year, month, day);
}

bool checkDateTime() {
    uint8_t year, month, day, hour, minute, second;
    if (persist_conf::readSystemDate(year, month, day) &&
        persist_conf::readSystemTime(hour, minute, second)) {
        uint8_t rtc_year, rtc_month, rtc_day, rtc_hour, rtc_minute, rtc_second;
        rtc::readDateTime(rtc_year, rtc_month, rtc_day, rtc_hour, rtc_minute, rtc_second);
        return cmp_datetime(year, month, day, hour, minute, second, rtc_year, rtc_month, rtc_day,
                            rtc_hour, rtc_minute, rtc_second) <= 0;
    }
    return false;
}

bool setDate(uint8_t year, uint8_t month, uint8_t day, unsigned dst) {
    if (rtc::writeDate(year, month, day)) {
        persist_conf::writeSystemDate(year, month, day, dst);
        setQuesBits(QUES_TIME, !checkDateTime());
        event_queue::pushEvent(event_queue::EVENT_INFO_SYSTEM_DATE_TIME_CHANGED);
        return true;
    }
    return false;
}

bool getTime(uint8_t &hour, uint8_t &minute, uint8_t &second) {
    return rtc::readTime(hour, minute, second);
}

bool setTime(uint8_t hour, uint8_t minute, uint8_t second, unsigned dst) {
    if (rtc::writeTime(hour, minute, second)) {
        persist_conf::writeSystemTime(hour, minute, second, dst);
        setQuesBits(QUES_TIME, !checkDateTime());
        event_queue::pushEvent(event_queue::EVENT_INFO_SYSTEM_DATE_TIME_CHANGED);
        return true;
    }
    return false;
}

bool getDateTime(uint8_t &year, uint8_t &month, uint8_t &day, uint8_t &hour, uint8_t &minute, uint8_t &second) {
    return rtc::readDateTime(year, month, day, hour, minute, second);
}

bool setDateTime(uint8_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t minute,
                 uint8_t second, bool pushChangedEvent, unsigned dst) {
    if (rtc::writeDateTime(year, month, day, hour, minute, second)) {
        persist_conf::writeSystemDateTime(year, month, day, hour, minute, second, dst);
        setQuesBits(QUES_TIME, !checkDateTime());
        if (pushChangedEvent) {
            event_queue::pushEvent(event_queue::EVENT_INFO_SYSTEM_DATE_TIME_CHANGED);
        }
        return true;
    }
    return false;
}

void convertTime24to12(int &hour, bool &am) {
    if (hour == 0) {
        hour = 12;
        am = true;
    } else if (hour < 12) {
        am = true;
    } else if (hour == 12) {
        am = false;
    } else {
        hour = hour - 12;
        am = false;
    }
}

void convertTime24to12(uint8_t &hour, bool &am) {
    int h = hour;
    convertTime24to12(h, am);
    hour = h;
}

void convertTime12to24(int &hour, bool am) {
    if (am) {
        if (hour == 12) {
            hour = 0;
        }
    } else {
        if (hour < 12) {
            hour += 12;
        }
    }
}

void convertTime12to24(uint8_t &hour, bool am) {
    int h = hour;
    convertTime12to24(h, am);
    hour = h;
}

bool getDateTimeAsString(char *buffer) {
    uint8_t year, month, day, hour, minute, second;
    if (datetime::getDateTime(year, month, day, hour, minute, second)) {
        if (persist_conf::devConf.dateTimeFormat == FORMAT_DMY_24) {
            sprintf(buffer, "%02d-%02d-%02d %02d:%02d:%02d", (int)day, (int)month, (int)year, (int)hour, (int)minute, (int)second);
        } else if (persist_conf::devConf.dateTimeFormat == FORMAT_MDY_24) {
            sprintf(buffer, "%02d-%02d-%02d %02d:%02d:%02d", (int)month, (int)day, (int)year, (int)hour, (int)minute, (int)second);
        } else if (persist_conf::devConf.dateTimeFormat == FORMAT_DMY_12) {
            bool am;
            convertTime24to12(hour, am);
            sprintf(buffer, "%02d-%02d-%02d %02d:%02d:%02d %s", (int)day, (int)month, (int)year, (int)hour, (int)minute, (int)second, am ? "AM" : "PM");
        } else if (persist_conf::devConf.dateTimeFormat == FORMAT_MDY_12) {
            bool am;
            convertTime24to12(hour, am);
            sprintf(buffer, "%02d-%02d-%02d %02d:%02d:%02d %s", (int)month, (int)day, (int)year, (int)hour, (int)minute, (int)second, am ? "AM" : "PM");
        }
        return true;
    }
    return false;
}

uint32_t now() {
    uint8_t year, month, day, hour, minute, second;
    rtc::readDateTime(year, month, day, hour, minute, second);
    return makeTime(2000 + year, month, day, hour, minute, second);
}

uint32_t nowUtc() {
    uint8_t year, month, day, hour, minute, second;
    rtc::readDateTime(year, month, day, hour, minute, second);
    uint32_t now = makeTime(2000 + year, month, day, hour, minute, second);
    return localToUtc(now, persist_conf::devConf.timeZone, (DstRule)persist_conf::devConf.dstRule);
}

uint32_t makeTime(int year, int month, int day, int hour, int minute, int second) {
    // seconds from 1970 till 1 jan 00:00:00 of the given year
    year -= 1970;

    uint32_t seconds = year * 365 * SECONDS_PER_DAY;

    for (int i = 0; i < year; i++) {
        if (LEAP_YEAR(i)) {
            seconds += SECONDS_PER_DAY; // add extra days for leap years
        }
    }

    // add days for this year, months start from 1
    for (int i = 1; i < month; i++) {
        if ((i == 2) && LEAP_YEAR(year)) {
            seconds += SECONDS_PER_DAY * 29;
        } else {
            seconds += SECONDS_PER_DAY * monthDays[i - 1]; // monthDay array starts from 0
        }
    }
    seconds += (day - 1) * SECONDS_PER_DAY;
    seconds += hour * SECONDS_PER_HOUR;
    seconds += minute * SECONDS_PER_MINUTE;
    seconds += second;

    return seconds;
}

void breakTime(uint32_t time, int &resultYear, int &resultMonth, int &resultDay, int &resultHour,
               int &resultMinute, int &resultSecond) {
    // break the given time_t into time components
    uint8_t year;
    uint8_t month, monthLength;
    uint32_t days;

    resultSecond = time % 60;
    time /= 60; // now it is minutes

    resultMinute = time % 60;
    time /= 60; // now it is hours

    resultHour = time % 24;
    time /= 24; // now it is days

    year = 0;
    days = 0;
    while ((unsigned)(days += (LEAP_YEAR(year) ? 366 : 365)) <= time) {
        year++;
    }
    resultYear = year + 1970; // year is offset from 1970

    days -= LEAP_YEAR(year) ? 366 : 365;
    time -= days; // now it is days in this year, starting at 0

    days = 0;
    month = 0;
    monthLength = 0;
    for (month = 0; month < 12; ++month) {
        if (month == 1) { // february
            if (LEAP_YEAR(year)) {
                monthLength = 29;
            } else {
                monthLength = 28;
            }
        } else {
            monthLength = monthDays[month];
        }

        if (time >= monthLength) {
            time -= monthLength;
        } else {
            break;
        }
    }

    resultMonth = month + 1; // jan is month 1
    resultDay = time + 1;    // day of month
}

uint8_t dayOfWeek(int y, int m, int d) {
    static int t[] = { 0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4 };
    if (m < 3) {
        --y;
    }
    return (y + y / 4 - y / 100 + y / 400 + t[m - 1] + d) % 7 + 1;
}

uint32_t timeChangeRuleToLocal(TimeChangeRule &r, int year) {
    uint8_t month = r.month;
    uint8_t week = r.week;
    if (week == 0) {        // Last week == 0
        if (++month > 12) { // for "Last", go to the next month
            month = 1;
            ++year;
        }
        week = 1; // and treat as first week of next month, subtract 7 days later
    }

    uint32_t t = makeTime(year, month, 1, r.hour, 0, 0);

    uint8_t dow = dayOfWeek(year, month, 1);

    t += (7 * (week - 1) + (r.dow - dow + 7) % 7) * SECONDS_PER_DAY;
    if (r.week == 0) {
        t -= 7 * SECONDS_PER_DAY; // back up a week if this is a "Last" rule
    }

    return t;
}

bool isDst(uint32_t local, DstRule dstRule) {
    if (dstRule == DST_RULE_OFF) {
        return false;
    }

    int year, month, day, hour, minute, second;
    breakTime(local, year, month, day, hour, minute, second);

    uint32_t dstStart = timeChangeRuleToLocal(g_dstRules[dstRule - 1].dstStart, year);
    uint32_t dstEnd = timeChangeRuleToLocal(g_dstRules[dstRule - 1].dstEnd, year);

    return (dstStart < dstEnd && (local >= dstStart && local < dstEnd)) ||
           (dstStart > dstEnd && (local >= dstStart || local < dstEnd));
}

uint32_t utcToLocal(uint32_t utc, int16_t timeZone, DstRule dstRule) {
    uint32_t local = utc + ((timeZone / 100) * 60 + timeZone % 100) * 60L;
    if (isDst(local, dstRule)) {
        local += SECONDS_PER_HOUR;
    }
    return local;
}

uint32_t localToUtc(uint32_t local, int16_t timeZone, DstRule dstRule) {
    uint32_t utc = local - ((timeZone / 100) * 60 + timeZone % 100) * 60L;
    if (isDst(local, dstRule)) {
        utc -= SECONDS_PER_HOUR;
    }
    return utc;
}

////////////////////////////////////////////////////////////////////////////////

DateTime::DateTime() {
}

DateTime::DateTime(uint16_t year_, uint8_t month_, uint8_t day_, uint8_t hour_, uint8_t minute_,
                   uint8_t second_)
    : year(year_), month(month_), day(day_), hour(hour_), minute(minute_), second(second_) {
}

DateTime::DateTime(const DateTime &rhs) {
    memcpy(this, &rhs, sizeof(DateTime));
}

DateTime DateTime::now() {
    uint8_t year, month, day, hour, minute, second;

    if (!getDateTime(year, month, day, hour, minute, second)) {
        year = 16;
        month = 10;
        day = 1;
        hour = 0;
        minute = 0;
        second = 0;
    }

    return DateTime(2000 + year, month, day, hour, minute, second);
}

bool DateTime::operator==(const DateTime &rhs) {
    return memcmp(this, &rhs, sizeof(DateTime)) == 0;
}

bool DateTime::operator!=(const DateTime &rhs) {
    return memcmp(this, &rhs, sizeof(DateTime)) != 0;
}

} // namespace datetime
} // namespace psu
} // namespace eez
