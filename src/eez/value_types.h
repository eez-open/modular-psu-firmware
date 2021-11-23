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

#include <eez/conf.h>
#include <eez/unit.h>

#define VALUE_TYPES \
    VALUE_TYPE(UNDEFINED) \
    VALUE_TYPE(NULL) \
    VALUE_TYPE(BOOLEAN) \
    VALUE_TYPE(INT8) \
    VALUE_TYPE(UINT8) \
    VALUE_TYPE(INT16) \
    VALUE_TYPE(UINT16) \
    VALUE_TYPE(INT32) \
    VALUE_TYPE(UINT32) \
    VALUE_TYPE(INT64) \
    VALUE_TYPE(UINT64) \
    VALUE_TYPE(FLOAT) \
	VALUE_TYPE(DOUBLE) \
    VALUE_TYPE(STRING) \
    VALUE_TYPE(ARRAY) \
	VALUE_TYPE(STRING_REF) \
	VALUE_TYPE(VERSIONED_STRING) \
	VALUE_TYPE(VALUE_PTR) \
    VALUE_TYPE(FLOW_OUTPUT) \
    VALUE_TYPE(RANGE) \
    VALUE_TYPE(POINTER) \
    VALUE_TYPE(ENUM) \
    VALUE_TYPE(YT_DATA_GET_VALUE_FUNCTION_POINTER) \
    CUSTOM_VALUE_TYPES

namespace eez {

#define VALUE_TYPE(NAME) VALUE_TYPE_##NAME,
enum ValueType {
	VALUE_TYPES
};
#undef VALUE_TYPE

}
