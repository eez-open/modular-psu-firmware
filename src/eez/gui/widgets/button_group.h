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

struct ButtonGroupWidget : public Widget {
    int16_t selectedStyle;
};

struct ButtonGroupWidgetState : public WidgetState {
    ButtonGroupWidgetState(const WidgetCursor &widgetCursor) : WidgetState(widgetCursor) {
        auto widget = (const ButtonGroupWidget *)widgetCursor.widget;
        data = get(widgetCursor, widget->data);
    }

    bool operator!=(const ButtonGroupWidgetState& previousState) {
        return
            flags.active != previousState.flags.active ||
			data != previousState.data; 
    }

    void draw(WidgetState *previousState) override;
	bool hasOnTouch() override;
	void onTouch(Event &touchEvent) override;
};

} // namespace gui
} // namespace eez
