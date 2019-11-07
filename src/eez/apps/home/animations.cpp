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

#if OPTION_DISPLAY

#include <eez/apps/home/animations.h>
#include <eez/gui/gui.h>

#include <eez/apps/psu/psu.h>
#include <eez/apps/psu/persist_conf.h>

using namespace eez::gui;

static const Rect g_displayRect = { 0, 0, 480, 272 };

namespace eez {
namespace home {

void animateFadeOutFadeIn() {
    int i = 0;
    g_animRects[i++] = { BUFFER_SOLID_COLOR, g_displayRect, g_displayRect, 0, OPACITY_SOLID, POSITION_TOP_LEFT };
    g_animRects[i++] = { BUFFER_OLD, g_displayRect, g_displayRect, 0, OPACITY_FADE_OUT, POSITION_TOP_LEFT };
    g_animRects[i++] = { BUFFER_NEW, g_displayRect, g_displayRect, 0, OPACITY_FADE_IN, POSITION_TOP_LEFT };
    animateRects(BUFFER_NEW, i, 2 * psu::persist_conf::devConf.animationsDuration);
}

} // namespace home
} // namespace eez

#endif
