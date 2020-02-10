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

#define LIST_ITEMS_PER_PAGE 5
#define EMPTY_VALUE "\x92"

namespace eez {
namespace psu {
namespace gui {

class ChSettingsTriggerPage : public Page {
  public:
    void editTriggerMode();

    void editVoltageTriggerValue();
    void editCurrentTriggerValue();

    void editTriggerOnListStop();

    void editListCount();

  private:
    static void onFinishTriggerModeSet();
    static void onTriggerModeSet(uint16_t value);

    static void onTriggerOnListStopSet(uint16_t value);

    static void onVoltageTriggerValueSet(float value);
    static void onCurrentTriggerValueSet(float value);

    static void onListCountSet(float value);
    static void onListCountSetToInfinity();
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

private:
    char m_listFilePath[MAX_PATH_LENGTH + 1];
    float m_voltageListLoad[MAX_LIST_LENGTH];
    uint16_t m_voltageListLengthLoad;
    float m_currentListLoad[MAX_LIST_LENGTH];
    uint16_t m_currentListLengthLoad;
    float m_dwellListLoad[MAX_LIST_LENGTH];
    uint16_t m_dwellListLengthLoad;

    int getColumnIndex();
    int getCursorIndexWithinPage();
    uint8_t getDataIdAtCursor();
    int getCursorIndex(const eez::gui::data::Cursor &cursor, uint16_t id);

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
};

} // namespace gui
} // namespace psu
} // namespace eez
