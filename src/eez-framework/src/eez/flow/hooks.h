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
#include <stdlib.h>

#include <eez/core/value.h>
#include <eez/flow/private.h>

#if defined(EEZ_FOR_LVGL)
#ifdef LV_LVGL_H_INCLUDE_SIMPLE
#include "lvgl.h"
#else
#include "lvgl/lvgl.h"
#endif
#endif

namespace eez {
namespace flow {

extern void (*replacePageHook)(int16_t pageId, uint32_t animType, uint32_t speed, uint32_t delay);
extern void (*showKeyboardHook)(Value label, Value initialText, Value minChars, Value maxChars, bool isPassword, void(*onOk)(char *), void(*onCancel)());
extern void (*showKeypadHook)(Value label, Value initialValue, Value min, Value max, Unit unit, void(*onOk)(float), void(*onCancel)());
extern void (*stopScriptHook)();

extern void (*scpiComponentInitHook)();

extern void (*startToDebuggerMessageHook)();
extern void (*writeDebuggerBufferHook)(const char *buffer, uint32_t length);
extern void (*finishToDebuggerMessageHook)();
extern void (*onDebuggerInputAvailableHook)();

extern void (*executeDashboardComponentHook)(uint16_t componentType, int flowStateIndex, int componentIndex);

extern void (*onArrayValueFreeHook)(ArrayValue *arrayValue);

#if defined(EEZ_FOR_LVGL)
extern lv_obj_t *(*getLvglObjectFromIndexHook)(int32_t index);
extern const void *(*getLvglImageByNameHook)(const char *name);
extern void (*executeLvglActionHook)(int actionIndex);
#endif

extern double (*getDateNowHook)();

} // flow
} // eez
