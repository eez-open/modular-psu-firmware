/*
* EEZ Generic Firmware
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

#include <eez/gui/draw.h>

#include <eez/gui/document.h>
#include <eez/gui/font.h>
#include <eez/gui/gui.h>
#include <eez/gui/widget.h>
#include <eez/modules/mcu/display.h>
#include <eez/util.h>

using namespace eez::mcu;

namespace eez {
namespace gui {

////////////////////////////////////////////////////////////////////////////////

bool styleIsHorzAlignLeft(const Style *style) {
    return (style->flags & STYLE_FLAGS_HORZ_ALIGN) == STYLE_FLAGS_HORZ_ALIGN_LEFT;
}

bool styleIsHorzAlignRight(const Style *style) {
    return (style->flags & STYLE_FLAGS_HORZ_ALIGN) == STYLE_FLAGS_HORZ_ALIGN_RIGHT;
}

bool styleIsVertAlignTop(const Style *style) {
    return (style->flags & STYLE_FLAGS_VERT_ALIGN) == STYLE_FLAGS_VERT_ALIGN_TOP;
}

bool styleIsVertAlignBottom(const Style *style) {
    return (style->flags & STYLE_FLAGS_VERT_ALIGN) == STYLE_FLAGS_VERT_ALIGN_BOTTOM;
}

font::Font styleGetFont(const Style *style) {
    return font::Font(style->font > 0 ? (g_fontsData + ((uint32_t *)g_fontsData)[style->font - 1])
                                      : 0);
}

bool styleIsBlink(const Style *style) {
    return style->flags & STYLE_FLAGS_BLINK ? true : false;
}

////////////////////////////////////////////////////////////////////////////////

void drawText(const char *text, int textLength, int x, int y, int w, int h, const Style *style,
              const Style *activeStyle, bool active, bool blink, bool ignoreLuminocity,
              uint16_t *overrideBackgroundColor) {
    int x1 = x;
    int y1 = y;
    int x2 = x + w - 1;
    int y2 = y + h - 1;

    int borderRadius = style->border_radius;
    if (style->border_size_top > 0 || style->border_size_right > 0 || style->border_size_bottom > 0 || style->border_size_left > 0) {
        display::setColor(style->border_color);
        if ((style->border_size_top == 1 && style->border_size_right == 1 && style->border_size_bottom == 1 && style->border_size_left == 1) && borderRadius == 0) {
            display::drawRect(x1, y1, x2, y2);
        } else {
            display::fillRect(x1, y1, x2, y2, style->border_radius);
			borderRadius = MAX(borderRadius - MAX(style->border_size_top, MAX(style->border_size_right, MAX(style->border_size_bottom, style->border_size_left))), 0);
        }
        x1 += style->border_size_left;
        y1 += style->border_size_top;
        x2 -= style->border_size_right;
        y2 -= style->border_size_bottom;
    }

    font::Font font = styleGetFont(style);

    int width = display::measureStr(text, textLength, font, x2 - x1 + 1);
    int height = font.getHeight();

    int x_offset;
    if (styleIsHorzAlignLeft(style))
        x_offset = x1 + style->padding_left;
    else if (styleIsHorzAlignRight(style))
        x_offset = x2 - style->padding_left - width;
    else
        x_offset = x1 + ((x2 - x1 + 1) - width) / 2;
    if (x_offset < 0)
        x_offset = x1;

    int y_offset;
    if (styleIsVertAlignTop(style))
        y_offset = y1 + style->padding_top;
    else if (styleIsVertAlignBottom(style))
        y_offset = y2 - style->padding_top - height;
    else
        y_offset = y1 + ((y2 - y1 + 1) - height) / 2;
    if (y_offset < 0)
        y_offset = y1;

    uint16_t backgroundColor;
    if (overrideBackgroundColor) {
        backgroundColor = *overrideBackgroundColor;
    } else {
        backgroundColor = style->background_color;
    }

    // fill background
    if (active || blink) {
        if (activeStyle) {
            display::setColor(activeStyle->background_color, ignoreLuminocity);
        } else {
            display::setColor(style->color, ignoreLuminocity);
        }
    } else {
        display::setColor(backgroundColor, ignoreLuminocity);
    }
    display::fillRect(x1, y1, x2, y2, borderRadius);

    // draw text
    if (active || blink) {
        if (activeStyle) {
            display::setColor(activeStyle->color, ignoreLuminocity);
        } else {
            display::setColor(backgroundColor, ignoreLuminocity);
        }
    } else {
        display::setColor(style->color, ignoreLuminocity);
    }
    display::drawStr(text, textLength, x_offset, y_offset, x1, y1, x2, y2, font);
}

void drawMultilineText(const char *text, int x, int y, int w, int h, const Style *style,
                       const Style *activeStyle, bool active) {
    int x1 = x;
    int y1 = y;
    int x2 = x + w - 1;
    int y2 = y + h - 1;

    int borderRadius = style->border_radius;
	if (style->border_size_top > 0 || style->border_size_right > 0 || style->border_size_bottom > 0 || style->border_size_left > 0) {
        display::setColor(style->border_color);
		if ((style->border_size_top == 1 && style->border_size_right == 1 && style->border_size_bottom == 1 && style->border_size_left == 1) && borderRadius == 0) {
            display::drawRect(x1, y1, x2, y2);
        } else {
            display::fillRect(x1, y1, x2, y2, style->border_radius);
			borderRadius = MAX(borderRadius - MAX(style->border_size_top, MAX(style->border_size_right, MAX(style->border_size_bottom, style->border_size_left))), 0);
        }
		x1 += style->border_size_left;
		y1 += style->border_size_top;
		x2 -= style->border_size_right;
		y2 -= style->border_size_bottom;
	}

    font::Font font = styleGetFont(style);
    int height = (int)(0.9 * font.getHeight());

    font::Glyph space_glyph;
    font.getGlyph(' ', space_glyph);
    int space_width = space_glyph.dx;

    // fill background
    uint16_t background_color = active
                                    ? (activeStyle ? activeStyle->background_color : style->color)
                                    : style->background_color;
    display::setColor(background_color);
    display::fillRect(x1, y1, x2, y2, borderRadius);

    // draw text
    if (active) {
        if (activeStyle) {
            display::setColor(activeStyle->color);
        } else {
            display::setColor(style->background_color);
        }
    } else {
        display::setColor(style->color);
    }

    x1 += style->padding_left;
    x2 -= style->padding_right;
    y1 += style->padding_top;
    y2 -= style->padding_bottom;

    x = x1;
    y = y1;

    int i = 0;
    while (true) {
        int j = i;
        while (text[i] != 0 && text[i] != ' ' && text[i] != '\n')
            ++i;

        int width = display::measureStr(text + j, i - j, font);

        while (width > x2 - x + 1) {
            y += height;
            if (y + height > y2) {
                break;
            }

            x = x1;
        }

        if (y + height > y2) {
            break;
        }

        display::drawStr(text + j, i - j, x, y, x1, y1, x2, y2, font);

        x += width;

        while (text[i] == ' ') {
            x += space_width;
            ++i;
        }

        if (text[i] == 0 || text[i] == '\n') {
            y += height;

            if (text[i] == 0) {
                break;
            }

            ++i;

            int extraHeightBetweenParagraphs = (int)(0.2 * height);
            y += extraHeightBetweenParagraphs;

            if (y + height > y2) {
                break;
            }
            x = x1;
        }
    }
}

void drawBitmap(void *bitmapPixels, int bpp, int bitmapWidth, int bitmapHeight, int x, int y, int w,
                int h, const Style *style, const Style *activeStyle, bool active) {
    int x1 = x;
    int y1 = y;
    int x2 = x + w - 1;
    int y2 = y + h - 1;

	if (style->border_size_top > 0 || style->border_size_right > 0 || style->border_size_bottom > 0 || style->border_size_left > 0) {
		display::setColor(style->border_color);
		if (style->border_size_top == 1 && style->border_size_right == 1 && style->border_size_bottom == 1 && style->border_size_left == 1) {
			display::drawRect(x1, y1, x2, y2);
		}
		else {
			display::fillRect(x1, y1, x2, y2, 0);
		}
		x1 += style->border_size_left;
		y1 += style->border_size_top;
		x2 -= style->border_size_right;
		y2 -= style->border_size_bottom;
	}

    int width = bitmapWidth;
    int height = bitmapHeight;

    int x_offset;
    if (styleIsHorzAlignLeft(style))
        x_offset = x1 + style->padding_left;
    else if (styleIsHorzAlignRight(style))
        x_offset = x2 - style->padding_right - width;
    else
        x_offset = x1 + ((x2 - x1) - width) / 2;
    if (x_offset < 0)
        x_offset = x1;

    int y_offset;
    if (styleIsVertAlignTop(style))
        y_offset = y1 + style->padding_top;
    else if (styleIsVertAlignBottom(style))
        y_offset = y2 - style->padding_bottom - height;
    else
        y_offset = y1 + ((y2 - y1) - height) / 2;
    if (y_offset < 0)
        y_offset = y1;

    uint16_t background_color = active
                                    ? (activeStyle ? activeStyle->background_color : style->color)
                                    : style->background_color;
    display::setColor(background_color);

    // fill background
    if (x1 <= x_offset - 1 && y1 <= y2)
        display::fillRect(x1, y1, x_offset - 1, y2);
    if (x_offset + width <= x2 && y1 <= y2)
        display::fillRect(x_offset + width, y1, x2, y2);

    int right = MIN(x_offset + width - 1, x2);

    if (x_offset <= right && y1 <= y_offset - 1)
        display::fillRect(x_offset, y1, right, y_offset - 1);
    if (x_offset <= right && y_offset + height <= y2)
        display::fillRect(x_offset, y_offset + height, right, y2);

    // draw bitmap
    uint8_t savedOpacity = display::getOpacity();

    if (active) {
        if (activeStyle) {
            display::setBackColor(activeStyle->background_color);
            display::setColor(activeStyle->color);
            display::setOpacity(activeStyle->opacity);
        } else {
            display::setBackColor(style->color);
            display::setColor(style->background_color);
            display::setOpacity(style->opacity);
        }
    } else {
        display::setBackColor(style->background_color);
        display::setColor(style->color);
        display::setOpacity(style->opacity);
    }

    display::drawBitmap(bitmapPixels, bpp, width, x_offset, y_offset, width, height);

    display::setOpacity(savedOpacity);
}

void drawRectangle(int x, int y, int w, int h, const Style *style, const Style *activeStyle,
                   bool active, bool ignoreLuminocity) {
    if (w > 0 && h > 0) {
        int x1 = x;
        int y1 = y;
        int x2 = x + w - 1;
        int y2 = y + h - 1;

        int borderRadius = style->border_radius;
		if (style->border_size_top > 0 || style->border_size_right > 0 || style->border_size_bottom > 0 || style->border_size_left > 0) {
            display::setColor(style->border_color);
			if ((style->border_size_top == 1 && style->border_size_right == 1 && style->border_size_bottom == 1 && style->border_size_left == 1) && borderRadius == 0) {
                display::drawRect(x1, y1, x2, y2);
            } else {
                display::fillRect(x1, y1, x2, y2, style->border_radius);
				borderRadius = MAX(borderRadius - MAX(style->border_size_top, MAX(style->border_size_right, MAX(style->border_size_bottom, style->border_size_left))), 0);
            }
			x1 += style->border_size_left;
			y1 += style->border_size_top;
			x2 -= style->border_size_right;
			y2 -= style->border_size_bottom;
		}

        uint16_t color =
            active ? (activeStyle ? activeStyle->color : style->background_color) : style->color;
        display::setColor(color, ignoreLuminocity);
        display::fillRect(x1, y1, x2, y2, borderRadius);
    }
}

} // namespace gui
} // namespace eez
