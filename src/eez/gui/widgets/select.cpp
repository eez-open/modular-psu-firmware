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

#include <eez/gui/widgets/select.h>

#include <eez/gui/assets.h>
#include <eez/gui/draw.h>
#include <eez/gui/gui.h>
#include <eez/gui/widgets/container.h>

namespace eez {
namespace gui {

#if OPTION_SDRAM
void SelectWidget_fixPointers(Widget *widget) {
    SelectWidget *selectWidget = (SelectWidget *)widget->specific;
    WidgetList_fixPointers(selectWidget->widgets);
}
#endif

void SelectWidget_enum(WidgetCursor &widgetCursor, EnumWidgetsCallback callback) {
	auto savedCurrentState = widgetCursor.currentState;
	auto savedPreviousState = widgetCursor.previousState;

    data::Value indexValue = data::get(widgetCursor.cursor, widgetCursor.widget->data);
    if (indexValue.getType() == VALUE_TYPE_NONE) {
        indexValue = data::Value(0);
    }

    if (widgetCursor.currentState) {
		widgetCursor.currentState->data = indexValue;
    }

    if (widgetCursor.previousState && widgetCursor.previousState->data != widgetCursor.currentState->data) {
		widgetCursor.previousState = 0;
    }

    // move to the selected widget state
    if (widgetCursor.previousState) {
        ++widgetCursor.previousState;
    }
    if (widgetCursor.currentState) {
        ++widgetCursor.currentState;
    }

    auto savedWidget = widgetCursor.widget;

	const ContainerWidget *containerWidget = GET_WIDGET_PROPERTY(widgetCursor.widget, specific, const ContainerWidget *);
	
    widgetCursor.widget = GET_WIDGET_LIST_ELEMENT(containerWidget->widgets, + indexValue.getInt());

    enumWidget(widgetCursor, callback);

    widgetCursor.widget = savedWidget;

    if (widgetCursor.currentState) {
        savedCurrentState->size = sizeof(WidgetState) + widgetCursor.currentState->size;
    }

	widgetCursor.currentState = savedCurrentState;
	widgetCursor.previousState = savedPreviousState;
}

} // namespace gui
} // namespace eez
