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

#include <eez/core/debug.h>
#include <eez/core/os.h>

#include <eez/gui/gui.h>
#include <eez/gui/widgets/containers/container.h>

namespace eez {
namespace gui {

bool ContainerWidgetState::updateState() {
    const WidgetCursor &widgetCursor = g_widgetCursor;
	auto widget = (const ContainerWidget *)widgetCursor.widget;

	bool hasPreviousState = widgetCursor.hasPreviousState;

	WIDGET_STATE(styleId, g_hooks.overrideStyle(widgetCursor, widget->style));
	WIDGET_STATE(flags.active, g_isActiveWidget);

	overlay = getOverlay(widgetCursor);
	if (overlay) {
		// update overlay data
		auto containerWidget = (const ContainerWidget *)widget;
		Value widgetCursorValue((void *)&widgetCursor, VALUE_TYPE_POINTER);
		DATA_OPERATION_FUNCTION(containerWidget->overlay, DATA_OPERATION_UPDATE_OVERLAY_DATA, widgetCursor, widgetCursorValue);

		WIDGET_STATE(overlayState, overlay->state);
	}

	return !hasPreviousState;
}

void ContainerWidgetState::render() {
	if (overlay && overlayState == 0) {
		return;
	}

    const WidgetCursor &widgetCursor = g_widgetCursor;

	auto widget = (const ContainerWidget *)widgetCursor.widget;

	displayBufferIndex = -1;
	if (overlay) {
		displayBufferIndex = display::beginBufferRendering();
	}

	drawRectangle(
		widgetCursor.x, widgetCursor.y, (int)widget->w, (int)widget->h,
		getStyle(styleId), flags.active
	);
	
	repainted = true;
}

void ContainerWidgetState::enumChildren() {
    if (overlay && overlayState == 0) {
        return;
    }

    WidgetCursor &widgetCursor = g_widgetCursor;
	auto widget = (const ContainerWidget *)widgetCursor.widget;

	bool savedRefreshed = false;

	if (g_findCallback == nullptr) {
		if (overlay) {
			if (displayBufferIndex == -1) {
				displayBufferIndex = display::beginBufferRendering();
			}
		}

		savedRefreshed = widgetCursor.refreshed;
		if (repainted) {
			repainted = false;
			widgetCursor.refreshed = true;
		} else if (!widgetCursor.refreshed) {
			const Style* style = getStyle(styleId);
			widgetCursor.pushBackground(widgetCursor.x, widgetCursor.y, style, flags.active);
		}
	}

	auto savedWidget = widgetCursor.widget;

	int xOffset = 0;
	int yOffset = 0;

	if (overlay) {
		WidgetCursor &widgetCursor = g_widgetCursor;

		getOverlayOffset(widgetCursor, xOffset, yOffset);

		g_xOverlayOffset = xOffset;
		g_yOverlayOffset = yOffset;

		auto widget = (const ContainerWidget *)widgetCursor.widget;
		auto &widgets = widget->widgets;

		auto widgetOverrides = overlay->widgetOverrides;
		for (uint32_t index = 0; index < widgets.count; ++index) {
			if (widgetOverrides) {
				if (!widgetOverrides->isVisible) {
					widgetOverrides++;
					continue;
				}
			}

			widgetCursor.widget = widgets[index];

			int16_t xSaved = 0;
			int16_t ySaved = 0;
			int16_t wSaved = 0;
			int16_t hSaved = 0;

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

			enumWidget();

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
    } else {
		auto &widgets = widget->widgets;

		for (uint32_t index = 0; index < widgets.count; ++index) {
			widgetCursor.widget = widgets[index];
			enumWidget();
		}
	}

	widgetCursor.widget = savedWidget;

	if (g_findCallback == nullptr) {
		if (overlay) {
			const Style *style = getStyle(widgetCursor.widget->style);

			display::endBufferRendering(
				displayBufferIndex,
				widgetCursor.x,
				widgetCursor.y,
				overlay ? overlay->width : widget->w,
				overlay ? overlay->height : widget->h,
				(widget->flags & SHADOW_FLAG) != 0,
				style->opacity,
				xOffset,
				yOffset,
				nullptr
			);

			displayBufferIndex = -1;
		}
		
		if (!widgetCursor.refreshed) {
			widgetCursor.popBackground();
		}

		widgetCursor.refreshed = savedRefreshed;
	}
}

} // namespace gui
} // namespace eez
