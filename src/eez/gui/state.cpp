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

#include <eez/gui/update.h>

#include <eez/gui/app_context.h>
#include <eez/gui/data.h>

using namespace eez::gui::data;

namespace eez {
namespace gui {

void callStateManagmentForAppView(const WidgetCursor &widgetCursor) {
    const Widget *widget = widgetCursor.widget;
    if (widget->type == WIDGET_TYPE_APP_VIEW) {
        Value appContextValue;
        g_dataOperationsFunctions[widget->data](data::DATA_OPERATION_GET,
                                                (Cursor &)widgetCursor.cursor, appContextValue);
        AppContext *appContext = appContextValue.getAppContext();

        AppContext *saved = g_appContext;
        g_appContext = appContext;

        appContext->stateManagment();

        g_appContext = saved;
    }
}

void stateManagment() {
    getRootAppContext().stateManagment();

    // call stateManagment from embedded AppView's
    enumWidgets(callStateManagmentForAppView);
}

} // namespace gui
} // namespace eez
