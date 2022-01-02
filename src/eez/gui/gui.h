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

#include <eez/gui_conf.h>
#include <eez/gui/assets.h>
#include <eez/gui/display.h>

enum {
    FIRST_INTERNAL_PAGE_ID = 32000,
    INTERNAL_PAGE_ID_SELECT_FROM_ENUM,
    INTERNAL_PAGE_ID_TOAST_MESSAGE,
    INTERNAL_PAGE_ID_MENU_WITH_BUTTONS
};

enum InternalActionsEnum {
    FIRST_INTERNAL_ACTION_ID = 32000,
    ACTION_ID_INTERNAL_SELECT_ENUM_ITEM,
    ACTION_ID_INTERNAL_DIALOG_CLOSE,
    ACTION_ID_INTERNAL_TOAST_ACTION,
    ACTION_ID_INTERNAL_TOAST_ACTION_WITHOUT_PARAM,
    ACTION_ID_INTERNAL_MENU_WITH_BUTTONS
};

namespace eez {
namespace gui {

////////////////////////////////////////////////////////////////////////////////

extern WidgetCursor g_activeWidget;
extern bool g_isBlinkTime;

////////////////////////////////////////////////////////////////////////////////

void guiInit();
void guiTick();

WidgetCursor &getFoundWidgetAtDown();
void setFoundWidgetAtDown(WidgetCursor &widgetCursor);
void clearFoundWidgetAtDown();
bool isFocusWidget(const WidgetCursor &widgetCursor);
void refreshScreen();
inline bool isPageInternal(int pageId) { return pageId > FIRST_INTERNAL_PAGE_ID; }

inline int getWidgetAction(const WidgetCursor &widgetCursor) {
    if (widgetCursor.widget->type == WIDGET_TYPE_INPUT) {
		if (widgetCursor.widget->action == ACTION_ID_NONE) {
			return EEZ_CONF_ACTION_ID_EDIT;
		}
    }
	return widgetCursor.widget->action;
}

void executeAction(const WidgetCursor &widgetCursor, int actionId);
void executeActionFunction(int actionId);
void executeInternalAction(int actionId);

int16_t getAppContextId(AppContext *pAppContext);
AppContext *getAppContextFromId(int16_t id);

extern const char *g_discardMessage;

} // namespace gui
} // namespace eez

#include <eez/gui/app_context.h>
#include <eez/gui/update.h>
#include <eez/gui/overlay.h>
#include <eez/gui/font.h>
#include <eez/gui/draw.h>
#include <eez/gui/touch.h>
#include <eez/gui/page.h>
#include <eez/gui/hooks.h>

#define DATA_OPERATION_FUNCTION(id, operation, widgetCursor, value) (id >= 0 ? g_dataOperationsFunctions[id](operation, widgetCursor, value) : g_hooks.externalData(id, operation, widgetCursor, value))
