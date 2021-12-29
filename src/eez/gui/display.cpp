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
#include <eez/gui/thread.h>

#include <eez/gui/display-private.h>

#define CONF_BACKDROP_OPACITY 128

using namespace eez::gui;

namespace eez {
namespace gui {
namespace display {

DisplayState g_displayState;

VideoBuffer g_renderBuffer1;
VideoBuffer g_renderBuffer2;

VideoBuffer g_animationBuffer1;
VideoBuffer g_animationBuffer2;

VideoBuffer g_syncedBuffer;
VideoBuffer g_renderBuffer;
VideoBuffer g_animationBuffer;

bool g_takeScreenshot;

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

RenderBuffer g_renderBuffers[NUM_BUFFERS];
static VideoBuffer g_mainBufferPointer;
static int g_numBuffersToDraw;

bool g_screenshotAllocated;

////////////////////////////////////////////////////////////////////////////////

void init() {
    onLuminocityChanged();
    onThemeChanged();

    g_renderBuffer1 = (VideoBuffer)VRAM_BUFFER1_START_ADDRESS;
    g_renderBuffer2 = (VideoBuffer)VRAM_BUFFER2_START_ADDRESS;

    g_animationBuffer1 = (VideoBuffer)VRAM_ANIMATION_BUFFER1_START_ADDRESS;
    g_animationBuffer2 = (VideoBuffer)VRAM_ANIMATION_BUFFER2_START_ADDRESS;

    g_renderBuffers[0].bufferPointer = (VideoBuffer)(VRAM_AUX_BUFFER1_START_ADDRESS);
	g_renderBuffers[1].bufferPointer = (VideoBuffer)(VRAM_AUX_BUFFER2_START_ADDRESS);
	g_renderBuffers[2].bufferPointer = (VideoBuffer)(VRAM_AUX_BUFFER3_START_ADDRESS);
	g_renderBuffers[3].bufferPointer = (VideoBuffer)(VRAM_AUX_BUFFER4_START_ADDRESS);
	g_renderBuffers[4].bufferPointer = (VideoBuffer)(VRAM_AUX_BUFFER5_START_ADDRESS);
	g_renderBuffers[5].bufferPointer = (VideoBuffer)(VRAM_AUX_BUFFER6_START_ADDRESS);

    initDriver();

    // start with the black screen
    setColor(0, 0, 0);
    g_renderBuffer = g_renderBuffer1;
    fillRect(0, 0, getDisplayWidth() - 1, getDisplayHeight() - 1);
    g_renderBuffer = g_renderBuffer2;
    fillRect(0, 0, getDisplayWidth() - 1, getDisplayHeight() - 1);

    g_animationBuffer = g_animationBuffer1;
    
    g_syncedBuffer = g_renderBuffer1;
    syncBuffer();
}

void turnOn() {
    if (g_displayState != ON && g_displayState != TURNING_ON) {
        turnOnStartHook();
    }
}

bool isOn() {
    return g_displayState == ON || g_displayState == TURNING_ON;
}

void turnOff() {
    if (g_displayState != OFF && g_displayState != TURNING_OFF) {
        turnOffStartHook();
    }
}

////////////////////////////////////////////////////////////////////////////////

#ifdef EEZ_CONF_GUI_CALC_FPS
bool g_calcFpsEnabled;
#if defined(EEZ_CONF_STYLE_ID_FPS_GRAPH)
bool g_drawFpsGraphEnabled;
#endif
uint32_t g_fpsValues[NUM_FPS_VALUES];
uint32_t g_fpsAvg;
static uint32_t g_fpsTotal;
static uint32_t g_lastTimeFPS;

void calcFPS() {
    // calculate last FPS value
	g_fpsTotal -= g_fpsValues[0];

	for (size_t i = 1; i < NUM_FPS_VALUES; i++) {
		g_fpsValues[i - 1] = g_fpsValues[i];
	}

	uint32_t time = millis();
	auto diff = time - g_lastTimeFPS;

	auto fps = 1000 / diff;
    if (fps > 60) {
        fps = 60;
    }
    g_fpsValues[NUM_FPS_VALUES - 1] = fps;

	g_fpsTotal += g_fpsValues[NUM_FPS_VALUES - 1];
	g_fpsAvg = g_fpsTotal / NUM_FPS_VALUES;
}

void drawFpsGraph(int x, int y, int w, int h, const Style *style) {
	int x1 = x;
	int y1 = y;
	int x2 = x + w - 1;
	int y2 = y + h - 1;
	drawBorderAndBackground(x1, y1, x2, y2, style, style->backgroundColor);

	x1++;
	y1++;
	x2--;
	y2--;

	bool isRed = false;
	display::setColor(style->color);

	x = x1;
	for (size_t i = 0; i < NUM_FPS_VALUES && x <= x2; i++, x++) {
		int y = y2 - g_fpsValues[i] * (y2 - y1) / 60;
		if (y < y1) {
			y = y1;
		}

		if (g_fpsValues[i] < 40) {
			if (!isRed) {
				display::setColor16(COLOR_RED);
				isRed = true;
			}
		} else {
			if (isRed) {
				display::setColor(style->color);
				isRed = false;
			}
		}

		display::drawVLine(x, y, y2 - y);
	}
}
#endif

////////////////////////////////////////////////////////////////////////////////

static void finishAnimation() {
    g_animationState.enabled = false;

    if (g_renderBuffer == g_renderBuffer1) {
        g_renderBuffer = g_renderBuffer2;
        bitBlt(g_renderBuffer1, 0, 0, getDisplayWidth() - 1, getDisplayHeight() - 1);
    } else {
        g_renderBuffer = g_renderBuffer1;
        bitBlt(g_renderBuffer2, 0, 0, getDisplayWidth() - 1, getDisplayHeight() - 1);
    }

    g_syncedBuffer = g_renderBuffer1;
    syncBuffer();
}

void animate(Buffer startBuffer, void (*callback)(float t, VideoBuffer bufferOld, VideoBuffer bufferNew, VideoBuffer bufferDst), float duration) {
    if (g_animationState.enabled) {
        display::finishAnimation();
    }

    g_animationState.enabled = true;
    g_animationState.startTime = 0;
    g_animationState.duration = duration != -1 ? duration : getDefaultAnimationDurationHook();
    g_animationState.startBuffer = startBuffer;
    g_animationState.callback = callback;
    g_animationState.easingRects = remapOutQuad;
    g_animationState.easingOpacity = remapOutCubic;
}

static void animateStep() {
    uint32_t time = millis();
    if (time == 0) {
        time = 1;
    }
    if (g_animationState.startTime == 0) {
        g_animationState.startTime = time;
    }
    float t = (time - g_animationState.startTime) / (1000.0f * g_animationState.duration);
    if (t < 1.0f) {
		if (g_syncedBuffer == g_animationBuffer1) {
			g_animationBuffer = g_animationBuffer2;
		} else {
			g_animationBuffer = g_animationBuffer1;
		}

        if (g_renderBuffer == g_renderBuffer1) {
            g_animationState.callback(t, g_renderBuffer2, g_renderBuffer1, g_animationBuffer);
        } else {
            g_animationState.callback(t, g_renderBuffer1, g_renderBuffer2, g_animationBuffer);
        }

        g_syncedBuffer = g_animationBuffer;
        syncBuffer();
    } else {
    	finishAnimation();
    }
}

void update() {
    if (g_displayState == TURNING_ON) {
        turnOnTickHook();
    } else if (g_displayState == TURNING_OFF) {
        turnOffTickHook();
    } else if (g_displayState == OFF) {
        if (g_animationState.enabled) {
            display::finishAnimation();
        }
        osDelay(16);
        sendMessageToGuiThread(GUI_QUEUE_MESSAGE_TYPE_DISPLAY_VSYNC, 0, 0);
        return;
    }

	g_lastTimeFPS = millis();

    display::beginRendering();
    updateScreen();
    display::endRendering();

#ifdef EEZ_CONF_GUI_CALC_FPS
    if (g_calcFpsEnabled) {
        calcFPS();
    }
#endif

    if (!g_screenshotAllocated && g_animationState.enabled) {
        animateStep();
    } else {
        g_syncedBuffer = g_renderBuffer;
		syncBuffer();

        if (g_takeScreenshot) {
            copySyncedBufferToScreenshotBuffer();
            
            g_takeScreenshot = false;
            g_screenshotAllocated = true;
        }
    }
}

const uint8_t *takeScreenshot() {
    while (g_screenshotAllocated) {
    }

	g_takeScreenshot = true;

#ifdef __EMSCRIPTEN__
    doTakeScreenshot();
#endif

	do {
		osDelay(0);
	} while (g_takeScreenshot);

    return SCREENSHOOT_BUFFER_START_ADDRESS;
}

void releaseScreenshot() {
    g_screenshotAllocated = false;
}

////////////////////////////////////////////////////////////////////////////////

VideoBuffer getBufferPointer() {
    return g_renderBuffer;
}

void setBufferPointer(VideoBuffer buffer) {
    g_renderBuffer = buffer;
}

void beginRendering() {
    if (g_syncedBuffer == g_renderBuffer1) {
        g_renderBuffer = g_renderBuffer2;
    } else if (g_syncedBuffer == g_renderBuffer2) {
        g_renderBuffer = g_renderBuffer1;
    }

    clearDirty();

    g_mainBufferPointer = getBufferPointer();
    g_numBuffersToDraw = 0;
}

int beginBufferRendering() {
    int bufferIndex = g_numBuffersToDraw++;
	g_renderBuffers[bufferIndex].previousBuffer = getBufferPointer();
    setBufferPointer(g_renderBuffers[bufferIndex].bufferPointer);
    return bufferIndex;
}

void endBufferRendering(int bufferIndex, int x, int y, int width, int height, bool withShadow, uint8_t opacity, int xOffset, int yOffset, Rect *backdrop) {
    RenderBuffer &renderBuffer = g_renderBuffers[bufferIndex];
    
	renderBuffer.x = x;
	renderBuffer.y = y;
	renderBuffer.width = width;
	renderBuffer.height = height;
	renderBuffer.withShadow = withShadow;
	renderBuffer.opacity = opacity;
	renderBuffer.xOffset = xOffset;
	renderBuffer.yOffset = yOffset;
	renderBuffer.backdrop = backdrop;

    setBufferPointer(renderBuffer.previousBuffer);
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

#if defined(EEZ_CONF_GUI_CALC_FPS) && defined(EEZ_CONF_STYLE_ID_FPS_GRAPH)
    if (g_drawFpsGraphEnabled) {
	    setDirty();
    }
#endif

    if (isDirty()) {
        for (int bufferIndex = 0; bufferIndex < g_numBuffersToDraw; bufferIndex++) {
            RenderBuffer &renderBuffer = g_renderBuffers[bufferIndex];

            int sx = renderBuffer.x;
            int sy = renderBuffer.y;

            int x1 = renderBuffer.x + renderBuffer.xOffset;
            int y1 = renderBuffer.y + renderBuffer.yOffset;
            int x2 = x1 + renderBuffer.width - 1;
            int y2 = y1 + renderBuffer.height - 1;

            if (renderBuffer.backdrop) {
                // opacity backdrop
                auto savedOpacity = setOpacity(CONF_BACKDROP_OPACITY);
                setColor(EEZ_CONF_COLOR_ID_BACKDROP);
                fillRect(renderBuffer.backdrop->x, renderBuffer.backdrop->y, renderBuffer.backdrop->x + renderBuffer.backdrop->w - 1, renderBuffer.backdrop->y + renderBuffer.backdrop->h - 1);
                setOpacity(savedOpacity);
            }

            if (renderBuffer.withShadow) {
                drawShadow(x1, y1, x2, y2);
            }

            bitBlt(g_renderBuffers[bufferIndex].bufferPointer, nullptr, sx, sy, x2 - x1 + 1, y2 - y1 + 1, x1, y1, renderBuffer.opacity);
        }

#if defined(EEZ_CONF_GUI_CALC_FPS) && defined(EEZ_CONF_STYLE_ID_FPS_GRAPH)
        if (g_drawFpsGraphEnabled) {
            drawFpsGraph(getDisplayWidth() - 64 - 4, 4, 64, 32, getStyle(EEZ_CONF_STYLE_ID_FPS_GRAPH));
        }
#endif

#if OPTION_KEYBOARD
        keyboard::updateDisplay();
#endif

#if OPTION_MOUSE
        mouse::updateDisplay();
#endif
    } else {
        if (g_syncedBuffer == g_renderBuffer1) {
            bitBlt(g_renderBuffer1, 0, 0, getDisplayWidth() - 1, getDisplayHeight() - 1);
        } else if (g_syncedBuffer == g_renderBuffer2) {
            bitBlt(g_renderBuffer2, 0, 0, getDisplayWidth() - 1, getDisplayHeight() - 1);
        }        
    }
}

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
    auto selectedThemeIndex = getSelectedThemeIndexHook();
    g_themeColors = getThemeColors(selectedThemeIndex);
    g_themeColorsCount = getThemeColorsCount(selectedThemeIndex);
    g_colors = getColors();
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
    if (!ignoreLuminocity) {
        adjustColor(g_fc);
    }
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
    if (!ignoreLuminocity) {
	    adjustColor(g_bc);
    }
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

////////////////////////////////////////////////////////////////////////////////////////////////

void aggInit(AggDrawing& aggDrawing) {
	aggDrawing.rbuf.attach((uint8_t *)getBufferPointer(), getDisplayWidth(), getDisplayHeight(), getDisplayWidth() * DISPLAY_BPP / 8);
	aggDrawing.graphics.attach(aggDrawing.rbuf.buf(), aggDrawing.rbuf.width(), aggDrawing.rbuf.height(), aggDrawing.rbuf.stride());
}

void drawRoundedRect(
    AggDrawing& aggDrawing,
    double x1, double y1, double x2, double y2,
    double lineWidth,
	double rtlx, double rtly, double rtrx, double rtry,
	double rbrx, double rbry, double rblx, double rbly
) {
    fillRoundedRect(
        aggDrawing,
        x1, y1, x2, y2,
        lineWidth,
        rtlx, rtly, rtrx, rtry,
	    rbrx, rbry, rblx, rbly,
        true, false
    );
}

void fillRoundedRect(
    AggDrawing& aggDrawing,
	double x1, double y1, double x2, double y2,
	double lineWidth,
	double rtlx, double rtly, double rtrx, double rtry,
	double rbrx, double rbry, double rblx, double rbly, 
	bool drawLine, bool fill,
	double clip_x1, double clip_y1, double clip_x2, double clip_y2
) {
    auto &graphics = aggDrawing.graphics;

	if (clip_x1 != -1) {
		graphics.clipBox(clip_x1, clip_y1, clip_x2 + 1, clip_y2 + 1);
	} else {
		graphics.clipBox(x1, y1, x2 + 1, y2 + 1);
	}
    graphics.masterAlpha(g_opacity / 255.0);
	graphics.translate(x1, y1);
    graphics.lineWidth(lineWidth);
    if (lineWidth > 0 && drawLine) {
	    graphics.lineColor(COLOR_TO_R(g_fc), COLOR_TO_G(g_fc), COLOR_TO_B(g_fc));
    } else {
        graphics.noLine();
    }	
    if (fill) {
        graphics.fillColor(COLOR_TO_R(g_bc), COLOR_TO_G(g_bc), COLOR_TO_B(g_bc));
    } else {
        graphics.noFill();
    }
	auto w = x2 - x1 + 1;
	auto h = y2 - y1 + 1;
	graphics.roundedRect(
		lineWidth / 2.0, lineWidth / 2.0, w - lineWidth, h - lineWidth,
		rtlx, rtly, rtrx, rtry, rbrx, rbry, rblx, rbly
	);
    
	graphics.translate(-x1, -y1);
	graphics.clipBox(0, 0, aggDrawing.rbuf.width(), aggDrawing.rbuf.height());
}

void fillRoundedRect(
    AggDrawing& aggDrawing,
	double x1, double y1, double x2, double y2,
	double lineWidth,
	double r, 
	bool drawLine, bool fill,
	double clip_x1, double clip_y1, double clip_x2, double clip_y2
) {
	fillRoundedRect(aggDrawing, x1, y1, x2, y2, lineWidth, r, r, r, r, r, r, r, r, drawLine, fill, clip_x1, clip_y1, clip_x2, clip_y2);
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

    if (cursorPosition != -1 && xCursor - CURSOR_WIDTH / 2 >= clip_x1 && xCursor + CURSOR_WIDTH / 2 - 1 <= clip_x2) {
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

} // namespace display
} // namespace gui
} // namespace eez
