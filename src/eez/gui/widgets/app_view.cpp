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
    DATA_OPERATION_FUNCTION(widgetCursor.widget->data, DATA_OPERATION_GET, widgetCursor.cursor, appContextValue);
    AppContext *appContext = appContextValue.getAppContext();

    WidgetCursor savedWidgetCursor = widgetCursor;
    widgetCursor.appContext = appContext;

    if (callback == drawWidgetCallback) {
        appContext->updateAppView(widgetCursor);
    } else {
		enumWidgets(widgetCursor, callback);
    }

    if (widgetCursor.currentState) {
        savedWidgetCursor.currentState->size = ((uint8_t *)widgetCursor.currentState) - ((uint8_t *)savedWidgetCursor.currentState);
    }

    widgetCursor = savedWidgetCursor;
};

DrawFunctionType APP_VIEW_draw = [](const WidgetCursor &widgetCursor) {
    widgetCursor.currentState->size = sizeof(WidgetState);

    const Widget *widget = widgetCursor.widget;
    Value appContextValue;
    DATA_OPERATION_FUNCTION(widget->data, DATA_OPERATION_GET, widgetCursor.cursor, appContextValue);
    AppContext *appContext = appContextValue.getAppContext();

    bool refresh = !widgetCursor.previousState;
    if (refresh && !appContext->isActivePageInternal() && appContext->getActivePageId() != PAGE_ID_NONE) {
        appContext->rect.x = widgetCursor.x;
        appContext->rect.y = widgetCursor.y;
        appContext->rect.w = widget->w;
        appContext->rect.h = widget->h;

        // clear background
		const Widget *page = getPageWidget(appContext->getActivePageId());
        const Style* style = getStyle(page->style);
        mcu::display::setColor(style->background_color);

		mcu::display::fillRect(appContext->rect.x, appContext->rect.y, appContext->rect.x + page->w - 1, appContext->rect.y + page->h - 1);
    }
};

OnTouchFunctionType APP_VIEW_onTouch = nullptr;

OnKeyboardFunctionType APP_VIEW_onKeyboard = nullptr;

} // namespace gui
} // namespace eez

#endif