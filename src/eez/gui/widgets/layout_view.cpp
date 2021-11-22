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

#include <eez/gui/gui.h>
#include <eez/gui/widgets/container.h>
#include <eez/gui/widgets/layout_view.h>

namespace eez {
namespace gui {

int getLayoutId(const WidgetCursor &widgetCursor) {
    if (widgetCursor.widget->data) {
        auto layoutValue = get(widgetCursor, widgetCursor.widget->data);
        return layoutValue.getInt();
    }
    
    auto layoutView = (const LayoutViewWidget *)widgetCursor.widget;
    return layoutView->layout;
}

EnumFunctionType LAYOUT_VIEW_enum = [](WidgetCursor &widgetCursor, EnumWidgetsCallback callback) {
    auto cursor = widgetCursor.cursor;

	auto layoutView = (const LayoutViewWidget *)widgetCursor.widget;

    if (widgetCursor.previousState && (widgetCursor.previousState->data != widgetCursor.currentState->data || ((LayoutViewWidgetState *)widgetCursor.previousState)->context != ((LayoutViewWidgetState *)widgetCursor.currentState)->context)) {
        widgetCursor.previousState = 0;
    }

    Value oldContext;
    Value newContext;
    if (layoutView->context) {
        setContext(widgetCursor, layoutView->context, oldContext, newContext);
    }

    int layoutId = getLayoutId(widgetCursor);
	auto flowState = widgetCursor.flowState;
    auto layout = getPageAsset(layoutId, widgetCursor);

    if (layout) {
		auto layoutView = (PageAsset *)layout;
		enumContainer(widgetCursor, callback, layoutView->widgets);
	} else {
		if (widgetCursor.currentState) {
			widgetCursor.currentState->size = 0;
		}
    }

    if (layoutView->context) {
        restoreContext(widgetCursor, layoutView->context, oldContext);
    }

	widgetCursor.flowState = flowState;
    widgetCursor.cursor = cursor;

};

DrawFunctionType LAYOUT_VIEW_draw = [](const WidgetCursor &widgetCursor) {
	auto widget = (const LayoutViewWidget *)widgetCursor.widget;

	((LayoutViewWidgetState *)widgetCursor.currentState)->context.clear();

    Value oldContext;
    Value newContext;
    if (widget->context) {
        setContext((WidgetCursor &)widgetCursor, widget->context, oldContext, newContext);
        ((LayoutViewWidgetState *)widgetCursor.currentState)->context = newContext;
    } else {
        ((LayoutViewWidgetState *)widgetCursor.currentState)->context = Value();
    }

    widgetCursor.currentState->data.clear();
    widgetCursor.currentState->data = getLayoutId(widgetCursor);

    if (widget->context) {
        restoreContext((WidgetCursor &)widgetCursor, widget->context, oldContext);
    }

    bool refresh =
        !widgetCursor.previousState ||
        widgetCursor.previousState->flags.active != widgetCursor.currentState->flags.active ||
        widgetCursor.previousState->data != widgetCursor.currentState->data ||
        ((LayoutViewWidgetState *)widgetCursor.previousState)->context != ((LayoutViewWidgetState *)widgetCursor.currentState)->context;

    if (refresh) {
        const Style* style = getStyle(widget->style);
        drawRectangle(widgetCursor.x, widgetCursor.y, (int)widget->w, (int)widget->h, style, widgetCursor.currentState->flags.active, false, true);
    }

	((LayoutViewWidgetState *)widgetCursor.currentState)->context.freeRef();
    widgetCursor.currentState->data.freeRef();
};

OnTouchFunctionType LAYOUT_VIEW_onTouch = nullptr;

OnKeyboardFunctionType LAYOUT_VIEW_onKeyboard = nullptr;

} // namespace gui
} // namespace eez

#endif
