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

#include <stdlib.h>

#include <eez/gui/gui.h>
#include <eez/gui/page.h>
#include <eez/modules/mcu/display.h>

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
    char m_textMessage[32 + 1];
    uint8_t m_textMessageVersion;
    void (*m_dialogYesCallback)();
    void (*m_dialogNoCallback)();
    void (*m_dialogCancelCallback)();
    void (*m_dialogLaterCallback)();
    void(*m_toastAction)(int param);
    int m_toastActionParam;
    void (*m_checkAsyncOperationStatus)();

    virtual void stateManagment();

    void showPage(int pageId);
    void showPageOnNextIter(int pageId);
    void pushPage(int pageId, Page *page = 0);
    void popPage();

    int getActivePageId();
    Page *getActivePage();

    bool isActivePageInternal();
    InternalPage *getActivePageInternal() {
        return (InternalPage *)getActivePage();
    }

    void pushSelectFromEnumPage(const data::EnumItem *enumDefinition, uint16_t currentValue,
                                bool (*disabledCallback)(uint16_t value), void (*onSet)(uint16_t),
                                bool smallFont);
    void pushSelectFromEnumPage(void (*enumDefinitionFunc)(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value), 
                                uint16_t currentValue, bool (*disabledCallback)(uint16_t value), void (*onSet)(uint16_t),
                                bool smallFont);

    void replacePage(int pageId, Page *page = nullptr);

    Page *getPage(int pageId);
    bool isPageOnStack(int pageId);

    virtual bool isFocusWidget(const WidgetCursor &widgetCursor);

    virtual bool isBlinking(const data::Cursor &cursor, uint16_t id);

    virtual uint32_t getNumHistoryValues(uint16_t id);
    virtual uint32_t getCurrentHistoryValuePosition(const Cursor &cursor, uint16_t id);
    virtual Value getHistoryValue(const Cursor &cursor, uint16_t id, uint32_t position);

    virtual bool isActiveWidget(const WidgetCursor &widgetCursor);
    virtual void onPageTouch(const WidgetCursor &foundWidget, Event &touchEvent);

    virtual bool testExecuteActionOnTouchDown(int action);

    virtual bool isAutoRepeatAction(int action);

    bool isWidgetActionEnabled(const WidgetCursor &widgetCursor);

    void updateAppView(WidgetCursor &widgetCursor);

    virtual int getLongTouchActionHook(const WidgetCursor &widgetCursor);

    int getNumPagesOnStack() {
        return m_pageNavigationStackPointer + 1;
    }

    void infoMessage(const char *message);

    const data::EnumItem *getActiveSelectEnumDefinition();

protected:
    PageOnStack m_pageNavigationStack[CONF_GUI_PAGE_NAVIGATION_STACK_SIZE];
    int m_pageNavigationStackPointer = 0;
    int m_activePageIndex;
    int m_updatePageIndex;

    bool m_setPageIdOnNextIter;
    int m_pageIdToSetOnNextIter;

    SelectFromEnumPage m_selectFromEnumPage;

    const char *m_showInfoMessageOnNextIter;
      
    virtual int getMainPageId() = 0;
    virtual void onPageChanged(int previousPageId, int activePageId);

    void doShowPage(int index, Page *page, int previousPageId);
    void setPage(int pageId);

    int getActivePageStackPointer();

    void updatePage(int i, WidgetCursor &widgetCursor);

    bool isPageFullyCovered(int pageNavigationStackIndex);
}; // namespace gui

extern AppContext *g_appContext;

AppContext &getRootAppContext();

} // namespace gui
} // namespace eez
