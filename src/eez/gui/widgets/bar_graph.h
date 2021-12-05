/*
 * EEZ Modular Firmware
 * Copyright (C) 2021-present, Envox d.o.o.
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

namespace eez {
namespace gui {

struct BarGraphWidget : public Widget {
    int16_t textStyle;
    int16_t line1Data;
    int16_t line1Style;
    int16_t line2Data;
    int16_t line2Style;
    uint8_t orientation; // BAR_GRAPH_ORIENTATION_...
};

struct BarGraphWidgetState : public WidgetState {
    uint16_t color;
    uint16_t backgroundColor;
    uint16_t activeColor;
    uint16_t activeBackgroundColor;
    Value line1Data;
    Value line2Data;
    Value textData;
    uint32_t textDataRefreshLastTime;

    BarGraphWidgetState(const WidgetCursor &widgetCursor) : WidgetState(widgetCursor) {
        auto widget = (const BarGraphWidget *)widgetCursor.widget;

        const Style* style = getStyle(overrideStyleHook(widgetCursor, widget->style));

        flags.blinking = g_isBlinkTime && isBlinking(widgetCursor, widget->data);
        data = get(widgetCursor, widget->data);
        
        color = getColor(widgetCursor, widget->data, style);
        backgroundColor = getBackgroundColor(widgetCursor, widget->data, style);
        activeColor = getActiveColor(widgetCursor, widget->data, style);
        activeBackgroundColor = getActiveBackgroundColor(widgetCursor, widget->data, style);

        line1Data = get(widgetCursor, widget->line1Data);
        
        line2Data = get(widgetCursor, widget->line2Data);

        textData = data;
        textDataRefreshLastTime = millis();
    }

    bool operator!=(const BarGraphWidgetState& previousState) {
        return 
            flags.active != previousState.flags.active ||
            flags.blinking != previousState.flags.blinking ||
            data != previousState.data ||
            color != previousState.color ||
            backgroundColor != previousState.backgroundColor ||
            activeColor != previousState.activeColor ||
            activeBackgroundColor != previousState.activeBackgroundColor ||
            line1Data != previousState.line1Data ||
            line2Data != previousState.line2Data ||
            textData != previousState.textData;
    }

    void refreshTextData(BarGraphWidgetState *previousState);

    void draw(WidgetState *previousState) override;
};

} // namespace gui
} // namespace eez
