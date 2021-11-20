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

#include <eez/system.h>
#include <eez/debug.h>

#include <eez/gui/gui.h>
#include <eez/gui/widgets/container.h>
#include <eez/gui/widgets/layout_view.h>

namespace eez {
namespace gui {

static int g_bufferIndex;

EnumFunctionType CONTAINER_enum = [](WidgetCursor &widgetCursor, EnumWidgetsCallback callback) {
	bool callEndBufferDrawing = false;
    Overlay *overlay = nullptr;
	WidgetOverride *widgetOverrides = nullptr;
    if (isOverlay(widgetCursor)) {
        overlay = getOverlay(widgetCursor);
		if (!overlay->state) {
			return;
		}
		if (callback == drawWidgetCallback) {
			callEndBufferDrawing = true;
		}
		widgetOverrides = overlay->widgetOverrides;
    }
    
	auto widget = ((ContainerWidget *)widgetCursor.widget);
	
    ListOfAssetsPtr<Widget> &widgets = widget->widgets;

	auto savedWidget = widgetCursor.widget;
	
	for (uint32_t index = 0; index < widgets.count; ++index) {
        widgetCursor.widget = widgets.item(widgetCursor.assets, index);

        int xSaved = 0;
        int ySaved = 0;
        int wSaved = 0;
        int hSaved = 0;

        if (widgetOverrides) {
            if (!overlay->widgetOverrides[index].isVisible) {
                continue;
            }

            xSaved = widgetCursor.widget->x;
            ySaved = widgetCursor.widget->y;
            wSaved = widgetCursor.widget->w;
            hSaved = widgetCursor.widget->h;

            ((Widget*)widgetCursor.widget)->x = overlay->widgetOverrides[index].x;
            ((Widget*)widgetCursor.widget)->y = overlay->widgetOverrides[index].y;
            ((Widget*)widgetCursor.widget)->w = overlay->widgetOverrides[index].w;
            ((Widget*)widgetCursor.widget)->h = overlay->widgetOverrides[index].h;
        }

        enumWidget(widgetCursor, callback);

        if (widgetOverrides) {
            ((Widget*)widgetCursor.widget)->x = xSaved;
            ((Widget*)widgetCursor.widget)->y = ySaved;
            ((Widget*)widgetCursor.widget)->w = wSaved;
            ((Widget*)widgetCursor.widget)->h = hSaved;
        }
    }

    widgetCursor.widget = savedWidget;

    if (callEndBufferDrawing) {
		int xOffset = 0;
		int yOffset = 0;
		getOverlayOffset(widgetCursor, xOffset, yOffset);

		const Style *style = getStyle(widgetCursor.widget->style);

        mcu::display::endAuxBufferDrawing(
            g_bufferIndex,
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
    }
};

DrawFunctionType CONTAINER_draw = [](const WidgetCursor &widgetCursor) {
    auto widget = (const ContainerWidget *)widgetCursor.widget;

    int w;
    int h;

    if (isOverlay(widgetCursor)) {
        Overlay *overlay = getOverlay(widgetCursor);
		if (!overlay->state) {
            return;
        }
		w = overlay->width;
		h = overlay->height;
		g_bufferIndex = mcu::display::beginAuxBufferDrawing();
	} else {
        w = (int)widget->w;
        h = (int)widget->h;
    }

    drawRectangle(
        widgetCursor.x, widgetCursor.y, w, h, 
        getStyle(widget->style), g_isActiveWidget, false, true
    );
};

OnTouchFunctionType CONTAINER_onTouch = nullptr;

OnKeyboardFunctionType CONTAINER_onKeyboard = nullptr;

} // namespace gui
} // namespace eez

#endif
