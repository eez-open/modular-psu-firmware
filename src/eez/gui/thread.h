/*
* EEZ Generic Firmware
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
* along with this program.  If not, see http://www.gnu.org/licenses.
*/

#pragma once

#include <eez/os.h>

namespace eez {
namespace gui {

void startThread();

enum {
    GUI_QUEUE_MESSAGE_TYPE_SHOW_PAGE = 1,
    GUI_QUEUE_MESSAGE_TYPE_PUSH_PAGE,

    GUI_QUEUE_MESSAGE_MOUSE_X_MOVE,
    GUI_QUEUE_MESSAGE_MOUSE_Y_MOVE,
    GUI_QUEUE_MESSAGE_MOUSE_BUTTON_DOWN,
    GUI_QUEUE_MESSAGE_MOUSE_BUTTON_UP,
    GUI_QUEUE_MESSAGE_MOUSE_DISCONNECTED,

    GUI_QUEUE_MESSAGE_REFRESH_SCREEN,

    GUI_QUEUE_MESSAGE_FLOW_START,
    GUI_QUEUE_MESSAGE_FLOW_STOP,

    GUI_QUEUE_MESSAGE_UNLOAD_EXTERNAL_ASSETS,

    GUI_QUEUE_MESSAGE_DEBUGGER_CLIENT_CONNECTED,
    GUI_QUEUE_MESSAGE_DEBUGGER_CLIENT_DISCONNECTED,
    GUI_QUEUE_MESSAGE_DEBUGGER_INPUT_AVAILABLE,

    GUI_QUEUE_MESSAGE_KEY_DOWN,

};

class AppContext;
class Page;

void sendMessageToGuiThread(uint8_t messageType, uint32_t messageParam = 0, uint32_t timeoutMillisec = osWaitForever);
bool pushPageInGuiThread(AppContext *appContext, int pageId, Page *page);
bool showPageInGuiThread(AppContext *appContext, int pageId);

void onGuiQueueMessageHook(uint8_t type, int16_t param);

bool isGuiThread();

void processGuiQueue(uint32_t timeoutMillisec);

} // namespace gui
} // namespace eez
