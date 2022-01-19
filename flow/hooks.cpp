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

#include <math.h>

#include <eez/flow/hooks.h>
#include <eez/gui/gui.h>

namespace eez {
namespace flow {

static bool isFlowRunning() {
	return true;
}

static void replacePage(int16_t pageId) {
	eez::gui::getAppContextFromId(APP_CONTEXT_ID_DEVICE)->replacePage(pageId);
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

bool (*isFlowRunningHook)() = isFlowRunning;
void (*replacePageHook)(int16_t pageId) = replacePage;
void (*showKeyboardHook)(Value label, Value initialText, Value minChars, Value maxChars, bool isPassword, void(*onOk)(char *), void(*onCancel)()) = showKeyboard;
void (*showKeypadHook)(Value label, Value initialValue, Value min, Value max, Unit unit, void(*onOk)(float), void(*onCancel)()) = showKeypad;
void (*stopScriptHook)() = stopScript;

extern void (*scpiComponentInitHook)() = scpiComponentInit;

void (*startToDebuggerMessageHook)() = startToDebuggerMessage;
void (*writeDebuggerBufferHook)(const char *buffer, uint32_t length) = writeDebuggerBuffer;
void (*finishToDebuggerMessageHook)() = finishToDebuggerMessage;
void (*onDebuggerInputAvailableHook)() = onDebuggerInputAvailable;

} // namespace flow
} // namespace eez

