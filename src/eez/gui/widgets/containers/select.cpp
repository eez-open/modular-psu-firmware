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

#include <eez/gui/gui.h>
#include <eez/gui/widgets/containers/select.h>

namespace eez {
namespace gui {

bool SelectWidgetState::updateState() {
    const WidgetCursor &widgetCursor = g_widgetCursor;

	bool hasPreviousState = widgetCursor.hasPreviousState;
	auto widget = (const SelectWidget *)widgetCursor.widget;
   	if (widget->widgets.count > 0) {
		Value indexValue = get(widgetCursor, widgetCursor.widget->data);
		int err;
		int index = indexValue.toInt32(&err);
		if (err) {
			index = indexValue.getInt();
		}
		WIDGET_STATE(widgetIndex, index < 0 || index >= (int)widget->widgets.count ? 0 : index);
	} else {
		WIDGET_STATE(widgetIndex, -1);
	}

	return !hasPreviousState;
}

void SelectWidgetState::render() {
    const WidgetCursor &widgetCursor = g_widgetCursor;
	auto widget = (const SelectWidget *)widgetCursor.widget;

	drawRectangle(
		widgetCursor.x, widgetCursor.y, (int)widget->w, (int)widget->h,
		getStyle(widget->style), false
	);
	
	repainted = true;
}

void SelectWidgetState::enumChildren() {
    WidgetCursor &widgetCursor = g_widgetCursor;

	bool savedRefreshed = false;
	if (g_findCallback == nullptr) {
		savedRefreshed = widgetCursor.refreshed;
		if (repainted) {
			repainted = false;
			widgetCursor.refreshed = true;
		}

		if (!widgetCursor.refreshed) {
			const Style* style = getStyle(widgetCursor.widget->style);
			widgetCursor.pushBackground(widgetCursor.x, widgetCursor.y, style, false);
		}
	}

	if (widgetIndex != -1) {
		auto widget = (const SelectWidget *)widgetCursor.widget;

		auto savedWidget = widgetCursor.widget;
		widgetCursor.widget = widget->widgets.item(widgetIndex);

        enumWidget();

		widgetCursor.widget = savedWidget;
	}

	if (g_findCallback == nullptr) {
		if (!widgetCursor.refreshed) {
			widgetCursor.popBackground();
		}

		widgetCursor.refreshed = savedRefreshed;
	}
}

} // namespace gui
} // namespace eez
