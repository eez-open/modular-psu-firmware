/*
* EEZ Generic Firmware
* Copyright (C) 2018-present, Envox d.o.o.
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

#ifndef UTF8_SUPPORT
#define UTF8_SUPPORT 1
#endif

#if UTF8_SUPPORT

#include <eez/libs/utf8.h>

#else

#include <string.h>

typedef char utf8_int8_t;
typedef int32_t utf8_int32_t;

inline const utf8_int8_t* utf8codepoint(const utf8_int8_t *str, utf8_int32_t *out_codepoint) {
    *out_codepoint = *((uint8_t *)str);
    return str + 1;
}

inline utf8_int8_t *utf8catcodepoint(utf8_int8_t *str, utf8_int32_t chr, size_t n) {
    if (n < 1) return nullptr;
    str[0] = (char)chr;
    return str + 1;
}

#define utf8len strlen
#define utf8cmp strcmp

#ifdef _MSC_VER
#define utf8casecmp _stricmp
#else
#define utf8casecmp stricmp
#endif

#endif
