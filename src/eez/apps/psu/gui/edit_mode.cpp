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

#include <eez/apps/psu/psu.h>

#include <stdio.h>
#include <string.h>

#include <eez/apps/psu/calibration.h>
#include <eez/apps/psu/channel_dispatcher.h>
#include <eez/sound.h>

#include <eez/apps/psu/gui/data.h>
#include <eez/apps/psu/gui/edit_mode.h>
#include <eez/apps/psu/gui/edit_mode_keypad.h>
#include <eez/apps/psu/gui/edit_mode_slider.h>
#include <eez/apps/psu/gui/edit_mode_step.h>
#include <eez/apps/psu/gui/numeric_keypad.h>
#include <eez/apps/psu/gui/psu.h>
#include <eez/gui/app_context.h>

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

        gui::selectChannel();

        if (getFoundWidgetAtDown().widget->data == DATA_ID_CHANNEL_PROTECTION_OVP_LIMIT) {
            changeVoltageLimit(g_channel->index - 1);
            return;
        }

        if (getFoundWidgetAtDown().widget->data == DATA_ID_CHANNEL_PROTECTION_OCP_LIMIT) {
            changeCurrentLimit(g_channel->index - 1);
            return;
        }

        if (getFoundWidgetAtDown().widget->data == DATA_ID_CHANNEL_PROTECTION_OPP_LIMIT) {
            changePowerLimit(g_channel->index - 1);
            return;
        }

        if (getFoundWidgetAtDown().widget->data == DATA_ID_CHANNEL_PROTECTION_OPP_LEVEL) {
            changePowerTripLevel(g_channel->index - 1);
            return;
        }

        if (getFoundWidgetAtDown().widget->data == DATA_ID_CHANNEL_PROTECTION_OTP_LEVEL) {
            changeTemperatureTripLevel(g_channel->index - 1);
            return;
        }

        if (getFoundWidgetAtDown().widget->data == DATA_ID_CHANNEL_PROTECTION_OPP_DELAY) {
            changePowerTripDelay(g_channel->index - 1);
            return;
        }

        if (getFoundWidgetAtDown().widget->data == DATA_ID_CHANNEL_PROTECTION_OTP_DELAY) {
            changeTemperatureTripDelay(g_channel->index - 1);
            return;
        }
    }
#endif

    if (tabIndex == -1) {
        gui::selectChannel();
        data::Cursor newDataCursor = getFoundWidgetAtDown().cursor;
        if (channel_dispatcher::isCoupled() || channel_dispatcher::isTracked()) {
            newDataCursor.i = 0;
        }
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
        edit_mode_keypad::enter(Channel::get(g_focusCursor.i), g_editValue, g_minValue, g_maxValue);
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
        if (g_editValue.isMicro()) {
            edit_mode_keypad::g_keypad->switchToMicro();
        } else if (g_editValue.isMilli()) {
            edit_mode_keypad::g_keypad->switchToMilli();
        }
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

bool setValue(float value_) {
    data::Value value = MakeValue(value_, getUnit(), g_focusCursor.i);
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

int getInfoTextPartIndex(data::Cursor &cursor, uint16_t dataId) {
    int dataIdIndex;
    
    if (dataId == DATA_ID_CHANNEL_U_SET || dataId == DATA_ID_CHANNEL_U_EDIT) {
        dataIdIndex = 0;
    } else if (dataId == DATA_ID_CHANNEL_I_SET || dataId == DATA_ID_CHANNEL_I_EDIT) {
        dataIdIndex = 1;
    } else if (dataId == DATA_ID_CHANNEL_PROTECTION_OVP_LIMIT) {
        dataIdIndex = 2;
    } else if (dataId == DATA_ID_CHANNEL_PROTECTION_OCP_LIMIT) {
        dataIdIndex = 3;
    } else if (dataId == DATA_ID_CHANNEL_PROTECTION_OPP_LIMIT) {
        dataIdIndex = 4;
    } else if (dataId == DATA_ID_CHANNEL_PROTECTION_OPP_LEVEL) {
        dataIdIndex = 5;
    } else if (dataId == DATA_ID_CHANNEL_PROTECTION_OPP_DELAY) {
        dataIdIndex = 6;
    } else if (dataId == DATA_ID_CHANNEL_PROTECTION_OTP_LEVEL) {
        dataIdIndex = 7;
    } else if (dataId == DATA_ID_CHANNEL_PROTECTION_OTP_DELAY) {
        dataIdIndex = 8;
    } else {
        dataIdIndex = 9;
    }

    return g_focusCursor.i * 10 + dataIdIndex;
}

void getInfoText(int partIndex, char *infoText) {
    int iChannel = partIndex / 10;

    int dataIdIndex = partIndex % 10;

    int dataId;
    const char *dataName;
    const char *unitName;
    int numSignificantDecimalDigits = 2;
    if (dataIdIndex == 0) {
        dataId = DATA_ID_CHANNEL_U_EDIT;
        dataName = "Voltage";
        unitName = "V";
    } else if (dataIdIndex == 1) {
        dataId = DATA_ID_CHANNEL_I_EDIT;
        dataName = "Current";
        unitName = "A";
    } else if (dataIdIndex == 2) {
        dataId = DATA_ID_CHANNEL_PROTECTION_OVP_LIMIT;
        dataName = "OVP Limit";
        unitName = "V";
    } else if (dataIdIndex == 3) {
        dataId = DATA_ID_CHANNEL_PROTECTION_OCP_LIMIT;
        dataName = "OCP Limit";
        unitName = "A";
    } else if (dataIdIndex == 4) {
        dataId = DATA_ID_CHANNEL_PROTECTION_OPP_LIMIT;
        dataName = "OPP Limit";
        unitName = "W";
    } else if (dataIdIndex == 5) {
        dataId = DATA_ID_CHANNEL_PROTECTION_OPP_LEVEL;
        dataName = "OPP Level";
        unitName = "W";
    } else if (dataIdIndex == 6) {
        dataId = DATA_ID_CHANNEL_PROTECTION_OPP_DELAY;
        dataName = "OPP Delay";
        unitName = "s";
        numSignificantDecimalDigits = 0;
    } else if (dataIdIndex == 7) {
        dataId = DATA_ID_CHANNEL_PROTECTION_OTP_LEVEL;
        dataName = "OTP Level";
        unitName = "oC";
        numSignificantDecimalDigits = 0;
    } else if (dataIdIndex == 8) {
        dataId = DATA_ID_CHANNEL_PROTECTION_OTP_DELAY;
        dataName = "OTP Delay";
        unitName = "s";
        numSignificantDecimalDigits = 0;
    } else {
        dataId = DATA_ID_NONE;
        dataName = "Unknown";
        unitName = "";
    }

    data::Cursor cursor(iChannel);
    float minValue = data::getMin(cursor, dataId).getFloat();
    float maxValue = (dataId == DATA_ID_CHANNEL_U_EDIT || dataId == DATA_ID_CHANNEL_I_EDIT) ?
        data::getLimit(cursor, dataId).getFloat() : data::getMax(cursor, dataId).getFloat();

    if (channel_dispatcher::isCoupled() || channel_dispatcher::isTracked()) {
        strcpy(infoText, "Set ");
    } else {
        sprintf(infoText, "Set Ch%d ", iChannel + 1);
    }

    strcat(infoText, dataName);

    strcat(infoText, " [");
    strcatFloat(infoText, minValue, numSignificantDecimalDigits);
    strcat(infoText, "-");
    strcatFloat(infoText, maxValue, numSignificantDecimalDigits);
    strcat(infoText, unitName);
    strcat(infoText, "]");
}

} // namespace edit_mode
} // namespace gui
} // namespace psu
} // namespace eez

#endif