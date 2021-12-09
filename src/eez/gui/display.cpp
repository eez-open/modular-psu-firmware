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

#include <stdio.h>
#include <string.h>
#include <memory.h>

#include <eez/conf.h>
#include <eez/util.h>

#if OPTION_KEYBOARD
#include <eez/keyboard.h>
#endif

#if OPTION_MOUSE
#include <eez/mouse.h>
#endif

#include <eez/gui/gui.h>

#include <eez/gui/display-private.h>

#define CONF_BACKDROP_OPACITY 128

using namespace eez::gui;

namespace eez {
namespace gui {
namespace display {

uint16_t g_fc, g_bc;
uint8_t g_opacity = 255;

gui::font::Font g_font;

static uint8_t g_colorCache[256][4];

#define FLOAT_TO_COLOR_COMPONENT(F) ((F) < 0 ? 0 : (F) > 255 ? 255 : (uint8_t)(F))
#define RGB_TO_HIGH_BYTE(R, G, B) (((R) & 248) | (G) >> 5)
#define RGB_TO_LOW_BYTE(R, G, B) (((G) & 28) << 3 | (B) >> 3)

static const uint16_t *g_themeColors;
static uint32_t g_themeColorsCount;
static const uint16_t *g_colors;

bool g_dirty;

Buffer g_buffers[NUM_BUFFERS];
static void *g_mainBufferPointer;
static int g_numBuffersToDraw;

////////////////////////////////////////////////////////////////////////////////

uint32_t color16to32(uint16_t color, uint8_t opacity) {
    uint32_t color32;
    ((uint8_t *)&color32)[0] = COLOR_TO_B(color);
    ((uint8_t *)&color32)[1] = COLOR_TO_G(color);
    ((uint8_t *)&color32)[2] = COLOR_TO_R(color);
    ((uint8_t *)&color32)[3] = opacity;
    return color32;
}

uint16_t color32to16(uint32_t color) {
    auto pcolor = (uint8_t *)&color;
    return RGB_TO_COLOR(pcolor[2], pcolor[1], pcolor[0]);
}

uint32_t blendColor(uint32_t fgColor, uint32_t bgColor) {
    uint8_t *fg = (uint8_t *)&fgColor;
    uint8_t *bg = (uint8_t *)&bgColor;

    float alphaMult = fg[3] * bg[3] / 255.0f;
    float alphaOut = fg[3] + bg[3] - alphaMult;

    float r = (fg[2] * fg[3] + bg[2] * bg[3] - bg[2] * alphaMult) / alphaOut;
    float g = (fg[1] * fg[3] + bg[1] * bg[3] - bg[1] * alphaMult) / alphaOut;
    float b = (fg[0] * fg[3] + bg[0] * bg[3] - bg[0] * alphaMult) / alphaOut;

    r = clamp(r, 0.0f, 255.0f);
    g = clamp(g, 0.0f, 255.0f);
    b = clamp(b, 0.0f, 255.0f);

    uint32_t result;
    uint8_t *presult = (uint8_t *)&result;
    presult[0] = (uint8_t)b;
    presult[1] = (uint8_t)g;
    presult[2] = (uint8_t)r;
    presult[3] = (uint8_t)alphaOut;

    return result;
}

void onThemeChanged() {
	if (g_isMainAssetsLoaded) {
		auto selectedThemeIndex = getSelectedThemeIndexHook();
		g_themeColors = getThemeColors(selectedThemeIndex);
		g_themeColorsCount = getThemeColorsCount(selectedThemeIndex);
		g_colors = getColors();
	}
}

void onLuminocityChanged() {
    // invalidate cache
    for (int i = 0; i < 256; ++i) {
        g_colorCache[i][0] = 0;
        g_colorCache[i][1] = 0;
        g_colorCache[i][2] = 0;
        g_colorCache[i][3] = 0;
    }
}

////////////////////////////////////////////////////////////////////////////////

#define swap(type, i, j) {type t = i; i = j; j = t;}

void rgbToHsl(float r, float g, float b, float &h, float &s, float &l) {
    r /= 255;
    g /= 255;
    b /= 255;

    float min = r;
    float mid = g;
    float max = b;

    if (min > mid) {
        swap(float, min, mid);
    }
    if (mid > max) {
        swap(float, mid, max);
    }
    if (min > mid) {
        swap(float, min, mid);
    }

    l = (max + min) / 2;

    if (max == min) {
        h = s = 0; // achromatic
    } else {
        float d = max - min;
        s = l > 0.5 ? d / (2 - max - min) : d / (max + min);

        if (max == r) {
            h = (g - b) / d + (g < b ? 6 : 0);
        } else if (max == g) {
            h = (b - r) / d + 2;
        } else if (max == b) {
            h = (r - g) / d + 4;
        }

        h /= 6;
    }
}

float hue2rgb(float p, float q, float t) {
    if (t < 0) t += 1;
    if (t > 1) t -= 1;
    if (t < 1.0f/6) return p + (q - p) * 6 * t;
    if (t < 1.0f/2) return q;
    if (t < 2.0f/3) return p + (q - p) * (2.0f/3 - t) * 6;
    return p;
}

void hslToRgb(float h, float s, float l, float &r, float &g, float &b) {
    if (s == 0) {
        r = g = b = l; // achromatic
    } else {
        float q = l < 0.5 ? l * (1 + s) : l + s - l * s;
        float p = 2 * l - q;

        r = hue2rgb(p, q, h + 1.0f/3);
        g = hue2rgb(p, q, h);
        b = hue2rgb(p, q, h - 1.0f/3);
    }

    r *= 255;
    g *= 255;
    b *= 255;
}

void adjustColor(uint16_t &c) {
    if (getDisplayBackgroundLuminosityStepHook() == DISPLAY_BACKGROUND_LUMINOSITY_STEP_DEFAULT) {
        return;
    }

	uint8_t ch = c >> 8;
	uint8_t cl = c & 0xFF;

    int i = (ch & 0xF0) | (cl & 0x0F);
    if (ch == g_colorCache[i][0] && cl == g_colorCache[i][1]) {
        // cache hit!
		c = (g_colorCache[i][2] << 8) | g_colorCache[i][3];
        return;
    }

    uint8_t r, g, b;
    r = ch & 248;
    g = ((ch << 5) | (cl >> 3)) & 252;
    b = cl << 3;

    float h, s, l;
    rgbToHsl(r, g, b, h, s, l);

    float a = l < 0.5 ? l : 1 - l;
    if (a > 0.3f) {
        a = 0.3f;
    }
    float lmin = l - a;
    float lmax = l + a;

    float lNew = remap((float)getDisplayBackgroundLuminosityStepHook(),
        (float)DISPLAY_BACKGROUND_LUMINOSITY_STEP_MIN,
        lmin,
        (float)DISPLAY_BACKGROUND_LUMINOSITY_STEP_MAX,
        lmax);

    float floatR, floatG, floatB;
    hslToRgb(h, s, lNew, floatR, floatG, floatB);

    r = FLOAT_TO_COLOR_COMPONENT(floatR);
    g = FLOAT_TO_COLOR_COMPONENT(floatG);
    b = FLOAT_TO_COLOR_COMPONENT(floatB);

    uint8_t chNew = RGB_TO_HIGH_BYTE(r, g, b);
    uint8_t clNew = RGB_TO_LOW_BYTE(r, g, b);

    // store new color in the cache
    g_colorCache[i][0] = ch;
    g_colorCache[i][1] = cl;
    g_colorCache[i][2] = chNew;
    g_colorCache[i][3] = clNew;

	c = (chNew << 8) | clNew;
}

uint16_t getColor16FromIndex(uint16_t color) {
    color = transformColorHook(color);
	return color < g_themeColorsCount ? g_themeColors[color] : g_colors[color - g_themeColorsCount];
}

void setColor(uint8_t r, uint8_t g, uint8_t b) {
    g_fc = RGB_TO_COLOR(r, g, b);
	adjustColor(g_fc);
}

void setColor16(uint16_t color) {
    g_fc = color;
    adjustColor(g_fc);
}

void setColor(uint16_t color, bool ignoreLuminocity) {
    g_fc = getColor16FromIndex(color);
    adjustColor(g_fc);
}

uint16_t getColor() {
    return g_fc;
}

void setBackColor(uint8_t r, uint8_t g, uint8_t b) {
    g_bc = RGB_TO_COLOR(r, g, b);
	adjustColor(g_bc);
}

void setBackColor(uint16_t color, bool ignoreLuminocity) {
	g_bc = getColor16FromIndex(color);
	adjustColor(g_bc);
}

uint16_t getBackColor() {
    return g_bc;
}

uint8_t setOpacity(uint8_t opacity) {
    uint8_t savedOpacity = g_opacity;
    g_opacity = opacity;
    return savedOpacity;
}

uint8_t getOpacity() {
    return g_opacity;
}

////////////////////////////////////////////////////////////////////////////////

void drawHLine(int x, int y, int l) {
    fillRect(x, y, x + l, y);
}

void drawVLine(int x, int y, int l) {
    fillRect(x, y, x, y + l);
}

void drawRect(int x1, int y1, int x2, int y2) {
    drawHLine(x1, y1, x2 - x1);
    drawHLine(x1, y2, x2 - x1);
    drawVLine(x1, y1, y2 - y1);
    drawVLine(x2, y1, y2 - y1);

    setDirty();
}

void drawFocusFrame(int x, int y, int w, int h) {
    int lineWidth = MIN(MIN(3, w), h);

    setColor16(RGB_TO_COLOR(255, 0, 255));

    // top
    fillRect(x, y, x + w - 1, y + lineWidth - 1);

    // left
    fillRect(x, y + lineWidth, x + lineWidth - 1, y + h - lineWidth - 1);

    // right
    fillRect(x + w - lineWidth, y + lineWidth, x + w - 1, y + h - lineWidth - 1);

    // bottom
    fillRect(x, y + h - lineWidth, x + w - 1, y + h - 1);
}

void fillRoundedRect(int x1, int y1, int x2, int y2, int r) {
    fillRect(x1 + r, y1, x2 - r, y1 + r - 1);
    fillRect(x1, y1 + r, x1 + r - 1, y2 - r);
    fillRect(x2 + 1 - r, y1 + r, x2, y2 - r);
    fillRect(x1 + r, y2 - r + 1, x2 - r, y2);
    fillRect(x1 + r, y1 + r, x2 - r, y2 - r);

    for (int ry = 0; ry <= r; ry++) {
        int rx = (int)round(sqrt(r * r - ry * ry));
        drawHLine(x2 - r, y2 - r + ry, rx);
        drawHLine(x1 + r - rx, y2 - r + ry, rx);
        drawHLine(x2 - r, y1 + r - ry, rx);
        drawHLine(x1 + r - rx, y1 + r - ry, rx);
    }
}

////////////////////////////////////////////////////////////////////////////////

static int8_t measureGlyph(uint8_t encoding) {
    auto glyph = g_font.getGlyph(encoding);
    if (!glyph)
        return 0;

    return glyph->dx;
}

int8_t measureGlyph(uint8_t encoding, gui::font::Font &font) {
    auto glyph = g_font.getGlyph(encoding);
    if (!glyph)
        return 0;

    return glyph->dx;
}

int measureStr(const char *text, int textLength, gui::font::Font &font, int max_width) {
    g_font = font;

    int width = 0;

    if (textLength == -1) {
        char encoding;
        while ((encoding = *text++) != 0) {
            int glyph_width = measureGlyph(encoding);
            if (max_width > 0 && width + glyph_width > max_width) {
                return max_width;
            }
            width += glyph_width;
        }
    } else {
        for (int i = 0; i < textLength && text[i]; ++i) {
            char encoding = text[i];
            int glyph_width = measureGlyph(encoding);
            if (max_width > 0 && width + glyph_width > max_width) {
                return max_width;
            }
            width += glyph_width;
        }
    }

    return width;
}

void drawStr(const char *text, int textLength, int x, int y, int clip_x1, int clip_y1, int clip_x2, int clip_y2, gui::font::Font &font, int cursorPosition) {
    g_font = font;

    drawStrInit();

    if (textLength == -1) {
        textLength = strlen(text);
    }

    int xCursor = x;

    int i;

    for (i = 0; i < textLength && text[i]; ++i) {
        char encoding = text[i];
        if (i == cursorPosition) {
            xCursor = x;
        }

        auto x1 = x;
        auto y1 = y;

        auto glyph = g_font.getGlyph(encoding);
        if (glyph) {
            int x_glyph = x1 + glyph->x;
            int y_glyph = y1 + g_font.getAscent() - (glyph->y + glyph->height);

            // draw glyph pixels
            int iStartByte = 0;
            if (x_glyph < clip_x1) {
                int dx_off = clip_x1 - x_glyph;
                iStartByte = dx_off;
                x_glyph = clip_x1;
            }

			if (iStartByte < glyph->width) {
				int offset = 0;
				int glyphHeight = glyph->height;
				if (y_glyph < clip_y1) {
					int dy_off = clip_y1 - y_glyph;
					offset += dy_off * glyph->width;
					glyphHeight -= dy_off;
					y_glyph = clip_y1;
				}

				int width;
				if (x_glyph + (glyph->width - iStartByte) - 1 > clip_x2) {
					width = clip_x2 - x_glyph + 1;
				} else {
					width = (glyph->width - iStartByte);
				}

				int height;
				if (y_glyph + glyphHeight - 1 > clip_y2) {
					height = clip_y2 - y_glyph + 1;
				} else {
					height = glyphHeight;
				}

				if (width > 0 && height > 0) {
					drawGlyph(glyph->pixels + offset + iStartByte, glyph->width - width, x_glyph, y_glyph, width, height);
				}
			}
			
			x += glyph->dx;
		}
    }

    if (i == cursorPosition) {
        xCursor = x;
    }

    if (cursorPosition != -1) {
        auto d = MAX(((clip_y2 - clip_y1) - font.getHeight()) / 2, 0);
        fillRect(xCursor - CURSOR_WIDTH / 2, clip_y1 + d, xCursor + CURSOR_WIDTH / 2 - 1, clip_y2 - d);
    }

    setDirty();
}

int getCharIndexAtPosition(int xPos, const char *text, int textLength, int x, int y, int clip_x1, int clip_y1, int clip_x2,int clip_y2, gui::font::Font &font) {
    if (textLength == -1) {
        textLength = strlen(text);
    }

    int i;

    for (i = 0; i < textLength && text[i]; ++i) {
        char encoding = text[i];
        auto glyph = font.getGlyph(encoding);
        auto dx = 0;
        if (glyph) {
            dx = glyph->dx;
        }
        if (xPos < x + dx / 2) {
            return i;
        }
        x += dx;
    }

    return i;
}

int getCursorXPosition(int cursorPosition, const char *text, int textLength, int x, int y, int clip_x1, int clip_y1, int clip_x2,int clip_y2, gui::font::Font &font) {
    if (textLength == -1) {
        textLength = strlen(text);
    }

    for (int i = 0; i < textLength && text[i]; ++i) {
        if (i == cursorPosition) {
            return x;
        }
        char encoding = text[i];
        auto glyph = font.getGlyph(encoding);
        if (glyph) {
            x += glyph->dx;
        }
    }

    return x;
}

////////////////////////////////////////////////////////////////////////////////

void beginRendering() {
    g_mainBufferPointer = getBufferPointer();
    g_numBuffersToDraw = 0;
}

int beginBufferRendering() {
    int bufferIndex = g_numBuffersToDraw++;
    g_buffers[bufferIndex].previousBuffer = getBufferPointer();
    setBufferPointer(g_buffers[bufferIndex].bufferPointer);
    return bufferIndex;
}

void endBufferRendering(int bufferIndex, int x, int y, int width, int height, bool withShadow, uint8_t opacity, int xOffset, int yOffset, Rect *backdrop) {
    Buffer &buffer = g_buffers[bufferIndex];
    
    buffer.x = x;
    buffer.y = y;
    buffer.width = width;
    buffer.height = height;
    buffer.withShadow = withShadow;
    buffer.opacity = opacity;
    buffer.xOffset = xOffset;
    buffer.yOffset = yOffset;
    buffer.backdrop = backdrop;

    setBufferPointer(buffer.previousBuffer);
}

void endRendering() {
    setBufferPointer(g_mainBufferPointer);

#if OPTION_KEYBOARD
    if (keyboard::isDisplayDirty()) {
    	setDirty();
    }
#endif

#if OPTION_MOUSE
    if (mouse::isDisplayDirty()) {
    	setDirty();
    }
#endif

    if (isDirty()) {
        for (int bufferIndex = 0; bufferIndex < g_numBuffersToDraw; bufferIndex++) {
            Buffer &buffer = g_buffers[bufferIndex];

            int sx = buffer.x;
            int sy = buffer.y;

            int x1 = buffer.x + buffer.xOffset;
            int y1 = buffer.y + buffer.yOffset;
            int x2 = x1 + buffer.width - 1;
            int y2 = y1 + buffer.height - 1;

            if (buffer.backdrop) {
                // opacity backdrop
                auto savedOpacity = setOpacity(CONF_BACKDROP_OPACITY);
                setColor(EEZ_CONF_COLOR_ID_BACKDROP);
                fillRect(buffer.backdrop->x, buffer.backdrop->y, buffer.backdrop->x + buffer.backdrop->w - 1, buffer.backdrop->y + buffer.backdrop->h - 1);
                setOpacity(savedOpacity);
            }

            if (buffer.withShadow) {
                drawShadow(x1, y1, x2, y2);
            }

            bitBlt(g_buffers[bufferIndex].bufferPointer, nullptr, sx, sy, x2 - x1 + 1, y2 - y1 + 1, x1, y1, buffer.opacity);
        }

#if OPTION_KEYBOARD
        keyboard::updateDisplay();
#endif

#if OPTION_MOUSE
        mouse::updateDisplay();
#endif
    }
}

} // namespace display
} // namespace gui
} // namespace eez
