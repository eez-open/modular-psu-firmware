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

#include <eez/conf.h>

#ifndef UTF8_SUPPORT
#define UTF8_SUPPORT 1
#endif

#if UTF8_SUPPORT

#include <eez/libs/utf8.h>

#else

typedef char utf8_int8_t;
typedef int32_t utf8_int32_t;

inline const utf8_int8_t* utf8codepoint(const utf8_int8_t *str, utf8_int32_t *out_codepoint) {
    *out_codepoint = *((uint8_t *)str);
    return str + 1; 
}

#define utf8len strlen

#endif
