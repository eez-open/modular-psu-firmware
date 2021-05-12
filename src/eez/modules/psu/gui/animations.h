/*
* EEZ Modular PSU Firmware
* Copyright (C) 2019-present, Envox d.o.o.
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

extern const Rect g_workingAreaRect;

void animateFromDefaultViewToMaxView(int iMax, bool isFullScreenView);
void animateFromMaxViewToDefaultView(int iMax, bool isFullScreenView);
void animateFromMinViewToMaxView(int maxSlotIndexBefore, bool isFullScreenView);

void animateSlideUp();
void animateSlideDown();
void animateSlideLeft();
void animateSlideRight();
void animateSlideLeftWithoutHeader();
void animateSlideRightWithoutHeader();
void animateSlideLeftInDefaultViewVert(int viewIndex);
void animateSlideLeftInDefaultViewHorz(int viewIndex);
void animateFadeOutFadeIn();
void animateFadeOutFadeInWorkingArea();
void animateFadeOutFadeIn(const Rect &rect);

} // namespace gui
} // namespace psu
} // namespace eez
