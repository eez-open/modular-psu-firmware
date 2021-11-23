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

#include <string.h>
#define _USE_MATH_DEFINES
#include <math.h>

#include <eez/gui_conf.h>
#include <eez/util.h>

#include <eez/gui/gui.h>

namespace eez {
namespace gui {

////////////////////////////////////////////////////////////////////////////////

bool styleIsHorzAlignLeft(const Style *style) {
    return (style->flags & STYLE_FLAGS_HORZ_ALIGN_MASK) == STYLE_FLAGS_HORZ_ALIGN_LEFT;
}

bool styleIsHorzAlignRight(const Style *style) {
    return (style->flags & STYLE_FLAGS_HORZ_ALIGN_MASK) == STYLE_FLAGS_HORZ_ALIGN_RIGHT;
}

bool styleIsVertAlignTop(const Style *style) {
    return (style->flags & STYLE_FLAGS_VERT_ALIGN_MASK) == STYLE_FLAGS_VERT_ALIGN_TOP;
}

bool styleIsVertAlignBottom(const Style *style) {
    return (style->flags & STYLE_FLAGS_VERT_ALIGN_MASK) == STYLE_FLAGS_VERT_ALIGN_BOTTOM;
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
              uint16_t *overrideActiveColor, uint16_t *overrideActiveBackgroundColor,
              bool useSmallerFontIfDoesNotFit, int cursorPosition, int xScroll) {
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

    int width = display::measureStr(text, textLength, font, 0);
    while (useSmallerFontIfDoesNotFit && width > x2 - x1 + 1 && styleGetSmallerFontHook(font)) {
        width = display::measureStr(text, textLength, font, 0);
    }
    int height = font.getHeight();

    int x_offset;
    if (styleIsHorzAlignLeft(style)) {
        x_offset = x1 + style->padding_left;
    } else if (styleIsHorzAlignRight(style)) {
        x_offset = x2 - style->padding_right - width;
    } else {
        x_offset = x1 + ((x2 - x1 + 1) - width) / 2;
        if (x_offset < x1) {
            x_offset = x1;
        }
    }

    int y_offset;
    if (styleIsVertAlignTop(style)) {
        y_offset = y1 + style->padding_top;
    } else if (styleIsVertAlignBottom(style)) {
        y_offset = y2 - style->padding_bottom - height;
    } else {
        y_offset = y1 + ((y2 - y1 + 1) - height) / 2;
    }
    if (y_offset < 0) {
        y_offset = y1;
    }

    // fill background
    if (active || blink) {
        if (overrideActiveBackgroundColor) {
            display::setColor(*overrideActiveBackgroundColor, ignoreLuminocity);
        } else {
            display::setColor(style->active_background_color, ignoreLuminocity);
        }
    } else {
        if (overrideBackgroundColor) {
            display::setColor(*overrideBackgroundColor, ignoreLuminocity);
        } else {
            display::setColor(style->background_color, ignoreLuminocity);
        }
    }
    display::fillRect(x1, y1, x2, y2, borderRadius);

    // draw text
    if (active || blink) {
        if (overrideActiveColor) {
            display::setColor(*overrideActiveColor, ignoreLuminocity);
        } else {
            display::setColor(style->active_color, ignoreLuminocity);
        }
    }  else {
        if (overrideColor) {
            display::setColor(*overrideColor, ignoreLuminocity);
        } else {
            display::setColor(style->color, ignoreLuminocity);
        }
    }
    display::drawStr(text, textLength, x_offset - xScroll, y_offset, x1, y1, x2, y2, font, cursorPosition);
}

////////////////////////////////////////////////////////////////////////////////

int getCharIndexAtPosition(int xPos, const char *text, int textLength, int x, int y, int w, int h, const Style *style) {
    int x1 = x;
    int y1 = y;
    int x2 = x + w - 1;
    int y2 = y + h - 1;

    if (style->border_size_top > 0 || style->border_size_right > 0 || style->border_size_bottom > 0 || style->border_size_left > 0) {
        x1 += style->border_size_left;
        y1 += style->border_size_top;
        x2 -= style->border_size_right;
        y2 -= style->border_size_bottom;
    }

    font::Font font = styleGetFont(style);

    int width = display::measureStr(text, textLength, font, 0);
    int height = font.getHeight();

    int x_offset;
    if (styleIsHorzAlignLeft(style)) {
        x_offset = x1 + style->padding_left;
    } else if (styleIsHorzAlignRight(style)) {
        x_offset = x2 - style->padding_right - width;
    } else {
        x_offset = x1 + ((x2 - x1 + 1) - width) / 2;
        if (x_offset < x1) {
            x_offset = x1;
        }
    }

    int y_offset;
    if (styleIsVertAlignTop(style)) {
        y_offset = y1 + style->padding_top;
    } else if (styleIsVertAlignBottom(style)) {
        y_offset = y2 - style->padding_bottom - height;
    } else {
        y_offset = y1 + ((y2 - y1 + 1) - height) / 2;
    }
    if (y_offset < 0) {
        y_offset = y1;
    }

    return display::getCharIndexAtPosition(xPos, text, textLength, x_offset, y_offset, x1, y1, x2, y2, font);
}

int getCursorXPosition(int cursorPosition, const char *text, int textLength, int x, int y, int w, int h, const Style *style) {
    int x1 = x;
    int y1 = y;
    int x2 = x + w - 1;
    int y2 = y + h - 1;

    if (style->border_size_top > 0 || style->border_size_right > 0 || style->border_size_bottom > 0 || style->border_size_left > 0) {
        x1 += style->border_size_left;
        y1 += style->border_size_top;
        x2 -= style->border_size_right;
        y2 -= style->border_size_bottom;
    }

    font::Font font = styleGetFont(style);

    int width = display::measureStr(text, textLength, font, 0);
    int height = font.getHeight();

    int x_offset;
    if (styleIsHorzAlignLeft(style)) {
        x_offset = x1 + style->padding_left;
    } else if (styleIsHorzAlignRight(style)) {
        x_offset = x2 - style->padding_right - width;
    } else {
        x_offset = x1 + ((x2 - x1 + 1) - width) / 2;
        if (x_offset < x1) {
            x_offset = x1;
        }
    }

    int y_offset;
    if (styleIsVertAlignTop(style)) {
        y_offset = y1 + style->padding_top;
    } else if (styleIsVertAlignBottom(style)) {
        y_offset = y2 - style->padding_bottom - height;
    } else {
        y_offset = y1 + ((y2 - y1 + 1) - height) / 2;
    }
    if (y_offset < 0) {
        y_offset = y1;
    }

    return display::getCursorXPosition(cursorPosition, text, textLength, x_offset, y_offset, x1, y1, x2, y2, font);
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

                display::drawStr(line, -1, x + lineIndent, y, x, y, x + lineWidth - 1, y + font.getHeight() - 1, font, -1);
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
				if (!line[0]) {
					i--;
					width = display::measureStr(text + j, i - j, font);
					continue;
				}

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

    int measure() {
        if (style->border_size_top > 0 || style->border_size_right > 0 || style->border_size_bottom > 0 || style->border_size_left > 0) {
            x1 += style->border_size_left;
            y1 += style->border_size_top;
            x2 -= style->border_size_right;
            y2 -= style->border_size_bottom;
        }

        font = styleGetFont(style);
        
        lineHeight = (int)(0.9 * font.getHeight());
        if (lineHeight <= 0) {
            return 0;
        }

        auto spaceGlyph = font.getGlyph(' ');
        spaceWidth = spaceGlyph->dx;

        x1 += style->padding_left;
        x2 -= style->padding_right;
        y1 += style->padding_top;
        y2 -= style->padding_bottom;

        return executeStep(MEASURE);
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

        auto spaceGlyph = font.getGlyph(' ');
        spaceWidth = spaceGlyph->dx;

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

int measureMultilineText(const char *text, int x, int y, int w, int h, const Style *style, int firstLineIndent, int hangingIndent) {
    MultilineTextRender multilineTextRender;

    multilineTextRender.text = text;
    multilineTextRender.x1 = x;
    multilineTextRender.y1 = y;
    multilineTextRender.x2 = x + w - 1;
    multilineTextRender.y2 = y + h - 1;
    multilineTextRender.style = style;
    multilineTextRender.active = false;
    multilineTextRender.firstLineIndent = firstLineIndent;
    multilineTextRender.hangingIndent = hangingIndent;

    return multilineTextRender.measure();
}

////////////////////////////////////////////////////////////////////////////////

void drawBitmap(Image *image, int x, int y, int w, int h, const Style *style, bool active) {
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

    int width = image->width;
    int height = image->height;

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

    display::drawBitmap(image, x_offset, y_offset);

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

////////////////////////////////////////////////////////////////////////////////

enum ShadowGlpyh {
	SHADOW_GLYPH_TOP_LEFT,
	SHADOW_GLYPH_TOP,
	SHADOW_GLYPH_TOP_RIGHT,
	SHADOW_GLYPH_LEFT,
	SHADOW_GLYPH_RIGHT,
	SHADOW_GLYPH_BOTTOM_LEFT,
	SHADOW_GLYPH_BOTTOM,
	SHADOW_GLYPH_BOTTOM_RIGHT,
};

static const int T = 4;
static const int R = 7;
static const int B = 10;
static const int L = 7;

static const int W = 20;
static const int H = 20;

void drawShadowGlyph(ShadowGlpyh shadowGlyph, int x, int y, int xClip = -1, int yClip = -1) {
	font::Font font(getFontData(EEZ_CONF_FONT_ID_SHADOW));

	if (xClip == -1) {
		xClip = x + W - 1;
	}
	if (yClip == -1) {
		yClip = y + H - 1;
	}
	char glyph = 32 + shadowGlyph;
	display::drawStr(&glyph, 1, x, y, x, y, xClip, yClip, font, -1);
}

void drawShadow(int x1, int y1, int x2, int y2) {
	display::setColor(64, 64, 64);

	int left = x1 - L;
	int top = y1 - T;

	int right = x2 + R - (W - 1);
	int bottom = y2 + B - (H - 1);

	drawShadowGlyph(SHADOW_GLYPH_TOP_LEFT, left, top);
	for (int x = left + W; x < right; x += W) {
		drawShadowGlyph(SHADOW_GLYPH_TOP, x, top, right - 1);
	}
	drawShadowGlyph(SHADOW_GLYPH_TOP_RIGHT, right, top);
	for (int y = top + H; y < bottom; y += H) {
		drawShadowGlyph(SHADOW_GLYPH_LEFT, left, y, -1, bottom - 1);
	}
	for (int y = top + H; y < bottom; y += H) {
		drawShadowGlyph(SHADOW_GLYPH_RIGHT, right, y, -1, bottom - 1);
	}
	drawShadowGlyph(SHADOW_GLYPH_BOTTOM_LEFT, left, bottom);
	for (int x = left + W; x < right; x += W) {
		drawShadowGlyph(SHADOW_GLYPH_BOTTOM, x, bottom, right - 1);
	}
	drawShadowGlyph(SHADOW_GLYPH_BOTTOM_RIGHT, right, bottom);
}

void expandRectWithShadow(int &x1, int &y1, int &x2, int &y2) {
	x1 -= L;
	y1 -= T;
	x2 += R;
	y2 += B;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void drawLine(int x1, int y1, int x2, int y2) {
   int dx = x2 - x1;
   int dy = y2 - y1;
   
   int length;
   if (abs(dx) > abs(dy)) {
       length = abs(dx);
   } else {
       length = abs(dy);
   }
   
   float xinc = (float)dx / length;
   float yinc = (float)dy / length;
   float x = (float)x1;
   float y = (float)y1;
   for (int i = 0; i < length; i++) {
       display::drawPixel((int)roundf(x), (int)roundf(y));
       x += xinc;
       y += yinc;
   }
}

// http://members.chello.at/~easyfilter/bresenham.html
void drawAntialiasedLine(int x0, int y0, int x1, int y1) {
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx - dy, e2, x2;                       /* error value e_xy */
    int ed = dx + dy == 0 ? 1 : (int)sqrt((float)dx*dx + (float)dy*dy);

    for (; ; ) {                                         /* pixel loop */
        display::drawPixel(x0, y0, 255 - 255 * abs(err - dx + dy) / ed);
        e2 = err; x2 = x0;
        if (2 * e2 >= -dx) {                                    /* x step */
            if (x0 == x1) break;
            if (e2 + dy < ed) display::drawPixel(x0, y0 + sy, 255 - 255 * (e2 + dy) / ed);
            err -= dy; x0 += sx;
        }
        if (2 * e2 <= dy) {                                     /* y step */
            if (y0 == y1) break;
            if (dx - e2 < ed) display::drawPixel(x2 + sx, y0, 255 - 255 * (dx - e2) / ed);
            err += dx; y0 += sy;
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// based on SDL2 gfx primitives

void aaLine(int x1, int y1, int x2, int y2, int draw_endpoint) {
	int xx0, yy0, xx1, yy1;
	double erracc, erradj;
	int dx, dy, tmp, xdir, y0p1, x0pxdir;

	/*
	* Keep on working with 32bit numbers
	*/
	xx0 = x1;
	yy0 = y1;
	xx1 = x2;
	yy1 = y2;

	/*
	* Reorder points to make dy positive
	*/
	if (yy0 > yy1) {
		tmp = yy0;
		yy0 = yy1;
		yy1 = tmp;
		tmp = xx0;
		xx0 = xx1;
		xx1 = tmp;
	}

	/*
	* Calculate distance
	*/
	dx = xx1 - xx0;
	dy = yy1 - yy0;

	/*
	* Adjust for negative dx and set xdir
	*/
	if (dx >= 0) {
		xdir = 1;
	} else {
		xdir = -1;
		dx = (-dx);
	}

	/*
	* Check for special cases
	*/
	if (dx == 0) {
		/*
		* Vertical line
		*/
		if (draw_endpoint) {
			display::drawVLine(x1, y1, y2 - y1 + 1);
		} else {
			if (dy > 0) {
				display::drawVLine(x1, yy0, yy0 + dy - yy0);
			} else {
				display::drawPixel(x1, y1);
			}
		}
	}
	
	if (dy == 0) {
		/*
		* Horizontal line
		*/
		if (draw_endpoint) {
			display::drawHLine(x1, y1, x2 - x1 + 1);
		} else {
			if (dx > 0) {
				if (xdir > 0) {
					display::drawHLine(xx0, y1, dx);
				} else {
					display::drawHLine(xx0 - dx, y1, xx0 + dx - xx0);
				}
			} else {
				display::drawPixel(x1, y1);
			}
		}
		return;
	}
	
//	if ((dx == dy) && (draw_endpoint)) {
//		/*
//		* Diagonal line (with endpoint)
//		*/
//		drawLine(x1, y1, x2, y2);
//		return;
//	}


	/*
	* Zero accumulator
	*/
	erracc = 0;

	/*
	* Draw the initial pixel in the foreground color
	*/
	display::drawPixel(x1, y1);

	/*
	* x-major or y-major?
	*/
	if (dy > dx) {

		/*
		* y-major.  Calculate 16-bit fixed point fractional part of a pixel that
		* X advances every time Y advances 1 pixel, truncating the result so that
		* we won't overrun the endpoint along the X axis
		*/
		/*
		* Not-so-portable version: erradj = ((Uint64)dx << 32) / (Uint64)dy;
		*/
		erradj = 1.0 * dx / dy;

		/*
		* draw all pixels other than the first and last
		*/
		x0pxdir = xx0 + xdir;
		while (--dy) {
			erracc += erradj;
			if (erracc >= 1.0) {
				erracc -= 1.0;
				/*
				* rollover in error accumulator, x coord advances
				*/
				xx0 = x0pxdir;
				x0pxdir += xdir;
			}
			yy0++;		/* y-major so always advance Y */

			/*
			* the AAbits most significant bits of erracc give us the intensity
			* weighting for this pixel, and the complement of the weighting for
			* the paired pixel.
			*/
			auto wgt = (int)(255 * erracc);
			display::drawPixel(xx0, yy0, 255 - wgt);
			display::drawPixel(x0pxdir, yy0, wgt);
		}

	} else {

		/*
		* x-major line.  Calculate 16-bit fixed-point fractional part of a pixel
		* that Y advances each time X advances 1 pixel, truncating the result so
		* that we won't overrun the endpoint along the X axis.
		*/
		/*
		* Not-so-portable version: erradj = ((Uint64)dy << 32) / (Uint64)dx;
		*/
		erradj = 1.0 * dy / dx;

		/*
		* draw all pixels other than the first and last
		*/
		y0p1 = yy0 + 1;
		while (--dx) {
			erracc += erradj;
			if (erracc >= 1.0) {
				erracc -= 1.0;
				/*
				* Accumulator turned over, advance y
				*/
				yy0 = y0p1;
				y0p1++;
			}
			xx0 += xdir;	/* x-major so always advance X */
			/*
			* the AAbits most significant bits of erracc give us the intensity
			* weighting for this pixel, and the complement of the weighting for
			* the paired pixel.
			*/
			auto wgt = (int)(255 * erracc);
			display::drawPixel(xx0, yy0, 255 - wgt);
			display::drawPixel(xx0, y0p1, wgt);
		}
	}

	/*
	* Do we have to draw the endpoint
	*/
	if (draw_endpoint) {
		/*
		* Draw final pixel, always exactly intersected by the line and doesn't
		* need to be weighted.
		*/
		display::drawPixel(x2, y2);
	}
}

int compareInt(const void *a, const void *b) {
	return (*(const int *)a) - (*(const int *)b);
}

int drawPolygon(const int16_t *vx, const int16_t *vy, int n) {
	/*
	* Sanity check number of edges
	*/
	if (n < 3) {
		return -1;
	}

    for (int i = 1; i < n; i++) {
        aaLine(vx[i - 1], vy[i - 1], vx[i], vy[i], 0);
		//drawAntialiasedLine(vx[i - 1], vy[i - 1], vx[i], vy[i]);
    }
    
	aaLine(vx[n - 1], vy[n - 1], vx[0], vy[0], 0);
	//drawAntialiasedLine(vx[n - 1], vy[n - 1], vx[0], vy[0]);

    return 0;
}

int fillPolygon(const int16_t *vx, const int16_t *vy, int n, int *polyInts) {
	int i;
	int y, xa, xb;
	int miny, maxy;
	int x1, y1;
	int x2, y2;
	int ind1, ind2;
	int ints;

	/*
	* Vertex array NULL check
	*/
	if (vx == NULL) {
		return (-1);
	}
	if (vy == NULL) {
		return (-1);
	}

	/*
	* Sanity check number of edges
	*/
	if (n < 3) {
		return -1;
	}

	/*
	* Determine Y maxima
	*/
	miny = vy[0];
	maxy = vy[0];
	for (i = 1; (i < n); i++) {
		if (vy[i] < miny) {
			miny = vy[i];
		} else if (vy[i] > maxy) {
			maxy = vy[i];
		}
	}

	/*
	* Draw, scanning y
	*/
	for (y = miny; (y <= maxy); y++) {
		ints = 0;
		for (i = 0; (i < n); i++) {
			if (!i) {
				ind1 = n - 1;
				ind2 = 0;
			} else {
				ind1 = i - 1;
				ind2 = i;
			}
			y1 = vy[ind1];
			y2 = vy[ind2];
			if (y1 < y2) {
				x1 = vx[ind1];
				x2 = vx[ind2];
			} else if (y1 > y2) {
				y2 = vy[ind1];
				y1 = vy[ind2];
				x2 = vx[ind1];
				x1 = vx[ind2];
			} else {
				continue;
			}
			if (((y >= y1) && (y < y2)) || ((y == maxy) && (y > y1) && (y <= y2))) {
				polyInts[ints++] = ((65536 * (y - y1)) / (y2 - y1)) * (x2 - x1) + (65536 * x1);
			}
		}

		qsort(polyInts, ints, sizeof(int), compareInt);

		/*
		* Set color
		*/
		for (i = 0; (i < ints); i += 2) {
			xa = polyInts[i] + 1;
			xa = (xa >> 16) + ((xa & 32768) >> 15);
			xb = polyInts[i + 1] - 1;
			xb = (xb >> 16) + ((xb & 32768) >> 15);
			display::drawHLine(xa, y, xb - xa + 1);
		}
	}

	return 0;
}

void arcBarAsPolygon(
	int xCenter,
	int yCenter,
	int radius,
	float fromAngleDeg,
	float toAngleDeg,
	float width,
	int16_t *vx,
	int16_t *vy,
	size_t &n
) {
	float fromAngle = float(fromAngleDeg * M_PI / 180);
	float toAngle = float(toAngleDeg * M_PI / 180);

	int j = 0;

	vx[j] = floor(xCenter + (radius + width / 2.0f) * cosf(fromAngle));
	vy[j] = floor(yCenter - (radius + width / 2.0f) * sinf(fromAngle));
	j++;

	for (size_t i = 0; ; i++) {
		auto angle = i * 2 * M_PI / n;
		if (angle >= toAngle) {
			break;
		}
		if (angle > fromAngle) {
			vx[j] = floor(xCenter + (radius + width / 2.0f) * cosf(angle));
			vy[j] = floor(yCenter - (radius + width / 2.0f) * sinf(angle));
			j++;
		}
	}

	vx[j] = floor(xCenter + (radius + width / 2.0f) * cosf(toAngle));
	vy[j] = floor(yCenter - (radius + width / 2.0f) * sinf(toAngle));
	j++;

	vx[j] = floor(xCenter + (radius - width / 2.0f) * cosf(toAngle));
	vy[j] = floor(yCenter - (radius - width / 2.0f) * sinf(toAngle));
	j++;

	for (size_t i = 0; ; i++) {
		auto angle = 2 * M_PI - i * 2 * M_PI / n;
		if (angle <= fromAngle) {
			break;
		}

		if (angle < toAngle) {
			vx[j] = floor(xCenter + (radius - width / 2.0f) * cosf(angle));
			vy[j] = floor(yCenter - (radius - width / 2.0f) * sinf(angle));
			j++;
		}
	}

	vx[j] = floor(xCenter + (radius - width / 2.0f) * cosf(fromAngle));
	vy[j] = floor(yCenter - (radius - width / 2.0f) * sinf(fromAngle));
	j++;

	n = j;
}

void drawArcBar(int xCenter, int yCenter, int radius, float fromAngleDeg, float toAngleDeg, int width) {
	static const size_t N = 200;
	int16_t vx[N + 8];
	int16_t vy[N + 8];
	size_t n = N;
	arcBarAsPolygon(xCenter, yCenter, radius, fromAngleDeg, toAngleDeg, width, vx, vy, n);
	drawPolygon(vx, vy, n);
}

void fillArcBar(int xCenter, int yCenter, int radius, float fromAngleDeg, float toAngleDeg, int width) {
	if (isNaN(fromAngleDeg) || fromAngleDeg < 0) {
		fromAngleDeg = 0;
	}
	if (isNaN(toAngleDeg) || toAngleDeg > 360.0f) {
		toAngleDeg = 360.0f;
	}

	static const size_t N = 200;
	int16_t vx[N + 8];
	int16_t vy[N + 8];
	int polyInts[N + 8];
	size_t n = N;
	arcBarAsPolygon(xCenter, yCenter, radius, fromAngleDeg, toAngleDeg, width, vx, vy, n);
	//drawPolygon(vx, vy, n);
	fillPolygon(vx, vy, n, polyInts);
}

} // namespace gui
} // namespace eez
