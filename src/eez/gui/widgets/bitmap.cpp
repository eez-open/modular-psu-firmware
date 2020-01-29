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
#include <eez/util.h>

#include <eez/gui/gui.h>
#include <eez/gui/widgets/bitmap.h>

namespace eez {
namespace gui {

void BitmapWidget_draw(const WidgetCursor &widgetCursor) {
	const Widget *widget = widgetCursor.widget;

	widgetCursor.currentState->size = sizeof(WidgetState);

    widgetCursor.currentState->data = widget->data ? data::getBitmapPixels(widgetCursor.cursor, widget->data) : 0;

    bool refresh = !widgetCursor.previousState ||
    		widgetCursor.previousState->flags.active != widgetCursor.currentState->flags.active ||
			widgetCursor.currentState->data != widgetCursor.previousState->data;

    if (refresh) {
        const BitmapWidget *display_bitmap_widget = GET_WIDGET_PROPERTY(widget, specific, const BitmapWidget *);
        const Style* style = getStyle(widget->style);

        const Bitmap *bitmap = nullptr;

        if (widget->data) {
            if (widgetCursor.currentState->data.getType() != VALUE_TYPE_NONE) {
                auto pixels = (uint8_t *)widgetCursor.currentState->data.getVoidPointer();
                if (pixels) {
                    drawBitmap(pixels, 24,
                    	getBitmapWidth(widgetCursor.cursor, widget->data),
                        getBitmapHeight(widgetCursor.cursor, widget->data),
						widgetCursor.x, widgetCursor.y, (int)widget->w, (int)widget->h,
						style, widgetCursor.currentState->flags.active
					);
                } else {
                    drawRectangle(widgetCursor.x, widgetCursor.y, (int)widget->w, (int)widget->h, style, widgetCursor.currentState->flags.active, true, true);
                }
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
            drawBitmap((uint16_t *)&bitmap->pixels[0], bitmap->bpp, bitmap->w, bitmap->h,
                       widgetCursor.x, widgetCursor.y, (int)widget->w, (int)widget->h, style,
                       widgetCursor.currentState->flags.active);
        }
    }
}

} // namespace gui
} // namespace eez

#endif
