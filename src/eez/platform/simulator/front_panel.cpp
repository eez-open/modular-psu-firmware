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

#include <eez/platform/simulator/front_panel.h>

#include <eez/apps/home/home.h>
#include <eez/gui/assets.h>
#include <eez/gui/document.h>

using namespace eez::gui;
using namespace eez::home;

namespace eez {
namespace gui {

using data::Cursor;
using data::DataOperationEnum;
using data::Value;
using namespace mcu::display;

FrontPanelAppContext g_frontPanelAppContext;

////////////////////////////////////////////////////////////////////////////////

AppContext &getRootAppContext() {
    return g_frontPanelAppContext;
}

////////////////////////////////////////////////////////////////////////////////

FrontPanelAppContext::FrontPanelAppContext() {
    showPage(PAGE_ID_FRONT_PANEL);
}

int FrontPanelAppContext::getMainPageId() {
    return PAGE_ID_FRONT_PANEL;
}

////////////////////////////////////////////////////////////////////////////////

void data_main_app_view(DataOperationEnum operation, Cursor &cursor, Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        value = Value(&g_homeAppContext);
    }
}

} // namespace gui
} // namespace eez

#endif