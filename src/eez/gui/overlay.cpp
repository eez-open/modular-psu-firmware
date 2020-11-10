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
    Value overlayValue;
    DATA_OPERATION_FUNCTION(containerWidget->overlay, DATA_OPERATION_GET_OVERLAY_DATA, (Cursor )widgetCursor.cursor, overlayValue);
    if (overlayValue.getType() != VALUE_TYPE_POINTER) {
        return nullptr;
    }
    return (Overlay *)overlayValue.getVoidPointer();
}

void getOverlayOffset(WidgetCursor &widgetCursor, int &xOffset, int &yOffset) {
    Overlay *overlay = getOverlay(widgetCursor);
    if (overlay) {
        if (!overlay->moved && overlay->state) {
            overlay->xOffset = overlay->x - widgetCursor.widget->x;
            overlay->yOffset = overlay->y - widgetCursor.widget->y;
        }

        int x = widgetCursor.x + overlay->xOffset;
        if (x < widgetCursor.appContext->rect.x) {
            x = widgetCursor.appContext->rect.x;
        }
        if (x + overlay->width > widgetCursor.appContext->rect.x + widgetCursor.appContext->rect.w) {
            x = widgetCursor.appContext->rect.x + widgetCursor.appContext->rect.w - overlay->width;
        }

        int y = widgetCursor.y + overlay->yOffset;
        if (y < widgetCursor.appContext->rect.y) {
            y = widgetCursor.appContext->rect.y;
        }
        if (y + overlay->height > widgetCursor.appContext->rect.y + widgetCursor.appContext->rect.h) {
            y = widgetCursor.appContext->rect.y + widgetCursor.appContext->rect.h - overlay->height;
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
            overlay->moved = true;
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
    Value widgetCursorValue((void *)&widgetCursor, VALUE_TYPE_POINTER);
    DATA_OPERATION_FUNCTION(containerWidget->overlay, DATA_OPERATION_UPDATE_OVERLAY_DATA, widgetCursor.cursor, widgetCursorValue);

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
