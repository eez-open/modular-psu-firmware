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

#include <eez/os.h>
#include <eez/sound.h>

#include <eez/gui/gui.h>
#include <eez/gui/touch_filter.h>

#include <eez/gui/touch_calibration.h>

#define CONF_GUI_TOUCH_CALIBRATION_M 17
#define TOUCH_POINT_ACTIVATION_THRESHOLD 200

////////////////////////////////////////////////////////////////////////////////

namespace eez {
namespace gui {

static AppContext *g_appContext;

static uint32_t g_pointStartTime;

static struct {
    int x;
    int y;
} g_points[3];
static int g_currentPoint;

bool isTouchCalibrated() {
    bool success;

	int16_t touchScreenCalTlx;
	int16_t touchScreenCalTly;
	int16_t touchScreenCalBrx;
	int16_t touchScreenCalBry;
	int16_t touchScreenCalTrx;
	int16_t touchScreenCalTry;

	getTouchScreenCalibrationParamsHook(
		touchScreenCalTlx, touchScreenCalTly,
		touchScreenCalBrx, touchScreenCalBry,
		touchScreenCalTrx, touchScreenCalTry
	);

    success = touch::calibrateTransform(
        touchScreenCalTlx, touchScreenCalTly,
        touchScreenCalBrx, touchScreenCalBry,
        touchScreenCalTrx, touchScreenCalTry,
        CONF_GUI_TOUCH_CALIBRATION_M, display::getDisplayWidth(), display::getDisplayHeight());
    return success;
}

void startCalibration() {
    touch::resetTransformCalibration();
    g_currentPoint = 0;
    g_pointStartTime = millis();
}

void enterTouchCalibration(AppContext *appContext) {
    g_appContext = appContext;
	onEnterTouchCalibrationHook();
    startCalibration();
}

void touchCalibrationDialogYes() {
	setTouchScreenCalibrationParamsHook(g_points[0].x, g_points[0].y, g_points[1].x, g_points[1].y, g_points[2].x, g_points[2].y);
	onTouchCalibrationOkHook();
}

void touchCalibrationDialogNo() {
    startCalibration();
}

void touchCalibrationDialogCancel() {
	onTouchCalibrationCancelHook();
}

void selectTouchCalibrationPoint() {
    g_points[g_currentPoint].x = touch::getX();
    g_points[g_currentPoint].y = touch::getY();

    g_currentPoint++;
    g_pointStartTime = millis();

    if (g_currentPoint == 3) {
        g_currentPoint = 0;

        bool success = touch::calibrateTransform(
            g_points[0].x, g_points[0].y, g_points[1].x, g_points[1].y, g_points[2].x,
            g_points[2].y, CONF_GUI_TOUCH_CALIBRATION_M, display::getDisplayWidth(), display::getDisplayHeight());

        if (success) {
			onTouchCalibrationConfirmHook();
        } else {
            startCalibration();
			g_appContext->errorMessage("Received data is invalid due to\nimprecise pointing or\ncommunication problem!", true);
        }
    }
}

bool isTouchPointActivated() {
    return millis() - g_pointStartTime > TOUCH_POINT_ACTIVATION_THRESHOLD;
}

void findActiveWidget() {
    if (g_activeWidget) {
        return;
    }

    const WidgetCursor& widgetCursor = g_widgetCursor;

    if (widgetCursor.appContext->getActivePageId() == PAGE_ID_TOUCH_CALIBRATION) {
        if (widgetCursor.widget->type == WIDGET_TYPE_TEXT) {
            g_activeWidget = widgetCursor;
        }
    }    
}

void onTouchCalibrationPageTouch(const WidgetCursor &foundWidget, Event &touchEvent) {
    if (touchEvent.type == EVENT_TYPE_TOUCH_DOWN) {
        g_pointStartTime = millis();
    } else if (touchEvent.type == EVENT_TYPE_TOUCH_MOVE) {
        if (!g_activeWidget && isTouchPointActivated()) {
			forEachWidget(findActiveWidget);
        }
    } else if (touchEvent.type == EVENT_TYPE_TOUCH_UP) {
        if (isTouchPointActivated()) {
            g_activeWidget = 0;
            selectTouchCalibrationPoint();
        }
    }
}

void data_touch_calibration_point(DataOperationEnum operation, const WidgetCursor& widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = Value(g_currentPoint);
    }
}

} // namespace gui
} // namespace eez
