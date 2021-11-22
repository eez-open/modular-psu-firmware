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

#pragma once

#define CONF_GUI_PAGE_NAVIGATION_STACK_SIZE 10
#define CONF_MAX_STATE_SIZE (16 * 1024)

namespace eez {
namespace gui {

struct PageOnStack {
    int pageId = PAGE_ID_NONE;
    Page *page = nullptr;
    int displayBufferIndex = -1;
};

class AppContext {
public:
	Rect rect;

    AppContext();

    virtual void stateManagment();

    void showPage(int pageId);
    void doShowPage();
    
    void pushPage(int pageId, Page *page = nullptr);
    void doPushPage();
    
    void popPage();
    void removePageFromStack(int pageId);

    int getActivePageId();
    Page *getActivePage();

    bool isActivePageInternal();

    int getPreviousPageId();

    void replacePage(int pageId, Page *page = nullptr);

    Page *getPage(int pageId);
    bool isPageOnStack(int pageId);
    int getNumPagesOnStack() {
        return m_pageNavigationStackPointer + 1;
    }

    virtual bool isFocusWidget(const WidgetCursor &widgetCursor);

    virtual bool isBlinking(const WidgetCursor &widgetCursor, int16_t id);

    virtual bool isActiveWidget(const WidgetCursor &widgetCursor);
    virtual void onPageTouch(const WidgetCursor &foundWidget, Event &touchEvent);

    virtual bool testExecuteActionOnTouchDown(int action);

    virtual bool isAutoRepeatAction(int action);

    virtual bool isWidgetActionEnabled(const WidgetCursor &widgetCursor);

    void updateAppView(WidgetCursor &widgetCursor);

    virtual int getLongTouchActionHook(const WidgetCursor &widgetCursor);
    virtual int getExtraLongTouchActionHook(const WidgetCursor &widgetCursor);

protected:
    PageOnStack m_pageNavigationStack[CONF_GUI_PAGE_NAVIGATION_STACK_SIZE];
    int m_pageNavigationStackPointer = 0;
    int m_activePageIndex;
    int m_updatePageIndex;

    int m_pageIdToSetOnNextIter;
    Page *m_pageToSetOnNextIter;

    uint32_t m_showPageTime;

    virtual int getMainPageId() = 0;
    virtual void onPageChanged(int previousPageId, int activePageId);

    void doShowPage(int index, Page *page, int previousPageId);
    void setPage(int pageId);

    int getActivePageStackPointer();

    virtual void updatePage(int i, WidgetCursor &widgetCursor);

    bool isPageFullyCovered(int pageNavigationStackIndex);
    
    virtual bool canExecuteActionWhenTouchedOutsideOfActivePage(int pageId, int action);
};

AppContext &getRootAppContext();

} // namespace gui
} // namespace eez
