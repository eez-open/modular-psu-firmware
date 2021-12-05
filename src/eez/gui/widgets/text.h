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

#pragma once

namespace eez {
namespace gui {

struct TextWidget : public Widget {
    AssetsPtr<const char> text;
    uint8_t flags;
}; 

struct TextWidgetState : public WidgetState {
    TextWidgetState(const WidgetCursor &widgetCursor) : WidgetState(widgetCursor) {
        auto widget = (const TextWidget *)widgetCursor.widget;

        flags.focused = isFocusWidget(widgetCursor);

        const Style *style = getStyle(overrideStyleHook(widgetCursor, widget->style));
        flags.blinking = g_isBlinkTime && styleIsBlink(style);
        
        const char *text = widget->text.ptr(widgetCursor.assets);
        data = !(text && text[0]) && widget->data ? get(widgetCursor, widget->data) : 0;
    }

    bool operator!=(const TextWidgetState& previousState) {
        return
            flags.focused != previousState.flags.focused ||
            flags.active != previousState.flags.active ||
            flags.blinking != previousState.flags.blinking ||
            data != previousState.data;
    }

    void draw(WidgetState *previousState) override;
};

void TextWidget_autoSize(Assets *assets, TextWidget& widget);

} // namespace gui
} // namespace eez
