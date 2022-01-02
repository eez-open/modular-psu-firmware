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

#include <assert.h>
#include <math.h>
#include <string.h>

#include <eez/conf.h>
#include <eez/core/os.h>
#include <eez/core/debug.h>

#if OPTION_MOUSE
#include <eez/core/mouse.h>
#endif

#include <eez/core/sound.h>
#include <eez/core/util.h>

#include <eez/gui/gui.h>

#include <eez/flow/flow.h>

#define CONF_GUI_BLINK_TIME 400 // 400ms

namespace eez {
namespace gui {

bool g_isBlinkTime;
static bool g_wasBlinkTime;

////////////////////////////////////////////////////////////////////////////////

void guiInit() {
    loadMainAssets(assets, sizeof(assets));

    display::init();

    if (g_mainAssets->flowDefinition) {
        flow::start(g_mainAssets);
    }
}

void guiTick() {
    g_wasBlinkTime = g_isBlinkTime;
    g_isBlinkTime = (millis() % (2 * CONF_GUI_BLINK_TIME)) > CONF_GUI_BLINK_TIME;

    if (g_mainAssets->flowDefinition) {
        flow::tick();
    }

	g_hooks.stateManagment();
}

////////////////////////////////////////////////////////////////////////////////

bool isInternalAction(int actionId) {
    return actionId > FIRST_INTERNAL_ACTION_ID;
}

void executeAction(const WidgetCursor &widgetCursor, int actionId) {
    if (actionId == ACTION_ID_NONE) {
        return;
    }

    sound::playClick();

	g_hooks.executeActionThread();

    if (isInternalAction(actionId)) {
        executeInternalAction(actionId);
    } else {
        if (actionId >= 0) {
            g_actionExecFunctions[actionId]();
        } else {
			g_hooks.executeExternalAction(widgetCursor, actionId);
        }
    }
}

void executeActionFunction(int actionId) {
	assert(actionId > 0);
	g_actionExecFunctions[actionId]();
}

void popPage() {
	getAppContextFromId(APP_CONTEXT_ID_DEVICE)->popPage();
}

// from InternalActionsEnum
static ActionExecFunc g_internalActionExecFunctions[] = {
    0,
    // ACTION_ID_INTERNAL_SELECT_ENUM_ITEM
	SelectFromEnumPage::selectEnumItem,

    // ACTION_ID_INTERNAL_DIALOG_CLOSE
    popPage,

    // ACTION_ID_INTERNAL_TOAST_ACTION
    ToastMessagePage::executeAction,

    // ACTION_ID_INTERNAL_TOAST_ACTION_WITHOUT_PARAM
    ToastMessagePage::executeActionWithoutParam,

    // ACTION_ID_INTERNAL_MENU_WITH_BUTTONS
    MenuWithButtonsPage::executeAction
};

void executeInternalAction(int actionId) {
    g_internalActionExecFunctions[actionId - FIRST_INTERNAL_ACTION_ID]();
}

bool isFocusWidget(const WidgetCursor &widgetCursor) {
    return widgetCursor.appContext->isFocusWidget(widgetCursor);
}

} // namespace gui
} // namespace eez
