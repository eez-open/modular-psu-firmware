/*
 * EEZ Modular Firmware
 * Copyright (C) 2020-present, Envox d.o.o.
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

#include <eez/core/unit.h>

#define VALUE_TYPES \
    VALUE_TYPE(UNDEFINED)                          /*  0 */ \
    VALUE_TYPE(NULL)                               /*  1 */ \
    VALUE_TYPE(BOOLEAN)                            /*  2 */ \
    VALUE_TYPE(INT8)                               /*  3 */ \
    VALUE_TYPE(UINT8)                              /*  4 */ \
    VALUE_TYPE(INT16)                              /*  5 */ \
    VALUE_TYPE(UINT16)                             /*  6 */ \
    VALUE_TYPE(INT32)                              /*  7 */ \
    VALUE_TYPE(UINT32)                             /*  8 */ \
    VALUE_TYPE(INT64)                              /*  9 */ \
    VALUE_TYPE(UINT64)                             /* 10 */ \
    VALUE_TYPE(FLOAT)                              /* 11 */ \
	VALUE_TYPE(DOUBLE)                             /* 12 */ \
    VALUE_TYPE(STRING)                             /* 13 */ \
    VALUE_TYPE(STRING_ASSET)                       /* 14 */ \
    VALUE_TYPE(ARRAY)                              /* 15 */ \
    VALUE_TYPE(ARRAY_ASSET)                        /* 16 */ \
	VALUE_TYPE(STRING_REF)                         /* 17 */ \
    VALUE_TYPE(ARRAY_REF)                          /* 18 */ \
    VALUE_TYPE(BLOB_REF)                           /* 19 */ \
    VALUE_TYPE(STREAM)                             /* 20 */ \
    VALUE_TYPE(DATE)                               /* 21 */ \
	VALUE_TYPE(VERSIONED_STRING)                   /* 22 */ \
	VALUE_TYPE(VALUE_PTR)                          /* 23 */ \
    VALUE_TYPE(ARRAY_ELEMENT_VALUE)                /* 24 */ \
    VALUE_TYPE(FLOW_OUTPUT)                        /* 25 */ \
    VALUE_TYPE(NATIVE_VARIABLE)                    /* 26 */ \
    VALUE_TYPE(ERROR)                              /* 27 */ \
    VALUE_TYPE(RANGE)                              /* 28 */ \
    VALUE_TYPE(POINTER)                            /* 29 */ \
    VALUE_TYPE(ENUM)                               /* 30 */ \
    VALUE_TYPE(IP_ADDRESS)                         /* 31 */ \
    VALUE_TYPE(TIME_ZONE)                          /* 32 */ \
    VALUE_TYPE(YT_DATA_GET_VALUE_FUNCTION_POINTER) /* 33 */ \
    CUSTOM_VALUE_TYPES

namespace eez {

#define VALUE_TYPE(NAME) VALUE_TYPE_##NAME,
enum ValueType {
	VALUE_TYPES
};
#undef VALUE_TYPE

}
