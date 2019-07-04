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

#include <eez/gui/widgets/container.h>

#include <eez/gui/assets.h>

namespace eez {
namespace gui {

void enumContainer(WidgetCursor &widgetCursor, EnumWidgetsCallback callback, List widgets) {
    auto savedCurrentState = widgetCursor.currentState;
	auto savedPreviousState = widgetCursor.previousState;

    WidgetState *endOfContainerInPreviousState = 0;
    if (widgetCursor.previousState)
        endOfContainerInPreviousState = nextWidgetState(widgetCursor.previousState);

    // move to the first child widget state
    if (widgetCursor.previousState)
        ++widgetCursor.previousState;
    if (widgetCursor.currentState)
        ++widgetCursor.currentState;

    auto savedWidgetOffset = widgetCursor.widgetOffset;
    auto savedWidget = widgetCursor.widget;

    for (uint32_t index = 0; index < widgets.count; ++index) {
		auto widgetOffset = getListItemOffset(widgets, index, sizeof(Widget));
		widgetCursor.widgetOffset = widgetOffset;

		// TODO optimize this
		DECL_WIDGET(widget, widgetCursor.widgetOffset);
		widgetCursor.widget = widget;

        enumWidget(widgetCursor, callback);

        if (widgetCursor.previousState) {
			widgetCursor.previousState = nextWidgetState(widgetCursor.previousState);
            if (widgetCursor.previousState >= endOfContainerInPreviousState)
				widgetCursor.previousState = 0;
        }

        if (widgetCursor.currentState)
			widgetCursor.currentState = nextWidgetState(widgetCursor.currentState);
    }

    widgetCursor.widgetOffset = savedWidgetOffset;
    widgetCursor.widget = savedWidget;

    if (widgetCursor.currentState) {
        savedCurrentState->size = ((uint8_t *)widgetCursor.currentState) - ((uint8_t *)savedCurrentState);
    }

	widgetCursor.currentState = savedCurrentState;
	widgetCursor.previousState = savedPreviousState;
}

void ContainerWidget_enum(WidgetCursor &widgetCursor, EnumWidgetsCallback callback) {
    DECL_WIDGET_SPECIFIC(ContainerWidget, container, widgetCursor.widget);
    enumContainer(widgetCursor, callback, container->widgets);
}

} // namespace gui
} // namespace eez
