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

#include <eez/gui/widgets/up_down.h>

#include <eez/gui/draw.h>
#include <eez/gui/gui.h>
#include <eez/util.h>
#include <eez/sound.h>

namespace eez {
namespace gui {

enum UpDownWidgetSegment {
    UP_DOWN_WIDGET_SEGMENT_TEXT,
    UP_DOWN_WIDGET_SEGMENT_DOWN_BUTTON,
    UP_DOWN_WIDGET_SEGMENT_UP_BUTTON
};

static UpDownWidgetSegment g_segment;
static WidgetCursor g_selectedWidget;

#if OPTION_SDRAM
void UpDownWidget_fixPointers(Widget *widget) {
    UpDownWidget *upDownWidget = (UpDownWidget *)widget->specific;
    upDownWidget->downButtonText = (const char *)((uint8_t *)g_document + (uint32_t)upDownWidget->downButtonText);
    upDownWidget->upButtonText = (const char *)((uint8_t *)g_document + (uint32_t)upDownWidget->upButtonText);
}
#endif

void UpDownWidget_draw(const WidgetCursor &widgetCursor) {
    const Widget *widget = widgetCursor.widget;
    const UpDownWidget *upDownWidget = GET_WIDGET_PROPERTY(widget, specific, const UpDownWidget *);

    widgetCursor.currentState->size = sizeof(WidgetState);
    widgetCursor.currentState->data = data::get(widgetCursor.cursor, widget->data);
    widgetCursor.currentState->flags.active = g_selectedWidget == widgetCursor;

    bool refresh =
        !widgetCursor.previousState ||
        widgetCursor.previousState->flags.active != widgetCursor.currentState->flags.active ||
        widgetCursor.previousState->data != widgetCursor.currentState->data;

    if (refresh) {
        const Style *buttonsStyle = getStyle(upDownWidget->buttonsStyle);

        font::Font buttonsFont = styleGetFont(buttonsStyle);
        int buttonWidth = buttonsFont.getHeight();

        drawText(GET_WIDGET_PROPERTY(upDownWidget, downButtonText, const char *), -1, widgetCursor.x, widgetCursor.y, buttonWidth, (int)widget->h,
                 buttonsStyle,
                 widgetCursor.currentState->flags.active &&
                     g_segment == UP_DOWN_WIDGET_SEGMENT_DOWN_BUTTON,
                 false, false, nullptr, nullptr);

        char text[64];
        widgetCursor.currentState->data.toText(text, sizeof(text));
        const Style *style = getStyle(widget->style);
        drawText(text, -1, widgetCursor.x + buttonWidth, widgetCursor.y,
                 (int)(widget->w - 2 * buttonWidth), (int)widget->h, style, false, false,
                 false, nullptr, nullptr);

        drawText(GET_WIDGET_PROPERTY(upDownWidget, upButtonText, const char *), -1, widgetCursor.x + widget->w - buttonWidth, widgetCursor.y,
                 buttonWidth, (int)widget->h, buttonsStyle,
                 widgetCursor.currentState->flags.active &&
                     g_segment == UP_DOWN_WIDGET_SEGMENT_UP_BUTTON,
                 false, false, nullptr, nullptr);
    }
}

void upDown(const WidgetCursor &widgetCursor, UpDownWidgetSegment segment) {
    g_segment = segment;

    const Widget *widget = widgetCursor.widget;

    int value = data::get(widgetCursor.cursor, widget->data).getInt();

    int newValue = value;

    if (g_segment == UP_DOWN_WIDGET_SEGMENT_DOWN_BUTTON) {
        --newValue;
    } else if (g_segment == UP_DOWN_WIDGET_SEGMENT_UP_BUTTON) {
        ++newValue;
    }

    int min = data::getMin(widgetCursor.cursor, widget->data).getInt();
    if (newValue < min) {
        newValue = min;
    }

    int max = data::getMax(widgetCursor.cursor, widget->data).getInt();
    if (newValue > max) {
        newValue = max;
    }

    if (newValue != value) {
        data::set(widgetCursor.cursor, widget->data, newValue, 0);
    }
}

void UpDownWidget_onTouch(const WidgetCursor &widgetCursor, Event &touchEvent) {
    const Widget *widget = widgetCursor.widget;

    if (touchEvent.type == EVENT_TYPE_TOUCH_DOWN || touchEvent.type == EVENT_TYPE_AUTO_REPEAT) {
        if (touchEvent.type == EVENT_TYPE_TOUCH_DOWN) {
            g_selectedWidget = widgetCursor;
        }

        if (touchEvent.x < widgetCursor.x + widget->w / 2) {
            upDown(widgetCursor, UP_DOWN_WIDGET_SEGMENT_DOWN_BUTTON);
        } else {
            upDown(widgetCursor, UP_DOWN_WIDGET_SEGMENT_UP_BUTTON);
        }

        sound::playClick();
    } else if (touchEvent.type == EVENT_TYPE_TOUCH_UP) {
        g_selectedWidget = 0;
    }
}

} // namespace gui
} // namespace eez

#endif