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

#include <stdio.h>
#include <string.h>
#include <math.h>

#include <eez/sound.h>

#if OPTION_ENCODER
#include <eez/modules/mcu/encoder.h>
#endif

#include <eez/modules/psu/psu.h>
#include <eez/modules/psu/calibration.h>
#include <eez/modules/psu/channel_dispatcher.h>
#include <eez/modules/psu/dlog_record.h>

#include <eez/modules/psu/gui/psu.h>
#include <eez/modules/psu/gui/edit_mode.h>
#include <eez/modules/psu/gui/keypad.h>

#define CONF_GUI_EDIT_MODE_STEP_THRESHOLD_PX 5

namespace eez {
namespace psu {
namespace gui {
namespace edit_mode {

static int g_tabIndex = PAGE_ID_EDIT_MODE_KEYPAD;

static Value g_editValue;
static Value g_undoValue;
static Value g_minValue;
static Value g_maxValue;
static bool g_isInteractiveMode = true;

////////////////////////////////////////////////////////////////////////////////

static void update();

////////////////////////////////////////////////////////////////////////////////

bool isActive(AppContext *appContext) {
    return appContext->getActivePageId() == PAGE_ID_EDIT_MODE_KEYPAD ||
           appContext->getActivePageId() == PAGE_ID_EDIT_MODE_STEP ||
           appContext->getActivePageId() == PAGE_ID_EDIT_MODE_SLIDER;
}

void initEditValue() {
    g_editValue = eez::gui::getEditValue(g_focusCursor, g_focusDataId);
    g_undoValue = g_editValue;
}

void enter(int tabIndex, bool setFocus) {
#if OPTION_ENCODER
    if (setFocus && !isActive(&g_psuAppContext)) {
        if (!g_psuAppContext.isFocusWidget(getFoundWidgetAtDown()) || g_focusEditValue.getType() != VALUE_TYPE_NONE) {
            setFocusCursor(getFoundWidgetAtDown().cursor, getFoundWidgetAtDown().widget->data);
            return;
        }
    }
#endif

    if (tabIndex == -1) {
        gui::selectChannel();
        Cursor newDataCursor = getFoundWidgetAtDown().cursor;
        int16_t newDataId = getFoundWidgetAtDown().widget->data;
        setFocusCursor(newDataCursor, newDataId);
        update();

        if (!isActive(&g_psuAppContext)) {
            pushPage(g_tabIndex);
        }
    } else {
        g_tabIndex = tabIndex;
        replacePage(g_tabIndex);
    }

    if (g_tabIndex == PAGE_ID_EDIT_MODE_KEYPAD) {
        Cursor cursor(g_focusCursor);
        edit_mode_keypad::enter(isChannelData(cursor, g_focusDataId) ? g_focusCursor : -1, g_editValue, getAllowZero(g_focusCursor, g_focusDataId), g_minValue, g_maxValue);
    } else {
        edit_mode_keypad::exit();
    }
}

void exit() {
    edit_mode_keypad::exit();
    popPage();
}

void nonInteractiveSet() {
    Value result = set(g_focusCursor, g_focusDataId, g_editValue);
    if (result.getType() == VALUE_TYPE_SCPI_ERROR) {
        psuErrorMessage(g_focusCursor, result);
    } else {
        popPage();
    }
}

void nonInteractiveDiscard() {
    g_editValue = g_undoValue;
    set(g_focusCursor, g_focusDataId, g_undoValue);
}

bool isInteractiveMode() {
    return g_isInteractiveMode;
}

void toggleInteractiveMode() {
    g_isInteractiveMode = !g_isInteractiveMode;
    initEditValue();
}

const Value &getEditValue() {
    return g_editValue;
}

Value getCurrentValue() {
    return get(g_focusCursor, g_focusDataId);
}

const Value &getMin() {
    return g_minValue;
}

const Value &getMax() {
    return g_maxValue;
}

Unit getUnit() {
    return g_editValue.getUnit();
}

bool setValue(float floatValue) {
    if (isChannelData(g_focusCursor, g_focusDataId)) {
        floatValue = channel_dispatcher::roundChannelValue(Channel::get(g_focusCursor), getUnit(), floatValue);
    }

    Value value = MakeValue(floatValue, getUnit());
    if (g_isInteractiveMode || g_tabIndex == PAGE_ID_EDIT_MODE_KEYPAD) {
        Value result = set(g_focusCursor, g_focusDataId, value);
        if (result.getType() == VALUE_TYPE_SCPI_ERROR) {
            psuErrorMessage(g_focusCursor, result);
            return false;
        }
    }
    g_editValue = value;
    return true;
}

#define NUM_PARTS 15

void getInfoText(char *infoText, int count) {
    const char *dataName = getName(g_focusCursor, g_focusDataId);
    if (!dataName) {
        dataName = "Unknown";
    }

    Value minValue = eez::gui::getMin(g_focusCursor, g_focusDataId);
    Value maxValue = (g_focusDataId == DATA_ID_CHANNEL_U_EDIT || g_focusDataId == DATA_ID_CHANNEL_I_EDIT) ?
        getLimit(g_focusCursor, g_focusDataId) : eez::gui::getMax(g_focusCursor, g_focusDataId);

    if (isChannelData(g_focusCursor, g_focusDataId)) {
        Channel& channel = Channel::get(g_focusCursor);
        if ((channel.channelIndex < 2 && channel_dispatcher::getCouplingType() != channel_dispatcher::COUPLING_TYPE_NONE) || channel.flags.trackingEnabled) {
            strcpy(infoText, "Set ");
        } else {
            sprintf(infoText, "Set Ch%d ", g_focusCursor + 1);
        }
        
        strcat(infoText, dataName);
    } else {
        strcat(infoText, dataName);
    }

    strcat(infoText, " [");
    minValue.toText(infoText + strlen(infoText), count - strlen(infoText));
    strcat(infoText, " - ");
    maxValue.toText(infoText + strlen(infoText), count - strlen(infoText));
    strcat(infoText, "]");
}

static void update() {
    initEditValue();
    g_minValue = eez::gui::getMin(g_focusCursor, g_focusDataId);
    g_maxValue = eez::gui::getMax(g_focusCursor, g_focusDataId);
    if (edit_mode_keypad::g_keypad) {
        edit_mode_keypad::g_keypad->m_options.editValueUnit = g_editValue.getUnit();
    }
}

} // namespace edit_mode

namespace edit_mode_keypad {

NumericKeypad g_theKeypad;
NumericKeypad *g_keypad;

////////////////////////////////////////////////////////////////////////////////

void onKeypadOk(float value) {
    if (edit_mode::setValue(value)) {
        g_keypad->getAppContext()->popPage();
    }
}

void enter(int channelIndex, const Value &editValue, bool allowZero, const Value &minValue, Value &maxValue) {
    g_keypad = &g_theKeypad;

    NumericKeypadOptions options;

    options.channelIndex = channelIndex;

    options.editValueUnit = editValue.getUnit();

    options.allowZero = allowZero;
    options.min = minValue.getFloat();
    options.max = maxValue.getFloat();
    options.def = options.min;

    options.enableMaxButton();
    options.enableMinButton();
    options.flags.signButtonEnabled = options.min < 0;
    options.flags.dotButtonEnabled = true;

    g_keypad->init(&g_psuAppContext, 0, editValue, options, onKeypadOk, 0, 0);
}

void exit() {
    if (g_keypad) {
        g_keypad = 0;
    }
}

} // namespace edit_mode_keypad

////////////////////////////////////////////////////////////////////////////////

namespace edit_mode_step {

#define NUM_UNITS 7

static const Value CONF_GUI_UNIT_STEPS_LIST[NUM_UNITS][NUM_STEPS_PER_UNIT] = {
    { 
        Value(2.0f, UNIT_VOLT), 
        Value(1.0f, UNIT_VOLT), 
        Value(0.5f, UNIT_VOLT), 
        Value(0.1f, UNIT_VOLT) 
    },
    { 
        Value(0.25f, UNIT_AMPER), 
        Value(0.1f, UNIT_AMPER), 
        Value(0.05f, UNIT_AMPER), 
        Value(0.01f, UNIT_AMPER) 
    },
    { 
        Value(0.0025f, UNIT_AMPER), 
        Value(0.001f, UNIT_AMPER), 
        Value(0.0005f, UNIT_AMPER), 
        Value(0.0001f, UNIT_AMPER) 
    },
    { 
        Value(5.0f, UNIT_WATT), 
        Value(2.0f, UNIT_WATT), 
        Value(1.0f, UNIT_WATT), 
        Value(0.5f, UNIT_WATT) 
    },
    { 
        Value(10.0f, UNIT_CELSIUS), 
        Value(5.0f, UNIT_CELSIUS), 
        Value(2.0f, UNIT_CELSIUS), 
        Value(1.0f, UNIT_CELSIUS) 
    },
    { 
        Value(20.0f, UNIT_SECOND), 
        Value(10.0f, UNIT_SECOND), 
        Value(5.0f, UNIT_SECOND), 
        Value(1.0f, UNIT_SECOND) 
    },
    { 
        Value(0.2f, UNIT_AMPER), 
        Value(0.1f, UNIT_AMPER), 
        Value(0.04f, UNIT_AMPER), 
        Value(0.02f, UNIT_AMPER) 
    }
};

static int g_stepIndexes[NUM_UNITS][CH_MAX];

static const int DEFAULT_INDEX = 1;

static bool g_changed;
static int g_startPos;

int getUnitStepValuesIndex(Unit unit) {
    if (unit == UNIT_VOLT) {
        return 0;
    } 
    
    if (unit == UNIT_AMPER) {
        Channel &channel = Channel::get(g_focusCursor);
        if (channel.flags.currentRangeSelectionMode == CURRENT_RANGE_SELECTION_ALWAYS_LOW) {
            return 2;
        }

        // DCM220
        int slotIndex = Channel::get(channel.channelIndex).slotIndex;
        auto &slot = g_slots[slotIndex];
        if (slot.moduleInfo->moduleType == MODULE_TYPE_DCM220) {
            return 6;
        }

        return 1;
    } 
    
    if (unit == UNIT_WATT) {
        return 3;
    } 
    
    if (unit == UNIT_CELSIUS) {
        return 4;
    } 
    
    if (unit == UNIT_SECOND) {
        return 5;
    }

    return 0;
}

void getUnitStepValues(Unit unit, StepValues &stepValues) {
    stepValues.count = NUM_STEPS_PER_UNIT;
    stepValues.values = &CONF_GUI_UNIT_STEPS_LIST[getUnitStepValuesIndex(unit)][0];
}

int getStepIndex() {
    int stepIndex = g_stepIndexes[getUnitStepValuesIndex(edit_mode::getUnit())][g_focusCursor];
    if (stepIndex == 0) {
        return DEFAULT_INDEX;
    }
    return stepIndex - 1;
}

void setStepIndex(int value) {
    int unitStepValuesIndex = getUnitStepValuesIndex(edit_mode::getUnit());
    g_stepIndexes[unitStepValuesIndex][g_focusCursor] = 1 + value;
}

void switchToNextStepIndex() {
    g_stepIndexes[getUnitStepValuesIndex(edit_mode::getUnit())][g_focusCursor] = 1 + ((getStepIndex() + 1) % NUM_STEPS_PER_UNIT);
}

void getStepValues(StepValues &stepValues) {
    return getUnitStepValues(edit_mode::getUnit(), stepValues);
}

float getStepValue() {
    StepValues stepValues;
    getStepValues(stepValues);
    return stepValues.values[MIN(getStepIndex(), stepValues.count - 1)].getFloat();
}

void increment(int counter, bool playClick) {
    float min = edit_mode::getMin().getFloat();
    float max = edit_mode::getMax().getFloat();
    float stepValue = getStepValue();

    float value = edit_mode::getEditValue().getFloat();

    for (int i = 0; i < abs(counter); ++i) {
        if (counter > 0) {
            if (value == min) {
                value = (floorf(min / stepValue) + 1) * stepValue;
            } else {
                value += stepValue;
            }
            if (value > max) {
                value = max;
            }
        } else {
            if (value == max) {
                value = (ceilf(max / stepValue) - 1) * stepValue;
            } else {
                value -= stepValue;
            }
            if (value < min) {
                value = min;
            }
        }
    }

    if (edit_mode::setValue(value)) {
        g_changed = true;
        if (playClick) {
            sound::playClick();
        }
    }
}

#if OPTION_ENCODER

void onEncoder(int counter) {
    increment(counter, false);
}

#endif

void test() {
    if (!g_changed) {
        int d = eez::gui::touch::getX() - g_startPos;
        if (abs(d) >= CONF_GUI_EDIT_MODE_STEP_THRESHOLD_PX) {
            increment(d > 0 ? 1 : -1, true);
        }
    }
}

void onTouchDown() {
    g_startPos = eez::gui::touch::getX();
    g_changed = false;
}

void onTouchMove() {
    test();
}

void onTouchUp() {
}

Value getCurrentEncoderStepValue() {
    StepValues stepValues;
    if (!getEncoderStepValues(g_focusCursor, g_focusDataId, stepValues)) {
        getUnitStepValues(getCurrentEncoderUnit(), stepValues);
    }
    int stepValueIndex = mcu::encoder::ENCODER_MODE_STEP4 - mcu::encoder::g_encoderMode;
    if (stepValueIndex >= stepValues.count) {
        stepValueIndex = stepValues.count - 1;
    }
    return stepValues.values[stepValueIndex];
}

void showCurrentEncoderMode() {
#if OPTION_ENCODER
    if (mcu::encoder::g_encoderMode == mcu::encoder::ENCODER_MODE_AUTO) {
        infoMessage("Auto");
    } else {
        infoMessage(getCurrentEncoderStepValue());
    }
#endif
}

} // namespace edit_mode_step

////////////////////////////////////////////////////////////////////////////////

namespace edit_mode_slider {

static const int TOP_BORDER = 51;
static const int BOTTOM_BORDER = 255;

static const int DX = 10;

static float startValue;
static int startX;
static float stepValue;

void onTouchDown() {
    startValue = edit_mode::getEditValue().getFloat();
    startX = eez::gui::touch::getX();

    int y = eez::gui::touch::getY() - g_psuAppContext.y;
    if (y < TOP_BORDER) {
        y = TOP_BORDER;
    } else if (y > BOTTOM_BORDER) {
        y = BOTTOM_BORDER;
    }
    y -= TOP_BORDER;

    int stepIndex = NUM_STEPS_PER_UNIT * y / (BOTTOM_BORDER - TOP_BORDER);
    stepIndex = MAX(MIN(stepIndex, NUM_STEPS_PER_UNIT - 1), 0);
    StepValues stepValues;
    edit_mode_step::getStepValues(stepValues);
    stepValue = stepValues.values[MIN(stepIndex, stepValues.count - 1)].getFloat();
}

void onTouchMove() {
    float min = edit_mode::getMin().getFloat();
    float max = edit_mode::getMax().getFloat();

    int counter = (eez::gui::touch::getX() - startX) / DX;

    float value = roundPrec(startValue + counter * stepValue, stepValue);

    if (value < min) {
        value = min;
    }
    if (value > max) {
        value = max;
    }

    edit_mode::setValue(value);
}

void onTouchUp() {
}

} // namespace edit_mode_slider

} // namespace gui
} // namespace psu
} // namespace eez

#endif
