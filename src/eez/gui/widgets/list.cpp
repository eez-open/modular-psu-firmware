/*
 * EEZ Generic Firmware
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

#include <eez/gui/widgets/list.h>

#include <eez/gui/assets.h>

namespace eez {
namespace gui {

void ListWidget_fixPointers(Widget *widget) {
    ListWidget *listWidget = (ListWidget *)widget->specific;
    listWidget->item_widget = (Widget *)((uint8_t *)g_document + (uint32_t)listWidget->item_widget);
    Widget_fixPointers(listWidget->item_widget);
}

void ListWidget_enum(WidgetCursor &widgetCursor, EnumWidgetsCallback callback) {
	auto savedCurrentState = widgetCursor.currentState;
	auto savedPreviousState = widgetCursor.previousState;
	
    WidgetState *endOfContainerInPreviousState = 0;
    if (widgetCursor.previousState)
        endOfContainerInPreviousState = nextWidgetState(widgetCursor.previousState);

    // move to the first child widget state
    if (widgetCursor.previousState) {
        ++widgetCursor.previousState;
    }
    if (widgetCursor.currentState) {
        ++widgetCursor.currentState;
    }

    auto savedWidget = widgetCursor.widget;

    auto parentWidget = savedWidget;

    const ListWidget *listWidget = (const ListWidget *)widgetCursor.widget->specific;

    const Widget *childWidget = listWidget->item_widget;
    widgetCursor.widget = childWidget;

	auto savedX = widgetCursor.x;
	auto savedY = widgetCursor.y;

    int offset = 0;
    int count = data::count(parentWidget->data);
    for (int index = 0; index < count; ++index) {
        data::select(widgetCursor.cursor, parentWidget->data, index);

        if (listWidget->listType == LIST_TYPE_VERTICAL) {
            if (offset < parentWidget->h) {
				widgetCursor.y = savedY + offset;
                enumWidget(widgetCursor, callback);
				offset += childWidget->h;
            } else {
                // TODO: add vertical scroll
                break;
            }
        } else {
            if (offset < parentWidget->w) {
				widgetCursor.x = savedX + offset;
				enumWidget(widgetCursor, callback);
				offset += childWidget->w;
            } else {
                // TODO: add horizontal scroll
                break;
            }
        }

        if (widgetCursor.previousState) {
			widgetCursor.previousState = nextWidgetState(widgetCursor.previousState);
            if (widgetCursor.previousState >= endOfContainerInPreviousState)
				widgetCursor.previousState = 0;
        }

        if (widgetCursor.currentState)
			widgetCursor.currentState = nextWidgetState(widgetCursor.currentState);
    }

	widgetCursor.x = savedX;
	widgetCursor.y = savedY;

    widgetCursor.widget = savedWidget;

    if (widgetCursor.currentState) {
        savedCurrentState->size = ((uint8_t *)widgetCursor.currentState) - ((uint8_t *)savedCurrentState);
    }

    data::select(widgetCursor.cursor, widgetCursor.widget->data, -1);

	widgetCursor.currentState = savedCurrentState;
	widgetCursor.previousState = savedPreviousState;
}

} // namespace gui
} // namespace eez
