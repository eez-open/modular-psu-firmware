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

#include <eez/gui/widgets/bitmap.h>

#include <eez/gui/assets.h>
#include <eez/gui/draw.h>
#include <eez/gui/gui.h>
#include <eez/gui/widget.h>
#include <eez/system.h>
#include <eez/util.h>

namespace eez {
namespace gui {

void BitmapWidget_draw(const WidgetCursor &widgetCursor) {
    widgetCursor.currentState->size = sizeof(WidgetState);

    bool refresh = !widgetCursor.previousState || widgetCursor.previousState->flags.active !=
                                                      widgetCursor.currentState->flags.active;

    const Widget *widget = widgetCursor.widget;

    if (refresh) {
        const BitmapWidget *display_bitmap_widget = GET_WIDGET_PROPERTY(widget, specific, const BitmapWidget *);
        const Style* style = getWidgetStyle(widget);

        const Bitmap *bitmap = nullptr;

        if (widget->data) {
            Value valuePixels;
            g_dataOperationsFunctions[widget->data](data::DATA_OPERATION_GET_BITMAP_PIXELS,
                                                    (Cursor &)widgetCursor.cursor, valuePixels);
            if (valuePixels.getType() != VALUE_TYPE_NONE) {
                Value valueWidth;
                g_dataOperationsFunctions[widget->data](data::DATA_OPERATION_GET_BITMAP_WIDTH,
                                                        (Cursor &)widgetCursor.cursor, valueWidth);

                Value valueHeight;
                g_dataOperationsFunctions[widget->data](data::DATA_OPERATION_GET_BITMAP_HEIGHT,
                                                        (Cursor &)widgetCursor.cursor, valueHeight);

                drawBitmap((uint16_t *)valuePixels.getVoidPointer(), 16, valueWidth.getUInt16(),
                           valueHeight.getUInt16(), widgetCursor.x, widgetCursor.y, (int)widget->w,
                           (int)widget->h, style, nullptr, widgetCursor.currentState->flags.active);
                return;
            } else {
                Value valueBitmapId;
                g_dataOperationsFunctions[widget->data](
                    data::DATA_OPERATION_GET, (Cursor &)widgetCursor.cursor, valueBitmapId);

                bitmap = getBitmap(valueBitmapId.getInt());
            }
        } else if (display_bitmap_widget->bitmap != 0) {
            bitmap = getBitmap(display_bitmap_widget->bitmap);
        }

        if (bitmap) {
			const Style *activeStyle = getStyle(widget->activeStyle);

            drawBitmap((uint16_t *)&bitmap->pixels[0], bitmap->bpp, bitmap->w, bitmap->h,
                       widgetCursor.x, widgetCursor.y, (int)widget->w, (int)widget->h, style,
                       activeStyle, widgetCursor.currentState->flags.active);
        }
    }
}

} // namespace gui
} // namespace eez

#endif