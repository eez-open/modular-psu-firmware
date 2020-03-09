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

#define CONF_GUI_PAGE_NAVIGATION_STACK_SIZE 5
#define CONF_MAX_STATE_SIZE (10 * 1024)

namespace eez {
namespace gui {

struct PageOnStack {
    int pageId = INTERNAL_PAGE_ID_NONE;
    Page *page = nullptr;
#if OPTION_SDRAM    
    int displayBufferIndex = -1;
#endif
};

class AppContext {
public:
	int x;
	int y;
	int width;
	int height;

    AppContext();

    // TODO these should be private
    uint32_t m_showPageTime;
    void (*m_dialogYesCallback)();
    void (*m_dialogNoCallback)();
    void (*m_dialogCancelCallback)();
    void (*m_dialogLaterCallback)();
    void(*m_toastAction)(int param);
    int m_toastActionParam;
    void(*m_toastActionWithoutParam)();
    void (*m_checkAsyncOperationStatus)();

    virtual void stateManagment();

    void showPage(int pageId);
    void doShowPage();
    
    void pushPage(int pageId, Page *page = nullptr);
    void doPushPage();
    
    void popPage();

    int getActivePageId();
    Page *getActivePage();

    bool isActivePageInternal();

    void pushSelectFromEnumPage(const data::EnumItem *enumDefinition, uint16_t currentValue, bool (*disabledCallback)(uint16_t value), void (*onSet)(uint16_t), bool smallFont, bool showRadioButtonIcon);
    void pushSelectFromEnumPage(void (*enumDefinitionFunc)(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value), uint16_t currentValue, bool (*disabledCallback)(uint16_t value), void (*onSet)(uint16_t), bool smallFont, bool showRadioButtonIcon);

    void replacePage(int pageId, Page *page = nullptr);

    Page *getPage(int pageId);
    int getNumPagesOnStack();
    bool isPageOnStack(int pageId);
    int getNumPagesOnStack() {
        return m_pageNavigationStackPointer + 1;
    }

    virtual bool isFocusWidget(const WidgetCursor &widgetCursor);

    virtual bool isBlinking(const data::Cursor &cursor, uint16_t id);

    virtual bool isActiveWidget(const WidgetCursor &widgetCursor);
    virtual void onPageTouch(const WidgetCursor &foundWidget, Event &touchEvent);

    virtual bool testExecuteActionOnTouchDown(int action);

    virtual bool isAutoRepeatAction(int action);

    virtual bool isWidgetActionEnabled(const WidgetCursor &widgetCursor);

    void updateAppView(WidgetCursor &widgetCursor);

    virtual int getLongTouchActionHook(const WidgetCursor &widgetCursor);

    const data::EnumItem *getActiveSelectEnumDefinition();

protected:
    PageOnStack m_pageNavigationStack[CONF_GUI_PAGE_NAVIGATION_STACK_SIZE];
    int m_pageNavigationStackPointer = 0;
    int m_activePageIndex;
    int m_updatePageIndex;

    int m_pageIdToSetOnNextIter;
    Page *m_pageToSetOnNextIter;

    SelectFromEnumPage m_selectFromEnumPage;

    virtual int getMainPageId() = 0;
    virtual void onPageChanged(int previousPageId, int activePageId);

    void doShowPage(int index, Page *page, int previousPageId);
    void setPage(int pageId);

    int getActivePageStackPointer();

    virtual void updatePage(int i, WidgetCursor &widgetCursor);

    bool isPageFullyCovered(int pageNavigationStackIndex);
    
    virtual bool canExecuteActionWhenTouchedOutsideOfActivePage(int pageId, int action);
}; // namespace gui

extern AppContext *g_appContext;

AppContext &getRootAppContext();

} // namespace gui
} // namespace eez
