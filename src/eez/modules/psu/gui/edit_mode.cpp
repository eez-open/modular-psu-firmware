/* / mcu / sound.h
* EEZ PSU Firmware
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

#include <eez/modules/psu/psu.h>

#include <stdio.h>
#include <string.h>

#include <eez/sound.h>

#include <eez/gui/gui.h>

#include <eez/modules/psu/calibration.h>
#include <eez/modules/psu/channel_dispatcher.h>
#if OPTION_SD_CARD
#include <eez/modules/psu/dlog_record.h>
#endif

#include <eez/modules/psu/gui/data.h>
#include <eez/modules/psu/gui/edit_mode.h>
#include <eez/modules/psu/gui/edit_mode_keypad.h>
#include <eez/modules/psu/gui/edit_mode_slider.h>
#include <eez/modules/psu/gui/edit_mode_step.h>
#include <eez/modules/psu/gui/numeric_keypad.h>
#include <eez/modules/psu/gui/psu.h>

namespace eez {
namespace psu {
namespace gui {
namespace edit_mode {

static int g_tabIndex = PAGE_ID_EDIT_MODE_KEYPAD;

static data::Value g_editValue;
static data::Value g_undoValue;
static data::Value g_minValue;
static data::Value g_maxValue;
static bool g_isInteractiveMode = true;

////////////////////////////////////////////////////////////////////////////////

bool isActive() {
    return getActivePageId() == PAGE_ID_EDIT_MODE_KEYPAD ||
           getActivePageId() == PAGE_ID_EDIT_MODE_STEP ||
           getActivePageId() == PAGE_ID_EDIT_MODE_SLIDER;
}

void initEditValue() {
    g_editValue = data::getEditValue(g_focusCursor, g_focusDataId);
    g_undoValue = g_editValue;
}

void enter(int tabIndex) {
#if OPTION_ENCODER
    if (!isActive()) {
        if (!g_appContext->isFocusWidget(getFoundWidgetAtDown()) || g_focusEditValue.getType() != VALUE_TYPE_NONE) {
            setFocusCursor(getFoundWidgetAtDown().cursor, getFoundWidgetAtDown().widget->data);
            return;
        }
    }
#endif

    if (tabIndex == -1) {
        gui::selectChannel();
        data::Cursor newDataCursor = getFoundWidgetAtDown().cursor;
        int newDataId = getFoundWidgetAtDown().widget->data;
        setFocusCursor(newDataCursor, newDataId);
        update();

        if (!isActive()) {
            pushPage(g_tabIndex);
        }
    } else {
        g_tabIndex = tabIndex;
        replacePage(g_tabIndex);
    }

    if (g_tabIndex == PAGE_ID_EDIT_MODE_KEYPAD) {
        Cursor cursor(g_focusCursor.i);
        edit_mode_keypad::enter(isChannelData(cursor, g_focusDataId) ? g_focusCursor.i : -1, g_editValue, g_minValue, g_maxValue);
    } else {
        edit_mode_keypad::exit();
    }
}

void update() {
    initEditValue();
    g_minValue = data::getMin(g_focusCursor, g_focusDataId);
    g_maxValue = data::getMax(g_focusCursor, g_focusDataId);
    if (edit_mode_keypad::g_keypad) {
        edit_mode_keypad::g_keypad->m_options.editValueUnit = g_editValue.getUnit();
    }
}

void exit() {
    edit_mode_keypad::exit();
    popPage();
}

void nonInteractiveSet() {
    int16_t error;
    if (!data::set(g_focusCursor, g_focusDataId, g_editValue, &error)) {
        psuErrorMessage(g_focusCursor, data::MakeScpiErrorValue(error));
    } else {
        popPage();
    }
}

void nonInteractiveDiscard() {
    g_editValue = g_undoValue;
    data::set(g_focusCursor, g_focusDataId, g_undoValue, 0);
}

bool isInteractiveMode() {
    return g_isInteractiveMode;
}

void toggleInteractiveMode() {
    g_isInteractiveMode = !g_isInteractiveMode;
    initEditValue();
}

const data::Value &getEditValue() {
    return g_editValue;
}

data::Value getCurrentValue() {
    return data::get(g_focusCursor, g_focusDataId);
}

const data::Value &getMin() {
    return g_minValue;
}

const data::Value &getMax() {
    return g_maxValue;
}

Unit getUnit() {
    return g_editValue.getUnit();
}

bool setValue(float floatValue) {
    Cursor cursor(g_focusCursor.i);
    if (isChannelData(cursor, g_focusDataId)) {
        floatValue = channel_dispatcher::roundChannelValue(Channel::get(g_focusCursor.i), getUnit(), floatValue);
    }

    data::Value value = MakeValue(floatValue, getUnit());
    if (g_isInteractiveMode || g_tabIndex == PAGE_ID_EDIT_MODE_KEYPAD) {
        int16_t error;
        if (!data::set(g_focusCursor, g_focusDataId, value, &error)) {
            psuErrorMessage(g_focusCursor, data::MakeScpiErrorValue(error));
            return false;
        }
    }
    g_editValue = value;
    return true;
}

#define NUM_PARTS 15

void getInfoText(char *infoText, int count) {
    Cursor cursor(g_focusCursor.i);

    const char *dataName = getName(cursor, g_focusDataId);
    if (!dataName) {
        dataName = "Unknown";
    }
    const char *unitName = getUnitName(getUnit(cursor, g_focusDataId));

    float minValue = data::getMin(cursor, g_focusDataId).getFloat();
    float maxValue = (g_focusDataId == DATA_ID_CHANNEL_U_EDIT || g_focusDataId == DATA_ID_CHANNEL_I_EDIT) ?
        data::getLimit(cursor, g_focusDataId).getFloat() : data::getMax(cursor, g_focusDataId).getFloat();

    if (isChannelData(cursor, g_focusDataId)) {
        Channel& channel = Channel::get(g_focusCursor.i);
        if ((channel.channelIndex < 2 && channel_dispatcher::getCouplingType() != channel_dispatcher::COUPLING_TYPE_NONE) || channel.flags.trackingEnabled) {
            strcpy(infoText, "Set ");
        } else {
            sprintf(infoText, "Set Ch%d ", g_focusCursor.i + 1);
        }
        
        strcat(infoText, dataName);
    } else {
        strcat(infoText, dataName);
    }

    strcat(infoText, " [");
    strcatFloat(infoText, minValue);
    strcat(infoText, "-");
    strcatFloat(infoText, maxValue);
    strcat(infoText, unitName);
    strcat(infoText, "]");
}

} // namespace edit_mode
} // namespace gui
} // namespace psu
} // namespace eez

#endif
