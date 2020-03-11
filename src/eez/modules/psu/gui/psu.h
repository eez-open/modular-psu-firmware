/*
* EEZ PSU Firmware
* Copyright (C) 2018-present, Envox d.o.o.
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

#include <eez/gui/gui.h>

#include <eez/modules/psu/gui/numeric_keypad.h>

using namespace eez::gui;

namespace eez {
namespace psu {

class Channel;

namespace gui {

void channelToggleOutput();
void channelInitiateTrigger();
void channelSetToFixed();
void channelEnableOutput();

void selectChannel(Channel *channel = nullptr);
extern Channel *g_channel;
extern int g_channelIndex;

extern data::Cursor g_focusCursor;
extern int16_t g_focusDataId;
extern data::Value g_focusEditValue;
void setFocusCursor(const data::Cursor &cursor, int16_t dataId);
bool isFocusChanged();

void changeVoltageLimit(int iChannel);
void changeCurrentLimit(int iChannel);
void changePowerLimit(int iChannel);
void changePowerTripLevel(int iChannel);
void changePowerTripDelay(int iChannel);
void changeTemperatureTripLevel(int iChannel);
void changeTemperatureTripDelay(int iChannel);

void psuErrorMessage(const data::Cursor &cursor, data::Value value, void (*ok_callback)() = 0);

Unit getCurrentEncoderUnit();

float encoderIncrement(data::Value value, int counter, float min, float max, int channelIndex, float precision = 1.0f);

bool isEncoderEnabledInActivePage();

void lockFrontPanel();
void unlockFrontPanel();
bool isFrontPanelLocked();

void showWelcomePage();
void showStandbyPage();
void showEnteringStandbyPage();
void showSavingPage();
void showShutdownPage();

void showAsyncOperationInProgress(const char *message, void (*checkStatus)() = 0);
void hideAsyncOperationInProgress();
extern data::Value g_progress;

struct TextInputParams {
    size_t m_minChars;
    size_t m_maxChars;
    const char *m_input;
    static void onSet(char *);
    static void onCancel();
};

struct NumberInputParams {
    NumericKeypadOptions m_options;
    float m_input;
    static void onSet(float value);
    static void onCancel();
};

struct MenuInputParams {
    MenuType m_type;
    const char **m_items;
    int m_input;
    static void onSet(int value);
};

class PsuAppContext : public AppContext {
public:
    PsuAppContext();

    void stateManagment() override;

    bool isActiveWidget(const WidgetCursor &widgetCursor) override;

    bool isFocusWidget(const WidgetCursor &widgetCursor) override;

    bool isBlinking(const data::Cursor &cursor, int16_t id) override;

    bool isWidgetActionEnabled(const WidgetCursor &widgetCursor) override;
    
    static void showProgressPage(const char *message, void (*abortCallback)() = 0);
    static void showProgressPageWithoutAbort(const char *message);
    static bool updateProgressPage(size_t processedSoFar, size_t totalSize);
    static void hideProgressPage();

    static void setTextMessage(const char *message, unsigned int len);
    static void clearTextMessage();
    static const char *getTextMessage();
    static uint8_t getTextMessageVersion();

    void showUncaughtScriptExceptionMessage();

    const char *textInput(const char *label, size_t minChars, size_t maxChars, const char *value);
    void doShowTextInput();
    
    float numberInput(const char *label, Unit unit, float min, float max, float value);
    void doShowNumberInput();
    
    int menuInput(const char *label, MenuType menuType, const char **menuItems);
    void doShowMenuInput();

    void dialogOpen();
    const char *dialogAction(uint32_t timeoutMs);
    void dialogResetDataItemValues();
    void dialogSetDataItemValue(int16_t dataId, Value& value);
    void dialogClose();

protected:
    bool m_pushProgressPage;
    const char *m_progressMessage;
    bool m_progressWithoutAbort;
    void (*m_progressAbortCallback)();
    bool m_popProgressPage;

    char m_textMessage[32 + 1];
    uint8_t m_textMessageVersion;
    bool m_showTextMessage;
    bool m_clearTextMessage;

    bool m_showUncaughtScriptExceptionMessage;

    const char *m_inputLabel;

    friend struct TextInputParams;
    TextInputParams m_textInputParams;
    
    friend struct NumberInputParams;
    NumberInputParams m_numberInputParams;
    
    friend struct MenuInputParams;
    MenuInputParams m_menuInputParams;

    bool m_inputReady;

    int getMainPageId() override;
    void onPageChanged(int previousPageId, int activePageId) override;
    bool isAutoRepeatAction(int action) override;
    void onPageTouch(const WidgetCursor &foundWidget, Event &touchEvent) override;
    bool testExecuteActionOnTouchDown(int action) override;
    bool canExecuteActionWhenTouchedOutsideOfActivePage(int pageId, int action);
    void updatePage(int i, WidgetCursor &widgetCursor);

private:
    void doShowProgressPage();
    void doHideProgressPage();
};

extern PsuAppContext g_psuAppContext;

static const uint16_t g_ytGraphStyles[] = {
    STYLE_ID_YT_GRAPH_Y1,
    STYLE_ID_YT_GRAPH_Y2,
    STYLE_ID_YT_GRAPH_Y3,
    STYLE_ID_YT_GRAPH_Y4
};

enum {
    GUI_QUEUE_MESSAGE_TYPE_LISTS_PAGE_IMPORT_LIST_FINISHED = 100,
    GUI_QUEUE_MESSAGE_TYPE_LISTS_PAGE_EXPORT_LIST_FINISHED,
    GUI_QUEUE_MESSAGE_TYPE_USER_PROFILES_PAGE_ASYNC_OPERATION_FINISHED,
    GUI_QUEUE_MESSAGE_TYPE_SHOW_TEXT_INPUT,
    GUI_QUEUE_MESSAGE_TYPE_SHOW_NUMBER_INPUT,
    GUI_QUEUE_MESSAGE_TYPE_SHOW_MENU_INPUT,
    GUI_QUEUE_MESSAGE_TYPE_DIALOG_OPEN,
    GUI_QUEUE_MESSAGE_TYPE_DIALOG_CLOSE
};

} // namespace gui
} // namespace psu
} // namespace eez
