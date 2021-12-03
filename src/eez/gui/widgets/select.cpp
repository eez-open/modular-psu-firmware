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
#include <eez/gui/widgets/select.h>

namespace eez {
namespace gui {

void SelectWidgetState::draw() {
    Value indexValue = get(widgetCursor, widgetCursor.widget->data);
    data = indexValue;

	auto selectWidget = (const SelectWidget *)widgetCursor.widget;
    if (selectWidget->widgets.count > 0) {
        int err;
		int index = indexValue.toInt32(&err);
        if (err) {
            index = indexValue.getInt();
        }

	    auto widgetIndex = index < 0 || index >= (int)selectWidget->widgets.count ? 0 : index;

        WidgetCursor childWidgetCursor = getFirstChildWidgetCursor();

        childWidgetCursor.widget = selectWidget->widgets.item(widgetCursor.assets, widgetIndex);

        if (widgetCursor.previousState && widgetCursor.previousState->data != data) {
            childWidgetCursor.previousState = 0;
        }

        enumWidget(childWidgetCursor);

        childWidgetCursor.currentState = nextWidgetState(childWidgetCursor.currentState);
        widgetStateSize = (uint8_t *)childWidgetCursor.currentState - (uint8_t *)this;
    }
}

} // namespace gui
} // namespace eez
