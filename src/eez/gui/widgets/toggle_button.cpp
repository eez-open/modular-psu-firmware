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

#include <eez/util.h>

#include <eez/gui/gui.h>
#include <eez/gui/widgets/toggle_button.h>

namespace eez {
namespace gui {

void ToggleButtonWidgetState::draw(WidgetState *previousStateBase) {
    auto previousState = (ToggleButtonWidgetState *)previousStateBase;
    bool refresh = !previousState || *this != *previousState;
    if (refresh) {
        auto widget = (const ToggleButtonWidget *)widgetCursor.widget;
        const Style* style = getStyle(widget->style);
        drawText(
            flags.enabled ? 
                widget->text2.ptr(widgetCursor.assets): 
                widget->text1.ptr(widgetCursor.assets),
            -1,
			widgetCursor.x, widgetCursor.y, (int)widget->w, (int)widget->h, style,
            flags.active, false, false, nullptr, nullptr, nullptr, nullptr);
    }
}

} // namespace gui
} // namespace eez
