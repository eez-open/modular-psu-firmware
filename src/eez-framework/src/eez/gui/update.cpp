/*
* EEZ Generic Firmware
* Copyright (C) 2018-present, Envox d.o.o.
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

#include <stdio.h>

#include <eez/core/debug.h>
#include <eez/core/os.h>

#include <eez/gui/gui.h>
#include <eez/gui/widgets/containers/app_view.h>

namespace eez {
namespace gui {

static bool g_refreshScreen;
static Widget *g_rootWidget;

WidgetState *g_widgetStateStart;
WidgetState *g_widgetStateEnd;

bool g_widgetStateStructureChanged;

WidgetCursor g_widgetCursor;

void refreshScreen() {
	g_refreshScreen = true;
}

void updateScreen() {
	if (!g_rootWidget) {
		static AppViewWidget g_rootAppViewWidget;

		g_rootWidget = &g_rootAppViewWidget;

		g_rootWidget->type = WIDGET_TYPE_APP_VIEW;
		g_rootWidget->data = DATA_ID_NONE;
		g_rootWidget->action = ACTION_ID_NONE;
		g_rootWidget->x = 0;
		g_rootWidget->y = 0;
		g_rootWidget->width = display::getDisplayWidth();
		g_rootWidget->height = display::getDisplayHeight();
		g_rootWidget->style = 0;
	}

	if (g_refreshScreen) {
		g_refreshScreen = false;

		if (g_widgetStateStart) {
			// invalidate widget states
			freeWidgetStates(g_widgetStateStart);
			g_widgetStateStart = nullptr;
		}

        // auto bufferIndex = display::beginBufferRendering();
        // display::setColor(0, 0, 0);
        // display::fillRect(0, 0, display::getDisplayWidth(), display::getDisplayHeight());
        // display::endBufferRendering(bufferIndex, 0, 0, display::getDisplayWidth(), display::getDisplayHeight(), false, 255, 0, 0, nullptr);
	}

	bool hasPreviousState = g_widgetStateStart != nullptr;
	g_widgetStateStart = (WidgetState *)GUI_STATE_BUFFER;

    g_isActiveWidget = false;

	g_widgetCursor = WidgetCursor();
	g_widgetCursor.assets = g_mainAssets;
	g_widgetCursor.appContext = getRootAppContext();
	g_widgetCursor.widget = g_rootWidget;
	g_widgetCursor.currentState = g_widgetStateStart;
	g_widgetCursor.refreshed = !hasPreviousState;
	g_widgetCursor.hasPreviousState = hasPreviousState;

	g_foundWidgetAtDownInvalid = false;

    g_widgetCursor.x = 0;
    g_widgetCursor.y = 0;
    g_widgetCursor.w = g_rootWidget->width;
    g_widgetCursor.h = g_rootWidget->height;

    if (g_mainAssets->assetsType != ASSETS_TYPE_DASHBOARD) {
        enumWidget();
    }

	g_widgetStateEnd = g_widgetCursor.currentState;
	g_widgetStateStructureChanged = !g_widgetCursor.hasPreviousState;

	if (g_foundWidgetAtDownInvalid) {
		WidgetCursor widgetCursor;
		setFoundWidgetAtDown(widgetCursor);
	}
}

void enumRootWidget() {
	if (!g_widgetStateStart  || g_refreshScreen) {
		return;
	}

    g_isActiveWidget = false;

	g_widgetCursor = WidgetCursor();
	g_widgetCursor.assets = g_mainAssets;
	g_widgetCursor.appContext = getRootAppContext();
	g_widgetCursor.widget = g_rootWidget;
	g_widgetCursor.currentState = g_widgetStateStart;
	g_widgetCursor.refreshed = false;
	g_widgetCursor.hasPreviousState = true;

    g_widgetCursor.x = 0;
    g_widgetCursor.y = 0;
    g_widgetCursor.w = g_rootWidget->width;
    g_widgetCursor.h = g_rootWidget->height;

    enumWidget();
}

} // namespace gui
} // namespace eez

#endif // EEZ_OPTION_GUI
