/* / mcu / sound.h
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

#if OPTION_DISPLAY

#include <eez/modules/psu/gui/touch_calibration.h>

#include <eez/modules/psu/psu.h>
#include <eez/modules/psu/gui/psu.h>
#include <eez/sound.h>

#include <eez/gui/dialogs.h>
#include <eez/gui/document.h>
#include <eez/gui/touch.h>
#include <eez/gui/touch_filter.h>
#include <eez/modules/mcu/display.h>
#include <eez/system.h>

#define CONF_GUI_TOUCH_CALIBRATION_M 17
#define TOUCH_POINT_ACTIVATION_THRESHOLD 200

////////////////////////////////////////////////////////////////////////////////

using namespace eez::gui::touch;
using namespace eez::mcu::display;

namespace eez {
namespace psu {
namespace gui {

static uint32_t g_pointStartTime;

static struct {
    int x;
    int y;
} g_points[3];
static int g_currentPoint;

bool isTouchCalibrated() {
    bool success;
    success = touch::calibrateTransform(
        persist_conf::devConf.touch_screen_cal_tlx, persist_conf::devConf.touch_screen_cal_tly,
        persist_conf::devConf.touch_screen_cal_brx, persist_conf::devConf.touch_screen_cal_bry,
        persist_conf::devConf.touch_screen_cal_trx, persist_conf::devConf.touch_screen_cal_try,
        CONF_GUI_TOUCH_CALIBRATION_M, getDisplayWidth(), getDisplayHeight());
    return success;
}

void startCalibration() {
    touch::resetTransformCalibration();
    g_currentPoint = 0;
    g_pointStartTime = millis();
}

void enterTouchCalibration() {
    psu::gui::g_psuAppContext.replacePage(PAGE_ID_TOUCH_CALIBRATION);
    Channel::saveAndDisableOE();
    startCalibration();
}

void dialogYes() {
    persist_conf::setTouchscreenCalParams(g_points[0].x, g_points[0].y, g_points[1].x, g_points[1].y, g_points[2].x, g_points[2].y);

    if (isPageOnStack(PAGE_ID_SYS_SETTINGS_DISPLAY)) {
        popPage();
        infoMessage("Touch screen is calibrated.");
    } else {
	    showPage(PAGE_ID_MAIN);
    }

    Channel::restoreOE();
}

void dialogNo() {
    startCalibration();
}

void dialogCancel() {
    if (isPageOnStack(PAGE_ID_SYS_SETTINGS_DISPLAY)) {
        popPage();
    } else {
	    showPage(PAGE_ID_MAIN);
    }

    Channel::restoreOE();
}

void selectTouchCalibrationPoint() {
    g_points[g_currentPoint].x = getX();
    g_points[g_currentPoint].y = getY();

    g_currentPoint++;
    g_pointStartTime = millis();

    if (g_currentPoint == 3) {
        g_currentPoint = 0;

        bool success = touch::calibrateTransform(
            g_points[0].x, g_points[0].y, g_points[1].x, g_points[1].y, g_points[2].x,
            g_points[2].y, CONF_GUI_TOUCH_CALIBRATION_M, getDisplayWidth(), getDisplayHeight());

        if (success) {
            yesNoDialog(
                isPageOnStack(PAGE_ID_SYS_SETTINGS_DISPLAY) ? PAGE_ID_TOUCH_CALIBRATION_YES_NO_CANCEL : PAGE_ID_TOUCH_CALIBRATION_YES_NO,
                "Save changes?", dialogYes, dialogNo, dialogCancel
            );
        } else {
            startCalibration();
            errorMessage("Received data is invalid due to", "imprecise pointing or", "communication problem!");
        }
    }
}

void onTouchCalibrationPageTouch(const WidgetCursor &foundWidget, Event &touchEvent) {
    if (touchEvent.type == EVENT_TYPE_TOUCH_DOWN) {
        g_pointStartTime = millis();
    } else if (touchEvent.type == EVENT_TYPE_TOUCH_UP) {
        if (millis() - g_pointStartTime > TOUCH_POINT_ACTIVATION_THRESHOLD) {
            selectTouchCalibrationPoint();
        }
    }
}

bool isTouchPointActivated() {
    return millis() - g_pointStartTime > TOUCH_POINT_ACTIVATION_THRESHOLD;
}

} // namespace gui
} // namespace psu
} // namespace eez

namespace eez {
namespace gui {

void data_touch_calibration_point(data::DataOperationEnum operation, data::Cursor &cursor,
                                  data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        value = Value(psu::gui::g_currentPoint);
    }
}

} // namespace gui
} // namespace eez

#endif
