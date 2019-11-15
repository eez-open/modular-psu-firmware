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

#include <assert.h>

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
    g_dataOperationsFunctions[widget->data](data::DATA_OPERATION_GET, (Cursor &)widgetCursor.cursor, appContextValue);
    AppContext *appContext = appContextValue.getAppContext();

    bool refresh = !widgetCursor.previousState;
    if (refresh && !appContext->isActivePageInternal() && appContext->getActivePageId() != INTERNAL_PAGE_ID_NONE) {
        appContext->x = widgetCursor.x;
        appContext->y = widgetCursor.y;
        appContext->width = widget->w;
        appContext->height = widget->h;

        // clear background
		const Widget *page = getPageWidget(appContext->getActivePageId());
        const Style* style = getStyle(page->style);
        mcu::display::setColor(style->background_color);

		mcu::display::fillRect(appContext->x, appContext->y, appContext->x + page->w - 1, appContext->y + page->h - 1);
    }
}

void AppViewWidget_enum(WidgetCursor &widgetCursor, EnumWidgetsCallback callback) {
    Value appContextValue;
    g_dataOperationsFunctions[widgetCursor.widget->data](data::DATA_OPERATION_GET, widgetCursor.cursor, appContextValue);
    AppContext *appContext = appContextValue.getAppContext();

    assert(g_appContext == widgetCursor.appContext);
    auto savedAppContext = widgetCursor.appContext;
    
    widgetCursor.appContext = appContext;
    g_appContext = appContext;

    if (callback == drawWidgetCallback) {
        g_appContext->updateAppView(widgetCursor);
    } else {
		enumWidgets(widgetCursor, callback);
    }

    widgetCursor.appContext = savedAppContext;
    g_appContext = savedAppContext;
}

} // namespace gui
} // namespace eez

#endif