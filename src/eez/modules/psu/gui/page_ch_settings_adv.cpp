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

#include <eez/modules/psu/psu.h>

#include <eez/modules/psu/channel_dispatcher.h>
#include <eez/modules/psu/gui/data.h>
#include <eez/modules/psu/gui/numeric_keypad.h>
#include <eez/modules/psu/gui/page_ch_settings_adv.h>
#include <eez/modules/psu/gui/psu.h>
#include <eez/modules/psu/profile.h>
#include <eez/modules/psu/trigger.h>
#include <eez/gui/dialogs.h>
#include <eez/system.h>

namespace eez {
namespace psu {
namespace gui {

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
    pushSelectFromEnumPage(g_channelCurrentRangeSelectionModeEnumDefinition, g_channel->getCurrentRangeSelectionMode(), 0, onModeSet);
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
    pushSelectFromEnumPage(g_channelDisplayValueEnumDefinition, displayValue1, isDisabledDisplayValue1, onDisplayValue1Set);
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
    pushSelectFromEnumPage(g_channelDisplayValueEnumDefinition, displayValue2, isDisabledDisplayValue2, onDisplayValue2Set);
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
    return (origDisplayValue1 != displayValue1 || origDisplayValue2 != displayValue2 ||
            origYTViewRate != ytViewRate)
               ? 1
               : 0;
}

void ChSettingsAdvViewPage::set() {
    if (getDirty()) {
        channel_dispatcher::setDisplayViewSettings(*g_channel, displayValue1, displayValue2,
                                                   ytViewRate);
        profile::save();
        g_actionExecFunctions[ACTION_ID_SHOW_CH_SETTINGS]();
        infoMessage("View settings changed!");
    }
}

} // namespace gui
} // namespace psu
} // namespace eez

#endif
