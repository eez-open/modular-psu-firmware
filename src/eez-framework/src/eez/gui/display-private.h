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

namespace eez {
namespace gui {
namespace display {

void initDriver();

#if defined(EEZ_PLATFORM_STM32)
typedef uint16_t *VideoBuffer;
#endif
#if defined(EEZ_PLATFORM_SIMULATOR) || defined(__EMSCRIPTEN__)
typedef uint32_t *VideoBuffer;
#endif

enum DisplayState {
    OFF,
    TURNING_ON,
    ON,
    TURNING_OFF
};

extern DisplayState g_displayState;

extern VideoBuffer g_syncedBuffer;
void syncBuffer();

void copySyncedBufferToScreenshotBuffer();

extern uint16_t g_fc, g_bc;
extern uint8_t g_opacity;

extern gui::font::Font g_font;

void drawStrInit();
void drawGlyph(const uint8_t *src, uint32_t srcLineOffset, int x, int y, int width, int height);

static const int NUM_BUFFERS = 6;

struct RenderBuffer {
    VideoBuffer bufferPointer;
    VideoBuffer previousBuffer;
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
extern RenderBuffer g_renderBuffers[NUM_BUFFERS];

void setBufferPointer(VideoBuffer buffer);

extern bool g_dirty;
inline void clearDirty() { g_dirty = false; }
inline void setDirty() { g_dirty = true; }
inline bool isDirty() { return g_dirty; }

extern bool g_screenshotAllocated;

#ifdef GUI_CALC_FPS
static const size_t NUM_FPS_VALUES = 60;
extern uint32_t g_fpsValues[NUM_FPS_VALUES];
void calcFPS();
#endif

} // namespace display
} // namespace gui
} // namespace eez
