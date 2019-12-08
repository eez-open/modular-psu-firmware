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

#include <eez/gui/app_context.h>
#include <eez/gui/gui.h>

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

class PsuAppContext : public AppContext {
public:
    PsuAppContext();

    void stateManagment() override;

    bool isActiveWidget(const WidgetCursor &widgetCursor) override;

    bool isFocusWidget(const WidgetCursor &widgetCursor) override;

    bool isBlinking(const data::Cursor &cursor, uint16_t id) override;

    uint32_t getNumHistoryValues(uint16_t id) override;
    uint32_t getCurrentHistoryValuePosition(const Cursor &cursor, uint16_t id) override;
    
    static void showProgressPage(const char *message, void (*abortCallback)() = 0);
    static bool updateProgressPage(size_t processedSoFar, size_t totalSize);
    static void hideProgressPage();

    static void setTextMessage(const char *message, unsigned int len);
    static void clearTextMessage();
    static const char *getTextMessage();
    static uint8_t getTextMessageVersion();

    void showUncaughtScriptExceptionMessage();

protected:
    bool m_pushProgressPage;
    const char *m_progressMessage;
    void (*m_progressAbortCallback)();
    bool m_popProgressPage;

    char m_textMessage[32 + 1];
    uint8_t m_textMessageVersion;
    bool m_showTextMessage;
    bool m_clearTextMessage;

    bool m_showUncaughtScriptExceptionMessage;

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
