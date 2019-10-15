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

#include <eez/modules/mcu/display.h>

#include <math.h>
#include <memory.h>
#include <stdio.h>
#include <string.h>
#include <utility>

#include <SDL.h>
#include <SDL_image.h>

#include <cmsis_os.h>
#include <eez/apps/home/home.h>
#include <eez/debug.h>
#include <eez/gui/app_context.h>
#include <eez/gui/data.h>
#include <eez/gui/document.h>
#include <eez/gui/font.h>
#include <eez/gui/widget.h>
#include <eez/platform/simulator/front_panel.h>
#include <eez/system.h>
#include <eez/util.h>

#include "texture.h"

#include <eez/apps/home/home.h>

using namespace eez::gui;
using namespace eez::home;

namespace eez {
namespace mcu {
namespace display {

////////////////////////////////////////////////////////////////////////////////

static const char *TITLE = "EEZ Modular Firmware Simulator";
static const char *ICON = "eez.png";

static SDL_Window *g_mainWindow;
static SDL_Renderer *g_renderer;

static uint16_t g_frontPanelWidth = 1396;
static uint16_t g_frontPanelHeight = 563;
static uint32_t *g_frontPanelBuffer;

static uint32_t *g_frontPanelBuffer1;
static uint32_t *g_frontPanelBuffer2;
static uint32_t *g_frontPanelBuffer3;

static uint8_t *g_screenshotBuffer;
static int g_screenshotY;

////////////////////////////////////////////////////////////////////////////////

static uint32_t blendColor(uint32_t fgColor, uint32_t bgColor) {
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

static uint32_t color16to32(uint16_t color) {
    uint32_t color32;
    ((uint8_t *)&color32)[0] = COLOR_TO_B(color);
    ((uint8_t *)&color32)[1] = COLOR_TO_G(color);
    ((uint8_t *)&color32)[2] = COLOR_TO_R(color);
    ((uint8_t *)&color32)[3] = 255;
    return color32;
}

////////////////////////////////////////////////////////////////////////////////

// heuristics to find resource file
std::string getFullPath(std::string category, std::string path) {
    std::string fullPath = category + "/" + path;
    for (int i = 0; i < 5; ++i) {
        FILE *fp = fopen(fullPath.c_str(), "r");
        if (fp) {
            fclose(fp);
            return fullPath;
        }
        fullPath = std::string("../") + fullPath;
    }
    return path;
}

int getDesktopResolution(int *w, int *h) {
    SDL_Init(SDL_INIT_VIDEO);

    SDL_DisplayMode dm;
    if (SDL_GetDesktopDisplayMode(0, &dm) == 0) {
        *w = dm.w;
        *h = dm.h;

        return 1;
    }

    *w = -1;
    *h = -1;

    return 0;
}

bool init() {
    // Set texture filtering to linear
    if (!SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1")) {
        printf("Warning: Linear texture filtering not enabled!");
    }

    // Create window
    g_mainWindow = SDL_CreateWindow(TITLE, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                    g_frontPanelWidth, g_frontPanelHeight, SDL_WINDOW_HIDDEN);

    if (g_mainWindow == NULL) {
        printf("Window could not be created! SDL Error: %s\n", SDL_GetError());
        return false;
    }

    // Create renderer
    g_renderer =
        SDL_CreateRenderer(g_mainWindow, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (g_renderer == NULL) {
        printf("Renderer could not be created! SDL Error: %s\n", SDL_GetError());
        return false;
    }

    SDL_SetRenderDrawBlendMode(g_renderer, SDL_BLENDMODE_BLEND);

    // Initialize PNG loading
    int imgFlags = IMG_INIT_PNG;
    if ((IMG_Init(imgFlags) & imgFlags) != imgFlags) {
        printf("SDL_image could not initialize! SDL_image Error: %s\n", IMG_GetError());
    } else {
#if !defined(__EMSCRIPTEN__)
        // Set icon
        SDL_Surface *iconSurface = IMG_Load(getFullPath("images", ICON).c_str());
        if (!iconSurface) {
            printf("Failed to load icon! SDL Error: %s\n", SDL_GetError());
        } else {
            SDL_SetWindowIcon(g_mainWindow, iconSurface);
            SDL_FreeSurface(iconSurface);
        }
#endif
    }

    SDL_ShowWindow(g_mainWindow);

    return true;
}

#if OPTION_SDRAM
void *getBufferPointer() {
    return g_frontPanelBuffer;
}

void setBufferPointer(void *buffer) {
    g_frontPanelBuffer = (uint32_t *)buffer;
}
#endif

void turnOn() {
    if (!isOn()) {
        g_frontPanelBuffer1 = new uint32_t[g_frontPanelWidth * g_frontPanelHeight];
        memset(g_frontPanelBuffer1, 0, g_frontPanelWidth * g_frontPanelHeight * sizeof(uint32_t));
        
        g_frontPanelBuffer2 = new uint32_t[g_frontPanelWidth * g_frontPanelHeight];
        memset(g_frontPanelBuffer2, 0, g_frontPanelWidth * g_frontPanelHeight * sizeof(uint32_t));

        g_frontPanelBuffer3 = new uint32_t[g_frontPanelWidth * g_frontPanelHeight];
        memset(g_frontPanelBuffer3, 0, g_frontPanelWidth * g_frontPanelHeight * sizeof(uint32_t));

        g_frontPanelBuffer = g_frontPanelBuffer1;

        for (int bufferIndex = 0; bufferIndex < NUM_BUFFERS; bufferIndex++) {
            g_buffers[bufferIndex].bufferPointer = new uint32_t[g_frontPanelWidth * g_frontPanelHeight];
            memset(g_buffers[bufferIndex].bufferPointer, 0, g_frontPanelWidth * g_frontPanelHeight * sizeof(uint32_t));
        }

        refreshScreen();
    }
}

void updateScreen(uint32_t *buffer);

void turnOff() {
    if (isOn()) {
        // clear screen
        setColor(0, 0, 0);
        fillRect(g_homeAppContext.x, g_homeAppContext.y, 
            g_homeAppContext.x + g_homeAppContext.width - 1, g_homeAppContext.y + g_homeAppContext.height - 1);
        updateScreen(g_frontPanelBuffer);

        // free frame buffers
        delete g_frontPanelBuffer1;
        delete g_frontPanelBuffer2;
        delete g_frontPanelBuffer3;

        g_frontPanelBuffer = nullptr;

        for (int bufferIndex = 0; bufferIndex < NUM_BUFFERS; bufferIndex++) {
            delete (uint32_t *)g_buffers[bufferIndex].bufferPointer;
        }
    }
}

bool isOn() {
    return g_frontPanelBuffer != nullptr;
}

void updateBrightness() {
}

void updateScreen(uint32_t *buffer) {
    SDL_Surface *rgbSurface = SDL_CreateRGBSurfaceFrom(
        buffer, g_frontPanelWidth, g_frontPanelHeight, 32, 4 * g_frontPanelWidth, 0, 0, 0, 0);
    if (rgbSurface != NULL) {
        SDL_Texture *texture = SDL_CreateTextureFromSurface(g_renderer, rgbSurface);
        if (texture != NULL) {
            SDL_Rect srcRect = { 0, 0, g_frontPanelWidth, g_frontPanelHeight };
            SDL_Rect dstRect = { 0, 0, g_frontPanelWidth, g_frontPanelHeight };

            SDL_RenderCopyEx(g_renderer, texture, &srcRect, &dstRect, 0.0, NULL, SDL_FLIP_NONE);

            SDL_DestroyTexture(texture);
        } else {
            printf("Unable to create texture from image buffer! SDL Error: %s\n", SDL_GetError());
        }
        SDL_FreeSurface(rgbSurface);
    } else {
        printf("Unable to render text surface! SDL Error: %s\n", SDL_GetError());
    }
    SDL_RenderPresent(g_renderer);
}

void animate() {
    uint32_t *bufferOld;
    uint32_t *bufferNew;

    if (g_frontPanelBuffer == g_frontPanelBuffer1) {
        bufferOld = g_frontPanelBuffer2;
        bufferNew = g_frontPanelBuffer1;
    } else {
        bufferOld = g_frontPanelBuffer1;
        bufferNew = g_frontPanelBuffer2;
    }

    float t = (millis() - g_animationState.startTime) / (1000.0f * g_animationState.duration);
    if (t < 1.0f) {
        g_animationState.callback(t, bufferOld, bufferNew, g_frontPanelBuffer3);
        updateScreen(g_frontPanelBuffer3);
    } else {
        g_animationState.enabled = false;
    }
}

void sync() {
    if (!isOn()) {
        return;
    }

    if (g_mainWindow == nullptr) {
        init();
    }

    if (g_animationState.enabled) {
        animate();
        if (!g_animationState.enabled) {
            finishAnimation();
        }
        g_painted = false;
        return;
    }

    if (g_painted) {
        updateScreen(g_frontPanelBuffer);

        if (g_frontPanelBuffer == g_frontPanelBuffer1) {
            g_frontPanelBuffer = g_frontPanelBuffer2;
        } else {
            g_frontPanelBuffer = g_frontPanelBuffer1;
        }
    }
}

void finishAnimation() {
    updateScreen(g_frontPanelBuffer);

    if (g_frontPanelBuffer == g_frontPanelBuffer1) {
        g_frontPanelBuffer = g_frontPanelBuffer2;
    } else {
        g_frontPanelBuffer = g_frontPanelBuffer1;
    }
}

////////////////////////////////////////////////////////////////////////////////

int getDisplayWidth() {
    return g_frontPanelWidth;
}

int getDisplayHeight() {
    return g_frontPanelHeight;
}

////////////////////////////////////////////////////////////////////////////////

void screanshotBegin() {
    g_screenshotBuffer = new uint8_t[480 * 272 * 3];

    uint8_t *src = (uint8_t *)(g_frontPanelBuffer + home::g_homeAppContext.y * g_frontPanelWidth + home::g_homeAppContext.x);
    uint8_t *dst = g_screenshotBuffer;

    int srcAdvance = (g_frontPanelWidth - 480) * 4;

    for (int y = 0; y < 272; y++) {
        for (int x = 0; x < 480; x++) {
            *dst++ = *src++;
            *dst++ = *src++;
            *dst++ = *src++;
            src++;
        }
        src += srcAdvance;
    }

    g_screenshotY = 272 - 1;
}

bool screanshotGetLine(uint8_t *line) {
    if (g_screenshotY < 0) {
        return false;
    }
    memcpy(line, g_screenshotBuffer + g_screenshotY * 480 * 3, 480 * 3);
    --g_screenshotY;
    return true;
}

void screanshotEnd() {
    delete g_screenshotBuffer;
    g_screenshotBuffer = nullptr;
}

////////////////////////////////////////////////////////////////////////////////

static void doDrawGlyph(const gui::font::Glyph &glyph, int x_glyph, int y_glyph, int width, int height, int offset, int iStartByte) {
    uint32_t pixel;
    ((uint8_t *)&pixel)[0] = COLOR_TO_B(g_fc);
    ((uint8_t *)&pixel)[1] = COLOR_TO_G(g_fc);
    ((uint8_t *)&pixel)[2] = COLOR_TO_R(g_fc);
    uint8_t *pixelAlpha = ((uint8_t *)&pixel) + 3;

    const uint8_t *src = glyph.data + offset + iStartByte;
    int nlSrc = glyph.width - width;

    uint32_t *dst = g_frontPanelBuffer + y_glyph * g_frontPanelWidth + x_glyph;
    int nlDst = g_frontPanelWidth - width;

    for (const uint8_t *srcEnd = src + height * glyph.width; src != srcEnd; src += nlSrc, dst += nlDst) {
        for (uint32_t *dstEnd = dst + width; dst != dstEnd; src++, dst++) {
            *pixelAlpha = *src;
            *dst = blendColor(pixel, *dst);
        }
    }
}

static int8_t drawGlyph(int x1, int y1, int clip_x1, int clip_y1, int clip_x2, int clip_y2,
                        uint8_t encoding) {
    gui::font::Glyph glyph;
    g_font.getGlyph(encoding, glyph);
    if (!glyph)
        return 0;

    int x_glyph = x1 + glyph.x;
    int y_glyph = y1 + g_font.getAscent() - (glyph.y + glyph.height);

    // draw glyph pixels
    int iStartByte = 0;
    if (x_glyph < clip_x1) {
        int dx_off = clip_x1 - x_glyph;
        iStartByte = dx_off;
        x_glyph = clip_x1;
    }

    int offset = gui::font::GLYPH_HEADER_SIZE;
    if (y_glyph < clip_y1) {
        int dy_off = clip_y1 - y_glyph;
        offset += dy_off * glyph.width;
        y_glyph = clip_y1;
    }

    int width;
    if (x_glyph + glyph.width - 1 > clip_x2) {
        width = clip_x2 - x_glyph + 1;
        // if glyph doesn't fit, don't paint it
        //return glyph.dx;
    } else {
        width = glyph.width;
    }

    int height;
    if (y_glyph + glyph.height - 1 > clip_y2) {
        height = clip_y2 - y_glyph + 1;
    } else {
        height = glyph.height;
    }

    if (width > 0 && height > 0) {
        doDrawGlyph(glyph, x_glyph, y_glyph, width, height, offset, iStartByte);
    }

    return glyph.dx;
}

////////////////////////////////////////////////////////////////////////////////

void drawPixel(int x, int y) {
    *(g_frontPanelBuffer + y * g_frontPanelWidth + x) = color16to32(g_fc);

    g_painted = true;
}

void drawRect(int x1, int y1, int x2, int y2) {
    if (x1 > x2) {
        std::swap<int>(x1, x2);
    }
    if (y1 > y2) {
        std::swap<int>(y1, y2);
    }

    drawHLine(x1, y1, x2 - x1);
    drawHLine(x1, y2, x2 - x1);
    drawVLine(x1, y1, y2 - y1);
    drawVLine(x2, y1, y2 - y1);

    g_painted = true;
}

void fillRect(int x1, int y1, int x2, int y2, int r) {
    if (r == 0) {
        uint32_t color32 = color16to32(g_fc);
        uint32_t *dst = g_frontPanelBuffer + y1 * g_frontPanelWidth + x1;
        int width = x2 - x1 + 1;
        int height = y2 - y1 + 1;
        int nl = g_frontPanelWidth - width;
        for (uint32_t *dstEnd = dst + height * g_frontPanelWidth; dst != dstEnd; dst += nl) {
            for (uint32_t *lineEnd = dst + width; dst != lineEnd; dst++) {
                *dst = color32;
            }
        }
    } else {
        // draw rounded rect
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

    g_painted = true;
}

void fillRect(void *dstBuffer, int x1, int y1, int x2, int y2) {
    uint32_t color32 = color16to32(g_fc);
    uint32_t *dst = (uint32_t *)dstBuffer + y1 * g_frontPanelWidth + x1;
    int nl = g_frontPanelWidth - (x2 - x1 + 1);
    for (int y = y1; y <= y2; y++) {
        for (int x = x1; x <= x2; x++) {
            *dst++ = color32;
        }
        dst += nl;
    }
}

void drawHLine(int x, int y, int l) {
    uint32_t color32 = color16to32(g_fc);

    uint32_t *dst = g_frontPanelBuffer + y * g_frontPanelWidth + x;
    uint32_t *dstEnd = dst + l + 1;
    while (dst < dstEnd) {
        *dst++ = color32;
    }

    g_painted = true;
}

void drawVLine(int x, int y, int l) {
    uint32_t color32 = color16to32(g_fc);

    uint32_t *dst = g_frontPanelBuffer + y * g_frontPanelWidth + x;
    uint32_t *dstEnd = dst + (l + 1) * g_frontPanelWidth;

    while (dst < dstEnd) {
        *dst = color32;
        dst += g_frontPanelWidth;
    }

    g_painted = true;
}

void bitBlt(int x1, int y1, int x2, int y2, int dstx, int dsty) {
    int width = x2 - x1 + 1;
    int height = y2 - y1 + 1;

    uint32_t *src = g_frontPanelBuffer + y1 * g_frontPanelWidth + x1;
    uint32_t *dst = g_frontPanelBuffer + dsty * g_frontPanelWidth + dstx;
    int nl = g_frontPanelWidth - width;

    for (int y = y1; y <= y2; y++, src += nl, dst += nl) {
        for (uint32_t *lineEnd = dst + width; dst != lineEnd; dst++, src++) {
            uint8_t *src8 = (uint8_t *)src;
            *dst = color16to32(RGB_TO_COLOR(src8[2], src8[1], src8[0]));
        }
    }

    g_painted = true;
}

void bitBlt(void *src, int x1, int y1, int x2, int y2) {
    bitBlt(src, g_frontPanelBuffer, x1, y1, x2, y2);
}

void bitBlt(void *src, void *dst, int x1, int y1, int x2, int y2) {
    for (int y = y1; y <= y2; ++y) {
        for (int x = x1; x <= x2; ++x) {
            int i = y * g_frontPanelWidth + x;
            ((uint32_t *)dst)[i] = ((uint32_t *)src)[i];
        }
    }
}

void bitBlt(void *src, void *dst, int sx, int sy, int sw, int sh, int dx, int dy, uint8_t opacity) {
    if (opacity == 255) {
        for (int y = 0; y < sh; ++y) {
            for (int x = 0; x < sw; ++x) {
                ((uint32_t *)dst)[(dy + y) * g_frontPanelWidth + dx + x] = ((uint32_t *)src)[(sy + y) * g_frontPanelWidth + sx + x];
            }
        }
    } else {
        for (int y = 0; y < sh; ++y) {
            for (int x = 0; x < sw; ++x) {
                uint8_t *p = (uint8_t *)&((uint32_t *)src)[(sy + y) * g_frontPanelWidth + sx + x];
                p[3] = opacity;
                ((uint32_t *)dst)[(dy + y) * g_frontPanelWidth + dx + x] = blendColor(
                    ((uint32_t *)src)[(sy + y) * g_frontPanelWidth + sx + x],
                    ((uint32_t *)dst)[(dy + y) * g_frontPanelWidth + dx + x]
                );
            }
        }
    }
}

void drawBitmap(void *bitmapData, int bitmapBpp, int bitmapWidth, int x, int y, int width, int height) {
    uint32_t *dst = g_frontPanelBuffer + y * g_frontPanelWidth + x;
    int nlDst = g_frontPanelWidth - width;

    if (bitmapBpp == 32) {
        uint32_t *src = (uint32_t *)bitmapData;
        int nlSrc = bitmapWidth - width;

        uint32_t pixel;
        uint8_t *pixelAlpha = ((uint8_t *)&pixel) + 3;

        for (uint32_t *srcEnd = src + bitmapWidth * height; src != srcEnd; src += nlSrc, dst += nlDst) {
            for (uint32_t *lineEnd = dst + width; dst != lineEnd; src++, dst++) {
                pixel = *src;
                *pixelAlpha = *pixelAlpha * g_opacity / 255;
                *dst = blendColor(pixel, *dst);
            }
        }
    } else {
        uint16_t *src = (uint16_t *)bitmapData;
        int nlSrc = bitmapWidth - width;

        for (uint16_t *srcEnd = src + bitmapWidth * height; src != srcEnd; src += nlSrc, dst += nlDst) {
            for (uint32_t *lineEnd = dst + width; dst != lineEnd; src++, dst++) {
                *dst = color16to32(*src);
            }
        }
    }

    g_painted = true;
}

void drawStr(const char *text, int textLength, int x, int y, int clip_x1, int clip_y1, int clip_x2,
             int clip_y2, gui::font::Font &font) {
    g_font = font;

    if (textLength == -1) {
        char encoding;
        while ((encoding = *text++) != 0) {
            x += drawGlyph(x, y, clip_x1, clip_y1, clip_x2, clip_y2, encoding);
        }
    } else {
        for (int i = 0; i < textLength && text[i]; ++i) {
            char encoding = text[i];
            x += drawGlyph(x, y, clip_x1, clip_y1, clip_x2, clip_y2, encoding);
        }
    }

    g_painted = true;
}

} // namespace display
} // namespace mcu
} // namespace eez

#endif