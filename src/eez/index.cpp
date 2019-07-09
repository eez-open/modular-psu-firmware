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
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <eez/index.h>

#include <eez/apps/psu/gui/psu.h>
#include <eez/apps/psu/init.h>
#include <eez/apps/settings/settings.h>
#include <eez/gui/gui.h>
#include <eez/modules/mcu/display.h>
#include <eez/modules/mcu/touch.h>

#include <eez/gui/document.h>

#if OPTION_DISPLAY
#include <eez/apps/psu/psu.h>

#include <eez/apps/psu/gui/page_ch_settings_adv.h>
#include <eez/apps/psu/gui/page_ch_settings_protection.h>
#include <eez/apps/psu/gui/page_ch_settings_trigger.h>
#include <eez/apps/psu/gui/page_event_queue.h>
#include <eez/apps/psu/gui/page_self_test_result.h>
#include <eez/apps/psu/gui/page_sys_settings.h>
#include <eez/apps/psu/gui/page_user_profiles.h>
using namespace eez::psu::gui;
#endif

#if OPTION_ETHERNET
#include <eez/modules/mcu/ethernet.h>
#endif

namespace eez {

OnSystemStateChangedCallback g_onSystemStateChangedCallbacks[] = {
    // modules
    mcu::display::onSystemStateChanged,
#if OPTION_ETHERNET
    mcu::ethernet::onSystemStateChanged,
#endif    
    mcu::touch::onSystemStateChanged,
    // subsystems
    gui::onSystemStateChanged,
    // applications
    psu::onSystemStateChanged,
};

int g_numOnSystemStateChangedCallbacks =
    sizeof(g_onSystemStateChangedCallbacks) / sizeof(OnSystemStateChangedCallback);

SlotInfo g_slots[NUM_SLOTS] = {
    {
    	MODULE_TYPE_NONE
    },
    {
    	MODULE_TYPE_NONE
    },
    {
        MODULE_TYPE_NONE
    }
};

#if OPTION_DISPLAY

ApplicationInfo g_applications[] = {
    { "PSU", gui::BITMAP_ID_PSU_ICON, &psu::gui::g_psuAppContext },
    { "Settings", gui::BITMAP_ID_SETTINGS_ICON, &settings::g_settingsAppContext },
};

int g_numApplications = sizeof(g_applications) / sizeof(ApplicationInfo);

namespace gui {

static SelfTestResultPage g_SelfTestResultPage;
static EventQueuePage g_EventQueuePage;
static ChSettingsProtectionPage g_ChSettingsProtectionPage;
static ChSettingsOvpProtectionPage g_ChSettingsOvpProtectionPage;
static ChSettingsOcpProtectionPage g_ChSettingsOcpProtectionPage;
static ChSettingsOppProtectionPage g_ChSettingsOppProtectionPage;
static ChSettingsOtpProtectionPage g_ChSettingsOtpProtectionPage;
static ChSettingsAdvRemotePage g_ChSettingsAdvRemotePage;
static ChSettingsAdvRangesPage g_ChSettingsAdvRangesPage;
static ChSettingsAdvTrackingPage g_ChSettingsAdvTrackingPage;
static ChSettingsAdvCouplingPage g_ChSettingsAdvCouplingPage;
static ChSettingsAdvViewPage g_ChSettingsAdvViewPage;
static ChSettingsTriggerPage g_ChSettingsTriggerPage;
static ChSettingsListsPage g_ChSettingsListsPage;
static SysSettingsDateTimePage g_SysSettingsDateTimePage;
static SysSettingsEthernetPage g_SysSettingsEthernetPage;
static SysSettingsEthernetStaticPage g_SysSettingsEthernetStaticPage;
static SysSettingsProtectionsPage g_SysSettingsProtectionsPage;
static SysSettingsTriggerPage g_SysSettingsTriggerPage;
static SysSettingsIOPinsPage g_SysSettingsIOPinsPage;
static SysSettingsAuxOtpPage g_SysSettingsAuxOtpPage;
static SysSettingsSoundPage g_SysSettingsSoundPage;
static SysSettingsEncoderPage g_SysSettingsEncoderPage;
static SysSettingsSerialPage g_SysSettingsSerialPage;
static UserProfilesPage g_UserProfilesPage;

Page *getPageFromId(int pageId) {
    switch (pageId) {
    case PAGE_ID_SELF_TEST_RESULT:
        return &g_SelfTestResultPage;
    case PAGE_ID_EVENT_QUEUE:
        return &g_EventQueuePage;
    case PAGE_ID_CH_SETTINGS_PROT:
        return &g_ChSettingsProtectionPage;
    case PAGE_ID_CH_SETTINGS_PROT_OVP:
        return &g_ChSettingsOvpProtectionPage;
    case PAGE_ID_CH_SETTINGS_PROT_OCP:
        return &g_ChSettingsOcpProtectionPage;
    case PAGE_ID_CH_SETTINGS_PROT_OPP:
        return &g_ChSettingsOppProtectionPage;
    case PAGE_ID_CH_SETTINGS_PROT_OTP:
        return &g_ChSettingsOtpProtectionPage;
    case PAGE_ID_CH_SETTINGS_ADV_REMOTE:
        return &g_ChSettingsAdvRemotePage;
    case PAGE_ID_CH_SETTINGS_ADV_RANGES:
        return &g_ChSettingsAdvRangesPage;
    case PAGE_ID_CH_SETTINGS_ADV_TRACKING:
        return &g_ChSettingsAdvTrackingPage;
    case PAGE_ID_CH_SETTINGS_ADV_COUPLING:
    case PAGE_ID_CH_SETTINGS_ADV_COUPLING_INFO:
        return &g_ChSettingsAdvCouplingPage;
    case PAGE_ID_CH_SETTINGS_ADV_VIEW:
        return &g_ChSettingsAdvViewPage;
    case PAGE_ID_CH_SETTINGS_TRIGGER:
        return &g_ChSettingsTriggerPage;
    case PAGE_ID_CH_SETTINGS_LISTS:
        return &g_ChSettingsListsPage;
    case PAGE_ID_SYS_SETTINGS_DATE_TIME:
        return &g_SysSettingsDateTimePage;
#if OPTION_ETHERNET
    case PAGE_ID_SYS_SETTINGS_ETHERNET:
        return &g_SysSettingsEthernetPage;
    case PAGE_ID_SYS_SETTINGS_ETHERNET_STATIC:
        return &g_SysSettingsEthernetStaticPage;
#endif
    case PAGE_ID_SYS_SETTINGS_PROTECTIONS:
        return &g_SysSettingsProtectionsPage;
    case PAGE_ID_SYS_SETTINGS_TRIGGER:
        return &g_SysSettingsTriggerPage;
    case PAGE_ID_SYS_SETTINGS_IO:
        return &g_SysSettingsIOPinsPage;
    case PAGE_ID_SYS_SETTINGS_AUX_OTP:
        return &g_SysSettingsAuxOtpPage;
    case PAGE_ID_SYS_SETTINGS_SOUND:
        return &g_SysSettingsSoundPage;
#if OPTION_ENCODER
    case PAGE_ID_SYS_SETTINGS_ENCODER:
        return &g_SysSettingsEncoderPage;
#endif
    case PAGE_ID_SYS_SETTINGS_SERIAL:
        return &g_SysSettingsSerialPage;
    case PAGE_ID_USER_PROFILES:
    case PAGE_ID_USER_PROFILES2:
    case PAGE_ID_USER_PROFILE_0_SETTINGS:
    case PAGE_ID_USER_PROFILE_SETTINGS:
        return &g_UserProfilesPage;
    }

    return nullptr;
}

} // namespace gui

#endif

} // namespace eez
