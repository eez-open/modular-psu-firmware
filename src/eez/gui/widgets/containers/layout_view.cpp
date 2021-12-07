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

#include <eez/gui/gui.h>
#include <eez/gui/widgets/containers/container.h>
#include <eez/gui/widgets/containers/layout_view.h>

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

bool LayoutViewWidgetState::updateState() {
    const WidgetCursor &widgetCursor = g_widgetCursor;

    bool hasPreviousState = widgetCursor.hasPreviousState;
    auto widget = (const LayoutViewWidget *)widgetCursor.widget;

    if (widget->context) {
        Value newContext;
        setContext((WidgetCursor &)widgetCursor, widget->context, oldContext, newContext);
        WIDGET_STATE(context, newContext);
    } else {
        WIDGET_STATE(context, Value());
    }

    WIDGET_STATE(flags.active, g_isActiveWidget);

    int layoutId = getLayoutId(widgetCursor);
	auto layout = getPageAsset(layoutId);
	if (layout) {
		WIDGET_STATE(data, layoutId);
	} else {
		WIDGET_STATE(data, 0);
	}

    return !hasPreviousState;
}

void LayoutViewWidgetState::render() {
	const WidgetCursor& widgetCursor = g_widgetCursor;
    auto widget = (const LayoutViewWidget *)widgetCursor.widget;
    const Style* style = getStyle(widget->style);
    drawRectangle(widgetCursor.x, widgetCursor.y, (int)widget->w, (int)widget->h, style, flags.active, false, true);
	repainted = true;
}

void LayoutViewWidgetState::enumChildren() {
	WidgetCursor& widgetCursor = g_widgetCursor;

	auto widget = (const LayoutViewWidget *)widgetCursor.widget;
	const Style* style = getStyle(widget->style);
	widgetCursor.pushBackground(style, flags.active, repainted);

    auto savedForceRefresh = widgetCursor.forceRefresh;
	if (repainted) {
		repainted = false;
		widgetCursor.forceRefresh = true;
	}

	if (g_findCallback != nullptr) {
		if (widget->context) {
			Value newContext;
			setContext((WidgetCursor &)widgetCursor, widget->context, oldContext, newContext);
		}
	}

	auto layoutId = data.getInt();
    if (layoutId) {
		auto layout = getPageAsset(layoutId, widgetCursor);

		auto savedWidget = widgetCursor.widget;
        
        auto &widgets = layout->widgets;
        auto widgetPtr = widgets.itemsPtr(widgetCursor.assets);
        for (uint32_t index = 0; index < widgets.count; ++index, ++widgetPtr) {
			widgetCursor.widget = (const Widget *)widgetPtr->ptr(widgetCursor.assets);

            enumWidget();
        }

		widgetCursor.widget = savedWidget;
	}

    if (widget->context) {
        restoreContext(widgetCursor, widget->context, oldContext);
    }

    widgetCursor.forceRefresh = savedForceRefresh;

    widgetCursor.popBackground();
}

} // namespace gui
} // namespace eez
