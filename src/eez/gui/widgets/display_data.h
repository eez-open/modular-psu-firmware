/*
 * EEZ Modular Firmware
 * Copyright (C) 2020-present, Envox d.o.o.
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

#define CONF_GUI_TEXT_CURSOR_BLINK_TIME_MS 500

namespace eez {
namespace gui {

struct DisplayDataWidget : public Widget {
    uint8_t displayOption;
};

struct DisplayDataWidgetState : public WidgetState {
    uint16_t color;
    uint16_t backgroundColor;
    uint16_t activeColor;
    uint16_t activeBackgroundColor;
    uint32_t dataRefreshLastTime;
    int16_t cursorPosition;
    uint8_t xScroll;

    DisplayDataWidgetState(const WidgetCursor &widgetCursor) : WidgetState(widgetCursor) {
        auto widget = (const DisplayDataWidget *)widgetCursor.widget;

        flags.focused = isFocusWidget(widgetCursor);

        const Style *style = getStyle(overrideStyleHook(widgetCursor, widget->style));

        flags.blinking = g_isBlinkTime && isBlinking(widgetCursor, widget->data);
        
        data = get(widgetCursor, widget->data);
        dataRefreshLastTime = millis();

        color = flags.focused ? style->focus_color : getColor(widgetCursor, widget->data, style);
        backgroundColor = flags.focused ? style->focus_background_color : getBackgroundColor(widgetCursor, widget->data, style);
        activeColor = flags.focused ? style->focus_background_color : getActiveColor(widgetCursor, widget->data, style);
        activeBackgroundColor = flags.focused ? style->focus_color : getActiveBackgroundColor(widgetCursor, widget->data, style);

        bool cursorVisible = millis() % (2 * CONF_GUI_TEXT_CURSOR_BLINK_TIME_MS) < CONF_GUI_TEXT_CURSOR_BLINK_TIME_MS;
        cursorPosition = cursorVisible ? getTextCursorPosition(widgetCursor, widget->data) : -1;
        
        xScroll = getXScroll(widgetCursor);
    }

    bool operator!=(const DisplayDataWidgetState& previousState) {
        return 
            flags.active != previousState.flags.active ||
            data != previousState.data ||
            flags.focused != previousState.flags.focused ||
            flags.blinking != previousState.flags.blinking ||
            color != previousState.color ||
            backgroundColor != previousState.backgroundColor ||
            activeColor != previousState.activeColor ||
            activeBackgroundColor != previousState.activeBackgroundColor ||
            cursorPosition != previousState.cursorPosition ||
            xScroll != previousState.xScroll;
    }

    void refreshTextData(DisplayDataWidgetState *previousState);

    void draw(WidgetState *previousState) override;
};

int DISPLAY_DATA_getCharIndexAtPosition(int xPos, const WidgetCursor &widgetCursor);
int DISPLAY_DATA_getCursorXPosition(int cursorPosition, const WidgetCursor &widgetCursor);

} // namespace gui
} // namespace eez