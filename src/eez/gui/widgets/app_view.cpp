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

#include <eez/gui/gui.h>

namespace eez {
namespace gui {

EnumFunctionType APP_VIEW_enum = [](WidgetCursor &widgetCursor, EnumWidgetsCallback callback) {
    Value appContextValue;
    DATA_OPERATION_FUNCTION(widgetCursor.widget->data, DATA_OPERATION_GET, widgetCursor, appContextValue);
    AppContext *appContext = appContextValue.getAppContext();

    WidgetCursor savedWidgetCursor = widgetCursor;
    widgetCursor.appContext = appContext;

    if (callback == drawWidgetCallback) {
        appContext->updateAppView(widgetCursor);
    } else {
		enumWidgets(widgetCursor, callback);
    }

    widgetCursor = savedWidgetCursor;
};

DrawFunctionType APP_VIEW_draw = [](const WidgetCursor &widgetCursor) {
    Value appContextValue;
    DATA_OPERATION_FUNCTION(widgetCursor.widget->data, DATA_OPERATION_GET, widgetCursor, appContextValue);
    AppContext *appContext = appContextValue.getAppContext();

    appContext->rect.x = widgetCursor.x;
    appContext->rect.y = widgetCursor.y;
    appContext->rect.w = widgetCursor.widget->w;
    appContext->rect.h = widgetCursor.widget->h;

    // clear background
    if (!appContext->isActivePageInternal() && appContext->getActivePageId() != PAGE_ID_NONE) {
        auto page = getPageAsset(appContext->getActivePageId());
		const Style* style = getStyle(page->style);
		mcu::display::setColor(style->background_color);

		mcu::display::fillRect(appContext->rect.x, appContext->rect.y, page->w, page->h);
	}
};

OnTouchFunctionType APP_VIEW_onTouch = nullptr;

OnKeyboardFunctionType APP_VIEW_onKeyboard = nullptr;

} // namespace gui
} // namespace eez

#endif