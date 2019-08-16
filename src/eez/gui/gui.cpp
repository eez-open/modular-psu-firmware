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

#include <eez/apps/psu/gui/psu.h>
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
        mcu::display::sync();
    } else {
        oneIter();
    }
#else
    while (1) {
        osDelay(1);
        oneIter();
    }
#endif
}

void oneIter() {
    mcu::display::sync();

    g_wasBlinkTime = g_isBlinkTime;
    g_isBlinkTime = (micros() % (2 * CONF_GUI_BLINK_TIME)) > CONF_GUI_BLINK_TIME &&
                    touch::g_eventType == EVENT_TYPE_TOUCH_NONE;

    touch::tick();

    g_appContext = &getRootAppContext();

    g_appContext->x = 0;
    g_appContext->y = 0;
    g_appContext->width = mcu::display::getDisplayWidth();
    g_appContext->height = mcu::display::getDisplayWidth();

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

bool isPageInternal(int pageId) {
    return pageId < INTERNAL_PAGE_ID_NONE;
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
    dialogYes,
    ToastMessagePage::executeAction
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
	home::g_homeAppContext.showPageOnNextIter(PAGE_ID_WELCOME);
}

void showStandbyPage() {
	home::g_homeAppContext.showPageOnNextIter(PAGE_ID_STANDBY);
}

void showEnteringStandbyPage() {
	home::g_homeAppContext.showPageOnNextIter(PAGE_ID_ENTERING_STANDBY);
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

using namespace mcu::display;

#ifdef EEZ_PLATFORM_SIMULATOR
#define ANIMATION_DURATION_OPEN 250
#define ANIMATION_DURATION_CLOSE 250
#else
#define ANIMATION_DURATION_OPEN 250
#define ANIMATION_DURATION_CLOSE 250
#endif

AnimationState g_animationState;
static bool g_animationStateDirection;
static Rect g_animationStateSrcRect;
static Rect g_animationStateDstRect;

void animateOpenCloseCallback(float t, void *bufferOld, void *bufferNew, void *bufferDst) {
    if (!g_animationStateDirection) {
        auto bufferTemp = bufferOld;
        bufferOld = bufferNew;
        bufferNew = bufferTemp;
    }

    auto remapX = g_animationStateDirection ? remapExp : remapOutExp;
    auto remapY = g_animationStateDirection ? remapExp : remapOutExp;

    int srcX1;
    int srcY1;
    int srcX2;
    int srcY2;

    int dstX1;
    int dstY1;
    int dstX2;
    int dstY2;

    if (g_animationStateDirection) {
        srcX1 = g_animationStateSrcRect.x;
        srcY1 = g_animationStateSrcRect.y;
        srcX2 = g_animationStateSrcRect.x + g_animationStateSrcRect.w;
        srcY2 = g_animationStateSrcRect.y + g_animationStateSrcRect.h;

        int dx = MAX(g_animationStateSrcRect.x - g_animationStateDstRect.x,
            g_animationStateDstRect.x + g_animationStateDstRect.w -
            (g_animationStateSrcRect.x + g_animationStateSrcRect.w));

        int dy = MAX(g_animationStateSrcRect.y - g_animationStateDstRect.y,
            g_animationStateDstRect.y + g_animationStateDstRect.h -
            (g_animationStateSrcRect.y + g_animationStateSrcRect.h));

        dstX1 = g_animationStateSrcRect.x - dx;
        dstY1 = g_animationStateSrcRect.y - dy;
        dstX2 = g_animationStateSrcRect.x + g_animationStateSrcRect.w + dx;
        dstY2 = g_animationStateSrcRect.y + g_animationStateSrcRect.h + dy;
    } else {
        int dx = MAX(g_animationStateDstRect.x - g_animationStateSrcRect.x,
            g_animationStateSrcRect.x + g_animationStateSrcRect.w -
            (g_animationStateDstRect.x + g_animationStateDstRect.w));

        int dy = MAX(g_animationStateDstRect.y - g_animationStateSrcRect.y,
            g_animationStateSrcRect.y + g_animationStateSrcRect.h -
            g_animationStateDstRect.y + g_animationStateDstRect.h);

        srcX1 = g_animationStateDstRect.x - dx;
        srcY1 = g_animationStateDstRect.y - dx;
        srcX2 = g_animationStateDstRect.x + g_animationStateDstRect.w + dx;
        srcY2 = g_animationStateDstRect.y + g_animationStateDstRect.h + dy;

        dstX1 = g_animationStateDstRect.x;
        dstY1 = g_animationStateDstRect.y;
        dstX2 = g_animationStateDstRect.x + g_animationStateDstRect.w;
        dstY2 = g_animationStateDstRect.y + g_animationStateDstRect.h;
    }

    int x1 = (int)round(remapX((float)t, 0, (float)srcX1, 1, (float)dstX1));
    if (g_animationStateDirection) {
        if (x1 < g_animationStateDstRect.x) {
            x1 = g_animationStateDstRect.x;
        }
    } else {
        if (x1 < g_animationStateSrcRect.x) {
            x1 = g_animationStateSrcRect.x;
        }
    }

    int y1 = (int)round(remapY((float)t, 0, (float)srcY1, 1, (float)dstY1));
    if (g_animationStateDirection) {
        if (y1 < g_animationStateDstRect.y) {
            y1 = g_animationStateDstRect.y;
        }
    } else {
        if (y1 < g_animationStateSrcRect.y) {
            y1 = g_animationStateSrcRect.y;
        }
    }

    int x2 = (int)round(remapX((float)t, 0, (float)srcX2, 1, (float)dstX2));
    if (g_animationStateDirection) {
        if (x2 > g_animationStateDstRect.x + g_animationStateDstRect.w) {
            x2 = g_animationStateDstRect.x + g_animationStateDstRect.w;
        }
    } else {
        if (x2 > g_animationStateSrcRect.x + g_animationStateSrcRect.w) {
            x2 = g_animationStateSrcRect.x + g_animationStateSrcRect.w;
        }
    }

    int y2 = (int)round(remapY((float)t, 0, (float)srcY2, 1, (float)dstY2));
    if (g_animationStateDirection) {
        if (y2 > g_animationStateDstRect.y + g_animationStateDstRect.h) {
            y2 = g_animationStateDstRect.y + g_animationStateDstRect.h;
        }
    } else {
        if (y2 > g_animationStateSrcRect.y + g_animationStateSrcRect.h) {
            y2 = g_animationStateSrcRect.y + g_animationStateSrcRect.h;
        }
    }

    bitBlt(bufferOld, bufferDst, 0, 0, getDisplayWidth() - 1, getDisplayHeight() - 1);
    bitBlt(bufferNew, bufferDst, x1, y1, x2, y2);
}

void animate(uint32_t duration, void(*callback)(float t, void *bufferOld, void *bufferNew, void *bufferDst)) {
    g_animationState.enabled = true;
    g_animationState.startTime = millis();
    g_animationState.duration = duration;
    g_animationState.callback = callback;
}

void animateOpenClose(uint32_t duration, const Rect &srcRect, const Rect &dstRect, bool direction) {
    animate(duration, animateOpenCloseCallback);
    g_animationStateSrcRect = srcRect;
    g_animationStateDstRect = dstRect;
    g_animationStateDirection = direction;
}

void animateOpen(const Rect &srcRect, const Rect &dstRect) {
    animateOpenClose(ANIMATION_DURATION_OPEN, srcRect, dstRect, true);
}

void animateClose(const Rect &srcRect, const Rect &dstRect) {
    animateOpenClose(ANIMATION_DURATION_CLOSE, srcRect, dstRect, false);
}

static const Rect g_vertDefRects[3] = {
    {   0, 0, 160, 240 },
    { 160, 0, 160, 240 },
    { 320, 0, 160, 240 }
};
static const Rect g_horzDefRects[3] = {
    { 0,   0, 480, 80 },
    { 0,  80, 480, 80 },
    { 0, 160, 480, 80 }
};
static const Rect g_maxRect = { 0, 0, 480, 167 };
static const Rect g_minRects[2] = {
    {   0, 167, 240, 74 },
    { 240, 167, 240, 74 }
};

static int g_xOffset;
static int g_yOffset;

static int g_numRects;
static Rect g_srcRects[3];
static Rect g_dstRects[3];

void bitBltSpec(int i, void *src, void *dst, int sx, int sy, int sw, int sh, int dx, int dy, int dw, int dh, float opacity) {
    int w = MIN(dw, sw);
    int h = MIN(dh, sh);

    if (i == 2) {
        bitBlt(src, dst,
            sx + (sw - w) / 2, sy + (sh - h) / 2, w, h,
            dx + (dw - w) / 2, dy + (dh - h) / 2,
			(uint8_t)round(opacity * 255));
    } else {
        bitBlt(src, dst,
            (sw > dw ? sx : sx + (sw - w) / 2), (sh > dh ? sy : sy + (sh - h) / 2), w, h,
            (dw > sw ? dx : dx + (dw - w) / 2), 
            (dh > sh ? dy : dy + (dh - h) / 2),
			(uint8_t)round(opacity * 255));
    }
}

void animateRectsStep(float t, void *bufferOld, void *bufferNew, void *bufferDst) {
    auto tSaved = t;
	t = remapOutQuad(t, 0, 0, 1, 1);

    bitBlt(bufferOld, bufferDst, 0, 0, getDisplayWidth() - 1, getDisplayHeight() - 1);
    setColor(0);
    fillRect(bufferDst, g_xOffset + 0, g_yOffset + 0, g_xOffset + 480 - 1, g_yOffset + 240 - 1);
    
    for (int i = 0; i < 3; ++i) {
        if (g_srcRects[i].x == g_dstRects[i].x && g_srcRects[i].y == g_dstRects[i].y && g_srcRects[i].w == g_dstRects[i].w && g_srcRects[i].h == g_dstRects[i].h) {
            bitBlt(bufferNew, bufferDst, g_srcRects[i].x, g_srcRects[i].y, g_srcRects[i].x + g_srcRects[i].w - 1, g_srcRects[i].y + g_srcRects[i].h - 1);
        } else {
            bitBltSpec(i,
                bufferOld, bufferDst,
                g_srcRects[i].x, g_srcRects[i].y, g_srcRects[i].w, g_srcRects[i].h,
                (int)roundf(g_srcRects[i].x + t * (g_dstRects[i].x - g_srcRects[i].x)),
                (int)roundf(g_srcRects[i].y + t * (g_dstRects[i].y - g_srcRects[i].y)),
                (int)roundf(g_srcRects[i].w + t * (g_dstRects[i].w - g_srcRects[i].w)),
                (int)roundf(g_srcRects[i].h + t * (g_dstRects[i].h - g_srcRects[i].h)),
                1 - tSaved);

            bitBltSpec(i,
                bufferNew, bufferDst,
                g_dstRects[i].x, g_dstRects[i].y, g_dstRects[i].w, g_dstRects[i].h,
                (int)roundf(g_srcRects[i].x + t * (g_dstRects[i].x - g_srcRects[i].x)),
                (int)roundf(g_srcRects[i].y + t * (g_dstRects[i].y - g_srcRects[i].y)),
                (int)roundf(g_srcRects[i].w + t * (g_dstRects[i].w - g_srcRects[i].w)),
                (int)roundf(g_srcRects[i].h + t * (g_dstRects[i].h - g_srcRects[i].h)),
                tSaved);
        }
    }
}

void animateRects() {
    animate(350, animateRectsStep);

    g_xOffset = g_appContext->x;
    g_yOffset = g_appContext->y;

    if (g_xOffset > 0) {
        for (int i = 0; i < 3; i++) {
            g_srcRects[i].x += g_xOffset;
            g_dstRects[i].x += g_xOffset;
        }
    }

    if (g_appContext->y > 0) {
        for (int i = 0; i < 3; i++) {
            g_srcRects[i].y += g_yOffset;
            g_dstRects[i].y += g_yOffset;
        }
    }
}

void animateFromDefaultViewToMaxView() {
    int iMax = psu::gui::g_channel->index - 1;
    int iMin1 = iMax == 0 ? 1 : 0;
    int iMin2 = iMax == 2 ? 1 : 2;

    g_numRects = 3;

    auto g_defRects = psu::persist_conf::devConf.flags.channelsViewMode == CHANNELS_VIEW_MODE_NUMERIC || 
        psu::persist_conf::devConf.flags.channelsViewMode == CHANNELS_VIEW_MODE_HORZ_BAR ? g_vertDefRects : g_horzDefRects;

    g_srcRects[0] = g_defRects[iMin1];
    g_dstRects[0] = g_minRects[0];

    g_srcRects[1] = g_defRects[iMin2];
    g_dstRects[1] = g_minRects[1];

    g_srcRects[2] = g_defRects[iMax];
    g_dstRects[2] = g_maxRect;

    animateRects();
}

void animateFromMaxViewToDefaultView() {
    int iMax = psu::gui::g_channel->index - 1;
    int iMin1 = iMax == 0 ? 1 : 0;
    int iMin2 = iMax == 2 ? 1 : 2;

    g_numRects = 3;

    auto g_defRects = psu::persist_conf::devConf.flags.channelsViewMode == CHANNELS_VIEW_MODE_NUMERIC || 
        psu::persist_conf::devConf.flags.channelsViewMode == CHANNELS_VIEW_MODE_HORZ_BAR ? g_vertDefRects : g_horzDefRects;

    g_srcRects[0] = g_minRects[0];
    g_dstRects[0] = g_defRects[iMin1];

    g_srcRects[1] = g_minRects[1];
    g_dstRects[1] = g_defRects[iMin2];

    g_srcRects[2] = g_maxRect;
    g_dstRects[2] = g_defRects[iMax];

    animateRects();
}

void animateFromMinViewToMaxView(int iWasMax) {
    int iMax = psu::persist_conf::devConf.flags.channelMax;

    if ((iMax == 1 && iWasMax == 2) || (iMax == 2 && iWasMax == 1)) {
        g_srcRects[0] = g_maxRect;
        g_dstRects[0] = g_minRects[0];

        g_srcRects[1] = g_minRects[0];
        g_dstRects[1] = g_maxRect;

        g_srcRects[2] = g_minRects[1];
        g_dstRects[2] = g_minRects[1];
    } else if ((iMax == 2 && iWasMax == 3) || (iMax == 3 && iWasMax == 2)) {
        g_srcRects[0] = g_maxRect;
        g_dstRects[0] = g_minRects[1];

        g_srcRects[1] = g_minRects[1];
        g_dstRects[1] = g_maxRect;

        g_srcRects[2] = g_minRects[0];
        g_dstRects[2] = g_minRects[0];
    } else if (iMax == 1 && iWasMax == 3) {
        g_srcRects[1] = g_minRects[1];
        g_dstRects[1] = g_minRects[0];

        g_srcRects[0] = g_maxRect;
        g_dstRects[0] = g_minRects[1];

        g_srcRects[2] = g_minRects[0];
        g_dstRects[2] = g_maxRect;
    } else if (iMax == 3 && iWasMax == 1) {
        g_srcRects[0] = g_maxRect;
        g_dstRects[0] = g_minRects[0];

        g_srcRects[1] = g_minRects[0];
        g_dstRects[1] = g_minRects[1];

        g_srcRects[2] = g_minRects[1];
        g_dstRects[2] = g_maxRect;
    }

    animateRects();
}

} // namespace gui
} // namespace eez
