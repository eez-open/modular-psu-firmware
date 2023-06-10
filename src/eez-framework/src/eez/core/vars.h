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

#ifndef EEZ_FRAMEWORK_CORE_VARS_H
#define EEZ_FRAMEWORK_CORE_VARS_H

typedef enum {
    NATIVE_VAR_TYPE_NONE,
    NATIVE_VAR_TYPE_INTEGER,
    NATIVE_VAR_TYPE_BOOLEAN,
    NATIVE_VAR_TYPE_FLOAT,
    NATIVE_VAR_TYPE_DOUBLE,
    NATIVE_VAR_TYPE_STRING,
} NativeVarType;

typedef struct _native_var_t {
    NativeVarType type;
    void *get;
    void *set;
} native_var_t;

#ifdef __cplusplus
extern "C" {
#endif

extern native_var_t native_vars[];

#ifdef __cplusplus
}
#endif

#endif // EEZ_FRAMEWORK_CORE_VARS_H
