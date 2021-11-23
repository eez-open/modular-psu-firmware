#include <bb3/firmware.h>

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

void mainLoop(const void *);

#if defined(EEZ_PLATFORM_STM32)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wwrite-strings"
#endif

osThreadDef(g_guiTask, mainLoop, osPriorityNormal, 0, 4096);

#if defined(EEZ_PLATFORM_STM32)
#pragma GCC diagnostic pop
#endif

osThreadId g_guiTaskHandle;

#define GUI_QUEUE_SIZE 10

osMessageQDef(g_guiMessageQueue, GUI_QUEUE_SIZE, uint32_t);
osMessageQId g_guiMessageQueueId;

#define GUI_QUEUE_MESSAGE(type, param) ((((uint32_t)(uint16_t)(int16_t)param) << 8) | (type))
#define GUI_QUEUE_MESSAGE_TYPE(message) ((message) & 0xFF)
#define GUI_QUEUE_MESSAGE_PARAM(param) ((int16_t)(message >> 8))

void startThread() {
    loadMainAssets(assets, sizeof(assets));
    display::onThemeChanged();
    mouse::init();
    g_guiMessageQueueId = osMessageCreate(osMessageQ(g_guiMessageQueue), 0);
    g_guiTaskHandle = osThreadCreate(osThread(g_guiTask), nullptr);
}

void oneIter();

void mainLoop(const void *) {
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

void oneIter() {
	static uint32_t lastTime;
	uint32_t tickCount = millis();
	g_debugFPS = 1000.0f / (tickCount - lastTime);
	lastTime = tickCount;

	while (true) {
		osEvent event = osMessageGet(g_guiMessageQueueId, 0);

		if (event.status != osEventMessage) {
			break;
		}

		uint32_t message = event.value.v;
		uint8_t type = GUI_QUEUE_MESSAGE_TYPE(message);
		int16_t param = GUI_QUEUE_MESSAGE_PARAM(message);
		onGuiQueueMessage(type, param);
	}

	WATCHDOG_RESET(WATCHDOG_GUI_THREAD);

	guiTick();

    scripting::flowTick();
}

void sendMessageToGuiThread(uint8_t messageType, uint32_t messageParam, uint32_t timeoutMillisec) {
    osMessagePut(g_guiMessageQueueId, GUI_QUEUE_MESSAGE(messageType, messageParam), timeoutMillisec);
}

bool pushPageThreadHook(AppContext *appContext, int pageId, Page *page) {
    if (osThreadGetId() != g_guiTaskHandle) {
        appContext->m_pageIdToSetOnNextIter = pageId;
        appContext->m_pageToSetOnNextIter = page;
        sendMessageToGuiThread(GUI_QUEUE_MESSAGE_TYPE_PUSH_PAGE, getAppContextId(appContext));
        return true;
    }
    return false;
}

bool showPageThreadHook(AppContext *appContext, int pageId) {
    if (osThreadGetId() != g_guiTaskHandle) {
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

} // namespace gui
} // namespace eez
