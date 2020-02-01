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
extern uint16_t g_focusDataId;
extern data::Value g_focusEditValue;
void setFocusCursor(const data::Cursor &cursor, uint16_t dataId);
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

class PsuAppContext : public AppContext {
public:
    PsuAppContext();

    void stateManagment() override;

    bool isActiveWidget(const WidgetCursor &widgetCursor) override;

    bool isFocusWidget(const WidgetCursor &widgetCursor) override;

    bool isBlinking(const data::Cursor &cursor, uint16_t id) override;

    uint32_t getNumHistoryValues(uint16_t id) override;
    uint32_t getCurrentHistoryValuePosition(const Cursor &cursor, uint16_t id) override;

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
    float numberInput(const char *label, Unit unit, float min, float max, float value);
    int menuInput(const char *label, MenuType menuType, const char **menuItems);

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

    bool m_showTextInputOnNextIter;
    size_t m_textInputMinChars;
    size_t m_textInputMaxChars;
    const char *m_textInput;
    static void onSetTextInputResult(char *);
    static void onCancelTextInput();

    bool m_showNumberInputOnNextIter;
    NumericKeypadOptions m_numberInputOptions;
    float m_numberInput;
    static void onSetNumberInputResult(float value);
    static void onCancelNumberInput();

    bool m_showMenuInputOnNextIter;
    MenuType m_menuType;
    const char **m_menuItems;
    int m_menuInput;
    static void onSetMenuInputResult(int value);

    bool m_inputReady;

    int getMainPageId() override;
    void onPageChanged(int previousPageId, int activePageId) override;
    bool isAutoRepeatAction(int action) override;
    void onPageTouch(const WidgetCursor &foundWidget, Event &touchEvent) override;
    bool testExecuteActionOnTouchDown(int action) override;
};

extern PsuAppContext g_psuAppContext;

static const uint16_t g_ytGraphStyles[] = {
    STYLE_ID_YT_GRAPH_Y1,
    STYLE_ID_YT_GRAPH_Y2,
    STYLE_ID_YT_GRAPH_Y3,
    STYLE_ID_YT_GRAPH_Y4
};

} // namespace gui
} // namespace psu
} // namespace eez
