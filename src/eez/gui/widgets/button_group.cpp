/*
* EEZ Generic Firmware
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

#include <eez/sound.h>
#include <eez/util.h>

#include <eez/gui/gui.h>
#include <eez/gui/widgets/button_group.h>

namespace eez {
namespace gui {

void drawButtons(const Widget *widget, int x, int y, const Style *style, const Style *selectedStyle, int selectedButton, int count) {
    if (widget->w > widget->h) {
        // horizontal orientation
        display::setColor(style->background_color);
        display::fillRect(x, y, x + widget->w - 1, y + widget->h - 1);

        int w = widget->w / count;
        x += (widget->w - w * count) / 2;
        int h = widget->h;
        for (Cursor i = 0; i < count; i++) {
            char text[32];
			WidgetCursor widgetCursor;
			widgetCursor = i;
            getLabel(widgetCursor, widget->data, text, 32);
            drawText(text, -1, x, y, w, h, i == selectedButton ? selectedStyle : style, false, false, false, nullptr, nullptr, nullptr, nullptr);
            x += w;
        }
    } else {
        // vertical orientation
        int w = widget->w;
        int h = widget->h / count;

        int bottom = y + widget->h - 1;
        int topPadding = (widget->h - h * count) / 2;

        if (topPadding > 0) {
            display::setColor(style->background_color);
            display::fillRect(x, y, x + widget->w - 1, y + topPadding - 1);

            y += topPadding;
        }

        int labelHeight = MIN(w, h);
        int yOffset = (h - labelHeight) / 2;

		WidgetCursor widgetCursor;
		
		for (Cursor i = 0; i < count; i++) {
            if (yOffset > 0) {
                display::setColor(style->background_color);
                display::fillRect(x, y, x + widget->w - 1, y + yOffset - 1);
            }

            char text[32];
			widgetCursor = i;
            getLabel(widgetCursor, widget->data, text, 32);
            drawText(text, -1, x, y + yOffset, w, labelHeight, i == selectedButton ? selectedStyle: style, false, false, false, nullptr, nullptr, nullptr, nullptr);

            int b = y + yOffset + labelHeight;

            y += h;

            if (b < y) {
                display::setColor(style->background_color);
                display::fillRect(x, b, x + widget->w - 1, y - 1);
            }
        }

        if (y <= bottom) {
            display::setColor(style->background_color);
            display::fillRect(x, y, x + widget->w - 1, bottom);
        }
    }
}

void ButtonGroupWidgetState::draw() {
    auto widget = (const ButtonGroupWidget *)widgetCursor.widget;

    data = get(widgetCursor, widget->data);

    bool refresh =
        !widgetCursor.previousState ||
        widgetCursor.previousState->flags.active != flags.active ||
        widgetCursor.previousState->data != data;

    if (refresh) {
        const Style* style = getStyle(widget->style);
        const Style* selectedStyle = getStyle(widget->selectedStyle);

        drawButtons(widget, widgetCursor.x, widgetCursor.y, style, selectedStyle, data.getInt(), count(widgetCursor, widget->data));
    }
}

bool ButtonGroupWidgetState::hasOnTouch() {
    return true;
}

void ButtonGroupWidgetState::onTouch(Event &touchEvent) {
    if (touchEvent.type == EVENT_TYPE_TOUCH_DOWN) {
        const Widget *widget = widgetCursor.widget;

        int count_ = count(widgetCursor, widget->data);

        int selectedButton;
        if (widget->w > widget->h) {
            int w = widget->w / count_;
            int x = widgetCursor.x + (widget->w - w * count_) / 2;

            selectedButton = (touchEvent.x - x) / w;
        } else {
            int h = widget->h / count_;
            int y = widgetCursor.y + (widget->h - h * count_) / 2;
            selectedButton = (touchEvent.y - y) / h;
        }

        if (selectedButton >= 0 && selectedButton < count_) {
            set(widgetCursor, widget->data, selectedButton);
            sound::playClick();
        }
    }
}

} // namespace gui
} // namespace eez
