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

#include <eez/hmi.h>
#include <eez/gui/gui.h>

#include <eez/modules/psu/psu.h>
#include <eez/modules/psu/gui/psu.h>
#include <eez/modules/psu/gui/animations.h>

static const Rect g_displayRect = { 0, 0, 480, 272 };

static const Rect g_workingAreaRect = { 0, 0, 480, 240 };
static const Rect g_workingAreaRectTop = { 0, -240, 480, 240 };
//static const Rect g_workingAreaRectBottom = { 0, 240, 480, 240 };
static const Rect g_workingAreaRectLeft = { -480, 0, 480, 240 };
static const Rect g_workingAreaRectRight = { 480, 0, 480, 240 };

static const Rect g_statusLineRect = { 0, 240, 480, 32 };
static const Rect g_statusLineRectBottom = { 0, 272, 480, 32 };
static const Rect g_statusLineRectLeft = { -480, 240, 480, 32 };
static const Rect g_statusLineRectRight = { 480, 240, 480, 32 };

static const Rect g_vertDefRects[] = {
    {   0, 0, 160, 240 },
    { 160, 0, 160, 240 },
    { 320, 0, 160, 240 }
};

static const Rect g_horzDefRects[] = {
    { 0,   0, 480, 80 },
    { 0,  80, 480, 80 },
    { 0, 160, 480, 80 }
};

static const Rect g_maxRect = { 0, 0, 480, 168 };

static const Rect g_maxRectMax = { 0, 0, 360, 168 };
static const Rect g_maxRectMin = { 360, 0, 120, 168 };

static const Rect g_minRects[] = {
    {   0, 168, 240, 72 },
    { 240, 168, 240, 72 }
};

static const Rect g_microRects[] = {
    {   0, 168, 160, 72 },
    { 160, 168, 160, 72 },
    { 320, 168, 160, 72 }
};

static const Rect g_settingsRect = { 0, 0, 480, 168 };
static const Rect g_settingsRectTop = { 0, -168, 480, 168 };
static const Rect g_settingsRectLeft = { -480, 0, 480, 240 };
static const Rect g_settingsRectRight = { 480, 0, 480, 240 };

static const Rect g_settingsRectNoChannels = { 0, 0, 480, 240 };

namespace eez {
namespace psu {
namespace gui {

void animateFromDefaultViewToMaxView() {
    int iMax = hmi::g_selectedSlotIndex;
    int iMin1 = iMax == 0 ? 1 : 0;
    int iMin2 = iMax == 2 ? 1 : 2;

    auto g_defRects = psu::persist_conf::devConf.channelsViewMode == CHANNELS_VIEW_MODE_NUMERIC || 
        psu::persist_conf::devConf.channelsViewMode == CHANNELS_VIEW_MODE_VERT_BAR ? g_vertDefRects : g_horzDefRects;

    int i = 0;

    g_animRects[i++] = { BUFFER_SOLID_COLOR, g_workingAreaRect, g_workingAreaRect, 0, OPACITY_SOLID, POSITION_TOP_LEFT };

    g_animRects[i++] = { BUFFER_OLD, g_defRects[iMin1], g_minRects[0], 0, OPACITY_FADE_OUT, POSITION_TOP_LEFT };
    g_animRects[i++] = { BUFFER_OLD, g_defRects[iMin2], g_minRects[1], 0, OPACITY_FADE_OUT, POSITION_TOP_LEFT };
    g_animRects[i++] = { BUFFER_OLD, g_defRects[iMax], g_maxRect, 0, OPACITY_FADE_OUT, iMax == 1 ? POSITION_CENTER : POSITION_TOP };

    g_animRects[i++] = { BUFFER_NEW, g_defRects[iMin1], g_minRects[0], 0, OPACITY_FADE_IN, POSITION_TOP_LEFT };
    g_animRects[i++] = { BUFFER_NEW, g_defRects[iMin2], g_minRects[1], 0, OPACITY_FADE_IN, POSITION_TOP_LEFT };
    g_animRects[i++] = { BUFFER_NEW, g_defRects[iMax], g_maxRect, 0, OPACITY_FADE_IN, POSITION_BOTTOM};
    
    animateRects(&g_psuAppContext, BUFFER_OLD, i);
}

void animateFromMaxViewToDefaultView() {
    int iMax = hmi::g_selectedSlotIndex;
    int iMin1 = iMax == 0 ? 1 : 0;
    int iMin2 = iMax == 2 ? 1 : 2;

    auto g_defRects = psu::persist_conf::devConf.channelsViewMode == CHANNELS_VIEW_MODE_NUMERIC || 
        psu::persist_conf::devConf.channelsViewMode == CHANNELS_VIEW_MODE_VERT_BAR ? g_vertDefRects : g_horzDefRects;

    int i = 0;

    g_animRects[i++] = { BUFFER_SOLID_COLOR, g_workingAreaRect, g_workingAreaRect, 0, OPACITY_SOLID, POSITION_TOP_LEFT };

    g_animRects[i++] = { BUFFER_OLD, g_minRects[0], g_defRects[iMin1], 0, OPACITY_FADE_OUT, POSITION_TOP_LEFT };
    g_animRects[i++] = { BUFFER_OLD, g_minRects[1], g_defRects[iMin2], 0, OPACITY_FADE_OUT, POSITION_TOP_LEFT };
    g_animRects[i++] = { BUFFER_OLD, g_maxRect, g_defRects[iMax], 0, OPACITY_FADE_OUT, POSITION_BOTTOM };

    g_animRects[i++] = { BUFFER_NEW, g_minRects[0], g_defRects[iMin1], 0, OPACITY_FADE_IN, POSITION_TOP_LEFT };
    g_animRects[i++] = { BUFFER_NEW, g_minRects[1], g_defRects[iMin2], 0, OPACITY_FADE_IN, POSITION_TOP_LEFT };
    g_animRects[i++] = { BUFFER_NEW, g_maxRect, g_defRects[iMax], 0, OPACITY_FADE_IN, iMax == 1 ? POSITION_CENTER : POSITION_TOP };

    animateRects(&g_psuAppContext, BUFFER_OLD, i);
}

void animateFromMinViewToMaxView(int maxSlotIndexBefore) {
    int iMaxBefore = maxSlotIndexBefore + 1;
    int iMax = psu::persist_conf::getMaxSlotIndex() + 1;

    int i = 0;

    g_animRects[i++] = { BUFFER_SOLID_COLOR, g_workingAreaRect, g_workingAreaRect, 0, OPACITY_SOLID, POSITION_TOP_LEFT };

    if ((iMax == 1 && iMaxBefore == 2) || (iMax == 2 && iMaxBefore == 1)) {
        g_animRects[i++] = { BUFFER_NEW, g_minRects[1], g_minRects[1], 0, OPACITY_SOLID, POSITION_CENTER };

        g_animRects[i++] = { BUFFER_OLD, g_maxRect, g_minRects[0], 0, OPACITY_FADE_OUT, POSITION_TOP_LEFT };
        g_animRects[i++] = { BUFFER_OLD, g_minRects[0], g_maxRect, 0, OPACITY_FADE_OUT, POSITION_TOP_LEFT };

        g_animRects[i++] = { BUFFER_NEW, g_maxRect, g_minRects[0], 0, OPACITY_FADE_IN, POSITION_TOP_LEFT };
        g_animRects[i++] = { BUFFER_NEW, g_minRects[0], g_maxRect, 0, OPACITY_FADE_IN, POSITION_TOP_LEFT };
    } else if ((iMax == 2 && iMaxBefore == 3) || (iMax == 3 && iMaxBefore == 2)) {
        g_animRects[i++] = { BUFFER_NEW, g_minRects[0], g_minRects[0], 0, OPACITY_SOLID, POSITION_CENTER };

        g_animRects[i++] = { BUFFER_OLD, g_maxRect, g_minRects[1], 0, OPACITY_FADE_OUT, POSITION_TOP_LEFT };
        g_animRects[i++] = { BUFFER_OLD, g_minRects[1], g_maxRect, 0, OPACITY_FADE_OUT, POSITION_TOP_LEFT };

        g_animRects[i++] = { BUFFER_NEW, g_maxRect, g_minRects[1], 0, OPACITY_FADE_IN, POSITION_TOP_LEFT };
        g_animRects[i++] = { BUFFER_NEW, g_minRects[1], g_maxRect, 0, OPACITY_FADE_IN, POSITION_TOP_LEFT };
    } else if (iMax == 1 && iMaxBefore == 3) {
        g_animRects[i++] = { BUFFER_OLD, g_minRects[1], g_minRects[0], 0, OPACITY_FADE_OUT, POSITION_TOP_LEFT };
        g_animRects[i++] = { BUFFER_OLD, g_maxRect, g_minRects[1], 0, OPACITY_FADE_OUT, POSITION_TOP_LEFT };
        g_animRects[i++] = { BUFFER_OLD, g_minRects[0], g_maxRect, 0, OPACITY_FADE_OUT, POSITION_TOP_LEFT };

        g_animRects[i++] = { BUFFER_NEW, g_minRects[1], g_minRects[0], 0, OPACITY_FADE_IN, POSITION_TOP_LEFT };
        g_animRects[i++] = { BUFFER_NEW, g_maxRect, g_minRects[1], 0, OPACITY_FADE_IN, POSITION_TOP_LEFT };
        g_animRects[i++] = { BUFFER_NEW, g_minRects[0], g_maxRect, 0, OPACITY_FADE_IN, POSITION_TOP_LEFT };
    } else if (iMax == 3 && iMaxBefore == 1) {
        g_animRects[i++] = { BUFFER_OLD, g_minRects[0], g_minRects[1], 0, OPACITY_FADE_OUT, POSITION_TOP_LEFT };
        g_animRects[i++] = { BUFFER_OLD, g_maxRect, g_minRects[0], 0, OPACITY_FADE_OUT, POSITION_TOP_LEFT };
        g_animRects[i++] = { BUFFER_OLD, g_minRects[1], g_maxRect, 0, OPACITY_FADE_OUT, POSITION_TOP_LEFT };

        g_animRects[i++] = { BUFFER_NEW, g_minRects[0], g_minRects[1], 0, OPACITY_FADE_IN, POSITION_TOP_LEFT };
        g_animRects[i++] = { BUFFER_NEW, g_maxRect, g_minRects[0], 0, OPACITY_FADE_IN, POSITION_TOP_LEFT };
        g_animRects[i++] = { BUFFER_NEW, g_minRects[1], g_maxRect, 0, OPACITY_FADE_IN, POSITION_TOP_LEFT };
    } else {
        g_animRects[i++] = { BUFFER_NEW, g_minRects[0], g_minRects[0], 0, OPACITY_SOLID, POSITION_CENTER };
        g_animRects[i++] = { BUFFER_NEW, g_minRects[1], g_minRects[1], 0, OPACITY_SOLID, POSITION_CENTER };

        g_animRects[i++] = { BUFFER_OLD, g_maxRectMax, g_maxRectMin, 0, OPACITY_FADE_OUT, POSITION_TOP_LEFT };
        g_animRects[i++] = { BUFFER_OLD, g_maxRectMin, g_maxRectMax, 0, OPACITY_FADE_OUT, POSITION_TOP_LEFT };

        g_animRects[i++] = { BUFFER_NEW, g_maxRectMax, g_maxRectMin, 0, OPACITY_FADE_IN, POSITION_TOP_LEFT };
        g_animRects[i++] = { BUFFER_NEW, g_maxRectMin, g_maxRectMax, 0, OPACITY_FADE_IN, POSITION_TOP_LEFT };
    }

    animateRects(&g_psuAppContext, BUFFER_OLD, i);
}

void animateFromMicroViewToMaxView() {
    int iMax = hmi::g_selectedSlotIndex;
    int iMin1 = iMax == 0 ? 1 : 0;
    int iMin2 = iMax == 2 ? 1 : 2;

    int i = 0;

    g_animRects[i++] = { BUFFER_SOLID_COLOR, g_workingAreaRect, g_workingAreaRect, 0, OPACITY_SOLID, POSITION_TOP_LEFT };

    g_animRects[i++] = { BUFFER_OLD, g_microRects[iMin1], g_minRects[0], 0, OPACITY_FADE_OUT, POSITION_TOP_LEFT };
    g_animRects[i++] = { BUFFER_OLD, g_microRects[iMin2], g_minRects[1], 0, OPACITY_FADE_OUT, POSITION_TOP_LEFT };
    g_animRects[i++] = { BUFFER_OLD, g_microRects[iMax], g_maxRect, 0, OPACITY_FADE_OUT, iMax == 1 ? POSITION_CENTER : POSITION_TOP };

    g_animRects[i++] = { BUFFER_NEW, g_microRects[iMin1], g_minRects[0], 0, OPACITY_FADE_IN, POSITION_TOP_LEFT };
    g_animRects[i++] = { BUFFER_NEW, g_microRects[iMin2], g_minRects[1], 0, OPACITY_FADE_IN, POSITION_TOP_LEFT };
    g_animRects[i++] = { BUFFER_NEW, g_microRects[iMax], g_maxRect, 0, OPACITY_FADE_IN, POSITION_BOTTOM};
    
    animateRects(&g_psuAppContext, BUFFER_OLD, i);
}

void animateShowSysSettings() {
    auto g_defRects = psu::persist_conf::devConf.channelsViewMode == CHANNELS_VIEW_MODE_NUMERIC || 
        psu::persist_conf::devConf.channelsViewMode == CHANNELS_VIEW_MODE_VERT_BAR ? g_vertDefRects : g_horzDefRects;

    int i = 0;

    g_animRects[i++] = { BUFFER_SOLID_COLOR, g_workingAreaRect, g_workingAreaRect, 0, OPACITY_SOLID, POSITION_TOP_LEFT };

    g_animRects[i++] = { BUFFER_OLD, g_defRects[0], g_microRects[0], 0, OPACITY_FADE_OUT, POSITION_TOP_LEFT };
    g_animRects[i++] = { BUFFER_OLD, g_defRects[1], g_microRects[1], 0, OPACITY_FADE_OUT, POSITION_TOP };
    g_animRects[i++] = { BUFFER_OLD, g_defRects[2], g_microRects[2], 0, OPACITY_FADE_OUT, POSITION_TOP_RIGHT };

    g_animRects[i++] = { BUFFER_NEW, g_defRects[0], g_microRects[0], 0, OPACITY_FADE_IN, POSITION_TOP_LEFT };
    g_animRects[i++] = { BUFFER_NEW, g_defRects[1], g_microRects[1], 0, OPACITY_FADE_IN, POSITION_TOP };
    g_animRects[i++] = { BUFFER_NEW, g_defRects[2], g_microRects[2], 0, OPACITY_FADE_IN, POSITION_TOP_RIGHT };

    g_animRects[i++] = { BUFFER_NEW, g_settingsRectTop, g_settingsRect, 0, OPACITY_SOLID, POSITION_TOP_LEFT };

    g_animRects[i++] = { BUFFER_NEW, g_statusLineRectBottom, g_statusLineRect, 0, OPACITY_SOLID, POSITION_TOP_LEFT };

    animateRects(&g_psuAppContext, BUFFER_OLD, i);
}

void animateHideSysSettings() {
    auto g_defRects = psu::persist_conf::devConf.channelsViewMode == CHANNELS_VIEW_MODE_NUMERIC ||
        psu::persist_conf::devConf.channelsViewMode == CHANNELS_VIEW_MODE_VERT_BAR ? g_vertDefRects : g_horzDefRects;

    int i = 0;

    g_animRects[i++] = { BUFFER_SOLID_COLOR, g_workingAreaRect, g_workingAreaRect, 0, OPACITY_SOLID, POSITION_TOP_LEFT };

    g_animRects[i++] = { BUFFER_OLD, g_microRects[0], g_defRects[0], 0, OPACITY_FADE_OUT, POSITION_TOP_LEFT };
    g_animRects[i++] = { BUFFER_OLD, g_microRects[1], g_defRects[1], 0, OPACITY_FADE_OUT, POSITION_TOP_LEFT };
    g_animRects[i++] = { BUFFER_OLD, g_microRects[2], g_defRects[2], 0, OPACITY_FADE_OUT, POSITION_TOP_LEFT };

    g_animRects[i++] = { BUFFER_NEW, g_microRects[0], g_defRects[0], 0, OPACITY_FADE_IN, POSITION_TOP_LEFT };
    g_animRects[i++] = { BUFFER_NEW, g_microRects[1], g_defRects[1], 0, OPACITY_FADE_IN, POSITION_TOP_LEFT };
    g_animRects[i++] = { BUFFER_NEW, g_microRects[2], g_defRects[2], 0, OPACITY_FADE_IN, POSITION_TOP_LEFT };

    g_animRects[i++] = { BUFFER_OLD, g_settingsRect, g_settingsRectTop, 0, OPACITY_SOLID, POSITION_TOP_LEFT };

    g_animRects[i++] = { BUFFER_OLD, g_statusLineRect, g_statusLineRectBottom, 0, OPACITY_SOLID, POSITION_TOP_LEFT };

    animateRects(&g_psuAppContext, BUFFER_NEW, i);
}

void animateSettingsSlideLeft(bool noChannels) {
    int i = 0;

    g_animRects[i++] = { BUFFER_OLD, noChannels ? g_settingsRectNoChannels : g_settingsRect, g_settingsRectLeft, 0, OPACITY_SOLID, POSITION_TOP_LEFT };
    g_animRects[i++] = { BUFFER_NEW, g_settingsRectRight, noChannels ? g_settingsRectNoChannels : g_settingsRect, 0, OPACITY_SOLID, POSITION_TOP_LEFT };

    g_animRects[i++] = { BUFFER_OLD, g_statusLineRect, g_statusLineRectLeft, 0, OPACITY_SOLID, POSITION_TOP_LEFT };
    g_animRects[i++] = { BUFFER_NEW, g_statusLineRectRight, g_statusLineRect, 0, OPACITY_SOLID, POSITION_TOP_LEFT };

    animateRects(&g_psuAppContext, BUFFER_NEW, i);
}

void animateSettingsSlideRight(bool noChannels) {
    int i = 0;

    g_animRects[i++] = { BUFFER_OLD, noChannels ? g_settingsRectNoChannels : g_settingsRect, g_settingsRectRight, 0, OPACITY_SOLID, POSITION_TOP_LEFT };
    g_animRects[i++] = { BUFFER_NEW, g_settingsRectLeft, noChannels ? g_settingsRectNoChannels : g_settingsRect, 0, OPACITY_SOLID, POSITION_TOP_LEFT };

    g_animRects[i++] = { BUFFER_OLD, g_statusLineRect, g_statusLineRectRight, 0, OPACITY_SOLID, POSITION_TOP_LEFT };
    g_animRects[i++] = { BUFFER_NEW, g_statusLineRectLeft, g_statusLineRect, 0, OPACITY_SOLID, POSITION_TOP_LEFT };

    animateRects(&g_psuAppContext, BUFFER_NEW, i);
}

void animateSlideUp() {
    int i = 0;

    g_animRects[i++] = { BUFFER_OLD, g_workingAreaRect, g_workingAreaRectTop, 0, OPACITY_SOLID, POSITION_TOP_LEFT };

    g_animRects[i++] = { BUFFER_OLD, g_statusLineRect, g_statusLineRectBottom, 0, OPACITY_SOLID, POSITION_TOP_LEFT };

    animateRects(&g_psuAppContext, BUFFER_NEW, i);
}

void animateSlideDown() {
    int i = 0;

    g_animRects[i++] = { BUFFER_NEW, g_workingAreaRectTop, g_workingAreaRect, 0, OPACITY_SOLID, POSITION_TOP_LEFT };

    g_animRects[i++] = { BUFFER_NEW, g_statusLineRectBottom, g_statusLineRect, 0, OPACITY_SOLID, POSITION_TOP_LEFT };

    animateRects(&g_psuAppContext, BUFFER_OLD, i);
}

void animateSlideLeft() {
    int i = 0;

    g_animRects[i++] = { BUFFER_OLD, g_workingAreaRect, g_workingAreaRectLeft, 0, OPACITY_SOLID, POSITION_TOP_LEFT };
    g_animRects[i++] = { BUFFER_NEW, g_workingAreaRectRight, g_workingAreaRect, 0, OPACITY_SOLID, POSITION_TOP_LEFT };

    animateRects(&g_psuAppContext, BUFFER_NEW, i);

    g_animationState.easingRects = remapInOutQuad;
}

void animateSlideRight() {
    int i = 0;

    g_animRects[i++] = { BUFFER_OLD, g_workingAreaRect, g_workingAreaRectRight, 0, OPACITY_SOLID, POSITION_TOP_LEFT };
    g_animRects[i++] = { BUFFER_NEW, g_workingAreaRectLeft, g_workingAreaRect, 0, OPACITY_SOLID, POSITION_TOP_LEFT };

    animateRects(&g_psuAppContext, BUFFER_NEW, i);

    g_animationState.easingRects = remapInOutQuad;
}

void animateFadeOutFadeIn() {
    int i = 0;
    g_animRects[i++] = { BUFFER_SOLID_COLOR, g_displayRect, g_displayRect, 0, OPACITY_SOLID, POSITION_TOP_LEFT };
    g_animRects[i++] = { BUFFER_OLD, g_displayRect, g_displayRect, 0, OPACITY_FADE_OUT, POSITION_TOP_LEFT };
    g_animRects[i++] = { BUFFER_NEW, g_displayRect, g_displayRect, 0, OPACITY_FADE_IN, POSITION_TOP_LEFT };
    animateRects(&g_psuAppContext, BUFFER_NEW, i, 2 * psu::persist_conf::devConf.animationsDuration);
}

void animateFadeOutFadeInWorkingArea() {
    int i = 0;
    g_animRects[i++] = { BUFFER_SOLID_COLOR, g_workingAreaRect, g_workingAreaRect, 0, OPACITY_SOLID, POSITION_TOP_LEFT };
    g_animRects[i++] = { BUFFER_OLD, g_workingAreaRect, g_workingAreaRect, 0, OPACITY_FADE_OUT, POSITION_TOP_LEFT };
    g_animRects[i++] = { BUFFER_NEW, g_workingAreaRect, g_workingAreaRect, 0, OPACITY_FADE_IN, POSITION_TOP_LEFT };
    animateRects(&g_psuAppContext, BUFFER_NEW, i, 2 * psu::persist_conf::devConf.animationsDuration);
}

} // namespace gui
} // namespace psu
} // namespace eez

#endif
