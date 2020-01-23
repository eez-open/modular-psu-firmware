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

#include <eez/mp.h>

#include <eez/modules/psu/psu.h>

#if OPTION_DISPLAY
#include <eez/modules/psu/gui/psu.h>
#endif

#include <eez/modules/psu/init.h>
#include <eez/gui/gui.h>
#include <eez/modules/mcu/display.h>
#include <eez/modules/mcu/touch.h>
#include <eez/scpi/scpi.h>

#include <eez/gui/document.h>

#if OPTION_DISPLAY
#include <eez/modules/psu/psu.h>

#include <eez/modules/psu/gui/page_ch_settings_adv.h>
#include <eez/modules/psu/gui/page_ch_settings_protection.h>
#include <eez/modules/psu/gui/page_ch_settings_trigger.h>
#include <eez/modules/psu/gui/page_event_queue.h>
#include <eez/modules/psu/gui/page_self_test_result.h>
#include <eez/modules/psu/gui/page_sys_settings.h>
#include <eez/modules/psu/gui/page_user_profiles.h>
#include <eez/modules/psu/gui/file_manager.h>
using namespace eez::psu::gui;
#endif

#if OPTION_ETHERNET
#include <eez/modules/mcu/ethernet.h>
#endif

#include <eez/modules/dcpX05/channel.h>
#include <eez/modules/dcm220/channel.h>

namespace eez {

////////////////////////////////////////////////////////////////////////////////

ChannelInterface::ChannelInterface(int slotIndex_) 
    : slotIndex(slotIndex_) 
{
}

unsigned ChannelInterface::getRPol(int subchannelIndex) {
    return 0;
}

void ChannelInterface::setRemoteSense(int subchannelIndex, bool enable) {
}

void ChannelInterface::setRemoteProgramming(int subchannelIndex, bool enable) {
}

void ChannelInterface::setCurrentRange(int subchannelIndex) {
}

bool ChannelInterface::isVoltageBalanced(int subchannelIndex) {
	return false;
}

bool ChannelInterface::isCurrentBalanced(int subchannelIndex) {
	return false;
}

float ChannelInterface::getUSetUnbalanced(int subchannelIndex) {
    psu::Channel &channel = psu::Channel::getBySlotIndex(slotIndex, subchannelIndex);
    return channel.u.set;
}

float ChannelInterface::getISetUnbalanced(int subchannelIndex) {
    psu::Channel &channel = psu::Channel::getBySlotIndex(slotIndex, subchannelIndex);
    return channel.i.set;
}

void ChannelInterface::readAllRegisters(int subchannelIndex, uint8_t ioexpRegisters[], uint8_t adcRegisters[]) {
}

void ChannelInterface::onSpiIrq() {
}

#if defined(DEBUG) && defined(EEZ_PLATFORM_STM32)
int ChannelInterface::getIoExpBitDirection(int subchannelIndex, int io_bit) {
	return 0;
}

bool ChannelInterface::testIoExpBit(int subchannelIndex, int io_bit) {
	return false;
}

void ChannelInterface::changeIoExpBit(int subchannelIndex, int io_bit, bool set) {
}
#endif

////////////////////////////////////////////////////////////////////////////////

OnSystemStateChangedCallback g_onSystemStateChangedCallbacks[] = {
    // modules
#if OPTION_ETHERNET
    mcu::ethernet::onSystemStateChanged,
#endif    
    mcu::touch::onSystemStateChanged,

    // subsystems
#if OPTION_DISPLAY
    gui::onSystemStateChanged,
#endif
    scpi::onSystemStateChanged,
    mp::onSystemStateChanged,

    // applications
    psu::onSystemStateChanged,
};

int g_numOnSystemStateChangedCallbacks = sizeof(g_onSystemStateChangedCallbacks) / sizeof(OnSystemStateChangedCallback);

static ModuleInfo g_modules[] = {
    { 
        MODULE_TYPE_NONE,
        "None",
        0,
        1,
        nullptr
    },
    {
        MODULE_TYPE_DCP405,
        "DCP405",
        MODULE_REVISION_DCP405_R2B7,
        1,
        dcpX05::g_channelInterfaces
    },
    {
        MODULE_TYPE_DCP405B,
        "DCP405B",
        MODULE_REVISION_DCP405B_R2B7,
        1,
        dcpX05::g_channelInterfaces
    },
    {
        MODULE_TYPE_DCP505, 
        "DCP505",
        MODULE_REVISION_DCP505_R1B3,
        1,
        dcpX05::g_channelInterfaces
    },
    {
        MODULE_TYPE_DCM220, 
        "DCM220",
        MODULE_REVISION_DCM220_R2B4,
        2,
        dcm220::g_channelInterfaces
    }
};

ModuleInfo *getModuleInfo(uint16_t moduleType) {
    for (size_t i = 0; i < sizeof(g_modules) / sizeof(ModuleInfo); i++) {
        if (g_modules[i].moduleType == moduleType) {
            return &g_modules[i];
        }
    }
    return &g_modules[0];
}

SlotInfo g_slots[NUM_SLOTS] = {
    {
        &g_modules[0]
    },
    {
        &g_modules[0]
    },
    {
        &g_modules[0]
    }
};

#if OPTION_DISPLAY

namespace gui {

static SelfTestResultPage g_SelfTestResultPage;
static EventQueuePage g_EventQueuePage;
static ChSettingsOvpProtectionPage g_ChSettingsOvpProtectionPage;
static ChSettingsOcpProtectionPage g_ChSettingsOcpProtectionPage;
static ChSettingsOppProtectionPage g_ChSettingsOppProtectionPage;
static ChSettingsOtpProtectionPage g_ChSettingsOtpProtectionPage;
static ChSettingsAdvOptionsPage g_ChSettingsAdvOptionPage;
static ChSettingsAdvRangesPage g_ChSettingsAdvRangesPage;
static ChSettingsAdvViewPage g_ChSettingsAdvViewPage;
static ChSettingsTriggerPage g_ChSettingsTriggerPage;
static ChSettingsListsPage g_ChSettingsListsPage;
static SysSettingsDateTimePage g_SysSettingsDateTimePage;
#if OPTION_ETHERNET
static SysSettingsEthernetPage g_SysSettingsEthernetPage;
static SysSettingsEthernetStaticPage g_SysSettingsEthernetStaticPage;
static SysSettingsMqttPage g_SysSettingsMqttPage;
#endif
static SysSettingsProtectionsPage g_SysSettingsProtectionsPage;
static SysSettingsTriggerPage g_SysSettingsTriggerPage;
static SysSettingsIOPinsPage g_SysSettingsIOPinsPage;
static SysSettingsTemperaturePage g_SysSettingsTemperaturePage;
static SysSettingsSoundPage g_SysSettingsSoundPage;
#if OPTION_ENCODER
static SysSettingsEncoderPage g_SysSettingsEncoderPage;
#endif
static SysSettingsSerialPage g_SysSettingsSerialPage;
static SysSettingsTrackingPage g_sysSettingsTrackingPage;
static SysSettingsCouplingPage g_sysSettingsCouplingPage;
static UserProfilesPage g_UserProfilesPage;
static file_manager::FileBrowserPage g_FileBrowserPage;

Page *getPageFromId(int pageId) {
    Page *page = nullptr;

    switch (pageId) {
    case PAGE_ID_SELF_TEST_RESULT:
        page = &g_SelfTestResultPage;
        break;
    case PAGE_ID_EVENT_QUEUE:
        page = &g_EventQueuePage;
        break;
    case PAGE_ID_CH_SETTINGS_PROT_OVP:
        page = &g_ChSettingsOvpProtectionPage;
        break;
    case PAGE_ID_CH_SETTINGS_PROT_OCP:
        page = &g_ChSettingsOcpProtectionPage;
        break;
    case PAGE_ID_CH_SETTINGS_PROT_OPP:
        page = &g_ChSettingsOppProtectionPage;
        break;
    case PAGE_ID_CH_SETTINGS_PROT_OTP:
        page = &g_ChSettingsOtpProtectionPage;
        break;
    case PAGE_ID_CH_SETTINGS_ADV_OPTIONS:
        page = &g_ChSettingsAdvOptionPage;
        break;
    case PAGE_ID_CH_SETTINGS_ADV_RANGES:
        page = &g_ChSettingsAdvRangesPage;
        break;
    case PAGE_ID_SYS_SETTINGS_TRACKING:
        page = &g_sysSettingsTrackingPage;
        break;
    case PAGE_ID_SYS_SETTINGS_COUPLING:
        page = &g_sysSettingsCouplingPage;
        break;
    case PAGE_ID_CH_SETTINGS_ADV_VIEW:
        page = &g_ChSettingsAdvViewPage;
        break;
    case PAGE_ID_CH_SETTINGS_TRIGGER:
        page = &g_ChSettingsTriggerPage;
        break;
    case PAGE_ID_CH_SETTINGS_LISTS:
        page = &g_ChSettingsListsPage;
        break;
    case PAGE_ID_SYS_SETTINGS_DATE_TIME:
        page = &g_SysSettingsDateTimePage;
        break;
#if OPTION_ETHERNET
    case PAGE_ID_SYS_SETTINGS_ETHERNET:
        page = &g_SysSettingsEthernetPage;
        break;
    case PAGE_ID_SYS_SETTINGS_ETHERNET_STATIC:
        page = &g_SysSettingsEthernetStaticPage;
        break;
    case PAGE_ID_SYS_SETTINGS_MQTT:
        page = &g_SysSettingsMqttPage;
        break;
#endif
    case PAGE_ID_SYS_SETTINGS_PROTECTIONS:
        page = &g_SysSettingsProtectionsPage;
        break;
    case PAGE_ID_SYS_SETTINGS_TRIGGER:
        page = &g_SysSettingsTriggerPage;
        break;
    case PAGE_ID_SYS_SETTINGS_IO:
        page = &g_SysSettingsIOPinsPage;
        break;
    case PAGE_ID_SYS_SETTINGS_TEMPERATURE:
        page = &g_SysSettingsTemperaturePage;
        break;
    case PAGE_ID_SYS_SETTINGS_SOUND:
        page = &g_SysSettingsSoundPage;
        break;
#if OPTION_ENCODER
    case PAGE_ID_SYS_SETTINGS_ENCODER:
        page = &g_SysSettingsEncoderPage;
        break;
#endif
    case PAGE_ID_SYS_SETTINGS_SERIAL:
        page = &g_SysSettingsSerialPage;
        break;
    case PAGE_ID_USER_PROFILES:
    case PAGE_ID_USER_PROFILE_0_SETTINGS:
    case PAGE_ID_USER_PROFILE_SETTINGS:
        page = &g_UserProfilesPage;
        break;
    case PAGE_ID_FILE_BROWSER:
        page = &g_FileBrowserPage;
        break;
    }

    if (page) {
        page->pageAlloc();
    }

    return page;
}

} // namespace gui

#endif

} // namespace eez
