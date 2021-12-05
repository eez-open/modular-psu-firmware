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

struct ButtonWidget : public Widget {
    AssetsPtr<const char> text;
    int16_t enabled;
    int16_t disabledStyle;
};

struct ButtonWidgetState : public WidgetState {
    ButtonWidgetState(const WidgetCursor &widgetCursor) : WidgetState(widgetCursor) {
        auto widget = (const ButtonWidget *)widgetCursor.widget;

        auto enabled = get(widgetCursor, widget->enabled);
        flags.enabled = enabled.getType() == VALUE_TYPE_UNDEFINED || get(widgetCursor, widget->enabled).getInt() ? 1 : 0;

        const Style *style = getStyle(flags.enabled ? widget->style : widget->disabledStyle);
        flags.blinking = g_isBlinkTime && (isBlinking(widgetCursor, widget->data) || styleIsBlink(style));

        data = widget->data ? get(widgetCursor, widget->data) : 0;
    }

    bool operator!=(const ButtonWidgetState& previousState) {
        return
            flags.active != previousState.flags.active ||
            flags.enabled != previousState.flags.enabled ||
            flags.blinking != previousState.flags.blinking ||
            data != previousState.data;
    }

    void draw(WidgetState *previousState) override;
};

} // namespace gui
} // namespace eez
