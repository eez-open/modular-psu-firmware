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
#include <eez/sound.h>
#include <eez/index.h>
#include <eez/mqtt.h>

#include <eez/scpi/scpi.h>

#include <eez/modules/psu/psu.h>
#include <eez/modules/psu/channel_dispatcher.h>
#include <eez/modules/psu/event_queue.h>
#include <eez/modules/psu/persist_conf.h>
#include <eez/modules/psu/trigger.h>
#include <eez/modules/psu/dlog_record.h>
#include <eez/modules/psu/dlog_view.h>

#include <eez/modules/psu/gui/psu.h>
#include <eez/modules/psu/gui/animations.h>
#include <eez/modules/psu/gui/calibration.h>
#include <eez/modules/psu/gui/data.h>
#include <eez/modules/psu/gui/edit_mode.h>
#include <eez/modules/psu/gui/keypad.h>
#include <eez/modules/psu/gui/page_ch_settings.h>
#include <eez/modules/psu/gui/page_sys_settings.h>
#include <eez/modules/psu/gui/page_user_profiles.h>
#include <eez/modules/psu/gui/password.h>
#include <eez/modules/psu/gui/data.h>
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

void action_enter_touch_calibration() {
    psu::gui::enterTouchCalibration();
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

void showMainPage() {
    showPage(PAGE_ID_MAIN);
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
    pushPage(PAGE_ID_SYS_SETTINGS_SERIAL);
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

void action_show_main_help_page() {
    showPage(PAGE_ID_MAIN_HELP);
}

void action_show_edit_mode_step_help() {
    pushPage(PAGE_ID_EDIT_MODE_STEP_HELP);
}

void action_show_edit_mode_slider_help() {
    pushPage(PAGE_ID_EDIT_MODE_SLIDER_HELP);
}

void action_show_ch_settings() {
    selectChannel();
    showPage(PAGE_ID_CH_SETTINGS);
}

void action_show_ch_settings_prot_clear() {
    selectChannel();
    pushPage(PAGE_ID_CH_SETTINGS_PROT_CLEAR);
}

void action_show_ch_settings_prot_ocp() {
    selectChannel();
    pushPage(PAGE_ID_CH_SETTINGS_PROT_OCP);
}

void action_show_ch_settings_prot_ovp() {
    selectChannel();
    pushPage(PAGE_ID_CH_SETTINGS_PROT_OVP);
}

void action_show_ch_settings_prot_opp() {
    selectChannel();
    pushPage(PAGE_ID_CH_SETTINGS_PROT_OPP);
}

void action_show_ch_settings_prot_otp() {
    selectChannel();
    pushPage(PAGE_ID_CH_SETTINGS_PROT_OTP);
}

void action_show_ch_settings_trigger() {
    selectChannel();
    pushPage(PAGE_ID_CH_SETTINGS_TRIGGER);
}

void action_show_ch_settings_lists() {
    selectChannel();
    pushPage(PAGE_ID_CH_SETTINGS_LISTS);
}

void action_show_ch_settings_adv_options() {
    selectChannel();
    pushPage(PAGE_ID_CH_SETTINGS_ADV_OPTIONS);
}

void action_show_ch_settings_adv_ranges() {
    selectChannel();
    pushPage(PAGE_ID_CH_SETTINGS_ADV_RANGES);
}

void action_show_sys_settings_tracking() {
    selectChannel();
    pushPage(PAGE_ID_SYS_SETTINGS_TRACKING);
}

void action_show_sys_settings_coupling() {
    selectChannel();
    pushPage(PAGE_ID_SYS_SETTINGS_COUPLING);
}

void action_show_ch_settings_adv_view() {
    selectChannel();
    pushPage(PAGE_ID_CH_SETTINGS_ADV_VIEW);
}

void action_show_ch_settings_info() {
    selectChannel();
    pushPage(PAGE_ID_CH_SETTINGS_INFO);
}

void action_show_ch_settings_cal() {
    selectChannel();
    pushPage(PAGE_ID_CH_SETTINGS_CALIBRATION);
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

void channelsEnumDefinition(data::DataOperationEnum operation, data::Cursor cursor, data::Value &value) {
    int channelIndex = cursor < g_channel->channelIndex ? cursor : cursor + 1;

    if (operation == data::DATA_OPERATION_GET_VALUE) {
        value = (uint8_t)channelIndex;
    } else if (operation == data::DATA_OPERATION_GET_LABEL) {
		if (channelIndex < CH_NUM) {
			value = Value(channelIndex, VALUE_TYPE_CHANNEL_SHORT_LABEL_WITHOUT_COLUMN);
		}
    }
}

void action_ch_settings_copy() {
    if (CH_NUM >= 3) {
        pushSelectFromEnumPage(channelsEnumDefinition, -1, nullptr, onChannelCopyDestinationSelected, false, false);
    } else {
        onChannelCopyDestinationSelected(g_channel->channelIndex ? 0 : 1);
    }
}

void action_ch_settings_calibration_wiz_start() {
    calibration_wizard::start();
}

void action_ch_settings_calibration_wiz_step_previous() {
    calibration_wizard::previousStep();
}

void action_ch_settings_calibration_wiz_step_next() {
    calibration_wizard::nextStep();
}

void action_ch_settings_calibration_wiz_stop_and_show_previous_page() {
    calibration_wizard::stop(popPage);
}

void action_ch_settings_calibration_wiz_stop_and_show_main_page() {
    calibration_wizard::stop(action_show_main_page);
}

void action_ch_settings_calibration_wiz_step_set() {
    calibration_wizard::set();
}

void action_ch_settings_calibration_wiz_step_set_level_value() {
    calibration_wizard::setLevelValue();
}

void action_ch_settings_calibration_wiz_save() {
    calibration_wizard::save();
}

void action_ch_settings_calibration_toggle_enable() {
    calibration_wizard::toggleEnable();
}

void action_ch_settings_prot_clear() {
    ChSettingsProtectionPage::clear();
}

void action_ch_settings_prot_clear_and_disable() {
    ChSettingsProtectionPage::clearAndDisable();
}

void action_ch_settings_prot_toggle_state() {
    ((ChSettingsProtectionSetPage *)getActivePage())->toggleState();
}

void action_ch_settings_prot_toggle_type() {
    if (getActivePageId() == PAGE_ID_CH_SETTINGS_PROT_OVP) {
        ((ChSettingsProtectionSetPage *)getActivePage())->toggleType();
    } else {
        selectChannel();
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
    selectChannel();

    if (getActivePageId() != PAGE_ID_MAIN) {
        showMainPage();
        persist_conf::setMaxChannelIndex(g_channel->channelIndex);
        animateFromMicroViewToMaxView();
    } else {
        auto isMaxChannelViewBefore = persist_conf::isMaxChannelView();
        auto maxChannelIndexBefore = persist_conf::getMaxChannelIndex();

        persist_conf::toggleMaxChannelIndex(g_channel->channelIndex);
        
        if (!isMaxChannelViewBefore && persist_conf::isMaxChannelView()) {
            animateFromDefaultViewToMaxView();
        } else if (isMaxChannelViewBefore && !persist_conf::isMaxChannelView()) {
            animateFromMaxViewToDefaultView();
        } else {
            animateFromMinViewToMaxView(maxChannelIndexBefore);
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
    selectChannel();
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

void action_ch_settings_adv_view_edit_display_value1() {
    ((ChSettingsAdvViewPage *)getActivePage())->editDisplayValue1();
}

void action_ch_settings_adv_view_edit_display_value2() {
    ((ChSettingsAdvViewPage *)getActivePage())->editDisplayValue2();
}

void action_ch_settings_adv_view_swap_display_values() {
    ((ChSettingsAdvViewPage *)getActivePage())->swapDisplayValues();
}

void action_ch_settings_adv_view_edit_ytview_rate() {
    ((ChSettingsAdvViewPage *)getActivePage())->editYTViewRate();
}

void action_sys_settings_encoder_toggle_confirmation_mode() {
    #if OPTION_ENCODER
    ((SysSettingsEncoderPage *)getActivePage())->toggleConfirmationMode();
    #endif
}

void action_ch_settings_trigger_edit_trigger_mode() {
    ((ChSettingsTriggerPage *)getActivePage())->editTriggerMode();
}

void action_ch_settings_trigger_edit_voltage_trigger_value() {
    ((ChSettingsTriggerPage *)getActivePage())->editVoltageTriggerValue();
}

void action_ch_settings_trigger_edit_current_trigger_value() {
    ((ChSettingsTriggerPage *)getActivePage())->editCurrentTriggerValue();
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

void action_channel_initiate_trigger() {
    channelInitiateTrigger();
}

void action_channel_set_to_fixed() {
    channelSetToFixed();
}

void action_channel_enable_output() {
    channelEnableOutput();
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

void action_serial_toggle() {
    persist_conf::enableSerial(!persist_conf::isSerialEnabled());
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

void themesEnumDefinition(data::DataOperationEnum operation, data::Cursor cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET_VALUE) {
        value = (uint8_t)cursor;
    } else if (operation == data::DATA_OPERATION_GET_LABEL) {
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
    options.max = 2.0f;
    options.def = CONF_DEFAULT_ANIMATIONS_DURATION;

    options.enableDefButton();
    options.flags.signButtonEnabled = false;
    options.flags.dotButtonEnabled = true;

    NumericKeypad::start(0, data::Value(psu::persist_conf::devConf.animationsDuration, UNIT_SECOND), options, onSetAnimationsDuration, 0, 0);
}

void action_user_switch_clicked() {
	if (g_shutdownInProgress || psu::persist_conf::devConf.displayState == 0) {
		return;
	}

    if (getActiveSelectEnumDefinition() == g_userSwitchActionEnumDefinition) {
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

            if (psu::gui::isEncoderEnabledInActivePage()) {
                mcu::encoder::switchEncoderMode();
                psu::gui::edit_mode_step::showCurrentEncoderMode();
            }
        }
#endif
        break;

    case persist_conf::USER_SWITCH_ACTION_SCREENSHOT:
        using namespace scpi;
        osMessagePut(g_scpiMessageQueueId, SCPI_QUEUE_MESSAGE(SCPI_QUEUE_MESSAGE_TARGET_NONE, SCPI_QUEUE_MESSAGE_SCREENSHOT, 0), osWaitForever);
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
        if (getNumPagesOnStack() > 1) {
            action_show_previous_page();
        } else if (getActivePageId() != PAGE_ID_MAIN) {
            showMainPage();
        } else if (persist_conf::isMaxChannelView()) {
            action_toggle_channels_max_view();
        }
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
    if (getActiveSelectEnumDefinition() == g_userSwitchActionEnumDefinition) {
    	popSelectFromEnumPage();
    } else {
        clearFoundWidgetAtDown();
        pushSelectFromEnumPage(g_userSwitchActionEnumDefinition, persist_conf::devConf.userSwitchAction, nullptr, onSetUserSwitchAction);
    }
}

#if defined(EEZ_PLATFORM_SIMULATOR)

static int g_slotIndex;

void onSetModuleType(uint16_t moduleType) {
    g_frontPanelAppContext.popPage();

    bp3c::eeprom::writeModuleType(g_slotIndex, moduleType);

#ifdef __EMSCRIPTEN__
    infoMessage("Reload page to apply change!");
#else
    infoMessage("Restart program to apply change!");
#endif
}

void selectSlot(int slotIndex) {
    g_slotIndex = slotIndex;
    pushSelectFromEnumPage(&g_frontPanelAppContext, g_moduleTypeEnumDefinition, g_slots[slotIndex].moduleInfo->moduleType, NULL, onSetModuleType);
}

void action_front_panel_select_slot1() {
    selectSlot(0);
}

void action_front_panel_select_slot2() {
    selectSlot(1);
}

void action_front_panel_select_slot3() {
    selectSlot(2);
}

#endif

void action_drag_overlay() {
}

void action_show_dlog_params() {
    pushPage(PAGE_ID_DLOG_PARAMS);
}

void action_dlog_voltage_toggle() {
    dlog_record::g_guiParameters.logVoltage[getFoundWidgetAtDown().cursor] = !dlog_record::g_guiParameters.logVoltage[getFoundWidgetAtDown().cursor];
}

void action_dlog_current_toggle() {
    dlog_record::g_guiParameters.logCurrent[getFoundWidgetAtDown().cursor] = !dlog_record::g_guiParameters.logCurrent[getFoundWidgetAtDown().cursor];
}

void action_dlog_power_toggle() {
    dlog_record::g_guiParameters.logPower[getFoundWidgetAtDown().cursor] = !dlog_record::g_guiParameters.logPower[getFoundWidgetAtDown().cursor];
}

void action_dlog_edit_period() {
    editValue(DATA_ID_DLOG_PERIOD);
}

void action_dlog_edit_duration() {
    editValue(DATA_ID_DLOG_DURATION);
}

void action_dlog_edit_file_name() {
    editValue(DATA_ID_DLOG_FILE_NAME);
}

void action_dlog_toggle() {
    dlog_record::toggle();
}

void action_show_dlog_view() {
    dlog_view::g_showLatest = true;
    if (!dlog_record::isExecuting()) {
        dlog_view::openFile(dlog_record::getLatestFilePath());
    }
    showPage(PAGE_ID_DLOG_VIEW);
}

void action_dlog_start_recording() {
    popPage();

    char filePath[MAX_PATH_LENGTH + 1];

    if (isStringEmpty(dlog_record::g_guiParameters.filePath)) {
        uint8_t year, month, day, hour, minute, second;
        datetime::getDateTime(year, month, day, hour, minute, second);

        if (persist_conf::devConf.dateTimeFormat == datetime::FORMAT_DMY_24) {
            sprintf(filePath, "%s/%s%02d_%02d_%02d-%02d_%02d_%02d.dlog",
                RECORDINGS_DIR,
                dlog_record::g_guiParameters.filePath,
                (int)day, (int)month, (int)year,
                (int)hour, (int)minute, (int)second);
        } else if (persist_conf::devConf.dateTimeFormat == datetime::FORMAT_MDY_24) {
            sprintf(filePath, "%s/%s%02d_%02d_%02d-%02d_%02d_%02d.dlog",
                RECORDINGS_DIR,
                dlog_record::g_guiParameters.filePath,
                (int)month, (int)day, (int)year,
                (int)hour, (int)minute, (int)second);
        } else if (persist_conf::devConf.dateTimeFormat == datetime::FORMAT_DMY_12) {
            bool am;
            datetime::convertTime24to12(hour, am);
            sprintf(filePath, "%s/%s%02d_%02d_%02d-%02d_%02d_%02d_%s.dlog",
                RECORDINGS_DIR,
                dlog_record::g_guiParameters.filePath,
                (int)day, (int)month, (int)year,
                (int)hour, (int)minute, (int)second, am ? "AM" : "PM");
        } else if (persist_conf::devConf.dateTimeFormat == datetime::FORMAT_MDY_12) {
            bool am;
            datetime::convertTime24to12(hour, am);
            sprintf(filePath, "%s/%s%02d_%02d_%02d-%02d_%02d_%02d_%s.dlog",
                RECORDINGS_DIR,
                dlog_record::g_guiParameters.filePath,
                (int)month, (int)day, (int)year,
                (int)hour, (int)minute, (int)second, am ? "AM" : "PM");
        }
    } else {
        sprintf(filePath, "%s/%s.dlog", RECORDINGS_DIR, dlog_record::g_guiParameters.filePath);
    }

    memcpy(&dlog_record::g_parameters, &dlog_record::g_guiParameters, sizeof(dlog_record::g_guiParameters));
    strcpy(dlog_record::g_parameters.filePath, filePath);

    dlog_record::toggle();
}

void action_dlog_view_show_overlay_options() {
    pushPage(PAGE_ID_DLOG_VIEW_OVERLAY_OPTIONS);
}

void action_dlog_value_toggle() {
    dlog_view::Recording &recording = dlog_view::getRecording();
    int dlogValueIndex = getFoundWidgetAtDown().cursor;
    recording.dlogValues[dlogValueIndex].isVisible = !recording.dlogValues[dlogValueIndex].isVisible;
}

void action_dlog_view_toggle_legend() {
    dlog_view::g_showLegend = !dlog_view::g_showLegend;
}

void action_dlog_view_toggle_labels() {
    dlog_view::g_showLabels = !dlog_view::g_showLabels;
}

void action_dlog_view_select_visible_value() {
    dlog_view::Recording &recording = dlog_view::getRecording();
    recording.selectedVisibleValueIndex = (recording.selectedVisibleValueIndex + 1) % dlog_view::getNumVisibleDlogValues(recording);
}

void action_dlog_auto_scale() {
    dlog_view::autoScale(dlog_view::getRecording());
}

void action_dlog_scale_to_fit() {
    dlog_view::scaleToFit(dlog_view::getRecording());
}

void action_dlog_upload() {
    dlog_view::uploadFile();
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
    int err;
    if (!bp3c::flash_slave::start(g_channel->slotIndex, filePath, &err)) {
		errorMessage("Failed to start update!");
    }
}

void onSelectFirmware() {
    file_manager::browseForFile("Select DCM220 firmware file", "/Updates", FILE_TYPE_HEX, file_manager::DIALOG_TYPE_OPEN, onFirmwareSelected);
}

void action_channel_update_firmware() {
    yesNoDialog(PAGE_ID_YES_NO_FLASH_SLAVE, nullptr, onSelectFirmware, nullptr, nullptr);
}

} // namespace gui
} // namespace eez

#endif
