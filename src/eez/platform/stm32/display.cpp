/*
 * EEZ Modular Firmware
 * Copyright (C) 2021-present, Envox d.o.o.
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

#if defined(EEZ_PLATFORM_STM32)

#include <assert.h>
#include <math.h>
#include <memory.h>
#include <stdio.h>
#include <string>
#include <utility>

#include <dma2d.h>
#include <i2c.h>
#include <ltdc.h>

#include <eez/util.h>
#include <eez/memory.h>
#include <eez/os.h>

#include <eez/gui/gui.h>
#include <eez/gui/display.h>
#include <eez/gui/display-private.h>

#include <eez/platform/stm32/display.h>

using namespace eez::gui;

namespace eez {
namespace gui {
namespace display {

////////////////////////////////////////////////////////////////////////////////

DisplayState g_displayState;

static uint16_t *g_bufferOld;
static uint16_t *g_bufferNew;
static uint16_t *g_buffer;
static uint16_t *g_animationBuffer;

static bool g_takeScreenshot;

////////////////////////////////////////////////////////////////////////////////

#define DMA2D_WAIT while (HAL_DMA2D_PollForTransfer(&hdma2d, 1000) != HAL_OK)

uint32_t vramOffset(uint16_t *vram, int x, int y) {
    return (uint32_t)(vram + y * DISPLAY_WIDTH + x);
}

uint32_t vramOffsetRGB888(uint8_t *vram, int x, int y) {
    return (uint32_t)(vram + (y * DISPLAY_WIDTH  + x) * 3);
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
		auto auxBuffer = (uint32_t *)VRAM_AUX_BUFFER7_START_ADDRESS;

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
		hdma2d.LayerCfg[0].InputColorMode = DMA2D_INPUT_RGB565;
		hdma2d.LayerCfg[0].AlphaMode = DMA2D_NO_MODIF_ALPHA;
		hdma2d.LayerCfg[0].InputAlpha = 0;

		hdma2d.LayerCfg[1].InputOffset = DISPLAY_WIDTH - width;
		hdma2d.LayerCfg[1].InputColorMode = DMA2D_INPUT_ARGB8888;
		hdma2d.LayerCfg[1].AlphaMode = DMA2D_NO_MODIF_ALPHA;
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
    setDirty();
}

void bitBlt(void *src, int srcBpp, uint32_t srcLineOffset, uint16_t *dst, int x, int y, int width, int height) {
    hdma2d.Init.ColorMode = DMA2D_OUTPUT_RGB565;
    hdma2d.Init.OutputOffset = DISPLAY_WIDTH - width;

    if (srcBpp == 32) {
        hdma2d.Init.Mode = DMA2D_M2M_BLEND;

        hdma2d.LayerCfg[0].InputOffset = DISPLAY_WIDTH - width;
        hdma2d.LayerCfg[0].InputColorMode = DMA2D_INPUT_RGB565;
        hdma2d.LayerCfg[0].AlphaMode = DMA2D_NO_MODIF_ALPHA;
        hdma2d.LayerCfg[0].InputAlpha = 0;

        hdma2d.LayerCfg[1].InputOffset = srcLineOffset;
        hdma2d.LayerCfg[1].InputColorMode = DMA2D_INPUT_ARGB8888;
        hdma2d.LayerCfg[1].AlphaMode = DMA2D_NO_MODIF_ALPHA;
        hdma2d.LayerCfg[1].InputAlpha = 0;
    } else if (srcBpp == 24) {
    	hdma2d.Init.Mode = DMA2D_M2M_PFC;
        hdma2d.Init.RedBlueSwap = DMA2D_RB_SWAP;

        hdma2d.LayerCfg[1].InputOffset = srcLineOffset;
        hdma2d.LayerCfg[1].InputColorMode = DMA2D_INPUT_RGB888;
        hdma2d.LayerCfg[1].AlphaMode = DMA2D_NO_MODIF_ALPHA;
        hdma2d.LayerCfg[1].InputAlpha = 0;
    } else {
    	hdma2d.Init.Mode = DMA2D_M2M;

        hdma2d.LayerCfg[1].InputOffset = srcLineOffset;
        hdma2d.LayerCfg[1].InputColorMode = DMA2D_INPUT_RGB565;
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

    if (srcBpp == 24) {
        hdma2d.Init.RedBlueSwap = DMA2D_RB_REGULAR;
    }
}

void bitBlt(void *src, int x1, int y1, int x2, int y2) {
    bitBlt(src, g_buffer, x1, y1, x2, y2);
    setDirty();
}

void bitBlt(uint16_t *src, uint16_t *dst, int x, int y, int width, int height) {
    hdma2d.Init.Mode = DMA2D_M2M;
    hdma2d.Init.ColorMode = DMA2D_OUTPUT_RGB565;
    hdma2d.Init.OutputOffset = DISPLAY_WIDTH - width;

    hdma2d.LayerCfg[1].InputOffset = DISPLAY_WIDTH - width;
    hdma2d.LayerCfg[1].InputColorMode = DMA2D_INPUT_RGB565;
    hdma2d.LayerCfg[1].AlphaMode = DMA2D_NO_MODIF_ALPHA;
    hdma2d.LayerCfg[1].InputAlpha = 0;

    DMA2D_WAIT;

    HAL_DMA2D_Init(&hdma2d);
    HAL_DMA2D_ConfigLayer(&hdma2d, 1);
    HAL_DMA2D_Start(&hdma2d, vramOffset(src, x, y), vramOffset(dst, x, y), width, height);
}

void bitBltRGB888(uint16_t *src, uint8_t *dst, int x, int y, int width, int height) {
    hdma2d.Init.Mode = DMA2D_M2M_PFC;
    hdma2d.Init.ColorMode = DMA2D_OUTPUT_RGB888;
    hdma2d.Init.OutputOffset = DISPLAY_WIDTH - width;
    hdma2d.Init.RedBlueSwap = DMA2D_RB_SWAP;

    hdma2d.LayerCfg[1].InputOffset = DISPLAY_WIDTH - width;
    hdma2d.LayerCfg[1].InputColorMode = DMA2D_INPUT_RGB565;
    hdma2d.LayerCfg[1].AlphaMode = DMA2D_NO_MODIF_ALPHA;
    hdma2d.LayerCfg[1].InputAlpha = 0;

    DMA2D_WAIT;

    HAL_DMA2D_Init(&hdma2d);
    HAL_DMA2D_ConfigLayer(&hdma2d, 1);
    HAL_DMA2D_Start(&hdma2d, vramOffset(src, x, y), vramOffsetRGB888(dst, x, y), width, height);

    hdma2d.Init.RedBlueSwap = DMA2D_RB_REGULAR;
}

void bitBlt(void *src, void *dst, int x1, int y1, int x2, int y2) {
    bitBlt((uint16_t *)src, (uint16_t *)dst, x1, y1, x2 - x1 + 1, y2 - y1 + 1);
    setDirty();
}

void bitBlt(uint16_t *src, uint16_t *dst, int x, int y, int width, int height, int dstx, int dsty) {
    hdma2d.Init.Mode = DMA2D_M2M;
    hdma2d.Init.ColorMode = DMA2D_OUTPUT_RGB565;
    hdma2d.Init.OutputOffset = DISPLAY_WIDTH - width;

    hdma2d.LayerCfg[1].InputOffset = DISPLAY_WIDTH - width;
    hdma2d.LayerCfg[1].InputColorMode = DMA2D_INPUT_RGB565;
    hdma2d.LayerCfg[1].AlphaMode = DMA2D_NO_MODIF_ALPHA;
    hdma2d.LayerCfg[1].InputAlpha = 0;

    DMA2D_WAIT;

    HAL_DMA2D_Init(&hdma2d);
    HAL_DMA2D_ConfigLayer(&hdma2d, 1);
    HAL_DMA2D_Start(&hdma2d, vramOffset(src, x, y), vramOffset(dst, dstx, dsty), width, height);
}

void bitBlt(void *src, void *dst, int sx, int sy, int sw, int sh, int dx, int dy, uint8_t opacity) {
    if (dst == nullptr) {
        dst = g_buffer;
    }

	auto srcOffset = vramOffset((uint16_t *)src, sx, sy);
	auto dstOffset = vramOffset((uint16_t *)dst, dx, dy);

	if (opacity == 255) {
	    hdma2d.Init.Mode = DMA2D_M2M;
	    hdma2d.Init.ColorMode = DMA2D_OUTPUT_RGB565;
	    hdma2d.Init.OutputOffset = DISPLAY_WIDTH - sw;

	    hdma2d.LayerCfg[1].InputOffset = DISPLAY_WIDTH - sw;
	    hdma2d.LayerCfg[1].InputColorMode = DMA2D_INPUT_RGB565;
	    hdma2d.LayerCfg[1].AlphaMode = DMA2D_NO_MODIF_ALPHA;
	    hdma2d.LayerCfg[1].InputAlpha = 0;

	    DMA2D_WAIT;

	    HAL_DMA2D_Init(&hdma2d);
	    HAL_DMA2D_ConfigLayer(&hdma2d, 1);
	    HAL_DMA2D_Start(&hdma2d, srcOffset, dstOffset, sw, sh);
	} else {
		hdma2d.Init.Mode = DMA2D_M2M_BLEND;
		hdma2d.Init.ColorMode = DMA2D_OUTPUT_RGB565;
		hdma2d.Init.OutputOffset = DISPLAY_WIDTH - sw;

		hdma2d.LayerCfg[0].InputOffset = DISPLAY_WIDTH - sw;
		hdma2d.LayerCfg[0].InputColorMode = DMA2D_INPUT_RGB565;
		hdma2d.LayerCfg[0].AlphaMode = DMA2D_COMBINE_ALPHA;
		hdma2d.LayerCfg[0].InputAlpha = 0xFF;

		hdma2d.LayerCfg[1].InputOffset = DISPLAY_WIDTH - sw;
		hdma2d.LayerCfg[1].InputColorMode = DMA2D_INPUT_RGB565;
		hdma2d.LayerCfg[1].AlphaMode = DMA2D_COMBINE_ALPHA;
		hdma2d.LayerCfg[1].InputAlpha = opacity;

		DMA2D_WAIT;

		HAL_DMA2D_Init(&hdma2d);
		HAL_DMA2D_ConfigLayer(&hdma2d, 1);
		HAL_DMA2D_ConfigLayer(&hdma2d, 0);
		HAL_DMA2D_BlendingStart(&hdma2d, srcOffset, dstOffset, dstOffset, sw, sh);
	}
}

////////////////////////////////////////////////////////////////////////////////

void *getBufferPointer() {
    return g_buffer;
}

void setBufferPointer(void *buffer) {
    g_buffer = (uint16_t *)buffer;
}

static void setAddress(void *buffer) {
    // Configure the color frame buffer start address
	auto layer = (LTDC_Layer_TypeDef *)((uint32_t)hltdc.Instance + 0x84U);
	layer->CFBAR = (uint32_t)buffer;
    // Set the Immediate Reload type
	hltdc.Instance->SRCR = LTDC_SRCR_IMR;
}

void turnOn() {
    if (g_displayState != ON && g_displayState != TURNING_ON) {
        __HAL_RCC_DMA2D_CLK_ENABLE();

        // clear video RAM
        g_bufferOld = (uint16_t *)VRAM_BUFFER2_START_ADDRESS;
        g_bufferNew = (uint16_t *)VRAM_BUFFER1_START_ADDRESS;
        g_buffer = g_bufferNew;

		g_buffers[0].bufferPointer = (uint16_t *)(VRAM_AUX_BUFFER1_START_ADDRESS);
		g_buffers[1].bufferPointer = (uint16_t *)(VRAM_AUX_BUFFER2_START_ADDRESS);
		g_buffers[2].bufferPointer = (uint16_t *)(VRAM_AUX_BUFFER3_START_ADDRESS);
		g_buffers[3].bufferPointer = (uint16_t *)(VRAM_AUX_BUFFER4_START_ADDRESS);
		g_buffers[4].bufferPointer = (uint16_t *)(VRAM_AUX_BUFFER5_START_ADDRESS);
		g_buffers[5].bufferPointer = (uint16_t *)(VRAM_AUX_BUFFER6_START_ADDRESS);

        fillRect(g_bufferOld, 0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT, 0);
        fillRect(g_bufferNew, 0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT, 0);

        // set video RAM address
        setAddress(g_buffer);

        turnOnStartHook();
    }
}

void turnOff() {
    if (g_displayState != OFF && g_displayState != TURNING_OFF) {
        turnOffStartHook();
    }
}

bool isOn() {
    return g_displayState == ON || g_displayState == TURNING_ON;
}

////////////////////////////////////////////////////////////////////////////////

void animate() {
	float t = (millis() - g_animationState.startTime) / (1000.0f * g_animationState.duration);
	if (t < 1.0f) {
		g_animationBuffer = g_animationBuffer == (uint16_t *)VRAM_ANIMATION_BUFFER1_START_ADDRESS
						 ? (uint16_t *)VRAM_ANIMATION_BUFFER2_START_ADDRESS
						 : (uint16_t *)VRAM_ANIMATION_BUFFER1_START_ADDRESS;

		g_animationState.callback(t, g_bufferOld, g_buffer, g_animationBuffer);

		DMA2D_WAIT;

		// wait for VSYNC
		while (!(LTDC->CDSR & LTDC_CDSR_VSYNCS)) {
			osDelay(0);
		}

		setAddress(g_animationBuffer);
	} else {
		g_animationState.enabled = false;
	}
}

void swapBuffers() {
    // wait for VSYNC
    while (!(LTDC->CDSR & LTDC_CDSR_VSYNCS)) {
    	osDelay(0);
    }

    setAddress(g_buffer);

    auto temp = g_bufferNew;
    g_bufferNew = g_bufferOld;
    g_bufferOld = temp;

    g_buffer = g_bufferNew;
}

void sync() {
    if (g_displayState == OFF) {
        return;
    }

    if (g_displayState == TURNING_OFF) {
    	turnOffTickHook();
        return;
    }

    if (g_displayState == TURNING_ON) {
    	turnOnTickHook();
    }

    DMA2D_WAIT;

    if (g_animationState.enabled) {
        animate();
        if (!g_animationState.enabled) {
            finishAnimation();
        }
        clearDirty();
        // clearDirty();
        return;
    }

    if (isDirty()) {
        clearDirty();

        swapBuffers();
    }

    if (g_takeScreenshot) {
    	bitBltRGB888(g_bufferOld, SCREENSHOOT_BUFFER_START_ADDRESS, 0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT);
        DMA2D_WAIT;
    	g_takeScreenshot = false;
    }
}

void finishAnimation() {
    auto oldBuffer = g_buffer;
    swapBuffers();
    bitBlt(oldBuffer, 0, 0, getDisplayWidth() - 1, getDisplayHeight() - 1);
}

////////////////////////////////////////////////////////////////////////////////

int getDisplayWidth() {
    return DISPLAY_WIDTH;
}

int getDisplayHeight() {
    return DISPLAY_HEIGHT;
}

////////////////////////////////////////////////////////////////////////////////

const uint8_t *takeScreenshot() {
	g_takeScreenshot = true;
	do {
		osDelay(0);
	} while (g_takeScreenshot);

	return SCREENSHOOT_BUFFER_START_ADDRESS;
}

////////////////////////////////////////////////////////////////////////////////

void drawPixel(int x, int y) {
    DMA2D_WAIT;
    *(g_buffer + y * DISPLAY_WIDTH + x) = g_fc;

    setDirty();
}

void drawPixel(int x, int y, uint8_t opacity) {
    DMA2D_WAIT;

    auto dest = g_buffer + y * DISPLAY_WIDTH + x;
    *dest = color32to16(
		blendColor(
			color16to32(g_fc, opacity),
			color16to32(*dest, 255 - opacity)
		)
	);

    setDirty();
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

    setDirty();
}

void fillRect(int x1, int y1, int x2, int y2, int r) {
    if (r == 0) {
        fillRect(g_buffer, x1, y1, x2 - x1 + 1, y2 - y1 + 1, g_fc);
    } else {
        fillRoundedRect(x1, y1, x2, y2, r);
    }

    setDirty();
}

void drawHLine(int x, int y, int l) {
    fillRect(x, y, x + l, y);

    setDirty();
}

void drawVLine(int x, int y, int l) {
    fillRect(x, y, x, y + l);

    setDirty();
}

void bitBlt(int x1, int y1, int x2, int y2, int dstx, int dsty) {
    bitBlt(g_buffer, g_buffer, x1, y1, x2-x1+1, y2-y1+1, dstx, dsty);

    setDirty();
}

void drawBitmap(Image *image, int x, int y) {
    bitBlt(image->pixels, image->bpp, image->lineOffset, g_buffer, x, y, image->width, image->height);

    setDirty();
}

void drawStrInit() {
    auto color = g_fc;

    // initialize everything except lineOffset

    hdma2d.Init.Mode = DMA2D_M2M_BLEND;
    hdma2d.Init.ColorMode = DMA2D_OUTPUT_RGB565;
    hdma2d.Init.OutputOffset = 0;

    // background
    hdma2d.LayerCfg[0].InputColorMode = DMA2D_INPUT_RGB565;
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

void drawGlyph(const uint8_t *src, uint32_t srcLineOffset, int x, int y, int width, int height) {
    uint32_t lineOffset = DISPLAY_WIDTH - width;
    uint32_t dst = vramOffset(g_buffer, x, y);

    DMA2D_WAIT;

    // initialize lineOffset
    WRITE_REG(hdma2d.Instance->OOR, lineOffset);
    WRITE_REG(hdma2d.Instance->BGOR, lineOffset);
    WRITE_REG(hdma2d.Instance->FGOR, srcLineOffset);

    HAL_DMA2D_BlendingStart(&hdma2d, (uint32_t)src, dst, dst, width, height);
}

} // namespace display
} // namespace gui
} // namespace eez

#endif // EEZ_PLATFORM_STM32
