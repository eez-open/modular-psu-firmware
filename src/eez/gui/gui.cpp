/*
 * EEZ Generic Firmware
 * Copyright (C) 2015-present, Envox d.o.o.
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

#include <eez/gui/gui.h>

#include <math.h>
#include <string.h>

#include <eez/gui/app_context.h>
#include <eez/gui/dialogs.h>
#include <eez/gui/event.h>
#include <eez/gui/state.h>
#include <eez/gui/touch.h>
#include <eez/gui/update.h>
#include <eez/sound.h>
#include <eez/system.h>
#include <eez/util.h>

#include <eez/apps/home/home.h>

// TODO this must be removed from here
#include <eez/apps/psu/psu.h>

#include <eez/apps/psu/gui/password.h>
#include <eez/apps/psu/persist_conf.h>

#include <eez/modules/mcu/display.h>

#define CONF_GUI_BLINK_TIME 400000UL // 400ms

namespace eez {
namespace gui {

bool g_isBlinkTime;
static bool g_wasBlinkTime;

////////////////////////////////////////////////////////////////////////////////

void mainLoop(const void *);

#if defined(EEZ_PLATFORM_STM32)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wwrite-strings"
#endif

osThreadDef(g_guiTask, mainLoop, osPriorityNormal, 0, 2048);

#if defined(EEZ_PLATFORM_STM32)
#pragma GCC diagnostic pop
#endif

osThreadId g_guiTaskHandle;

bool onSystemStateChanged() {
    if (eez::g_systemState == eez::SystemState::BOOTING) {
        if (eez::g_systemStatePhase == 0) {
            return false;
        } else if (eez::g_systemStatePhase == 1) {
            decompressAssets();
        	mcu::display::onThemeChanged();
        	mcu::display::updateBrightness();
            g_guiTaskHandle = osThreadCreate(osThread(g_guiTask), nullptr);
        }
    }

    return true;
}

void oneIter();

void mainLoop(const void *) {
#ifdef __EMSCRIPTEN__
    if (g_animationState.enabled) {
        mcu::display::sync(true);
    } else {
        oneIter();
    }
#else
    while (1) {
        oneIter();
    }
#endif
}

void oneIter() {
    mcu::display::sync(g_painted);
    g_painted = false;

    g_wasBlinkTime = g_isBlinkTime;
    g_isBlinkTime = (micros() % (2 * CONF_GUI_BLINK_TIME)) > CONF_GUI_BLINK_TIME &&
                    touch::g_eventType == EVENT_TYPE_TOUCH_NONE;

    touch::tick();

    g_appContext = &getRootAppContext();
    eventHandling();
    stateManagment();
    updateScreen();
}

////////////////////////////////////////////////////////////////////////////////

uint32_t getShowPageTime() {
    return g_appContext->m_showPageTime;
}

void setShowPageTime(uint32_t time) {
    g_appContext->m_showPageTime = time;
}

void refreshScreen() {
    getRootAppContext().markForRefreshAppView();
}

void showPage(int pageId) {
    if (g_appContext) {
        g_appContext->showPage(pageId);
    }
}

void pushPage(int pageId, Page *page) {
    g_appContext->pushPage(pageId, page);
}

void popPage() {
    g_appContext->popPage();
}

void replacePage(int pageId, Page *page) {
    g_appContext->replacePage(pageId, page);
}

int getActivePageId() {
    return g_appContext->getActivePageId();
}

Page *getActivePage() {
    return g_appContext->getActivePage();
}

int getPreviousPageId() {
    return g_appContext->getPreviousPageId();
}

Page *getPreviousPage() {
    return g_appContext->getPreviousPage();
}

bool isPageActiveOrOnStack(int pageId) {
    return g_appContext->isPageActiveOrOnStack(pageId);
}

void pushSelectFromEnumPage(const data::EnumItem *enumDefinition, uint8_t currentValue,
                            bool (*disabledCallback)(uint8_t value), void (*onSet)(uint8_t)) {
    g_appContext->pushSelectFromEnumPage(enumDefinition, currentValue, disabledCallback, onSet);
}

void pushSelectFromEnumPage(void(*enumDefinitionFunc)(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value),
	                        uint8_t currentValue, bool(*disabledCallback)(uint8_t value), void(*onSet)(uint8_t)) {
	g_appContext->pushSelectFromEnumPage(enumDefinitionFunc, currentValue, disabledCallback, onSet);
}

int transformStyle(const Widget *widget) {
    return g_appContext->transformStyle(widget);
}

////////////////////////////////////////////////////////////////////////////////

void action_internal_select_enum_item() {
    ((SelectFromEnumPage *)g_appContext->getActivePage())->selectEnumItem();
}

static ActionExecFunc g_internalActionExecFunctions[] = {
    0,
    action_internal_select_enum_item,
    dialogYes
};

bool isInternalAction(int actionId) {
    return actionId < 0;
}

void executeInternalAction(int actionId) {
    g_internalActionExecFunctions[-actionId]();
}

void executeAction(int actionId) {
    AppContext *saved = g_appContext;
    g_appContext = getFoundWidgetAtDown().appContext;

    if (isInternalAction(actionId)) {
        executeInternalAction(actionId);
    } else {
        g_actionExecFunctions[actionId]();
    }

    g_appContext = saved;

    sound::playClick();
}

////////////////////////////////////////////////////////////////////////////////

void showWelcomePage() {
	home::g_homeAppContext.showPage(PAGE_ID_WELCOME);
}

void showStandbyPage() {
	home::g_homeAppContext.showPage(PAGE_ID_STANDBY);
}

void showEnteringStandbyPage() {
	home::g_homeAppContext.showPage(PAGE_ID_ENTERING_STANDBY);
}

////////////////////////////////////////////////////////////////////////////////

// TODO move these to actions.cpp

void standBy() {
    psu::changePowerState(false);
}

void turnDisplayOff() {
    psu::persist_conf::setDisplayState(0);
}

void reset() {
    popPage();
    psu::reset();
}

////////////////////////////////////////////////////////////////////////////////

static void doUnlockFrontPanel() {
    popPage();

    if (psu::persist_conf::lockFrontPanel(false)) {
        infoMessage("Front panel is unlocked!");
    }
}

static void checkPasswordToUnlockFrontPanel() {
    psu::gui::checkPassword("Password: ", psu::persist_conf::devConf2.systemPassword,
                            doUnlockFrontPanel);
}

void lockFrontPanel() {
    if (psu::persist_conf::lockFrontPanel(true)) {
        infoMessage("Front panel is locked!");
    }
}

void unlockFrontPanel() {
    if (strlen(psu::persist_conf::devConf2.systemPassword) > 0) {
        checkPasswordToUnlockFrontPanel();
    } else {
        if (psu::persist_conf::lockFrontPanel(false)) {
            infoMessage("Front panel is unlocked!");
        }
    }
}

bool isFrontPanelLocked() {
    return psu::g_rlState != psu::RL_STATE_LOCAL;
}

////////////////////////////////////////////////////////////////////////////////

#ifdef EEZ_PLATFORM_SIMULATOR
#define ANIMATION_DURATION_OPEN 250
#define ANIMATION_DURATION_CLOSE 250
#else
#define ANIMATION_DURATION_OPEN 250
#define ANIMATION_DURATION_CLOSE 250
#endif

AnimationState g_animationState;

void animate(uint32_t duration, const Rect &srcRect, const Rect &dstRect) {
    g_animationState.enabled = true;
    g_animationState.startTime = millis();
    g_animationState.duration = duration;

    g_animationState.srcRect = srcRect;
    g_animationState.dstRect = dstRect;
}

void animateOpen(const Rect &srcRect, const Rect &dstRect) {
    animate(ANIMATION_DURATION_OPEN, srcRect, dstRect);
    g_animationState.direction = true;
}

void animateClose(const Rect &srcRect, const Rect &dstRect) {
    animate(ANIMATION_DURATION_CLOSE, srcRect, dstRect);
    g_animationState.direction = false;
}

} // namespace gui
} // namespace eez
