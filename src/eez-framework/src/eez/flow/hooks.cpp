/*
 * EEZ Modular Firmware
 * Copyright (C) 2021-present, Envox d.o.o.
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

#include <eez/conf-internal.h>

#include <assert.h>
#include <math.h>

#include <eez/flow/hooks.h>

#include <eez/core/util.h>

#if EEZ_OPTION_GUI
#include <eez/gui/gui.h>
#endif

#include <chrono>

namespace eez {
namespace flow {

static void replacePage(int16_t pageId, uint32_t animType, uint32_t speed, uint32_t delay) {
#if EEZ_OPTION_GUI
	eez::gui::getAppContextFromId(APP_CONTEXT_ID_DEVICE)->replacePage(pageId);
#endif
}

static void showKeyboard(Value label, Value initialText, Value minChars, Value maxChars, bool isPassword, void(*onOk)(char *), void(*onCancel)()) {
}

static void showKeypad(Value label, Value initialValue, Value min, Value max, Unit unit, void(*onOk)(float), void(*onCancel)()) {
}

static void stopScript() {
	assert(false);
}

static void scpiComponentInit() {
}

static void startToDebuggerMessage() {
}

static void writeDebuggerBuffer(const char *buffer, uint32_t length) {
}

static void finishToDebuggerMessage() {
}

static void onDebuggerInputAvailable() {
}

void (*replacePageHook)(int16_t pageId, uint32_t animType, uint32_t speed, uint32_t delay) = replacePage;
void (*showKeyboardHook)(Value label, Value initialText, Value minChars, Value maxChars, bool isPassword, void(*onOk)(char *), void(*onCancel)()) = showKeyboard;
void (*showKeypadHook)(Value label, Value initialValue, Value min, Value max, Unit unit, void(*onOk)(float), void(*onCancel)()) = showKeypad;
void (*stopScriptHook)() = stopScript;

void (*scpiComponentInitHook)() = scpiComponentInit;

void (*startToDebuggerMessageHook)() = startToDebuggerMessage;
void (*writeDebuggerBufferHook)(const char *buffer, uint32_t length) = writeDebuggerBuffer;
void (*finishToDebuggerMessageHook)() = finishToDebuggerMessage;
void (*onDebuggerInputAvailableHook)() = onDebuggerInputAvailable;

void (*executeDashboardComponentHook)(uint16_t componentType, int flowStateIndex, int componentIndex) = nullptr;

void (*onArrayValueFreeHook)(ArrayValue *arrayValue) = nullptr;

#if defined(EEZ_FOR_LVGL)
static lv_obj_t *getLvglObjectFromIndex(int32_t index) {
    return 0;
}

static const void *getLvglImageByName(const char *name) {
    return 0;
}

static void executeLvglAction(int actionIndex) {
}

lv_obj_t *(*getLvglObjectFromIndexHook)(int32_t index) = getLvglObjectFromIndex;
const void *(*getLvglImageByNameHook)(const char *name) = getLvglImageByName;
void (*executeLvglActionHook)(int actionIndex) = executeLvglAction;
#endif

double getDateNowDefaultImplementation() {
    using namespace std::chrono;
    milliseconds ms = duration_cast<milliseconds>(system_clock::now().time_since_epoch());
    return (double)ms.count();
}

double (*getDateNowHook)() = getDateNowDefaultImplementation;

} // namespace flow
} // namespace eez

