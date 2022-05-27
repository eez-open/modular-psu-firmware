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

#include <string.h>

#include <eez/core/os.h>
#include <eez/core/util.h>

#include <eez/gui/gui.h>
#include <eez/gui/widgets/display_data.h>

namespace eez {
namespace gui {

enum {
    DISPLAY_OPTION_ALL = 0,
    DISPLAY_OPTION_INTEGER = 1,
    DISPLAY_OPTION_FRACTION = 2,
    DISPLAY_OPTION_FRACTION_AND_UNIT = 3,
    DISPLAY_OPTION_UNIT = 4,
    DISPLAY_OPTION_INTEGER_AND_FRACTION = 5
};

int findStartOfFraction(char *text) {
    int i;
    for (i = 0; text[i] && (text[i] == '<' || text[i] == ' ' || text[i] == '-' || (text[i] >= '0' && text[i] <= '9')); i++);
    return i;
}

int findStartOfUnit(char *text, int i) {
    for (i = 0; text[i] && (text[i] == '<' || text[i] == ' ' || text[i] == '-' || (text[i] >= '0' && text[i] <= '9') || text[i] == '.'); i++);
    return i;
}

bool DisplayDataWidgetState::updateState() {
    WIDGET_STATE_START(DisplayDataWidget);

    const Style *style = getStyle(g_hooks.overrideStyle(widgetCursor, widget->style));

    WIDGET_STATE(flags.active, g_isActiveWidget);
    WIDGET_STATE(flags.focused, isFocusWidget(widgetCursor));
    WIDGET_STATE(flags.blinking, g_isBlinkTime && isBlinking(widgetCursor, widget->data));

    bool refreshData = true;
    auto newData = get(widgetCursor, widget->data);
    auto currentTime = millis();
    if (hasPreviousState && data != newData) {
        uint32_t refreshRate = getTextRefreshRate(widgetCursor, widget->data);
        if (refreshRate != 0 && currentTime - dataRefreshLastTime < refreshRate) {
            refreshData = false;
        }
    }
    if (refreshData) {
        WIDGET_STATE(data, newData);
        dataRefreshLastTime = currentTime;
    }

    WIDGET_STATE(color,                 flags.focused ? style->focusColor           : getColor(widgetCursor, widget->data, style));
    WIDGET_STATE(backgroundColor,       flags.focused ? style->focusBackgroundColor : getBackgroundColor(widgetCursor, widget->data, style));
    WIDGET_STATE(activeColor,           flags.focused ? style->focusBackgroundColor : getActiveColor(widgetCursor, widget->data, style));
    WIDGET_STATE(activeBackgroundColor, flags.focused ? style->focusColor           : getActiveBackgroundColor(widgetCursor, widget->data, style));

    bool cursorVisible = millis() % (2 * CONF_GUI_TEXT_CURSOR_BLINK_TIME_MS) < CONF_GUI_TEXT_CURSOR_BLINK_TIME_MS;
    WIDGET_STATE(cursorPosition, cursorVisible ? getTextCursorPosition(widgetCursor, widget->data) : -1);

    WIDGET_STATE(xScroll, getXScroll(widgetCursor));

    WIDGET_STATE_END()
}

void DisplayDataWidgetState::render() {
    const WidgetCursor &widgetCursor = g_widgetCursor;

    auto widget = (const DisplayDataWidget *)widgetCursor.widget;
    const Style *style = getStyle(g_hooks.overrideStyle(widgetCursor, widget->style));

    char text[64];
    data.toText(text, sizeof(text));

    char *start = text;

    int length = -1;

    if (widget->displayOption != DISPLAY_OPTION_ALL) {
        if (data.getType() == VALUE_TYPE_FLOAT) {
            if (widget->displayOption == DISPLAY_OPTION_INTEGER) {
                int i = findStartOfFraction(text);
                text[i] = 0;
            } else if (widget->displayOption == DISPLAY_OPTION_FRACTION) {
                int i = findStartOfFraction(text);
                start = text + i;
            } else if (widget->displayOption == DISPLAY_OPTION_FRACTION_AND_UNIT) {
                int i = findStartOfFraction(text);
                int k = findStartOfUnit(text, i);
                if (i < k) {
                    start = text + i;
                    text[k] = 0;
                }
                else {
                    stringCopy(text, sizeof(text), ".0");
                }
            } else if (widget->displayOption == DISPLAY_OPTION_UNIT) {
                int i = findStartOfUnit(text, 0);
                start = text + i;
            } else if (widget->displayOption == DISPLAY_OPTION_INTEGER_AND_FRACTION) {
                int i = findStartOfUnit(text, 0);
                text[i] = 0;
            }

            // trim left
            while (*start && *start == ' ') {
                start++;
            }

            // trim right
            length = strlen(start);
            if (length > 0 && start[length - 1] == ' ') {
                length--;
            }
        } else {
            if (
                widget->displayOption != DISPLAY_OPTION_INTEGER &&
                widget->displayOption != DISPLAY_OPTION_INTEGER_AND_FRACTION
            ) {
                *text = 0;
            }
        }
    }

    drawText(
        start, length,
        widgetCursor.x, widgetCursor.y, widgetCursor.w, widgetCursor.h,
        style,
        flags.active, flags.blinking, false,
        &color, &backgroundColor, &activeColor, &activeBackgroundColor,
        data.getType() == VALUE_TYPE_FLOAT || widget->data == DATA_ID_KEYPAD_EDIT_UNIT, // useSmallerFontIfDoesNotFit
        cursorPosition,
        xScroll
    );
}

int DISPLAY_DATA_getCharIndexAtPosition(int xPos, const WidgetCursor &widgetCursor) {
    auto widget = (const DisplayDataWidget *)widgetCursor.widget;

	const Style *style = getStyle(g_hooks.overrideStyle(widgetCursor, widget->style));

    char text[64];
    Value data = get(widgetCursor, widget->data);
    data.toText(text, sizeof(text));

    char *start = text;

    if (widget->displayOption == DISPLAY_OPTION_INTEGER) {
        int i = findStartOfFraction(text);
        text[i] = 0;
    } else if (widget->displayOption == DISPLAY_OPTION_FRACTION) {
        int i = findStartOfFraction(text);
        start = text + i;
    } else if (widget->displayOption == DISPLAY_OPTION_FRACTION_AND_UNIT) {
        int i = findStartOfFraction(text);
        int k = findStartOfUnit(text, i);
        if (i < k) {
            start = text + i;
            text[k] = 0;
        } else {
            stringCopy(text, sizeof(text), ".0");
        }
    } else if (widget->displayOption == DISPLAY_OPTION_UNIT) {
        int i = findStartOfUnit(text, 0);
        start = text + i;
    } else if (widget->displayOption == DISPLAY_OPTION_INTEGER_AND_FRACTION) {
        int i = findStartOfUnit(text, 0);
        text[i] = 0;
    }

    // trim left
    while (*start && *start == ' ') {
        start++;
    }

    // trim right
    int length = strlen(start);
    while (length > 0 && start[length - 1] == ' ') {
        length--;
    }

    int xScroll = getXScroll(widgetCursor);

    return getCharIndexAtPosition(xPos, start, length, widgetCursor.x - xScroll, widgetCursor.y, widgetCursor.w, widgetCursor.h, style);
}

int DISPLAY_DATA_getCursorXPosition(int cursorPosition, const WidgetCursor &widgetCursor) {
    auto widget = (const DisplayDataWidget *)widgetCursor.widget;

	const Style *style = getStyle(g_hooks.overrideStyle(widgetCursor, widget->style));

    char text[64];
    Value data = get(widgetCursor, widget->data);
    data.toText(text, sizeof(text));

    char *start = text;

    if (widget->displayOption == DISPLAY_OPTION_INTEGER) {
        int i = findStartOfFraction(text);
        text[i] = 0;
    } else if (widget->displayOption == DISPLAY_OPTION_FRACTION) {
        int i = findStartOfFraction(text);
        start = text + i;
    } else if (widget->displayOption == DISPLAY_OPTION_FRACTION_AND_UNIT) {
        int i = findStartOfFraction(text);
        int k = findStartOfUnit(text, i);
        if (i < k) {
            start = text + i;
            text[k] = 0;
        } else {
            stringCopy(text, sizeof(text), ".0");
        }
    } else if (widget->displayOption == DISPLAY_OPTION_UNIT) {
        int i = findStartOfUnit(text, 0);
        start = text + i;
    } else if (widget->displayOption == DISPLAY_OPTION_INTEGER_AND_FRACTION) {
        int i = findStartOfUnit(text, 0);
        text[i] = 0;
    }

    // trim left
    while (*start && *start == ' ') {
        start++;
    }

    // trim right
    int length = strlen(start);
    while (length > 0 && start[length - 1] == ' ') {
        length--;
    }

    return getCursorXPosition(cursorPosition, start, length, widgetCursor.x, widgetCursor.y, widgetCursor.w, widgetCursor.h, style);
}

} // namespace gui
} // namespace eez
