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

#include <eez/system.h>
#include <eez/util.h>

#include <eez/gui/gui.h>

namespace eez {
namespace gui {

struct BitmapWidget : public Widget {
    int16_t bitmap;
};

EnumFunctionType BITMAP_enum = nullptr;

DrawFunctionType BITMAP_draw = [](const WidgetCursor &widgetCursor) {
	auto widget = (const BitmapWidget *)widgetCursor.widget;

	widgetCursor.currentState->size = sizeof(WidgetState);

    widgetCursor.currentState->data.clear();
    widgetCursor.currentState->data = widget->data ? getBitmapImage(widgetCursor.cursor, widget->data) : 0;

    bool refresh = !widgetCursor.previousState ||
    		widgetCursor.previousState->flags.active != widgetCursor.currentState->flags.active ||
			widgetCursor.currentState->data != widgetCursor.previousState->data;

    if (refresh) {
        const Style* style = getStyle(widget->style);

        const Bitmap *bitmap = nullptr;

        if (widget->data) {
            if (widgetCursor.currentState->data.getType() != VALUE_TYPE_UNDEFINED) {
                drawRectangle(widgetCursor.x, widgetCursor.y, (int)widget->w, (int)widget->h, style, widgetCursor.currentState->flags.active, true, true);
                auto image = (Image *)widgetCursor.currentState->data.getVoidPointer();
                if (image) {
                    drawBitmap(image, widgetCursor.x, widgetCursor.y, (int)widget->w, (int)widget->h, style, widgetCursor.currentState->flags.active);
                }
                return;
            } else {
                Value valueBitmapId;
                DATA_OPERATION_FUNCTION(widget->data,  DATA_OPERATION_GET, widgetCursor, valueBitmapId);
                bitmap = getBitmap(valueBitmapId.getInt());
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

            drawBitmap(&image, widgetCursor.x, widgetCursor.y, (int)widget->w, (int)widget->h, style, widgetCursor.currentState->flags.active);
        }
    }

    widgetCursor.currentState->data.freeRef();
};

OnTouchFunctionType BITMAP_onTouch = nullptr;

OnKeyboardFunctionType BITMAP_onKeyboard = nullptr;

} // namespace gui
} // namespace eez
