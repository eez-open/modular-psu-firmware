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

static const Rect g_workingAreaRectTop = { 0, -240, 480, 240 };
//static const Rect g_workingAreaRectBottom = { 0, 240, 480, 240 };
static const Rect g_workingAreaRectLeft = { -480, 0, 480, 240 };
static const Rect g_workingAreaRectRight = { 480, 0, 480, 240 };

static const Rect g_workingAreaRectWithoutHeader = { 0, 32, 480, 208 };
static const Rect g_workingAreaRectLeftWithoutHeader = { -480, 32, 480, 208 };
static const Rect g_workingAreaRectRightWithoutHeader = { 480, 32, 480, 208 };

static const Rect g_statusLineRect = { 0, 240, 480, 32 };
static const Rect g_statusLineRectBottom = { 0, 272, 480, 32 };

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

static const Rect g_vertDefRectsCol2Mode[] = {
	{   0, 0, 240, 240 },
	{ 240, 0, 240, 240 },
	{ 240, 0, 240, 240 },
};

static const Rect g_horzDefRectsCol2Mode[] = {
	{ 0,   0, 480, 120 },
	{ 0, 120, 480, 120 },
	{ 0, 120, 480, 120 },
};

static const Rect g_fullScreenRect = { 0, 0, 480, 240 };

namespace eez {
namespace psu {
namespace gui {

const Rect g_workingAreaRect = { 0, 0, 480, 240 };

void animateFromDefaultViewToMaxView(int iMax) {
    int iMin1 = iMax == 0 ? 1 : 0;
    int iMin2 = iMax == 2 ? 1 : 2;

    auto defRects = g_isCol2Mode ?
		(psu::persist_conf::devConf.channelsViewMode == CHANNELS_VIEW_MODE_NUMERIC ||
        psu::persist_conf::devConf.channelsViewMode == CHANNELS_VIEW_MODE_VERT_BAR ? g_vertDefRectsCol2Mode : g_horzDefRectsCol2Mode) :
		(psu::persist_conf::devConf.channelsViewMode == CHANNELS_VIEW_MODE_NUMERIC ||
			psu::persist_conf::devConf.channelsViewMode == CHANNELS_VIEW_MODE_VERT_BAR ? g_vertDefRects : g_horzDefRects);

    int i = 0;

    g_animRects[i++] = { BUFFER_SOLID_COLOR, g_workingAreaRect, g_workingAreaRect, 0, OPACITY_SOLID, POSITION_TOP_LEFT };

    g_animRects[i++] = { BUFFER_OLD, defRects[iMin1], defRects[iMin1], 0, OPACITY_FADE_OUT, POSITION_TOP_LEFT };
    g_animRects[i++] = { BUFFER_OLD, defRects[iMin2], defRects[iMin2], 0, OPACITY_FADE_OUT, POSITION_TOP_LEFT };
    g_animRects[i++] = { BUFFER_OLD, defRects[iMax], g_fullScreenRect, 0, OPACITY_FADE_OUT, iMax == 1 ? POSITION_CENTER : POSITION_TOP };

    g_animRects[i++] = { BUFFER_NEW, defRects[iMax], g_fullScreenRect, 0, OPACITY_FADE_IN, POSITION_BOTTOM};
    
    animateRects(&g_psuAppContext, BUFFER_OLD, i);
}

void animateFromMaxViewToDefaultView(int iMax) {
    int iMin1 = iMax == 0 ? 1 : 0;
    int iMin2 = iMax == 2 ? 1 : 2;

	auto defRects = g_isCol2Mode ?
		(psu::persist_conf::devConf.channelsViewMode == CHANNELS_VIEW_MODE_NUMERIC ||
			psu::persist_conf::devConf.channelsViewMode == CHANNELS_VIEW_MODE_VERT_BAR ? g_vertDefRectsCol2Mode : g_horzDefRectsCol2Mode) :
			(psu::persist_conf::devConf.channelsViewMode == CHANNELS_VIEW_MODE_NUMERIC ||
				psu::persist_conf::devConf.channelsViewMode == CHANNELS_VIEW_MODE_VERT_BAR ? g_vertDefRects : g_horzDefRects);

    int i = 0;

    g_animRects[i++] = { BUFFER_SOLID_COLOR, g_workingAreaRect, g_workingAreaRect, 0, OPACITY_SOLID, POSITION_TOP_LEFT };

    g_animRects[i++] = { BUFFER_OLD, g_fullScreenRect, defRects[iMax], 0, OPACITY_FADE_OUT, POSITION_BOTTOM };

    g_animRects[i++] = { BUFFER_NEW, defRects[iMin1], defRects[iMin1], 0, OPACITY_FADE_IN, POSITION_TOP_LEFT };
    g_animRects[i++] = { BUFFER_NEW, defRects[iMin2], defRects[iMin2], 0, OPACITY_FADE_IN, POSITION_TOP_LEFT };
    g_animRects[i++] = { BUFFER_NEW, g_fullScreenRect, defRects[iMax], 0, OPACITY_FADE_IN, iMax == 1 ? POSITION_CENTER : POSITION_TOP };

    animateRects(&g_psuAppContext, BUFFER_OLD, i);
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

void animateSlideLeftWithoutHeader() {
	int i = 0;

	g_animRects[i++] = { BUFFER_OLD, g_workingAreaRectWithoutHeader, g_workingAreaRectLeftWithoutHeader, 0, OPACITY_SOLID, POSITION_TOP_LEFT };
	g_animRects[i++] = { BUFFER_NEW, g_workingAreaRectRightWithoutHeader, g_workingAreaRectWithoutHeader, 0, OPACITY_SOLID, POSITION_TOP_LEFT };

	animateRects(&g_psuAppContext, BUFFER_NEW, i);

	g_animationState.easingRects = remapInOutQuad;
}

void animateSlideRightWithoutHeader() {
	int i = 0;

	g_animRects[i++] = { BUFFER_OLD, g_workingAreaRectWithoutHeader, g_workingAreaRectRightWithoutHeader, 0, OPACITY_SOLID, POSITION_TOP_LEFT };
	g_animRects[i++] = { BUFFER_NEW, g_workingAreaRectLeftWithoutHeader, g_workingAreaRectWithoutHeader, 0, OPACITY_SOLID, POSITION_TOP_LEFT };

	animateRects(&g_psuAppContext, BUFFER_NEW, i);

	g_animationState.easingRects = remapInOutQuad;
}

void animateSlideLeftInDefaultViewVert(int viewIndex) {
	static const Rect g_defaultViewRectWithoutHeader[3] = {
		{ 2, 32, 154, 204 },
		{ 2 + 160, 32, 154, 204 },
		{ 2 + 320, 32, 154, 204 }
	};

	static const Rect g_defaultViewRectLeftWithoutHeader[3] = {
		{ 2 - 154, 32, 154, 204 },
		{ 2 - 154 + 160, 32, 154, 204 },
		{ 2 - 154 + 320, 32, 154, 204 }
	};

	static const Rect g_defaultViewRectRightWithoutHeader[3] = {
		{ 2 + 154, 32, 154, 204 },
		{ 2 + 154 + 160, 32, 154, 204 },
		{ 2 + 154 + 320, 32, 154, 204 }
	};

	static const Rect g_defaultView2ColRectWithoutHeader[3] = {
		{ 2, 32, 234, 204 },
		{ 2 + 240, 32, 234, 204 }
	};

	static const Rect g_defaultView2ColRectLeftWithoutHeader[3] = {
		{ 2 - 234, 32, 234, 204 },
		{ 2 - 234 + 240, 32, 234, 204 }
	};

	static const Rect g_defaultView2ColRectRightWithoutHeader[3] = {
		{ 2 + 234, 32, 234, 204 },
		{ 2 + 234 + 240, 32, 234, 204 }
	};

	int i = 0;

    if (g_isCol2Mode) {
        if (g_slotIndexes[0] == viewIndex) {
            viewIndex = 0;
        } else {
            viewIndex = 1;
        }
	    g_animRects[i++] = { BUFFER_OLD, g_defaultView2ColRectWithoutHeader[viewIndex], g_defaultView2ColRectLeftWithoutHeader[viewIndex], 0, OPACITY_SOLID, POSITION_TOP_LEFT };
    	g_animRects[i++] = { BUFFER_NEW, g_defaultView2ColRectRightWithoutHeader[viewIndex], g_defaultView2ColRectWithoutHeader[viewIndex], 0, OPACITY_SOLID, POSITION_TOP_LEFT };
    } else {
	    g_animRects[i++] = { BUFFER_OLD, g_defaultViewRectWithoutHeader[viewIndex], g_defaultViewRectLeftWithoutHeader[viewIndex], 0, OPACITY_SOLID, POSITION_TOP_LEFT };
	    g_animRects[i++] = { BUFFER_NEW, g_defaultViewRectRightWithoutHeader[viewIndex], g_defaultViewRectWithoutHeader[viewIndex], 0, OPACITY_SOLID, POSITION_TOP_LEFT };
    }

	animateRects(&g_psuAppContext, BUFFER_NEW, i, -1,
        g_isCol2Mode ? &g_defaultView2ColRectWithoutHeader[viewIndex] : &g_defaultViewRectWithoutHeader[viewIndex]);

	g_animationState.easingRects = remapInOutQuad;
}

void animateSlideLeftInDefaultViewHorz(int viewIndex) {
	static const Rect g_defaultViewRectWithoutHeader[3] = {
		{ 52, 2, 426, 74 },
		{ 52, 2 + 80, 426, 74 },
		{ 52, 2 + 160, 426, 74 }
	};

	static const Rect g_defaultViewRectLeftWithoutHeader[3] = {
		{ 52 - 426, 2, 426, 74 },
		{ 52 - 426, 2 + 80, 426, 74 },
		{ 52 - 426, 2 + 160, 426, 74 }
	};

	static const Rect g_defaultViewRectRightWithoutHeader[3] = {
		{ 52 + 426, 2, 426, 74 },
		{ 52 + 426, 2 + 80, 426, 74 },
		{ 52 + 426, 2 + 160, 426, 74 }
	};

	static const Rect g_defaultView2ColRectWithoutHeader[3] = {
		{ 52, 2, 426, 114 },
		{ 52, 2 + 120, 426, 114 },
	};

	static const Rect g_defaultView2ColRectLeftWithoutHeader[3] = {
		{ 52 - 426, 2, 426, 114 },
		{ 52 - 426, 2 + 120, 426, 114 },
	};

	static const Rect g_defaultView2ColRectRightWithoutHeader[3] = {
		{ 52 + 426, 2, 426, 114 },
		{ 52 + 426, 2 + 120, 426, 114 },
	};

	int i = 0;

	if (g_isCol2Mode) {
		if (g_slotIndexes[0] == viewIndex) {
			viewIndex = 0;
		} else {
			viewIndex = 1;
		}
		g_animRects[i++] = { BUFFER_OLD, g_defaultView2ColRectWithoutHeader[viewIndex], g_defaultView2ColRectLeftWithoutHeader[viewIndex], 0, OPACITY_SOLID, POSITION_TOP_LEFT };
		g_animRects[i++] = { BUFFER_NEW, g_defaultView2ColRectRightWithoutHeader[viewIndex], g_defaultView2ColRectWithoutHeader[viewIndex], 0, OPACITY_SOLID, POSITION_TOP_LEFT };
	} else {
		g_animRects[i++] = { BUFFER_OLD, g_defaultViewRectWithoutHeader[viewIndex], g_defaultViewRectLeftWithoutHeader[viewIndex], 0, OPACITY_SOLID, POSITION_TOP_LEFT };
		g_animRects[i++] = { BUFFER_NEW, g_defaultViewRectRightWithoutHeader[viewIndex], g_defaultViewRectWithoutHeader[viewIndex], 0, OPACITY_SOLID, POSITION_TOP_LEFT };
	}

	animateRects(&g_psuAppContext, BUFFER_NEW, i, -1,
		g_isCol2Mode ? &g_defaultView2ColRectWithoutHeader[viewIndex] : &g_defaultViewRectWithoutHeader[viewIndex]);

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

void animateFadeOutFadeIn(const Rect &rect) {
    int i = 0;
    g_animRects[i++] = { BUFFER_SOLID_COLOR, rect, rect, 0, OPACITY_SOLID, POSITION_TOP_LEFT };
    g_animRects[i++] = { BUFFER_OLD, rect, rect, 0, OPACITY_FADE_OUT, POSITION_TOP_LEFT };
    g_animRects[i++] = { BUFFER_NEW, rect, rect, 0, OPACITY_FADE_IN, POSITION_TOP_LEFT };
    animateRects(&g_psuAppContext, BUFFER_NEW, i, 2 * psu::persist_conf::devConf.animationsDuration);
}

static const Rect g_rightDrawerClosed = { 480, 0, 160, 272 };
static const Rect g_rightDrawerOpened = { 320, 0, 160, 272 };

void animateRightDrawerOpen() {
	int i = 0;

	g_animRects[i++] = { BUFFER_NEW, g_rightDrawerClosed, g_rightDrawerOpened, 0, OPACITY_SOLID, POSITION_TOP_LEFT };

	animateRects(&g_psuAppContext, BUFFER_OLD, i);
    g_animationState.easingRects = remapCubic;
}

void animateRightDrawerClose() {
	int i = 0;

	g_animRects[i++] = { BUFFER_OLD, g_rightDrawerOpened, g_rightDrawerClosed, 0, OPACITY_SOLID, POSITION_TOP_LEFT };

	animateRects(&g_psuAppContext, BUFFER_NEW, i);
    g_animationState.easingRects = remapCubic;
}

} // namespace gui
} // namespace psu
} // namespace eez

#endif
