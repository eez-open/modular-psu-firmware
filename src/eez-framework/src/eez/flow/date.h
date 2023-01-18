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

#pragma once

#include <stdint.h>

namespace eez {
namespace flow {
namespace date {

enum DstRule { DST_RULE_OFF, DST_RULE_EUROPE, DST_RULE_USA, DST_RULE_AUSTRALIA };
enum Format { FORMAT_DMY_24, FORMAT_MDY_24, FORMAT_DMY_12, FORMAT_MDY_12 };

typedef uint64_t Date;

extern Format g_localeFormat;
extern int g_timeZone;
extern DstRule g_dstRule;

Date now();

void toString(Date time, char *str, uint32_t strLen);
void toLocaleString(Date time, char *str, uint32_t strLen);
Date fromString(const char *str);

Date makeDate(int year, int month, int day, int hours, int minutes, int seconds, int milliseconds);

void breakDate(Date time, int &year, int &month, int &day, int &hours, int &minutes, int &seconds, int &milliseconds);

int getYear(Date time);
int getMonth(Date time);
int getDay(Date time);
int getHours(Date time);
int getMinutes(Date time);
int getSeconds(Date time);
int getMilliseconds(Date time);

Date utcToLocal(Date utc);
Date localToUtc(Date local);

} // namespace date
} // namespace flow
} // namespace eez
