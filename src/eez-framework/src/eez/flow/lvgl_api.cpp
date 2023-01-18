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

#include <eez/conf-internal.h>

#if defined(EEZ_FOR_LVGL)

#include <stdio.h>

#if defined(EEZ_FOR_LVGL)
#ifdef LV_LVGL_H_INCLUDE_SIMPLE
#include "lvgl.h"
#else
#include "lvgl/lvgl.h"
#endif
#endif


#include <eez/core/os.h>
#include <eez/core/action.h>

#include <eez/flow/flow.h>
#include <eez/flow/expression.h>
#include <eez/flow/hooks.h>
#include <eez/flow/debugger.h>
#include <eez/flow/components.h>
#include <eez/flow/flow_defs_v3.h>

#include <eez/flow/lvgl_api.h>

static void replacePageHook(int16_t pageId, uint32_t animType, uint32_t speed, uint32_t delay);

extern "C" void create_screens();
extern "C" void tick_screen(int screen_index);

static lv_obj_t **g_objects;
static size_t g_numObjects;
static const ext_img_desc_t *g_images;
static size_t g_numImages;

int16_t g_currentScreen = -1;

static lv_obj_t *getLvglObjectFromIndex(int32_t index) {
    if (index == -1) {
        return 0;
    }
    return g_objects[index];
}

static const void *getLvglImageByName(const char *name) {
    for (size_t imageIndex = 0; imageIndex < g_numImages; imageIndex++) {
        if (strcmp(g_images[imageIndex].name, name) == 0) {
            return g_images[imageIndex].img_dsc;
        }
    }
    return 0;
}

extern "C" void loadScreen(int index) {
    g_currentScreen = index;
    lv_obj_t *screen = getLvglObjectFromIndex(index);
    lv_scr_load_anim(screen, LV_SCR_LOAD_ANIM_FADE_IN, 200, 0, false);
}

extern "C" void eez_flow_init(const uint8_t *assets, uint32_t assetsSize, lv_obj_t **objects, size_t numObjects, const ext_img_desc_t *images, size_t numImages) {
    g_objects = objects;
    g_numObjects = numObjects;
    g_images = images;
    g_numImages = numImages;

    eez::initAssetsMemory();
    eez::loadMainAssets(assets, assetsSize);
    eez::initOtherMemory();
    eez::initAllocHeap(eez::ALLOC_BUFFER, eez::ALLOC_BUFFER_SIZE);

    eez::flow::replacePageHook = replacePageHook;
    eez::flow::getLvglObjectFromIndexHook = getLvglObjectFromIndex;
    eez::flow::getLvglImageByNameHook = getLvglImageByName;

    eez::flow::start(eez::g_mainAssets);

    create_screens();
    replacePageHook(1, 0, 0, 0);
}

extern "C" void eez_flow_tick() {
    eez::flow::tick();
}

namespace eez {
ActionExecFunc g_actionExecFunctions[] = { 0 };
}

void replacePageHook(int16_t pageId, uint32_t animType, uint32_t speed, uint32_t delay) {
    eez::flow::onPageChanged(g_currentScreen + 1, pageId);
    g_currentScreen = pageId - 1;
    lv_scr_load_anim(getLvglObjectFromIndex(g_currentScreen), (lv_scr_load_anim_t)animType, speed, delay, false);
}

extern "C" void flowOnPageLoaded(unsigned pageIndex) {
    eez::flow::getPageFlowState(eez::g_mainAssets, pageIndex);
}

extern "C" void flowPropagateValue(unsigned pageIndex, unsigned componentIndex, unsigned outputIndex) {
    eez::flow::FlowState *flowState = eez::flow::getPageFlowState(eez::g_mainAssets, pageIndex);
    eez::flow::propagateValue(flowState, componentIndex, outputIndex);
}

static char textValue[1000];

extern "C" const char *evalTextProperty(unsigned pageIndex, unsigned componentIndex, unsigned propertyIndex, const char *errorMessage) {
    eez::flow::FlowState *flowState = eez::flow::getPageFlowState(eez::g_mainAssets, pageIndex);
    eez::Value value;
    if (!eez::flow::evalProperty(flowState, componentIndex, propertyIndex, value, errorMessage)) {
        return "";
    }
    value.toText(textValue, sizeof(textValue));
    return textValue;
}

extern "C" int32_t evalIntegerProperty(unsigned pageIndex, unsigned componentIndex, unsigned propertyIndex, const char *errorMessage) {
    eez::flow::FlowState *flowState = eez::flow::getPageFlowState(eez::g_mainAssets, pageIndex);
    eez::Value value;
    if (!eez::flow::evalProperty(flowState, componentIndex, propertyIndex, value, errorMessage)) {
        return 0;
    }
    int err;
    int32_t intValue = value.toInt32(&err);
    if (err) {
        eez::flow::throwError(flowState, componentIndex, errorMessage);
        return 0;
    }
    return intValue;
}

extern "C" bool evalBooleanProperty(unsigned pageIndex, unsigned componentIndex, unsigned propertyIndex, const char *errorMessage) {
    eez::flow::FlowState *flowState = eez::flow::getPageFlowState(eez::g_mainAssets, pageIndex);
    eez::Value value;
    if (!eez::flow::evalProperty(flowState, componentIndex, propertyIndex, value, errorMessage)) {
        return 0;
    }
    int err;
    bool booleanValue = value.toBool(&err);
    if (err) {
        eez::flow::throwError(flowState, componentIndex, errorMessage);
        return 0;
    }
    return booleanValue;
}

extern "C" void assignStringProperty(unsigned pageIndex, unsigned componentIndex, unsigned propertyIndex, const char *value, const char *errorMessage) {
    eez::flow::FlowState *flowState = eez::flow::getPageFlowState(eez::g_mainAssets, pageIndex);

    auto component = flowState->flow->components[componentIndex];

    eez::Value dstValue;
    if (!eez::flow::evalAssignableExpression(flowState, componentIndex, component->properties[propertyIndex]->evalInstructions, dstValue, errorMessage)) {
        return;
    }

    eez::Value srcValue = eez::Value::makeStringRef(value, -1, 0x3eefcf0d);

    eez::flow::assignValue(flowState, componentIndex, dstValue, srcValue);
}

extern "C" void assignIntegerProperty(unsigned pageIndex, unsigned componentIndex, unsigned propertyIndex, int32_t value, const char *errorMessage) {
    eez::flow::FlowState *flowState = eez::flow::getPageFlowState(eez::g_mainAssets, pageIndex);

    auto component = flowState->flow->components[componentIndex];

    eez::Value dstValue;
    if (!eez::flow::evalAssignableExpression(flowState, componentIndex, component->properties[propertyIndex]->evalInstructions, dstValue, errorMessage)) {
        return;
    }

    eez::Value srcValue((int)value, eez::VALUE_TYPE_INT32);

    eez::flow::assignValue(flowState, componentIndex, dstValue, srcValue);
}

extern "C" void assignBooleanProperty(unsigned pageIndex, unsigned componentIndex, unsigned propertyIndex, bool value, const char *errorMessage) {
    eez::flow::FlowState *flowState = eez::flow::getPageFlowState(eez::g_mainAssets, pageIndex);

    auto component = flowState->flow->components[componentIndex];

    eez::Value dstValue;
    if (!eez::flow::evalAssignableExpression(flowState, componentIndex, component->properties[propertyIndex]->evalInstructions, dstValue, errorMessage)) {
        return;
    }

    eez::Value srcValue(value, eez::VALUE_TYPE_BOOLEAN);

    eez::flow::assignValue(flowState, componentIndex, dstValue, srcValue);
}

extern "C" float getTimelinePosition(unsigned pageIndex) {
    eez::flow::FlowState *flowState = eez::flow::getPageFlowState(eez::g_mainAssets, pageIndex);
    return flowState->timelinePosition;
}

#endif // EEZ_FOR_LVGL
