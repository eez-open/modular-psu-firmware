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
    return AppContext::getLongTouchActionHook(widgetCursor);
}

////////////////////////////////////////////////////////////////////////////////

void data_main_app_view(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = Value(&psu::gui::g_psuAppContext);
    }
}

int getSlotView(int slotIndex, Cursor cursor) {
    auto &slot = *g_slots[slotIndex];
    
    if (slot.moduleType == MODULE_TYPE_DCP405) {
        return PAGE_ID_DIB_DCP405_SIMULATOR_FRONT_PANEL_MASK;
    }
    
    if (slot.moduleType == MODULE_TYPE_DCM220) {
        return PAGE_ID_DIB_DCM220_SIMULATOR_FRONT_PANEL_MASK;
    }
    
    if (slot.moduleType == MODULE_TYPE_DCM224) {
        return PAGE_ID_DIB_DCM224_SIMULATOR_FRONT_PANEL_MASK;
    }
    
    if (slot.moduleType == MODULE_TYPE_DIB_MIO168) {
        return PAGE_ID_DIB_MIO168_SIMULATOR_FRONT_PANEL_MASK;
    }
    
    if (slot.moduleType == MODULE_TYPE_DIB_PREL6) {
        return PAGE_ID_DIB_PREL6_SIMULATOR_FRONT_PANEL_MASK;
    }
    
    if (slot.moduleType == MODULE_TYPE_DIB_SMX46) {
        return PAGE_ID_DIB_SMX46_SIMULATOR_FRONT_PANEL_MASK;
    }

    if (slot.moduleType == MODULE_TYPE_DIB_MUX14D) {
        return PAGE_ID_DIB_MUX14D_SIMULATOR_FRONT_PANEL_MASK;
    }

    return PAGE_ID_FRONT_PANEL_EMPTY_SLOT;
}

void data_front_panel_slot1_view(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    auto cursor = widgetCursor.cursor;
    if (operation == DATA_OPERATION_GET) {
        value = getSlotView(0, cursor);
    }
}

void data_front_panel_slot2_view(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    auto cursor = widgetCursor.cursor;
    if (operation == DATA_OPERATION_GET) {
        value = getSlotView(1, cursor);
    }
}

void data_front_panel_slot3_view(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    auto cursor = widgetCursor.cursor;
    if (operation == DATA_OPERATION_GET) {
        value = getSlotView(2, cursor);
    }
}

} // namespace gui
} // namespace eez

#endif