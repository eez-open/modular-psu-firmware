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

#include <eez/gui/widgets/layout_view.h>

#include <eez/gui/assets.h>
#include <eez/gui/widgets/container.h>

namespace eez {
namespace gui {

void LayoutViewWidget_enum(OBJ_OFFSET widgetOffset, int16_t x, int16_t y, data::Cursor &cursor,
                           WidgetState *previousState, WidgetState *currentState,
                           EnumWidgetsCallback callback) {
    DECL_WIDGET(widget, widgetOffset);
    DECL_WIDGET_SPECIFIC(LayoutViewWidgetSpecific, layoutViewSpecific, widget);
	if (layoutViewSpecific->layout != -1) {
		DECL_WIDGET(layout, getPageOffset(layoutViewSpecific->layout));
		DECL_WIDGET_SPECIFIC(PageWidget, layoutSpecific, layout);
		enumContainer(x, y, cursor, previousState, currentState, callback, layoutSpecific->widgets);
	}
}

} // namespace gui
} // namespace eez
