/*
* EEZ Framework
* Copyright (C) 2022-present, Envox d.o.o.
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

#include <math.h>

#if defined(EEZ_FOR_LVGL)

#include <stdint.h>

#if defined(EEZ_FOR_LVGL)
#ifdef LV_LVGL_H_INCLUDE_SIMPLE
#include "lvgl.h"
#else
#include "lvgl/lvgl.h"
#endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

#ifndef EXT_IMG_DESC_T
#define EXT_IMG_DESC_T
typedef struct _ext_img_desc_t {
    const char *name;
    const lv_img_dsc_t *img_dsc;
} ext_img_desc_t;
#endif

typedef void (*ActionExecFunc)(lv_event_t * e);
void eez_flow_init(const uint8_t *assets, uint32_t assetsSize, lv_obj_t **objects, size_t numObjects, const ext_img_desc_t *images, size_t numImages, ActionExecFunc *actions);

void eez_flow_tick();

bool eez_flow_is_stopped();

extern int16_t g_currentScreen;

void loadScreen(int index);

void flowOnPageLoaded(unsigned pageIndex);

// if flowState is nullptr then userWidgetComponentIndexOrPageIndex is page index
void *getFlowState(void *flowState, unsigned userWidgetComponentIndexOrPageIndex);

void flowPropagateValue(void *flowState, unsigned componentIndex, unsigned outputIndex);

const char *evalTextProperty(void *flowState, unsigned componentIndex, unsigned propertyIndex, const char *errorMessage);
int32_t evalIntegerProperty(void *flowState, unsigned componentIndex, unsigned propertyIndex, const char *errorMessage);
bool evalBooleanProperty(void *flowState, unsigned componentIndex, unsigned propertyIndex, const char *errorMessage);

void assignStringProperty(void *flowState, unsigned componentIndex, unsigned propertyIndex, const char *value, const char *errorMessage);
void assignIntegerProperty(void *flowState, unsigned componentIndex, unsigned propertyIndex, int32_t value, const char *errorMessage);
void assignBooleanProperty(void *flowState, unsigned componentIndex, unsigned propertyIndex, bool value, const char *errorMessage);

float eez_linear(float x);
float eez_easeInQuad(float x);
float eez_easeOutQuad(float x);
float eez_easeInOutQuad(float x);
float eez_easeInCubic(float x);
float eez_easeOutCubic(float x);
float eez_easeInOutCubic(float x);
float eez_easeInQuart(float x);
float eez_easeOutQuart(float x);
float eez_easeInOutQuart(float x);
float eez_easeInQuint(float x);
float eez_easeOutQuint(float x);
float eez_easeInOutQuint(float x);
float eez_easeInSine(float x);
float eez_easeOutSine(float x);
float eez_easeInOutSine(float x);
float eez_easeInExpo(float x);
float eez_easeOutExpo(float x);
float eez_easeInOutExpo(float x);
float eez_easeInCirc(float x);
float eez_easeOutCirc(float x);
float eez_easeInOutCirc(float x);
float eez_easeInBack(float x);
float eez_easeOutBack(float x);
float eez_easeInOutBack(float x);
float eez_easeInElastic(float x);
float eez_easeOutElastic(float x);
float eez_easeInOutElastic(float x);
float eez_easeOutBounce(float x);
float eez_easeInBounce(float x);
float eez_easeOutBounce(float x);
float eez_easeInOutBounce(float x);

float getTimelinePosition(void *flowState);

extern int g_eezFlowLvlgMeterTickIndex;

#ifdef __cplusplus
}
#endif

#endif // defined(EEZ_FOR_LVGL)
