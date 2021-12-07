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

#include <eez/debug.h>
#include <eez/os.h>

#include <eez/gui/gui.h>
#include <eez/gui/widgets/containers/container.h>

namespace eez {
namespace gui {

bool ContainerWidgetState::updateState(const WidgetCursor &widgetCursor) {
	bool hasPreviousState = widgetCursor.hasPreviousState;
	WIDGET_STATE(flags.active, g_isActiveWidget);

	overlay = getOverlay(widgetCursor);
	if (overlay) {
		auto widget = (const ContainerWidget *)widgetCursor.widget;

		// update overlay data
		auto containerWidget = (const ContainerWidget *)widget;
		Value widgetCursorValue((void *)&widgetCursor, VALUE_TYPE_POINTER);
		DATA_OPERATION_FUNCTION(containerWidget->overlay, DATA_OPERATION_UPDATE_OVERLAY_DATA, widgetCursor, widgetCursorValue);

		WIDGET_STATE(overlayState, overlay->state);
	}

	return !hasPreviousState;
}

void ContainerWidgetState::render(WidgetCursor &widgetCursor) {
	auto widget = (const ContainerWidget *)widgetCursor.widget;

	displayBufferIndex = -1;
	if (overlay) {
		if (overlayState == 0) {
			return;
		}
		displayBufferIndex = display::beginBufferRendering();
	}

	drawRectangle(
		widgetCursor.x, widgetCursor.y, (int)widget->w, (int)widget->h,
		getStyle(widget->style), flags.active, false, true
	);
	
	repainted = true;
}

void ContainerWidgetState::enumChildren(WidgetCursor &widgetCursor) {
	auto savedForceRefresh = widgetCursor.forceRefresh;
	if (repainted) {
		repainted = false;
		widgetCursor.forceRefresh = true;
	}
	
	if (overlay) {
        renderOverlayChildren(widgetCursor);

		widgetCursor.forceRefresh = savedForceRefresh;
        return;
    }

	auto savedWidget = widgetCursor.widget;
	
	auto widget = (const ContainerWidget *)widgetCursor.widget;
	auto &widgets = widget->widgets;

	auto widgetPtr = widgets.itemsPtr(widgetCursor.assets);
	for (uint32_t index = 0; index < widgets.count; ++index, ++widgetPtr) {
		widgetCursor.widget = (const Widget *)widgetPtr->ptr(widgetCursor.assets);

        enumWidget(widgetCursor);
    }

	widgetCursor.widget = savedWidget;

	widgetCursor.forceRefresh = savedForceRefresh;
}

void ContainerWidgetState::renderOverlayChildren(WidgetCursor &widgetCursor) {
    if (overlayState == 0) {
        return;
    }

	if (g_findCallback == nullptr) {
		if (displayBufferIndex == -1) {
			displayBufferIndex = display::beginBufferRendering();
		}
	}

	auto savedWidget = widgetCursor.widget;

	int xOffset = 0;
	int yOffset = 0;
	getOverlayOffset(widgetCursor, xOffset, yOffset);

	g_xOverlayOffset = xOffset;
	g_yOverlayOffset = yOffset;

	auto widget = (const ContainerWidget *)widgetCursor.widget;
	auto &widgets = widget->widgets;

    auto widgetOverrides = overlay->widgetOverrides;
    auto widgetPtr = widgets.itemsPtr(widgetCursor.assets);
    for (uint32_t index = 0; index < widgets.count; ++index, ++widgetPtr) {
        if (widgetOverrides) {
			if (!widgetOverrides->isVisible) {
				widgetOverrides++;
				continue;
			}
        }

		widgetCursor.widget = (const Widget *)widgetPtr->ptr(widgetCursor.assets);

        int xSaved = 0;
        int ySaved = 0;
        int wSaved = 0;
        int hSaved = 0;

		if (widgetOverrides) {
			xSaved = widgetCursor.widget->x;
			ySaved = widgetCursor.widget->y;
			wSaved = widgetCursor.widget->w;
			hSaved = widgetCursor.widget->h;

			((Widget*)widgetCursor.widget)->x = widgetOverrides->x;
			((Widget*)widgetCursor.widget)->y = widgetOverrides->y;
			((Widget*)widgetCursor.widget)->w = widgetOverrides->w;
			((Widget*)widgetCursor.widget)->h = widgetOverrides->h;
		}

        enumWidget(widgetCursor);

		if (widgetOverrides) {
			((Widget*)widgetCursor.widget)->x = xSaved;
			((Widget*)widgetCursor.widget)->y = ySaved;
			((Widget*)widgetCursor.widget)->w = wSaved;
			((Widget*)widgetCursor.widget)->h = hSaved;
			
			widgetOverrides++;
		}
    }

	g_xOverlayOffset = 0;
	g_yOverlayOffset = 0;

	if (g_findCallback == nullptr) {
		const Style *style = getStyle(widgetCursor.widget->style);

		display::endBufferRendering(
			displayBufferIndex,
			widgetCursor.x,
			widgetCursor.y,
			overlay->width,
			overlay->height,
			(widget->flags & SHADOW_FLAG) != 0,
			style->opacity,
			xOffset,
			yOffset,
			nullptr
		);

		displayBufferIndex = -1;
	}

	widgetCursor.widget = savedWidget;
}

} // namespace gui
} // namespace eez
