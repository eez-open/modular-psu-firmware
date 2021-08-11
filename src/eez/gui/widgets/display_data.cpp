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

#define CONF_GUI_TEXT_CURSOR_BLINK_TIME_MS 500

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

struct DisplayDataWidget : public Widget {
    uint8_t displayOption;
};

struct DisplayDataState : public WidgetState {
    uint16_t color;
    uint16_t backgroundColor;
    uint16_t activeColor;
    uint16_t activeBackgroundColor;
    uint32_t dataRefreshLastTime;
    int16_t cursorPosition;
    uint8_t xScroll;
};

EnumFunctionType DISPLAY_DATA_enum = nullptr;

int findStartOfFraction(char *text) {
    int i;
    for (i = 0; text[i] && (text[i] == '<' || text[i] == ' ' || text[i] == '-' || (text[i] >= '0' && text[i] <= '9')); i++);
    return i;
}

int findStartOfUnit(char *text, int i) {
    for (i = 0; text[i] && (text[i] == '<' || text[i] == ' ' || text[i] == '-' || (text[i] >= '0' && text[i] <= '9') || text[i] == '.'); i++);
    return i;
}

DrawFunctionType DISPLAY_DATA_draw = [](const WidgetCursor &widgetCursor) {
    auto widget = (const DisplayDataWidget *)widgetCursor.widget;

    DisplayDataState *currentState = (DisplayDataState *)widgetCursor.currentState;
    DisplayDataState *previousState = (DisplayDataState *)widgetCursor.previousState;

    widgetCursor.currentState->size = sizeof(DisplayDataState);
    widgetCursor.currentState->flags.focused = isFocusWidget(widgetCursor);

	const Style *style = getStyle(overrideStyleHook(widgetCursor, widget->style));

    widgetCursor.currentState->flags.blinking = g_isBlinkTime && isBlinking(widgetCursor, widget->data);
    
    uint32_t currentTime = millis();
	widgetCursor.currentState->data.clear();
	widgetCursor.currentState->data = get(widgetCursor, widget->data);
    bool refreshData = false;
    if (widgetCursor.previousState) {
        refreshData = widgetCursor.currentState->data != widgetCursor.previousState->data;
        if (refreshData) {
            uint32_t refreshRate = getTextRefreshRate(widgetCursor, widget->data);
            if (refreshRate != 0) {
                refreshData = (currentTime - previousState->dataRefreshLastTime) > refreshRate;
                if (!refreshData) {
                    widgetCursor.currentState->data.clear();
                    widgetCursor.currentState->data = widgetCursor.previousState->data;
                }
            }
        }
    } else {
        refreshData = true;
    }
    currentState->dataRefreshLastTime = refreshData ? currentTime : previousState->dataRefreshLastTime;

    currentState->color = widgetCursor.currentState->flags.focused ? style->focus_color : getColor(widgetCursor, widget->data, style);
    currentState->backgroundColor = widgetCursor.currentState->flags.focused ? style->focus_background_color : getBackgroundColor(widgetCursor, widget->data, style);
    currentState->activeColor = widgetCursor.currentState->flags.focused ? style->focus_background_color : getActiveColor(widgetCursor, widget->data, style);
    currentState->activeBackgroundColor = widgetCursor.currentState->flags.focused ? style->focus_color : getActiveBackgroundColor(widgetCursor, widget->data, style);

    bool cursorVisible = millis() % (2 * CONF_GUI_TEXT_CURSOR_BLINK_TIME_MS) < CONF_GUI_TEXT_CURSOR_BLINK_TIME_MS;
    currentState->cursorPosition = cursorVisible ? getTextCursorPosition(widgetCursor, widget->data) : -1;
    
    currentState->xScroll = getXScroll(widgetCursor);

    bool refresh =
        !widgetCursor.previousState ||
        widgetCursor.previousState->flags.focused != widgetCursor.currentState->flags.focused ||
        widgetCursor.previousState->flags.active != widgetCursor.currentState->flags.active ||
        widgetCursor.previousState->flags.blinking != widgetCursor.currentState->flags.blinking ||
        refreshData ||
        currentState->color != previousState->color ||
        currentState->backgroundColor != previousState->backgroundColor ||
        currentState->activeColor != previousState->activeColor ||
        currentState->activeBackgroundColor != previousState->activeBackgroundColor ||
        currentState->cursorPosition != previousState->cursorPosition ||
        currentState->xScroll != previousState->xScroll;

    if (refresh) {
        char text[64];
        widgetCursor.currentState->data.toText(text, sizeof(text));

        char *start = text;

        int length = -1;

        if (widget->displayOption != DISPLAY_OPTION_ALL) {
			if (widgetCursor.currentState->data.getType() == VALUE_TYPE_FLOAT) {
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

        drawText(start, length, widgetCursor.x, widgetCursor.y, (int)widget->w, (int)widget->h,
            style, widgetCursor.currentState->flags.active,
            widgetCursor.currentState->flags.blinking, false,
            &currentState->color, &currentState->backgroundColor, &currentState->activeColor, &currentState->activeBackgroundColor,
            widgetCursor.currentState->data.getType() == VALUE_TYPE_FLOAT || widget->data == DATA_ID_EDIT_UNIT,
            currentState->cursorPosition, currentState->xScroll);
    }

    widgetCursor.currentState->data.freeRef();
};

int DISPLAY_DATA_getCharIndexAtPosition(int xPos, const WidgetCursor &widgetCursor) {
    auto widget = (const DisplayDataWidget *)widgetCursor.widget;

	const Style *style = getStyle(overrideStyleHook(widgetCursor, widget->style));

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

    return getCharIndexAtPosition(xPos, start, length, widgetCursor.x - xScroll, widgetCursor.y, (int)widget->w, (int)widget->h, style);
}

int DISPLAY_DATA_getCursorXPosition(int cursorPosition, const WidgetCursor &widgetCursor) {
    auto widget = (const DisplayDataWidget *)widgetCursor.widget;

	const Style *style = getStyle(overrideStyleHook(widgetCursor, widget->style));

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

    return getCursorXPosition(cursorPosition, start, length, widgetCursor.x, widgetCursor.y, (int)widget->w, (int)widget->h, style);
}

OnTouchFunctionType DISPLAY_DATA_onTouch = nullptr;

OnKeyboardFunctionType DISPLAY_DATA_onKeyboard = nullptr;

} // namespace gui
} // namespace eez

#endif
