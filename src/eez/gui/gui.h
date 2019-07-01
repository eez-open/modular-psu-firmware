/*
 * EEZ Generic Firmware
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

#include <eez/gui/assets.h>
#include <eez/gui/data.h>
#include <eez/gui/page.h>
#include <eez/modules/mcu/display.h>

#define INTERNAL_PAGE_ID_NONE -1
#define INTERNAL_PAGE_ID_SELECT_FROM_ENUM -2

enum InternalActionsEnum {
    ACTION_ID_INTERNAL_SELECT_ENUM_ITEM = -1,
    ACTION_ID_INTERNAL_SHOW_PREVIOUS_PAGE = -2
};

namespace eez {

namespace gui {

bool onSystemStateChanged();

////////////////////////////////////////////////////////////////////////////////

extern bool g_isBlinkTime;

inline bool isBlinkTime() {
    return g_isBlinkTime;
}

////////////////////////////////////////////////////////////////////////////////

WidgetCursor &getFoundWidgetAtDown();
bool isActiveWidget(const WidgetCursor &widgetCursor);
uint32_t getShowPageTime();
void setShowPageTime(uint32_t time);
void refreshScreen();
void showPage(int pageId);
void pushPage(int pageId, Page *page = 0);
void popPage();
void replacePage(int pageId, Page *page = nullptr);
int getActivePageId();
Page *getActivePage();
int getPreviousPageId();
Page *getPreviousPage();
bool isPageActiveOrOnStack(int pageId);
void pushSelectFromEnumPage(const data::EnumItem *enumDefinition, uint8_t currentValue,
                            bool (*disabledCallback)(uint8_t value), void (*onSet)(uint8_t));
void pushSelectFromEnumPage(void(*enumDefinitionFunc)(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value),
	                        uint8_t currentValue, bool(*disabledCallback)(uint8_t value), void(*onSet)(uint8_t)); 
void executeAction(int actionId);

int transformStyle(const Widget *widget);

////////////////////////////////////////////////////////////////////////////////

void showWelcomePage();
void showStandbyPage();
void showEnteringStandbyPage();

void standBy();
void turnDisplayOff();
void reset();

void lockFrontPanel();
void unlockFrontPanel();
bool isFrontPanelLocked();

struct AnimationState {
    bool enabled;
    bool skipOneFrame;
    uint32_t startTime;
    uint32_t duration;
    bool direction;
    Rect srcRect;
    Rect dstRect;
};

extern AnimationState g_animationState;

void animateOpen(const Rect &srcRect, const Rect &dstRect);
void animateClose(const Rect &srcRect, const Rect &dstRect);

////////////////////////////////////////////////////////////////////////////////

int getCurrentStateBufferIndex();
uint32_t getCurrentStateBufferSize(const WidgetCursor &widgetCursor);

} // namespace gui
} // namespace eez
