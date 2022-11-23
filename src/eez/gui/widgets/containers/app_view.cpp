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

#if EEZ_OPTION_GUI || !defined(EEZ_OPTION_GUI)

#include <assert.h>

#include <eez/gui/gui.h>
#include <eez/gui/widgets/containers/app_view.h>

namespace eez {
namespace gui {

bool AppViewWidgetState::updateState() {
    WIDGET_STATE_START(Widget)

    if (widgetCursor.widget->data != DATA_ID_NONE) {
        Value appContextValue = get(widgetCursor, widgetCursor.widget->data);
        appContext = (AppContext *)appContextValue.getVoidPointer();;
    } else {
        appContext = widgetCursor.appContext;
    }

    WIDGET_STATE_END()
}

void AppViewWidgetState::enumChildren() {
    WidgetCursor &widgetCursor = g_widgetCursor;

    appContext->rect.x = widgetCursor.x;
    appContext->rect.y = widgetCursor.y;
    appContext->rect.w = widgetCursor.w;
    appContext->rect.h = widgetCursor.h;

	auto savedAppContext = widgetCursor.appContext;
    widgetCursor.appContext = appContext;

    if (appContext->getActivePageId() != PAGE_ID_NONE) {
        for (int i = 0; i <= appContext->m_pageNavigationStackPointer; i++) {
			if (!appContext->isPageFullyCovered(i)) {
				appContext->updatePage(i, widgetCursor);
			} else {
                appContext->m_pageNavigationStack[i].displayBufferIndex = -1;
            }
        }
    } else {
        enumNoneWidget();
    }

	widgetCursor.appContext = savedAppContext;
}

} // namespace gui
} // namespace eez

#endif // EEZ_OPTION_GUI || !defined(EEZ_OPTION_GUI)
