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

#include <eez/os.h>
#include <eez/util.h>

#include <eez/gui_conf.h>
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

void DisplayDataWidgetState::refreshTextData(DisplayDataWidgetState *previousState) {
    if (previousState && data != previousState->data) {
		auto widget = (const DisplayDataWidget *)widgetCursor.widget;
        uint32_t refreshRate = getTextRefreshRate(widgetCursor, widget->data);
        if (refreshRate != 0) {
            if (dataRefreshLastTime - previousState->dataRefreshLastTime < refreshRate) {
                data = previousState->data;
                dataRefreshLastTime = previousState->dataRefreshLastTime;
            }
        }
    }
}

void DisplayDataWidgetState::draw(WidgetState *previousStateBase) {
    auto previousState = (DisplayDataWidgetState *)previousStateBase;
    refreshTextData(previousState);
    bool refresh = !previousState || *this != *previousState;
    if (refresh) {
        auto widget = (const DisplayDataWidget *)widgetCursor.widget;
        const Style *style = getStyle(overrideStyleHook(widgetCursor, widget->style));

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

        drawText(start, length, widgetCursor.x, widgetCursor.y, (int)widget->w, (int)widget->h,
            style, flags.active,
            flags.blinking, false,
            &color, &backgroundColor, &activeColor, &activeBackgroundColor,
            data.getType() == VALUE_TYPE_FLOAT || widget->data == EEZ_CONF_DATA_ID_EDIT_UNIT,
            cursorPosition, xScroll);
    }
}

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

} // namespace gui
} // namespace eez
