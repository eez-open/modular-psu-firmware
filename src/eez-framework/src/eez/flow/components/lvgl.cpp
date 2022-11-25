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

#include <stdio.h>
#include <eez/core/os.h>

#include <eez/flow/components.h>
#include <eez/flow/flow_defs_v3.h>
#include <eez/flow/expression.h>
#include <eez/flow/private.h>
#include <eez/flow/queue.h>
#include <eez/flow/debugger.h>
#include <eez/flow/hooks.h>

#include <eez/flow/components/lvgl.h>

#if defined(EEZ_FOR_LVGL)

namespace eez {
namespace flow {

////////////////////////////////////////////////////////////////////////////////

void anim_callback_set_x(lv_anim_t * a, int32_t v) { lv_obj_set_x((lv_obj_t *)a->user_data, v); }
int32_t anim_callback_get_x(lv_anim_t * a) { return lv_obj_get_x_aligned((lv_obj_t *)a->user_data); }

void anim_callback_set_y(lv_anim_t * a, int32_t v) { lv_obj_set_y((lv_obj_t *)a->user_data, v); }
int32_t anim_callback_get_y(lv_anim_t * a) { return lv_obj_get_y_aligned((lv_obj_t *)a->user_data); }

void anim_callback_set_width(lv_anim_t * a, int32_t v) { lv_obj_set_width((lv_obj_t *)a->user_data, v); }
int32_t anim_callback_get_width(lv_anim_t * a) { return lv_obj_get_width((lv_obj_t *)a->user_data); }

void anim_callback_set_height(lv_anim_t * a, int32_t v) { lv_obj_set_height((lv_obj_t *)a->user_data, v); }
int32_t anim_callback_get_height(lv_anim_t * a) { return lv_obj_get_height((lv_obj_t *)a->user_data); }

void anim_callback_set_opacity(lv_anim_t * a, int32_t v) { lv_obj_set_style_opa((lv_obj_t *)a->user_data, v, 0); }
int32_t anim_callback_get_opacity(lv_anim_t * a) { return lv_obj_get_style_opa((lv_obj_t *)a->user_data, 0); }

void anim_callback_set_image_zoom(lv_anim_t * a, int32_t v) { lv_img_set_zoom((lv_obj_t *)a->user_data, v); }
int32_t anim_callback_get_image_zoom(lv_anim_t * a) { return lv_img_get_zoom((lv_obj_t *)a->user_data); }

void anim_callback_set_image_angle(lv_anim_t * a, int32_t v) { lv_img_set_angle((lv_obj_t *)a->user_data, v); }
int32_t anim_callback_get_image_angle(lv_anim_t * a) { return lv_img_get_angle((lv_obj_t *)a->user_data); }

void (*anim_set_callbacks[])(lv_anim_t *a, int32_t v) = {
    anim_callback_set_x,
    anim_callback_set_y,
    anim_callback_set_width,
    anim_callback_set_height,
    anim_callback_set_opacity,
    anim_callback_set_image_zoom,
    anim_callback_set_image_angle
};

int32_t (*anim_get_callbacks[])(lv_anim_t *a) = {
    anim_callback_get_x,
    anim_callback_get_y,
    anim_callback_get_width,
    anim_callback_get_height,
    anim_callback_get_opacity,
    anim_callback_get_image_zoom,
    anim_callback_get_image_angle
};

int32_t (*anim_path_callbacks[])(const lv_anim_t *a) = {
    lv_anim_path_linear,
    lv_anim_path_ease_in,
    lv_anim_path_ease_out,
    lv_anim_path_ease_in_out,
    lv_anim_path_overshoot,
    lv_anim_path_bounce
};

////////////////////////////////////////////////////////////////////////////////

enum PropertyCode {
    NONE,

    ARC_VALUE,

    BAR_VALUE,

    BASIC_X,
    BASIC_Y,
    BASIC_WIDTH,
    BASIC_HEIGHT,
    BASIC_OPACITY,
    BASIC_HIDDEN,
    BASIC_CHECKED,
    BASIC_DISABLED,

    DROPDOWN_SELECTED,

    IMAGE_IMAGE,
    IMAGE_ANGLE,
    IMAGE_ZOOM,

    LABEL_TEXT,

    ROLLER_SELECTED,

    SLIDER_VALUE,

    KEYBOARD_TEXTAREA
};

////////////////////////////////////////////////////////////////////////////////

void executeLVGLComponent(FlowState *flowState, unsigned componentIndex) {
    auto component = (LVGLComponent *)flowState->flow->components[componentIndex];

    char errorMessage[256];

    for (uint32_t actionIndex = 0; actionIndex < component->actions.count; actionIndex++) {
        auto general = (LVGLComponent_ActionType *)component->actions[actionIndex];
        if (general->action == CHANGE_SCREEN) {
            auto specific = (LVGLComponent_ChangeScreen_ActionType *)general;
            replacePageHook(specific->screen, specific->fadeMode, specific->speed, specific->delay);
        } else if (general->action == PLAY_ANIMATION) {
            auto specific = (LVGLComponent_PlayAnimation_ActionType *)general;

            auto target = getLvglObjectFromIndexHook(specific->target);

            lv_anim_t anim;

            lv_anim_init(&anim);
            lv_anim_set_time(&anim, specific->time);
            lv_anim_set_user_data(&anim, target);
            lv_anim_set_custom_exec_cb(&anim, anim_set_callbacks[specific->property]);
            lv_anim_set_values(&anim, specific->start, specific->end);
            lv_anim_set_path_cb(&anim, anim_path_callbacks[specific->path]);
            lv_anim_set_delay(&anim, specific->delay);
            lv_anim_set_early_apply(&anim, specific->flags & ANIMATION_ITEM_FLAG_INSTANT ? true : false);
            if (specific->flags & ANIMATION_ITEM_FLAG_RELATIVE) {
                lv_anim_set_get_value_cb(&anim, anim_get_callbacks[specific->property]);
            }

            lv_anim_start(&anim);
        } else if (general->action == SET_PROPERTY) {
            auto specific = (LVGLComponent_SetProperty_ActionType *)general;

            auto target = getLvglObjectFromIndexHook(specific->target);

            if (specific->property == KEYBOARD_TEXTAREA) {
                auto textarea = specific->textarea != -1 ? getLvglObjectFromIndexHook(specific->textarea) : nullptr;
                lv_keyboard_set_textarea(target, textarea);
            } else {
                Value value;
                snprintf(errorMessage, sizeof(errorMessage), "Failed to evaluate Value in LVGL Set Property action #%d", (int)(actionIndex + 1));
                if (!evalExpression(flowState, componentIndex, specific->value, value, errorMessage)) {
                    return;
                }

                if (specific->property == IMAGE_IMAGE || specific->property == LABEL_TEXT) {
                    const char *strValue = value.toString(0xe42b3ca2).getString();
                    if (specific->property == IMAGE_IMAGE) {
                        const void *src = getLvglImageByNameHook(strValue);
                        if (src) {
                            lv_img_set_src(target, src);
                        } else {
                            snprintf(errorMessage, sizeof(errorMessage), "Image \"%s\" not found in LVGL Set Property action #%d", strValue, (int)(actionIndex + 1));
                            throwError(flowState, componentIndex, errorMessage);
                        }
                    } else {
                        lv_label_set_text(target, strValue);
                    }
                } else if (specific->property == BASIC_HIDDEN) {
                    int err;
                    bool booleanValue = value.toBool(&err);
                    if (err) {
                        snprintf(errorMessage, sizeof(errorMessage), "Failed to convert value to boolean in LVGL Set Property action #%d", (int)(actionIndex + 1));
                        throwError(flowState, componentIndex, errorMessage);
                        return;
                    }

                    lv_state_t flag = LV_OBJ_FLAG_HIDDEN;

                    if (booleanValue) lv_obj_add_flag(target, flag);
                    else lv_obj_clear_flag(target, flag);
                } else if (specific->property == BASIC_CHECKED || specific->property == BASIC_DISABLED) {
                    int err;
                    bool booleanValue = value.toBool(&err);
                    if (err) {
                        snprintf(errorMessage, sizeof(errorMessage), "Failed to convert value to boolean in LVGL Set Property action #%d", (int)(actionIndex + 1));
                        throwError(flowState, componentIndex, errorMessage);
                        return;
                    }

                    lv_state_t state = specific->property == BASIC_CHECKED ? LV_STATE_CHECKED : LV_STATE_DISABLED;

                    if (booleanValue) lv_obj_add_state(target, state);
                    else lv_obj_clear_state(target, state);
                } else {
                    int err;
                    int32_t intValue = value.toInt32(&err);
                    if (err) {
                        snprintf(errorMessage, sizeof(errorMessage), "Failed to convert value to integer in LVGL Set Property action #%d", (int)(actionIndex + 1));
                        throwError(flowState, componentIndex, errorMessage);
                        return;
                    }

                    if (specific->property == ARC_VALUE) {
                        lv_arc_set_value(target, intValue);
                    } else if (specific->property == BAR_VALUE) {
                        lv_bar_set_value(target, intValue, specific->animated ? LV_ANIM_ON : LV_ANIM_OFF);
                    } else if (specific->property == BASIC_X) {
                        lv_obj_set_x(target, intValue);
                    } else if (specific->property == BASIC_Y) {
                        lv_obj_set_y(target, intValue);
                    } else if (specific->property == BASIC_WIDTH) {
                        lv_obj_set_width(target, intValue);
                    } else if (specific->property == BASIC_HEIGHT) {
                        lv_obj_set_height(target, intValue);
                    } else if (specific->property == BASIC_OPACITY) {
                        lv_obj_set_style_opa(target, intValue, 0);
                    } else if (specific->property == DROPDOWN_SELECTED) {
                        lv_dropdown_set_selected(target, intValue);
                    } else if (specific->property == IMAGE_ANGLE) {
                        lv_img_set_angle(target, intValue);
                    } else if (specific->property == IMAGE_ZOOM) {
                        lv_img_set_zoom(target, intValue);
                    } else if (specific->property == ROLLER_SELECTED) {
                        lv_roller_set_selected(target, intValue, specific->animated ? LV_ANIM_ON : LV_ANIM_OFF);
                    } else if (specific->property == SLIDER_VALUE) {
                        lv_slider_set_value(target, intValue, specific->animated ? LV_ANIM_ON : LV_ANIM_OFF);
                    }
                }
            }
        }
    }

    propagateValueThroughSeqout(flowState, componentIndex);
}

} // namespace flow
} // namespace eez


#else

namespace eez {
namespace flow {

void executeLVGLComponent(FlowState *flowState, unsigned componentIndex) {
    throwError(flowState, componentIndex, "Not implemented");
}

} // namespace flow
} // namespace eez

#endif
