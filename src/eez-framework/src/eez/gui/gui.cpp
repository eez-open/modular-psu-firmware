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

#include <eez/conf-internal.h>

#if EEZ_OPTION_GUI

#include <assert.h>
#include <math.h>
#include <string.h>

#include <eez/core/assets.h>
#include <eez/core/os.h>
#include <eez/core/debug.h>
#include <eez/core/action.h>

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

void (*loadMainAssets)(const uint8_t* assets, uint32_t assetsSize) = eez::loadMainAssets;
Assets*& g_mainAssets = eez::g_mainAssets;

bool g_isBlinkTime;
static bool g_wasBlinkTime;

////////////////////////////////////////////////////////////////////////////////

void guiInit() {
    if (!g_isMainAssetsLoaded) {
        loadMainAssets(assets, sizeof(assets));
    }

    if (g_mainAssets->flowDefinition) {
        flow::start(g_mainAssets);
    }

    display::init();
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

void executeAction(const WidgetCursor &widgetCursor, int actionId, void *param) {
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
			g_hooks.executeExternalAction(widgetCursor, actionId, param);
        }
    }
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
    MenuWithButtonsPage::executeAction,

    // ACTION_ID_INTERNAL_QUESTION_PAGE_BUTTON
    QuestionPage::executeAction
};

void executeInternalAction(int actionId) {
    g_internalActionExecFunctions[actionId - FIRST_INTERNAL_ACTION_ID]();
}

bool isFocusWidget(const WidgetCursor &widgetCursor) {
    return widgetCursor.appContext->isFocusWidget(widgetCursor);
}

bool isExternalPageOnStack() {
	return getAppContextFromId(APP_CONTEXT_ID_DEVICE)->isExternalPageOnStack();
}

void removeExternalPagesFromTheStack() {
	return getAppContextFromId(APP_CONTEXT_ID_DEVICE)->removeExternalPagesFromTheStack();
}

struct OverrideStyleRule {
    int16_t fromStyle;
    int16_t toStyle;
};
static OverrideStyleRule g_overrideStyleRules[10];

void setOverrideStyleRule(int16_t fromStyle, int16_t toStyle) {
    for (size_t i = 0; i < sizeof(g_overrideStyleRules) / sizeof(OverrideStyleRule); i++) {
        if (g_overrideStyleRules[i].fromStyle == STYLE_ID_NONE) {
            g_overrideStyleRules[i].fromStyle = fromStyle;
            g_overrideStyleRules[i].toStyle = toStyle;
        } else if (g_overrideStyleRules[i].fromStyle == fromStyle) {
            g_overrideStyleRules[i].toStyle = toStyle;
            break;
        }
    }
}

int overrideStyle(const WidgetCursor &widgetCursor, int styleId) {
    if (g_overrideStyleRules[0].fromStyle != STYLE_ID_NONE) {
        for (size_t i = 0; i < sizeof(g_overrideStyleRules) / sizeof(OverrideStyleRule); i++) {
            if (g_overrideStyleRules[i].fromStyle == STYLE_ID_NONE) {
                break;
            }
            if (g_overrideStyleRules[i].fromStyle == styleId) {
                styleId = g_overrideStyleRules[i].toStyle;
                break;
            }
        }
    }
    if (g_hooks.overrideStyle) {
        return g_hooks.overrideStyle(widgetCursor, styleId);
    }
    return styleId;
}

} // namespace gui
} // namespace eez

#endif // EEZ_OPTION_GUI
