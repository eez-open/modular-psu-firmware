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

#if OPTION_DISPLAY

#include <eez/debug.h>

#include <eez/gui/gui.h>

namespace eez {
namespace gui {

static uint8_t g_stateBuffer[2][CONF_MAX_STATE_SIZE];
static WidgetState *g_previousState;
static WidgetState *g_currentState;
static bool g_refreshScreen;

int getCurrentStateBufferIndex() {
    return (uint8_t *)g_currentState == &g_stateBuffer[0][0] ? 0 : 1;
}

uint32_t getCurrentStateBufferSize(const WidgetCursor &widgetCursor) {
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
    g_currentState = (WidgetState *)(&g_stateBuffer[getCurrentStateBufferIndex() == 0 ? 1 : 0][0]);

	WidgetCursor widgetCursor;
	widgetCursor.appContext = &getRootAppContext();
	widgetCursor.previousState = g_previousState;
	widgetCursor.currentState = g_currentState;

    widgetCursor.appContext->updateAppView(widgetCursor);
}

} // namespace gui
} // namespace eez

#endif
