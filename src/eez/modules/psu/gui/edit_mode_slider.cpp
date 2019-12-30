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

#include <eez/modules/psu/psu.h>

#if OPTION_DISPLAY

#include <eez/modules/psu/gui/psu.h>
#include <eez/modules/psu/gui/edit_mode.h>
#include <eez/modules/psu/gui/edit_mode_step.h>
#include <eez/gui/touch.h>
#include <eez/modules/mcu/display.h>

namespace eez {
namespace psu {
namespace gui {
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

    int y = eez::gui::touch::getY() - g_appContext->y;
    if (y < TOP_BORDER) {
        y = TOP_BORDER;
    } else if (y > BOTTOM_BORDER) {
        y = BOTTOM_BORDER;
    }
    y -= TOP_BORDER;

    int stepIndex = NUM_STEPS_PER_UNIT * y / (BOTTOM_BORDER - TOP_BORDER);
    stepIndex = MAX(MIN(stepIndex, NUM_STEPS_PER_UNIT - 1), 0);
    data::StepValues stepValues;
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