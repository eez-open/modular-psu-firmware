/*
 * EEZ Home Application
 * Copyright (C) 2018-present, Envox d.o.o.
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

#include <eez/apps/home/home.h>

#include <eez/apps/home/data.h>
#include <eez/apps/home/touch_calibration.h>
#include <eez/gui/gui.h>
#include <eez/gui/touch.h>
#include <eez/index.h>
#include <eez/system.h>

// TODO this must be removed from here
#include <eez/apps/psu/psu.h>

#include <eez/apps/psu/devices.h>
#include <eez/apps/psu/idle.h>

#define CONF_GUI_ENTERING_STANDBY_PAGE_TIMEOUT 2000000L // 2s
#define CONF_GUI_STANDBY_PAGE_TIMEOUT 10000000L         // 10s
#define CONF_GUI_DISPLAY_OFF_PAGE_TIMEOUT 2000000L      // 2s
#define CONF_GUI_WELCOME_PAGE_TIMEOUT 2000000L          // 2s

namespace eez {
namespace home {

HomeAppContext g_homeAppContext;

////////////////////////////////////////////////////////////////////////////////

HomeAppContext::HomeAppContext() {
    showPage(PAGE_ID_WELCOME);
}

void HomeAppContext::stateManagment() {
    AppContext::stateManagment();

    uint32_t tickCount = micros();

    // wait some time for transitional pages
    int activePageId = getActivePageId();
    if (activePageId == PAGE_ID_STANDBY) {
        if (int32_t(tickCount - getShowPageTime()) < CONF_GUI_STANDBY_PAGE_TIMEOUT) {
            return;
        }
    } else if (activePageId == PAGE_ID_ENTERING_STANDBY) {
        if (int32_t(tickCount - getShowPageTime()) >= CONF_GUI_ENTERING_STANDBY_PAGE_TIMEOUT) {
            uint32_t saved_showPageTime = getShowPageTime();
            showStandbyPage();
            setShowPageTime(saved_showPageTime);
        }
        return;
    } else if (activePageId == PAGE_ID_WELCOME) {
        if (int32_t(tickCount - getShowPageTime()) < CONF_GUI_WELCOME_PAGE_TIMEOUT) {
            return;
        }
    }

    // turn the screen off if power is down
    if (!psu::isPowerUp()) {
        showPage(INTERNAL_PAGE_ID_NONE);
        eez::mcu::display::turnOff();
        return;
    }

    // select page to go after transitional page
    if (activePageId == PAGE_ID_WELCOME || activePageId == PAGE_ID_STANDBY ||
        activePageId == PAGE_ID_ENTERING_STANDBY) {
        if (!isTouchCalibrated()) {
            // touch screen is not calibrated
            showPage(PAGE_ID_TOUCH_CALIBRATION_INTRO);
        } else {
            showPage(getMainPageId());
        }
        return;
    }

    // turn display on/off depending on displayState
    if (psu::persist_conf::devConf2.flags.displayState == 0 &&
        (activePageId != PAGE_ID_DISPLAY_OFF && activePageId != PAGE_ID_SELF_TEST_RESULT &&
         isTouchCalibrated())) {
        showPage(PAGE_ID_DISPLAY_OFF);
        return;
    } else if (psu::persist_conf::devConf2.flags.displayState == 1 &&
               activePageId == PAGE_ID_DISPLAY_OFF) {
        eez::mcu::display::turnOn();
        showPage(getMainPageId());
        return;
    }

    // start touch screen calibration automatically after period of time
    uint32_t inactivityPeriod = psu::idle::getGuiAndEncoderInactivityPeriod();
    if (activePageId == PAGE_ID_TOUCH_CALIBRATION_INTRO) {
        if (inactivityPeriod >= 20 * 1000UL) {
            enterTouchCalibration(PAGE_ID_TOUCH_CALIBRATION_YES_NO, getMainPageId());
            return;
        }
    }

    // handling of display off page
    if (activePageId == PAGE_ID_DISPLAY_OFF) {
        if (eez::mcu::display::isOn()) {
            if (int32_t(tickCount - getShowPageTime()) >= CONF_GUI_DISPLAY_OFF_PAGE_TIMEOUT) {
                eez::mcu::display::turnOff();
                setShowPageTime(tickCount);
            }
        }
        return;
    }
}

bool HomeAppContext::isActiveWidget(const WidgetCursor &widgetCursor) {
    if (getActivePageId() == PAGE_ID_TOUCH_CALIBRATION) {
        if (touch::g_eventType != EVENT_TYPE_TOUCH_NONE) {
            if (widgetCursor.widget->type == WIDGET_TYPE_TEXT && isTouchPointActivated()) {
                return true;
            }
        }
    }

    return AppContext::isActiveWidget(widgetCursor);
}

int HomeAppContext::getMainPageId() {
    return PAGE_ID_HOME_PSU;
}

void HomeAppContext::onPageTouch(const WidgetCursor &foundWidget, Event &touchEvent) {
#ifdef DEBUG
    if (touchEvent.type == EVENT_TYPE_TOUCH_MOVE &&
        (getActivePageId() == PAGE_ID_TOUCH_CALIBRATION_YES_NO ||
         getActivePageId() == PAGE_ID_TOUCH_CALIBRATION_YES_NO_CANCEL)) {
        int x = touchEvent.x;
        if (x < 1)
            x = 1;
        else if (x > eez::mcu::display::getDisplayWidth() - 2)
            x = eez::mcu::display::getDisplayWidth() - 2;

        int y = touchEvent.y;
        if (y < 1)
            y = 1;
        else if (y > eez::mcu::display::getDisplayHeight() - 2)
            y = eez::mcu::display::getDisplayHeight() - 2;

        eez::mcu::display::setColor(255, 255, 255);
        eez::mcu::display::fillRect(x - 1, y - 1, x + 1, y + 1);

        g_painted = true;
    }
#endif

    if (getActivePageId() == PAGE_ID_TOUCH_CALIBRATION_INTRO) {
        if (touchEvent.type == EVENT_TYPE_TOUCH_UP) {
            enterTouchCalibration(PAGE_ID_TOUCH_CALIBRATION_YES_NO, getMainPageId());
        }
    } else if (getActivePageId() == PAGE_ID_TOUCH_CALIBRATION) {
        onTouchCalibrationPageTouch(foundWidget, touchEvent);
    }

} // namespace home

////////////////////////////////////////////////////////////////////////////////

static gui::Rect g_lastAnimateOpenSrcRect;

// TODO change hardcoding
#ifdef EEZ_PLATFORM_SIMULATOR
static const gui::Rect g_animateOpenDstRect = { 331, 41, 480, 272 };
#else
static const gui::Rect g_animateOpenDstRect = { 0, 0, 480, 272 };
#endif

void openApplication() {
    // open application
    const WidgetCursor &widgetCursor = gui::getFoundWidgetAtDown();
    g_foregroundApplicationIndex = widgetCursor.cursor.i;

    // animate open
    const Widget *widget = widgetCursor.widget;
    g_lastAnimateOpenSrcRect = {
        int16_t(widgetCursor.x + widget->x + 28 + 32),
        int16_t(widgetCursor.y + widget->y + 32),
        0,
        0,
    };
    animateOpen(g_lastAnimateOpenSrcRect, g_animateOpenDstRect);
}

void closeApplication() {
    // close application
    g_foregroundApplicationIndex = -1;

    // animate close
    animateClose(g_animateOpenDstRect, g_lastAnimateOpenSrcRect);
}

} // namespace home
} // namespace eez

namespace eez {
namespace gui {

#if !defined(_MSC_VER) || !defined(EEZ_PLATFORM_SIMULATOR)
__attribute__((weak)) AppContext &getRootAppContext() {
    return home::g_homeAppContext;
}
#endif

} // namespace gui
} // namespace eez

#endif
