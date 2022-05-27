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

#include <eez/core/sound.h>
#include <eez/core/util.h>

#include <eez/gui/gui.h>
#include <eez/gui/widgets/switch.h>

static const uint32_t ANIMATION_DURATION = 150;

namespace eez {
namespace gui {

bool SwitchWidgetState::updateState() {
    WIDGET_STATE_START(SwitchWidget);

	if (changeStartTime != 0) {
		if (millis() - changeStartTime > ANIMATION_DURATION) {
			changeStartTime = 0;
		}
		hasPreviousState = false;
	}

    WIDGET_STATE(data, widget->data ? get(widgetCursor, widget->data) : 0);

    WIDGET_STATE_END()
}

void SwitchWidgetState::render() {
    const WidgetCursor &widgetCursor = g_widgetCursor;
    auto widget = (const ButtonWidget *)widgetCursor.widget;
    const Style *style = getStyle(widget->style);

	drawRectangle(
		widgetCursor.x, widgetCursor.y, widgetCursor.w, widgetCursor.h,
		nullptr,
		false,
		false
	);

    auto enabled = data.getInt() ? true : false;

	float t;
	if (changeStartTime) {
		t = 1.0f * (millis() - changeStartTime) / ANIMATION_DURATION;
		if (t > 1.0f) {
			t = 1.0f;
		}
		if (!enabled) {
			t = 1.0f - t;
		}
	} else {
		t = enabled ? 1.0f : 0;
	}

    double x = widgetCursor.x + style->paddingLeft;
	double y = widgetCursor.y + style->paddingTop;
	double w =
        widgetCursor.w -
        style->paddingLeft -
        style->paddingRight;
	double h =
        widgetCursor.h -
        style->paddingTop -
        style->paddingBottom;

    display::AggDrawing aggDrawing;
    display::aggInit(aggDrawing);

    display::setColor(style->borderColor);

	display::setBackColor(style->backgroundColor);
    display::fillRoundedRect(
		aggDrawing,
		x, y, x + w - 1, y + h - 1,
		style->borderSizeLeft, h, true, true,
		x + t * (w - 1), y, x + w - 1, y + h - 1
	);

	display::setBackColor(style->activeBackgroundColor);
	display::fillRoundedRect(
		aggDrawing,
		x, y, x + w - 1, y + h - 1,
		style->borderSizeLeft, h, true, true,
		x, y, x + t * (w - 1), y + h - 1
	);

	y += 2 + style->borderSizeLeft;
	h -= 2 * (2 + style->borderSizeLeft);

	auto xDisabled = x + 1 + style->borderSizeLeft;
	auto xEnabled = x + w - h - (1 + style->borderSizeLeft);
    x = xDisabled + t * (xEnabled - xDisabled);

    w = h;

    display::setBackColor(style->color);
    display::fillRoundedRect(aggDrawing, x, y, x + w - 1, y + h - 1, 1, h, false, true);
}

bool SwitchWidgetState::hasOnTouch() {
    return true;
}

void SwitchWidgetState::onTouch(const WidgetCursor &widgetCursor, Event &touchEvent) {
	if (touchEvent.type == EVENT_TYPE_TOUCH_UP) {
        set(widgetCursor, widgetCursor.widget->data, get(widgetCursor, widgetCursor.widget->data).getInt() ? 0 : 1);

		sound::playClick();

		changeStartTime = millis();
		if (changeStartTime == 0) {
			changeStartTime = 1;
		}
	}
}


} // namespace gui
} // namespace eez
