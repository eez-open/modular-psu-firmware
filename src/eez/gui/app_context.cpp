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

#include <eez/index.h>
#include <eez/sound.h>
#include <eez/system.h>

#include <eez/gui/gui.h>
#include <eez/gui/widgets/button.h>

#include <eez/modules/psu/psu.h>
#include <eez/modules/psu/idle.h>

#define CONF_GUI_TOAST_DURATION_MS 2000L

namespace eez {
namespace gui {

AppContext *g_appContext;

////////////////////////////////////////////////////////////////////////////////

AppContext::AppContext() {
    m_nextIterOperation = NEXT_ITER_OPERATION_NONE;
    m_updatePageIndex = -1;
}


void AppContext::stateManagment() {
    // remove alert message after period of time
    uint32_t inactivityPeriod = psu::idle::getHmiInactivityPeriod();
    if (getActivePageId() == INTERNAL_PAGE_ID_TOAST_MESSAGE) {
        ToastMessagePage *page = (ToastMessagePage *)getActivePage();
        if (!page->hasAction() && inactivityPeriod >= CONF_GUI_TOAST_DURATION_MS) {
            popPage();
        }
    }

    if (m_nextIterOperation == NEXT_ITER_OPERATION_SET) {
        setPage(m_pageIdToSetOnNextIter);
        if (m_pageIdToSetOnNextIter == PAGE_ID_WELCOME) {
            playPowerUp(sound::PLAY_POWER_UP_CONDITION_WELCOME_PAGE_IS_ACTIVE);
        } 
        m_nextIterOperation = NEXT_ITER_OPERATION_NONE;
    } else if (m_nextIterOperation == NEXT_ITER_OPERATION_PUSH) {
        pushPage(m_pageIdToSetOnNextIter, m_pageToSetOnNextIter);
        m_nextIterOperation = NEXT_ITER_OPERATION_NONE;
    }

    // call m_checkAsyncOperationStatus
    if (getActivePageId() == PAGE_ID_ASYNC_OPERATION_IN_PROGRESS) {
        if (m_checkAsyncOperationStatus) {
            m_checkAsyncOperationStatus();
        }
    }
}

////////////////////////////////////////////////////////////////////////////////

bool AppContext::isActivePageInternal() {
    return isPageInternal(getActivePageId());
}

bool AppContext::isWidgetActionEnabled(const WidgetCursor &widgetCursor) {
    const Widget *widget = widgetCursor.widget;
    if (widget->action) {
        AppContext *saved = g_appContext;
        g_appContext = this;

        if (widget->type == WIDGET_TYPE_BUTTON) {
            const ButtonWidget *buttonWidget = GET_WIDGET_PROPERTY(widget, specific, const ButtonWidget *);
            if (!data::get(widgetCursor.cursor, buttonWidget->enabled).getInt()) {
                g_appContext = saved;
                return false;
            }
        }

        g_appContext = saved;
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

void AppContext::onPageChanged(int previousPageId, int activePageId) {
    eez::mcu::display::turnOn();
    psu::idle::noteHmiActivity();
}

void AppContext::doShowPage(int pageId, Page *page, int previousPageId) {
    page = page ? page : getPageFromIdHook(pageId);

    m_pageNavigationStack[m_pageNavigationStackPointer].page = page;
    m_pageNavigationStack[m_pageNavigationStackPointer].pageId = pageId;
#if OPTION_SDRAM
    m_pageNavigationStack[m_pageNavigationStackPointer].displayBufferIndex = mcu::display::allocBuffer();
#endif

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
    if (osThreadGetId() != g_guiTaskHandle) {
        pushPageOnNextIter(pageId, page);
    } else {
        int previousPageId = getActivePageId();

        // advance stack pointre
        if (getActivePageId() != INTERNAL_PAGE_ID_NONE) {
            m_pageNavigationStackPointer++;
            assert (m_pageNavigationStackPointer < CONF_GUI_PAGE_NAVIGATION_STACK_SIZE);
        }

        doShowPage(pageId, page, previousPageId);
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
	if (pageId != getActivePageId()) {
		setPage(pageId);
	}
}

void AppContext::showPageOnNextIter(int pageId, Page *page) {
    m_pageIdToSetOnNextIter = pageId;
    m_pageToSetOnNextIter = page;
    m_nextIterOperation = NEXT_ITER_OPERATION_SET;
}

void AppContext::pushPageOnNextIter(int pageId, Page *page) {
    m_pageIdToSetOnNextIter = pageId;
    m_pageToSetOnNextIter = page;
    m_nextIterOperation = NEXT_ITER_OPERATION_PUSH;
}

void AppContext::pushSelectFromEnumPage(const data::EnumItem *enumDefinition, uint16_t currentValue,
                                        bool (*disabledCallback)(uint16_t value),
                                        void (*onSet)(uint16_t),
                                        bool smallFont) {
	m_selectFromEnumPage.init(enumDefinition, currentValue, disabledCallback, onSet, smallFont);
    pushPage(INTERNAL_PAGE_ID_SELECT_FROM_ENUM, &m_selectFromEnumPage);
}

void AppContext::pushSelectFromEnumPage(void (*enumDefinitionFunc)(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value),
                                        uint16_t currentValue, bool (*disabledCallback)(uint16_t value), void (*onSet)(uint16_t),
                                        bool smallFont) {
	m_selectFromEnumPage.init(enumDefinitionFunc, currentValue, disabledCallback, onSet, smallFont);
    pushPage(INTERNAL_PAGE_ID_SELECT_FROM_ENUM, &m_selectFromEnumPage);
}

////////////////////////////////////////////////////////////////////////////////

bool AppContext::testExecuteActionOnTouchDown(int action) {
    return false;
}

bool AppContext::isBlinking(const data::Cursor &cursor, uint16_t id) {
    return false;
}

bool AppContext::isActiveWidget(const WidgetCursor &widgetCursor) {
    return false;
}

void AppContext::onPageTouch(const WidgetCursor &foundWidget, Event &touchEvent) {
    int activePageId = getActivePageId();
    if (!isPageInternal(activePageId)) {
        const Widget *page = getPageWidget(activePageId);
		const PageWidget *pageSpecific = GET_WIDGET_PROPERTY(page, specific, const PageWidget *);
        if ((pageSpecific->flags & CLOSE_PAGE_IF_TOUCHED_OUTSIDE_FLAG) != 0) {
            if (!pointInsideRect(touchEvent.x, touchEvent.y, g_appContext->x + page->x, g_appContext->y + page->y, page->w, page->h)) {
                popPage();
            }
        }
    }
}

////////////////////////////////////////////////////////////////////////////////

void AppContext::updatePage(int i, WidgetCursor &widgetCursor) {
    m_updatePageIndex = i;

#if OPTION_SDRAM
    mcu::display::selectBuffer(m_pageNavigationStack[i].displayBufferIndex);
#endif

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

#if OPTION_SDRAM
    mcu::display::setBufferBounds(m_pageNavigationStack[i].displayBufferIndex, x, y, width, height, withShadow);
#endif

    m_updatePageIndex = -1;
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
    if (getActivePageId() == INTERNAL_PAGE_ID_NONE) {
        return;
    }

    for (int i = 0; i <= m_pageNavigationStackPointer; i++) {
        if (!isPageFullyCovered(i)) {
            widgetCursor.cursor = Cursor();
            updatePage(i, widgetCursor);
            widgetCursor.nextState();
        }
    }
}

int AppContext::getLongTouchActionHook(const WidgetCursor &widgetCursor) {
    return ACTION_ID_NONE;
}

const data::EnumItem *AppContext::getActiveSelectEnumDefinition() {
    if (getActivePageId() == INTERNAL_PAGE_ID_SELECT_FROM_ENUM) {
        return m_selectFromEnumPage.getEnumDefinition();
    }
    return nullptr;
}

} // namespace gui
} // namespace eez

#endif
