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

#include <eez/core/os.h>

#include <eez/gui/gui.h>
#include <eez/gui/thread.h>

#include <eez/flow/flow.h>
#include <eez/flow/hooks.h>

namespace eez {
namespace gui {

void mainLoop(void *);

EEZ_THREAD_DECLARE(gui, Normal, 12 * 1024)

EEZ_MESSAGE_QUEUE_DECLARE(gui, {
    uint8_t type;
    union {
        uint32_t param;

        struct {
            AppContext *appContext;
            int pageId;
            Page *page;
        } changePage;
        
        Event touchEvent;
    };
});

void startThread() {
	EEZ_MESSAGE_QUEUE_CREATE(gui, 20);
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

    if (type == GUI_QUEUE_MESSAGE_TYPE_DISPLAY_VSYNC) {
        display::update();
    } else if (type == GUI_QUEUE_MESSAGE_TYPE_TOUCH_EVENT) {
        processTouchEvent(obj.touchEvent);
    } else if (type == GUI_QUEUE_MESSAGE_TYPE_SHOW_PAGE) {
        obj.changePage.appContext->showPage(obj.changePage.pageId);
    } else if (type == GUI_QUEUE_MESSAGE_TYPE_PUSH_PAGE) {
        obj.changePage.appContext->pushPage(obj.changePage.pageId, obj.changePage.page);
    } else if (type == GUI_QUEUE_MESSAGE_REFRESH_SCREEN) {
        refreshScreen();
    } else if (type == GUI_QUEUE_MESSAGE_UNLOAD_EXTERNAL_ASSETS) {
        unloadExternalAssets();
    } else if (type == GUI_QUEUE_MESSAGE_DEBUGGER_CLIENT_CONNECTED) {
        flow::onDebuggerClientConnected();
    } else if (type == GUI_QUEUE_MESSAGE_DEBUGGER_CLIENT_DISCONNECTED) {
        flow::onDebuggerClientDisconnected();
    } else if (type == GUI_QUEUE_MESSAGE_DEBUGGER_INPUT_AVAILABLE) {
        flow::onDebuggerInputAvailableHook();
    } else {
        g_hooks.onGuiQueueMessage(type, obj.param);
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

void sendTouchEventToGuiThread(Event &touchEvent) {
	touchEvent.time = millis();

    guiMessageQueueObject obj;
    obj.type = GUI_QUEUE_MESSAGE_TYPE_TOUCH_EVENT;
    obj.touchEvent = touchEvent;
	EEZ_MESSAGE_QUEUE_PUT(gui, obj, 0);
}

bool pushPageInGuiThread(AppContext *appContext, int pageId, Page *page) {
    if (!isGuiThread()) {
        guiMessageQueueObject obj;
        obj.type = GUI_QUEUE_MESSAGE_TYPE_PUSH_PAGE;
        obj.changePage.appContext = appContext;
        obj.changePage.pageId = pageId;
        obj.changePage.page = page;
        EEZ_MESSAGE_QUEUE_PUT(gui, obj, osWaitForever);
        return true;
    }
    return false;
}

bool showPageInGuiThread(AppContext *appContext, int pageId) {
    if (!isGuiThread()) {
        guiMessageQueueObject obj;
        obj.type = GUI_QUEUE_MESSAGE_TYPE_SHOW_PAGE;
        obj.changePage.appContext = appContext;
        obj.changePage.pageId = pageId;
        EEZ_MESSAGE_QUEUE_PUT(gui, obj, osWaitForever);
        return true;
    }
    return false;
}

bool isGuiThread() {
    return osThreadGetId() == g_guiTaskHandle;
}

} // namespace gui
} // namespace eez
