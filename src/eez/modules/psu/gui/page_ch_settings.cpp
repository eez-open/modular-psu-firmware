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

#if OPTION_DISPLAY

#include <string.h>

#include <eez/system.h>

#include <eez/scpi/scpi.h>

#include <eez/modules/psu/psu.h>
#include <eez/modules/psu/channel_dispatcher.h>
#include <eez/modules/psu/datetime.h>
#include <eez/modules/psu/list_program.h>
#include <eez/modules/psu/profile.h>
#include <eez/modules/psu/temperature.h>
#include <eez/modules/psu/trigger.h>

#include <eez/modules/psu/gui/psu.h>
#include <eez/modules/psu/gui/file_manager.h>
#include <eez/modules/psu/gui/keypad.h>
#include <eez/modules/psu/gui/page_ch_settings.h>

#include <scpi/scpi.h>

namespace eez {
namespace psu {
namespace gui {

static uint8_t g_newTriggerMode;

////////////////////////////////////////////////////////////////////////////////

void ChSettingsAdvOptionsPage::toggleSense() {
    channel_dispatcher::remoteSensingEnable(*g_channel, !g_channel->isRemoteSensingEnabled());
}

void ChSettingsAdvOptionsPage::toggleProgramming() {
    g_channel->remoteProgrammingEnable(!g_channel->isRemoteProgrammingEnabled());
}

void ChSettingsAdvOptionsPage::toggleDprog() {
    if (g_channel->flags.dprogState == DPROG_STATE_OFF) {
        g_channel->setDprogState(DPROG_STATE_ON);
    } else {
        g_channel->setDprogState(DPROG_STATE_OFF);
    }
}

////////////////////////////////////////////////////////////////////////////////

void ChSettingsAdvRangesPage::onModeSet(uint16_t value) {
    popPage();
    channel_dispatcher::setCurrentRangeSelectionMode(*g_channel, (CurrentRangeSelectionMode)value);
}

void ChSettingsAdvRangesPage::selectMode() {
    pushSelectFromEnumPage(ENUM_DEFINITION_CHANNEL_CURRENT_RANGE_SELECTION_MODE, g_channel->getCurrentRangeSelectionMode(), 0, onModeSet);
}

void ChSettingsAdvRangesPage::toggleAutoRanging() {
    channel_dispatcher::enableAutoSelectCurrentRange(*g_channel, !g_channel->isAutoSelectCurrentRangeEnabled());
}

////////////////////////////////////////////////////////////////////////////////

void ChSettingsAdvViewPage::pageAlloc() {
    origDisplayValue1 = displayValue1 = g_channel->flags.displayValue1;
    origDisplayValue2 = displayValue2 = g_channel->flags.displayValue2;
    origYTViewRate = ytViewRate = g_channel->ytViewRate;
}

bool ChSettingsAdvViewPage::isDisabledDisplayValue1(uint16_t value) {
    ChSettingsAdvViewPage *page = (ChSettingsAdvViewPage *)getPage(PAGE_ID_CH_SETTINGS_ADV_VIEW);
    return value == page->displayValue2;
}

void ChSettingsAdvViewPage::onDisplayValue1Set(uint16_t value) {
    popPage();
    ChSettingsAdvViewPage *page = (ChSettingsAdvViewPage *)getActivePage();
    page->displayValue1 = (uint8_t)value;
}

void ChSettingsAdvViewPage::editDisplayValue1() {
    pushSelectFromEnumPage(ENUM_DEFINITION_CHANNEL_DISPLAY_VALUE, displayValue1, isDisabledDisplayValue1, onDisplayValue1Set);
}

bool ChSettingsAdvViewPage::isDisabledDisplayValue2(uint16_t value) {
    ChSettingsAdvViewPage *page = (ChSettingsAdvViewPage *)getPage(PAGE_ID_CH_SETTINGS_ADV_VIEW);
    return value == page->displayValue1;
}

void ChSettingsAdvViewPage::onDisplayValue2Set(uint16_t value) {
    popPage();
    ChSettingsAdvViewPage *page = (ChSettingsAdvViewPage *)getActivePage();
    page->displayValue2 = (uint8_t)value;
}

void ChSettingsAdvViewPage::editDisplayValue2() {
    pushSelectFromEnumPage(ENUM_DEFINITION_CHANNEL_DISPLAY_VALUE, displayValue2, isDisabledDisplayValue2, onDisplayValue2Set);
}

void ChSettingsAdvViewPage::onYTViewRateSet(float value) {
    popPage();
    ChSettingsAdvViewPage *page = (ChSettingsAdvViewPage *)getActivePage();
    page->ytViewRate = value;
}

void ChSettingsAdvViewPage::swapDisplayValues() {
    uint8_t temp = displayValue1;
    displayValue1 = displayValue2;
    displayValue2 = temp;
}

void ChSettingsAdvViewPage::editYTViewRate() {
    NumericKeypadOptions options;

    options.editValueUnit = UNIT_SECOND;

    options.min = GUI_YT_VIEW_RATE_MIN;
    options.max = GUI_YT_VIEW_RATE_MAX;
    options.def = GUI_YT_VIEW_RATE_DEFAULT;

    options.enableDefButton();
    options.flags.signButtonEnabled = true;
    options.flags.dotButtonEnabled = true;

    NumericKeypad::start(0, MakeValue(ytViewRate, UNIT_SECOND), options, onYTViewRateSet, 0, 0);
}

int ChSettingsAdvViewPage::getDirty() {
    return (origDisplayValue1 != displayValue1 || origDisplayValue2 != displayValue2 || origYTViewRate != ytViewRate) ? 1 : 0;
}

void ChSettingsAdvViewPage::set() {
    if (getDirty()) {
        channel_dispatcher::setDisplayViewSettings(*g_channel, displayValue1, displayValue2, ytViewRate);
        g_actionExecFunctions[ACTION_ID_SHOW_CH_SETTINGS]();
        infoMessage("View settings changed!");
    }
}

////////////////////////////////////////////////////////////////////////////////

void ChSettingsProtectionPage::clear() {
    channel_dispatcher::clearProtection(*g_channel);

    popPage();
    infoMessage("Cleared!");
}

void onClearAndDisableYes() {
    channel_dispatcher::clearProtection(*g_channel);
    channel_dispatcher::disableProtection(*g_channel);

    popPage();
    infoMessage("Cleared and disabled!");
}

void ChSettingsProtectionPage::clearAndDisable() {
    areYouSure(onClearAndDisableYes);
}

////////////////////////////////////////////////////////////////////////////////

int ChSettingsProtectionSetPage::getDirty() {
    return (origState != state || origType != type || origLimit != limit || origLevel != level || origDelay != delay) ? 1 : 0;
}

void ChSettingsProtectionSetPage::onSetFinish(bool showInfo) {
    popPage();
    if (showInfo) {
        infoMessage("Protection params changed!");
    }
}

void ChSettingsProtectionSetPage::set() {
    if (getDirty()) {
        setParams(true);
    }
}

void ChSettingsProtectionSetPage::toggleState() {
    state = state ? 0 : 1;
}

void ChSettingsProtectionSetPage::toggleType() {
    type = type ? 0 : 1;
}

void ChSettingsProtectionSetPage::onLimitSet(float value) {
    popPage();
    ChSettingsProtectionSetPage *page = (ChSettingsProtectionSetPage *)getActivePage();
    page->limit = MakeValue(value, page->limit.getUnit());

    if (page->isLevelLimited) {
        if (page->level.getFloat() > page->limit.getFloat()) {
            page->level = page->limit;
        }
    }
}

void ChSettingsProtectionSetPage::editLimit() {
    NumericKeypadOptions options;

    options.channelIndex = g_channel->channelIndex;

    options.editValueUnit = limit.getUnit();

    options.min = minLimit;
    options.max = maxLimit;
    options.def = defLimit;

    options.enableMaxButton();
    options.enableDefButton();
    options.flags.signButtonEnabled = true;
    options.flags.dotButtonEnabled = true;

    NumericKeypad::start(0, limit, options, onLimitSet, 0, 0);
}

void ChSettingsProtectionSetPage::onLevelSet(float value) {
    popPage();
    ChSettingsProtectionSetPage *page = (ChSettingsProtectionSetPage *)getActivePage();
    page->level = MakeValue(value, page->level.getUnit());
}

void ChSettingsProtectionSetPage::editLevel() {
    NumericKeypadOptions options;

    options.channelIndex = g_channel->channelIndex;

    options.editValueUnit = level.getUnit();

    options.min = minLevel;
    options.max = maxLevel;
    options.def = defLevel;

    if (isLevelLimited) {
        options.max = limit.getFloat();
    }

    options.enableMaxButton();
    options.enableDefButton();
    options.flags.signButtonEnabled = true;
    options.flags.dotButtonEnabled = true;

    NumericKeypad::start(0, level, options, onLevelSet, 0, 0);
}

void ChSettingsProtectionSetPage::onDelaySet(float value) {
    popPage();
    ChSettingsProtectionSetPage *page = (ChSettingsProtectionSetPage *)getActivePage();
    page->delay = MakeValue(value, page->delay.getUnit());
}

void ChSettingsProtectionSetPage::editDelay() {
    NumericKeypadOptions options;

    options.editValueUnit = delay.getUnit();

    options.min = minDelay;
    options.max = maxDelay;
    options.def = defaultDelay;

    options.enableMaxButton();
    options.enableDefButton();
    options.flags.signButtonEnabled = true;
    options.flags.dotButtonEnabled = true;

    NumericKeypad::start(0, delay, options, onDelaySet, 0, 0);
}

////////////////////////////////////////////////////////////////////////////////

void ChSettingsOvpProtectionPage::pageAlloc() {
    origState = state = g_channel->prot_conf.flags.u_state ? 1 : 0;
    origType = type = g_channel->prot_conf.flags.u_type ? 1 : 0;

    origLimit = limit = MakeValue(channel_dispatcher::getULimit(*g_channel), UNIT_VOLT);
    minLimit = channel_dispatcher::getUMin(*g_channel);
    maxLimit = channel_dispatcher::getUMaxOvpLimit(*g_channel);
    defLimit = channel_dispatcher::getUMax(*g_channel);

    origLevel = level = MakeValue(channel_dispatcher::getUProtectionLevel(*g_channel), UNIT_VOLT);
    minLevel = channel_dispatcher::getUSet(*g_channel);
    maxLevel = channel_dispatcher::getUMaxOvpLevel(*g_channel);
    defLevel = channel_dispatcher::getUMax(*g_channel);

    origDelay = delay = MakeValue(g_channel->prot_conf.u_delay, UNIT_SECOND);
    minDelay = g_channel->params.OVP_MIN_DELAY;
    maxDelay = g_channel->params.OVP_MAX_DELAY;
    defaultDelay = g_channel->params.OVP_DEFAULT_DELAY;

    isLevelLimited = false;
}

void ChSettingsOvpProtectionPage::onSetParamsOk() {
    ((ChSettingsOvpProtectionPage *)getActivePage())->setParams(false);
}

void ChSettingsOvpProtectionPage::setParams(bool checkLoad) {
    if (
        checkLoad && 
        g_channel->isOutputEnabled() &&
        limit.getFloat() < channel_dispatcher::getUMon(*g_channel) &&
        channel_dispatcher::getIMon(*g_channel) >= 0
    ) {
        areYouSureWithMessage("This change will affect current load.", onSetParamsOk);
    } else {
        channel_dispatcher::setVoltageLimit(*g_channel, limit.getFloat());
        channel_dispatcher::setOvpParameters(*g_channel, state, type, level.getFloat(), delay.getFloat());
        onSetFinish(checkLoad);
    }
}

////////////////////////////////////////////////////////////////////////////////

void ChSettingsOcpProtectionPage::pageAlloc() {
    origState = state = g_channel->prot_conf.flags.i_state ? 1 : 0;
    origType = type = 0;

    origLimit = limit = MakeValue(channel_dispatcher::getILimit(*g_channel), UNIT_AMPER);
    minLimit = channel_dispatcher::getIMin(*g_channel);
    maxLimit = channel_dispatcher::getIMaxLimit(*g_channel);
    defLimit = maxLimit;

    origLevel = level = 0;

    origDelay = delay = MakeValue(g_channel->prot_conf.i_delay, UNIT_SECOND);
    minDelay = g_channel->params.OCP_MIN_DELAY;
    maxDelay = g_channel->params.OCP_MAX_DELAY;
    defaultDelay = g_channel->params.OCP_DEFAULT_DELAY;

    isLevelLimited = false;
}

void ChSettingsOcpProtectionPage::onSetParamsOk() {
    ((ChSettingsOcpProtectionPage *)getActivePage())->setParams(false);
}

void ChSettingsOcpProtectionPage::setParams(bool checkLoad) {
    if (checkLoad && g_channel->isOutputEnabled() &&
        limit.getFloat() < channel_dispatcher::getIMon(*g_channel)) {
        areYouSureWithMessage("This change will affect current load.", onSetParamsOk);
    } else {
        channel_dispatcher::setCurrentLimit(*g_channel, limit.getFloat());
        channel_dispatcher::setOcpParameters(*g_channel, state, delay.getFloat());
        onSetFinish(checkLoad);
    }
}

////////////////////////////////////////////////////////////////////////////////

void ChSettingsOppProtectionPage::pageAlloc() {
    origState = state = g_channel->prot_conf.flags.p_state ? 1 : 0;
    origType = type = 0;

    origLimit = limit = MakeValue(channel_dispatcher::getPowerLimit(*g_channel), UNIT_WATT);
    minLimit = channel_dispatcher::getPowerMinLimit(*g_channel);
    maxLimit = channel_dispatcher::getPowerMaxLimit(*g_channel);
    defLimit = channel_dispatcher::getPowerDefaultLimit(*g_channel);

    origLevel = level = MakeValue(channel_dispatcher::getPowerProtectionLevel(*g_channel), UNIT_WATT);
    minLevel = channel_dispatcher::getOppMinLevel(*g_channel);
    maxLevel = channel_dispatcher::getOppMaxLevel(*g_channel);
    defLevel = channel_dispatcher::getOppDefaultLevel(*g_channel);

    origDelay = delay = MakeValue(g_channel->prot_conf.p_delay, UNIT_SECOND);
    minDelay = g_channel->params.OPP_MIN_DELAY;
    maxDelay = g_channel->params.OPP_MAX_DELAY;
    defaultDelay = g_channel->params.OPP_DEFAULT_DELAY;

    isLevelLimited = true;
}

void ChSettingsOppProtectionPage::onSetParamsOk() {
    ((ChSettingsOppProtectionPage *)getActivePage())->setParams(false);
}

void ChSettingsOppProtectionPage::setParams(bool checkLoad) {
    if (checkLoad && g_channel->isOutputEnabled()) {
        float pMon = channel_dispatcher::getUMon(*g_channel) * channel_dispatcher::getIMon(*g_channel);
        if (limit.getFloat() < pMon && channel_dispatcher::getIMon(*g_channel) >= 0) {
            areYouSureWithMessage("This change will affect current load.", onSetParamsOk);
            return;
        }
    }

    channel_dispatcher::setPowerLimit(*g_channel, limit.getFloat());
    channel_dispatcher::setOppParameters(*g_channel, state, level.getFloat(), delay.getFloat());
    onSetFinish(checkLoad);
}

////////////////////////////////////////////////////////////////////////////////

void ChSettingsOtpProtectionPage::pageAlloc() {
    origState = state = temperature::getChannelSensorState(g_channel) ? 1 : 0;
    origType = type = 0;

    origLimit = limit = 0;
    minLimit = 0;
    maxLimit = 0;
    defLimit = 0;

    origLevel = level = MakeValue(temperature::getChannelSensorLevel(g_channel), UNIT_CELSIUS);
    minLevel = OTP_AUX_MIN_LEVEL;
    maxLevel = OTP_AUX_MAX_LEVEL;
    defLevel = OTP_AUX_DEFAULT_LEVEL;

    origDelay = delay = MakeValue(temperature::getChannelSensorDelay(g_channel), UNIT_SECOND);
    minDelay = OTP_AUX_MIN_DELAY;
    maxDelay = OTP_AUX_MAX_DELAY;
    defaultDelay = OTP_CH_DEFAULT_DELAY;

    isLevelLimited = false;
}

void ChSettingsOtpProtectionPage::setParams(bool checkLoad) {
    channel_dispatcher::setOtpParameters(*g_channel, state, level.getFloat(), delay.getFloat());
    onSetFinish(checkLoad);
}

////////////////////////////////////////////////////////////////////////////////

void ChSettingsTriggerPage::onFinishTriggerModeSet() {
    trigger::abort();
    channel_dispatcher::setVoltageTriggerMode(*g_channel, (TriggerMode)g_newTriggerMode);
    channel_dispatcher::setCurrentTriggerMode(*g_channel, (TriggerMode)g_newTriggerMode);
    channel_dispatcher::setTriggerOutputState(*g_channel, true);
}

void ChSettingsTriggerPage::onTriggerModeSet(uint16_t value) {
    popPage();

    if (channel_dispatcher::getVoltageTriggerMode(*g_channel) != value || channel_dispatcher::getCurrentTriggerMode(*g_channel) != value) {
        g_newTriggerMode = (uint8_t)value;

        if (trigger::isInitiated() || trigger::isActive()) {
            yesNoDialog(PAGE_ID_YES_NO, "Trigger is active. Are you sure?", onFinishTriggerModeSet, 0, 0);
        } else {
            onFinishTriggerModeSet();
        }
    }
}

void ChSettingsTriggerPage::editTriggerMode() {
    pushSelectFromEnumPage(ENUM_DEFINITION_CHANNEL_TRIGGER_MODE, channel_dispatcher::getVoltageTriggerMode(*g_channel), 0, onTriggerModeSet);
}

////////////////////////////////////////////////////////////////////////////////

void ChSettingsListsPage::pageAlloc() {
    m_listVersion = 0;
    m_iCursor = 0;

    float *dwellList = list::getDwellList(*g_channel, &m_dwellListLength);
    memcpy(m_dwellList, dwellList, m_dwellListLength * sizeof(float));

    float *voltageList = list::getVoltageList(*g_channel, &m_voltageListLength);
    memcpy(m_voltageList, voltageList, m_voltageListLength * sizeof(float));

    float *currentList = list::getCurrentList(*g_channel, &m_currentListLength);
    memcpy(m_currentList, currentList, m_currentListLength * sizeof(float));

    m_listCount = m_listCountOrig = list::getListCount(*g_channel);
    m_triggerOnListStop = m_triggerOnListStopOrig = channel_dispatcher::getTriggerOnListStop(*g_channel);
}

void ChSettingsListsPage::previousPage() {
    int iPage = getPageIndex();
    if (iPage > 0) {
        --iPage;
        m_iCursor = iPage * LIST_ITEMS_PER_PAGE * 3;
        moveCursorToFirstAvailableCell();
    }
}

void ChSettingsListsPage::nextPage() {
    int iPage = getPageIndex();
    if (iPage < getNumPages() - 1) {
        ++iPage;
        m_iCursor = iPage * LIST_ITEMS_PER_PAGE * 3;
        moveCursorToFirstAvailableCell();
    }
}

bool ChSettingsListsPage::isFocusedValueEmpty() {
    Cursor cursor(getCursorIndexWithinPage());
    Value value = get(cursor, getDataIdAtCursor());
    return value.getType() == VALUE_TYPE_STR;
}

float ChSettingsListsPage::getFocusedValue() {
    Cursor cursor(getCursorIndexWithinPage());

    Value value = get(cursor, getDataIdAtCursor());

    if (value.getType() == VALUE_TYPE_STR) {
        value = getDef(cursor, getDataIdAtCursor());
    }

    return value.getFloat();
}

void ChSettingsListsPage::setFocusedValue(float value) {
    Cursor cursor(getCursorIndexWithinPage());

    int16_t dataId = getDataIdAtCursor();

    Value min = getMin(cursor, dataId);
    Value max = getMax(cursor, dataId);

    if (value >= min.getFloat() && value <= max.getFloat()) {
        int iRow = getRowIndex();

        if (dataId == DATA_ID_CHANNEL_LIST_DWELL) {
            m_dwellList[iRow] = value;
            if (iRow >= m_dwellListLength) {
                m_dwellListLength = iRow + 1;
            }
        } else if (dataId == DATA_ID_CHANNEL_LIST_VOLTAGE) {
            m_voltageList[iRow] = value;
            if (iRow >= m_voltageListLength) {
                m_voltageListLength = iRow + 1;
            }
        } else {
            m_currentList[iRow] = value;
            if (iRow >= m_currentListLength) {
                m_currentListLength = iRow + 1;
            }
        }

        ++m_listVersion;
    }
}

void ChSettingsListsPage::onValueSet(float value) {
    ChSettingsListsPage *page = (ChSettingsListsPage *)getPage(PAGE_ID_CH_SETTINGS_LISTS);
    page->doValueSet(value);
}

void ChSettingsListsPage::doValueSet(float value) {
    int16_t dataId = getDataIdAtCursor();

    if (dataId != DATA_ID_CHANNEL_LIST_DWELL) {
        int iRow = getRowIndex();

        float power = 0;

        if (dataId == DATA_ID_CHANNEL_LIST_VOLTAGE) {
            if (iRow == 0 && m_voltageListLength <= 1) {
                for (int i = 0; i < m_currentListLength; ++i) {
                    if (value * m_currentList[i] > channel_dispatcher::getPowerMaxLimit(*g_channel)) {
                        errorMessage("Power limit exceeded");
                        return;
                    }
                }
            } else {
                if (iRow < m_currentListLength) {
                    power = value * m_currentList[iRow];
                } else if (m_currentListLength > 0) {
                    power = value * m_currentList[0];
                }
            }
        } else {
            if (iRow == 0 && m_currentListLength <= 1) {
                for (int i = 0; i < m_voltageListLength; ++i) {
                    if (value * m_voltageList[i] > channel_dispatcher::getPowerMaxLimit(*g_channel)) {
                        errorMessage("Power limit exceeded");
                        return;
                    }
                }
            } else {
                if (iRow < m_voltageListLength) {
                    power = value * m_voltageList[iRow];
                } else if (m_voltageListLength > 0) {
                    power = value * m_voltageList[0];
                }
            }
        }

        if (power > channel_dispatcher::getPowerMaxLimit(*g_channel)) {
            errorMessage("Power limit exceeded");
            return;
        }
    }

    popPage();
    setFocusedValue(value);
}

void ChSettingsListsPage::edit() {
    const Widget *widget = getFoundWidgetAtDown().widget;

    if (isFocusWidget(getFoundWidgetAtDown())) {
        NumericKeypadOptions options;

        options.channelIndex = g_channel->channelIndex;

        Cursor cursor(getCursorIndexWithinPage());

        int16_t dataId = getDataIdAtCursor();

        Value value = get(cursor, dataId);

        Value def = getDef(cursor, dataId);

        if (value.getType() == VALUE_TYPE_STR) {
            value = Value();
            options.editValueUnit = def.getUnit();
        } else {
            options.editValueUnit = value.getUnit();
        }

        Value min = getMin(cursor, dataId);
        Value max = getMax(cursor, dataId);

        options.def = def.getFloat();
        options.min = min.getFloat();
        options.max = max.getFloat();

        options.flags.signButtonEnabled = true;
        options.flags.dotButtonEnabled = true;

        char label[64];
        strcpy(label, "[");
        if (dataId == DATA_ID_CHANNEL_LIST_DWELL) {
            char dwell[64];
            min.toText(dwell, sizeof(dwell));
            strcat(label, dwell);
        } else {
            strcatFloat(label, options.min);
        }
        strcat(label, "-");
        if (dataId == DATA_ID_CHANNEL_LIST_DWELL) {
            char dwell[64];
            max.toText(dwell, sizeof(dwell));
            strcat(label, dwell);
        } else if (dataId == DATA_ID_CHANNEL_LIST_VOLTAGE) {
            strcatVoltage(label, options.max);
        } else {
            strcatCurrent(label, options.max);
        }
        strcat(label, "]: ");

        NumericKeypad::start(label, value, options, onValueSet, 0, 0);
    } else {
        m_iCursor = getCursorIndex(getFoundWidgetAtDown().cursor, widget->data);

        if (isFocusedValueEmpty()) {
            edit();
        }
    }
}

bool ChSettingsListsPage::isFocusWidget(const WidgetCursor &widgetCursor) {
    return m_iCursor == getCursorIndex(widgetCursor.cursor, widgetCursor.widget->data);
}

int ChSettingsListsPage::getRowIndex() {
    return m_iCursor / 3;
}

int ChSettingsListsPage::getColumnIndex() {
    return m_iCursor % 3;
}

int ChSettingsListsPage::getPageIndex() {
    return getRowIndex() / LIST_ITEMS_PER_PAGE;
}

uint16_t ChSettingsListsPage::getMaxListLength() {
    uint16_t size = m_dwellListLength;

    if (m_voltageListLength > size) {
        size = m_voltageListLength;
    }

    if (m_currentListLength > size) {
        size = m_currentListLength;
    }

    return size;
}

uint16_t ChSettingsListsPage::getNumPages() {
    return getMaxListLength() / LIST_ITEMS_PER_PAGE + 1;
}

int ChSettingsListsPage::getCursorIndexWithinPage() {
    return getRowIndex() % LIST_ITEMS_PER_PAGE;
}

int16_t ChSettingsListsPage::getDataIdAtCursor() {
    int iColumn = getColumnIndex();
    if (iColumn == 0) {
        return DATA_ID_CHANNEL_LIST_DWELL;
    } else if (iColumn == 1) {
        return DATA_ID_CHANNEL_LIST_VOLTAGE;
    } else {
        return DATA_ID_CHANNEL_LIST_CURRENT;
    }
}

int ChSettingsListsPage::getCursorIndex(const Cursor cursor, int16_t id) {
    int iCursor = (getPageIndex() * LIST_ITEMS_PER_PAGE + cursor) * 3;
    if (id == DATA_ID_CHANNEL_LIST_DWELL) {
        return iCursor;
    } else if (id == DATA_ID_CHANNEL_LIST_VOLTAGE) {
        return iCursor + 1;
    } else if (id == DATA_ID_CHANNEL_LIST_CURRENT) {
        return iCursor + 2;
    } else {
        return -1;
    }
}

void ChSettingsListsPage::moveCursorToFirstAvailableCell() {
    int iRow = getRowIndex();
    int iColumn = getColumnIndex();
    if (iColumn == 0) {
        if (iRow > m_dwellListLength) {
            if (iRow > m_voltageListLength) {
                if (iRow > m_currentListLength) {
                    m_iCursor = 0;
                } else {
                    m_iCursor += 2;
                }
            } else {
                m_iCursor += 1;
            }
        }
    } else if (iColumn == 1) {
        if (iRow > m_voltageListLength) {
            if (iRow > m_currentListLength) {
                m_iCursor += 2;
                moveCursorToFirstAvailableCell();
            } else {
                m_iCursor += 1;
            }
        }
    } else {
        if (iRow > m_currentListLength) {
            m_iCursor += 1;
            moveCursorToFirstAvailableCell();
        }
    }
}

int ChSettingsListsPage::getDirty() {
    return m_listVersion > 0 || m_listCount != m_listCountOrig || m_triggerOnListStop || m_triggerOnListStopOrig;
}

void ChSettingsListsPage::set() {
    if (getDirty()) {
        if (list::areListLengthsEquivalent(m_dwellListLength, m_voltageListLength, m_currentListLength) || getMaxListLength() == 0) {
            trigger::abort();

            channel_dispatcher::setDwellList(*g_channel, m_dwellList, m_dwellListLength);
            channel_dispatcher::setVoltageList(*g_channel, m_voltageList, m_voltageListLength);
            channel_dispatcher::setCurrentList(*g_channel, m_currentList, m_currentListLength);

            channel_dispatcher::setListCount(*g_channel, m_listCount);
            channel_dispatcher::setTriggerOnListStop(*g_channel, m_triggerOnListStop);

            popPage();

            if (
                channel_dispatcher::getVoltageTriggerMode(*g_channel) != TRIGGER_MODE_LIST ||
                channel_dispatcher::getCurrentTriggerMode(*g_channel) != TRIGGER_MODE_LIST
            ) {
                trigger::abort();
                channel_dispatcher::setVoltageTriggerMode(*g_channel, TRIGGER_MODE_LIST);
                channel_dispatcher::setCurrentTriggerMode(*g_channel, TRIGGER_MODE_LIST);
                channel_dispatcher::setTriggerOutputState(*g_channel, true);
            }
        } else {
            errorMessage("List lengths are not equivalent!");
        }
    }
}

void ChSettingsListsPage::onEncoder(int counter) {
#if OPTION_ENCODER
    Cursor cursor(getCursorIndexWithinPage());
    int16_t dataId = getDataIdAtCursor();

    Value value = get(cursor, dataId);
    if (value.getType() == VALUE_TYPE_STR) {
        value = getDef(cursor, dataId);
    }

    Value min = getMin(cursor, dataId);
    Value max = getMax(cursor, dataId);

    float newValue = encoderIncrement(value, counter, min.getFloat(), max.getFloat(), g_channel->channelIndex, 0);

    setFocusedValue(newValue);
#endif
}

void ChSettingsListsPage::onEncoderClicked() {
    int16_t dataId = getDataIdAtCursor();
    int iRow = getRowIndex();

    if (dataId == DATA_ID_CHANNEL_LIST_DWELL) {
        if (iRow <= m_voltageListLength) {
            m_iCursor += 1;
            return;
        }

        if (iRow <= m_currentListLength) {
            m_iCursor += 2;
            return;
        }

        m_iCursor += 3;
    } else if (dataId == DATA_ID_CHANNEL_LIST_VOLTAGE) {
        if (iRow <= m_currentListLength) {
            m_iCursor += 1;
            return;
        }

        m_iCursor += 2;
    } else {
        m_iCursor += 1;
    }

    moveCursorToFirstAvailableCell();
}

Unit ChSettingsListsPage::getEncoderUnit() {
    Cursor cursor(getCursorIndexWithinPage());

    Value value = get(cursor, getDataIdAtCursor());

    if (value.getType() == VALUE_TYPE_STR) {
        value = getDef(cursor, getDataIdAtCursor());
    }

    return value.getUnit();
}

void ChSettingsListsPage::showInsertMenu() {
    if (getRowIndex() < getMaxListLength()) {
        pushPage(PAGE_ID_CH_SETTINGS_LISTS_INSERT_MENU);
    }
}

void ChSettingsListsPage::insertRow(int iRow, int iCopyRow) {
    if (getMaxListLength() < MAX_LIST_LENGTH) {
        for (int i = MAX_LIST_LENGTH - 2; i >= iRow; --i) {
            m_dwellList[i + 1] = m_dwellList[i];
            m_voltageList[i + 1] = m_voltageList[i];
            m_currentList[i + 1] = m_currentList[i];
        }

        if (iCopyRow < m_dwellListLength && iRow <= m_dwellListLength) {
            m_dwellList[iRow] = m_dwellList[iCopyRow];
            ++m_dwellListLength;
        }

        if (iCopyRow < m_voltageListLength && iRow <= m_voltageListLength) {
            m_voltageList[iRow] = m_voltageList[iCopyRow];
            ++m_voltageListLength;
        }

        if (iCopyRow < m_currentListLength && iRow <= m_currentListLength) {
            m_currentList[iRow] = m_currentList[iCopyRow];
            ++m_currentListLength;
        }
    }
}

void ChSettingsListsPage::insertRowAbove() {
    int iRow = getRowIndex();
    insertRow(iRow, iRow);
}

void ChSettingsListsPage::insertRowBelow() {
    int iRow = getRowIndex();
    if (iRow < getMaxListLength()) {
        m_iCursor += 3;
        insertRow(getRowIndex(), iRow);
    }
}

void ChSettingsListsPage::showDeleteMenu() {
    if (getMaxListLength()) {
        pushPage(PAGE_ID_CH_SETTINGS_LISTS_DELETE_MENU);
    }
}

void ChSettingsListsPage::deleteRow() {
    int iRow = getRowIndex();
    if (iRow < getMaxListLength()) {
        for (int i = iRow + 1; i < MAX_LIST_LENGTH; ++i) {
            m_dwellList[i - 1] = m_dwellList[i];
            m_voltageList[i - 1] = m_voltageList[i];
            m_currentList[i - 1] = m_currentList[i];
        }

        if (iRow < m_dwellListLength) {
            --m_dwellListLength;
        }

        if (iRow < m_voltageListLength) {
            --m_voltageListLength;
        }

        if (iRow < m_currentListLength) {
            --m_currentListLength;
        }

        ++m_listVersion;
    }
}

void ChSettingsListsPage::onClearColumn() {
    ((ChSettingsListsPage *)getActivePage())->doClearColumn();
}

void ChSettingsListsPage::doClearColumn() {
    int iRow = getRowIndex();
    int iColumn = getColumnIndex();
    if (iColumn == 0) {
        m_dwellListLength = iRow;
    } else if (iColumn == 1) {
        m_voltageListLength = iRow;
    } else {
        m_currentListLength = iRow;
    }
    ++m_listVersion;
}

void ChSettingsListsPage::clearColumn() {
    yesNoDialog(PAGE_ID_YES_NO, "Are you sure?", onClearColumn, 0, 0);
}

void ChSettingsListsPage::onDeleteRows() {
    ((ChSettingsListsPage *)getActivePage())->doDeleteRows();
}

void ChSettingsListsPage::doDeleteRows() {
    int iRow = getRowIndex();
    if (iRow < m_dwellListLength)
        m_dwellListLength = iRow;
    if (iRow < m_voltageListLength)
        m_voltageListLength = iRow;
    if (iRow < m_currentListLength)
        m_currentListLength = iRow;
    ++m_listVersion;
}

void ChSettingsListsPage::deleteRows() {
    yesNoDialog(PAGE_ID_YES_NO, "Are you sure?", onDeleteRows, 0, 0);
}

void ChSettingsListsPage::onDeleteAll() {
    ((ChSettingsListsPage *)getActivePage())->doDeleteAll();
}

void ChSettingsListsPage::doDeleteAll() {
    m_dwellListLength = 0;
    m_voltageListLength = 0;
    m_currentListLength = 0;
    m_iCursor = 0;
    ++m_listVersion;
}

void ChSettingsListsPage::deleteAll() {
    yesNoDialog(PAGE_ID_YES_NO, "Are you sure?", onDeleteAll, 0, 0);
}

void ChSettingsListsPage::showFileMenu() {
    pushPage(PAGE_ID_CH_SETTINGS_LISTS_FILE_MENU);
}

void ChSettingsListsPage::onImportListFileSelected(const char *listFilePath) {
    auto *page = (ChSettingsListsPage *)getActivePage();
    strcpy(page->m_listFilePath, listFilePath);

    showProgressPageWithoutAbort("Import list...");

    using namespace eez::scpi;
    osMessagePut(g_scpiMessageQueueId, SCPI_QUEUE_MESSAGE(SCPI_QUEUE_MESSAGE_TARGET_NONE, SCPI_QUEUE_MESSAGE_TYPE_LISTS_PAGE_IMPORT_LIST, 0), osWaitForever);
}

void ChSettingsListsPage::doImportList() {
    auto *page = (ChSettingsListsPage *)getPage(PAGE_ID_CH_SETTINGS_LISTS);

    int err;
    list::loadList(
        page->m_listFilePath,
        page->m_dwellListLoad, page->m_dwellListLengthLoad,
        page->m_voltageListLoad, page->m_voltageListLengthLoad,
        page->m_currentListLoad, page->m_currentListLengthLoad,
        true,
        &err
    );

    osMessagePut(g_guiMessageQueueId, GUI_QUEUE_MESSAGE(GUI_QUEUE_MESSAGE_TYPE_LISTS_PAGE_IMPORT_LIST_FINISHED, err), osWaitForever);
}

void ChSettingsListsPage::onImportListFinished(int16_t err) {
    hideProgressPage();

    if (err == SCPI_RES_OK) {
        m_dwellListLength = m_dwellListLengthLoad;
        memcpy(m_dwellList, m_dwellListLoad, m_dwellListLength * sizeof(float));

        m_voltageListLength = m_voltageListLengthLoad;
        memcpy(m_voltageList, m_voltageListLoad, m_voltageListLength * sizeof(float));

        m_currentListLength = m_currentListLengthLoad;
        memcpy(m_currentList, m_currentListLoad, m_currentListLength * sizeof(float));

        ++m_listVersion;

        m_iCursor = 0;
    } else {
        errorMessage(Value(err, VALUE_TYPE_SCPI_ERROR));
    }
}

void ChSettingsListsPage::fileImport() {
    file_manager::browseForFile("Import list", "/Lists", FILE_TYPE_LIST, file_manager::DIALOG_TYPE_OPEN, onImportListFileSelected);
}

void ChSettingsListsPage::onExportListFileSelected(const char *listFilePath) {
    auto *page = (ChSettingsListsPage *)getActivePage();
    strcpy(page->m_listFilePath, listFilePath);
    
    showProgressPageWithoutAbort("Exporting list...");

    using namespace eez::scpi;
    osMessagePut(g_scpiMessageQueueId, SCPI_QUEUE_MESSAGE(SCPI_QUEUE_MESSAGE_TARGET_NONE, SCPI_QUEUE_MESSAGE_TYPE_LISTS_PAGE_EXPORT_LIST, 0), osWaitForever);
}

void ChSettingsListsPage::doExportList() {
    auto *page = (ChSettingsListsPage *)getPage(PAGE_ID_CH_SETTINGS_LISTS);

    int err;
    list::saveList(
        page->m_listFilePath,
        page->m_dwellList, page->m_dwellListLength,
        page->m_voltageList, page->m_voltageListLength,
        page->m_currentList, page->m_currentListLength,
        true,
        &err
    );

    osMessagePut(g_guiMessageQueueId, GUI_QUEUE_MESSAGE(GUI_QUEUE_MESSAGE_TYPE_LISTS_PAGE_EXPORT_LIST_FINISHED, err), osWaitForever);
}

void ChSettingsListsPage::onExportListFinished(int16_t err) {
    hideProgressPage();

    if (err != SCPI_RES_OK) {
        errorMessage(Value(err, VALUE_TYPE_SCPI_ERROR));
    }
}

void ChSettingsListsPage::fileExport() {
    file_manager::browseForFile("Export list as", "/Lists", FILE_TYPE_LIST, file_manager::DIALOG_TYPE_SAVE, onExportListFileSelected);
}

void ChSettingsListsPage::editListCount() {
    NumericKeypadOptions options;

    options.min = 0;
    options.max = MAX_LIST_COUNT;
    options.def = 0;

    options.flags.option1ButtonEnabled = true;
    options.option1ButtonText = INFINITY_SYMBOL;
    options.option1 = onListCountSetToInfinity;

    NumericKeypad::start(0, Value((uint16_t)m_listCount), options, onListCountSet, 0, 0);
}

void ChSettingsListsPage::onListCountSet(float value) {
    popPage();
    ChSettingsListsPage *page = (ChSettingsListsPage *)getPage(PAGE_ID_CH_SETTINGS_LISTS);
    page->m_listCount = (uint16_t)value;
}

void ChSettingsListsPage::onListCountSetToInfinity() {
    popPage();
    ChSettingsListsPage *page = (ChSettingsListsPage *)getPage(PAGE_ID_CH_SETTINGS_LISTS);
    page->m_listCount = 0;
}

void ChSettingsListsPage::editTriggerOnListStop() {
    pushSelectFromEnumPage(ENUM_DEFINITION_CHANNEL_TRIGGER_ON_LIST_STOP, m_triggerOnListStop, 0, onTriggerOnListStopSet);
}

void ChSettingsListsPage::onTriggerOnListStopSet(uint16_t value) {
    popPage();
    ChSettingsListsPage *page = (ChSettingsListsPage *)getPage(PAGE_ID_CH_SETTINGS_LISTS);
    page->m_triggerOnListStop = (TriggerOnListStop)value;
}

} // namespace gui
} // namespace psu
} // namespace eez

#endif
