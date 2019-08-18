/* / system.h
 * EEZ Generic Firmware
 * Copyright (C) 2018-present, Envox d.o.o.
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

#include <assert.h>
#include <math.h>
#include <memory.h>
#include <stdio.h>
#include <string>
#include <utility>

#include <dma2d.h>
#include <i2c.h>
#include <ltdc.h>
#include <tim.h>

#include <cmsis_os.h>
#include <eez/apps/home/home.h>
#include <eez/gui/app_context.h>
#include <eez/gui/font.h>
#include <eez/gui/touch.h>
#include <eez/modules/mcu/display.h>
#include <eez/system.h>
#include <eez/util.h>

#include <eez/apps/psu/psu.h>
#include <eez/apps/psu/persist_conf.h>

#include <eez/platform/stm32/defines.h>

using namespace eez::gui;
using namespace eez::home;

namespace eez {
namespace mcu {
namespace display {

////////////////////////////////////////////////////////////////////////////////

static bool g_isOn;

static uint16_t *g_buffer;

static bool g_takeScreenshot;
static int g_screenshotY;

////////////////////////////////////////////////////////////////////////////////

#define DMA2D_WAIT while (HAL_DMA2D_PollForTransfer(&hdma2d, 1000) != HAL_OK)

uint32_t vramOffset(uint16_t *vram, int x, int y) {
    return (uint32_t)(vram + y * DISPLAY_WIDTH + x);
}

uint32_t vramOffset(uint32_t *vram, int x, int y) {
    return (uint32_t)(vram + y * DISPLAY_WIDTH + x);
}

void fillRect(uint16_t *dst, int x, int y, int width, int height, uint16_t color) {
	if (g_opacity == 255) {
		hdma2d.Init.Mode = DMA2D_R2M;
		hdma2d.Init.ColorMode = DMA2D_OUTPUT_RGB565;
		hdma2d.Init.OutputOffset = DISPLAY_WIDTH - width;

		uint32_t colorBGRA;
		uint8_t *pcolorBGRA = (uint8_t *)&colorBGRA;
		pcolorBGRA[0] = COLOR_TO_B(color);
		pcolorBGRA[1] = COLOR_TO_G(color);
		pcolorBGRA[2] = COLOR_TO_R(color);
		pcolorBGRA[3] = 255;

		DMA2D_WAIT;
		HAL_DMA2D_Init(&hdma2d);
		HAL_DMA2D_Start(&hdma2d, colorBGRA, vramOffset(dst, x, y), width, height);
	} else {
		// fill aux. buffer with BGRA color
		auto auxBuffer = (uint32_t *)VRAM_BUFFER3_START_ADDRESS;

		hdma2d.Init.Mode = DMA2D_R2M;
		hdma2d.Init.ColorMode = DMA2D_OUTPUT_ARGB8888;
		hdma2d.Init.OutputOffset = DISPLAY_WIDTH - width;

		uint32_t colorBGRA;
		uint8_t *pcolorBGRA = (uint8_t *)&colorBGRA;
		pcolorBGRA[0] = COLOR_TO_B(color);
		pcolorBGRA[1] = COLOR_TO_G(color);
		pcolorBGRA[2] = COLOR_TO_R(color);
		pcolorBGRA[3] = g_opacity;

		auto auxBufferOffset = vramOffset(auxBuffer, x, y);

		DMA2D_WAIT;
		HAL_DMA2D_Init(&hdma2d);
		HAL_DMA2D_Start(&hdma2d, colorBGRA, auxBufferOffset, width, height);

		// blend aux. buffer with dst buffer
	    hdma2d.Init.Mode = DMA2D_M2M_BLEND;
	    hdma2d.Init.ColorMode = DMA2D_OUTPUT_RGB565;
	    hdma2d.Init.OutputOffset = DISPLAY_WIDTH - width;

		hdma2d.LayerCfg[0].InputOffset = DISPLAY_WIDTH - width;
		hdma2d.LayerCfg[0].InputColorMode = DMA2D_OUTPUT_RGB565;
		hdma2d.LayerCfg[0].AlphaMode = DMA2D_NO_MODIF_ALPHA;
		hdma2d.LayerCfg[0].InputAlpha = 0;

		hdma2d.LayerCfg[1].InputOffset = DISPLAY_WIDTH - width;
		hdma2d.LayerCfg[1].InputColorMode = DMA2D_OUTPUT_ARGB8888;
		hdma2d.LayerCfg[1].AlphaMode = DMA2D_NO_MODIF_ALPHA;
		hdma2d.LayerCfg[1].AlphaInverted = DMA2D_REGULAR_ALPHA;
		hdma2d.LayerCfg[1].InputAlpha = 0;

	    auto dstOffset = vramOffset(dst, x, y);

	    DMA2D_WAIT;

	    HAL_DMA2D_Init(&hdma2d);
	    HAL_DMA2D_ConfigLayer(&hdma2d, 1);
		HAL_DMA2D_ConfigLayer(&hdma2d, 0);
		HAL_DMA2D_BlendingStart(&hdma2d, auxBufferOffset, dstOffset, dstOffset, width, height);
	}
}

void fillRect(void *dst, int x1, int y1, int x2, int y2) {
	fillRect((uint16_t *)dst, x1, y1, x2 - x1 + 1, y2 - y1 + 1, g_fc);
}

void bitBlt(void *src, int srcBpp, uint32_t srcLineOffset, uint16_t *dst, int x, int y, int width, int height) {
    hdma2d.Init.Mode = srcBpp == 32 ? DMA2D_M2M_BLEND : DMA2D_M2M_PFC;
    hdma2d.Init.ColorMode = DMA2D_OUTPUT_RGB565;
    hdma2d.Init.OutputOffset = DISPLAY_WIDTH - width;

    if (srcBpp == 32) {
        hdma2d.LayerCfg[0].InputOffset = DISPLAY_WIDTH - width;
        hdma2d.LayerCfg[0].InputColorMode = DMA2D_OUTPUT_RGB565;
        hdma2d.LayerCfg[0].AlphaMode = DMA2D_NO_MODIF_ALPHA;
        hdma2d.LayerCfg[0].InputAlpha = 0;

        hdma2d.LayerCfg[1].InputOffset = srcLineOffset;
        hdma2d.LayerCfg[1].InputColorMode = DMA2D_OUTPUT_ARGB8888;
        hdma2d.LayerCfg[1].AlphaMode = DMA2D_NO_MODIF_ALPHA;
        hdma2d.LayerCfg[1].AlphaInverted = DMA2D_REGULAR_ALPHA;
        hdma2d.LayerCfg[1].InputAlpha = 0;
    } else {
        hdma2d.LayerCfg[1].InputOffset = srcLineOffset;
        hdma2d.LayerCfg[1].InputColorMode = DMA2D_OUTPUT_RGB565;
        hdma2d.LayerCfg[1].AlphaMode = DMA2D_NO_MODIF_ALPHA;
        hdma2d.LayerCfg[1].InputAlpha = 0;
    }

    auto dstOffset = vramOffset(dst, x, y);

    DMA2D_WAIT;

    HAL_DMA2D_Init(&hdma2d);
    HAL_DMA2D_ConfigLayer(&hdma2d, 1);
    if (srcBpp == 32) {
        HAL_DMA2D_ConfigLayer(&hdma2d, 0);
        HAL_DMA2D_BlendingStart(&hdma2d, (uint32_t)src, dstOffset, dstOffset, width, height);
    } else {
        HAL_DMA2D_Start(&hdma2d, (uint32_t)src, dstOffset, width, height);
    }
}

void bitBlt(uint16_t *src, uint16_t *dst, int x, int y, int width, int height) {
    hdma2d.Init.Mode = DMA2D_M2M;
    hdma2d.Init.ColorMode = DMA2D_OUTPUT_RGB565;
    hdma2d.Init.OutputOffset = DISPLAY_WIDTH - width;

    hdma2d.LayerCfg[1].InputOffset = DISPLAY_WIDTH - width;
    hdma2d.LayerCfg[1].InputColorMode = DMA2D_OUTPUT_RGB565;
    hdma2d.LayerCfg[1].AlphaMode = DMA2D_NO_MODIF_ALPHA;
    hdma2d.LayerCfg[1].InputAlpha = 0;

    DMA2D_WAIT;

    HAL_DMA2D_Init(&hdma2d);
    HAL_DMA2D_ConfigLayer(&hdma2d, 1);
    HAL_DMA2D_Start(&hdma2d, (uint32_t)vramOffset(src, x, y), vramOffset(dst, x, y), width, height);
}

void bitBlt(void *src, void *dst, int x1, int y1, int x2, int y2) {
    bitBlt((uint16_t *)src, (uint16_t *)dst, x1, y1, x2 - x1 + 1, y2 - y1 + 1);
}

void bitBlt(uint16_t *src, uint16_t *dst, int x, int y, int width, int height, int dstx, int dsty) {
    hdma2d.Init.Mode = DMA2D_M2M;
    hdma2d.Init.ColorMode = DMA2D_OUTPUT_RGB565;
    hdma2d.Init.OutputOffset = DISPLAY_WIDTH - width;

    hdma2d.LayerCfg[1].InputOffset = DISPLAY_WIDTH - width;
    hdma2d.LayerCfg[1].InputColorMode = DMA2D_OUTPUT_RGB565;
    hdma2d.LayerCfg[1].AlphaMode = DMA2D_NO_MODIF_ALPHA;
    hdma2d.LayerCfg[1].InputAlpha = 0;

    DMA2D_WAIT;

    HAL_DMA2D_Init(&hdma2d);
    HAL_DMA2D_ConfigLayer(&hdma2d, 1);
    HAL_DMA2D_Start(&hdma2d, (uint32_t)vramOffset(src, x, y), vramOffset(dst, dstx, dsty), width, height);
}

void bitBlt(void *src, void *dst, int sx, int sy, int sw, int sh, int dx, int dy, uint8_t opacity) {
	hdma2d.Init.Mode = DMA2D_M2M_BLEND;
	hdma2d.Init.ColorMode = DMA2D_OUTPUT_RGB565;
	hdma2d.Init.OutputOffset = DISPLAY_WIDTH - sw;

	hdma2d.LayerCfg[0].InputOffset = DISPLAY_WIDTH - sw;
	hdma2d.LayerCfg[0].InputColorMode = DMA2D_OUTPUT_RGB565;
	hdma2d.LayerCfg[0].AlphaMode = DMA2D_COMBINE_ALPHA;
	hdma2d.LayerCfg[0].AlphaInverted = DMA2D_REGULAR_ALPHA;
	hdma2d.LayerCfg[0].InputAlpha = 0xFF;

	hdma2d.LayerCfg[1].InputOffset = DISPLAY_WIDTH - sw;
	hdma2d.LayerCfg[1].InputColorMode = DMA2D_OUTPUT_RGB565;
	hdma2d.LayerCfg[1].AlphaMode = DMA2D_COMBINE_ALPHA;
	hdma2d.LayerCfg[1].AlphaInverted = DMA2D_REGULAR_ALPHA;
	hdma2d.LayerCfg[1].InputAlpha = opacity;

	auto dstOffset = vramOffset((uint16_t *)dst, dx, dy);

	DMA2D_WAIT;

	HAL_DMA2D_Init(&hdma2d);
	HAL_DMA2D_ConfigLayer(&hdma2d, 1);
	HAL_DMA2D_ConfigLayer(&hdma2d, 0);
	HAL_DMA2D_BlendingStart(&hdma2d, vramOffset((uint16_t *)src, sx, sy), dstOffset, dstOffset, sw, sh);
}

void bitBltA8Init(uint16_t color) {
    // initialize everything except lineOffset

    hdma2d.Init.Mode = DMA2D_M2M_BLEND;
    hdma2d.Init.ColorMode = DMA2D_OUTPUT_RGB565;
    hdma2d.Init.OutputOffset = 0;

    // background
    hdma2d.LayerCfg[0].InputColorMode = DMA2D_OUTPUT_RGB565;
    hdma2d.LayerCfg[0].AlphaMode = DMA2D_NO_MODIF_ALPHA;
    hdma2d.LayerCfg[0].InputAlpha = 0;
    hdma2d.LayerCfg[0].InputOffset = 0;

    // foreground
    uint32_t colorBGRA;
    uint8_t *pcolorBGRA = (uint8_t *)&colorBGRA;
    pcolorBGRA[0] = COLOR_TO_B(color);
    pcolorBGRA[1] = COLOR_TO_G(color);
    pcolorBGRA[2] = COLOR_TO_R(color);
    pcolorBGRA[3] = 255;

    hdma2d.LayerCfg[1].InputColorMode = DMA2D_INPUT_A8;
    hdma2d.LayerCfg[1].AlphaMode = DMA2D_NO_MODIF_ALPHA;
    hdma2d.LayerCfg[1].InputAlpha = colorBGRA;
    hdma2d.LayerCfg[1].InputOffset = 0;

    DMA2D_WAIT;

    HAL_DMA2D_Init(&hdma2d);
    HAL_DMA2D_ConfigLayer(&hdma2d, 0);
    HAL_DMA2D_ConfigLayer(&hdma2d, 1);
}

void bitBltA8(const uint8_t *src, uint32_t srcLineOffset, int x, int y, int width, int height) {
    uint32_t lineOffset = DISPLAY_WIDTH - width;
    uint32_t dst = vramOffset(g_buffer, x, y);

    DMA2D_WAIT;

    // initialize lineOffset
    WRITE_REG(hdma2d.Instance->OOR, lineOffset);
    WRITE_REG(hdma2d.Instance->BGOR, lineOffset);
    WRITE_REG(hdma2d.Instance->FGOR, srcLineOffset);

    HAL_DMA2D_BlendingStart(&hdma2d, (uint32_t)src, dst, dst, width, height);
}

////////////////////////////////////////////////////////////////////////////////

void turnOn(bool withoutTransition) {
    if (!g_isOn) {
        __HAL_RCC_DMA2D_CLK_ENABLE();

        // clear video RAM
        g_buffer = (uint16_t *)VRAM_BUFFER1_START_ADDRESS;
        fillRect(g_buffer, 0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT, 0);

        // set video RAM address
        HAL_LTDC_SetAddress(&hltdc, (uint32_t)g_buffer, 0);

        // backlight on
        HAL_TIM_PWM_Start(&htim12, TIM_CHANNEL_2);
        uint32_t max = __HAL_TIM_GET_AUTORELOAD(&htim12);
        __HAL_TIM_SET_COMPARE(&htim12, TIM_CHANNEL_2, max / 2);

        g_isOn = true;
    }
}

void turnOff() {
    if (g_isOn) {
        __HAL_RCC_DMA2D_CLK_DISABLE();

        // backlight off
        HAL_TIM_PWM_Stop(&htim12, TIM_CHANNEL_2);

        g_isOn = false;
    }
}

bool isOn() {
    return g_isOn;
}

void updateBrightness() {
    uint32_t max = __HAL_TIM_GET_AUTORELOAD(&htim12);
    __HAL_TIM_SET_COMPARE(&htim12, TIM_CHANNEL_2, psu::persist_conf::devConf2.displayBrightness * max / 20);
}

////////////////////////////////////////////////////////////////////////////////

void animate() {
    uint16_t *bufferOld;
    uint16_t *bufferNew;

    if (g_buffer == (uint16_t *)VRAM_BUFFER1_START_ADDRESS) {
        bufferOld = (uint16_t *)VRAM_BUFFER2_START_ADDRESS;
        bufferNew = (uint16_t *)VRAM_BUFFER1_START_ADDRESS;
    } else {
        bufferOld = (uint16_t *)VRAM_BUFFER1_START_ADDRESS;
        bufferNew = (uint16_t *)VRAM_BUFFER2_START_ADDRESS;
    }

    uint16_t *bufferDst = nullptr;

    while (true) {
        float t = (millis() - g_animationState.startTime) / (1000.0f * psu::persist_conf::devConf2.animationsDuration);
        if (t >= 1.0f) {
            g_animationState.enabled = false;
            break;
        }

        bufferDst = bufferDst == (uint16_t *)VRAM_BUFFER3_START_ADDRESS
                         ? (uint16_t *)VRAM_BUFFER4_START_ADDRESS
                         : (uint16_t *)VRAM_BUFFER3_START_ADDRESS;

        g_animationState.callback(t, bufferOld, bufferNew, bufferDst);

        DMA2D_WAIT;

        // wait for VSYNC
        while (!(LTDC->CDSR & LTDC_CDSR_VSYNCS));

        HAL_LTDC_SetAddress(&hltdc, (uint32_t)bufferDst, 0);

        //osDelay(1);
    }
}

void sync() {
    if (!g_isOn) {
        return;
    }

    DMA2D_WAIT;

    if (g_takeScreenshot) {
    	bitBlt(g_buffer, (uint16_t *)VRAM_SCREENSHOOT_BUFFER_START_ADDRESS, 0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT);
        DMA2D_WAIT;
    	g_takeScreenshot = false;
    }

    if (g_painted) {
        g_painted = false;

        if (g_animationState.enabled) {
            animate();
        }

        // wait for VSYNC
        while (!(LTDC->CDSR & LTDC_CDSR_VSYNCS)) {}

		HAL_LTDC_SetAddress(&hltdc, (uint32_t)g_buffer, 0);

        // copy current video buffer to the new one
        uint16_t *g_newBufferAddress = g_buffer == (uint16_t *)VRAM_BUFFER1_START_ADDRESS
                                           ? (uint16_t *)VRAM_BUFFER2_START_ADDRESS
                                           : (uint16_t *)VRAM_BUFFER1_START_ADDRESS;
        bitBlt(g_buffer, g_newBufferAddress, 0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT);
        g_buffer = g_newBufferAddress;
    }
}

////////////////////////////////////////////////////////////////////////////////

int getDisplayWidth() {
    return DISPLAY_WIDTH;
}

int getDisplayHeight() {
    return DISPLAY_HEIGHT;
}

////////////////////////////////////////////////////////////////////////////////

void screanshotBegin() {
	g_takeScreenshot = true;
	do {
		osDelay(0);
	} while (g_takeScreenshot);

	g_screenshotY = 272 - 1;
}

bool screanshotGetLine(uint8_t *line) {
    if (g_screenshotY < 0) {
        return false;
    }

    uint16_t *src = ((uint16_t *)VRAM_SCREENSHOOT_BUFFER_START_ADDRESS) + g_screenshotY * getDisplayWidth();
    uint8_t *dst = line;

    int n = 480;
    while (n--) {
    	uint16_t color = *src++;

    	*dst++ = COLOR_TO_B(color);
		*dst++ = COLOR_TO_G(color);
    	*dst++ = COLOR_TO_R(color);
    }

    --g_screenshotY;

    return true;
}

void screanshotEnd() {
}

////////////////////////////////////////////////////////////////////////////////

static int8_t drawGlyph(int x1, int y1, int clip_x1, int clip_y1, int clip_x2, int clip_y2,
                        uint8_t encoding) {
    gui::font::Glyph glyph;
    g_font.getGlyph(encoding, glyph);
    if (!glyph.isFound())
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
        // // glyph doesn't fit, don't paint it
        // return glyph.dx;

    	width = clip_x2 - x_glyph + 1;
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
        bitBltA8(glyph.data + offset + iStartByte, glyph.width - width, x_glyph, y_glyph, width,
                 height);
    }

    return glyph.dx;
}

////////////////////////////////////////////////////////////////////////////////

void drawPixel(int x, int y) {
    DMA2D_WAIT;
    *(g_buffer + y * DISPLAY_WIDTH + x) = g_fc;

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
        fillRect(g_buffer, x1, y1, x2 - x1 + 1, y2 - y1 + 1, g_fc);
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

void drawHLine(int x, int y, int l) {
    fillRect(x, y, x + l, y);

    g_painted = true;
}

void drawVLine(int x, int y, int l) {
    fillRect(x, y, x, y + l);

    g_painted = true;
}

void bitBlt(int x1, int y1, int x2, int y2, int dstx, int dsty) {
    uint16_t * bufferOld;
    if (g_buffer == (uint16_t *)VRAM_BUFFER1_START_ADDRESS) {
        bufferOld = (uint16_t *)VRAM_BUFFER2_START_ADDRESS;
    } else {
        bufferOld = (uint16_t *)VRAM_BUFFER1_START_ADDRESS;
    }

    bitBlt(bufferOld, g_buffer, x1, y1, x2-x1+1, y2-y1+1, dstx, dsty);

    g_painted = true;
}

void drawBitmap(void *bitmapData, int bitmapBpp, int bitmapWidth, int x, int y, int width, int height) {
    bitBlt(bitmapData, bitmapBpp, bitmapWidth - width, g_buffer, x, y, width, height);
    g_painted = true;
}

void drawStr(const char *text, int textLength, int x, int y, int clip_x1, int clip_y1, int clip_x2,
             int clip_y2, gui::font::Font &font) {
    g_font = font;

    bitBltA8Init(g_fc);

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
