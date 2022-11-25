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

#include <eez/conf-internal.h>

#if EEZ_OPTION_GUI

#include <eez/core/util.h>

#include <eez/gui/gui.h>
#include <eez/gui/widgets/progress.h>

namespace eez {
namespace gui {

static const uint8_t PROGRESS_WIDGET_ORIENTATION_HORIZONTAL = 0;
// static const uint8_t PROGRESS_WIDGET_ORIENTATION_VERTICAL = 1;

bool ProgressWidgetState::updateState() {
    WIDGET_STATE_START(ProgressWidget);

    WIDGET_STATE(data, get(widgetCursor, widget->data));
    WIDGET_STATE(min, get(widgetCursor, widget->min));
    WIDGET_STATE(max, get(widgetCursor, widget->max));

    WIDGET_STATE_END()
}

void ProgressWidgetState::render() {
    const WidgetCursor &widgetCursor = g_widgetCursor;

    auto widget = (const ProgressWidget *)widgetCursor.widget;
    const Style* style = getStyle(widget->style);

    drawRectangle(widgetCursor.x, widgetCursor.y, widgetCursor.w, widgetCursor.h, style);

    int percentFrom;
    int percentTo;

    if (widgetCursor.flowState) {
        float fmin = min.toFloat();
        float fmax = max.toFloat();
        float value = data.toFloat();
        percentFrom = 0;
        percentTo = (value - fmin) * 100.0f / (fmax - fmin);
    } else {
        if (data.getType() == VALUE_TYPE_RANGE) {
            percentFrom = data.getRangeFrom();
            percentTo = data.getRangeTo();
        } else {
            percentFrom = 0;
            percentTo = data.getInt();
        }
    }

    percentFrom = clamp(percentFrom, 0, 100.0f);
    percentTo = clamp(percentTo, 0, 100.0f);
    if (percentFrom > percentTo) {
        percentFrom = percentTo;
    }

    auto isHorizontal = widget->orientation == PROGRESS_WIDGET_ORIENTATION_HORIZONTAL;
    if (isHorizontal) {
        auto xFrom = percentFrom * widgetCursor.w / 100;
        auto xTo = percentTo * widgetCursor.w / 100;
        if (g_isRTL) {
            drawRectangle(widgetCursor.x + widgetCursor.w - xTo, widgetCursor.y, xTo - xFrom, widgetCursor.h, style, true);
        } else {
            drawRectangle(widgetCursor.x + xFrom, widgetCursor.y, xTo - xFrom, widgetCursor.h, style, true);
        }
    } else {
        auto yFrom = percentFrom * widgetCursor.h / 100;
        auto yTo = percentTo * widgetCursor.h / 100;
        drawRectangle(widgetCursor.x, widgetCursor.y + widgetCursor.h - yTo, widgetCursor.w, yTo - yFrom, style, true);
    }
}

} // namespace gui
} // namespace eez

#endif // EEZ_OPTION_GUI
