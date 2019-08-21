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

#include <eez/gui/dialogs.h>
#include <eez/gui/gui.h>
#include <eez/apps/home/touch_calibration.h>

#include <eez/apps/home/home.h>

#include <eez/apps/psu/psu.h>

#include <eez/apps/psu/channel_dispatcher.h>
#include <eez/apps/psu/event_queue.h>
#include <eez/apps/psu/persist_conf.h>
#include <eez/sound.h>
#include <eez/apps/psu/trigger.h>

#include <eez/apps/psu/gui/psu.h>

#include <eez/apps/psu/gui/animations.h>
#include <eez/apps/psu/gui/calibration.h>
#include <eez/apps/psu/gui/edit_mode.h>
#include <eez/apps/psu/gui/edit_mode_keypad.h>
#include <eez/apps/psu/gui/keypad.h>
#include <eez/apps/psu/gui/numeric_keypad.h>
#include <eez/apps/psu/gui/page_ch_settings_adv.h>
#include <eez/apps/psu/gui/page_ch_settings_protection.h>
#include <eez/apps/psu/gui/page_ch_settings_trigger.h>
#include <eez/apps/psu/gui/page_sys_settings.h>
#include <eez/apps/psu/gui/page_user_profiles.h>
#include <eez/apps/psu/gui/password.h>
#include <eez/apps/psu/gui/data.h>

#include <eez/scripting.h>

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
    if (edit_mode::isActive()) {
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

void action_keypad_caps() {
    getActiveKeypad()->caps();
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
    home::enterTouchCalibration();
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

void action_stand_by() {
	popPage();
    standBy();
}

void action_show_previous_page() {
    popPage();
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

void action_show_channel_settings() {
    selectChannel();
    showPage(PAGE_ID_CH_SETTINGS_PROT);
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

void action_show_sys_settings_cal() {
    pushPage(PAGE_ID_SYS_SETTINGS_CAL);
}

void action_show_sys_settings_cal_ch() {
    selectChannel();
    pushPage(PAGE_ID_SYS_SETTINGS_CAL_CH);
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

void action_show_sys_settings_protections() {
    pushPage(PAGE_ID_SYS_SETTINGS_PROTECTIONS);
}

void action_show_sys_settings_aux_otp() {
    pushPage(PAGE_ID_SYS_SETTINGS_AUX_OTP);
}

void action_show_sys_settings_sound() {
    pushPage(PAGE_ID_SYS_SETTINGS_SOUND);
}

void action_show_sys_settings_encoder() {
    pushPage(PAGE_ID_SYS_SETTINGS_ENCODER);
}

void action_show_sys_info() {
    showPage(PAGE_ID_SYS_INFO);
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

void action_show_ch_settings_prot() {
    selectChannel();
    showPage(PAGE_ID_CH_SETTINGS_PROT);
}

void action_show_ch_settings_prot_clear() {
    selectChannel();
    showPage(PAGE_ID_CH_SETTINGS_PROT_CLEAR);
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
    showPage(PAGE_ID_CH_SETTINGS_TRIGGER);
}

void action_show_ch_settings_lists() {
    selectChannel();
    pushPage(PAGE_ID_CH_SETTINGS_LISTS);
}

void action_show_ch_settings_adv() {
    selectChannel();
    showPage(PAGE_ID_CH_SETTINGS_ADV);
}

void action_show_ch_settings_adv_remote() {
    selectChannel();
    showPage(PAGE_ID_CH_SETTINGS_ADV_REMOTE);
}

void action_show_ch_settings_adv_ranges() {
    selectChannel();
    showPage(PAGE_ID_CH_SETTINGS_ADV_RANGES);
}

void action_show_ch_settings_adv_tracking() {
    selectChannel();
    showPage(PAGE_ID_CH_SETTINGS_ADV_TRACKING);
}

void action_show_ch_settings_adv_coupling() {
    selectChannel();
    showPage(PAGE_ID_CH_SETTINGS_ADV_COUPLING);
}

void action_show_ch_settings_adv_view() {
    selectChannel();
    pushPage(PAGE_ID_CH_SETTINGS_ADV_VIEW);
}

void action_show_ch_settings_info() {
    selectChannel();
    showPage(PAGE_ID_CH_SETTINGS_INFO);
}

void action_show_ch_settings_info_cal() {
    selectChannel();
    pushPage(PAGE_ID_SYS_SETTINGS_CAL_CH);
}

void action_sys_settings_cal_edit_password() {
    editCalibrationPassword();
}

void action_sys_settings_cal_ch_wiz_start() {
    calibration_wizard::start();
}

void action_sys_settings_cal_ch_wiz_step_previous() {
    calibration_wizard::previousStep();
}

void action_sys_settings_cal_ch_wiz_step_next() {
    calibration_wizard::nextStep();
}

void action_sys_settings_cal_ch_wiz_stop_and_show_previous_page() {
    calibration_wizard::stop(popPage);
}

void action_sys_settings_cal_ch_wiz_stop_and_show_main_page() {
    calibration_wizard::stop(action_show_main_page);
}

void action_sys_settings_cal_ch_wiz_step_set() {
    calibration_wizard::set();
}

void action_sys_settings_cal_ch_wiz_step_set_level_value() {
    calibration_wizard::setLevelValue();
}

void action_sys_settings_cal_ch_wiz_save() {
    calibration_wizard::save();
}

void action_sys_settings_cal_toggle_enable() {
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
	if (page && page->getDirty()) {
		areYouSureWithMessage(g_discardMessage, discard);
	}
	else {
		discard();
	}
}

void action_edit_field() {
    ((SetPage *)getActivePage())->edit();
}

void action_event_queue_previous_page() {
    event_queue::moveToPreviousPage();
    animateSlideRight();
}

void action_event_queue_next_page() {
    event_queue::moveToNextPage();
    animateSlideLeft();
}

void action_ch_settings_adv_remote_toggle_sense() {
    ((ChSettingsAdvRemotePage *)getActivePage())->toggleSense();
}

void action_ch_settings_adv_remote_toggle_programming() {
    ((ChSettingsAdvRemotePage *)getActivePage())->toggleProgramming();
}

void action_date_time_select_dst_rule() {
    ((SysSettingsDateTimePage *)getActivePage())->selectDstRule();
}

void action_show_user_profiles() {
    showPage(PAGE_ID_USER_PROFILES);
}

void action_show_user_profiles2() {
    showPage(PAGE_ID_USER_PROFILES2);
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
    ((UserProfilesPage *)getActivePage())->recall();
}

void action_profile_save() {
    ((UserProfilesPage *)getActivePage())->save();
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
        persist_conf::setChannelsMaxView(g_channel->index);
        animateFromMicroViewToMaxView();
    } else {
        auto channelsIsMaxView = persist_conf::devConf.flags.channelsIsMaxView;
        auto channelMax = persist_conf::devConf.flags.channelMax;

        persist_conf::toggleChannelsMaxView(g_channel->index);
        
        if (!channelsIsMaxView && persist_conf::devConf.flags.channelsIsMaxView) {
            animateFromDefaultViewToMaxView();
        } else if (channelsIsMaxView && !persist_conf::devConf.flags.channelsIsMaxView) {
            animateFromMaxViewToDefaultView();
        } else {
            animateFromMinViewToMaxView(channelMax);
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

void action_ch_settings_adv_coupling_uncouple() {
    ((ChSettingsAdvCouplingPage *)getActivePage())->uncouple();
}

void action_ch_settings_adv_coupling_set_parallel_info() {
    ((ChSettingsAdvCouplingPage *)getActivePage())->setParallelInfo();
}

void action_ch_settings_adv_coupling_set_series_info() {
    ((ChSettingsAdvCouplingPage *)getActivePage())->setSeriesInfo();
}

void action_ch_settings_adv_coupling_set_parallel() {
    ((ChSettingsAdvCouplingPage *)getActivePage())->setParallel();
}

void action_ch_settings_adv_coupling_set_series() {
    ((ChSettingsAdvCouplingPage *)getActivePage())->setSeries();
}

void action_ch_settings_adv_toggle_tracking_mode() {
    ((ChSettingsAdvTrackingPage *)getActivePage())->toggleTrackingMode();
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
    ((SysSettingsAuxOtpPage *)getActivePage())->toggleState();
}

void action_sys_settings_protections_aux_otp_edit_level() {
    ((SysSettingsAuxOtpPage *)getActivePage())->editLevel();
}

void action_sys_settings_protections_aux_otp_edit_delay() {
    ((SysSettingsAuxOtpPage *)getActivePage())->editDelay();
}

void action_sys_settings_protections_aux_otp_clear() {
    SysSettingsAuxOtpPage::clear();
}

void action_on_last_error_event_action() {
    onLastErrorEventAction();
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

void action_turn_display_off() {
    popPage();    
    turnDisplayOff();
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

void action_ch_settings_trigger_edit_list_count() {
    ((ChSettingsTriggerPage *)getActivePage())->editListCount();
}

void action_ch_settings_trigger_toggle_output_state() {
    ((ChSettingsTriggerPage *)getActivePage())->toggleOutputState();
}

void action_ch_settings_trigger_edit_on_list_stop() {
    ((ChSettingsTriggerPage *)getActivePage())->editTriggerOnListStop();
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

void action_show_channel_lists_delete_menu() {
    ((ChSettingsListsPage *)getActivePage())->showDeleteMenu();
}

void action_channel_lists_insert_row_above() {
    popPage();
    ((ChSettingsListsPage *)getActivePage())->insertRowAbove();
}

void action_channel_lists_insert_row_below() {
    popPage();
    ((ChSettingsListsPage *)getActivePage())->insertRowBelow();
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
    if (trigger::generateTrigger(trigger::SOURCE_MANUAL, false) != SCPI_ERROR_TRIGGER_IGNORED) {
        sound::playClick();
        return;
    }
}

void action_trigger_show_general_settings() {
    pushPage(PAGE_ID_SYS_SETTINGS_TRIGGER);
}

void action_show_stand_by_menu() {
    pushPage(PAGE_ID_STAND_BY_MENU);
}

void action_reset() {
    eez::gui::reset();
}

void hard_reset() {
#if defined(EEZ_PLATFORM_STM32)
    NVIC_SystemReset();
#endif
}

void action_hard_reset() {
    areYouSure(hard_reset);
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

void action_io_pin_select_function() {
    ((SysSettingsIOPinsPage *)getActivePage())->selectFunction();
}

void action_serial_toggle() {
    ((SysSettingsSerialPage *)getActivePage())->toggle();
}

void action_serial_select_parity() {
    ((SysSettingsSerialPage *)getActivePage())->selectParity();
}

void action_ntp_toggle() {
    ((SysSettingsDateTimePage *)getActivePage())->toggleNtp();
}

void action_ntp_edit_server() {
    ((SysSettingsDateTimePage *)getActivePage())->editNtpServer();
}

void action_open_application() {
    home::openApplication();
}

void action_show_home() {
    home::closeApplication();
}

#if defined(EEZ_PLATFORM_SIMULATOR)

void onSimulatorLoadSet(float value) {
	popPage();
	channel_dispatcher::setLoadEnabled(*g_channel, true);
	channel_dispatcher::setLoad(*g_channel, value);
}

void onSimulatorDisconnectLoad() {
	popPage();
	channel_dispatcher::setLoadEnabled(*g_channel, false);
}

void action_simulator_load() {
	selectChannel();

	NumericKeypadOptions options;

	options.pageId = PAGE_ID_NUMERIC_KEYPAD2;

	options.editValueUnit = UNIT_OHM;

	options.min = 0;

	options.flags.signButtonEnabled = false;
	options.flags.dotButtonEnabled = true;
	options.flags.option1ButtonEnabled = true;
	options.option1ButtonText = "Off";
	options.option1 = onSimulatorDisconnectLoad;

	NumericKeypad::start(
		0, MakeValue(g_channel->simulator.getLoad(), UNIT_OHM), options,
		onSimulatorLoadSet, 0, 0);
}

#endif

void themesEnumDefinition(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_GET_VALUE) {
        value = (uint8_t)cursor.i;
    } else if (operation == data::DATA_OPERATION_GET_LABEL) {
		if (cursor.i < (int)g_colorsData->themes.count) {
			value = g_colorsData->themes.first[cursor.i].name;
		} else {
			value = (const char *)NULL;
		}
    }
}


void onSetSelectedThemeIndex(uint8_t value) {
    popPage();
	persist_conf::devConf2.selectedThemeIndex = value;
	persist_conf::saveDevice2();
    mcu::display::onThemeChanged();
	refreshScreen();
}

void action_select_theme() {
    pushSelectFromEnumPage(themesEnumDefinition, persist_conf::devConf2.selectedThemeIndex, NULL, onSetSelectedThemeIndex);
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

    NumericKeypad::start(0, data::Value(psu::persist_conf::devConf2.animationsDuration, UNIT_SECOND), options, onSetAnimationsDuration, 0, 0);
}

void action_test() {
    infoMessage("Hello, world!");
}

void action_show_scripts() {
    pushPage(PAGE_ID_SCRIPTS);
}

void action_start_script() {
    // TODO
}

void action_set_scripts_page_mode_scripts() {
    scripting::g_scriptsPageMode = scripting::SCRIPTS_PAGE_MODE_SCRIPTS;
}

void action_set_scripts_page_mode_shell() {
    scripting::g_scriptsPageMode = scripting::SCRIPTS_PAGE_MODE_SHELL;
}

void action_scripts_previous_page() {
    scripting::g_currentPageIndex--;
}

void action_scripts_next_page() {
    scripting::g_currentPageIndex++;
}

} // namespace gui
} // namespace eez
