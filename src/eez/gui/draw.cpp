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

#if OPTION_DISPLAY

#include <string.h>

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
    return font::Font(getFontData(style->font));
}

bool styleIsBlink(const Style *style) {
    return style->flags & STYLE_FLAGS_BLINK ? true : false;
}

////////////////////////////////////////////////////////////////////////////////

void drawText(const char *text, int textLength, int x, int y, int w, int h, const Style *style,
              bool active, bool blink, bool ignoreLuminocity,
              uint16_t *overrideColor, uint16_t *overrideBackgroundColor,
              uint16_t *overrideActiveColor, uint16_t *overrideActiveBackgroundColor) {
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
    if (active) {
        if (overrideActiveBackgroundColor) {
            display::setColor(*overrideActiveBackgroundColor, ignoreLuminocity);
        } else {
            display::setColor(style->active_background_color, ignoreLuminocity);
        }
    } else if (blink) {
        display::setColor(style->color, ignoreLuminocity);
    }else {
        display::setColor(backgroundColor, ignoreLuminocity);
    }
    display::fillRect(x1, y1, x2, y2, borderRadius);

    // draw text
    if (active) {
        if (overrideActiveColor) {
            display::setColor(*overrideActiveColor, ignoreLuminocity);
        } else {
            display::setColor(style->active_color, ignoreLuminocity);
        }
    } else if (blink) {
        display::setColor(backgroundColor, ignoreLuminocity);
    }else {
        if (overrideColor) {
            display::setColor(*overrideColor, ignoreLuminocity);
        } else {
            display::setColor(style->color, ignoreLuminocity);
        }
    }
    display::drawStr(text, textLength, x_offset, y_offset, x1, y1, x2, y2, font);
}

////////////////////////////////////////////////////////////////////////////////

static const unsigned int CONF_MULTILINE_TEXT_MAX_LINE_LENGTH = 100;

enum MultilineTextRenderStep {
    MEASURE,
    RENDER
};

struct MultilineTextRender {
    const char *text;
    int x1;
    int y1;
    int x2;
    int y2;
    const Style *style;
    bool active;
    int firstLineIndent;
    int hangingIndent;

    font::Font font;
    int spaceWidth;

    int lineHeight;
    int textHeight;

    char line[CONF_MULTILINE_TEXT_MAX_LINE_LENGTH + 1];
    int lineIndent;
    int lineWidth;

    void appendToLine(const char *str, size_t n) {
        size_t j = strlen(line);
        for (size_t i = 0; i < n && j < CONF_MULTILINE_TEXT_MAX_LINE_LENGTH; i++, j++) {
            line[j] = str[i];
        }
        line[j] = 0;
    }

    void flushLine(int y, MultilineTextRenderStep step) {
        if (line[0] && lineWidth) {
            if (step == RENDER) {
                int x;

                if (styleIsHorzAlignLeft(style)) {
                    x = x1;
                } else if (styleIsHorzAlignRight(style)) {
                    x = x2 + 1 - lineWidth;
                } else {
                    x = x1 + int((x2 - x1 + 1 - lineWidth) / 2);
                }

                display::drawStr(line, -1, x + lineIndent, y, x, y, x + lineWidth - 1, y + font.getHeight() - 1, font);
            } else {
                textHeight = MAX(textHeight, y + lineHeight - y1);
            }

            line[0] = 0;
            lineWidth = lineIndent = hangingIndent;
        }
    }

    int executeStep(MultilineTextRenderStep step) {
        textHeight = 0;
        
        int y = y1;

        line[0] = 0;
        lineWidth = lineIndent = firstLineIndent;

        int i = 0;
        while (true) {
            int j = i;
            while (text[i] != 0 && text[i] != ' ' && text[i] != '\n')
                ++i;

            int width = display::measureStr(text + j, i - j, font);

            while (lineWidth + (line[0] ? spaceWidth : 0) + width > x2 - x1 + 1) {
                flushLine(y, step);
                
                y += lineHeight;
                if (y + lineHeight - 1 > y2) {
                    break;
                }
            }

            if (y + lineHeight - 1 > y2) {
                break;
            }

            if (line[0]) {
                appendToLine(" ", 1);
                lineWidth += spaceWidth;
            }
            appendToLine(text + j, i - j);
            lineWidth += width;

            while (text[i] == ' ') {
                ++i;
            }

            if (text[i] == 0 || text[i] == '\n') {
                flushLine(y, step);
                
                y += lineHeight;

                if (text[i] == 0) {
                    break;
                }

                ++i;

                int extraHeightBetweenParagraphs = (int)(0.2 * lineHeight);
                y += extraHeightBetweenParagraphs;

                if (y + lineHeight - 1 > y2) {
                    break;
                }
            }
        }

        flushLine(y, step);

        return textHeight + font.getHeight() - lineHeight;
    }

    void render() {
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

        // fill background
        uint16_t background_color = active ? style->active_background_color : style->background_color;
        display::setColor(background_color);
        display::fillRect(x1, y1, x2, y2, borderRadius);

        //
        font = styleGetFont(style);
        
        lineHeight = (int)(0.9 * font.getHeight());
        if (lineHeight <= 0) {
            return;
        }

        font::Glyph space_glyph;
        font.getGlyph(' ', space_glyph);
        spaceWidth = space_glyph.dx;

        // draw text
        display::setColor(active ? style->active_color : style->color);

        x1 += style->padding_left;
        x2 -= style->padding_right;
        y1 += style->padding_top;
        y2 -= style->padding_bottom;

        int textHeight = executeStep(MEASURE);

        if (styleIsVertAlignTop(style)) {
        } else if (styleIsVertAlignBottom(style)) {
            y1 = y2 + 1 - textHeight;
        } else {
            y1 += (int)((y2 - y1 + 1 - textHeight) / 2);
        }
        y2 = y1 + textHeight - 1;

        executeStep(RENDER);

    }
};

void drawMultilineText(const char *text, int x, int y, int w, int h, const Style *style, bool active, int firstLineIndent, int hangingIndent) {
    MultilineTextRender multilineTextRender;

    multilineTextRender.text = text;
    multilineTextRender.x1 = x;
    multilineTextRender.y1 = y;
    multilineTextRender.x2 = x + w - 1;
    multilineTextRender.y2 = y + h - 1;
    multilineTextRender.style = style;
    multilineTextRender.active = active;
    multilineTextRender.firstLineIndent = firstLineIndent;
    multilineTextRender.hangingIndent = hangingIndent;

    multilineTextRender.render();
}

////////////////////////////////////////////////////////////////////////////////

void drawBitmap(void *bitmapPixels, int bpp, int bitmapWidth, int bitmapHeight, int x, int y, int w, int h, const Style *style, bool active) {
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


    // draw bitmap
    uint8_t savedOpacity = display::getOpacity();

    if (active) {
        display::setBackColor(style->active_background_color);
        display::setColor(style->active_color);
        display::setOpacity(style->opacity);
    } else {
        display::setBackColor(style->background_color);
        display::setColor(style->color);
        display::setOpacity(style->opacity);
    }

    display::drawBitmap(bitmapPixels, bpp, width, x_offset, y_offset, width, height);

    display::setOpacity(savedOpacity);
}

////////////////////////////////////////////////////////////////////////////////

void drawRectangle(int x, int y, int w, int h, const Style *style, bool active, bool ignoreLuminocity, bool invertColors) {
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

        if (invertColors) {
            display::setColor(active ? style->active_background_color : style->background_color, ignoreLuminocity);
        } else {
            display::setColor(active ? style->active_color : style->color, ignoreLuminocity);
        }
        
        display::fillRect(x1, y1, x2, y2, borderRadius);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static const int T = 4;
static const int R = 7;
static const int B = 10;
static const int L = 7;

static const int W = 20;
static const int H = 20;

void drawShadowGlyph(char glyph, int x, int y, int xClip = -1, int yClip = -1) {
    if (xClip == -1) {
        xClip = x + W - 1;
    }
    if (yClip == -1) {
        yClip = y + H - 1;
    }
    font::Font font(getFontData(FONT_ID_SHADOW));
    eez::mcu::display::drawStr(&glyph, 1, x, y, x, y, xClip, yClip, font);
}

void drawShadow(int x1, int y1, int x2, int y2) {
    mcu::display::setColor(64, 64, 64);

    int left = x1 - L;
    int top = y1 - T;

    int right = x2 + R - (W - 1);
    int bottom = y2 + B - (H - 1);

    drawShadowGlyph(32, left, top);

    for (int x = left + W; x < right; x += W) {
        drawShadowGlyph(33, x, top, right - 1);
    }

    drawShadowGlyph(34, right, top);

    for (int y = top + H; y < bottom; y += H) {
        drawShadowGlyph(35, left, y, -1, bottom - 1);
    }

    for (int y = top + H; y < bottom; y += H) {
        drawShadowGlyph(36, right, y, -1, bottom - 1);
    }

    drawShadowGlyph(37, left, bottom);

    for (int x = left + W; x < right; x += W) {
        drawShadowGlyph(38, x, bottom, right - 1);
    }

    drawShadowGlyph(39, right, bottom);
}

} // namespace gui
} // namespace eez

#endif
