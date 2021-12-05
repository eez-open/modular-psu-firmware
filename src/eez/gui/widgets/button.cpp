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
#include <eez/gui/widgets/button.h>

static const size_t MAX_TEXT_LEN = 128;

namespace eez {
namespace gui {

void ButtonWidgetState::draw(WidgetState *previousStateBase) {
    auto previousState = (ButtonWidgetState *)previousStateBase;
    bool refresh = !previousState || *this != *previousState;
    if (refresh) {
        auto widget = (const ButtonWidget *)widgetCursor.widget;
        const Style *style = getStyle(flags.enabled ? widget->style : widget->disabledStyle);
        
        if (widget->data) {
            if (data.isString()) {
                drawText(data.getString(), -1, widgetCursor.x,
                         widgetCursor.y, (int)widget->w, (int)widget->h, style,
                         flags.active,
                         flags.blinking, false, nullptr, nullptr, nullptr, nullptr);
            } else {
				char text[MAX_TEXT_LEN + 1];
				data.toText(text, sizeof(text));

                drawText(text, -1, widgetCursor.x, widgetCursor.y, (int)widget->w, (int)widget->h,
                         style, flags.active,
                         flags.blinking, false, nullptr, nullptr, nullptr, nullptr);
            }
        } else {
            drawText(widget->text.ptr(widgetCursor.assets), -1, widgetCursor.x, widgetCursor.y, (int)widget->w, (int)widget->h,
                     style, flags.active,
                     flags.blinking, false, nullptr, nullptr, nullptr, nullptr);
        }
    }
}

} // namespace gui
} // namespace eez
