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

#include <eez/util.h>
#include <eez/sound.h>
#include <eez/keyboard.h>

#include <eez/gui/gui.h>

namespace eez {
namespace gui {

struct UpDownWidget {
    uint16_t buttonsStyle;
    AssetsPtr<const char> downButtonText;
    AssetsPtr<const char> upButtonText;
};

FixPointersFunctionType UP_DOWN_fixPointers = [](Widget *widget, Assets *assets) {
    UpDownWidget *upDownWidget = (UpDownWidget *)widget->specific;
    upDownWidget->downButtonText = (const char *)((uint8_t *)(void *)assets->document + (uint32_t)upDownWidget->downButtonText);
    upDownWidget->upButtonText = (const char *)((uint8_t *)(void *)assets->document + (uint32_t)upDownWidget->upButtonText);
};

EnumFunctionType UP_DOWN_enum = nullptr;

enum UpDownWidgetSegment {
    UP_DOWN_WIDGET_SEGMENT_TEXT,
    UP_DOWN_WIDGET_SEGMENT_DOWN_BUTTON,
    UP_DOWN_WIDGET_SEGMENT_UP_BUTTON
};

static UpDownWidgetSegment g_segment;
static WidgetCursor g_selectedWidget;

DrawFunctionType UP_DOWN_draw = [](const WidgetCursor &widgetCursor) {
    const Widget *widget = widgetCursor.widget;
    const UpDownWidget *upDownWidget = GET_WIDGET_PROPERTY(widget, specific, const UpDownWidget *);

    widgetCursor.currentState->size = sizeof(WidgetState);
    widgetCursor.currentState->data = get(widgetCursor.cursor, widget->data);
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
                 false, false, nullptr, nullptr, nullptr, nullptr);

        char text[64];
        widgetCursor.currentState->data.toText(text, sizeof(text));
        const Style *style = getStyle(widget->style);
        drawText(text, -1, widgetCursor.x + buttonWidth, widgetCursor.y,
                 (int)(widget->w - 2 * buttonWidth), (int)widget->h, style, false, false,
                 false, nullptr, nullptr, nullptr, nullptr);

        drawText(GET_WIDGET_PROPERTY(upDownWidget, upButtonText, const char *), -1, widgetCursor.x + widget->w - buttonWidth, widgetCursor.y,
                 buttonWidth, (int)widget->h, buttonsStyle,
                 widgetCursor.currentState->flags.active &&
                     g_segment == UP_DOWN_WIDGET_SEGMENT_UP_BUTTON,
                 false, false, nullptr, nullptr, nullptr, nullptr);
    }
};

void upDown(const WidgetCursor &widgetCursor, UpDownWidgetSegment segment) {
    g_segment = segment;

    const Widget *widget = widgetCursor.widget;

    int value = get(widgetCursor.cursor, widget->data).getInt();

    int newValue = value;

    if (g_segment == UP_DOWN_WIDGET_SEGMENT_DOWN_BUTTON) {
        --newValue;
    } else if (g_segment == UP_DOWN_WIDGET_SEGMENT_UP_BUTTON) {
        ++newValue;
    }

    int min = getMin(widgetCursor.cursor, widget->data).getInt();
    if (newValue < min) {
        newValue = min;
    }

    int max = getMax(widgetCursor.cursor, widget->data).getInt();
    if (newValue > max) {
        newValue = max;
    }

    if (newValue != value) {
        set(widgetCursor.cursor, widget->data, newValue);
    }
}

OnTouchFunctionType UP_DOWN_onTouch = [](const WidgetCursor &widgetCursor, Event &touchEvent) {
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
};

OnKeyboardFunctionType UP_DOWN_onKeyboard = [](const WidgetCursor &widgetCursor, uint8_t key, uint8_t mod) {
    if (mod == 0) {
        if (key == KEY_LEFTARROW || key == KEY_DOWNARROW) {
            upDown(widgetCursor, UP_DOWN_WIDGET_SEGMENT_DOWN_BUTTON);
            sound::playClick();
            return true;
        } else if (key == KEY_RIGHTARROW || key == KEY_UPARROW) {
            upDown(widgetCursor, UP_DOWN_WIDGET_SEGMENT_UP_BUTTON);
            sound::playClick();
            return true;
        }
    }
    return false;
};

} // namespace gui
} // namespace eez

#endif
