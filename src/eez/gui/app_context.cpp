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

#include <eez/gui/app_context.h>

// TODO this must be removed from here
#include <eez/apps/psu/psu.h>

#include <eez/apps/psu/devices.h>
#include <eez/apps/psu/idle.h>

#include <eez/apps/psu/gui/psu.h>
//

#include <eez/gui/assets.h>
#include <eez/gui/dialogs.h>
#include <eez/gui/gui.h>
#include <eez/gui/touch.h>
#include <eez/gui/widgets/button.h>
#include <eez/index.h>
#include <eez/modules/mcu/display.h>
#include <eez/sound.h>
#include <eez/system.h>

#define CONF_GUI_TOAST_DURATION_MS 2000L

namespace eez {
namespace gui {

AppContext *g_appContext;

////////////////////////////////////////////////////////////////////////////////

AppContext::AppContext() {
}

void AppContext::stateManagment() {
    // update throbber in progress info
    uint32_t tickCount = micros();
    if (getActivePageId() == PAGE_ID_ASYNC_OPERATION_IN_PROGRESS) {
        data::set(data::Cursor(), DATA_ID_ASYNC_OPERATION_THROBBER,
                  data::Value(g_throbber[(tickCount % 1000000) / 125000]), 0);
        if (m_checkAsyncOperationStatus) {
            m_checkAsyncOperationStatus();
        }
    }

    // remove alert message after period of time
    uint32_t inactivityPeriod = psu::idle::getGuiAndEncoderInactivityPeriod();
    if (getActivePageId() == INTERNAL_PAGE_ID_TOAST_MESSAGE) {
        if (inactivityPeriod >= CONF_GUI_TOAST_DURATION_MS) {
            dialogOk();
            return;
        }
    }

    // show next page
    if (m_nextShowPageId != INTERNAL_PAGE_ID_NONE) {
        if (m_nextShowPageId != m_activePageId) {
            setPage(m_nextShowPageId);
        }
        m_nextShowPageId = INTERNAL_PAGE_ID_NONE;
    }
}

////////////////////////////////////////////////////////////////////////////////

bool isPageInternal(int pageId) {
    return pageId < INTERNAL_PAGE_ID_NONE;
}

bool AppContext::isActivePageInternal() {
    return isPageInternal(m_activePageId);
}

bool AppContext::isWidgetActionEnabled(const WidgetCursor &widgetCursor) {
    const Widget *widget = widgetCursor.widget;
    if (widget->action) {
        AppContext *saved = g_appContext;
        g_appContext = this;

        if (isFrontPanelLocked()) {
            int activePageId = getActivePageId();
            if (activePageId == PAGE_ID_KEYPAD ||
                activePageId == PAGE_ID_TOUCH_CALIBRATION_YES_NO ||
                activePageId == PAGE_ID_TOUCH_CALIBRATION_YES_NO_CANCEL) {
                g_appContext = saved;
                return true;
            }
            if (widget->action != ACTION_ID_SYS_FRONT_PANEL_UNLOCK) {
                g_appContext = saved;
                return false;
            }
        }

        if (widget->type == WIDGET_TYPE_BUTTON) {
            const ButtonWidget *buttonWidget = (const ButtonWidget *)widget->specific;
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

int AppContext::transformStyle(const Widget *widget) {
    if (isFrontPanelLocked()) {
        if (widget->style == STYLE_ID_BOTTOM_BUTTON) {
            if (widget->action != ACTION_ID_SYS_FRONT_PANEL_UNLOCK) {
                return STYLE_ID_BOTTOM_BUTTON_DISABLED;
            }
        } else if (widget->style == STYLE_ID_EDIT_S) {
            return STYLE_ID_DEFAULT_S;
        } else if (widget->style == STYLE_ID_MON_VALUE) {
            return STYLE_ID_DEFAULT;
        } else if (widget->style == STYLE_ID_CHANNEL_OFF) {
            return STYLE_ID_CHANNEL_OFF_DISABLED;
        } else if (widget->style == STYLE_ID_EDIT_VALUE_ACTIVE_S_RIGHT) {
            return STYLE_ID_EDIT_VALUE_S_RIGHT;
        }
    }

    return widget->style;
}

////////////////////////////////////////////////////////////////////////////////

int AppContext::getActivePageId() {
    return m_activePageId;
}

bool AppContext::isActivePage(int pageId) {
    return pageId == m_activePageId;
}

Page *AppContext::getActivePage() {
    return m_activePage;
}

int AppContext::getPreviousPageId() {
    return m_previousPageId;
}

Page *AppContext::getPreviousPage() {
    if (m_pageNavigationStackPointer > 0) {
        return m_pageNavigationStack[m_pageNavigationStackPointer - 1].page;
    } else {
        return 0;
    }
}

void AppContext::onPageChanged() {
    eez::mcu::display::turnOn();
    psu::idle::noteGuiActivity();
}

void AppContext::doShowPage(int index, Page *page) {
    // delete current page
    if (m_activePage) {
        delete m_activePage;
    }

    m_previousPageId = m_activePageId;
    m_activePageId = index;
    m_repaintActivePage = true;

    if (page) {
        m_activePage = page;
    } else {
        m_activePage = createPageFromId(m_activePageId);
    }

    if (m_activePage) {
        m_activePage->pageWillAppear();
    }

    m_showPageTime = micros();

    // clear text message if active page is not PAGE_ID_TEXT_MESSAGE
    if (getActivePageId() != PAGE_ID_TEXT_MESSAGE && m_textMessage[0]) {
        m_textMessage[0] = 0;
    }
    onPageChanged();
}

void AppContext::setPage(int pageId) {
    // delete stack
    for (int i = 0; i < m_pageNavigationStackPointer; ++i) {
        if (m_pageNavigationStack[i].page) {
            delete m_pageNavigationStack[i].page;
        }
    }
    m_pageNavigationStackPointer = 0;

    //
    doShowPage(pageId);
}

void AppContext::replacePage(int pageId, Page *page) {
    doShowPage(pageId, page);
}

void AppContext::pushPage(int pageId, Page *page) {
    // push current page on stack
    if (m_activePageId != INTERNAL_PAGE_ID_NONE) {
        if (m_pageNavigationStackPointer == CONF_GUI_PAGE_NAVIGATION_STACK_SIZE) {
            // no more space on the stack

            // delete page on the bottom
            if (m_pageNavigationStack[0].page) {
                delete m_pageNavigationStack[0].page;
            }

            // move stack one down
            for (int i = 1; i < m_pageNavigationStackPointer; ++i) {
                m_pageNavigationStack[i - 1] = m_pageNavigationStack[i];
            }

            --m_pageNavigationStackPointer;
        }

        m_pageNavigationStack[m_pageNavigationStackPointer].pageId = m_activePageId;
        m_pageNavigationStack[m_pageNavigationStackPointer].page = m_activePage;
        m_pageNavigationStack[m_pageNavigationStackPointer].repaint = m_repaintActivePage;
        m_activePage = 0;
        ++m_pageNavigationStackPointer;
    }

    doShowPage(pageId, page);
}

void AppContext::popPage() {
    if (m_pageNavigationStackPointer > 0) {
        --m_pageNavigationStackPointer;

        doShowPage(m_pageNavigationStack[m_pageNavigationStackPointer].pageId,
                   m_pageNavigationStack[m_pageNavigationStackPointer].page);
    } else {
        doShowPage(getMainPageId());
    }
}

bool AppContext::isPageActiveOrOnStack(int pageId) {
    if (m_activePageId == pageId) {
        return true;
    }

    for (int i = 0; i < m_pageNavigationStackPointer; ++i) {
        if (m_pageNavigationStack[i].pageId == pageId) {
            return true;
        }
    }
    return false;
}

void AppContext::showPage(int pageId) {
    m_nextShowPageId = pageId;
}

void AppContext::pushSelectFromEnumPage(const data::EnumItem *enumDefinition, uint8_t currentValue,
                                        bool (*disabledCallback)(uint8_t value),
                                        void (*onSet)(uint8_t)) {
    pushPage(INTERNAL_PAGE_ID_SELECT_FROM_ENUM, new SelectFromEnumPage(enumDefinition, currentValue, disabledCallback, onSet));
}

void AppContext::pushSelectFromEnumPage(void (*enumDefinitionFunc)(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value),
                                        uint8_t currentValue, bool (*disabledCallback)(uint8_t value), void (*onSet)(uint8_t)) {
    pushPage(INTERNAL_PAGE_ID_SELECT_FROM_ENUM, new SelectFromEnumPage(enumDefinitionFunc, currentValue, disabledCallback, onSet));
}

void AppContext::markForRefreshAppView() {
    for (int i = 0; i < m_pageNavigationStackPointer; ++i) {
        m_pageNavigationStack[i].repaint = true;
    }

    m_repaintActivePage = true;
}

////////////////////////////////////////////////////////////////////////////////

bool AppContext::testExecuteActionOnTouchDown(int action) {
    return false;
}

uint16_t AppContext::getWidgetBackgroundColor(const WidgetCursor &widgetCursor,
                                              const Style *style) {
    return style->background_color;
}

bool AppContext::isBlinking(const data::Cursor &cursor, uint16_t id) {
    return false;
}

void AppContext::onScaleUpdated(int dataId, bool scaleIsVertical, int scaleWidth,
                                float scaleHeight) {
}

int AppContext::getNumHistoryValues(uint16_t id) {
    return 0;
}

int AppContext::getCurrentHistoryValuePosition(const Cursor &cursor, uint16_t id) {
    return 0;
}

Value AppContext::getHistoryValue(const Cursor &cursor, uint16_t id, int position) {
    return Value();
}

bool AppContext::isActiveWidget(const WidgetCursor &widgetCursor) {
    return false;
}

void AppContext::onPageTouch(const WidgetCursor &foundWidget, Event &touchEvent) {
}

////////////////////////////////////////////////////////////////////////////////

bool AppContext::updatePage(int pageId, bool repaint, WidgetCursor &widgetCursor) {

    bool painted;

    if (isPageInternal(pageId)) {
        if (repaint) {
            ((InternalPage *)g_appContext->getActivePage())->refresh();
        }
        painted = ((InternalPage *)g_appContext->getActivePage())->updatePage();
    } else {
		Widget *page = g_document->pages.first + pageId;

		auto savedPreviousState = widgetCursor.previousState;

        if (repaint) {
            // clear background

            const Style* style = getWidgetStyle(page);
            mcu::display::setColor(style->background_color);

            mcu::display::fillRect(
				widgetCursor.x + page->x,
				widgetCursor.y + page->y, 
				widgetCursor.x + page->x + page->w - 1,
				widgetCursor.y + page->y + page->h - 1);

            // if there is no previous state then all content will be refreshed
			widgetCursor.previousState = 0;
        }

        bool saved = g_painted;
        g_painted = false;

		auto savedWidget = widgetCursor.widget;

        widgetCursor.widget = page;

        enumWidget(widgetCursor, drawWidgetCallback);

		widgetCursor.widget = savedWidget;

		widgetCursor.previousState = savedPreviousState;

        painted = g_painted;
        if (!g_painted) {
            g_painted = saved;
        }
    }

    return painted;
}

bool pageContained(int xPageAbove, int yPageAbove, int wPageAbove, int hPageAbove,
		int xPageBelow, int yPageBelow, int wPageBelow, int hPageBelow) {
    return xPageAbove <= xPageBelow && yPageAbove <= yPageBelow &&
           xPageAbove + wPageAbove >= xPageBelow + wPageBelow &&
           yPageAbove + hPageAbove >= yPageBelow + hPageBelow;
}

void getPageRect(int pageId, Page *page, int &x, int &y, int &w, int &h) {
	if (isPageInternal(pageId)) {
		x = ((InternalPage *)page)->x;
		y = ((InternalPage *)page)->y;
		w = ((InternalPage *)page)->width;
		h = ((InternalPage *)page)->height;
	} else {
		Widget *page = g_document->pages.first + pageId;
		x = page->x;
		y = page->y;
		w = page->w;
		h = page->h;
	}
}

void AppContext::updateAppView(WidgetCursor &widgetCursor) {
    if (m_activePageId == INTERNAL_PAGE_ID_NONE) {
        return;
    }

    int i;

    for (i = m_pageNavigationStackPointer - 1; i >= 0; i--) {
    	int xPageAbove, yPageAbove, wPageAbove, hPageAbove;
    	getPageRect(i == m_pageNavigationStackPointer - 1 ? m_activePageId : m_pageNavigationStack[i + 1].pageId,
    			i == m_pageNavigationStackPointer - 1 ? m_activePage : m_pageNavigationStack[i + 1].page,
    			xPageAbove, yPageAbove, wPageAbove, hPageAbove);

    	int xPageBelow, yPageBelow, wPageBelow, hPageBelow;
    	getPageRect(m_pageNavigationStack[i].pageId, m_pageNavigationStack[i].page,
    			xPageBelow, yPageBelow, wPageBelow, hPageBelow);

        if (pageContained(xPageAbove, yPageAbove, wPageAbove, hPageAbove,
        		xPageBelow, yPageBelow, wPageBelow, hPageBelow)) {
            break;
        }
    }

    bool repaint = false;

    for (++i; i < m_pageNavigationStackPointer; i++) {
        repaint = updatePage(m_pageNavigationStack[i].pageId, m_pageNavigationStack[i].repaint || repaint, widgetCursor);
        m_pageNavigationStack[i].repaint = false;

        if (widgetCursor.previousState) {
			widgetCursor.previousState = nextWidgetState(widgetCursor.previousState);
        }
        if (widgetCursor.currentState)
			widgetCursor.currentState = nextWidgetState(widgetCursor.currentState);
    }

    updatePage(m_activePageId, m_repaintActivePage || repaint, widgetCursor);
    m_repaintActivePage = false;
}

} // namespace gui
} // namespace eez
