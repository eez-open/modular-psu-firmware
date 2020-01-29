/*
* EEZ Generic Firmware
* Copyright (C) 2018-present, Envox d.o.o.
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

namespace eez {
namespace gui {

bool isOverlay(const  WidgetCursor &widgetCursor) {
    if (widgetCursor.widget->type != WIDGET_TYPE_CONTAINER) {
        return false;
    }
    const ContainerWidget *containerWidget = GET_WIDGET_PROPERTY(widgetCursor.widget, specific, const ContainerWidget *);
    return containerWidget->overlay != DATA_ID_NONE;
}

Overlay *getOverlay(const WidgetCursor &widgetCursor) {
    if (widgetCursor.widget->type != WIDGET_TYPE_CONTAINER) {
        return nullptr;
    }
    const ContainerWidget *containerWidget = GET_WIDGET_PROPERTY(widgetCursor.widget, specific, const ContainerWidget *);
    if (containerWidget->overlay == DATA_ID_NONE) {
        return nullptr;
    }
    data::Value overlayValue;
    g_dataOperationsFunctions[containerWidget->overlay](data::DATA_OPERATION_GET_OVERLAY_DATA, (data::Cursor &)widgetCursor.cursor, overlayValue);
    return (Overlay *)overlayValue.getVoidPointer();
}

void getOverlayOffset(WidgetCursor &widgetCursor, int &xOffset, int &yOffset) {
    Overlay *overlay = getOverlay(widgetCursor);
    if (overlay) {
        int x = widgetCursor.x + overlay->xOffset;
        if (x < g_appContext->x) {
            x = g_appContext->x;
        }
        if (x + overlay->width > g_appContext->x + g_appContext->width) {
            x = g_appContext->x + g_appContext->width - overlay->width;
        }

        int y = widgetCursor.y + overlay->yOffset;
        if (y < g_appContext->y) {
            y = g_appContext->y;
        }
        if (y + overlay->height > g_appContext->y + g_appContext->height) {
            y = g_appContext->y + g_appContext->height - overlay->height;
        }

        xOffset = overlay->xOffset = x - widgetCursor.x;
        yOffset = overlay->yOffset = y - widgetCursor.y;
    } else {
        xOffset = 0;
        yOffset = 0;
    }
}

void dragOverlay(Event &touchEvent) {
    Overlay *overlay = getOverlay(getFoundWidgetAtDown());
    if (overlay) {
        if (touchEvent.type == EVENT_TYPE_TOUCH_DOWN) {
            overlay->xOnTouchDown = touchEvent.x;
            overlay->yOnTouchDown = touchEvent.y;
            overlay->xOffsetOnTouchDown = overlay->xOffset;
            overlay->yOffsetOnTouchDown = overlay->yOffset;
        } else if (touchEvent.type == EVENT_TYPE_TOUCH_MOVE) {
            overlay->xOffset = overlay->xOffsetOnTouchDown + touchEvent.x - overlay->xOnTouchDown;
            overlay->yOffset = overlay->yOffsetOnTouchDown + touchEvent.y - overlay->yOnTouchDown;
        }
    }
}

void overlayEnumWidgetHook(WidgetCursor &widgetCursor, EnumWidgetsCallback callback) {
    if (!isOverlay(widgetCursor)) {
        return;
    }

    // update overlay data
    const ContainerWidget *containerWidget = GET_WIDGET_PROPERTY(widgetCursor.widget, specific, const ContainerWidget *);
    data::Value widgetCursorValue((void *)&widgetCursor, VALUE_TYPE_POINTER);
    g_dataOperationsFunctions[containerWidget->overlay](data::DATA_OPERATION_UPDATE_OVERLAY_DATA, widgetCursor.cursor, widgetCursorValue);

    if (callback == findWidgetStep) {
        int xOverlayOffset = 0;
        int yOverlayOffset = 0;
        getOverlayOffset(widgetCursor, xOverlayOffset, yOverlayOffset);
        widgetCursor.x += xOverlayOffset;
        widgetCursor.y += yOverlayOffset;
    }
}

} // namespace gui
} // namespace eez

#endif