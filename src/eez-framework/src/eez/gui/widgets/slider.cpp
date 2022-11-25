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

#include <eez/conf-internal.h>

#if EEZ_OPTION_GUI

#include <eez/core/sound.h>
#include <eez/core/util.h>

#include <eez/gui/gui.h>
#include <eez/gui/widgets/slider.h>

namespace eez {
namespace gui {

bool SliderWidgetState::updateState() {
    WIDGET_STATE_START(SliderWidget);

    WIDGET_STATE(value, get(widgetCursor, widget->data));
    WIDGET_STATE(minValue, get(widgetCursor, widget->min));
    WIDGET_STATE(maxValue, get(widgetCursor, widget->max));

    WIDGET_STATE_END()
}

void SliderWidgetState::render() {
    const WidgetCursor &widgetCursor = g_widgetCursor;
    auto widget = (const ButtonWidget *)widgetCursor.widget;
    const Style *style = getStyle(widget->style);

	drawRectangle(
		widgetCursor.x, widgetCursor.y, widgetCursor.w, widgetCursor.h,
		nullptr,
		false,
		false
	);

    double x = widgetCursor.x + style->paddingLeft;
    double y = widgetCursor.y + style->paddingTop;
    double w = widgetCursor.w - style->paddingLeft - style->paddingRight;
    double h = widgetCursor.h - style->paddingTop - style->paddingBottom;

    double barX = x + h / 2.0;
    double barW = w - h;
    double barH = (h * 8.0) / 20.0;
	double barY = y + (h - barH) / 2;
	double barBorderRadius = barH / 2.0;

    double knobRelativePosition = (value.toDouble() - minValue.toDouble()) / (maxValue.toDouble() - minValue.toDouble());
    if (knobRelativePosition < 0) knobRelativePosition = 0;
    else if (knobRelativePosition > 1.0f) knobRelativePosition = 1.0f;

	double knobPosition = barX + knobRelativePosition * (barW - 1);

    double knobRadius = h / 2;
    double knobX = knobPosition;
    double knobY = y;
    double knobW = h;
    double knobH = h;

	display::setColor(style->borderColor);

    display::AggDrawing aggDrawing;
    display::aggInit(aggDrawing);

    display::setBackColor(style->backgroundColor);
	display::fillRoundedRect(aggDrawing, barX - barBorderRadius, barY, barX + barW + barBorderRadius - 1, barY + barH - 1, style->borderSizeLeft, barBorderRadius, false, true,
		x, y, x + w -1, y + h - 1);

	display::setBackColor(style->color);
	display::fillRoundedRect(aggDrawing, knobX - knobRadius, knobY, knobX - knobRadius + knobW - 1, knobY + knobH - 1, style->borderSizeLeft, knobRadius, false, true,
		x, y, x + w - 1, y + h - 1);
}

bool SliderWidgetState::hasOnTouch() {
    return true;
}

void SliderWidgetState::onTouch(const WidgetCursor &widgetCursor, Event &touchEvent) {
	auto widget = (const ButtonWidget *)widgetCursor.widget;
	const Style *style = getStyle(widget->style);

	double x = widgetCursor.x + style->paddingLeft;
    double w = widgetCursor.w - style->paddingLeft - style->paddingRight;
    double h = widgetCursor.h - style->paddingTop - style->paddingBottom;

    double knobW = h;

    double barX = x + h / 2.0;
    double barW = w - h;

    if (touchEvent.type == EVENT_TYPE_TOUCH_DOWN) {
		double knobRelativePosition = (value.toDouble() - minValue.toDouble()) / (maxValue.toDouble() - minValue.toDouble());
		if (knobRelativePosition < 0) knobRelativePosition = 0;
		else if (knobRelativePosition > 1.0f) knobRelativePosition = 1.0f;

		double knobPosition = barX + knobRelativePosition * (barW - 1);

		if (abs(touchEvent.x - knobPosition) < knobW * 2) {
            dragging = true;
            dragOrigin = touchEvent.x;

            dragStartPosition = knobPosition;
        }
    } else if (touchEvent.type == EVENT_TYPE_TOUCH_MOVE) {
		if (dragging) {
			double knobPosition = dragStartPosition + (touchEvent.x - dragOrigin);
			double knobRelativePosition = (knobPosition - barX) / (barW - 1);

			double min = minValue.toDouble();
			double max = maxValue.toDouble();
			double newValue = min + knobRelativePosition * (max - min);
			if (newValue < min) newValue = min;
			else if (newValue > max) newValue = max;

			Value tempValue = value;
			assignValue(tempValue, Value(newValue, VALUE_TYPE_DOUBLE));

			set(widgetCursor, widgetCursor.widget->data, tempValue);
		}
	} else if (touchEvent.type == EVENT_TYPE_TOUCH_UP) {
		dragging = false;
	}
}


} // namespace gui
} // namespace eez

#endif // EEZ_OPTION_GUI
