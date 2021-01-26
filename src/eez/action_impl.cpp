/*
* EEZ Generic Firmware
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
* along with this program.  If not, see http://www.gnu.org/licenses.
*/

#if OPTION_DISPLAY

#include <stdio.h>

#include <eez/firmware.h>
#include <eez/system.h>
#include <eez/tasks.h>
#include <eez/sound.h>
#include <eez/index.h>
#include <eez/mqtt.h>
#include <eez/hmi.h>
#include <eez/usb.h>

#include <eez/fs_driver.h>

#include <eez/modules/psu/psu.h>
#include <eez/modules/psu/channel_dispatcher.h>
#include <eez/modules/psu/event_queue.h>
#include <eez/modules/psu/persist_conf.h>
#include <eez/modules/psu/trigger.h>
#include <eez/modules/psu/dlog_record.h>
#include <eez/modules/psu/dlog_view.h>
#include <eez/modules/psu/ntp.h>

#include <eez/modules/psu/gui/psu.h>
#include <eez/modules/psu/gui/animations.h>
#include <eez/modules/psu/gui/edit_mode.h>
#include <eez/modules/psu/gui/keypad.h>
#include <eez/modules/psu/gui/page_ch_settings.h>
#include <eez/modules/psu/gui/page_sys_settings.h>
#include <eez/modules/psu/gui/page_user_profiles.h>
#include <eez/modules/psu/gui/password.h>
#include <eez/modules/psu/gui/touch_calibration.h>
#include <eez/modules/psu/gui/file_manager.h>

#include <eez/modules/psu/sd_card.h>

#if OPTION_ENCODER
#include <eez/modules/mcu/encoder.h>
#endif

#include <eez/modules/bp3c/eeprom.h>
#include <eez/modules/bp3c/flash_slave.h>
#include <eez/modules/bp3c/io_exp.h>

#if defined(EEZ_PLATFORM_SIMULATOR)
#include <eez/platform/simulator/front_panel.h>
#endif

using namespace eez::gui;
using namespace eez::psu;
using namespace eez::psu::gui;

namespace eez {
namespace gui {

static const char *g_discardMessage = "All changes will be lost.";

void action_channel_toggle_output() {
    channelToggleOutput();
}

void action_edit() {
    edit_mode::enter();
}

void action_set_focus() {
#if OPTION_ENCODER
    setFocusCursor(getFoundWidgetAtDown().cursor, getFoundWidgetAtDown().widget->data);
#endif
}

void action_edit_no_focus() {
    edit_mode::enter(-1, false);
}

void action_edit_mode_slider() {
    edit_mode::enter(PAGE_ID_EDIT_MODE_SLIDER);
}

void action_edit_mode_step() {
    edit_mode::enter(PAGE_ID_EDIT_MODE_STEP);
}

void action_edit_mode_keypad() {
    edit_mode::enter(PAGE_ID_EDIT_MODE_KEYPAD);
}

void action_exit_edit_mode() {
    if (edit_mode::isActive(&g_psuAppContext)) {
        edit_mode::exit();
    }
}

void action_toggle_interactive_mode() {
    edit_mode::toggleInteractiveMode();
}

void action_non_interactive_enter() {
    edit_mode::nonInteractiveSet();
}

void action_non_interactive_discard() {
    edit_mode::nonInteractiveDiscard();
}

void action_keypad_key() {
    getActiveKeypad()->key();
}

void action_keypad_space() {
    getActiveKeypad()->space();
}

void action_keypad_back() {
    getActiveKeypad()->back();
}

void action_keypad_clear() {
    getActiveKeypad()->clear();
}

void action_toggle_keypad_mode() {
    auto keypad = getActiveKeypad();
    if (keypad->m_keypadMode == KEYPAD_MODE_LOWERCASE) {
        getActiveKeypad()->m_keypadMode = KEYPAD_MODE_UPPERCASE;
    } else if (keypad->m_keypadMode == KEYPAD_MODE_UPPERCASE) {
        getActiveKeypad()->m_keypadMode = KEYPAD_MODE_SYMBOL;
    } else {
        getActiveKeypad()->m_keypadMode = KEYPAD_MODE_LOWERCASE;
    }
}

void action_keypad_ok() {
    getActiveKeypad()->ok();
}

void action_keypad_cancel() {
    getActiveKeypad()->cancel();
}

void action_keypad_set_default() {
    getActiveKeypad()->setDefault();
}

void action_keypad_sign() {
    getActiveKeypad()->sign();
}

void action_keypad_unit() {
    getActiveKeypad()->unit();
}

void action_keypad_option1() {
    getActiveKeypad()->option1();
}

void action_keypad_option2() {
    getActiveKeypad()->option2();
}

void action_keypad_option3() {
    getActiveKeypad()->option3();
}

void action_enter_touch_calibration() {
    psu::gui::enterTouchCalibration();
}

void action_show_touch_calibration_intro() {
    showPage(PAGE_ID_TOUCH_CALIBRATION_INTRO);
}

void action_yes() {
    dialogYes();
}

void action_no() {
    dialogNo();
}

void action_ok() {
    dialogOk();
}

void action_cancel() {
    dialogCancel();
}

void action_later() {
    dialogLater();
}

void action_show_previous_page() {
    Page *page = getActivePage();
    if (page && page->getDirty()) {
        areYouSureWithMessage(g_discardMessage, popPage);
    } else {
        popPage();
    }
}

void action_show_main_page() {
	Page *page = getActivePage();
	if (page && page->getDirty()) {
		areYouSureWithMessage(g_discardMessage, showMainPage);
	}
	else {
		showMainPage();
	}
}

void action_show_event_queue() {
    showPage(PAGE_ID_EVENT_QUEUE);
}

void action_show_sys_settings() {
    showPage(PAGE_ID_SYS_SETTINGS);
}

void action_show_sys_settings_trigger() {
    pushPage(PAGE_ID_SYS_SETTINGS_TRIGGER);
}

void action_show_sys_settings_io() {
    pushPage(PAGE_ID_SYS_SETTINGS_IO);
}

void action_show_sys_settings_date_time() {
    pushPage(PAGE_ID_SYS_SETTINGS_DATE_TIME);
}

void action_show_sys_settings_screen_calibration() {
    pushPage(PAGE_ID_SYS_SETTINGS_SCREEN_CALIBRATION);
}

void action_show_sys_settings_display() {
    pushPage(PAGE_ID_SYS_SETTINGS_DISPLAY);
}

void action_show_sys_settings_serial() {
    pushPage(PAGE_ID_SYS_SETTINGS_USB);
}

void action_show_sys_settings_ethernet() {
    pushPage(PAGE_ID_SYS_SETTINGS_ETHERNET);
}

void action_show_sys_settings_ethernet_error() {
#if OPTION_ETHERNET    
    if (persist_conf::devConf.mqttEnabled && mqtt::g_connectionState == mqtt::CONNECTION_STATE_ERROR) {
        pushPage(PAGE_ID_SYS_SETTINGS_MQTT);
    } else {
        pushPage(PAGE_ID_SYS_SETTINGS_ETHERNET);
    }
#endif
}

void action_show_sys_settings_protections() {
    pushPage(PAGE_ID_SYS_SETTINGS_PROTECTIONS);
}

void action_show_sys_settings_aux_otp() {
    pushPage(PAGE_ID_SYS_SETTINGS_TEMPERATURE);
}

void action_show_sys_settings_sound() {
    pushPage(PAGE_ID_SYS_SETTINGS_SOUND);
}

void action_show_sys_settings_encoder() {
    pushPage(PAGE_ID_SYS_SETTINGS_ENCODER);
}

void action_show_sys_info() {
    pushPage(PAGE_ID_SYS_INFO);
}

void action_show_edit_mode_step_help() {
    pushPage(PAGE_ID_EDIT_MODE_STEP_HELP);
}

void action_show_edit_mode_slider_help() {
    pushPage(PAGE_ID_EDIT_MODE_SLIDER_HELP);
}

int getSlotSettingsPageId(int slotIndex) {
    if (g_slots[slotIndex]->getTestResult() != TEST_OK) {
        return PAGE_ID_SLOT_SETTINGS;
    }
    return g_slots[slotIndex]->getSlotSettingsPageId();
}

void action_show_slot_settings() {
    hmi::selectSlot(getFoundWidgetAtDown().cursor);
    showPage(getSlotSettingsPageId(hmi::g_selectedSlotIndex));
}

void action_show_ch_settings() {
    selectChannelByCursor();
    showPage(getSlotSettingsPageId(g_channel->slotIndex));
}

void action_show_ch_settings_prot_ocp() {
    selectChannelByCursor();
    pushPage(PAGE_ID_CH_SETTINGS_PROT_OCP);
}

void action_show_ch_settings_prot_ovp() {
    selectChannelByCursor();
    pushPage(PAGE_ID_CH_SETTINGS_PROT_OVP);
}

void action_show_ch_settings_prot_opp() {
    selectChannelByCursor();
    pushPage(PAGE_ID_CH_SETTINGS_PROT_OPP);
}

void action_show_ch_settings_prot_otp() {
    selectChannelByCursor();
    pushPage(PAGE_ID_CH_SETTINGS_PROT_OTP);
}

void action_show_ch_settings_trigger() {
    selectChannelByCursor();
    pushPage(PAGE_ID_CH_SETTINGS_TRIGGER);
}

void action_show_ch_settings_lists() {
    Page *page = getActivePage();
    if (page && page->getDirty()) {
        page->set();
    }
    pushPage(PAGE_ID_CH_SETTINGS_LISTS);
}

void action_show_ch_settings_adv_options() {
    selectChannelByCursor();
    pushPage(g_channel->getAdvancedOptionsPageId());
}

void action_show_ch_settings_adv_ranges() {
    selectChannelByCursor();
    pushPage(PAGE_ID_CH_SETTINGS_ADV_RANGES);
}

void action_show_sys_settings_tracking() {
    selectChannelByCursor();
    pushPage(PAGE_ID_SYS_SETTINGS_TRACKING);
}

void action_show_sys_settings_coupling() {
    selectChannelByCursor();
    pushPage(PAGE_ID_SYS_SETTINGS_COUPLING);
}

void action_show_ch_settings_adv_view() {
    selectChannelByCursor();
    pushPage(PAGE_ID_CH_SETTINGS_ADV_VIEW);
}

void action_show_ch_settings_info() {
    selectChannelByCursor();
    pushPage(PAGE_ID_CH_SETTINGS_INFO);
}

void action_edit_calibration_password() {
    editCalibrationPassword();
}

void onChannelCopyDestinationSelected(uint16_t value) {
    popPage();
    const char *err = channel_dispatcher::copyChannelToChannel(g_channel->channelIndex, value);
    if (err) {
        errorMessage("Copying is not possible!", err);
    }
}

void channelsEnumDefinition(DataOperationEnum operation, Cursor cursor, Value &value) {
    int channelIndex = cursor < g_channel->channelIndex ? cursor : cursor + 1;

    if (operation == DATA_OPERATION_GET_VALUE) {
        value = (uint8_t)channelIndex;
    } else if (operation == DATA_OPERATION_GET_LABEL) {
		if (channelIndex < CH_NUM) {
			value = Value(channelIndex, VALUE_TYPE_CHANNEL_SHORT_LABEL_WITHOUT_COLUMN);
		}
    }
}

void action_ch_settings_copy() {
    pushSelectFromEnumPage(channelsEnumDefinition, -1, nullptr, onChannelCopyDestinationSelected, false, false);
}

void action_ch_settings_calibration_toggle_enable() {
    if (g_channel) {
        g_channel->calibrationEnable(!g_channel->isCalibrationEnabled());
    } else {
        bool enable = !g_slots[hmi::g_selectedSlotIndex]->isVoltageCalibrationEnabled(hmi::g_selectedSubchannelIndex);
        g_slots[hmi::g_selectedSlotIndex]->enableVoltageCalibration(hmi::g_selectedSubchannelIndex, enable);
        g_slots[hmi::g_selectedSlotIndex]->enableCurrentCalibration(hmi::g_selectedSubchannelIndex, enable);
    }
}

void action_ch_settings_prot_clear() {
    channel_dispatcher::clearProtection(*g_channel);
    popPage();
    infoMessage("Cleared!");
}

void action_ch_settings_prot_toggle_state() {
    ((ChSettingsProtectionSetPage *)getActivePage())->toggleState();
}

void action_ch_settings_prot_toggle_type() {
    if (getActivePageId() == PAGE_ID_CH_SETTINGS_PROT_OVP) {
        ((ChSettingsProtectionSetPage *)getActivePage())->toggleType();
    } else {
        selectChannelByCursor();
        channel_dispatcher::setOvpType(*g_channel, g_channel->prot_conf.flags.u_type ? 0 : 1);
    }
}

void action_ch_settings_prot_edit_limit() {
    ((ChSettingsProtectionSetPage *)getActivePage())->editLimit();
}

void action_ch_settings_prot_edit_level() {
    ((ChSettingsProtectionSetPage *)getActivePage())->editLevel();
}

void action_ch_settings_prot_edit_delay() {
    ((ChSettingsProtectionSetPage *)getActivePage())->editDelay();
}

void action_set() {
    ((SetPage *)getActivePage())->set();
}

void discard() {
	((SetPage *)getActivePage())->discard();
}

void action_discard() {
	Page *page = getActivePage();
	if (page) {
        if (page->getDirty() && page->showAreYouSureOnDiscard()) {
            areYouSureWithMessage(g_discardMessage, discard);
        } else {
            discard();
        }
	}
}

void action_edit_field() {
    ((SetPage *)getActivePage())->edit();
}

void action_ch_settings_adv_remote_toggle_sense() {
    ((ChSettingsAdvOptionsPage *)getActivePage())->toggleSense();
}

void action_ch_settings_adv_remote_toggle_programming() {
    ((ChSettingsAdvOptionsPage *)getActivePage())->toggleProgramming();
}

void action_ch_settings_adv_toggle_dprog() {
    ((ChSettingsAdvOptionsPage *)getActivePage())->toggleDprog();
}

void action_date_time_select_dst_rule() {
    ((SysSettingsDateTimePage *)getActivePage())->selectDstRule();
}

void action_date_time_select_format() {
    ((SysSettingsDateTimePage *)getActivePage())->selectFormat();
}

void action_date_time_toggle_am_pm() {
    ((SysSettingsDateTimePage *)getActivePage())->toggleAmPm();
}

void action_show_user_profiles() {
    showPage(PAGE_ID_USER_PROFILES);
}

void action_show_user_profile_settings() {
    ((UserProfilesPage *)getActivePage())->showProfile();
}

void action_profiles_toggle_auto_recall() {
    ((UserProfilesPage *)getActivePage())->toggleAutoRecall();
}

void action_profile_toggle_is_auto_recall_location() {
    ((UserProfilesPage *)getActivePage())->toggleIsAutoRecallLocation();
}

void action_profile_recall() {
    ((UserProfilesPage *)getActivePage())->recallProfile();
}

void action_profile_save() {
    ((UserProfilesPage *)getActivePage())->saveProfile();
}

void action_profile_import() {
    ((UserProfilesPage *)getActivePage())->importProfile();
}

void action_profile_export() {
    ((UserProfilesPage *)getActivePage())->exportProfile();
}

void action_profile_delete() {
    ((UserProfilesPage *)getActivePage())->deleteProfile();
}

void action_profile_edit_remark() {
    ((UserProfilesPage *)getActivePage())->editRemark();
}

void action_toggle_channels_view_mode() {
    persist_conf::toggleChannelsViewMode();
    animateFadeOutFadeInWorkingArea();
}

void action_toggle_channels_max_view() {
    selectChannelByCursor();

    if (getActivePageId() != PAGE_ID_MAIN) {
        showMainPage();
        persist_conf::setMaxChannelIndex(g_channel->channelIndex);
        animateFromMicroViewToMaxView();
    } else {
        auto isMaxViewBefore = persist_conf::isMaxView();
		auto wasFullScreenView = isMaxViewBefore && isSlotFullScreenView();
        auto maxSlotIndexBefore = isMaxViewBefore ? persist_conf::getMaxSlotIndex() : -1;

        persist_conf::toggleMaxChannelIndex(g_channel->channelIndex);
        
        if (!isMaxViewBefore && persist_conf::isMaxView()) {
            animateFromDefaultViewToMaxView(g_channel->slotIndex, isSlotFullScreenView());
        } else if (isMaxViewBefore && !persist_conf::isMaxView()) {
            animateFromMaxViewToDefaultView(maxSlotIndexBefore, wasFullScreenView);
        } else {
            animateFromMinViewToMaxView(maxSlotIndexBefore, isSlotFullScreenView());
        }
    }
}

void action_toggle_slot_max_view() {
    auto slotIndex = getFoundWidgetAtDown().cursor;

    if (getActivePageId() != PAGE_ID_MAIN) {
        showMainPage();
        persist_conf::setMaxSlotIndex(slotIndex);
        animateFromMicroViewToMaxView();
    } else {
        auto isMaxViewBefore = persist_conf::isMaxView();
		auto wasFullScreenView = isMaxViewBefore && isSlotFullScreenView();
        auto maxSlotIndexBefore = isMaxViewBefore ? persist_conf::getMaxSlotIndex() : -1;

        persist_conf::toggleMaxSlotIndex(slotIndex);
        
        if (!isMaxViewBefore && persist_conf::isMaxView()) {
            animateFromDefaultViewToMaxView(slotIndex, isSlotFullScreenView());
        } else if (isMaxViewBefore && !persist_conf::isMaxView()) {
            animateFromMaxViewToDefaultView(maxSlotIndexBefore, wasFullScreenView);
        } else {
            animateFromMinViewToMaxView(maxSlotIndexBefore, isSlotFullScreenView());
        }
    }
}

void action_ethernet_toggle() {
    #if OPTION_ETHERNET
    ((SysSettingsEthernetPage *)getActivePage())->toggle();
    #endif
}

void action_ethernet_toggle_dhcp() {
    #if OPTION_ETHERNET
    ((SysSettingsEthernetPage *)getActivePage())->toggleDhcp();
    #endif
}

void action_ethernet_edit_mac_address() {
    #if OPTION_ETHERNET
    ((SysSettingsEthernetPage *)getActivePage())->editMacAddress();
    #endif
}

void action_ethernet_edit_static_address() {
    #if OPTION_ETHERNET
    ((SysSettingsEthernetPage *)getActivePage())->editStaticAddress();
    #endif
}

void action_ethernet_edit_host_name() {
#if OPTION_ETHERNET
    editValue(DATA_ID_ETHERNET_HOST_NAME);
#endif
}

void action_ethernet_edit_ip_address() {
    #if OPTION_ETHERNET
    ((SysSettingsEthernetStaticPage *)getActivePage())->editIpAddress();
    #endif
}

void action_ethernet_edit_dns() {
    #if OPTION_ETHERNET
    ((SysSettingsEthernetStaticPage *)getActivePage())->editDns();
    #endif
}

void action_ethernet_edit_gateway() {
    #if OPTION_ETHERNET
    ((SysSettingsEthernetStaticPage *)getActivePage())->editGateway();
    #endif
}

void action_ethernet_edit_subnet_mask() {
    #if OPTION_ETHERNET
    ((SysSettingsEthernetStaticPage *)getActivePage())->editSubnetMask();
    #endif
}

void action_ethernet_edit_scpi_port() {
    #if OPTION_ETHERNET
    ((SysSettingsEthernetPage *)getActivePage())->editScpiPort();
    #endif
}

void action_set_coupling_uncoupled() {
    ((SysSettingsCouplingPage *)getActivePage())->m_couplingType = channel_dispatcher::COUPLING_TYPE_NONE;
}

void action_set_coupling_parallel() {
    ((SysSettingsCouplingPage *)getActivePage())->m_couplingType = channel_dispatcher::COUPLING_TYPE_PARALLEL;
}

void action_set_coupling_series() {
    ((SysSettingsCouplingPage *)getActivePage())->m_couplingType = channel_dispatcher::COUPLING_TYPE_SERIES;
}

void action_set_coupling_common_gnd() {
    ((SysSettingsCouplingPage *)getActivePage())->m_couplingType = channel_dispatcher::COUPLING_TYPE_COMMON_GND;
}

void action_set_coupling_split_rails() {
    ((SysSettingsCouplingPage *)getActivePage())->m_couplingType = channel_dispatcher::COUPLING_TYPE_SPLIT_RAILS;
}

void action_toggle_enable_tracking_mode_in_coupling() {
    auto page = (SysSettingsCouplingPage *)getActivePage();
    page->m_enableTrackingMode = !page->m_enableTrackingMode;
}

void action_toggle_channel_tracking() {
    selectChannelByCursor();
    auto page = (SysSettingsTrackingPage *)getActivePage();
    page->toggleChannelTracking(g_channel->channelIndex);
}

void action_untrack_all() {
    auto page = (SysSettingsTrackingPage *)getActivePage();
    page->untrackAll();
}

void action_sys_settings_protections_toggle_output_protection_couple() {
    SysSettingsProtectionsPage::toggleOutputProtectionCouple();
}

void action_sys_settings_protections_toggle_shutdown_when_protection_tripped() {
    SysSettingsProtectionsPage::toggleShutdownWhenProtectionTripped();
}

void action_sys_settings_protections_toggle_force_disabling_all_outputs_on_power_up() {
    SysSettingsProtectionsPage::toggleForceDisablingAllOutputsOnPowerUp();
}

void action_sys_settings_protections_aux_otp_toggle_state() {
    ((SysSettingsTemperaturePage *)getActivePage())->toggleState();
}

void action_sys_settings_protections_aux_otp_edit_level() {
    ((SysSettingsTemperaturePage *)getActivePage())->editLevel();
}

void action_sys_settings_protections_aux_otp_edit_delay() {
    ((SysSettingsTemperaturePage *)getActivePage())->editDelay();
}

void action_sys_settings_protections_aux_otp_clear() {
    SysSettingsTemperaturePage::clear();
}

void action_sys_settings_fan_toggle_mode() {
    ((SysSettingsTemperaturePage *)getActivePage())->toggleFanMode();
}

void action_sys_settings_fan_edit_speed() {
    ((SysSettingsTemperaturePage *)getActivePage())->editFanSpeed();
}

void action_edit_system_password() {
    editSystemPassword();
}

void action_sys_front_panel_lock() {
    lockFrontPanel();
}

void action_sys_front_panel_unlock() {
    unlockFrontPanel();
}

void action_sys_settings_sound_toggle() {
    ((SysSettingsSoundPage *)getActivePage())->toggleSound();
}

void action_sys_settings_sound_toggle_click() {
    ((SysSettingsSoundPage *)getActivePage())->toggleClickSound();
}

void action_ch_settings_adv_view_edit() {
    ((ChSettingsAdvViewPage *)getActivePage())->edit();
}

void action_sys_settings_encoder_toggle_confirmation_mode() {
#if OPTION_ENCODER
    ((SysSettingsEncoderPage *)getActivePage())->toggleConfirmationMode();
#endif
}

void action_ch_settings_trigger_edit_trigger_mode() {
    ((ChSettingsTriggerPage *)getActivePage())->editTriggerMode();
}

void action_ch_settings_lists_edit_list_count() {
    ((ChSettingsListsPage *)getActivePage())->editListCount();
}

void action_ch_settings_lists_edit_on_list_stop() {
    ((ChSettingsListsPage *)getActivePage())->editTriggerOnListStop();
}

void action_channel_lists_previous_page() {
    ((ChSettingsListsPage *)getActivePage())->previousPage();
}

void action_channel_lists_next_page() {
    ((ChSettingsListsPage *)getActivePage())->nextPage();
}

void action_channel_lists_edit() {
    ((ChSettingsListsPage *)getActivePage())->edit();
}

void action_show_channel_lists_insert_menu() {
    ((ChSettingsListsPage *)getActivePage())->showInsertMenu();
}

void action_channel_lists_insert_row_above() {
    popPage();
    ((ChSettingsListsPage *)getActivePage())->insertRowAbove();
}

void action_channel_lists_insert_row_below() {
    popPage();
    ((ChSettingsListsPage *)getActivePage())->insertRowBelow();
}

void action_show_channel_lists_delete_menu() {
    ((ChSettingsListsPage *)getActivePage())->showDeleteMenu();
}

void action_channel_lists_delete_row() {
    popPage();
    ((ChSettingsListsPage *)getActivePage())->deleteRow();
}

void action_channel_lists_clear_column() {
    popPage();
    ((ChSettingsListsPage *)getActivePage())->clearColumn();
}

void action_channel_lists_delete_rows() {
    popPage();
    ((ChSettingsListsPage *)getActivePage())->deleteRows();
}

void action_channel_lists_delete_all() {
    popPage();
    ((ChSettingsListsPage *)getActivePage())->deleteAll();
}

void action_show_channel_lists_file_menu() {
    ((ChSettingsListsPage *)getActivePage())->showFileMenu();
}

void action_channel_lists_file_import() {
    popPage();
    ((ChSettingsListsPage *)getActivePage())->fileImport();
}

void action_channel_lists_file_export() {
    popPage();
    ((ChSettingsListsPage *)getActivePage())->fileExport();
}

void action_trigger_select_source() {
    ((SysSettingsTriggerPage *)getActivePage())->selectSource();
}

void action_trigger_edit_delay() {
    ((SysSettingsTriggerPage *)getActivePage())->editDelay();
}

void action_trigger_toggle_initiate_continuously() {
    ((SysSettingsTriggerPage *)getActivePage())->toggleInitiateContinuously();
}

void action_trigger_generate_manual() {
    trigger::generateTrigger(trigger::SOURCE_MANUAL, false);
}

void action_trigger_show_general_settings() {
    Page *page = getActivePage();
    if (page && page->getDirty()) {
        page->set();
    }
    pushPage(PAGE_ID_SYS_SETTINGS_TRIGGER);
}

void action_show_stand_by_menu() {
    pushPage(PAGE_ID_STAND_BY_MENU);
}

void action_reset() {
    popPage();
    eez::reset();
}

void action_stand_by() {
    popPage();
    eez::standBy();
}

void action_restart() {
    popPage();
    yesNoDialog(PAGE_ID_YES_NO, "Do you want to restart?", eez::restart, nullptr, nullptr);
}

void action_shutdown() {
    popPage();
    yesNoDialog(PAGE_ID_YES_NO, "Do you want to shutdown?", eez::shutdown, nullptr, nullptr);
}

void action_turn_display_off() {
    popPage();
    psu::persist_conf::setDisplayState(0);
}

void action_ch_settings_adv_ranges_select_mode() {
    ((ChSettingsAdvRangesPage *)getActivePage())->selectMode();
}

void action_ch_settings_adv_ranges_toggle_auto_ranging() {
    ((ChSettingsAdvRangesPage *)getActivePage())->toggleAutoRanging();
}

void action_io_pin_toggle_polarity() {
    ((SysSettingsIOPinsPage *)getActivePage())->togglePolarity();
}

void action_io_pin_toggle_state() {
    int pin = getFoundWidgetAtDown().cursor;
    io_pins::setPinState(pin, io_pins::getPinState(pin) ? 0 : 1);
}

void action_io_pin_select_function() {
    ((SysSettingsIOPinsPage *)getActivePage())->selectFunction();
}

void action_ntp_toggle() {
    ((SysSettingsDateTimePage *)getActivePage())->toggleNtp();
}

void action_ntp_edit_server() {
    ((SysSettingsDateTimePage *)getActivePage())->editNtpServer();
}

#if defined(EEZ_PLATFORM_SIMULATOR)

void onSimulatorLoadSet(float value) {
	g_frontPanelAppContext.popPage();
	channel_dispatcher::setLoadEnabled(*g_channel, true);
	channel_dispatcher::setLoad(*g_channel, value);
}

void onSimulatorDisconnectLoad() {
    g_frontPanelAppContext.popPage();
	channel_dispatcher::setLoadEnabled(*g_channel, false);
}

void selectSimulatorLoad() {
    NumericKeypadOptions options;

    options.pageId = PAGE_ID_FRONT_PANEL_NUMERIC_KEYPAD;

    options.editValueUnit = UNIT_OHM;

    options.min = 0;

    options.flags.signButtonEnabled = false;
    options.flags.dotButtonEnabled = true;
    options.flags.option1ButtonEnabled = true;
    options.option1ButtonText = "Off";
    options.option1 = onSimulatorDisconnectLoad;

    NumericKeypad::start(&g_frontPanelAppContext, 0, MakeValue(g_channel->simulator.getLoad(), UNIT_OHM), options, onSimulatorLoadSet, 0, 0);
}

void action_simulator_load() {
    int channelIndex = getFoundWidgetAtDown().cursor;
    if (getFoundWidgetAtDown().widget->data == DATA_ID_SIMULATOR_LOAD_STATE2) {
        channelIndex++;
    }
    selectChannel(&Channel::get(channelIndex));
    selectSimulatorLoad();
}

#endif

void themesEnumDefinition(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET_VALUE) {
        value = (uint8_t)cursor;
    } else if (operation == DATA_OPERATION_GET_LABEL) {
		if (cursor < getThemesCount()) {
			value = getThemeName(cursor);
		}
    }
}

void onSetSelectedThemeIndex(uint16_t value) {
    popPage();
	persist_conf::setSelectedThemeIndex((uint8_t)value);
    mcu::display::onThemeChanged();
	refreshScreen();
}

void action_select_theme() {
    pushSelectFromEnumPage(themesEnumDefinition, persist_conf::devConf.selectedThemeIndex, NULL, onSetSelectedThemeIndex);
}

void onSetAnimationsDuration(float value) {
    popPage();
    psu::persist_conf::setAnimationsDuration(value);
}

void action_edit_animations_duration() {
    NumericKeypadOptions options;

    options.editValueUnit = UNIT_SECOND;

    options.min = 0.0f;
    options.max = 5.0f;
    options.def = CONF_DEFAULT_ANIMATIONS_DURATION;

    options.enableDefButton();
    options.flags.signButtonEnabled = false;
    options.flags.dotButtonEnabled = true;

    NumericKeypad::start(0, Value(psu::persist_conf::devConf.animationsDuration, UNIT_SECOND), options, onSetAnimationsDuration, 0, 0);
}

void action_user_switch_clicked() {
	if (g_shutdownInProgress || psu::persist_conf::devConf.displayState == 0) {
		return;
	}

    if (getActivePageId() == PAGE_ID_SYS_SETTINGS_DISPLAY_TEST) {
        popPage();
        return;
    }

    if (isFrontPanelLocked()) {
        errorMessage("Front panel is locked!");
        return;
    }

    if (getActiveSelectEnumDefinition() == g_enumDefinitions[ENUM_DEFINITION_USER_SWITCH_ACTION]) {
        popSelectFromEnumPage();
        return;
    }

    switch (persist_conf::devConf.userSwitchAction) {
    case persist_conf::USER_SWITCH_ACTION_NONE:
        action_select_user_switch_action();
    	break;

    case persist_conf::USER_SWITCH_ACTION_ENCODER_STEP:
#if OPTION_ENCODER
        if (getActivePageId() == PAGE_ID_EDIT_MODE_STEP) {
            psu::gui::edit_mode_step::switchToNextStepIndex();
        } else {
            if (getActivePageId() == INTERNAL_PAGE_ID_TOAST_MESSAGE && !((ToastMessagePage *)getActivePage())->hasAction()) {
                popPage();
            }

            if (psu::gui::isEncoderEnabledInActivePage() || getActivePageId() == PAGE_ID_CH_SETTINGS_LISTS) {
                for (int i = mcu::encoder::ENCODER_MODE_MIN; i < mcu::encoder::ENCODER_MODE_MAX; i++) {
                    mcu::encoder::switchEncoderMode();
                    if (psu::gui::edit_mode_step::hasEncoderStepValue()) {
                        break;
                    }
                }
                psu::gui::edit_mode_step::showCurrentEncoderMode();
            }
        }
#endif
        break;

    case persist_conf::USER_SWITCH_ACTION_SCREENSHOT:
        takeScreenshot();
        break;

    case persist_conf::USER_SWITCH_ACTION_MANUAL_TRIGGER:
        action_trigger_generate_manual();
        break;

    case persist_conf::USER_SWITCH_ACTION_OUTPUT_ENABLE:
        for (int i = 0; i < CH_NUM; ++i) {
            Channel &channel = Channel::get(i);
            if (channel.flags.trackingEnabled) {
                channel_dispatcher::outputEnable(channel, !channel.isOutputEnabled());
                return;
            }
        }
        infoMessage("Tracking is not enabled.");
        break;

    case persist_conf::USER_SWITCH_ACTION_HOME:
        goBack();
        break;

    case persist_conf::USER_SWITCH_ACTION_INHIBIT:
        io_pins::setIsInhibitedByUser(!io_pins::getIsInhibitedByUser());
        break;

    case persist_conf::USER_SWITCH_ACTION_STANDBY:
    	changePowerState(isPowerUp() ? false : true);
    	break;
    }
}

void onSetUserSwitchAction(uint16_t value) {
    popPage();
    persist_conf::setUserSwitchAction((persist_conf::UserSwitchAction)value);
}

void action_select_user_switch_action() {
    if (
        getActivePageId() == PAGE_ID_STAND_BY_MENU ||
        getActivePageId() == PAGE_ID_SHUTDOWN ||
        getActivePageId() == PAGE_ID_ENTERING_STANDBY ||
        getActivePageId() == PAGE_ID_STANDBY ||
        getActivePageId() == PAGE_ID_SAVING ||
        getActivePageId() == PAGE_ID_DISPLAY_OFF ||
        getActivePageId() == PAGE_ID_WELCOME

    ) {
        return;
    }

    if (getActiveSelectEnumDefinition() == g_enumDefinitions[ENUM_DEFINITION_USER_SWITCH_ACTION]) {
    	popSelectFromEnumPage();
    } else {
        clearFoundWidgetAtDown();
        pushSelectFromEnumPage(ENUM_DEFINITION_USER_SWITCH_ACTION, persist_conf::devConf.userSwitchAction, nullptr, onSetUserSwitchAction);
    }
}

#if defined(EEZ_PLATFORM_SIMULATOR)

void onSetModuleType(uint16_t moduleType) {
    g_frontPanelAppContext.popPage();

    bp3c::eeprom::writeModuleType(hmi::g_selectedSlotIndex, moduleType);

#ifdef __EMSCRIPTEN__
    infoMessage("Reload page to apply change!");
#else
    infoMessage("Restart program to apply change!");
#endif
}

void selectSlotModuleType(int slotIndex) {
    hmi::selectSlot(slotIndex);
    pushSelectFromEnumPage(&g_frontPanelAppContext, ENUM_DEFINITION_MODULE_TYPE, g_slots[slotIndex]->moduleType, NULL, onSetModuleType);
}

void action_front_panel_select_slot1() {
    selectSlotModuleType(0);
}

void action_front_panel_select_slot2() {
    selectSlotModuleType(1);
}

void action_front_panel_select_slot3() {
    selectSlotModuleType(2);
}

#endif

void action_drag_overlay() {
}

void action_show_file_manager() {
    file_manager::openFileManager();
}

void action_show_sys_settings_mqtt() {
    pushPage(PAGE_ID_SYS_SETTINGS_MQTT);
}

void action_mqtt_toggle() {
#if OPTION_ETHERNET
    ((SysSettingsMqttPage *)getActivePage())->toggle();
#endif
}

void action_mqtt_edit_host() {
    editValue(DATA_ID_MQTT_HOST);
}

void action_mqtt_edit_port() {
    editValue(DATA_ID_MQTT_PORT);
}

void action_mqtt_edit_username() {
    editValue(DATA_ID_MQTT_USERNAME);
}

void action_mqtt_edit_password() {
    editValue(DATA_ID_MQTT_PASSWORD);
}

void action_mqtt_edit_period() {
    editValue(DATA_ID_MQTT_PERIOD);
}

void onFirmwareSelected(const char *filePath) {
    bp3c::flash_slave::start(hmi::g_selectedSlotIndex, filePath);
}

void onSelectFirmware() {
    file_manager::browseForFile("Select firmware file", "/Updates", FILE_TYPE_HEX, file_manager::DIALOG_TYPE_OPEN, onFirmwareSelected);
}

void action_channel_update_firmware() {
    yesNoDialog(PAGE_ID_YES_NO_FLASH_SLAVE, nullptr, onSelectFirmware, nullptr, nullptr);
}

void action_show_sys_settings_ramp_and_delay() {
    Page *page = getActivePage();
    if (page && page->getDirty()) {
        page->set();
    }
    pushPage(PAGE_ID_SYS_SETTINGS_RAMP_AND_DELAY);
}

void action_channel_toggle_ramp_state() {
    auto page = (SysSettingsRampAndDelayPage *)getPage(PAGE_ID_SYS_SETTINGS_RAMP_AND_DELAY);
    if (page) {
        page->toggleRampState(getFoundWidgetAtDown().cursor);
    }
}

void doSetDeviceMode() {
    sendMessageToLowPriorityThread(THREAD_MESSAGE_SELECT_USB_MODE, USB_MODE_DEVICE);
}

void onSetUsbMode(uint16_t value) {
	popPage();
    if (value == USB_MODE_DEVICE && usb::isOtgHostModeDetected()) {
        areYouSureWithMessage("OTG cable detected.", doSetDeviceMode);
    } else {
        sendMessageToLowPriorityThread(THREAD_MESSAGE_SELECT_USB_MODE, value);
    }
}

void action_select_usb_mode() {
    pushSelectFromEnumPage(ENUM_DEFINITION_USB_MODE, g_usbMode, nullptr, onSetUsbMode);
}

void action_select_usb_device_class() {
    auto isUsbDeviceClassDisabled = [] (uint16_t value) {
        return value == USB_DEVICE_CLASS_MASS_STORAGE_CLIENT && dlog_record::isExecuting();
    };

    auto onSetUsbDeviceClass = [] (uint16_t value) {
        popPage();
        sendMessageToLowPriorityThread(THREAD_MESSAGE_SELECT_USB_DEVICE_CLASS, value);
    };

    pushSelectFromEnumPage(ENUM_DEFINITION_USB_DEVICE_CLASS, g_usbDeviceClass, isUsbDeviceClassDisabled, onSetUsbDeviceClass);
}

void action_select_mass_storage_device() {
	auto onSelectMassStorageDevice = [] (uint16_t value) {
	    popPage();
        sendMessageToLowPriorityThread(THREAD_MESSAGE_SELECT_USB_MASS_STORAGE_DEVICE, value);
	};

	auto massStorageDeviceEnumDefinition = [] (DataOperationEnum operation, Cursor cursor, Value &value) {
		int massStorageDevice = fs_driver::getDiskDriveIndex(cursor, true);
		if (massStorageDevice >= 0) {
			if (operation == DATA_OPERATION_GET_VALUE) {
				value = (uint8_t)massStorageDevice;
			} else if (operation == DATA_OPERATION_GET_LABEL) {
				value = Value(massStorageDevice, VALUE_TYPE_MASS_STORAGE_DEVICE_LABEL);
			}
		}
	};

	auto massStorageDeviceIsDisabledCallback = [] (uint16_t massStorageDevice) {
		if (massStorageDevice == g_selectedMassStorageDevice) {
			return false;
		}

		if (massStorageDevice == 0) {
			return !sd_card::isMounted(nullptr, nullptr);
		} else {
			return !fs_driver::isDriverLinked(massStorageDevice - 1);
		}
	};

	pushSelectFromEnumPage(massStorageDeviceEnumDefinition, g_selectedMassStorageDevice, massStorageDeviceIsDisabledCallback, onSelectMassStorageDevice, false, true);
}

void action_show_display_test_page() {
    pushPage(PAGE_ID_SYS_SETTINGS_DISPLAY_TEST);
    infoMessage("Switch colors by screen tap or encoder.\nTo exit push encoder or user switch.", nullptr, "Close");
}

void action_toggle_display_test_color_index() {
    psu::gui::g_displayTestColorIndex = (psu::gui::g_displayTestColorIndex + 1) % 4;
}

void onSetNtpRefreshFrequency(float value) {
    popPage();
    SysSettingsDateTimePage *page = (SysSettingsDateTimePage *)getPage(PAGE_ID_SYS_SETTINGS_DATE_TIME);
    page->ntpRefreshFrequency = (uint32_t)value;
}

void action_edit_ntp_refresh_frequency() {
    NumericKeypadOptions options;

    options.min = NTP_REFRESH_FREQUENCY_MIN;
    options.max = NTP_REFRESH_FREQUENCY_MAX;
    options.def = NTP_REFRESH_FREQUENCY_DEF;

    options.enableMaxButton();
    options.enableDefButton();
    options.enableMinButton();

    SysSettingsDateTimePage *page = (SysSettingsDateTimePage *)getPage(PAGE_ID_SYS_SETTINGS_DATE_TIME);
    NumericKeypad::start(0, Value(page->ntpRefreshFrequency, VALUE_TYPE_UINT32), options, onSetNtpRefreshFrequency, 0, 0);
}

void action_module_resync() {
    sendMessageToPsu(PSU_MESSAGE_MODULE_RESYNC, hmi::g_selectedSlotIndex);
}

void action_select_ac_mains() {
    SysSettingsDateTimePage *page = (SysSettingsDateTimePage *)getPage(PAGE_ID_SYS_SETTINGS_DATE_TIME);
    page->powerLineFrequency = page->powerLineFrequency == 50 ? 60 : 50;
}

} // namespace gui
} // namespace eez

#endif
