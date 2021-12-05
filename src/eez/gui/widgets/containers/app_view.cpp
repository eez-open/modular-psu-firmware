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

#include <eez/gui/gui.h>
#include <eez/gui/widgets/containers/app_view.h>

namespace eez {
namespace gui {

void AppViewWidgetState::draw(WidgetState *previousState) {
    AppContext *appContext;
    if (widgetCursor.widget->data != DATA_ID_NONE) {
        Value appContextValue;
        DATA_OPERATION_FUNCTION(widgetCursor.widget->data, DATA_OPERATION_GET, widgetCursor, appContextValue);
        appContext = appContextValue.getAppContext();
    } else {
        appContext = widgetCursor.appContext;
    }

    appContext->rect.x = widgetCursor.x;
    appContext->rect.y = widgetCursor.y;
    appContext->rect.w = widgetCursor.widget->w;
    appContext->rect.h = widgetCursor.widget->h;

    WidgetState *childCurrentState = this;
	WidgetState *childPreviousState = previousState;
    WidgetCursor childWidgetCursor = getFirstChildWidgetCursor(widgetCursor, childCurrentState, childPreviousState);

    childWidgetCursor.appContext = appContext;

    if (appContext->getActivePageId() != PAGE_ID_NONE) {
        WidgetState *endOfContainerInPreviousState = 0;
        if (previousState) {
            endOfContainerInPreviousState = nextWidgetState(previousState);
		}

        for (int i = 0; i <= appContext->m_pageNavigationStackPointer; i++) {
			if (!appContext->isPageFullyCovered(i)) {
				appContext->updatePage(i, childWidgetCursor, childCurrentState, childPreviousState);

				if (childPreviousState) {
					childPreviousState = nextWidgetState(childPreviousState);
					if (childPreviousState > endOfContainerInPreviousState) {
						childPreviousState = 0;
					}
				}
				childCurrentState = nextWidgetState(childCurrentState);
			} else {
                appContext->m_pageNavigationStack[i].displayBufferIndex = -1;
            }
        }
    } else {
        enumNoneWidget(childWidgetCursor, childCurrentState, childPreviousState);
    }

    widgetStateSize = (uint8_t *)childCurrentState - (uint8_t *)this;
}

} // namespace gui
} // namespace eez