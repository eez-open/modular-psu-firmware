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

#pragma once

#include <eez/gui/widget.h>

namespace eez {
namespace gui {

struct WidgetOverride {
    bool isVisible;
    int16_t x;
    int16_t y;
    int16_t w;
    int16_t h;
};

struct Overlay {
    int width;
    int height;

    int state; // if 0 then overlay is not visible
    WidgetOverride *widgetOverrides;

    int xOffset;
    int yOffset;

    int xOffsetOnTouchDown;
    int yOffsetOnTouchDown;

    int xOnTouchDown;
    int yOnTouchDown;
};

bool isOverlay(const WidgetCursor &widgetCursor);
Overlay *getOverlay(const WidgetCursor &widgetCursor);
void getOverlayOffset(WidgetCursor &widgetCursor, int &xOffset, int &yOffset);
void dragOverlay(Event &touchEvent);
void overlayEnumWidgetHook(WidgetCursor &widgetCursor, EnumWidgetsCallback callback);

} // namespace gui
} // namespace eez