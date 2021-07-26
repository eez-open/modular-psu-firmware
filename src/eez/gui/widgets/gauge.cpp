/*
 * EEZ Modular Firmware
 * Copyright (C) 2020-present, Envox d.o.o.
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

#include <math.h>

#include <eez/util.h>

#include <eez/gui/gui.h>
#include <eez/gui/data.h>

namespace eez {
namespace gui {

EnumFunctionType GAUGE_enum = nullptr;

DrawFunctionType GAUGE_draw = [](const WidgetCursor &widgetCursor) {
    const Widget *widget = widgetCursor.widget;

    widgetCursor.currentState->size = sizeof(WidgetState);

    widgetCursor.currentState->data = get(widgetCursor, widget->data);

    bool refresh = !widgetCursor.previousState || widgetCursor.previousState->data != widgetCursor.currentState->data;

    if (refresh) {
		const Style* style = getStyle(widget->style);

		mcu::display::setColor16(RGB_TO_COLOR(255, 0, 0));
		mcu::display::fillRect(widgetCursor.x, widgetCursor.y, widgetCursor.x + widget->w - 1, widgetCursor.y + widget->h - 1, 0);

		mcu::display::setColor16(RGB_TO_COLOR(0, 255, 0));

		static const int BAR_WIDTH = 20;
		float min = 0.0f;
		float max = 40.0f;
		fillArcBar(
			widgetCursor.x + widget->w / 2,
			widgetCursor.y + widget->h - 4,
			(widget->w - 8) / 2 - BAR_WIDTH / 2,
			remap(widgetCursor.currentState->data.getFloat(), min, 180.0f, max, 0.0f),
			180.0f,
			BAR_WIDTH
		);
    }
};

OnTouchFunctionType GAUGE_onTouch = nullptr;

OnKeyboardFunctionType GAUGE_onKeyboard = nullptr;

} // namespace gui
} // namespace eez

#endif
