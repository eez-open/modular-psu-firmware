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

#include <eez/apps/psu/psu.h>
#include <eez/apps/psu/channel.h>
#include <eez/apps/psu/gui/psu.h>
#include <eez/apps/psu/gui/animations.h>
#include <eez/gui/gui.h>

static const Rect g_workingAreaRect = { 0, 0, 480, 240 };

static const Rect g_statusLineRects[] = {
    { 0, 272, 480, 32 },
    { 0, 240, 480, 32 }
};

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

static const Rect g_maxRect = { 0, 0, 480, 167 };

static const Rect g_minRects[] = {
    {   0, 167, 240, 74 },
    { 240, 167, 240, 74 }
};

static const Rect g_microRects[] = {
    {   0, 168, 160, 72 },
    { 160, 168, 160, 72 },
    { 320, 168, 160, 72 }
};

static const Rect g_settingsRects[] = {
    { 0, -168, 480, 168 },
    { 0, 0, 480, 168 }
};

namespace eez {
namespace psu {
namespace gui {

void animateFromDefaultViewToMaxView() {
    int iMax = g_channel->index - 1;
    int iMin1 = iMax == 0 ? 1 : 0;
    int iMin2 = iMax == 2 ? 1 : 2;

    auto g_defRects = psu::persist_conf::devConf.flags.channelsViewMode == CHANNELS_VIEW_MODE_NUMERIC || 
        psu::persist_conf::devConf.flags.channelsViewMode == CHANNELS_VIEW_MODE_HORZ_BAR ? g_vertDefRects : g_horzDefRects;

    int i = 0;

    g_animRects[i++] = { BUFFER_SOLID_COLOR, g_workingAreaRect, g_workingAreaRect, 0, OPACITY_SOLID, POSITION_TOP_LEFT };

    g_animRects[i++] = { BUFFER_OLD, g_defRects[iMin1], g_minRects[0], 0, OPACITY_FADE_OUT, POSITION_TOP_LEFT };
    g_animRects[i++] = { BUFFER_OLD, g_defRects[iMin2], g_minRects[1], 0, OPACITY_FADE_OUT, POSITION_TOP_LEFT };
    g_animRects[i++] = { BUFFER_OLD, g_defRects[iMax], g_maxRect, 0, OPACITY_FADE_OUT, iMax == 1 ? POSITION_CENTER : POSITION_TOP };

    g_animRects[i++] = { BUFFER_NEW, g_defRects[iMin1], g_minRects[0], 0, OPACITY_FADE_IN, POSITION_TOP_LEFT };
    g_animRects[i++] = { BUFFER_NEW, g_defRects[iMin2], g_minRects[1], 0, OPACITY_FADE_IN, POSITION_TOP_LEFT };
    g_animRects[i++] = { BUFFER_NEW, g_defRects[iMax], g_maxRect, 0, OPACITY_FADE_IN, POSITION_BOTTOM};
    
    animateRects(BUFFER_OLD, i);
}

void animateFromMaxViewToDefaultView() {
    int iMax = g_channel->index - 1;
    int iMin1 = iMax == 0 ? 1 : 0;
    int iMin2 = iMax == 2 ? 1 : 2;

    auto g_defRects = psu::persist_conf::devConf.flags.channelsViewMode == CHANNELS_VIEW_MODE_NUMERIC || 
        psu::persist_conf::devConf.flags.channelsViewMode == CHANNELS_VIEW_MODE_HORZ_BAR ? g_vertDefRects : g_horzDefRects;

    int i = 0;

    g_animRects[i++] = { BUFFER_SOLID_COLOR, g_workingAreaRect, g_workingAreaRect, 0, OPACITY_SOLID, POSITION_TOP_LEFT };

    g_animRects[i++] = { BUFFER_OLD, g_minRects[0], g_defRects[iMin1], 0, OPACITY_FADE_OUT, POSITION_TOP_LEFT };
    g_animRects[i++] = { BUFFER_OLD, g_minRects[1], g_defRects[iMin2], 0, OPACITY_FADE_OUT, POSITION_TOP_LEFT };
    g_animRects[i++] = { BUFFER_OLD, g_maxRect, g_defRects[iMax], 0, OPACITY_FADE_OUT, POSITION_BOTTOM };

    g_animRects[i++] = { BUFFER_NEW, g_minRects[0], g_defRects[iMin1], 0, OPACITY_FADE_IN, POSITION_TOP_LEFT };
    g_animRects[i++] = { BUFFER_NEW, g_minRects[1], g_defRects[iMin2], 0, OPACITY_FADE_IN, POSITION_TOP_LEFT };
    g_animRects[i++] = { BUFFER_NEW, g_maxRect, g_defRects[iMax], 0, OPACITY_FADE_IN, iMax == 1 ? POSITION_CENTER : POSITION_TOP };

    animateRects(BUFFER_OLD, i);
}

void animateFromMinViewToMaxView(int iMaxBefore) {
    int iMax = psu::persist_conf::devConf.flags.channelMax;

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
    }

    animateRects(BUFFER_OLD, i);
}

void animateFromMicroViewToMaxView() {
    int iMax = g_channel->index - 1;
    int iMin1 = iMax == 0 ? 1 : 0;
    int iMin2 = iMax == 2 ? 1 : 2;

    auto g_defRects = psu::persist_conf::devConf.flags.channelsViewMode == CHANNELS_VIEW_MODE_NUMERIC || 
        psu::persist_conf::devConf.flags.channelsViewMode == CHANNELS_VIEW_MODE_HORZ_BAR ? g_vertDefRects : g_horzDefRects;

    int i = 0;

    g_animRects[i++] = { BUFFER_SOLID_COLOR, g_workingAreaRect, g_workingAreaRect, 0, OPACITY_SOLID, POSITION_TOP_LEFT };

    g_animRects[i++] = { BUFFER_OLD, g_microRects[iMin1], g_minRects[0], 0, OPACITY_FADE_OUT, POSITION_TOP_LEFT };
    g_animRects[i++] = { BUFFER_OLD, g_microRects[iMin2], g_minRects[1], 0, OPACITY_FADE_OUT, POSITION_TOP_LEFT };
    g_animRects[i++] = { BUFFER_OLD, g_microRects[iMax], g_maxRect, 0, OPACITY_FADE_OUT, iMax == 1 ? POSITION_CENTER : POSITION_TOP };

    g_animRects[i++] = { BUFFER_NEW, g_microRects[iMin1], g_minRects[0], 0, OPACITY_FADE_IN, POSITION_TOP_LEFT };
    g_animRects[i++] = { BUFFER_NEW, g_microRects[iMin2], g_minRects[1], 0, OPACITY_FADE_IN, POSITION_TOP_LEFT };
    g_animRects[i++] = { BUFFER_NEW, g_microRects[iMax], g_maxRect, 0, OPACITY_FADE_IN, POSITION_BOTTOM};
    
    animateRects(BUFFER_OLD, i);
}

void animateShowSysSettings() {
    auto g_defRects = psu::persist_conf::devConf.flags.channelsViewMode == CHANNELS_VIEW_MODE_NUMERIC || 
        psu::persist_conf::devConf.flags.channelsViewMode == CHANNELS_VIEW_MODE_HORZ_BAR ? g_vertDefRects : g_horzDefRects;

    int i = 0;

    g_animRects[i++] = { BUFFER_SOLID_COLOR, g_workingAreaRect, g_workingAreaRect, 0, OPACITY_SOLID, POSITION_TOP_LEFT };

    g_animRects[i++] = { BUFFER_OLD, g_defRects[0], g_microRects[0], 0, OPACITY_FADE_OUT, POSITION_TOP_LEFT };
    g_animRects[i++] = { BUFFER_OLD, g_defRects[1], g_microRects[1], 0, OPACITY_FADE_OUT, POSITION_TOP };
    g_animRects[i++] = { BUFFER_OLD, g_defRects[2], g_microRects[2], 0, OPACITY_FADE_OUT, POSITION_TOP_RIGHT };

    g_animRects[i++] = { BUFFER_NEW, g_defRects[0], g_microRects[0], 0, OPACITY_FADE_IN, POSITION_TOP_LEFT };
    g_animRects[i++] = { BUFFER_NEW, g_defRects[1], g_microRects[1], 0, OPACITY_FADE_IN, POSITION_TOP };
    g_animRects[i++] = { BUFFER_NEW, g_defRects[2], g_microRects[2], 0, OPACITY_FADE_IN, POSITION_TOP_RIGHT };

    g_animRects[i++] = { BUFFER_NEW, g_settingsRects[0], g_settingsRects[1], 0, OPACITY_SOLID, POSITION_TOP_LEFT };

    g_animRects[i++] = { BUFFER_NEW, g_statusLineRects[0], g_statusLineRects[1], 0, OPACITY_SOLID, POSITION_TOP_LEFT };

    animateRects(BUFFER_OLD, i);
}

void animateHideSysSettings() {
    auto g_defRects = psu::persist_conf::devConf.flags.channelsViewMode == CHANNELS_VIEW_MODE_NUMERIC ||
        psu::persist_conf::devConf.flags.channelsViewMode == CHANNELS_VIEW_MODE_HORZ_BAR ? g_vertDefRects : g_horzDefRects;

    int i = 0;

    g_animRects[i++] = { BUFFER_SOLID_COLOR, g_workingAreaRect, g_workingAreaRect, 0, OPACITY_SOLID, POSITION_TOP_LEFT };

    g_animRects[i++] = { BUFFER_OLD, g_microRects[0], g_defRects[0], 0, OPACITY_FADE_OUT, POSITION_TOP_LEFT };
    g_animRects[i++] = { BUFFER_OLD, g_microRects[1], g_defRects[1], 0, OPACITY_FADE_OUT, POSITION_TOP_LEFT };
    g_animRects[i++] = { BUFFER_OLD, g_microRects[2], g_defRects[2], 0, OPACITY_FADE_OUT, POSITION_TOP_LEFT };

    g_animRects[i++] = { BUFFER_NEW, g_microRects[0], g_defRects[0], 0, OPACITY_FADE_IN, POSITION_TOP_LEFT };
    g_animRects[i++] = { BUFFER_NEW, g_microRects[1], g_defRects[1], 0, OPACITY_FADE_IN, POSITION_TOP_LEFT };
    g_animRects[i++] = { BUFFER_NEW, g_microRects[2], g_defRects[2], 0, OPACITY_FADE_IN, POSITION_TOP_LEFT };

    g_animRects[i++] = { BUFFER_OLD, g_settingsRects[1], g_settingsRects[0], 0, OPACITY_SOLID, POSITION_TOP_LEFT };

    g_animRects[i++] = { BUFFER_OLD, g_statusLineRects[1], g_statusLineRects[0], 0, OPACITY_SOLID, POSITION_TOP_LEFT };

    animateRects(BUFFER_NEW, i);
}

} // namespace gui
} // namespace psu
} // namespace eez

#endif
