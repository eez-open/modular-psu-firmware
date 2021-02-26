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

#include <math.h>
#include <assert.h>
#include <memory.h>

#include <eez/index.h>
#include <eez/sound.h>
#include <eez/system.h>
#include <eez/hmi.h>
#include <eez/util.h>
#include <eez/keyboard.h>
#include <eez/mouse.h>

#include <eez/gui/gui.h>
#include <eez/gui/widgets/button.h>

namespace eez {
namespace gui {

////////////////////////////////////////////////////////////////////////////////

AppContext::AppContext() {
    m_updatePageIndex = -1;
}

void AppContext::stateManagment() {
}

////////////////////////////////////////////////////////////////////////////////

bool AppContext::isActivePageInternal() {
    return isPageInternal(getActivePageId());
}

bool AppContext::isWidgetActionEnabled(const WidgetCursor &widgetCursor) {
    const Widget *widget = widgetCursor.widget;
    if (widget->action) {
        if (widget->type == WIDGET_TYPE_BUTTON) {
            const ButtonWidget *buttonWidget = GET_WIDGET_PROPERTY(widget, specific, const ButtonWidget *);
            if (!get(widgetCursor.cursor, buttonWidget->enabled).getInt()) {
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

int AppContext::getActivePageStackPointer() {
    return m_updatePageIndex != -1 ? m_updatePageIndex : m_pageNavigationStackPointer;
}

int AppContext::getActivePageId() {
    return m_pageNavigationStack[getActivePageStackPointer()].pageId;
}

Page *AppContext::getActivePage() {
    return m_pageNavigationStack[getActivePageStackPointer()].page;
}

int AppContext::getPreviousPageId() {
    if (getActivePageStackPointer() == 0) {
        return PAGE_ID_NONE;
    }
    return m_pageNavigationStack[getActivePageStackPointer() - 1].pageId;
}

void AppContext::onPageChanged(int previousPageId, int activePageId) {
    eez::mcu::display::turnOn();
    eez::hmi::noteActivity();
    eez::mouse::onPageChanged();
    eez::keyboard::onPageChanged();
}

void AppContext::doShowPage(int pageId, Page *page, int previousPageId) {
#if CONF_OPTION_FPGA
    pageId = PAGE_ID_WELCOME_800X480;
    page = nullptr;
#endif

    page = page ? page : getPageFromIdHook(pageId);

    m_pageNavigationStack[m_pageNavigationStackPointer].page = page;
    m_pageNavigationStack[m_pageNavigationStackPointer].pageId = pageId;
    m_pageNavigationStack[m_pageNavigationStackPointer].displayBufferIndex = mcu::display::allocBuffer();

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
#if OPTION_GUI_THREAD
    if (osThreadGetId() != g_guiTaskHandle) {
        m_pageIdToSetOnNextIter = pageId;
        m_pageToSetOnNextIter = page;
        sendMessageToGuiThread(GUI_QUEUE_MESSAGE_TYPE_PUSH_PAGE, getAppContextId(this));
        return;
    }
#endif

    int previousPageId = getActivePageId();

    // advance stack pointre
    if (getActivePageId() != PAGE_ID_NONE && getActivePageId() != PAGE_ID_ASYNC_OPERATION_IN_PROGRESS && getActivePageId() != INTERNAL_PAGE_ID_TOAST_MESSAGE) {
        m_pageNavigationStackPointer++;
        assert (m_pageNavigationStackPointer < CONF_GUI_PAGE_NAVIGATION_STACK_SIZE);
    }

    doShowPage(pageId, page, previousPageId);
}

void AppContext::doShowPage() {
    setPage(m_pageIdToSetOnNextIter);
    
    if (m_pageIdToSetOnNextIter == PAGE_ID_WELCOME) {
        playPowerUp(sound::PLAY_POWER_UP_CONDITION_WELCOME_PAGE_IS_ACTIVE);
    } 
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

void AppContext::showPage(int pageId) {
#if OPTION_GUI_THREAD    
    if (osThreadGetId() != g_guiTaskHandle) {
        m_pageIdToSetOnNextIter = pageId;
        m_pageToSetOnNextIter = nullptr;
        sendMessageToGuiThread(GUI_QUEUE_MESSAGE_TYPE_SHOW_PAGE, getAppContextId(this));
        return;
    }
#endif
    if (pageId != getActivePageId()) {
        setPage(pageId);
    }
}

void AppContext::doPushPage() {
    pushPage(m_pageIdToSetOnNextIter, m_pageToSetOnNextIter);
}

////////////////////////////////////////////////////////////////////////////////

bool AppContext::testExecuteActionOnTouchDown(int action) {
    return false;
}

bool AppContext::isBlinking(const Cursor cursor, int16_t id) {
    return false;
}

bool AppContext::isActiveWidget(const WidgetCursor &widgetCursor) {
    return false;
}

bool AppContext::canExecuteActionWhenTouchedOutsideOfActivePage(int pageId, int action) {
    return false;
}

void AppContext::onPageTouch(const WidgetCursor &foundWidget, Event &touchEvent) {
    int activePageId = getActivePageId();
    if (activePageId != PAGE_ID_NONE && !isPageInternal(activePageId)) {
        const Widget *page = getPageWidget(activePageId);
		const PageWidget *pageSpecific = GET_WIDGET_PROPERTY(page, specific, const PageWidget *);
        if ((pageSpecific->flags & CLOSE_PAGE_IF_TOUCHED_OUTSIDE_FLAG) != 0) {
            if (!pointInsideRect(touchEvent.x, touchEvent.y, foundWidget.appContext->rect.x + page->x, foundWidget.appContext->rect.y + page->y, page->w, page->h)) {
                int activePageId = getActivePageId();
                
                popPage();

                auto widgetCursor = findWidget(&getRootAppContext(), touchEvent.x, touchEvent.y);

                if (
                    widgetCursor.widget &&
                    widgetCursor.widget->action != ACTION_ID_NONE &&
                    canExecuteActionWhenTouchedOutsideOfActivePage(activePageId, widgetCursor.widget->action)
                ) {
                    eventHandling();
                }
            }
        }
    }
}

////////////////////////////////////////////////////////////////////////////////

void AppContext::updatePage(int i, WidgetCursor &widgetCursor) {
    if (!isPageFullyCovered(i)) {
        if (m_pageNavigationStack[i].displayBufferIndex == -1) {
            m_pageNavigationStack[i].displayBufferIndex = mcu::display::allocBuffer();
            widgetCursor.previousState = nullptr;
        }

		mcu::display::selectBuffer(m_pageNavigationStack[i].displayBufferIndex);

		m_updatePageIndex = i;

		widgetCursor.cursor = -1;

        int x;
        int y;
        int width;
        int height;
        bool withShadow;

        if (isPageInternal(m_pageNavigationStack[i].pageId)) {
            InternalPage *internalPage = ((InternalPage *)m_pageNavigationStack[i].page);
            
            if (!widgetCursor.previousState) {
                internalPage->refresh(widgetCursor);
            }
            internalPage->updatePage(widgetCursor);

            x = internalPage->x;
            y = internalPage->y;
            width = internalPage->width;
            height = internalPage->height;
            withShadow = true;
        } else {
            const Widget *page = getPageWidget(m_pageNavigationStack[i].pageId);

            auto savedPreviousState = widgetCursor.previousState;
            auto savedWidget = widgetCursor.widget;

            x = widgetCursor.x + page->x;
            y = widgetCursor.y + page->y;
            width = page->w;
            height = page->h;
            withShadow = page->x > 0;

            if (!widgetCursor.previousState) {
                // clear background
                const Style* style = getStyle(page->style);
                mcu::display::setColor(style->background_color);
                mcu::display::fillRect(x, y, x + width - 1, y + height - 1);
            }

            widgetCursor.widget = page;
            enumWidget(widgetCursor, drawWidgetCallback);

            widgetCursor.widget = savedWidget;
            widgetCursor.previousState = savedPreviousState;
        }
		
		mcu::display::setBufferBounds(m_pageNavigationStack[i].displayBufferIndex, x, y, width, height, withShadow, 255, 0, 0, withShadow && activePageHasBackdropHook() ? &rect : nullptr);

		widgetCursor.nextState();

		m_updatePageIndex = -1;
	} else {
        m_pageNavigationStack[i].displayBufferIndex = -1;
    }
}

bool isRect1FullyCoveredByRect2(int xRect1, int yRect1, int wRect1, int hRect1, int xRect2, int yRect2, int wRect2, int hRect2) {
    return xRect2 <= xRect1 && 
        yRect2 <= yRect1 &&
        xRect2 + wRect2 >= xRect1 + wRect1 &&
        yRect2 + hRect2 >= yRect1 + hRect1;
}

void getPageRect(int pageId, Page *page, int &x, int &y, int &w, int &h) {
    if (isPageInternal(pageId)) {
        x = ((InternalPage *)page)->x;
        y = ((InternalPage *)page)->y;
        w = ((InternalPage *)page)->width;
        h = ((InternalPage *)page)->height;
    } else {
        const Widget *page = getPageWidget(pageId);
        x = page->x;
        y = page->y;
        w = page->w;
        h = page->h;
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

void AppContext::updateAppView(WidgetCursor &widgetCursor) {
    if (getActivePageId() == PAGE_ID_NONE) {
        return;
    }

    for (int i = 0; i <= m_pageNavigationStackPointer; i++) {
        updatePage(i, widgetCursor);
    }
}

int AppContext::getLongTouchActionHook(const WidgetCursor &widgetCursor) {
    return ACTION_ID_NONE;
}

int AppContext::getExtraLongTouchActionHook(const WidgetCursor &widgetCursor) {
    return ACTION_ID_NONE;
}

} // namespace gui
} // namespace eez

#endif
