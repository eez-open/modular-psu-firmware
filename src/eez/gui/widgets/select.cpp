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
#include <eez/gui/widgets/container.h>

namespace eez {
namespace gui {

struct SelectWidget : public Widget {
    ListOfAssetsPtr<Widget> widgets;
};

EnumFunctionType SELECT_enum = [](WidgetCursor &widgetCursor, EnumWidgetsCallback callback) {
	auto savedCurrentState = widgetCursor.currentState;
	auto savedPreviousState = widgetCursor.previousState;

    Value indexValue = get(widgetCursor, widgetCursor.widget->data);

    if (widgetCursor.currentState) {
		widgetCursor.currentState->data.clear();
		widgetCursor.currentState->data = indexValue;

        if (widgetCursor.previousState && widgetCursor.previousState->data != widgetCursor.currentState->data) {
            widgetCursor.previousState = 0;
        }
    }

    // move to the selected widget state
    if (widgetCursor.previousState) {
        ++widgetCursor.previousState;
    }
    if (widgetCursor.currentState) {
        ++widgetCursor.currentState;
    }

    auto savedWidget = widgetCursor.widget;

	auto containerWidget = (const ContainerWidget *)widgetCursor.widget;
    if (containerWidget->widgets.count > 0) {
        int err;
		int index = indexValue.toInt32(&err);
        if (err) {
            index = indexValue.getInt();
        }

	    auto widgetIndex = index < 0 || index >= (int)containerWidget->widgets.count ? 0 : index;
        widgetCursor.widget = containerWidget->widgets.item(widgetCursor.assets, widgetIndex);
        enumWidget(widgetCursor, callback);
    }

    widgetCursor.widget = savedWidget;

	if (widgetCursor.currentState) {
		savedCurrentState->size = sizeof(WidgetState) + widgetCursor.currentState->size;
	}

	widgetCursor.currentState = savedCurrentState;
	widgetCursor.previousState = savedPreviousState;

	if (widgetCursor.currentState) {
		widgetCursor.currentState->data.freeRef();
	}
};

DrawFunctionType SELECT_draw = nullptr;

OnTouchFunctionType SELECT_onTouch = nullptr;

OnKeyboardFunctionType SELECT_onKeyboard = nullptr;

} // namespace gui
} // namespace eez
