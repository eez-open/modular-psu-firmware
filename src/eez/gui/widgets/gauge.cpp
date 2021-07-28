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

struct GaugeWidget : public Widget {
    int16_t min;
	int16_t max;
	int16_t threashold;
    int16_t barStyle;
	int16_t thresholdStyle;
};

EnumFunctionType GAUGE_enum = nullptr;

DrawFunctionType GAUGE_draw = [](const WidgetCursor &widgetCursor) {
    auto widget = (const GaugeWidget*)widgetCursor.widget;

    widgetCursor.currentState->size = sizeof(WidgetState);

    widgetCursor.currentState->data = get(widgetCursor, widget->data);

    bool refresh = !widgetCursor.previousState || widgetCursor.previousState->data != widgetCursor.currentState->data;

    if (refresh) {
		const Style* style = getStyle(widget->style);

		mcu::display::setColor(style->background_color);
		mcu::display::fillRect(widgetCursor.x, widgetCursor.y, widgetCursor.x + widget->w - 1, widgetCursor.y + widget->h - 1, 0);

		static const int BORDER_WIDTH = 32;
		static const int BAR_WIDTH = 16;
		float min = get(widgetCursor, widget->min).toFloat();
		float max = get(widgetCursor, widget->max).toFloat();

		const Style* barStyle = getStyle(widget->barStyle);
		mcu::display::setColor(barStyle->color);
		fillArcBar(
			widgetCursor.x + widget->w / 2,
			widgetCursor.y + widget->h - 4,
			(widget->w - 8) / 2 - BORDER_WIDTH / 2,
			remap(widgetCursor.currentState->data.getFloat(), min, 180.0f, max, 0.0f),
			180.0f,
			BAR_WIDTH
		);

		mcu::display::setColor(style->color);
		drawArcBar(
			widgetCursor.x + widget->w / 2,
			widgetCursor.y + widget->h - 4,
			(widget->w - 8) / 2 - BORDER_WIDTH / 2,
			0.0f,
			180.0f,
			BORDER_WIDTH
		);
    }
};

OnTouchFunctionType GAUGE_onTouch = nullptr;

OnKeyboardFunctionType GAUGE_onKeyboard = nullptr;

} // namespace gui
} // namespace eez

#endif
