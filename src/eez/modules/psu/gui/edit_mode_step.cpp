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

#include <math.h>

#include <eez/sound.h>

#include <eez/gui/touch.h>
#include <eez/gui/dialogs.h>

#if OPTION_ENCODER
#include <eez/modules/mcu/encoder.h>
#endif

#include <eez/modules/psu/gui/data.h>
#include <eez/modules/psu/gui/edit_mode.h>
#include <eez/modules/psu/gui/edit_mode_step.h>
#include <eez/modules/psu/gui/psu.h>

#define CONF_GUI_EDIT_MODE_STEP_THRESHOLD_PX 5

namespace eez {
namespace psu {
namespace gui {
namespace edit_mode_step {

using data::Value;

#define NUM_UNITS 6
#define NUM_STEPS_PER_UNIT 5

static const Value CONF_GUI_UNIT_STEPS_LIST[NUM_UNITS][NUM_STEPS_PER_UNIT] = {
    { 
        Value(5.0f, UNIT_VOLT), 
        Value(2.0f, UNIT_VOLT), 
        Value(1.0f, UNIT_VOLT), 
        Value(0.5f, UNIT_VOLT), 
        Value(0.1f, UNIT_VOLT) 
    },
    { 
        Value(0.5f, UNIT_AMPER), 
        Value(0.25f, UNIT_AMPER), 
        Value(0.1f, UNIT_AMPER), 
        Value(0.05f, UNIT_AMPER), 
        Value(0.01f, UNIT_AMPER) 
    },
    { 
        Value(0.005f, UNIT_AMPER), 
        Value(0.0025f, UNIT_AMPER), 
        Value(0.001f, UNIT_AMPER), 
        Value(0.0005f, UNIT_AMPER), 
        Value(0.0001f, UNIT_AMPER) 
    },
    { 
        Value(10.0f, UNIT_WATT), 
        Value(5.0f, UNIT_WATT), 
        Value(2.0f, UNIT_WATT), 
        Value(1.0f, UNIT_WATT), 
        Value(0.5f, UNIT_WATT) 
    },
    { 
        Value(20.0f, UNIT_CELSIUS), 
        Value(10.0f, UNIT_CELSIUS), 
        Value(5.0f, UNIT_CELSIUS), 
        Value(2.0f, UNIT_CELSIUS), 
        Value(1.0f, UNIT_CELSIUS) 
    },
    { 
        Value(30.0f, UNIT_SECOND), 
        Value(20.0f, UNIT_SECOND), 
        Value(10.0f, UNIT_SECOND), 
        Value(5.0f, UNIT_SECOND), 
        Value(1.0f, UNIT_SECOND) 
    },
};

static int g_stepIndexes[NUM_UNITS][CH_MAX];

static const int DEFAULT_INDEX = 2;

static bool g_changed;
static int g_startPos;

int getStepValuesCount() {
    return NUM_STEPS_PER_UNIT;
}

int getUnitStepValuesIndex(Unit unit) {
    if (unit == UNIT_VOLT) {
        return 0;
    } 
    
    if (unit == UNIT_AMPER) {
        if (Channel::get(g_focusCursor.i).flags.currentRangeSelectionMode == CURRENT_RANGE_SELECTION_ALWAYS_LOW) {
            return 2;
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

const Value *getUnitStepValues(Unit unit) {
    return CONF_GUI_UNIT_STEPS_LIST[getUnitStepValuesIndex(unit)];
}

int getStepIndex() {
    int stepIndex = g_stepIndexes[getUnitStepValuesIndex(edit_mode::getUnit())][g_focusCursor.i];
    if (stepIndex == 0) {
        return DEFAULT_INDEX;
    }
    return stepIndex - 1;
}

void setStepIndex(int value) {
    int unitStepValuesIndex = getUnitStepValuesIndex(edit_mode::getUnit());
    g_stepIndexes[unitStepValuesIndex][g_focusCursor.i] = 1 + value;
}

void switchToNextStepIndex() {
    g_stepIndexes[getUnitStepValuesIndex(edit_mode::getUnit())][g_focusCursor.i] = 1 + ((getStepIndex() + 1) % NUM_STEPS_PER_UNIT);
}

const Value *getStepValues() {
    return getUnitStepValues(edit_mode::getUnit());
}

float getStepValue() {
    return getStepValues()[getStepIndex()].getFloat();
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
    auto stepValues = getUnitStepValues(getCurrentEncoderUnit());
    return stepValues[mcu::encoder::ENCODER_MODE_STEP5 - mcu::encoder::g_encoderMode];
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
} // namespace gui
} // namespace psu
} // namespace eez

#endif