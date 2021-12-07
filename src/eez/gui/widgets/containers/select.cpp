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
	repainted = true;
}

void SelectWidgetState::enumChildren() {
    WidgetCursor &widgetCursor = g_widgetCursor;

    auto savedForceRefresh = widgetCursor.forceRefresh;
	if (repainted) {
		repainted = false;
		widgetCursor.forceRefresh = true;
	}
	
	if (widgetIndex != -1) {
		auto widget = (const SelectWidget *)widgetCursor.widget;

		auto savedWidget = widgetCursor.widget;
		widgetCursor.widget = widget->widgets.item(widgetCursor.assets, widgetIndex);

        enumWidget();

		widgetCursor.widget = savedWidget;
	}

	widgetCursor.forceRefresh = savedForceRefresh;
}

} // namespace gui
} // namespace eez
