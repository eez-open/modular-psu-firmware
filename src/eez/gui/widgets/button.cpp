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

#if EEZ_OPTION_GUI || !defined(EEZ_OPTION_GUI)

#include <eez/core/util.h>

#include <eez/gui/gui.h>
#include <eez/gui/widgets/button.h>

static const size_t MAX_TEXT_LEN = 128;

namespace eez {
namespace gui {

bool ButtonWidgetState::updateState() {
    WIDGET_STATE_START(ButtonWidget);

    WIDGET_STATE(flags.active, g_isActiveWidget);

    auto enabled = get(widgetCursor, widget->enabled);
    WIDGET_STATE(flags.enabled, enabled.getType() == VALUE_TYPE_UNDEFINED || enabled.getInt() ? 1 : 0);

    const Style *style = getStyle(flags.enabled ? widget->style : widget->disabledStyle);
    WIDGET_STATE(flags.blinking, g_isBlinkTime && (isBlinking(widgetCursor, widget->data) || styleIsBlink(style)));

    WIDGET_STATE(data, widget->data ? get(widgetCursor, widget->data) : 0);

    WIDGET_STATE_END()
}

void ButtonWidgetState::render() {
    const WidgetCursor &widgetCursor = g_widgetCursor;

    auto widget = (const ButtonWidget *)widgetCursor.widget;
    const Style *style = getStyle(flags.enabled ? widget->style : widget->disabledStyle);

    if (widget->data) {
        if (data.isString()) {
            drawText(data.getString(), -1, widgetCursor.x, widgetCursor.y, widgetCursor.w, widgetCursor.h, style, flags.active, flags.blinking);
        } else {
            char text[MAX_TEXT_LEN + 1];
            data.toText(text, sizeof(text));
            drawText(text, -1, widgetCursor.x, widgetCursor.y, widgetCursor.w, widgetCursor.h, style, flags.active, flags.blinking);
        }
    } else if (widget->text) {
        drawText(static_cast<const char *>(widget->text), -1, widgetCursor.x, widgetCursor.y, widgetCursor.w, widgetCursor.h, style, flags.active, flags.blinking);
    }
}

} // namespace gui
} // namespace eez

#endif // EEZ_OPTION_GUI || !defined(EEZ_OPTION_GUI)
