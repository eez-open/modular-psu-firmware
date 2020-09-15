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

#define LIST_ITEMS_PER_PAGE 5
#define EMPTY_VALUE "\x92"

namespace eez {
namespace psu {
namespace gui {

class ChSettingsAdvOptionsPage : public Page {
  public:
    void toggleSense();
    void toggleProgramming();
    void toggleDprog();
};

class ChSettingsAdvRangesPage : public Page {
  public:
    void selectMode();
    void toggleAutoRanging();

  private:
    static void onModeSet(uint16_t value);
};

class ChSettingsAdvViewPage : public SetPage {
  public:
    void pageAlloc();

    void editDisplayValue1();
    void editDisplayValue2();
    void swapDisplayValues();
    void editYTViewRate();

    int getDirty();
    void set();

    uint8_t displayValue1;
    uint8_t displayValue2;
    float ytViewRate;

  private:
    uint8_t origDisplayValue1;
    uint8_t origDisplayValue2;
    float origYTViewRate;

    static bool isDisabledDisplayValue1(uint16_t value);
    static void onDisplayValue1Set(uint16_t value);
    static bool isDisabledDisplayValue2(uint16_t value);
    static void onDisplayValue2Set(uint16_t value);
    static void onYTViewRateSet(float value);
};

class ChSettingsProtectionPage : public Page {
  public:
    static void clear();
    static void clearAndDisable();
};

class ChSettingsProtectionSetPage : public SetPage {
  public:
    int getDirty();
    void set();

    void toggleState();
    void toggleType();
    void editLimit();
    void editLevel();
    void editDelay();

    int state;
    int type;
    Value limit;
    Value level;
    Value delay;

  protected:
    int origType;
    int origState;

    Value origLimit;
    float minLimit;
    float maxLimit;
    float defLimit;

    Value origLevel;
    float minLevel;
    float maxLevel;
    float defLevel;

    Value origDelay;
    float minDelay;
    float maxDelay;
    float defaultDelay;

    bool isLevelLimited;

    virtual void setParams(bool checkLoad) = 0;

    static void onLimitSet(float value);
    static void onLevelSet(float value);
    static void onDelaySet(float value);
    static void onSetFinish(bool showInfo);
};

class ChSettingsOvpProtectionPage : public ChSettingsProtectionSetPage {
  public:
    void pageAlloc();

  protected:
    void setParams(bool checkLoad);

  private:
    static void onSetParamsOk();
};

class ChSettingsOcpProtectionPage : public ChSettingsProtectionSetPage {
  public:
    void pageAlloc();

  protected:
    void setParams(bool checkLoad);

  private:
    static void onSetParamsOk();
};

class ChSettingsOppProtectionPage : public ChSettingsProtectionSetPage {
  public:
    void pageAlloc();

  protected:
    void setParams(bool checkLoad);

  private:
    static void onSetParamsOk();
};

class ChSettingsOtpProtectionPage : public ChSettingsProtectionSetPage {
  public:
    void pageAlloc();

  protected:
    void setParams(bool checkLoad);
};

class ChSettingsTriggerPage : public SetPage {
public:
    void pageAlloc();

    int getDirty();
    void set();

    TriggerMode triggerMode;

    float triggerVoltage;
    float voltageRampDuration;

    float triggerCurrent;
    float currentRampDuration;

    float outputDelayDuration;

    void editTriggerMode();

private:
    TriggerMode triggerModeOrig;

    float triggerVoltageOrig;
    float voltageRampDurationOrig;

    float triggerCurrentOrig;
    float currentRampDurationOrig;

    float outputDelayDurationOrig;

    static void onTriggerModeSet(uint16_t value);
};

class ChSettingsListsPage : public SetPage {
public:
    void pageAlloc();

    void previousPage();
    void nextPage();

    void edit();

    bool isFocusWidget(const WidgetCursor &widgetCursor);

    int getDirty();
    void set();

    void onEncoder(int counter);
    void onEncoderClicked();
    Unit getEncoderUnit();

    void showInsertMenu();
    void insertRowAbove();
    void insertRowBelow();

    void showDeleteMenu();
    void deleteRow();
    void clearColumn();
    void deleteRows();
    void deleteAll();

    void showFileMenu();
    void fileImport();
    void fileExport();

    uint16_t getMaxListLength();
    int getPageIndex();
    uint16_t getNumPages();
    int getRowIndex();
    void moveCursorToFirstAvailableCell();

    static void doImportList();
    void onImportListFinished(int16_t err);

    static void doExportList();
    void onExportListFinished(int16_t err);

    int m_listVersion;

    float m_voltageList[MAX_LIST_LENGTH];
    uint16_t m_voltageListLength;

    float m_currentList[MAX_LIST_LENGTH];
    uint16_t m_currentListLength;

    float m_dwellList[MAX_LIST_LENGTH];
    uint16_t m_dwellListLength;

    int m_iCursor;

    void editListCount();
    void editTriggerOnListStop();

    uint16_t m_listCount;
    TriggerOnListStop m_triggerOnListStop;

private:
    char m_listFilePath[MAX_PATH_LENGTH + 1];
    float m_voltageListLoad[MAX_LIST_LENGTH];
    uint16_t m_voltageListLengthLoad;
    float m_currentListLoad[MAX_LIST_LENGTH];
    uint16_t m_currentListLengthLoad;
    float m_dwellListLoad[MAX_LIST_LENGTH];
    uint16_t m_dwellListLengthLoad;

    uint16_t m_listCountOrig;
    TriggerOnListStop m_triggerOnListStopOrig;

    int getColumnIndex();
    int getCursorIndexWithinPage();
    int16_t getDataIdAtCursor();
    int getCursorIndex(const eez::gui::Cursor cursor, int16_t id);

    bool isFocusedValueEmpty();
    float getFocusedValue();
    void setFocusedValue(float value);
    static void onValueSet(float value);
    void doValueSet(float value);

    void insertRow(int iRow, int iCopyRow);

    static void onClearColumn();
    void doClearColumn();

    static void onDeleteRows();
    void doDeleteRows();

    static void onDeleteAll();
    void doDeleteAll();

    static void onImportListFileSelected(const char *listFilePath);
    static void onExportListFileSelected(const char *listFilePath);

    static void onListCountSet(float value);
    static void onListCountSetToInfinity();

    static void onTriggerOnListStopSet(uint16_t value);
};

void drawCalibrationChart(const WidgetCursor &widgetCursor);
float remapDacValue(CalibrationValueConfiguration &configuration, Unit unit, float value);

class ChSettingsCalibrationEditPage : public SetPage {
    friend void drawCalibrationChart(const WidgetCursor &widgetCursor);

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
    void setCalibrationValueType(CalibrationValueType type);

    void setDacValue(float value);

    // get and set value measured with external instrument
    float getMeasuredValue();
    void setMeasuredValue(float value);
    bool canEditMeasuredValue();

    bool canMoveToPreviousPoint();
    void moveToPreviousPoint();

    bool canMoveToNextPoint();
    void moveToNextPoint();

    bool canSavePoint();
    void savePoint();

    bool canDeletePoint();
    void deletePoint();

    int getCurrentPointIndex();
    unsigned int getNumPoints();

private:
    uint32_t m_version;
    CalibrationValueType m_calibrationValueType;
    float m_measuredValue;
    bool m_measuredValueChanged;
    uint32_t m_chartVersion;
    uint16_t m_chartZoom;

    static void onStartPasswordOk();

    static void onSetRemarkOk(char *remark);

    calibration::Value &getCalibrationValue();

    float getChannelDacValue();
    float getChannelAdcValue();
    int compareDacValues(float dac1, float dac2);
    void selectPointAtIndex(int8_t i);
};

class ChSettingsCalibrationViewPage : public Page {
    friend void drawCalibrationChart(const WidgetCursor &widgetCursor);

public:
    static void start();

    void pageAlloc();

    int getChartVersion() { return m_chartVersion; }
    int getChartZoom() { return m_chartZoom; }
    void zoomChart();

    bool isCalibrationValueTypeSelectable();
    CalibrationValueType getCalibrationValueType();
    void setCalibrationValueType(CalibrationValueType type);

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

    CalibrationValueConfiguration &getCalibrationValueConfiguration();

    void selectPointAtIndex(int i);
};

} // namespace gui
} // namespace psu
} // namespace eez
