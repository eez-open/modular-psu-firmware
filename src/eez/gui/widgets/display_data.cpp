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

#if OPTION_DISPLAY

#include <string.h>

#include <eez/system.h>
#include <eez/util.h>

#include <eez/gui/gui.h>
#include <eez/gui/widgets/display_data.h>

namespace eez {
namespace gui {

int findStartOfFraction(char *text) {
    int i;
    for (i = 0; text[i] && (text[i] == '-' || (text[i] >= '0' && text[i] <= '9')); i++);
    return i;
}

int findStartOfUnit(char *text, int i) {
    for (i = 0; text[i] && (text[i] == '-' || (text[i] >= '0' && text[i] <= '9') || text[i] == '.'); i++);
    return i;
}

void DisplayDataWidget_draw(const WidgetCursor &widgetCursor) {
    const Widget *widget = widgetCursor.widget;
    const DisplayDataWidget *display_data_widget = GET_WIDGET_PROPERTY(widget, specific, const DisplayDataWidget *);

    DisplayDataState *currentState = (DisplayDataState *)widgetCursor.currentState;
    DisplayDataState *previousState = (DisplayDataState *)widgetCursor.previousState;

    widgetCursor.currentState->size = sizeof(DisplayDataState);
    widgetCursor.currentState->flags.focused = isFocusWidget(widgetCursor);

	const Style *style = getStyle(overrideStyleHook(widgetCursor, widgetCursor.currentState->flags.focused ? display_data_widget->focusStyle : widget->style));

    widgetCursor.currentState->flags.blinking = g_isBlinkTime && data::isBlinking(widgetCursor.cursor, widget->data);
    
    auto data = data::get(widgetCursor.cursor, widget->data);
    bool refreshData;
    if (widgetCursor.previousState) {
        refreshData = widgetCursor.previousState->data != data;
        if (refreshData) {
            uint32_t refreshRate = getTextRefreshRate(widgetCursor.cursor, widget->data);
            if (refreshRate != 0) {
                refreshData = (millis() - currentState->dataRefreshLastTime) > refreshRate;
            }
        }
        if (refreshData) {
            widgetCursor.currentState->data = data;
        } else {
            widgetCursor.currentState->data = widgetCursor.previousState->data;
        }
    } else {
        refreshData = true;
        widgetCursor.currentState->data = data;
    }
    if (refreshData) {
        currentState->dataRefreshLastTime = millis();
    }

    currentState->color = data::getColor(widgetCursor.cursor, widget->data, style);
    currentState->backgroundColor = data::getBackgroundColor(widgetCursor.cursor, widget->data, style);
    currentState->activeColor = data::getActiveColor(widgetCursor.cursor, widget->data, style);
    currentState->activeBackgroundColor = data::getActiveBackgroundColor(widgetCursor.cursor, widget->data, style);

    bool refresh =
        !widgetCursor.previousState ||
        widgetCursor.previousState->flags.focused != widgetCursor.currentState->flags.focused ||
        widgetCursor.previousState->flags.active != widgetCursor.currentState->flags.active ||
        widgetCursor.previousState->flags.blinking != widgetCursor.currentState->flags.blinking ||
        refreshData ||
        currentState->color != previousState->color ||
        currentState->backgroundColor != previousState->backgroundColor ||
        currentState->activeColor != previousState->activeColor ||
        currentState->activeBackgroundColor != previousState->activeBackgroundColor;

    if (refresh) {
        char text[64];
        widgetCursor.currentState->data.toText(text, sizeof(text));

        char *start = text;

        if (display_data_widget->displayOption == DISPLAY_OPTION_INTEGER) {
            int i = findStartOfFraction(text);
            text[i] = 0;
        } else if (display_data_widget->displayOption == DISPLAY_OPTION_FRACTION) {
            int i = findStartOfFraction(text);
            start = text + i;
        } else if (display_data_widget->displayOption == DISPLAY_OPTION_FRACTION_AND_UNIT) {
            int i = findStartOfFraction(text);
            int k = findStartOfUnit(text, i);
            if (i < k) {
                start = text + i;
                text[k] = 0;
            } else {
                strcpy(text, ".00");
            }
        } else if (display_data_widget->displayOption == DISPLAY_OPTION_UNIT) {
            int i = findStartOfUnit(text, 0);
            start = text + i;
        } else if (display_data_widget->displayOption == DISPLAY_OPTION_INTEGER_AND_FRACTION) {
            int i = findStartOfUnit(text, 0);
            text[i] = 0;
        }

        drawText(start, -1, widgetCursor.x, widgetCursor.y, (int)widget->w, (int)widget->h,
                 style, widgetCursor.currentState->flags.active,
                 widgetCursor.currentState->flags.blinking, false,
                 &currentState->color, &currentState->backgroundColor, &currentState->activeColor, &currentState->activeBackgroundColor);
    }
}

} // namespace gui
} // namespace eez

#endif