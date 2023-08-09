/*
 * EEZ Modular Firmware
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

#include <bb3/flow/flow.h>

#include <eez/flow/hooks.h>
#include <eez/flow/components.h>

namespace eez {
namespace flow {

void replacePage(int16_t pageId, uint32_t animType, uint32_t speed, uint32_t delay);
void showKeyboard(Value label, Value initialText, Value minChars, Value maxChars, bool isPassword, void(*onOk)(char *), void(*onCancel)());
void showKeypad(Value label, Value initialValue, Value min, Value max, Unit unit, void(*onOk)(float), void(*onCancel)());
void stopScript();

void scpiComponentInit();
void executeScpiComponent(FlowState *flowState, unsigned componentIndex);

void startToDebuggerMessage();
void writeDebuggerBuffer(const char *buffer, uint32_t length);
void finishToDebuggerMessage();
void onDebuggerInputAvailable();

void init() {
    replacePageHook = replacePage;
    showKeyboardHook = showKeyboard;
    showKeypadHook = showKeypad;
    stopScriptHook = stopScript;

    scpiComponentInitHook = scpiComponentInit;
    registerComponent(defs_v3::COMPONENT_TYPE_SCPI_ACTION, executeScpiComponent);

    startToDebuggerMessageHook = startToDebuggerMessage;
    writeDebuggerBufferHook = writeDebuggerBuffer;
    finishToDebuggerMessageHook = finishToDebuggerMessage;
    onDebuggerInputAvailableHook = onDebuggerInputAvailable;
}

} // namespace flow
} // namespace eez
