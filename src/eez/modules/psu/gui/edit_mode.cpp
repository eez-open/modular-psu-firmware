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
#include <eez/modules/psu/channel_dispatcher.h>
#include <eez/modules/psu/dlog_record.h>

#include <eez/modules/psu/gui/psu.h>
#include <eez/modules/psu/gui/edit_mode.h>
#include <eez/modules/psu/gui/keypad.h>
#include <eez/modules/psu/gui/page_ch_settings.h>

#define CONF_GUI_EDIT_MODE_STEP_THRESHOLD_PX 5

namespace eez {
namespace psu {
namespace gui {

int getFocusCursor() {
    return g_focusCursor != -1 ? g_focusCursor : g_channel ? g_channel->channelIndex : -1;
}

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

////////////////////////////////////////////////////////////////////////////////

bool isActive(AppContext *appContext) {
    return appContext->getActivePageId() == PAGE_ID_EDIT_MODE_KEYPAD ||
           appContext->getActivePageId() == PAGE_ID_EDIT_MODE_STEP ||
           appContext->getActivePageId() == PAGE_ID_EDIT_MODE_SLIDER;
}

void initEditValue() {
    g_editValue = eez::gui::getEditValue(getFocusCursor(), g_focusDataId);
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
        gui::selectChannelByCursor();
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
        Cursor cursor(getFocusCursor());
        edit_mode_keypad::enter(isChannelData(cursor, g_focusDataId) ? getFocusCursor() : -1, g_editValue, getAllowZero(getFocusCursor(), g_focusDataId), g_minValue, g_maxValue);
    } else {
        edit_mode_keypad::exit();
    }
}

void exit() {
    edit_mode_keypad::exit();
    popPage();
}

void nonInteractiveSet() {
    Value result = set(getFocusCursor(), g_focusDataId, g_editValue);
    if (result.getType() == VALUE_TYPE_SCPI_ERROR) {
        psuErrorMessage(getFocusCursor(), result);
    } else {
        popPage();
    }
}

void nonInteractiveDiscard() {
    g_editValue = g_undoValue;
    set(getFocusCursor(), g_focusDataId, g_undoValue);
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
    return get(getFocusCursor(), g_focusDataId);
}

const Value &getMin() {
    return g_minValue;
}

const Value &getMax() {
    return g_maxValue;
}

Unit getUnit() {
    if (g_editValue.getType() == VALUE_TYPE_FLOAT) {
        return g_editValue.getUnit();
    }
    return getMin().getUnit();
}

bool setValue(float floatValue) {
    if (isChannelData(getFocusCursor(), g_focusDataId)) {
        floatValue = channel_dispatcher::roundChannelValue(Channel::get(getFocusCursor()), getUnit(), floatValue);
    }

    Value value = MakeValue(floatValue, getUnit());
    if (g_isInteractiveMode || g_tabIndex == PAGE_ID_EDIT_MODE_KEYPAD) {
        Value result = set(getFocusCursor(), g_focusDataId, value);
        if (result.getType() == VALUE_TYPE_SCPI_ERROR) {
            psuErrorMessage(getFocusCursor(), result);
            return false;
        }
    }
    g_editValue = value;
    return true;
}

#define NUM_PARTS 15

void getInfoText(char *infoText, int count) {
    const char *dataName = getName(getFocusCursor(), g_focusDataId);
    if (!dataName) {
        dataName = "Unknown";
    }

    Value minValue = eez::gui::getMin(getFocusCursor(), g_focusDataId);
    Value maxValue = (g_focusDataId == DATA_ID_CHANNEL_U_EDIT || g_focusDataId == DATA_ID_CHANNEL_I_EDIT) ?
        getLimit(getFocusCursor(), g_focusDataId) : eez::gui::getMax(getFocusCursor(), g_focusDataId);

    if (isChannelData(getFocusCursor(), g_focusDataId)) {
        Channel& channel = Channel::get(getFocusCursor());
        if ((channel.channelIndex < 2 && channel_dispatcher::getCouplingType() != channel_dispatcher::COUPLING_TYPE_NONE) || channel.flags.trackingEnabled) {
            stringCopy(infoText, count, "Set ");
        } else {
            snprintf(infoText, count, "Set Ch%d ", getFocusCursor() + 1);
        }
        
        stringAppendString(infoText, count, dataName);
    } else {
        stringAppendString(infoText, count, dataName);
    }

    stringAppendString(infoText, count, " [");
    minValue.toText(infoText + strlen(infoText), count - strlen(infoText));
    stringAppendString(infoText, count, " - ");
    maxValue.toText(infoText + strlen(infoText), count - strlen(infoText));
    stringAppendString(infoText, count, "]");
}

static void update() {
    initEditValue();
    g_minValue = eez::gui::getMin(getFocusCursor(), g_focusDataId);
    g_maxValue = eez::gui::getMax(getFocusCursor(), g_focusDataId);
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

    if (editValue.getType() == VALUE_TYPE_FLOAT) {
        options.editValueUnit = editValue.getUnit();
    } else {
        options.editValueUnit = minValue.getUnit();
    }

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

struct DataIdStepIndex {
    uint16_t dataId;
    uint16_t stepIndex;
};

static int g_stepIndex;

static bool g_changed;
static int g_startPos;

int getStepIndex() {
    return g_stepIndex;
}

void setStepIndex(int value) {
    g_stepIndex = value;
}

void getStepValues(StepValues &stepValues) {
    ChSettingsListsPage *page = (ChSettingsListsPage *)getPage(PAGE_ID_CH_SETTINGS_LISTS);
    if (page) {
        getEncoderStepValues(g_channel->channelIndex, page->getDataIdAtCursor(), stepValues);
    } else {
        getEncoderStepValues(getFocusCursor(), g_focusDataId, stepValues);
    }
}

void switchToNextStepIndex() {
    setStepIndex(getStepIndex() + 1);
}

void increment(int counter, bool playClick) {
    float min = edit_mode::getMin().getFloat();
    float max = edit_mode::getMax().getFloat();

    StepValues stepValues;
    getStepValues(stepValues);
    float stepValue = stepValues.values[getStepIndex() % stepValues.count];

    float value = edit_mode::getEditValue().getFloat();

    for (int i = 0; i < abs(counter); ++i) {
        if (counter > 0) {
            value += stepValue;
            if (value > max) {
                value = max;
            }
        } else {
            value -= stepValue;
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

bool hasEncoderStepValue() {
    auto encoderMode = mcu::encoder::getEncoderMode();
    if (encoderMode == mcu::encoder::ENCODER_MODE_AUTO) {
        return true;
    }

    StepValues stepValues;
    getStepValues(stepValues);

    int index = encoderMode - mcu::encoder::ENCODER_MODE_STEP1;
    return index < stepValues.count;
}

Value getCurrentEncoderStepValue() {
    StepValues stepValues;
    getStepValues(stepValues);

    int stepValueIndex = mcu::encoder::getEncoderMode() - mcu::encoder::ENCODER_MODE_STEP1;
    if (stepValueIndex >= stepValues.count) {
        stepValueIndex = stepValues.count - 1;
    }

    return Value(stepValues.values[stepValueIndex], stepValues.unit);
}

void showCurrentEncoderMode() {
#if OPTION_ENCODER
    if (mcu::encoder::getEncoderMode() == mcu::encoder::ENCODER_MODE_AUTO) {
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

    int y = eez::gui::touch::getY() - g_psuAppContext.rect.y;
    if (y < TOP_BORDER) {
        y = TOP_BORDER;
    } else if (y > BOTTOM_BORDER) {
        y = BOTTOM_BORDER;
    }
    y -= TOP_BORDER;

    int stepIndex = edit_mode_step::NUM_STEPS * y / (BOTTOM_BORDER - TOP_BORDER);
    stepIndex = edit_mode_step::NUM_STEPS - 1 - MAX(MIN(stepIndex, edit_mode_step::NUM_STEPS - 1), 0);
    StepValues stepValues;
    edit_mode_step::getStepValues(stepValues);
    stepValue = stepValues.values[MIN(stepIndex, stepValues.count - 1)];
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
