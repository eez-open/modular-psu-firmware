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

using namespace eez::gui;

namespace eez {
namespace gui {

void getTouchScreenCalibrationParamsHook(
	int16_t &touchScreenCalTlx, int16_t &touchScreenCalTly,
	int16_t &touchScreenCalBrx, int16_t &touchScreenCalBry,
	int16_t &touchScreenCalTrx, int16_t &touchScreenCalTry
);

void setTouchScreenCalibrationParamsHook(
	int16_t touchScreenCalTlx, int16_t touchScreenCalTly,
	int16_t touchScreenCalBrx, int16_t touchScreenCalBry,
	int16_t touchScreenCalTrx, int16_t touchScreenCalTry
);

bool isTouchCalibrated();
void enterTouchCalibration(AppContext *appContext);
void onTouchCalibrationPageTouch(const WidgetCursor &foundWidget, Event &touchEvent);

void touchCalibrationDialogYes();
void touchCalibrationDialogNo();
void touchCalibrationDialogCancel();

} // namespace gui
} // namespace eez
