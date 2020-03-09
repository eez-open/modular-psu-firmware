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

#if OPTION_DISPLAY

#include <eez/platform/simulator/front_panel.h>

#include <eez/gui/gui.h>

#include <eez/modules/psu/psu.h>
#include <eez/modules/psu/gui/psu.h>

namespace eez {
namespace gui {

FrontPanelAppContext g_frontPanelAppContext;

////////////////////////////////////////////////////////////////////////////////

AppContext &getRootAppContext() {
    return g_frontPanelAppContext;
}

////////////////////////////////////////////////////////////////////////////////

void FrontPanelAppContext::stateManagment() {
    if (!isPageOnStack(PAGE_ID_FRONT_PANEL)) {
        showPage(PAGE_ID_FRONT_PANEL);
    }
}

int FrontPanelAppContext::getMainPageId() {
    return PAGE_ID_FRONT_PANEL;
}

int FrontPanelAppContext::getLongTouchActionHook(const WidgetCursor &widgetCursor) {
    if (widgetCursor.widget->action == ACTION_ID_USER_SWITCH_CLICKED) {
        return ACTION_ID_SELECT_USER_SWITCH_ACTION;
    }
    return ACTION_ID_NONE;
}

////////////////////////////////////////////////////////////////////////////////

void data_main_app_view(DataOperationEnum operation, Cursor &cursor, Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        value = Value(&psu::gui::g_psuAppContext);
    }
}

int getSlotView(int channelIndex) {
    int slotIndex = psu::Channel::get(channelIndex).slotIndex;

    if (g_slots[slotIndex].moduleInfo->moduleType == MODULE_TYPE_DCP405) {
        return PAGE_ID_DCP405_FRONT_PANEL;
    } else if (g_slots[slotIndex].moduleInfo->moduleType == MODULE_TYPE_DCP405B) {
        return PAGE_ID_DCP405B_FRONT_PANEL;
    } else if (g_slots[slotIndex].moduleInfo->moduleType == MODULE_TYPE_DCM220) {
        return PAGE_ID_DCM220_FRONT_PANEL;
    } else {
        return PAGE_ID_FRONT_PANEL_EMPTY_SLOT;
    }
}

void data_front_panel_slot1_view(DataOperationEnum operation, Cursor &cursor, Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        value = getSlotView(cursor.i);
    }
}

void data_front_panel_slot2_view(DataOperationEnum operation, Cursor &cursor, Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        value = getSlotView(cursor.i);
    }
}

void data_front_panel_slot3_view(DataOperationEnum operation, Cursor &cursor, Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        value = getSlotView(cursor.i);
    }
}

} // namespace gui
} // namespace eez

#endif