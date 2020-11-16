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

#include <stdint.h>

#include <eez/gui/font.h>
#include <eez/gui/geometry.h>

#include <eez/libs/image/image.h>

static const int CURSOR_WIDTH = 2;

namespace eez {
namespace mcu {
namespace display {

void sync();
void finishAnimation();

void turnOn();
void turnOff();
bool isOn();

void onThemeChanged();
void onLuminocityChanged();
void updateBrightness();

#define COLOR_BLACK 0x0000
#define COLOR_WHITE 0xFFFF
#define COLOR_RED 0xF800
#define COLOR_GREEN 0x0400
#define COLOR_BLUE 0x001F

// C: rrrrrggggggbbbbb
#define RGB_TO_COLOR(R, G, B)                                                                      \
    (uint16_t((R)&0xF8) << 8) | (uint16_t((G)&0xFC) << 3) | (((B)&0xF8) >> 3)

#define COLOR_TO_R(C) (uint8_t(((C) >> 11) << 3))
#define COLOR_TO_G(C) (uint8_t((((C) >> 5) << 2) & 0xFF))
#define COLOR_TO_B(C) (uint8_t(((C) << 3) & 0xFF))

extern uint16_t g_fc, g_bc;
extern uint8_t g_opacity;

extern gui::font::Font g_font;

uint32_t color16to32(uint16_t color, uint8_t opacity = 255);
uint16_t color32to16(uint32_t color);
uint32_t blendColor(uint32_t fgColor, uint32_t bgColor);

int getDisplayWidth();
int getDisplayHeight();

uint16_t getColor16FromIndex(uint16_t color);

void setColor(uint8_t r, uint8_t g, uint8_t b);
void setColor16(uint16_t color16);
void setColor(uint16_t color, bool ignoreLuminocity = false);
uint16_t getColor();

uint16_t transformColorHook(uint16_t color);

void setBackColor(uint8_t r, uint8_t g, uint8_t b);
void setBackColor(uint16_t color, bool ignoreLuminocity = false);
uint16_t getBackColor();

uint8_t setOpacity(uint8_t opacity);
uint8_t getOpacity();

const uint8_t * takeScreenshot();

void clearDirty();
// void markDirty(int x1, int y1, int x2, int y2);
extern bool g_dirty;
#define markDirty(x1, y1, x2, y2) g_dirty = true
bool isDirty();

void drawPixel(int x, int y);
void drawPixel(int x, int y, uint8_t opacity);
void drawRect(int x1, int y1, int x2, int y2);
void drawFocusFrame(int x, int y, int w, int h);
void fillRect(int x1, int y1, int x2, int y2, int radius = 0);
void fillRect(void *dst, int x1, int y1, int x2, int y2);
void fillRoundedRect(int x1, int y1, int x2, int y2, int r);
void drawHLine(int x, int y, int l);
void drawVLine(int x, int y, int l);
void bitBlt(int x1, int y1, int x2, int y2, int x, int y);
void bitBlt(void *src, int x1, int y1, int x2, int y2);
void bitBlt(void *src, void *dst, int x1, int y1, int x2, int y2);
void bitBlt(void *src, void *dst, int sx, int sy, int sw, int sh, int dx, int dy, uint8_t opacity);
void drawBitmap(Image *image, int x, int y);
void drawStr(const char *text, int textLength, int x, int y, int clip_x1, int clip_y1, int clip_x2,int clip_y2, gui::font::Font &font, int cursorPosition);
int getCharIndexAtPosition(int xPos, const char *text, int textLength, int x, int y, int clip_x1, int clip_y1, int clip_x2,int clip_y2, gui::font::Font &font);
int getCursorXPosition(int cursorPosition, const char *text, int textLength, int x, int y, int clip_x1, int clip_y1, int clip_x2,int clip_y2, gui::font::Font &font);
int8_t measureGlyph(uint8_t encoding, gui::font::Font &font);
int measureStr(const char *text, int textLength, gui::font::Font &font, int max_width = 0);

static const int NUM_BUFFERS = 6;
struct BufferFlags {
    unsigned allocated : 1;
    unsigned used : 1;
};

struct Buffer {
    void *bufferPointer;
    BufferFlags flags;
    int x;
    int y;
    int width;
    int height;
    bool withShadow;
    uint8_t opacity;
    int xOffset;
    int yOffset;
    gui::Rect *backdrop;
};
extern Buffer g_buffers[NUM_BUFFERS];

int allocBuffer();
void freeBuffer(int bufferIndex);
void selectBuffer(int bufferIndex);
void setBufferBounds(int bufferIndex, int x, int y, int width, int height, bool withShadow, uint8_t opacity, int xOffset, int yOffset, gui::Rect *backdrop);
void beginBuffersDrawing();
void endBuffersDrawing();

void *getBufferPointer();
void setBufferPointer(void *buffer);

} // namespace display
} // namespace mcu
} // namespace eez
