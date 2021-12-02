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

#include <bb3/firmware.h>

#include <eez/os.h>

#include <eez/gui/gui.h>
#include <eez/flow/flow.h>

#include <bb3/gui/document.h>
#include <bb3/gui/thread.h>
#include <bb3/mouse.h>
#include <bb3/scripting/scripting.h>
#include <bb3/scripting/flow.h>

volatile float g_debugFPS;

namespace eez {
namespace gui {

void mainLoop(void *);

EEZ_THREAD_DECLARE(gui, Normal, 8192)

EEZ_MESSAGE_QUEUE_DECLARE(gui, {
    uint8_t type;
    uint32_t param;
});

void startThread() {
    loadMainAssets(assets, sizeof(assets));
    display::onThemeChanged();
    mouse::init();
    guiInit();
	EEZ_MESSAGE_QUEUE_CREATE(gui, 10);
	EEZ_THREAD_CREATE(gui, mainLoop);
}

void oneIter();

void mainLoop(void *) {
#ifdef __EMSCRIPTEN__
	oneIter();
#else
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

void onGuiQueueMessage(uint8_t type, int16_t param) {
    if (type == GUI_QUEUE_MESSAGE_TYPE_SHOW_PAGE) {
        getAppContextFromId(param)->doShowPage();
    } else if (type == GUI_QUEUE_MESSAGE_TYPE_PUSH_PAGE) {
        getAppContextFromId(param)->doPushPage();
    } else if (type == GUI_QUEUE_MESSAGE_MOUSE_X_MOVE) {
        mouse::onMouseXMove(param);
    } else if (type == GUI_QUEUE_MESSAGE_MOUSE_Y_MOVE) {
        mouse::onMouseYMove(param);
    } else if (type == GUI_QUEUE_MESSAGE_MOUSE_BUTTON_DOWN) {
        mouse::onMouseButtonDown(param);
    } else if (type == GUI_QUEUE_MESSAGE_MOUSE_BUTTON_UP) {
        mouse::onMouseButtonUp(param);
    } else if (type == GUI_QUEUE_MESSAGE_MOUSE_DISCONNECTED) {
        mouse::onMouseDisconnected();
    } else if (type == GUI_QUEUE_MESSAGE_REFRESH_SCREEN) {
        refreshScreen();
    } else if (type == GUI_QUEUE_MESSAGE_FLOW_START) {
        scripting::startFlowScript();
    } else if (type == GUI_QUEUE_MESSAGE_FLOW_STOP) {
        scripting::stopFlowScript();
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

void processGuiQueue() {
	while (true) {
	    WATCHDOG_RESET(WATCHDOG_GUI_THREAD);

        guiMessageQueueObject obj;
		if (!EEZ_MESSAGE_QUEUE_GET(gui, obj, 0)) {
            break;
        }

        onGuiQueueMessage(obj.type, obj.param);
	}
}

void oneIter() {
	static uint32_t lastTime;
	uint32_t tickCount = millis();
	g_debugFPS = 1000.0f / (tickCount - lastTime);
	lastTime = tickCount;

	processGuiQueue();

	guiTick();

    scripting::flowTick();
}

void sendMessageToGuiThread(uint8_t messageType, uint32_t messageParam, uint32_t timeoutMillisec) {
    guiMessageQueueObject obj;
    obj.type = messageType;
    obj.param = messageParam;
	EEZ_MESSAGE_QUEUE_PUT(gui, obj, timeoutMillisec);
}

bool pushPageThreadHook(AppContext *appContext, int pageId, Page *page) {
    if (!isGuiThread()) {
        appContext->m_pageIdToSetOnNextIter = pageId;
        appContext->m_pageToSetOnNextIter = page;
        sendMessageToGuiThread(GUI_QUEUE_MESSAGE_TYPE_PUSH_PAGE, getAppContextId(appContext));
        return true;
    }
    return false;
}

bool showPageThreadHook(AppContext *appContext, int pageId) {
    if (!isGuiThread()) {
        appContext->m_pageIdToSetOnNextIter = pageId;
        appContext->m_pageToSetOnNextIter = nullptr;
        sendMessageToGuiThread(GUI_QUEUE_MESSAGE_TYPE_SHOW_PAGE, getAppContextId(appContext));
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
