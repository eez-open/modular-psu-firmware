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

#pragma once

namespace eez {
namespace psu {
namespace gui {
namespace edit_mode {

bool isActive(eez::gui::AppContext *appContext);
void enter(int tabIndex = -1, bool setFocus = true);
void exit();
bool isInteractiveMode();
void toggleInteractiveMode();

const eez::gui::data::Value &getEditValue();
eez::gui::data::Value getCurrentValue();
const eez::gui::data::Value &getMin();
const eez::gui::data::Value &getMax();
Unit getUnit();
bool setValue(float value);

void getInfoText(char *infoText, int count);

void nonInteractiveSet();
void nonInteractiveDiscard();

} // namespace edit_mode
} // namespace gui
} // namespace psu
} // namespace eez
