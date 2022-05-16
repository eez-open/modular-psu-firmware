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
#define CONF_MAX_STATE_SIZE (GUI_STATE_BUFFER_SIZE / 2)

#define APP_CONTEXT_ID_DEVICE 0
#define APP_CONTEXT_ID_SIMULATOR_FRONT_PANEL 1

namespace eez {
namespace gui {

class Page;

struct PageOnStack {
    int pageId = PAGE_ID_NONE;
    Page *page = nullptr;
    int displayBufferIndex = -1;
};

class ToastMessagePage;

class AppContext {
    friend struct AppViewWidgetState;

public:
	Rect rect;

    AppContext();

    virtual void stateManagment();

    void showPage(int pageId);
    void pushPage(int pageId, Page *page = nullptr);
    void popPage();
    void removePageFromStack(int pageId);

    int getActivePageStackPointer() {
        return m_updatePageIndex != -1 ? m_updatePageIndex : m_pageNavigationStackPointer;
    }

    int getActivePageId() {
        return m_pageNavigationStack[getActivePageStackPointer()].pageId;
    }

    Page *getActivePage() {
        return m_pageNavigationStack[getActivePageStackPointer()].page;
    }

    bool isActivePageInternal();

    int getPreviousPageId() {
        int index = getActivePageStackPointer();
        return index == 0 ? PAGE_ID_NONE : m_pageNavigationStack[index - 1].pageId;
    }

    void replacePage(int pageId, Page *page = nullptr);

    Page *getPage(int pageId);
    bool isPageOnStack(int pageId);
    bool isExternalPageOnStack();
	void removeExternalPagesFromTheStack();

    int getNumPagesOnStack() {
        return m_pageNavigationStackPointer + 1;
    }

    virtual bool isFocusWidget(const WidgetCursor &widgetCursor);

    virtual bool isBlinking(const WidgetCursor &widgetCursor, int16_t id);

    virtual void onPageTouch(const WidgetCursor &foundWidget, Event &touchEvent);

    virtual bool testExecuteActionOnTouchDown(int action);

    virtual bool isAutoRepeatAction(int action);

    virtual bool isWidgetActionEnabled(const WidgetCursor &widgetCursor);

    virtual int getLongTouchActionHook(const WidgetCursor &widgetCursor);

    void infoMessage(const char *message);
    void infoMessage(Value value);
    void infoMessage(const char *message, void (*action)(), const char *actionLabel);
    void errorMessage(const char *message, bool autoDismiss = false);
    void errorMessage(Value value);
    void errorMessageWithAction(Value value, void (*action)(int param), const char *actionLabel, int actionParam);
    void errorMessageWithAction(const char *message, void (*action)(), const char *actionLabel);

    void yesNoDialog(int yesNoPageId, const char *message, void (*yes_callback)(), void (*no_callback)(), void (*cancel_callback)());
    void yesNoDialog(int yesNoPageId, Value value, void(*yes_callback)(), void(*no_callback)(), void(*cancel_callback)());

	// TODO these should be private
	void(*m_dialogYesCallback)();
	void(*m_dialogNoCallback)();
	void(*m_dialogCancelCallback)();
	void(*m_dialogLaterCallback)();

    virtual int getMainPageId() = 0;

protected:
    PageOnStack m_pageNavigationStack[CONF_GUI_PAGE_NAVIGATION_STACK_SIZE];
    int m_pageNavigationStackPointer = 0;
    int m_updatePageIndex;

    uint32_t m_showPageTime;

    virtual void onPageChanged(int previousPageId, int activePageId);

    void doShowPage(int index, Page *page, int previousPageId);
    void setPage(int pageId);

    void updatePage(int i, WidgetCursor &widgetCursor);
    virtual void pageRenderCustom(int i, WidgetCursor &widgetCursor);

    void getPageRect(int pageId, Page *page, int &x, int &y, int &w, int &h);
    bool isPageFullyCovered(int pageNavigationStackIndex);

    virtual bool canExecuteActionWhenTouchedOutsideOfActivePage(int pageId, int action);

    void pushToastMessage(ToastMessagePage *toastMessage);
};

AppContext *getRootAppContext();

} // namespace gui
} // namespace eez
