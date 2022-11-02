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

#include <eez/flow/private.h>

namespace eez {
namespace flow {

////////////////////////////////////////////////////////////////////////////////

enum LVGL_ACTIONS {
    CHANGE_SCREEN,
    PLAY_ANIMATION,
    SET_PROPERTY
};

struct LVGLComponent_ActionType {
    uint32_t action;
};

////////////////////////////////////////////////////////////////////////////////

struct LVGLComponent_ChangeScreen_ActionType : public LVGLComponent_ActionType {
    int32_t screen;
    uint32_t fadeMode;
    uint32_t speed;
    uint32_t delay;
};

////////////////////////////////////////////////////////////////////////////////

#define ANIMATION_PROPERTY_POSITION_X 0
#define ANIMATION_PROPERTY_POSITION_Y 1
#define ANIMATION_PROPERTY_WIDTH 2
#define ANIMATION_PROPERTY_HEIGHT 3
#define ANIMATION_PROPERTY_OPACITY 4
#define ANIMATION_PROPERTY_IMAGE_ANGLE 5
#define ANIMATION_PROPERTY_IMAGE_ZOOM

#define ANIMATION_ITEM_FLAG_RELATIVE (1 << 0)
#define ANIMATION_ITEM_FLAG_INSTANT (1 << 1)

#define ANIMATION_PATH_LINEAR 0
#define ANIMATION_PATH_EASE_IN 1
#define ANIMATION_PATH_EASE_OUT 2
#define ANIMATION_PATH_EASE_IN_OUT 3
#define ANIMATION_PATH_OVERSHOOT 4
#define ANIMATION_PATH_BOUNCE 5

struct LVGLComponent_PlayAnimation_ActionType : public LVGLComponent_ActionType {
    int32_t target;
    uint32_t property;
    int32_t start;
    int32_t end;
    uint32_t delay;
    uint32_t time;
    uint32_t flags;
    uint32_t path;
};

////////////////////////////////////////////////////////////////////////////////

struct LVGLComponent_SetProperty_ActionType : public LVGLComponent_ActionType {
    int32_t target;
    uint32_t property;
    AssetsPtr<uint8_t> value;
    uint32_t animated;
};

struct LVGLComponent : public Component {
    ListOfAssetsPtr<LVGLComponent_ActionType> actions;
};

////////////////////////////////////////////////////////////////////////////////

} // flow
} // eez
