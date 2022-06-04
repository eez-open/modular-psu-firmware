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

#include <eez/flow/private.h>

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
    WIDGET_STATE_START(LayoutViewWidget);

    auto savedCursor = widgetCursor.cursor;

    Value oldContext;
    Value newContext;

    if (widget->context) {
        setContext((WidgetCursor &)widgetCursor, widget->context, oldContext, newContext);
        WIDGET_STATE(context, newContext);
    } else {
        WIDGET_STATE(context, Value());
    }

    WIDGET_STATE(flags.active, g_isActiveWidget);

    auto savedFlowState = widgetCursor.flowState;

    WIDGET_STATE(layout, getPageAsset(getLayoutId(widgetCursor), (WidgetCursor &)widgetCursor));

    flowState = widgetCursor.flowState;

    if (flowState && flowState->timelinePosition != timelinePosition) {
        hasPreviousState = false;
        timelinePosition = flowState->timelinePosition;
    }

    ((WidgetCursor &)widgetCursor).flowState = savedFlowState;

    if (widget->context) {
        restoreContext((WidgetCursor &)widgetCursor, widget->context, oldContext);
    }

    ((WidgetCursor &)widgetCursor).cursor = savedCursor;

    WIDGET_STATE_END()
}

void LayoutViewWidgetState::render() {
	const WidgetCursor& widgetCursor = g_widgetCursor;
    auto widget = (const LayoutViewWidget *)widgetCursor.widget;
    const Style* style = getStyle(widget->style);
    drawRectangle(widgetCursor.x, widgetCursor.y, widgetCursor.w, widgetCursor.h, style, flags.active);
    // if (layout) {
    //     const Style* styleLayout = getStyle(layout->style);
    //     drawRectangle(widgetCursor.x, widgetCursor.y, (int)layout->w, (int)layout->h, styleLayout, flags.active);
    // }
	repainted = true;
}

void LayoutViewWidgetState::enumChildren() {
	WidgetCursor& widgetCursor = g_widgetCursor;

    auto savedFlowState = widgetCursor.flowState;
    widgetCursor.flowState = flowState;

	auto widget = (const LayoutViewWidget *)widgetCursor.widget;

	bool savedRefreshed = false;

    auto savedCursor = widgetCursor.cursor;

    Value oldContext;
    Value newContext;

    if (widget->context) {
        setContext((WidgetCursor &)widgetCursor, widget->context, oldContext, newContext);
    }

	if (g_findCallback == nullptr) {
        savedRefreshed = widgetCursor.refreshed;
        if (repainted) {
            repainted = false;
            widgetCursor.refreshed = true;
        } else if (!widgetCursor.refreshed) {
		    const Style* style = getStyle(widget->style);
		    widgetCursor.pushBackground(widgetCursor.x, widgetCursor.y, style, flags.active);
        }
	}

    if (layout) {
		auto savedWidget = widgetCursor.widget;

        auto &widgets = layout->widgets;

        int containerOriginalWidth = layout->width;
        int containerOriginalHeight = layout->height;
        int containerWidth = widgetCursor.w;
        int containerHeight = widgetCursor.h;

        doStaticLayout(widgetCursor, widgets, containerOriginalWidth, containerOriginalHeight, containerWidth, containerHeight);

		widgetCursor.widget = savedWidget;
	}

    if (widget->context) {
        restoreContext(widgetCursor, widget->context, oldContext);
    }

    widgetCursor.cursor = savedCursor;

    widgetCursor.flowState = savedFlowState;

    if (g_findCallback == nullptr) {
        widgetCursor.refreshed = savedRefreshed;

        if (!widgetCursor.refreshed) {
            widgetCursor.popBackground();
        }
    }
}

} // namespace gui
} // namespace eez
