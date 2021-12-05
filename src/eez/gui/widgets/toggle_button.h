/*
 * EEZ Modular Firmware
 * Copyright (C) 2021-present, Envox d.o.o.
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

struct ToggleButtonWidget : public Widget {
    AssetsPtr<const char> text1;
	AssetsPtr<const char> text2;
};

struct ToggleButtonWidgetState : public WidgetState {
    ToggleButtonWidgetState(const WidgetCursor &widgetCursor) : WidgetState(widgetCursor) {
        auto widget = (const ToggleButtonWidget *)widgetCursor.widget;
        flags.enabled = get(widgetCursor, widget->data).getInt() ? 1 : 0;
    }

    bool operator!=(const ToggleButtonWidgetState& previousState) {
        return
            flags.active != previousState.flags.active ||
            flags.enabled != previousState.flags.enabled;
    }

    void draw(WidgetState *previousState) override;
};

} // namespace gui
} // namespace eez