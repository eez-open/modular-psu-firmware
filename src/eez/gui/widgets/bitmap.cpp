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

#include <eez/core/os.h>
#include <eez/core/util.h>

#include <eez/gui/gui.h>
#include <eez/gui/widgets/bitmap.h>

namespace eez {
namespace gui {

bool BitmapWidgetState::updateState() {
    const WidgetCursor &widgetCursor = g_widgetCursor;

    bool hasPreviousState = widgetCursor.hasPreviousState;
    auto widget = (const BitmapWidget *)widgetCursor.widget;
    
    WIDGET_STATE(flags.active, g_isActiveWidget);
    WIDGET_STATE(data, widget->data ? getBitmapImage(widgetCursor, widget->data) : get(widgetCursor, widget->data));

    return !hasPreviousState;
}

void BitmapWidgetState::render() {
    const WidgetCursor &widgetCursor = g_widgetCursor;
    
    auto widget = (const BitmapWidget *)widgetCursor.widget;
    const Style* style = getStyle(widget->style);

    const Bitmap *bitmap = nullptr;

    if (widget->data) {
        if (data.getType() == VALUE_TYPE_POINTER) {
            auto image = (Image *)data.getVoidPointer();
            if (image) {
                drawBitmap(image, widgetCursor.x, widgetCursor.y, widgetCursor.w, widgetCursor.h, style, flags.active);
            }
            return;
        } else {
            bitmap = getBitmap(data.getInt());
        }
    } else if (widget->bitmap != 0) {
        bitmap = getBitmap(widget->bitmap);
    }

    if (bitmap) {
        Image image;

        image.width = bitmap->w;
        image.height = bitmap->h;
        image.bpp = bitmap->bpp;
        image.lineOffset = 0;
        image.pixels = (uint8_t *)bitmap->pixels;

        drawBitmap(&image, widgetCursor.x, widgetCursor.y, widgetCursor.w, widgetCursor.h, style, flags.active);
    }
}

} // namespace gui
} // namespace eez
