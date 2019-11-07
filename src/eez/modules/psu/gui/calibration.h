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

#pragma once

#include <eez/gui/data.h>

using eez::gui::data::Value;

namespace eez {
namespace psu {
namespace gui {
namespace calibration_wizard {

void start();
Value getLevelValue();
void setLevelValue();
void set();
void previousStep();
void nextStep();
void save();
void stop(void (*callback)());
void toggleEnable();

static const int MAX_STEP_NUM = 10;
extern int g_stepNum;

} // namespace calibration_wizard
} // namespace gui
} // namespace psu
} // namespace eez
