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

#include <eez/gui/gui.h>

namespace eez {
namespace gui {

#define LIST_TYPE_VERTICAL 1
#define LIST_TYPE_HORIZONTAL 2

struct ListWidget : public Widget {
    AssetsPtr<Widget> itemWidget;
    uint8_t listType; // LIST_TYPE_VERTICAL or LIST_TYPE_HORIZONTAL
    uint8_t gap;
};

EnumFunctionType LIST_enum = [](WidgetCursor &widgetCursor, EnumWidgetsCallback callback) {
	auto savedCurrentState = widgetCursor.currentState;
	auto savedPreviousState = widgetCursor.previousState;
	
    WidgetState *endOfContainerInPreviousState = 0;
    if (widgetCursor.previousState) {
        endOfContainerInPreviousState = nextWidgetState(widgetCursor.previousState);
    }

    auto savedWidget = widgetCursor.widget;

    auto parentWidget = savedWidget;

    auto listWidget = (const ListWidget *)widgetCursor.widget;

    int startPosition = ytDataGetPosition(widgetCursor, widgetCursor.widget->data);

    // refresh when startPosition changes
    if (widgetCursor.currentState) {
        widgetCursor.currentState->data.clear();
		widgetCursor.currentState->data = startPosition;

        if (widgetCursor.previousState && widgetCursor.previousState->data != widgetCursor.currentState->data) {
            widgetCursor.previousState = 0;
        }
    }

	// move to the first child widget state
	if (widgetCursor.previousState) {
		++widgetCursor.previousState;
	}
	if (widgetCursor.currentState) {
		++widgetCursor.currentState;
	}

    const Widget *childWidget = listWidget->itemWidget.ptr(widgetCursor.assets);
    widgetCursor.widget = childWidget;

	auto savedX = widgetCursor.x;
	auto savedY = widgetCursor.y;

    auto savedCursor = widgetCursor.cursor;

    int offset = 0;
    int count = eez::gui::count(widgetCursor, parentWidget->data);

    Value oldValue;

    for (int index = startPosition; index < count; ++index) {
        select(widgetCursor, parentWidget->data, index, oldValue);

        if (listWidget->listType == LIST_TYPE_VERTICAL) {
            if (offset < parentWidget->h) {
				widgetCursor.y = savedY + offset;
                enumWidget(widgetCursor, callback);
				offset += childWidget->h + listWidget->gap;
            } else {
                break;
            }
        } else {
            if (offset < parentWidget->w) {
				widgetCursor.x = savedX + offset;
				enumWidget(widgetCursor, callback);
				offset += childWidget->w + listWidget->gap;
            } else {
                break;
            }
        }

        if (widgetCursor.previousState) {
			widgetCursor.previousState = nextWidgetState(widgetCursor.previousState);
            if (widgetCursor.previousState > endOfContainerInPreviousState) {
				widgetCursor.previousState = 0;
            }
        }

        if (widgetCursor.currentState) {
			widgetCursor.currentState = nextWidgetState(widgetCursor.currentState);
        }
    }

	widgetCursor.x = savedX;
	widgetCursor.y = savedY;

    widgetCursor.cursor = savedCursor;

    widgetCursor.widget = savedWidget;

    if (widgetCursor.currentState) {
        savedCurrentState->size = ((uint8_t *)widgetCursor.currentState) - ((uint8_t *)savedCurrentState);
    }

    if (count > 0) {
        deselect(widgetCursor, widgetCursor.widget->data, oldValue);
    }

	widgetCursor.currentState = savedCurrentState;
	widgetCursor.previousState = savedPreviousState;
};

DrawFunctionType LIST_draw = nullptr;

OnTouchFunctionType LIST_onTouch = nullptr;

OnKeyboardFunctionType LIST_onKeyboard = nullptr;

} // namespace gui
} // namespace eez

#endif
