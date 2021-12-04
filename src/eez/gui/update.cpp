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

#include <eez/debug.h>

#include <eez/gui/gui.h>
#include <eez/gui/widgets/app_view.h>

namespace eez {
namespace gui {

static uint8_t *g_stateBuffer = GUI_STATE_BUFFER;
static WidgetState *g_previousState;
WidgetState *g_currentState;
static bool g_refreshScreen;

int getCurrentStateBufferIndex() {
    return (uint8_t *)g_currentState == g_stateBuffer ? 0 : 1;
}

void refreshScreen() {
    g_refreshScreen = true;
}

void updateScreen() {
    if (g_refreshScreen) {
        g_refreshScreen = false;

		if (g_currentState != nullptr) {
			freeWidgetStates(g_currentState);
			g_currentState = nullptr;
		}

		display::freeAllBuffers();
    }

    g_isActiveWidget = false;

	if (g_previousState) {
		freeWidgetStates(g_previousState);
	}

    g_previousState = g_currentState;
    g_currentState = (WidgetState *)(g_stateBuffer + CONF_MAX_STATE_SIZE * (getCurrentStateBufferIndex() == 0 ? 1 : 0));;

	static AppViewWidget widget;
	widget.type = WIDGET_TYPE_APP_VIEW;
	widget.data = DATA_ID_NONE;
	widget.action = ACTION_ID_NONE;
	widget.x = 0;
	widget.y = 0;
	widget.w = display::getDisplayWidth();
	widget.h = display::getDisplayHeight();
	widget.style = 0;

	WidgetCursor widgetCursor;
	widgetCursor.assets = g_mainAssets;
	widgetCursor.appContext = &getRootAppContext();
	widgetCursor.widget = &widget;

    enumWidget(widgetCursor, g_currentState, g_previousState);
}

} // namespace gui
} // namespace eez
