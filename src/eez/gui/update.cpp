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

namespace eez {
namespace gui {

static uint8_t *g_stateBuffer = GUI_STATE_BUFFER;
static WidgetState *g_previousState;
WidgetState *g_currentState;
WidgetState *g_currentStateEnd;
static bool g_refreshScreen;

int getCurrentStateBufferIndex() {
    return (uint8_t *)g_currentState == g_stateBuffer ? 0 : 1;
}

size_t getCurrentStateBufferSize(const WidgetCursor &widgetCursor) {
    return (uint8_t *)widgetCursor.currentState - (uint8_t *)g_currentState;
}

void refreshScreen() {
    g_refreshScreen = true;
}

void updateScreen() {
    if (g_refreshScreen) {
        g_refreshScreen = false;
        g_currentState = 0;
    }

    g_isActiveWidget = false;
    g_previousState = g_currentState;
    g_currentState = (WidgetState *)(g_stateBuffer + CONF_MAX_STATE_SIZE * (getCurrentStateBufferIndex() == 0 ? 1 : 0));

	WidgetCursor widgetCursor;
	widgetCursor.assets = g_mainAssets;
	widgetCursor.appContext = &getRootAppContext();
	widgetCursor.previousState = g_previousState;
	widgetCursor.currentState = g_currentState;

    widgetCursor.appContext->updateAppView(widgetCursor);

    g_currentStateEnd = widgetCursor.currentState;
}

} // namespace gui
} // namespace eez
