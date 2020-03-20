/*
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

#include <eez/modules/psu/psu.h>

#if OPTION_DISPLAY

#include <eez/gui/gui.h>
#include <eez/modules/psu/gui/edit_mode.h>
#include <eez/modules/psu/gui/edit_mode_keypad.h>
#include <eez/modules/psu/gui/numeric_keypad.h>

namespace eez {

using namespace gui;

namespace psu {
namespace gui {
namespace edit_mode_keypad {

NumericKeypad g_theKeypad;
NumericKeypad *g_keypad;

////////////////////////////////////////////////////////////////////////////////

void onKeypadOk(float value) {
    if (edit_mode::setValue(value)) {
        popPage();
    }
}

void enter(int channelIndex, const eez::gui::data::Value &editValue, bool allowZero, const eez::gui::data::Value &minValue, eez::gui::data::Value &maxValue) {
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

    g_keypad->init(0, editValue, options, onKeypadOk, 0, 0);
}

void exit() {
    if (g_keypad) {
        g_keypad = 0;
    }
}

} // namespace edit_mode_keypad
} // namespace gui
} // namespace psu
} // namespace eez

#endif
