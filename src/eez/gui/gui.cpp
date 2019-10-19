/*
 * EEZ Modular Firmware
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

#if OPTION_DISPLAY

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

#define CONF_GUI_BLINK_TIME 400 // 400ms

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
            g_guiTaskHandle = osThreadCreate(osThread(g_guiTask), nullptr);
        }
    }

    return true;
}


void oneIter();

static bool g_assetsInitialized = false;

void mainLoop(const void *) {
	if (!g_assetsInitialized) {
		g_assetsInitialized = true;
		decompressAssets();
		mcu::display::onThemeChanged();
		mcu::display::updateBrightness();
	}
#ifdef __EMSCRIPTEN__
	oneIter();
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
    g_isBlinkTime = (millis() % (2 * CONF_GUI_BLINK_TIME)) > CONF_GUI_BLINK_TIME &&
                    touch::g_eventType == EVENT_TYPE_TOUCH_NONE;

    touch::tick();

    g_appContext = &getRootAppContext();

    g_appContext->x = 0;
    g_appContext->y = 0;
    g_appContext->width = mcu::display::getDisplayWidth();
    g_appContext->height = mcu::display::getDisplayHeight();

    eventHandling();
    stateManagment();

    if (mcu::display::isOn()) {
        updateScreen();
    }
}

////////////////////////////////////////////////////////////////////////////////

uint32_t getShowPageTime() {
    return g_appContext->m_showPageTime;
}

void setShowPageTime(uint32_t time) {
    g_appContext->m_showPageTime = time;
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

Page *getPage(int pageId) {
    return g_appContext->getPage(pageId);
}

bool isPageActiveOrOnStack(int pageId) {
    return g_appContext->isPageActiveOrOnStack(pageId);
}

void pushSelectFromEnumPage(const data::EnumItem *enumDefinition, uint16_t currentValue,
                            bool (*disabledCallback)(uint16_t value), void (*onSet)(uint16_t)) {
    g_appContext->pushSelectFromEnumPage(enumDefinition, currentValue, disabledCallback, onSet);
}

void pushSelectFromEnumPage(void(*enumDefinitionFunc)(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value),
	                        uint16_t currentValue, bool(*disabledCallback)(uint16_t value), void(*onSet)(uint16_t)) {
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
    popPage,
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

    psu::persist_conf::lockFrontPanel(false);
    infoMessage("Front panel is unlocked!");
}

static void checkPasswordToUnlockFrontPanel() {
    psu::gui::checkPassword("Password: ", psu::persist_conf::devConf2.systemPassword, doUnlockFrontPanel);
}

void lockFrontPanel() {
    psu::persist_conf::lockFrontPanel(true);
    infoMessage("Front panel is locked!");
}

void unlockFrontPanel() {
    if (strlen(psu::persist_conf::devConf2.systemPassword) > 0) {
        checkPasswordToUnlockFrontPanel();
    } else {
        psu::persist_conf::lockFrontPanel(false);
        infoMessage("Front panel is unlocked!");
    }
}

bool isFrontPanelLocked() {
    return psu::g_rlState != psu::RL_STATE_LOCAL;
}

////////////////////////////////////////////////////////////////////////////////

using namespace mcu::display;

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

void animate(Buffer startBuffer, void(*callback)(float t, void *bufferOld, void *bufferNew, void *bufferDst), float duration = -1) {
    if (g_animationState.enabled) {
        mcu::display::finishAnimation();
    }
    g_animationState.enabled = true;
    g_animationState.startTime = millis();
    g_animationState.duration = duration != -1 ? duration : psu::persist_conf::devConf2.animationsDuration;
    g_animationState.startBuffer = startBuffer;
    g_animationState.callback = callback;
    g_animationState.easingRects = remapOutQuad;
    g_animationState.easingOpacity = remapOutCubic;

}

void animateOpenClose(const Rect &srcRect, const Rect &dstRect, bool direction) {
    animate(BUFFER_OLD, animateOpenCloseCallback);
    g_animationStateSrcRect = srcRect;
    g_animationStateDstRect = dstRect;
    g_animationStateDirection = direction;
}

void animateOpen(const Rect &srcRect, const Rect &dstRect) {
    animateOpenClose(srcRect, dstRect, true);
}

void animateClose(const Rect &srcRect, const Rect &dstRect) {
    animateOpenClose(srcRect, dstRect, false);
}

static Rect g_clipRect;
static int g_numRects;
AnimRect g_animRects[MAX_ANIM_RECTS];

void animateRectsStep(float t, void *bufferOld, void *bufferNew, void *bufferDst) {
    bitBlt(g_animationState.startBuffer == BUFFER_OLD ? bufferOld : bufferNew, bufferDst, 0, 0, getDisplayWidth() - 1, getDisplayHeight() - 1);

    float t1 = g_animationState.easingRects(t, 0, 0, 1, 1); // rects
    float t2 = g_animationState.easingOpacity(t, 0, 0, 1, 1); // opacity

    for (int i = 0; i < g_numRects; i++) {
        AnimRect &animRect = g_animRects[i];

        int x, y, w, h;

        if (animRect.srcRect == animRect.dstRect) {
            x = animRect.srcRect.x;
            y = animRect.srcRect.y;
            w = animRect.srcRect.w;
            h = animRect.srcRect.h;
        } else {
            x = (int)roundf(animRect.srcRect.x + t1 * (animRect.dstRect.x - animRect.srcRect.x));
            y = (int)roundf(animRect.srcRect.y + t1 * (animRect.dstRect.y - animRect.srcRect.y));
            w = (int)roundf(animRect.srcRect.w + t1 * (animRect.dstRect.w - animRect.srcRect.w));
            h = (int)roundf(animRect.srcRect.h + t1 * (animRect.dstRect.h - animRect.srcRect.h));
        }

        uint8_t opacity;
        if (animRect.opacity == OPACITY_FADE_IN) {
            opacity = (uint8_t)roundf(clamp(roundf(t2 * 255), 0, 255));
        } else if (animRect.opacity == OPACITY_FADE_OUT) {
            opacity = (uint8_t)roundf(clamp((1 - t2) * 255, 0, 255));
        } else {
            opacity = 255;
        }

        if (animRect.buffer == BUFFER_SOLID_COLOR) {
            auto savedOpacity = setOpacity(opacity);
            setColor(animRect.color);

            // clip
            if (x < g_clipRect.x) {
                w -= g_clipRect.x - x;
                x = g_clipRect.x;
            }

            if (x + w > g_clipRect.x + g_clipRect.w) {
                w -= (x + w) - (g_clipRect.x + g_clipRect.w);
            }

            if (y < g_clipRect.y) {
                h -= g_clipRect.y - y;
                y = g_clipRect.y;
            }

            if (y + h > g_clipRect.y + g_clipRect.h) {
                h -= (y + h) - (g_clipRect.y + g_clipRect.y);
            }

            fillRect(bufferDst, x, y, x + w - 1, y + h - 1);

            setOpacity(savedOpacity);
        } else {
            void *buffer = animRect.buffer == BUFFER_OLD ? bufferOld : bufferNew;
            Rect &srcRect = animRect.buffer == BUFFER_OLD ? animRect.srcRect : animRect.dstRect;

            int sx;
            int sy;
            int sw;
            int sh;

            int dx;
            int dy;

            if (animRect.position == POSITION_TOP_LEFT || animRect.position == POSITION_LEFT || animRect.position == POSITION_BOTTOM_LEFT) {
                sx = srcRect.x;
                sw = MIN(srcRect.w, w);
                dx = x;
            } else if (animRect.position == POSITION_TOP || animRect.position == POSITION_CENTER || animRect.position == POSITION_BOTTOM) {
                if (srcRect.w < w) {
                    sx = srcRect.x;
                    sw = srcRect.w;
                    dx = x + (w - srcRect.w) / 2;
                } else if (srcRect.w > w) {
                    sx = srcRect.x + (srcRect.w - w) / 2;
                    sw = w;
                    dx = x;
                } else {
                    sx = srcRect.x;
                    sw = srcRect.w;
                    dx = x;
                }
            } else {
                sw = MIN(srcRect.w, w);
                sx = srcRect.x + srcRect.w - sw;
                dx = x + w - sw;
            }

            if (animRect.position == POSITION_TOP_LEFT || animRect.position == POSITION_TOP || animRect.position == POSITION_TOP_RIGHT) {
                sy = srcRect.y;
                sh = MIN(srcRect.h, h);
                dy = y;
            } else if (animRect.position == POSITION_LEFT || animRect.position == POSITION_CENTER || animRect.position == POSITION_RIGHT) {
                if (srcRect.h < h) {
                    sy = srcRect.y;
                    sh = srcRect.h;
                    dy = y + (h - srcRect.h) / 2;
                } else if (srcRect.h > h) {
                    sy = srcRect.y + (srcRect.h - h) / 2;
                    sh = h;
                    dy = y;
                } else {
                    sy = srcRect.y;
                    sh = srcRect.h;
                    dy = y;
                }
            } else {
                sh = MIN(srcRect.h, h);
                sy = srcRect.y + srcRect.h - sh;
                dy = y + h - sh;
            }

            // clip
            if (sx < g_clipRect.x) {
                sw -= g_clipRect.x - sx;
                dx += g_clipRect.x - sx;
                sx = g_clipRect.x;
            }

            if (dx < g_clipRect.x) {
                sw -= g_clipRect.x - dx;
                sx += g_clipRect.x - dx;
                dx = g_clipRect.x;
            }

            if (sx + sw > g_clipRect.x + g_clipRect.w) {
                sw -= (sx + sw) - (g_clipRect.x + g_clipRect.w);
            }

            if (dx + sw > g_clipRect.x + g_clipRect.w) {
                sw -= (dx + sw) - (g_clipRect.x + g_clipRect.w);
            }

            if (sy < g_clipRect.y) {
                sh -= g_clipRect.y - sy;
                dy += g_clipRect.y - sy;
                sy = g_clipRect.y;
            }

            if (dy < g_clipRect.y) {
                sh -= g_clipRect.y - dy;
                sy += g_clipRect.y - dy;
                dy = g_clipRect.y;
            }

            if (dy + sh > g_clipRect.y + g_clipRect.h) {
                sh -= (dy + sh) - (g_clipRect.y + g_clipRect.h);
            }

            if (sy + sh > g_clipRect.y + g_clipRect.h) {
                sy -= (sy + sh) - (g_clipRect.y + g_clipRect.h);
            }

            bitBlt(buffer, bufferDst, sx, sy, sw, sh, dx, dy, opacity);
        }
    }
}

void prepareRect(Rect &rect) {
    if (g_appContext->x > 0) {
        rect.x += g_appContext->x;
    }
    if (g_appContext->y > 0) {
        rect.y += g_appContext->y;
    }
}

void animateRects(Buffer startBuffer, int numRects, float duration) {
    animate(startBuffer, animateRectsStep, duration);

    g_numRects = numRects;

    g_clipRect.x = g_appContext->x;
    g_clipRect.y = g_appContext->y;
    g_clipRect.w = g_appContext->width;
    g_clipRect.h = g_appContext->height;


    for (int i = 0; i < numRects; i++) {
        prepareRect(g_animRects[i].srcRect);
        prepareRect(g_animRects[i].dstRect);
    }
}

} // namespace gui
} // namespace eez

#endif
