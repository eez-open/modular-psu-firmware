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

void LayoutViewWidgetState::draw(WidgetState *previousState) {
	auto widget = (const LayoutViewWidget *)widgetCursor.widget;

    Value oldContext;
    Value newContext;
    if (widget->context) {
        setContext((WidgetCursor &)widgetCursor, widget->context, oldContext, newContext);
        context = newContext;
    } else {
        context = Value();
    }

    data = getLayoutId(widgetCursor);

    bool refresh =
        !previousState ||
        previousState->flags.active != flags.active ||
        previousState->data != data ||
        ((LayoutViewWidgetState *)previousState)->context != context;

    if (refresh) {
        const Style* style = getStyle(widget->style);
        drawRectangle(widgetCursor.x, widgetCursor.y, (int)widget->w, (int)widget->h, style, flags.active, false, true);
    }

    int layoutId = getLayoutId(widgetCursor);
    auto layout = getPageAsset(layoutId, widgetCursor);

    if (layout) {
        WidgetState *childCurrentState = this;
        WidgetState *childPreviousState = previousState;
        WidgetCursor childWidgetCursor = getFirstChildWidgetCursor(widgetCursor, childCurrentState, childPreviousState);
        
        if (
            previousState && 
            (
                previousState->data != data || 
                ((LayoutViewWidgetState *)previousState)->context != context
            )
        ) {
            childPreviousState = 0;
        }

		auto layoutView = (PageAsset *)layout;

        auto &widgets = layoutView->widgets;

        WidgetState *endOfContainerInPreviousState = 0;
        if (previousState) {
            endOfContainerInPreviousState = nextWidgetState(previousState);
        }

        for (uint32_t index = 0; index < widgets.count; ++index) {
            childWidgetCursor.widget = widgets.item(widgetCursor.assets, index);

            enumWidget(childWidgetCursor, childCurrentState, childPreviousState);

            if (childPreviousState) {
                childPreviousState = nextWidgetState(childPreviousState);
                if (childPreviousState > endOfContainerInPreviousState) {
                    childPreviousState = 0;
                }
            }
            childCurrentState = nextWidgetState(childCurrentState);
        }

        widgetStateSize = (uint8_t *)childCurrentState - (uint8_t *)this;
	}
}

} // namespace gui
} // namespace eez
