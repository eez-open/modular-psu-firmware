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

#include <math.h>
#include <memory.h>
#include <stdio.h>
#include <string.h>
#include <string>

#if !defined(__EMSCRIPTEN__)
#include <SDL.h>
#include <SDL_image.h>
#endif

#include <eez/conf.h>
#include <eez/core/debug.h>
#include <eez/core/memory.h>
#include <eez/core/util.h>
#include <eez/core/os.h>

#include <eez/gui/gui.h>
#include <eez/gui/thread.h>
#include <eez/gui/touch.h>

#if OPTION_MOUSE
#include <eez/core/mouse.h>
#endif

#include <eez/gui/display-private.h>

namespace eez {
namespace gui {
namespace display {

////////////////////////////////////////////////////////////////////////////////

#if !defined(__EMSCRIPTEN__)
static SDL_Window *g_mainWindow;
static SDL_Renderer *g_renderer;
#endif

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

void initDriver() {
#if !defined(__EMSCRIPTEN__)
    // Set texture filtering to linear
    if (!SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1")) {
        printf("Warning: Linear texture filtering not enabled!");
    }

    // Create window
    g_mainWindow = SDL_CreateWindow(TITLE, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, DISPLAY_WIDTH, DISPLAY_HEIGHT, SDL_WINDOW_HIDDEN);

    if (g_mainWindow == NULL) {
        printf("Window could not be created! SDL Error: %s\n", SDL_GetError());
        return;
    }

    // Create renderer
    g_renderer = SDL_CreateRenderer(g_mainWindow, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (g_renderer == NULL) {
		g_mainWindow = NULL;
        printf("Renderer could not be created! SDL Error: %s\n", SDL_GetError());
        return;
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

#if OPTION_MOUSE
    if (mouse::isMouseEnabled()) {
        SDL_ShowCursor(SDL_DISABLE);
        SDL_CaptureMouse(SDL_TRUE);
    }
#endif
#endif
}

void syncBuffer() {
#if !defined(__EMSCRIPTEN__)
    if (!g_mainWindow) {
		return;
    }

    SDL_Surface *rgbSurface = SDL_CreateRGBSurfaceFrom(
        (uint32_t *)g_syncedBuffer, DISPLAY_WIDTH, DISPLAY_HEIGHT, 32, 4 * DISPLAY_WIDTH, 0x000000FF, 0x0000FF00, 0x00FF0000, 0xFF000000);
    if (rgbSurface != NULL) {
        SDL_Texture *texture = SDL_CreateTextureFromSurface(g_renderer, rgbSurface);
        if (texture != NULL) {
            SDL_Rect srcRect = { 0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT };
            SDL_Rect dstRect = { 0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT };

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

    sendMessageToGuiThread(GUI_QUEUE_MESSAGE_TYPE_DISPLAY_VSYNC, 0, 0);
#endif

    touch::tick();
}

void copySyncedBufferToScreenshotBuffer() {
    auto appContext = getAppContextFromId(APP_CONTEXT_ID_DEVICE);
    uint8_t *src = (uint8_t *)(g_renderBuffer + appContext->rect.y * DISPLAY_WIDTH + appContext->rect.x);
    uint8_t *dst = SCREENSHOOT_BUFFER_START_ADDRESS;

    int srcAdvance = (DISPLAY_WIDTH - 480) * 4;

    for (int y = 0; y < 272; y++) {
        for (int x = 0; x < 480; x++) {
            uint8_t r = *src++;
            uint8_t g = *src++;
            uint8_t b = *src++;
            src++;

            *dst++ = r;
            *dst++ = g;
            *dst++ = b;
        }
        src += srcAdvance;
    }
}

////////////////////////////////////////////////////////////////////////////////

void startPixelsDraw() {
}

void drawPixel(int x, int y) {
    *(g_renderBuffer + y * DISPLAY_WIDTH + x) = color16to32(g_fc);
}

void drawPixel(int x, int y, uint8_t opacity) {
    auto dest = g_renderBuffer + y * DISPLAY_WIDTH + x;
    auto destUint8 = (uint8_t *)dest;
    *dest = blendColor(
        color16to32(g_fc, opacity),
        color16to32(RGB_TO_COLOR(destUint8[0], destUint8[1], destUint8[2]), 255 - opacity));
}

void endPixelsDraw() {
    setDirty();
}

void fillRect(int x1, int y1, int x2, int y2) {
    uint32_t color32 = color16to32(g_fc, g_opacity);

	uint32_t *dst = g_renderBuffer + y1 * DISPLAY_WIDTH + x1;
    int width = x2 - x1 + 1;
    int height = y2 - y1 + 1;
    int nl = DISPLAY_WIDTH - width;
    if (g_opacity == 255) {
        for (uint32_t *dstEnd = dst + height * DISPLAY_WIDTH; dst != dstEnd; dst += nl) {
            for (uint32_t *lineEnd = dst + width; dst != lineEnd; dst++) {
                *dst = color32;
            }
        }
    } else {
        for (uint32_t *dstEnd = dst + height * DISPLAY_WIDTH; dst != dstEnd; dst += nl) {
            for (uint32_t *lineEnd = dst + width; dst != lineEnd; dst++) {
                *dst = blendColor(color32, *dst);
            }
        }
    }

    setDirty();
}

void fillRect(void *dstBuffer, int x1, int y1, int x2, int y2) {
    uint32_t color32 = color16to32(g_fc);
    uint32_t *dst = (uint32_t *)dstBuffer + y1 * DISPLAY_WIDTH + x1;
    int nl = DISPLAY_WIDTH - (x2 - x1 + 1);
    for (int y = y1; y <= y2; y++) {
        for (int x = x1; x <= x2; x++) {
            *dst++ = color32;
        }
        dst += nl;
    }

    setDirty();
}

void bitBlt(int x1, int y1, int x2, int y2, int dstx, int dsty) {
    int width = x2 - x1 + 1;

    uint32_t *src = g_renderBuffer + y1 * DISPLAY_WIDTH + x1;
    uint32_t *dst = g_renderBuffer + dsty * DISPLAY_WIDTH + dstx;
    int nl = DISPLAY_WIDTH - width;

    for (int y = y1; y <= y2; y++, src += nl, dst += nl) {
        for (uint32_t *lineEnd = dst + width; dst != lineEnd; dst++, src++) {
            uint8_t *src8 = (uint8_t *)src;
            *dst = color16to32(RGB_TO_COLOR(src8[0], src8[1], src8[2]));
        }
    }

    setDirty();
}

void bitBlt(void *src, int x1, int y1, int x2, int y2) {
    bitBlt(src, g_renderBuffer, x1, y1, x2, y2);
    setDirty();
}

void bitBlt(void *src, void *dst, int x1, int y1, int x2, int y2) {
    for (int y = y1; y <= y2; ++y) {
        for (int x = x1; x <= x2; ++x) {
            int i = y * DISPLAY_WIDTH + x;
            ((uint32_t *)dst)[i] = ((uint32_t *)src)[i];
        }
    }

    setDirty();
}

void bitBlt(void *src, void *dst, int sx, int sy, int sw, int sh, int dx, int dy, uint8_t opacity) {
    if (dst == nullptr) {
        dst = g_renderBuffer;
    }

    if (opacity == 255) {
        for (int y = 0; y < sh; ++y) {
            for (int x = 0; x < sw; ++x) {
                ((uint32_t *)dst)[(dy + y) * DISPLAY_WIDTH + dx + x] = ((uint32_t *)src)[(sy + y) * DISPLAY_WIDTH + sx + x];
            }
        }
    } else {
        for (int y = 0; y < sh; ++y) {
            for (int x = 0; x < sw; ++x) {
                uint8_t *p = (uint8_t *)&((uint32_t *)src)[(sy + y) * DISPLAY_WIDTH + sx + x];
                p[3] = opacity;
                ((uint32_t *)dst)[(dy + y) * DISPLAY_WIDTH + dx + x] = blendColor(
                    ((uint32_t *)src)[(sy + y) * DISPLAY_WIDTH + sx + x],
                    ((uint32_t *)dst)[(dy + y) * DISPLAY_WIDTH + dx + x]
                );
            }
        }
    }
}

void drawBitmap(Image *image, int x, int y) {
    uint32_t *dst = g_renderBuffer + y * DISPLAY_WIDTH + x;
    int nlDst = DISPLAY_WIDTH - image->width;

    if (image->bpp == 32) {
        uint32_t *src = (uint32_t *)image->pixels;
        int nlSrc = image->lineOffset;

        uint32_t pixel;
        uint8_t *pixelAlpha = ((uint8_t *)&pixel) + 3;

        for (uint32_t *srcEnd = src + (image->width + nlSrc) * image->height; src != srcEnd; src += nlSrc, dst += nlDst) {
            for (uint32_t *lineEnd = dst + image->width; dst != lineEnd; src++, dst++) {
                pixel = *src;
                *pixelAlpha = *pixelAlpha * g_opacity / 255;
                *dst = blendColor(pixel, *dst);
            }
        }
    } else if (image->bpp == 24) {
        uint8_t *src = (uint8_t *)image->pixels;
        int nlSrc = 3 * image->lineOffset;

        for (uint8_t *srcEnd = src + 3 * (image->width + nlSrc) * image->height; src != srcEnd; src += nlSrc, dst += nlDst) {
            for (uint32_t *lineEnd = dst + image->width; dst != lineEnd; src += 3, dst++) {
                ((uint8_t *)dst)[0] = ((uint8_t *)src)[0];
                ((uint8_t *)dst)[1] = ((uint8_t *)src)[1];
                ((uint8_t *)dst)[2] = ((uint8_t *)src)[2];
                ((uint8_t *)dst)[3] = 255;
            }
        }
    } else {
        uint16_t *src = (uint16_t *)image->pixels;
        int nlSrc = image->lineOffset;

        for (uint16_t *srcEnd = src + (image->width + nlSrc) * image->height; src != srcEnd; src += nlSrc, dst += nlDst) {
            for (uint32_t *lineEnd = dst + image->width; dst != lineEnd; src++, dst++) {
                *dst = color16to32(*src);
            }
        }
    }

    setDirty();
}

void drawStrInit() {
}

void drawGlyph(const uint8_t *src, uint32_t srcLineOffset, int x_glyph, int y_glyph, int width, int height) {
    // glyph->pixels + offset + iStartByte, glyph->width - width, x_glyph, y_glyph, width,height
    // const gui::GlyphData &glyph, int x_glyph, int y_glyph, int width, int height, int offset, int iStartByte

    uint32_t pixel;
    ((uint8_t *)&pixel)[0] = COLOR_TO_R(g_fc);
    ((uint8_t *)&pixel)[1] = COLOR_TO_G(g_fc);
    ((uint8_t *)&pixel)[2] = COLOR_TO_B(g_fc);
    uint8_t *pixelAlpha = ((uint8_t *)&pixel) + 3;

    uint32_t *dst = g_renderBuffer + y_glyph * DISPLAY_WIDTH + x_glyph;
    int nlDst = DISPLAY_WIDTH - width;

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            *pixelAlpha = *src * g_opacity / 255;
            *dst = blendColor(pixel, *dst);
            src++;
            dst++;
        }
        src += srcLineOffset;
        dst += nlDst;
    }
}

} // namespace display
} // namespace gui
} // namespace eez

#if defined(__EMSCRIPTEN__)

EM_PORT_API(uint8_t*) getSyncedBuffer() {
    using namespace eez::gui;
    sendMessageToGuiThread(GUI_QUEUE_MESSAGE_TYPE_DISPLAY_VSYNC, 0, 0);
	return (uint8_t*)display::g_syncedBuffer;
}

#endif
