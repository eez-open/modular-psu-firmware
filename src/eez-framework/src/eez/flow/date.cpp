/*
* EEZ Generic Firmware
* Copyright (C) 2022-present, Envox d.o.o.
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

#include <stdio.h>

#include <eez/core/os.h>

#include <eez/flow/date.h>
#include <eez/flow/hooks.h>

namespace eez {
namespace flow {
namespace date {

#define SECONDS_PER_MINUTE 60UL
#define SECONDS_PER_HOUR (SECONDS_PER_MINUTE * 60)
#define SECONDS_PER_DAY (SECONDS_PER_HOUR * 24)

// leap year calulator expects year argument as years offset from 1970
#define LEAP_YEAR(Y)                                                                               \
    (((1970 + Y) > 0) && !((1970 + Y) % 4) && (((1970 + Y) % 100) || !((1970 + Y) % 400)))

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
    uint8_t hours;
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

Format g_localeFormat = FORMAT_DMY_24;
int g_timeZone = 100;
DstRule g_dstRule = DST_RULE_EUROPE;

//
// PRIVATE function declarations
//

static void convertTime24to12(int &hours, bool &am);
//static void convertTime12to24(int &hours, bool am);
static bool isDst(Date time, DstRule dstRule);
static uint8_t dayOfWeek(int y, int m, int d);
static Date timeChangeRuleToLocal(TimeChangeRule &r, int year);

////////////////////////////////////////////////////////////////////////////////

Date now() {
    return utcToLocal(getDateNowHook());
}

void toString(Date time, char *str, uint32_t strLen) {
    int year, month, day, hours, minutes, seconds, milliseconds;
    breakDate(time, year, month, day, hours, minutes, seconds, milliseconds);
    snprintf(str, strLen, "%04d-%02d-%02dT%02d:%02d:%02d.%06d", year, month, day, hours, minutes, seconds, milliseconds);
}

void toLocaleString(Date time, char *str, uint32_t strLen) {
    int year, month, day, hours, minutes, seconds, milliseconds;
    breakDate(time, year, month, day, hours, minutes, seconds, milliseconds);
    if (g_localeFormat == FORMAT_DMY_24) {
        snprintf(str, strLen, "%02d-%02d-%02d %02d:%02d:%02d.%03d", day, month, year, hours, minutes, seconds, milliseconds);
    } else if (g_localeFormat == FORMAT_MDY_24) {
        snprintf(str, strLen, "%02d-%02d-%02d %02d:%02d:%02d.%03d", month, day, year, hours, minutes, seconds, milliseconds);
    } else if (g_localeFormat == FORMAT_DMY_12) {
        bool am;
        convertTime24to12(hours, am);
        snprintf(str, strLen, "%02d-%02d-%02d %02d:%02d:%02d.%03d %s", day, month, year, hours, minutes, seconds, milliseconds, am ? "AM" : "PM");
    } else if (g_localeFormat == FORMAT_MDY_12) {
        bool am;
        convertTime24to12(hours, am);
        snprintf(str, strLen, "%02d-%02d-%02d %02d:%02d:%02d.%03d %s", month, day, year, hours, minutes, seconds, milliseconds, am ? "AM" : "PM");
    }
}

Date fromString(const char *str) {
    int year = 0, month = 0, day = 0, hours = 0, minutes = 0, seconds = 0, milliseconds = 0;
    sscanf(str, "%d-%d-%dT%d:%d:%d.%d", &year, &month, &day, &hours, &minutes, &seconds, &milliseconds);
    return makeDate(year, month, day, hours, minutes, seconds, milliseconds);
}

Date makeDate(int year, int month, int day, int hours, int minutes, int seconds, int milliseconds) {
    // seconds from 1970 till 1 jan 00:00:00 of the given year
    year -= 1970;

    Date time = year * 365 * SECONDS_PER_DAY;

    for (int i = 0; i < year; i++) {
        if (LEAP_YEAR(i)) {
            time += SECONDS_PER_DAY; // add extra days for leap years
        }
    }

    // add days for this year, months start from 1
    for (int i = 1; i < month; i++) {
        if ((i == 2) && LEAP_YEAR(year)) {
            time += SECONDS_PER_DAY * 29;
        } else {
            time += SECONDS_PER_DAY * monthDays[i - 1]; // monthDay array starts from 0
        }
    }
    time += (day - 1) * SECONDS_PER_DAY;
    time += hours * SECONDS_PER_HOUR;
    time += minutes * SECONDS_PER_MINUTE;
    time += seconds;

    time *= 1000;

    time += milliseconds;

    return time;
}

void breakDate(Date time, int &result_year, int &result_month, int &result_day, int &result_hours, int &result_minutes, int &result_seconds, int &result_milliseconds) {
    // break the given time_t into time components
    uint8_t year;
    uint8_t month, monthLength;
    uint32_t days;

    result_milliseconds = time % 1000;
    time /= 1000; // now it is seconds

    result_seconds = time % 60;
    time /= 60; // now it is minutes

    result_minutes = time % 60;
    time /= 60; // now it is hours

    result_hours = time % 24;
    time /= 24; // now it is days

    year = 0;
    days = 0;
    while ((unsigned)(days += (LEAP_YEAR(year) ? 366 : 365)) <= time) {
        year++;
    }
    result_year = year + 1970; // year is offset from 1970

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

    result_month = month + 1; // jan is month 1
    result_day = time + 1;    // day of month
}

int getYear(Date time) {
    int year, month, day, hours, minutes, seconds, milliseconds;
    breakDate(time, year, month, day, hours, minutes, seconds, milliseconds);
    return year;
}

int getMonth(Date time) {
    int year, month, day, hours, minutes, seconds, milliseconds;
    breakDate(time, year, month, day, hours, minutes, seconds, milliseconds);
    return month;
}

int getDay(Date time) {
    int year, month, day, hours, minutes, seconds, milliseconds;
    breakDate(time, year, month, day, hours, minutes, seconds, milliseconds);
    return day;
}

int getHours(Date time) {
    int year, month, day, hours, minutes, seconds, milliseconds;
    breakDate(time, year, month, day, hours, minutes, seconds, milliseconds);
    return hours;
}

int getMinutes(Date time) {
    int year, month, day, hours, minutes, seconds, milliseconds;
    breakDate(time, year, month, day, hours, minutes, seconds, milliseconds);
    return minutes;
}

int getSeconds(Date time) {
    int year, month, day, hours, minutes, seconds, milliseconds;
    breakDate(time, year, month, day, hours, minutes, seconds, milliseconds);
    return seconds;
}

int getMilliseconds(Date time) {
    int year, month, day, hours, minutes, seconds, milliseconds;
    breakDate(time, year, month, day, hours, minutes, seconds, milliseconds);
    return milliseconds;
}

Date utcToLocal(Date utc) {
    Date local = utc + ((g_timeZone / 100) * 60 + g_timeZone % 100) * 60L;
    if (isDst(local, g_dstRule)) {
        local += SECONDS_PER_HOUR;
    }
    return local;
}

Date localToUtc(Date local) {
    Date utc = local - ((g_timeZone / 100) * 60 + g_timeZone % 100) * 60L;
    if (isDst(local, g_dstRule)) {
        utc -= SECONDS_PER_HOUR;
    }
    return utc;
}

////////////////////////////////////////////////////////////////////////////////
//
// PRIVATE function definitions
//

static void convertTime24to12(int &hours, bool &am) {
    if (hours == 0) {
        hours = 12;
        am = true;
    } else if (hours < 12) {
        am = true;
    } else if (hours == 12) {
        am = false;
    } else {
        hours = hours - 12;
        am = false;
    }
}

//static void convertTime12to24(int &hours, bool am) {
//    if (am) {
//        if (hours == 12) {
//            hours = 0;
//        }
//    } else {
//        if (hours < 12) {
//            hours += 12;
//        }
//    }
//}

static bool isDst(Date local, DstRule dstRule) {
    if (dstRule == DST_RULE_OFF) {
        return false;
    }

    int year, month, day, hours, minutes, seconds, milliseconds;
    breakDate(local, year, month, day, hours, minutes, seconds, milliseconds);

    Date dstStart = timeChangeRuleToLocal(g_dstRules[dstRule - 1].dstStart, year);
    Date dstEnd = timeChangeRuleToLocal(g_dstRules[dstRule - 1].dstEnd, year);

    return (dstStart < dstEnd && (local >= dstStart && local < dstEnd)) ||
           (dstStart > dstEnd && (local >= dstStart || local < dstEnd));
}

static uint8_t dayOfWeek(int y, int m, int d) {
    static int t[] = { 0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4 };
    if (m < 3) {
        --y;
    }
    return (y + y / 4 - y / 100 + y / 400 + t[m - 1] + d) % 7 + 1;
}

static Date timeChangeRuleToLocal(TimeChangeRule &r, int year) {
    uint8_t month = r.month;
    uint8_t week = r.week;
    if (week == 0) {        // Last week == 0
        if (++month > 12) { // for "Last", go to the next month
            month = 1;
            ++year;
        }
        week = 1; // and treat as first week of next month, subtract 7 days later
    }

    Date time = makeDate(year, month, 1, r.hours, 0, 0, 0);

    uint8_t dow = dayOfWeek(year, month, 1);

    time += (7 * (week - 1) + (r.dow - dow + 7) % 7) * SECONDS_PER_DAY;
    if (r.week == 0) {
        time -= 7 * SECONDS_PER_DAY; // back up a week if this is a "Last" rule
    }

    return time;
}

} // namespace date
} // namespace flow
} // namespace eez
