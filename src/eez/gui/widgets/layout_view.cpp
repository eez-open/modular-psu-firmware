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

#include <eez/gui/widgets/layout_view.h>

#include <eez/gui/assets.h>
#include <eez/gui/draw.h>
#include <eez/gui/widgets/container.h>

namespace eez {
namespace gui {

void LayoutViewWidget_enum(WidgetCursor &widgetCursor, EnumWidgetsCallback callback) {
    auto savedCurrentState = widgetCursor.currentState;
    auto savedPreviousState = widgetCursor.previousState;
    auto cursor = widgetCursor.cursor;

    const LayoutViewWidgetSpecific *layoutViewSpecific = GET_WIDGET_PROPERTY(widgetCursor.widget, specific, const LayoutViewWidgetSpecific *);

    data::setContext(widgetCursor.cursor, layoutViewSpecific->context);

    const Widget *layout = nullptr;

    if (widgetCursor.widget->data) {
        auto layoutValue = data::get(widgetCursor.cursor, widgetCursor.widget->data);

        if (widgetCursor.currentState) {
            widgetCursor.currentState->data = layoutValue;

            if (widgetCursor.previousState && widgetCursor.previousState->data != widgetCursor.currentState->data) {
                widgetCursor.previousState = 0;
            }
        }

        layout = getPageWidget(layoutValue.getInt());
    } else if (layoutViewSpecific->layout != -1) {
        layout = getPageWidget(layoutViewSpecific->layout);
    }
    
	if (layout) {
		const PageWidget *layoutSpecific = GET_WIDGET_PROPERTY(layout, specific, const PageWidget *);
		enumContainer(widgetCursor, callback, layoutSpecific->widgets);
	}

    widgetCursor.cursor = cursor;
    widgetCursor.currentState = savedCurrentState;
    widgetCursor.previousState = savedPreviousState;
}

} // namespace gui
} // namespace eez

#endif