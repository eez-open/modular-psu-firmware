/*
 * EEZ Generic Firmware
 * Copyright (C) 2018-present, Envox d.o.o.
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

#include <eez/gui/widgets/app_view.h>

#include <eez/gui/app_context.h>
#include <eez/gui/assets.h>
#include <eez/gui/widget.h>

namespace eez {
namespace gui {

void AppViewWidget_draw(const WidgetCursor &widgetCursor) {
    widgetCursor.currentState->size = sizeof(WidgetState);

    const Widget *widget = widgetCursor.widget;
    Value appContextValue;
    g_dataOperationsFunctions[widget->data](data::DATA_OPERATION_GET, (Cursor &)widgetCursor.cursor,
                                            appContextValue);
    AppContext *appContext = appContextValue.getAppContext();

    bool refresh = !widgetCursor.previousState;
    if (refresh) {
        // clear background
        DECL_WIDGET(page, getPageOffset(appContext->getActivePageId()));
        DECL_WIDGET_STYLE(style, page);
        mcu::display::setColor(style->background_color);
        mcu::display::fillRect(widgetCursor.x + page->x, widgetCursor.y + page->y,
                               page->x + page->w - 1, page->y + page->h - 1);

        g_painted = true;
    }
}

void AppViewWidget_enum(OBJ_OFFSET widgetOffset, int16_t x, int16_t y, data::Cursor &cursor,
                        WidgetState *previousState, WidgetState *currentState,
                        EnumWidgetsCallback callback) {
    DECL_WIDGET(widget, widgetOffset);

    Value appContextValue;
    g_dataOperationsFunctions[widget->data](data::DATA_OPERATION_GET, cursor, appContextValue);
    AppContext *appContext = appContextValue.getAppContext();

    AppContext *saved = g_appContext;
    g_appContext = appContext;

    if (callback == drawWidgetCallback) {
        g_appContext->updateAppView(x, y, cursor, previousState, currentState);
    } else {
		enumWidgets(x, y, cursor, previousState,currentState, callback);
    }

    g_appContext = saved;
}

} // namespace gui
} // namespace eez
