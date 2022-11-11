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

#include <math.h>
#include <assert.h>
#include <memory.h>

#include <eez/conf.h>
#include <eez/core/sound.h>
#include <eez/core/os.h>
#include <eez/core/util.h>

#if OPTION_KEYBOARD
#include <eez/core/keyboard.h>
#endif

#if OPTION_MOUSE
#include <eez/core/mouse.h>
#endif

#include <eez/gui/gui.h>
#include <eez/gui/thread.h>
#include <eez/gui/touch_calibration.h>
#include <eez/gui/widgets/button.h>

#include <eez/flow/debugger.h>
#include <eez/flow/private.h>

#include <eez/core/hmi.h>

#define CONF_GUI_TOAST_DURATION_MS 1000L

namespace eez {
namespace gui {

////////////////////////////////////////////////////////////////////////////////

AppContext::AppContext() {
    m_updatePageIndex = -1;
}

void AppContext::stateManagment() {
    // remove alert message after period of time
    if (getActivePageId() == INTERNAL_PAGE_ID_TOAST_MESSAGE) {
        ToastMessagePage *page = (ToastMessagePage *)getActivePage();
        if (!page->hasAction() && eez::hmi::getInactivityPeriodMs() >= CONF_GUI_TOAST_DURATION_MS) {
            popPage();
        }
    }
}

////////////////////////////////////////////////////////////////////////////////

bool AppContext::isActivePageInternal() {
    return isPageInternal(getActivePageId());
}

bool AppContext::isWidgetActionEnabled(const WidgetCursor &widgetCursor) {
    const Widget *widget = widgetCursor.widget;
    auto action = getWidgetAction(widgetCursor);
    if (action) {
        if (widget->type == WIDGET_TYPE_BUTTON) {
            auto buttonWidget = (const ButtonWidget *)widget;
			auto enabled = get(widgetCursor, buttonWidget->enabled);
            if (!(enabled.getType() == VALUE_TYPE_UNDEFINED || enabled.getInt() ? 1 : 0)) {
                return false;
            }
        }

        return true;
    }

    return false;
}

bool AppContext::isAutoRepeatAction(int action) {
    return false;
}

bool AppContext::isFocusWidget(const WidgetCursor &widgetCursor) {
    return false;
}

////////////////////////////////////////////////////////////////////////////////

void AppContext::onPageChanged(int previousPageId, int activePageId) {
    display::turnOn();
    hmi::noteActivity();

#if OPTION_MOUSE
    mouse::onPageChanged();
#endif

#if OPTION_KEYBOARD
    keyboard::onPageChanged();
#endif

    flow::onPageChanged(previousPageId, activePageId);
}

void AppContext::doShowPage(int pageId, Page *page, int previousPageId) {
#if CONF_OPTION_FPGA
    pageId = PAGE_ID_WELCOME_800X480;
    page = nullptr;
#endif

    page = page ? page : g_hooks.getPageFromId(pageId);

    m_pageNavigationStack[m_pageNavigationStackPointer].page = page;
    m_pageNavigationStack[m_pageNavigationStackPointer].pageId = pageId;
    m_pageNavigationStack[m_pageNavigationStackPointer].displayBufferIndex = -1;
    m_pageNavigationStack[m_pageNavigationStackPointer].timelinePosition = 0;

    if (page) {
        page->pageWillAppear();
    }

    m_showPageTime = millis();

    onPageChanged(previousPageId, pageId);

    refreshScreen();
}

void AppContext::setPage(int pageId) {
    int previousPageId = getActivePageId();

    // delete stack
    for (int i = 0; i <= m_pageNavigationStackPointer; ++i) {
        if (m_pageNavigationStack[i].page) {
            m_pageNavigationStack[i].page->pageFree();
        }
    }
    m_pageNavigationStackPointer = 0;

    //
    doShowPage(pageId, nullptr, previousPageId);
}

void AppContext::replacePage(int pageId, Page *page) {
    int previousPageId = getActivePageId();

    Page *activePage = getActivePage();
    if (activePage) {
    	activePage->pageFree();
    }

    doShowPage(pageId, page, previousPageId);
}

void AppContext::pushPage(int pageId, Page *page) {
    if (pushPageInGuiThread(this, pageId, page)) {
        return;
    }

    int previousPageId = getActivePageId();

    // advance stack pointer
    if (getActivePageId() != PAGE_ID_NONE && getActivePageId() != PAGE_ID_ASYNC_OPERATION_IN_PROGRESS && getActivePageId() != INTERNAL_PAGE_ID_TOAST_MESSAGE) {
        m_pageNavigationStackPointer++;
        assert (m_pageNavigationStackPointer < CONF_GUI_PAGE_NAVIGATION_STACK_SIZE);
    }

    doShowPage(pageId, page, previousPageId);
}

void AppContext::popPage() {
    if (m_pageNavigationStackPointer > 0) {
        int previousPageId = getActivePageId();

        if (m_pageNavigationStack[m_pageNavigationStackPointer].page) {
            m_pageNavigationStack[m_pageNavigationStackPointer].page->pageFree();
            m_pageNavigationStack[m_pageNavigationStackPointer].page = nullptr;
        }
        --m_pageNavigationStackPointer;

        doShowPage(m_pageNavigationStack[m_pageNavigationStackPointer].pageId, m_pageNavigationStack[m_pageNavigationStackPointer].page, previousPageId);
    }
}

void AppContext::removePageFromStack(int pageId) {
    for (int i = m_pageNavigationStackPointer; i > 0; i--) {
        if (m_pageNavigationStack[i].pageId == pageId) {
            if (i == m_pageNavigationStackPointer) {
                popPage();
            } else {
                if (m_pageNavigationStack[i].page) {
                    m_pageNavigationStack[i].page->pageFree();
                }

                for (int j = i + 1; j <= m_pageNavigationStackPointer; j++) {
                    memcpy(m_pageNavigationStack + j - 1, m_pageNavigationStack + j, sizeof(PageOnStack));
                }

                m_pageNavigationStack[m_pageNavigationStackPointer].page = nullptr;

                --m_pageNavigationStackPointer;
            }
            refreshScreen();
            break;
        }
    }
}

Page *AppContext::getPage(int pageId) {
    for (int i = 0; i <= m_pageNavigationStackPointer; ++i) {
        if (m_pageNavigationStack[i].pageId == pageId) {
            return m_pageNavigationStack[i].page;
        }
    }
    return nullptr;
}

bool AppContext::isPageOnStack(int pageId) {
    for (int i = 0; i <= m_pageNavigationStackPointer; ++i) {
        if (m_pageNavigationStack[i].pageId == pageId) {
            return true;
        }
    }
    return false;
}

bool AppContext::isExternalPageOnStack() {
    for (int i = 0; i <= m_pageNavigationStackPointer; ++i) {
        if (m_pageNavigationStack[i].pageId < 0) {
            return true;
        }
    }
    return false;
}

void AppContext::removeExternalPagesFromTheStack() {
	for (int i = 0; i <= m_pageNavigationStackPointer; ++i) {
		if (m_pageNavigationStack[i].pageId < 0) {
			removePageFromStack(m_pageNavigationStack[i].pageId);
			i = 0;
		}
	}
}

void AppContext::showPage(int pageId) {
    if (showPageInGuiThread(this, pageId)) {
        return;
    }

    if (pageId != getActivePageId()) {
        setPage(pageId);
    }
}

////////////////////////////////////////////////////////////////////////////////

bool AppContext::testExecuteActionOnTouchDown(int action) {
    return false;
}

bool AppContext::isBlinking(const WidgetCursor &widgetCursor, int16_t id) {
    return false;
}

bool AppContext::canExecuteActionWhenTouchedOutsideOfActivePage(int pageId, int action) {
    return false;
}

void AppContext::onPageTouch(const WidgetCursor &foundWidget, Event &touchEvent) {
    int activePageId = getActivePageId();
#if OPTION_TOUCH_CALIBRATION
    if (activePageId == PAGE_ID_TOUCH_CALIBRATION) {
        onTouchCalibrationPageTouch(foundWidget, touchEvent);
        return;
    }
#endif

    if (activePageId != PAGE_ID_NONE && !isPageInternal(activePageId)) {
        auto page = getPageAsset(activePageId);
        if ((page->flags & CLOSE_PAGE_IF_TOUCHED_OUTSIDE_FLAG) != 0) {
            int xPage;
            int yPage;
            int wPage;
            int hPage;
            getPageRect(activePageId, getActivePage(), xPage, yPage, wPage, hPage);

            if (!pointInsideRect(touchEvent.x, touchEvent.y, xPage, yPage, wPage, hPage)) {
                int activePageId = getActivePageId();

                // clicked outside page, close page
                popPage();

                auto widgetCursor = findWidget(touchEvent.x, touchEvent.y);

                if (widgetCursor.widget) {
                   auto action = getWidgetAction(widgetCursor);
                   if (action != ACTION_ID_NONE && canExecuteActionWhenTouchedOutsideOfActivePage(activePageId, action)) {
                       processTouchEvent(touchEvent);
                   }
                }
            }
        }
    }
}

////////////////////////////////////////////////////////////////////////////////

void AppContext::updatePage(int i, WidgetCursor &widgetCursor) {
	if (g_findCallback == nullptr) {
		m_pageNavigationStack[i].displayBufferIndex = display::beginBufferRendering();
	}

    m_updatePageIndex = i;

    int x;
    int y;
    int width;
    int height;
    bool withShadow;

    if (isPageInternal(m_pageNavigationStack[i].pageId)) {
        auto internalPage = ((InternalPage *)m_pageNavigationStack[i].page);

		x = internalPage->x;
		y = internalPage->y;
		width = internalPage->width;
		height = internalPage->height;
		withShadow = true;

        widgetCursor.w = width;
        widgetCursor.h = height;

		if (g_findCallback == nullptr) {
			internalPage->updateInternalPage();
		}

		enumNoneWidget();
    } else {
        auto page = getPageAsset(m_pageNavigationStack[i].pageId, widgetCursor);

        if (widgetCursor.flowState && widgetCursor.flowState->timelinePosition != m_pageNavigationStack[i].timelinePosition) {
            widgetCursor.hasPreviousState = false;
            m_pageNavigationStack[i].timelinePosition = widgetCursor.flowState->timelinePosition;
        }

        auto savedWidget = widgetCursor.widget;
        widgetCursor.widget = page;

        if ((page->flags & PAGE_SCALE_TO_FIT) && flow::g_debuggerMode == flow::DEBUGGER_MODE_RUN) {
		    x = rect.x;
		    y = rect.y;
		    width = rect.w;
		    height = rect.h;
        } else {
            x = widgetCursor.x + page->x;
            y = widgetCursor.y + page->y;
            width = page->width;
            height = page->height;
        }

        withShadow = page->x > 0;

        auto savedX = widgetCursor.x;
        auto savedY = widgetCursor.y;

        widgetCursor.x = x;
        widgetCursor.y = y;

        widgetCursor.w = width;
        widgetCursor.h = height;

        enumWidget();

        widgetCursor.x = savedX;
        widgetCursor.y = savedY;

        widgetCursor.widget = savedWidget;
    }

	if (g_findCallback == nullptr) {
        pageRenderCustom(i, widgetCursor);
		display::endBufferRendering(m_pageNavigationStack[i].displayBufferIndex, x, y, width, height, withShadow, 255, 0, 0, withShadow && g_hooks.activePageHasBackdrop() ? &rect : nullptr);
	}

    m_updatePageIndex = -1;
}

void AppContext::pageRenderCustom(int i, WidgetCursor &widgetCursor) {
}

bool isRect1FullyCoveredByRect2(int xRect1, int yRect1, int wRect1, int hRect1, int xRect2, int yRect2, int wRect2, int hRect2) {
    return xRect2 <= xRect1 && yRect2 <= yRect1 && xRect2 + wRect2 >= xRect1 + wRect1 && yRect2 + hRect2 >= yRect1 + hRect1;
}

void AppContext::getPageRect(int pageId, const Page *page, int &x, int &y, int &w, int &h) {
    if (isPageInternal(pageId)) {
        x = rect.x + ((InternalPage *)page)->x;
        y = rect.y + ((InternalPage *)page)->y;
        w = ((InternalPage *)page)->width;
        h = ((InternalPage *)page)->height;
    } else {
        auto page = getPageAsset(pageId);
        if ((page->flags & PAGE_SCALE_TO_FIT) && flow::g_debuggerMode == flow::DEBUGGER_MODE_RUN) {
		    x = rect.x;
		    y = rect.y;
		    w = rect.w;
		    h = rect.h;
        } else {
            x = rect.x + page->x;
            y = rect.y + page->y;
            w = page->width;
            h = page->height;
        }
    }
}

bool AppContext::isPageFullyCovered(int pageNavigationStackIndex) {
    int xPage, yPage, wPage, hPage;
    getPageRect(m_pageNavigationStack[pageNavigationStackIndex].pageId, m_pageNavigationStack[pageNavigationStackIndex].page, xPage, yPage, wPage, hPage);

    for (int i = pageNavigationStackIndex + 1; i <= m_pageNavigationStackPointer; i++) {
        int xPageAbove, yPageAbove, wPageAbove, hPageAbove;
        getPageRect(m_pageNavigationStack[i].pageId, m_pageNavigationStack[i].page, xPageAbove, yPageAbove, wPageAbove, hPageAbove);

        if (isRect1FullyCoveredByRect2(xPage, yPage, wPage, hPage, xPageAbove, yPageAbove, wPageAbove, hPageAbove)) {
            return true;
        }
    }

    return false;
}

int AppContext::getLongTouchActionHook(const WidgetCursor &widgetCursor) {
    return ACTION_ID_NONE;
}

void AppContext::yesNoDialog(int yesNoPageId, const char *message, void (*yes_callback)(), void (*no_callback)(), void (*cancel_callback)()) {
    set(WidgetCursor(), DATA_ID_ALERT_MESSAGE, Value(message));

    m_dialogYesCallback = yes_callback;
    m_dialogNoCallback = no_callback;
    m_dialogCancelCallback = cancel_callback;

    pushPage(yesNoPageId);
}

void AppContext::yesNoDialog(int yesNoPageId, Value value, void(*yes_callback)(), void(*no_callback)(), void(*cancel_callback)()) {
	set(WidgetCursor(), DATA_ID_ALERT_MESSAGE, value);

	m_dialogYesCallback = yes_callback;
	m_dialogNoCallback = no_callback;
	m_dialogCancelCallback = cancel_callback;

	pushPage(yesNoPageId);
}

void AppContext::infoMessage(const char *message) {
    pushToastMessage(ToastMessagePage::create(this, INFO_TOAST, message));
}

void AppContext::infoMessage(Value value) {
    pushToastMessage(ToastMessagePage::create(this, INFO_TOAST, value));
}

void AppContext::infoMessage(const char *message, void (*action)(), const char *actionLabel) {
    pushToastMessage(ToastMessagePage::create(this, INFO_TOAST, message, action, actionLabel));
}

void AppContext::errorMessage(const char *message, bool autoDismiss) {
    AppContext::pushToastMessage(ToastMessagePage::create(this, ERROR_TOAST, message, autoDismiss));
    sound::playBeep();
}

void AppContext::errorMessage(Value value) {
    AppContext::pushToastMessage(ToastMessagePage::create(this, ERROR_TOAST, value));
    sound::playBeep();
}

void AppContext::errorMessageWithAction(Value value, void (*action)(int param), const char *actionLabel, int actionParam) {
    AppContext::pushToastMessage(ToastMessagePage::create(this, ERROR_TOAST, value, action, actionLabel, actionParam));
    sound::playBeep();
}

void AppContext::errorMessageWithAction(const char *message, void (*action)(), const char *actionLabel) {
    AppContext::pushToastMessage(ToastMessagePage::create(this, ERROR_TOAST, message, action, actionLabel));
    sound::playBeep();
}

void AppContext::pushToastMessage(ToastMessagePage *toastMessage) {
    pushPage(INTERNAL_PAGE_ID_TOAST_MESSAGE, toastMessage);
}

void AppContext::questionDialog(Value message, Value buttons, void *userParam, void (*callback)(void *userParam, unsigned buttonIndex)) {
    pushPage(INTERNAL_PAGE_ID_QUESTION, QuestionPage::create(this, message, buttons, userParam, callback));
}

void AppContext::getBoundingRect(Rect &rectBounding) {
    if (m_pageNavigationStackPointer >= 0) {
        int x1 = rect.x + rect.w;
        int y1 = rect.y + rect.h;
        int x2 = rect.x;
        int y2 = rect.y;

        for (int i = 0; i <= m_pageNavigationStackPointer; ++i) {
            if (!isPageInternal(m_pageNavigationStack[i].pageId)) {
                int xPage, yPage, wPage, hPage;
                getPageRect(m_pageNavigationStack[i].pageId, m_pageNavigationStack[i].page, xPage, yPage, wPage, hPage);
                if (xPage < x1) x1 = xPage;
                if (xPage + wPage > x2) x2 = xPage + wPage;
                if (yPage < y1) y1 = yPage;
                if (yPage + hPage > y2) y2 = yPage + hPage;
            }
        }

        rectBounding.x = x1;
        rectBounding.y = y1;
        rectBounding.w = x2 - x1;
        rectBounding.h = y2 - y1;
    } else {
        rectBounding.x = rect.x;
        rectBounding.y = rect.y;
        rectBounding.w = rect.w;
        rectBounding.h = rect.h;
    }
}

AppContext *getRootAppContext() {
#ifdef EEZ_PLATFORM_SIMULATOR
    return getAppContextFromId(APP_CONTEXT_ID_SIMULATOR_FRONT_PANEL);
#else
    return getAppContextFromId(APP_CONTEXT_ID_DEVICE);
#endif
}

} // namespace gui
} // namespace eez
