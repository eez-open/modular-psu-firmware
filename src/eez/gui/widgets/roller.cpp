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

#include <stdio.h>

#include <eez/util.h>

#include <eez/gui/gui.h>
#include <eez/gui/widgets/roller.h>

namespace eez {
namespace gui {

bool RollerWidgetState::updateState() {
    const WidgetCursor &widgetCursor = g_widgetCursor;
	auto widget = (const RollerWidget *)widgetCursor.widget;
    bool hasPreviousState = widgetCursor.hasPreviousState && !dirty;
    WIDGET_STATE(data, get(widgetCursor, widget->data));
	if (!dragging && speed != 0) {
		offsetAtEndOfDragging += speed * 100;
		offset = int(offsetAtEndOfDragging);
		speed *= 0.9f;
		printf("%d, %g\n", offset, speed);

		if (fabs(speed) < 0.001) {
			speed = 0;
			const Style* style = getStyle(widget->style);

			font::Font font = styleGetFont(style);
			if (font) {
				auto textHeight = font.getHeight();

				auto oldPosition = data.getInt();
				int newPosition;
				if (offset > 0) {
					newPosition = (oldPosition * textHeight + offset + textHeight - 1) / textHeight;
				} else {
					newPosition = (oldPosition * textHeight + offset) / textHeight;
				}
				if (newPosition < 1) {
					newPosition = 1;
				}

				set(widgetCursor, widget->data, newPosition);
			}

			offset = 0;
		} else {
			hasPreviousState = false;
		}

	}
	return !hasPreviousState;
}

void RollerWidgetState::render() {
    const WidgetCursor &widgetCursor = g_widgetCursor;
    auto widget = (const RollerWidget *)widgetCursor.widget;
    const Style* style = getStyle(widget->style);

    font::Font font = styleGetFont(style);
	if (!font) {
		return;
	}

    drawRectangle(
        widgetCursor.x, widgetCursor.y, (int)widget->w, (int)widget->h,
        style
    );

    auto textHeight = font.getHeight();

	auto position = data.getInt();
	if (position < 1) {
		position = 1;
	}

    auto x = widgetCursor.x;
    auto y = widgetCursor.y + (widget->h - textHeight) / 2 - (position - 1) * textHeight - offset;

    display::setColor(style->color);

    for (int i = 1; ; i++) {
        char text[100];
        snprintf(text, sizeof(text), "%d", i);

		auto textLength = strlen(text);

        int textWidth = display::measureStr(text, textLength, font, 0);

        display::drawStr(
            text, textLength,
            x + (widget->w - textWidth) / 2, y,
            widgetCursor.x, widgetCursor.y, widgetCursor.x + widget->w - 1, widgetCursor.y + widget->h - 1,
            font,
            -1
        );

        y += textHeight;

        if (y >= widgetCursor.y + widget->h) {
            break;
        }
    }

    auto yCenter = widgetCursor.y + (widget->h - textHeight) / 2;
    display::drawRoundedRect(x, yCenter - 1, x + widget->w - 1, yCenter + textHeight + 2 - 1, 1, 4);
}

bool RollerWidgetState::hasOnTouch() {
    return true;
}

void RollerWidgetState::onTouch(const WidgetCursor &widgetCursor, Event &touchEvent) {
    if (touchEvent.type == EVENT_TYPE_TOUCH_DOWN) {
        yAtDown = touchEvent.y;
        offsetAtDown = offset;
		dragging = true;
		startTime = touchEvent.time;
    } else if (touchEvent.type == EVENT_TYPE_TOUCH_MOVE) {
        auto newOffset = offsetAtDown + yAtDown - touchEvent.y;
        if (newOffset != offset) {
            offset = newOffset;
			dirty = true;
        }
	} else if (touchEvent.type == EVENT_TYPE_TOUCH_UP) {
		uint32_t endTime = touchEvent.time;
		speed = 1.0f * offset / (endTime - startTime);
		offsetAtEndOfDragging = offset;
		dragging = false;
	}
}

} // namespace gui
} // namespace eez
