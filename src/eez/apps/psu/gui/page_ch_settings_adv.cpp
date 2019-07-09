/*
 * EEZ PSU Firmware
 * Copyright (C) 2016-present, Envox d.o.o.
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

#include <eez/apps/psu/channel_dispatcher.h>
#include <eez/apps/psu/gui/data.h>
#include <eez/apps/psu/gui/numeric_keypad.h>
#include <eez/apps/psu/gui/page_ch_settings_adv.h>
#include <eez/apps/psu/gui/psu.h>
#include <eez/apps/psu/profile.h>
#include <eez/apps/psu/trigger.h>
#include <eez/gui/dialogs.h>
#include <eez/system.h>

namespace eez {
namespace psu {
namespace gui {

////////////////////////////////////////////////////////////////////////////////

void ChSettingsAdvRemotePage::toggleSense() {
    channel_dispatcher::remoteSensingEnable(*g_channel, !g_channel->isRemoteSensingEnabled());
}

void ChSettingsAdvRemotePage::toggleProgramming() {
    g_channel->remoteProgrammingEnable(!g_channel->isRemoteProgrammingEnabled());
}

////////////////////////////////////////////////////////////////////////////////

void ChSettingsAdvRangesPage::onModeSet(uint8_t value) {
    popPage();
    g_channel->setCurrentRangeSelectionMode((CurrentRangeSelectionMode)value);
}

void ChSettingsAdvRangesPage::selectMode() {
    pushSelectFromEnumPage(g_channelCurrentRangeSelectionModeEnumDefinition,
                           g_channel->getCurrentRangeSelectionMode(), 0, onModeSet);
}

void ChSettingsAdvRangesPage::toggleAutoRanging() {
    g_channel->enableAutoSelectCurrentRange(!g_channel->isAutoSelectCurrentRangeEnabled());
}

////////////////////////////////////////////////////////////////////////////////

int ChSettingsAdvCouplingPage::selectedMode = 0;

void ChSettingsAdvCouplingPage::uncouple() {
    channel_dispatcher::setType(channel_dispatcher::TYPE_NONE);
    profile::save();
    infoMessage("Channels uncoupled!");
}

void ChSettingsAdvCouplingPage::setParallelInfo() {
    selectedMode = 0;
    pushPage(PAGE_ID_CH_SETTINGS_ADV_COUPLING_INFO);
}

void ChSettingsAdvCouplingPage::setSeriesInfo() {
    selectedMode = 1;
    pushPage(PAGE_ID_CH_SETTINGS_ADV_COUPLING_INFO);
}

void ChSettingsAdvCouplingPage::setParallel() {
    if (selectedMode == 0) {
        if (channel_dispatcher::setType(channel_dispatcher::TYPE_PARALLEL)) {
            profile::save();
            popPage();
            infoMessage("Channels coupled in parallel!");
        }
    }
}

void ChSettingsAdvCouplingPage::setSeries() {
    if (selectedMode == 1) {
        if (channel_dispatcher::setType(channel_dispatcher::TYPE_SERIES)) {
            profile::save();
            popPage();
            infoMessage("Channels coupled in series!");
        }
    }
}

////////////////////////////////////////////////////////////////////////////////

void ChSettingsAdvTrackingPage::toggleTrackingMode() {
    if (channel_dispatcher::isTracked()) {
        channel_dispatcher::setType(channel_dispatcher::TYPE_NONE);
        profile::save();
        infoMessage("Tracking disabled!");
    } else {
        channel_dispatcher::setType(channel_dispatcher::TYPE_TRACKED);
        profile::save();
        infoMessage("Tracking enabled!");
    }
}

////////////////////////////////////////////////////////////////////////////////

void ChSettingsAdvViewPage::pageWillAppear() {
    origDisplayValue1 = displayValue1 = g_channel->flags.displayValue1;
    origDisplayValue2 = displayValue2 = g_channel->flags.displayValue2;
    origYTViewRate = ytViewRate = g_channel->ytViewRate;
}

bool ChSettingsAdvViewPage::isDisabledDisplayValue1(uint8_t value) {
    ChSettingsAdvViewPage *page = (ChSettingsAdvViewPage *)getPreviousPage();
    return value == page->displayValue2;
}

void ChSettingsAdvViewPage::onDisplayValue1Set(uint8_t value) {
    popPage();
    ChSettingsAdvViewPage *page = (ChSettingsAdvViewPage *)getActivePage();
    page->displayValue1 = value;
}

void ChSettingsAdvViewPage::editDisplayValue1() {
    pushSelectFromEnumPage(g_channelDisplayValueEnumDefinition, displayValue1,
                           isDisabledDisplayValue1, onDisplayValue1Set);
}

bool ChSettingsAdvViewPage::isDisabledDisplayValue2(uint8_t value) {
    ChSettingsAdvViewPage *page = (ChSettingsAdvViewPage *)getPreviousPage();
    return value == page->displayValue1;
}

void ChSettingsAdvViewPage::onDisplayValue2Set(uint8_t value) {
    popPage();
    ChSettingsAdvViewPage *page = (ChSettingsAdvViewPage *)getActivePage();
    page->displayValue2 = value;
}

void ChSettingsAdvViewPage::editDisplayValue2() {
    pushSelectFromEnumPage(g_channelDisplayValueEnumDefinition, displayValue2,
                           isDisabledDisplayValue2, onDisplayValue2Set);
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
        g_actionExecFunctions[ACTION_ID_SHOW_CH_SETTINGS_ADV]();
        infoMessage("View settings changed!");
    }
}

} // namespace gui
} // namespace psu
} // namespace eez

#endif
