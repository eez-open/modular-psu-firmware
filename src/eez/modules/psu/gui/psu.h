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

using namespace eez::gui;

#include <eez/modules/psu/gui/data.h>
#include <eez/modules/psu/gui/keypad.h>
#include <eez/modules/psu/gui/page.h>

namespace eez {
namespace psu {

struct Channel;

namespace gui {

void channelToggleOutput();

void selectChannel(Channel *channel);
void selectChannelByCursor();
extern Channel *g_channel;
extern int g_channelIndex;

extern Cursor g_focusCursor;
extern int16_t g_focusDataId;
extern Value g_focusEditValue;
void setFocusCursor(const Cursor cursor, int16_t dataId);
bool isFocusChanged();

void changeVoltageLimit(int iChannel);
void changeCurrentLimit(int iChannel);
void changePowerLimit(int iChannel);
void changePowerTripLevel(int iChannel);
void changePowerTripDelay(int iChannel);
void changeTemperatureTripLevel(int iChannel);
void changeTemperatureTripDelay(int iChannel);

void psuErrorMessage(const Cursor cursor, Value value, void (*ok_callback)() = 0);

Unit getCurrentEncoderUnit();

float encoderIncrement(Value value, int counter, float min, float max, int channelIndex, float precision = 1.0f);

bool isEncoderEnabledInActivePage();

void lockFrontPanel();
void unlockFrontPanel();
bool isFrontPanelLocked();

void showWelcomePage();
void showStandbyPage();
void showEnteringStandbyPage();
void showSavingPage();
void showShutdownPage();

extern Value g_alertMessage;
extern Value g_alertMessage2;
extern Value g_alertMessage3;

void infoMessage(const char *message);
void infoMessage(Value value);
void errorMessage(const char *message, bool autoDismiss = false);
void errorMessage(Value value);
void errorMessageWithAction(Value value, void (*action)(int param), const char *actionLabel, int actionParam);
void errorMessageWithAction(const char *message, void (*action)(), const char *actionLabel);

void yesNoDialog(int yesNoPageId, const char *message, void (*yes_callback)(), void (*no_callback)(), void (*cancel_callback)());
void yesNoLater(const char *message, void (*yes_callback)(), void (*no_callback)(), void (*later_callback)() = 0);
void areYouSure(void (*yes_callback)());
void areYouSureWithMessage(const char *message, void (*yes_callback)());

void dialogYes();
void dialogNo();
void dialogCancel();
void dialogOk();
void dialogLater();

void pushSelectFromEnumPage(
    AppContext *appContext,
    EnumDefinition enumDefinition,
    uint16_t currentValue, 
    bool (*disabledCallback)(uint16_t value), 
    void (*onSet)(uint16_t),
    bool smallFont = false,
    bool showRadioButtonIcon = true
);
void pushSelectFromEnumPage(
    AppContext *appContext,
    void(*enumDefinitionFunc)(DataOperationEnum operation, Cursor cursor, Value &value),
    uint16_t currentValue,
    bool(*disabledCallback)(uint16_t value),
    void(*onSet)(uint16_t),
    bool smallFont = false,
    bool showRadioButtonIcon = true
);

const EnumItem *getActiveSelectEnumDefinition();
void popSelectFromEnumPage();

void showMainPage();
void goBack();
void takeScreenshot();

extern Value g_progress;

struct AsyncOperationInProgressParams {
    const char *message;
    void (*checkStatus)();
    uint32_t startTime;
};

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

struct IntegerInputParams {
    NumericKeypadOptions m_options;
    int32_t m_input;
    bool canceled;
    static void onSet(float value);
    static void onCancel();
};

struct MenuInputParams {
    MenuType m_type;
    const char **m_items;
    int m_input;
    static void onSet(int value);
};

struct SelectParams {
    const char **m_options;
    int m_defaultSelection;
    int m_input;
    static void enumDefinition(DataOperationEnum operation, Cursor cursor, Value &value);
    static void onSelect(uint16_t value);
};

enum DialogActionResult {
    DIALOG_ACTION_RESULT_EXIT,
    DIALOG_ACTION_RESULT_TIMEOUT,
    DIALOG_ACTION_RESULT_SELECTED_ACTION
};

class PsuAppContext : public AppContext {
#if OPTION_GUI_THREAD
    friend void eez::gui::onGuiQueueMessageHook(uint8_t type, int16_t param);
#endif

public:
    PsuAppContext();

    void stateManagment() override;

    bool isActiveWidget(const WidgetCursor &widgetCursor) override;

    bool isFocusWidget(const WidgetCursor &widgetCursor) override;

    bool isBlinking(const Cursor cursor, int16_t id) override;

    bool isWidgetActionEnabled(const WidgetCursor &widgetCursor) override;
    
    void showProgressPage(const char *message, void (*abortCallback)() = 0);
    void showProgressPageWithoutAbort(const char *message);
    bool updateProgressPage(size_t processedSoFar, size_t totalSize);
    void hideProgressPage();

    void showAsyncOperationInProgress(const char *message = nullptr, void (*checkStatus)() = nullptr);
    void hideAsyncOperationInProgress();
    uint32_t getAsyncInProgressStartTime();

    static const size_t MAX_TEXT_MESSAGE_LEN = 64;
    void setTextMessage(const char *message, unsigned int len);
    void clearTextMessage();
    const char *getTextMessage();
    uint8_t getTextMessageVersion();

    void showUncaughtScriptExceptionMessage();

    const char *textInput(const char *label, size_t minChars, size_t maxChars, const char *value);
    float numberInput(const char *label, Unit unit, float min, float max, float value);
    bool integerInput(const char *label, int32_t min, int32_t max, int32_t &value);
    int menuInput(const char *label, MenuType menuType, const char **menuItems);
    int select(const char **options, int defaultSelection);

    bool dialogOpen(int *err);
    DialogActionResult dialogAction(uint32_t timeoutMs, const char *&selectedActionName);
    void dialogResetDataItemValues();
    void dialogSetDataItemValue(int16_t dataId, Value& value);
    void dialogSetDataItemValue(int16_t dataId, const char *str);
    void dialogClose();

    // TODO these should be private
    void (*m_dialogYesCallback)();
    void (*m_dialogNoCallback)();
    void (*m_dialogCancelCallback)();
    void (*m_dialogLaterCallback)();

    int getExtraLongTouchActionHook(const WidgetCursor &widgetCursor) override;

protected:
    bool m_pushProgressPage;
    const char *m_progressMessage;
    bool m_progressWithoutAbort;
    void (*m_progressAbortCallback)();
    bool m_popProgressPage;

    char m_textMessage[MAX_TEXT_MESSAGE_LEN + 1];
    uint8_t m_textMessageVersion;
    bool m_showTextMessage;
    bool m_clearTextMessage;

    bool m_showUncaughtScriptExceptionMessage;

    const char *m_inputLabel;

    AsyncOperationInProgressParams m_asyncOperationInProgressParams;

    friend struct TextInputParams;
    TextInputParams m_textInputParams;
    
    friend struct NumberInputParams;
    NumberInputParams m_numberInputParams;
    
    friend struct IntegerInputParams;
    IntegerInputParams m_integerInputParams;

    friend struct MenuInputParams;
    MenuInputParams m_menuInputParams;

    friend struct SelectParams;
    SelectParams m_selectParams;

    bool m_inputReady;

    int getMainPageId() override;
    void onPageChanged(int previousPageId, int activePageId) override;
    bool isAutoRepeatAction(int action) override;
    void onPageTouch(const WidgetCursor &foundWidget, Event &touchEvent) override;
    bool testExecuteActionOnTouchDown(int action) override;
    bool canExecuteActionWhenTouchedOutsideOfActivePage(int pageId, int action) override;
    void updatePage(int i, WidgetCursor &widgetCursor) override;

private:
    void doShowProgressPage();
    void doHideProgressPage();

    void doShowAsyncOperationInProgress();
    void doHideAsyncOperationInProgress();

    void doShowTextInput();
    void doShowNumberInput();
    void doShowIntegerInput();
    void doShowMenuInput();
    void doShowSelect();
};

extern PsuAppContext g_psuAppContext;

inline int getActivePageId() {
    return g_psuAppContext.getActivePageId();
}

inline Page *getActivePage() {
    return g_psuAppContext.getActivePage();
}

inline int getPreviousPageId() {
    return g_psuAppContext.getPreviousPageId();
}

inline bool isPageOnStack(int pageId) {
    return g_psuAppContext.isPageOnStack(pageId);
}

inline Page *getPage(int pageId) {
    return g_psuAppContext.getPage(pageId);
}

inline int getNumPagesOnStack() {
    return g_psuAppContext.getNumPagesOnStack();
}

inline void showPage(int pageId) {
    g_psuAppContext.showPage(pageId);
}

inline void pushPage(int pageId, Page *page = nullptr) {
    g_psuAppContext.pushPage(pageId, page);
}

inline void popPage() {
    g_psuAppContext.popPage();
}

inline void removePageFromStack(int pageId) {
    g_psuAppContext.removePageFromStack(pageId);
}

inline void replacePage(int pageId, Page *page = nullptr) {
    g_psuAppContext.replacePage(pageId, page);
}

inline void showProgressPage(const char *message, void (*abortCallback)() = 0) {
    g_psuAppContext.showProgressPage(message, abortCallback);
}

inline void showProgressPageWithoutAbort(const char *message) {
    g_psuAppContext.showProgressPageWithoutAbort(message);
}

inline bool updateProgressPage(size_t processedSoFar, size_t totalSize) {
    return g_psuAppContext.updateProgressPage(processedSoFar, totalSize);
}

inline void hideProgressPage() {
    g_psuAppContext.hideProgressPage();
}

inline void pushSelectFromEnumPage(
    EnumDefinition enumDefinition,
    uint16_t currentValue, 
    bool (*disabledCallback)(uint16_t value), 
    void (*onSet)(uint16_t),
    bool smallFont = false,
    bool showRadioButtonIcon = true
) {
    pushSelectFromEnumPage(&g_psuAppContext, enumDefinition, currentValue, disabledCallback, onSet, smallFont, showRadioButtonIcon);
}

inline void pushSelectFromEnumPage(
    void(*enumDefinitionFunc)(DataOperationEnum operation, Cursor cursor, Value &value),
    uint16_t currentValue,
    bool(*disabledCallback)(uint16_t value),
    void(*onSet)(uint16_t),
    bool smallFont = false,
    bool showRadioButtonIcon = true
) {
    pushSelectFromEnumPage(&g_psuAppContext, enumDefinitionFunc, currentValue, disabledCallback, onSet, smallFont, showRadioButtonIcon);
}

inline void showAsyncOperationInProgress(const char *message = nullptr, void (*checkStatus)() = nullptr) {
    g_psuAppContext.showAsyncOperationInProgress(message, checkStatus);
}

inline void hideAsyncOperationInProgress() {
    g_psuAppContext.hideAsyncOperationInProgress();
}

inline void setTextMessage(const char *message, unsigned int len) {
    g_psuAppContext.setTextMessage(message, len); 
}

inline void clearTextMessage() {
    g_psuAppContext.clearTextMessage();
}

inline const char *getTextMessage() {
    return g_psuAppContext.getTextMessage();
}

inline uint8_t getTextMessageVersion() {
    return g_psuAppContext.getTextMessageVersion();
}

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
    GUI_QUEUE_MESSAGE_TYPE_SHOW_INTEGER_INPUT,
    GUI_QUEUE_MESSAGE_TYPE_SHOW_MENU_INPUT,
    GUI_QUEUE_MESSAGE_TYPE_SHOW_SELECT,
    GUI_QUEUE_MESSAGE_TYPE_DIALOG_OPEN,
    GUI_QUEUE_MESSAGE_TYPE_DIALOG_CLOSE,
    GUI_QUEUE_MESSAGE_TYPE_SHOW_ASYNC_OPERATION_IN_PROGRESS,
    GUI_QUEUE_MESSAGE_TYPE_HIDE_ASYNC_OPERATION_IN_PROGRESS
};

} // namespace gui
} // namespace psu
} // namespace eez
