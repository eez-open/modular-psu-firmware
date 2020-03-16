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

#if OPTION_DISPLAY

#include <eez/util.h>

#include <eez/gui/gui.h>

// TODO
#include <eez/modules/psu/psu.h>
#include <eez/modules/psu/persist_conf.h>

using namespace eez::gui;

namespace eez {
namespace mcu {
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

void onThemeChanged() {
	if (g_assetsLoaded) {
		g_themeColors = getThemeColors(psu::persist_conf::devConf.selectedThemeIndex);
		g_themeColorsCount = getThemeColorsCount(psu::persist_conf::devConf.selectedThemeIndex);
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
    if (psu::persist_conf::devConf.displayBackgroundLuminosityStep == DISPLAY_BACKGROUND_LUMINOSITY_STEP_DEFAULT) {
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

    float lNew = remap((float)psu::persist_conf::devConf.displayBackgroundLuminosityStep,
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

static int g_dirtyX1;
static int g_dirtyY1;
static int g_dirtyX2;
static int g_dirtyY2;

void clearDirty() {
    g_dirtyX1 = getDisplayWidth();
    g_dirtyY1 = getDisplayHeight();
    g_dirtyX2 = -1;
    g_dirtyY2 = -1;
}

void markDirty(int x1, int y1, int x2, int y2) {
    if (x1 < g_dirtyX1) {
        g_dirtyX1 = x1;
    }
    if (x2 > g_dirtyX2) {
        g_dirtyX2 = x2;
    }
    if (y1 < g_dirtyY1) {
        g_dirtyY1 = y1;
    }
    if (y2 > g_dirtyY2) {
        g_dirtyY2 = y2;
    }
}

bool isDirty() {
    if (g_dirtyX1 <= g_dirtyX2 && g_dirtyY1 <= g_dirtyY2) {
        printf("%d x %d\n", g_dirtyX2 - g_dirtyX1 + 1, g_dirtyY2 - g_dirtyY1 + 1);
        return true;
    }
    return false;
}

static int8_t measureGlyph(uint8_t encoding) {
    gui::font::Glyph glyph;
    g_font.getGlyph(encoding, glyph);
    if (!glyph)
        return 0;

    return glyph.dx;
}

int8_t measureGlyph(uint8_t encoding, gui::font::Font &font) {
    gui::font::Glyph glyph;
    font.getGlyph(encoding, glyph);
    if (!glyph)
        return 0;

    return glyph.dx;
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

#if OPTION_SDRAM

Buffer g_buffers[NUM_BUFFERS];

static void *g_bufferPointer;

static int g_bufferToDrawIndexes[NUM_BUFFERS];
static int g_numBuffersToDraw;

//int getNumFreeBuffers() {
//    int count = 0;
//    for (int bufferIndex = 0; bufferIndex < NUM_BUFFERS; bufferIndex++) {
//        if (!g_buffers[bufferIndex].flags.allocated) {
//            count++;
//        }
//    }
//    return count;
//}

int allocBuffer() {
    int bufferIndex;

    for (bufferIndex = 0; bufferIndex < NUM_BUFFERS; bufferIndex++) {
        if (!g_buffers[bufferIndex].flags.allocated) {
            // DebugTrace("Buffer %d allocated, %d more left!\n", bufferIndex, getNumFreeBuffers());
            break;
        }
    }

    if (bufferIndex == NUM_BUFFERS) {
        // DebugTrace("There is no free buffer available!\n");
        bufferIndex = NUM_BUFFERS - 1;
    }

    g_buffers[bufferIndex].flags.allocated = true;

    return bufferIndex;
}

void freeBuffer(int bufferIndex) {
    g_buffers[bufferIndex].flags.allocated = false;
    // DebugTrace("Buffer %d freed up, %d buffers available now!\n", bufferIndex, getNumFreeBuffers());
}

void selectBuffer(int bufferIndex) {
    g_buffers[bufferIndex].flags.used = true;
    g_bufferToDrawIndexes[g_numBuffersToDraw++] = bufferIndex;
    setBufferPointer(g_buffers[bufferIndex].bufferPointer);
}

void setBufferBounds(int bufferIndex, int x, int y, int width, int height, bool withShadow, uint8_t opacity, int xOffset, int yOffset) {
    Buffer &buffer = g_buffers[bufferIndex];
    
    if (buffer.x != x || buffer.y != y || buffer.width != width || buffer.height != height || buffer.withShadow != withShadow || buffer.opacity != opacity || buffer.xOffset != xOffset || buffer.yOffset != yOffset) {
        buffer.x = x;
        buffer.y = y;
        buffer.width = width;
        buffer.height = height;
        buffer.withShadow = withShadow;
        buffer.opacity = opacity;
        buffer.xOffset = xOffset;
        buffer.yOffset = yOffset;

        int x1 = x + xOffset;
        int y1 = y + yOffset;
        int x2 = x1 + width - 1;
        int y2 = y1 + height - 1;
        if (withShadow) {
            expandRectWithShadow(x1, y1, x2, y2);
        }
        markDirty(x1, y1, x2, y2);
    }

    for (int i = 0; i < g_numBuffersToDraw; i++) {
        if (g_bufferToDrawIndexes[i] == bufferIndex) {
            if (i > 0) {
                setBufferPointer(g_buffers[g_bufferToDrawIndexes[i - 1]].bufferPointer);
            }
            break;
        }
    }
}

void clearBufferUsage() {
    for (int bufferIndex = 0; bufferIndex < NUM_BUFFERS; bufferIndex++) {
        g_buffers[bufferIndex].flags.used = false;
    }
}

void freeUnusedBuffers() {
    for (int bufferIndex = 0; bufferIndex < NUM_BUFFERS; bufferIndex++) {
        if (g_buffers[bufferIndex].flags.allocated && !g_buffers[bufferIndex].flags.used) {
            g_buffers[bufferIndex].flags.allocated = false;
            // DebugTrace("Buffer %d allocated but not used!\n", bufferIndex);
        }
    }
    
    clearBufferUsage();
}

void beginBuffersDrawing() {
    g_bufferPointer = getBufferPointer();
}

void endBuffersDrawing() {
    setBufferPointer(g_bufferPointer);

    if (isDirty()) {
        for (int i = 0; i < g_numBuffersToDraw; i++) {
            int bufferIndex = g_bufferToDrawIndexes[i];
            Buffer &buffer = g_buffers[bufferIndex];
            if (buffer.withShadow) {
                drawShadow(buffer.x + buffer.xOffset, buffer.y + buffer.yOffset, buffer.x + buffer.xOffset + buffer.width - 1, buffer.y + buffer.yOffset + buffer.height - 1);
                bitBlt(buffer.bufferPointer, nullptr, buffer.x, buffer.y, buffer.width, buffer.height, buffer.x + buffer.xOffset, buffer.y + buffer.yOffset, buffer.opacity);
            } else {
                int sx = buffer.x;
                int sy = buffer.y;

                int x1 = buffer.x + buffer.xOffset;
                int y1 = buffer.y + buffer.yOffset;
                int x2 = x1 + buffer.width - 1;
                int y2 = y1 + buffer.height - 1;

                if (x1 < g_dirtyX1) {
                    int xd = g_dirtyX1 - x1;
                    sx += xd;
                    x1 += xd;
                }

                if (y1 < g_dirtyY1) {
                    int yd = g_dirtyY1 - y1;
                    sy += yd;
                    y1 += yd;
                }

                if (x2 > g_dirtyX2) {
                    x2 = g_dirtyX2;
                }

                if (y2 > g_dirtyY2) {
                    y2 = g_dirtyY2;
                }

                bitBlt(buffer.bufferPointer, nullptr, sx, sy, x2 - x1 + 1, y2 - y1 + 1, x1, y1, buffer.opacity);
            }
        }
    }

    g_numBuffersToDraw = 0;

    freeUnusedBuffers();
}

#endif

} // namespace display
} // namespace mcu
} // namespace eez

#endif
