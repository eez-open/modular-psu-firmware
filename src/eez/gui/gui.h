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

#include <cmsis_os.h>

#include <eez/gui/assets.h>
#include <eez/gui/dialogs.h>
#include <eez/gui/page.h>

#include <eez/modules/mcu/display.h>

#define INTERNAL_PAGE_ID_NONE -1
#define INTERNAL_PAGE_ID_SELECT_FROM_ENUM -2
#define INTERNAL_PAGE_ID_TOAST_MESSAGE -3
#define INTERNAL_PAGE_ID_MENU_WITH_BUTTONS -4

enum InternalActionsEnum {
    ACTION_ID_INTERNAL_SELECT_ENUM_ITEM = -1,
    ACTION_ID_INTERNAL_DIALOG_CLOSE = -2,
    ACTION_ID_INTERNAL_TOAST_ACTION = -3,
    ACTION_ID_INTERNAL_TOAST_ACTION_WITHOUT_PARAM = -4,
    ACTION_ID_INTERNAL_MENU_WITH_BUTTONS = -5
};

namespace eez {

namespace gui {

////////////////////////////////////////////////////////////////////////////////

void startThread();

extern osThreadId g_guiTaskHandle;
extern osMessageQId g_guiMessageQueueId;

#define GUI_QUEUE_MESSAGE_TYPE_LISTS_PAGE_IMPORT_LIST_FINISHED 1
#define GUI_QUEUE_MESSAGE_TYPE_LISTS_PAGE_EXPORT_LIST_FINISHED 2
#define GUI_QUEUE_MESSAGE_TYPE_USER_PROFILES_PAGE_ASYNC_OPERATION_FINISHED 3

#define GUI_QUEUE_MESSAGE(type, param) ((((uint32_t)(uint16_t)(int16_t)param) << 8) | (type))
#define GUI_QUEUE_MESSAGE_TYPE(message) ((message) & 0xFF)
#define GUI_QUEUE_MESSAGE_PARAM(param) ((int16_t)(message >> 8))

void onGuiQueueMessageHook(uint8_t type, int16_t param);

////////////////////////////////////////////////////////////////////////////////

extern bool g_isBlinkTime;

////////////////////////////////////////////////////////////////////////////////

void stateManagmentHook();
WidgetCursor &getFoundWidgetAtDown();
void clearFoundWidgetAtDown();
bool isActiveWidget(const WidgetCursor &widgetCursor);
bool isFocusWidget(const WidgetCursor &widgetCursor);
uint32_t getShowPageTime();
void setShowPageTime(uint32_t time);
void refreshScreen();
void showPage(int pageId);
void pushPage(int pageId, Page *page = 0);
void popPage();
void replacePage(int pageId, Page *page = nullptr);
int getActivePageId();
Page *getActivePage();
Page *getPage(int pageId);
bool isPageOnStack(int pageId);
void pushSelectFromEnumPage(const data::EnumItem *enumDefinition, uint16_t currentValue,
                            bool (*disabledCallback)(uint16_t value), void (*onSet)(uint16_t), bool smallFont = false);
void pushSelectFromEnumPage(void(*enumDefinitionFunc)(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value),
	                        uint16_t currentValue, bool(*disabledCallback)(uint16_t value), void(*onSet)(uint16_t), bool smallFont = false); 
bool isPageInternal(int pageId);
void executeAction(int actionId);

////////////////////////////////////////////////////////////////////////////////

void standBy();
void reset();

enum Buffer {
    BUFFER_OLD,
    BUFFER_NEW,
    BUFFER_SOLID_COLOR
};

struct AnimationState {
    bool enabled;
    uint32_t startTime;
    float duration;
    Buffer startBuffer;
    void (*callback)(float t, void *bufferOld, void *bufferNew, void *bufferDst);
    float (*easingRects)(float x, float x1, float y1, float x2, float y2);
    float (*easingOpacity)(float x, float x1, float y1, float x2, float y2);
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
void animateRects(Buffer startBuffer, int numRects, float duration = -1);

float getDefaultAnimationDurationHook();

////////////////////////////////////////////////////////////////////////////////

int getCurrentStateBufferIndex();
uint32_t getCurrentStateBufferSize(const WidgetCursor &widgetCursor);

Page *getPageFromIdHook(int pageId);

} // namespace gui
} // namespace eez

#include <eez/gui/app_context.h>
#include <eez/gui/update.h>
#include <eez/gui/touch.h>
#include <eez/gui/overlay.h>
#include <eez/gui/font.h>
#include <eez/gui/draw.h>
