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

#include <eez/gui/gui.h>
using namespace eez::gui;

#include <eez/modules/psu/calibration.h>

namespace eez {
namespace psu {
namespace gui {

void drawCalibrationChart(calibration::CalibrationBase &calibrationBase, const WidgetCursor &widgetCursor);
float remapDacValue(calibration::CalibrationBase &calibrationBase, CalibrationValueConfiguration &configuration, Unit unit, float value);
void getCalibrationValueTypeInfo(char *text, int count);

class ChSettingsCalibrationEditPage : public SetPage {
    friend void drawCalibrationChart(calibration::CalibrationBase &calibrationBase, const WidgetCursor &widgetCursor);

public:
    static void start();

    void pageAlloc();
    void pageFree();

    int getDirty();
    void set();

    int getChartVersion() { return m_chartVersion; }
    int getChartZoom() { return m_chartZoom; }
    void zoomChart();

    bool isCalibrationValueTypeSelectable();
    CalibrationValueType getCalibrationValueType();
    void getCalibrationValueTypeInfo(char *text, int count);
    void setCalibrationValueType(CalibrationValueType type);

    float getDacValue();
    void setDacValue(float value);

    bool isCalibrationValueSource();

    // get and set value measured with external instrument
    float getMeasuredValue();
    void setMeasuredValue(float value);
    bool canEditMeasuredValue();

    float getLiveMeasuredValue();

    bool canMoveToPreviousPoint();
    void moveToPreviousPoint();

    bool canMoveToNextPoint();
    void moveToNextPoint();

    bool canEditSetValue();

    bool canSavePoint();
    void savePoint();

    bool canDeletePoint();
    void deletePoint();

    int getCurrentPointIndex();
    unsigned int getNumPoints();

private:
    uint32_t m_version;
    CalibrationValueType m_calibrationValueType;
    float m_dacValue;
    float m_measuredValue;
    bool m_measuredValueChanged;
    uint32_t m_chartVersion;
    uint16_t m_chartZoom;

    static void onStartPasswordOk();

    static void onSetRemarkOk(char *remark);

    calibration::Value &getCalibrationValue();

    float getAdcValue();
    int compareValues(float value, float value2);
    void selectPointAtIndex(int8_t i);
};

class ChSettingsCalibrationViewPage : public Page {
    friend void drawCalibrationChart(calibration::CalibrationBase &calibrationBase, const WidgetCursor &widgetCursor);

public:
    static void start();

    void pageAlloc();

    int getChartVersion() { return m_chartVersion; }
    int getChartZoom() { return m_chartZoom; }
    void zoomChart();

    bool isCalibrationValueTypeSelectable();
    CalibrationValueType getCalibrationValueType();
    void getCalibrationValueTypeInfo(char *text, int count);
    void setCalibrationValueType(CalibrationValueType type);

    bool isCalibrationValueSource();

    float getDacValue();
    float getMeasuredValue();

    bool canMoveToPreviousPoint();
    void moveToPreviousPoint();

    bool canMoveToNextPoint();
    void moveToNextPoint();

    int getCurrentPointIndex();
    unsigned int getNumPoints();

private:
    CalibrationValueType m_calibrationValueType;
    int m_selectedPointIndex;
    uint32_t m_chartVersion;
    uint16_t m_chartZoom;

    CalibrationConfiguration m_calConf;
    CalibrationValueConfiguration &getCalibrationValueConfiguration();

    void selectPointAtIndex(int i);
};

} // namespace gui
} // namespace psu
} // namespace eez
