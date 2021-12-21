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

#include <eez/os.h>

#include <eez/gui/gui.h>
#include <eez/gui/thread.h>

#include <eez/flow/flow.h>

namespace eez {
namespace gui {

void mainLoop(void *);

EEZ_THREAD_DECLARE(gui, Normal, 8192)

EEZ_MESSAGE_QUEUE_DECLARE(gui, {
    uint8_t type;
    uint32_t param;
    int pageId;
    Page *page;
});

void startThread() {
	EEZ_MESSAGE_QUEUE_CREATE(gui, 10);
	EEZ_THREAD_CREATE(gui, mainLoop);
}

void oneIter();

void mainLoop(void *) {
#ifdef __EMSCRIPTEN__
    if (!g_isMainAssetsLoaded) {
        guiInit();
    }
	oneIter();
#else

    guiInit();

    while (1) {

#ifdef EEZ_PLATFORM_SIMULATOR
    if (g_shutdown) {
        break;
    }
#endif

        oneIter();
    }
#endif
}

void processGuiQueue(uint32_t timeout) {
    guiMessageQueueObject obj;
    if (!EEZ_MESSAGE_QUEUE_GET(gui, obj, timeout)) {
        return;
    }

    uint8_t type = obj.type;
    int16_t param = obj.param;

    if (type == GUI_QUEUE_MESSAGE_TYPE_DISPLAY_VSYNC) {
        display::update();
    } else if (type == GUI_QUEUE_MESSAGE_TYPE_SHOW_PAGE) {
        getAppContextFromId(param)->showPage(obj.pageId);
    } else if (type == GUI_QUEUE_MESSAGE_TYPE_PUSH_PAGE) {
        getAppContextFromId(param)->pushPage(obj.pageId, obj.page);
    } else if (type == GUI_QUEUE_MESSAGE_REFRESH_SCREEN) {
        refreshScreen();
    } else if (type == GUI_QUEUE_MESSAGE_UNLOAD_EXTERNAL_ASSETS) {
        unloadExternalAssets();
    } else if (type == GUI_QUEUE_MESSAGE_DEBUGGER_CLIENT_CONNECTED) {
        flow::onDebuggerClientConnected();
    } else if (type == GUI_QUEUE_MESSAGE_DEBUGGER_CLIENT_DISCONNECTED) {
        flow::onDebuggerClientDisconnected();
    } else if (type == GUI_QUEUE_MESSAGE_DEBUGGER_INPUT_AVAILABLE) {
        flow::onDebuggerInputAvailable();
    } else {
        onGuiQueueMessageHook(type, param);
    }
}

void oneIter() {
	processGuiQueue(100);
    guiTick();
}

void sendMessageToGuiThread(uint8_t messageType, uint32_t messageParam, uint32_t timeoutMillisec) {
    guiMessageQueueObject obj;
    obj.type = messageType;
    obj.param = messageParam;
	EEZ_MESSAGE_QUEUE_PUT(gui, obj, timeoutMillisec);
}

bool pushPageInGuiThread(AppContext *appContext, int pageId, Page *page) {
    if (!isGuiThread()) {
        guiMessageQueueObject obj;
        obj.type = GUI_QUEUE_MESSAGE_TYPE_PUSH_PAGE;
        obj.param = getAppContextId(appContext);
        obj.pageId = pageId;
        obj.page = page;
        EEZ_MESSAGE_QUEUE_PUT(gui, obj, osWaitForever);
        return true;
    }
    return false;
}

bool showPageInGuiThread(AppContext *appContext, int pageId) {
    if (!isGuiThread()) {
        guiMessageQueueObject obj;
        obj.type = GUI_QUEUE_MESSAGE_TYPE_SHOW_PAGE;
        obj.param = getAppContextId(appContext);
        obj.pageId = pageId;
        EEZ_MESSAGE_QUEUE_PUT(gui, obj, osWaitForever);
        return true;
    }
    return false;
}

void executeActionThreadHook() {
    // why is this required?
    osDelay(1);
}

bool isGuiThread() {
    return osThreadGetId() == g_guiTaskHandle;
}

} // namespace gui
} // namespace eez
