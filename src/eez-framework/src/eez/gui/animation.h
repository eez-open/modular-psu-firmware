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

namespace eez {
namespace gui {

enum Buffer {
    BUFFER_OLD,
    BUFFER_NEW,
    BUFFER_SOLID_COLOR
};

enum Opacity {
    OPACITY_SOLID,
    OPACITY_FADE_IN,
    OPACITY_FADE_OUT
};

enum Position {
    POSITION_TOP_LEFT,
    POSITION_TOP,
    POSITION_TOP_RIGHT,
    POSITION_LEFT,
    POSITION_CENTER,
    POSITION_RIGHT,
    POSITION_BOTTOM_LEFT,
    POSITION_BOTTOM,
    POSITION_BOTTOM_RIGHT
};

struct AnimationState {
    bool enabled;
    uint32_t startTime;
    float duration;
    Buffer startBuffer;
    void (*callback)(float t, VideoBuffer bufferOld, VideoBuffer bufferNew, VideoBuffer bufferDst);
    float (*easingRects)(float x, float x1, float y1, float x2, float y2);
    float (*easingOpacity)(float x, float x1, float y1, float x2, float y2);
};

struct AnimRect {
    Buffer buffer;
    Rect srcRect;
    Rect dstRect;
    uint16_t color;
    Opacity opacity;
    Position position;
};

extern AnimationState g_animationState;

#define MAX_ANIM_RECTS 10

extern AnimRect g_animRects[MAX_ANIM_RECTS];

void animateOpen(const Rect &srcRect, const Rect &dstRect);
void animateClose(const Rect &srcRect, const Rect &dstRect);
void animateRects(AppContext *appContext, Buffer startBuffer, int numRects, float duration = -1, const Rect *clipRect = nullptr);

} // gui
} // eez