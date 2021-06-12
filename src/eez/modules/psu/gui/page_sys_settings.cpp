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

#include <math.h>
#include <string.h>
#include <stdio.h>

#include <eez/firmware.h>
#include <eez/util.h>

#include <eez/modules/psu/psu.h>
#include <eez/modules/psu/channel_dispatcher.h>
#include <eez/modules/psu/ethernet.h>
#include <eez/modules/psu/persist_conf.h>
#include <eez/modules/psu/profile.h>
#include <eez/modules/psu/temperature.h>
#if OPTION_ETHERNET
#include <eez/modules/psu/ntp.h>
#endif

#include <eez/modules/psu/gui/psu.h>
#include <eez/modules/psu/gui/keypad.h>
#include <eez/modules/psu/gui/page_sys_settings.h>

#include <eez/modules/aux_ps/fan.h>

using namespace eez::psu::gui;

namespace eez {
namespace psu {
namespace gui {

////////////////////////////////////////////////////////////////////////////////

void SysSettingsDateTimePage::pageAlloc() {
#if OPTION_ETHERNET
    ntpEnabled = origNtpEnabled = persist_conf::isEthernetEnabled() && persist_conf::isNtpEnabled();
    stringCopy(ntpServer, sizeof(ntpServer), persist_conf::devConf.ntpServer);
    stringCopy(origNtpServer, sizeof(origNtpServer), persist_conf::devConf.ntpServer);
    ntpRefreshFrequency = origNtpRefreshFrequency = persist_conf::devConf.ntpRefreshFrequency;
#else
    ntpEnabled = origNtpEnabled = false;
    stringCopy(ntpServer, sizeof(ntpServer), "");
    stringCopy(origNtpServer, sizeof(origNtpServer), "");
#endif
    dateTimeModified = false;
    timeZone = origTimeZone = persist_conf::devConf.timeZone;
    dstRule = origDstRule = (datetime::DstRule)persist_conf::devConf.dstRule;
    dateTimeFormat = origDateTimeFormat = (datetime::Format)persist_conf::devConf.dateTimeFormat;
    powerLineFrequency = origPowerLineFrequency = persist_conf::getPowerLineFrequency();
}

void SysSettingsDateTimePage::toggleNtp() {
#if OPTION_ETHERNET
    ntpEnabled = !ntpEnabled;
#endif
}

void SysSettingsDateTimePage::editNtpServer() {
#if OPTION_ETHERNET
    Keypad::startPush(0, ntpServer, 0, 32, false, onSetNtpServer, popPage);
#endif
}

#if OPTION_ETHERNET
void SysSettingsDateTimePage::onSetNtpServer(char *value) {
    SysSettingsDateTimePage *page = (SysSettingsDateTimePage *)getPage(PAGE_ID_SYS_SETTINGS_DATE_TIME);
    stringCopy(page->ntpServer, sizeof(page->ntpServer), value);

    popPage();
}
#endif

void SysSettingsDateTimePage::edit() {
    const Widget *widget = getFoundWidgetAtDown().widget;
    int id = widget->data;

    NumericKeypadOptions options;

    const char *label = 0;

    Value value;

    if (id == DATA_ID_DATE_TIME_YEAR) {
        label = "Year (2016-2036): ";
        options.min = 2017;
        options.max = 2036;
        options.def = 2017;
        value = Value((int)dateTime.year);
    } else if (id == DATA_ID_DATE_TIME_MONTH) {
        label = "Month (1-12): ";
        options.min = 1;
        options.max = 12;
        options.def = 1;
        value = Value((int)dateTime.month);
    } else if (id == DATA_ID_DATE_TIME_DAY) {
        label = "Day (1-31): ";
        options.min = 1;
        options.max = 31;
        options.def = 1;
        value = Value((int)dateTime.day);
    } else if (id == DATA_ID_DATE_TIME_HOUR) {
        if (dateTimeFormat == datetime::FORMAT_DMY_24 || dateTimeFormat == datetime::FORMAT_MDY_24) {
            label = "Hour (0-23): ";
            options.min = 0;
            options.max = 23;
            options.def = 12;
            value = Value((int)dateTime.hour);
        } else {
            label = "Hour (1-12): ";
            options.min = 1;
            options.max = 12;
            options.def = 12;

            int hour = dateTime.hour;
            bool am;
            datetime::convertTime24to12(hour, am);

            value = Value(hour);
        }
    } else if (id == DATA_ID_DATE_TIME_MINUTE) {
        label = "Minute (0-59): ";
        options.min = 0;
        options.max = 59;
        options.def = 0;
        value = Value((int)dateTime.minute);
    } else if (id == DATA_ID_DATE_TIME_SECOND) {
        label = "Second (0-59): ";
        options.min = 0;
        options.max = 59;
        options.def = 0;
        value = Value((int)dateTime.second);
    } else if (id == DATA_ID_DATE_TIME_TIME_ZONE) {
        label = "Time zone: ";
        options.min = -12.00;
        options.max = 14.00;
        options.def = 0;
        options.flags.dotButtonEnabled = true;
        options.flags.signButtonEnabled = true;
        value = Value(timeZone, VALUE_TYPE_TIME_ZONE);
    }

    if (label) {
        editDataId = id;
        NumericKeypad::start(label, value, options, onSetValue, 0, 0);
    }
}

void SysSettingsDateTimePage::onDstRuleSet(uint16_t value) {
    popPage();
    SysSettingsDateTimePage *page = (SysSettingsDateTimePage *)getActivePage();
    page->dstRule = (datetime::DstRule)value;
}

void SysSettingsDateTimePage::selectDstRule() {
    pushSelectFromEnumPage(ENUM_DEFINITION_DST_RULE, dstRule, 0, onDstRuleSet);
}

void SysSettingsDateTimePage::onDateTimeFormatSet(uint16_t value) {
    popPage();
    SysSettingsDateTimePage *page = (SysSettingsDateTimePage *)getActivePage();
    page->dateTimeFormat = (datetime::Format)value;
}

void SysSettingsDateTimePage::selectFormat() {
    pushSelectFromEnumPage(ENUM_DEFINITION_DATE_TIME_FORMAT, dateTimeFormat, 0, onDateTimeFormatSet);
}

void SysSettingsDateTimePage::toggleAmPm() {
    int hour = dateTime.hour;
    bool am;
    datetime::convertTime24to12(hour, am);
    am = !am;
    datetime::convertTime12to24(hour, am);
    dateTime.hour = uint8_t(hour);
    dateTimeModified = true;
}

void SysSettingsDateTimePage::setValue(float value) {
    if (editDataId == DATA_ID_DATE_TIME_YEAR) {
        dateTime.year = uint16_t(value);
        dateTimeModified = true;
    } else if (editDataId == DATA_ID_DATE_TIME_MONTH) {
        dateTime.month = uint8_t(value);
        dateTimeModified = true;
    } else if (editDataId == DATA_ID_DATE_TIME_DAY) {
        dateTime.day = uint8_t(value);
        dateTimeModified = true;
    } else if (editDataId == DATA_ID_DATE_TIME_HOUR) {
        if (dateTimeFormat == datetime::FORMAT_DMY_24 || dateTimeFormat == datetime::FORMAT_MDY_24) {
            dateTime.hour = uint8_t(value);
        } else {
            int hour = dateTime.hour;
            bool am;
            datetime::convertTime24to12(hour, am);
            hour = (int)value;
            datetime::convertTime12to24(hour, am);
            dateTime.hour = uint8_t(hour);
        }
        dateTimeModified = true;
    } else if (editDataId == DATA_ID_DATE_TIME_MINUTE) {
        dateTime.minute = uint8_t(value);
        dateTimeModified = true;
    } else if (editDataId == DATA_ID_DATE_TIME_SECOND) {
        dateTime.second = uint8_t(value);
        dateTimeModified = true;
    } else if (editDataId == DATA_ID_DATE_TIME_TIME_ZONE) {
        timeZone = int16_t(roundf(value * 100));
    }
}

int SysSettingsDateTimePage::getDirty() {
    if (ntpEnabled && !ntpServer[0]) {
        return 0;
    }

    if (ntpEnabled != origNtpEnabled) {
        return 1;
    }

    if (ntpEnabled) {
        if (ntpServer[0] && strcmp(ntpServer, origNtpServer)) {
            return 1;
        }
        if (ntpRefreshFrequency != origNtpRefreshFrequency) {
            return 1;
        }
    } else {
        if (dateTimeModified) {
            return 1;
        }
    }

    return (
        timeZone != origTimeZone || 
        dstRule != origDstRule || 
        dateTimeFormat != origDateTimeFormat || 
        powerLineFrequency != origPowerLineFrequency
    ) ? 1 : 0;
}

#if OPTION_ETHERNET

void SysSettingsDateTimePage::checkTestNtpServerStatus() {
    bool testResult;
    if (ntp::isTestNtpServerDone(testResult)) {
        popPage();

        if (testResult) {
            SysSettingsDateTimePage *page = (SysSettingsDateTimePage *)getActivePage();
            page->doSet();
        } else {
            errorMessage("Unable to connect to NTP server!");
        }
    }
}

void SysSettingsDateTimePage::testNtpServer() {
    ntp::testNtpServer(ntpServer);
    showAsyncOperationInProgress("Testing NTP server...", checkTestNtpServerStatus);
}
#endif

void SysSettingsDateTimePage::set() {
    if (getDirty()) {
#if OPTION_ETHERNET
        if (ntpEnabled && strcmp(ntpServer, origNtpServer)) {
            testNtpServer();
            return;
        }
#endif
        doSet();
    }
}

void SysSettingsDateTimePage::doSet() {
    if (!ntpEnabled) {
        if (!datetime::isValidDate(uint8_t(dateTime.year - 2000), dateTime.month, dateTime.day)) {
            errorMessage("Invalid date!");
            return;
        }

        if (!datetime::isValidTime(dateTime.hour, dateTime.minute, dateTime.second)) {
            errorMessage("Invalid time!");
            popPage();
            return;
        }
    }

#if OPTION_ETHERNET
    if (ntpEnabled != origNtpEnabled || strcmp(ntpServer, origNtpServer) || ntpRefreshFrequency != origNtpRefreshFrequency) {
        persist_conf::setNtpSettings(ntpEnabled, ntpServer, ntpRefreshFrequency);
    }
#endif

    if (dstRule != origDstRule) {
        persist_conf::setDstRule(dstRule);
    }

    if (dateTimeFormat != origDateTimeFormat) {
        persist_conf::setDateTimeFormat(dateTimeFormat);
    }

    if (!ntpEnabled && dateTimeModified) {
        datetime::setDateTime(uint8_t(dateTime.year - 2000), dateTime.month, dateTime.day,
                              dateTime.hour, dateTime.minute, dateTime.second, true, 2);
    }

    if (timeZone != origTimeZone) {
        persist_conf::setTimeZone(timeZone);
    }

#if OPTION_ETHERNET
    ntp::reset();
#endif

    if (ntpEnabled || !dateTimeModified) {
        event_queue::pushEvent(event_queue::EVENT_INFO_SYSTEM_DATE_TIME_CHANGED);
    }

	if (powerLineFrequency != origPowerLineFrequency) {
		persist_conf::setPowerLineFrequency(powerLineFrequency);
	}

    popPage();
    
    return;
}

////////////////////////////////////////////////////////////////////////////////

#if OPTION_ETHERNET

void SysSettingsEthernetPage::pageAlloc() {
    m_enabledOrig = m_enabled = persist_conf::isEthernetEnabled();
    m_dhcpEnabledOrig = m_dhcpEnabled = persist_conf::isEthernetDhcpEnabled();
    m_ipAddressOrig = m_ipAddress = persist_conf::devConf.ethernetIpAddress;
    m_dnsOrig = m_dns = persist_conf::devConf.ethernetDns;
    m_gatewayOrig = m_gateway = persist_conf::devConf.ethernetGateway;
    m_subnetMaskOrig = m_subnetMask = persist_conf::devConf.ethernetSubnetMask;
    m_scpiPortOrig = m_scpiPort = persist_conf::devConf.ethernetScpiPort;
    memcpy(m_macAddressOrig, persist_conf::devConf.ethernetMacAddress, 6);
    memcpy(m_macAddress, persist_conf::devConf.ethernetMacAddress, 6);
    stringCopy(m_hostNameOrig, sizeof(m_hostNameOrig), persist_conf::devConf.ethernetHostName);
    stringCopy(m_hostName, sizeof(m_hostName), persist_conf::devConf.ethernetHostName);
}

void SysSettingsEthernetPage::toggle() {
    m_enabled = !m_enabled;
}

void SysSettingsEthernetPage::toggleDhcp() {
    m_dhcpEnabled = !m_dhcpEnabled;
}

void SysSettingsEthernetPage::editStaticAddress() {
    pushPage(PAGE_ID_SYS_SETTINGS_ETHERNET_STATIC);
}

void SysSettingsEthernetPage::onSetScpiPort(float value) {
    popPage();
    SysSettingsEthernetPage *page = (SysSettingsEthernetPage *)getActivePage();
    page->m_scpiPort = (uint16_t)value;
}

void SysSettingsEthernetPage::editScpiPort() {
    NumericKeypadOptions options;

    options.min = 0;
    options.max = 65535;
    options.def = TCP_PORT;

    options.enableDefButton();

    NumericKeypad::start(0, Value((int)m_scpiPort, VALUE_TYPE_PORT), options, onSetScpiPort, 0, 0);
}

void SysSettingsEthernetPage::onSetMacAddress(char *value) {
    uint8_t macAddress[6];
    if (!parseMacAddress(value, strlen(value), macAddress)) {
        errorMessage("Invalid MAC address!");
        return;
    }

    SysSettingsEthernetPage *page = (SysSettingsEthernetPage *)getPage(PAGE_ID_SYS_SETTINGS_ETHERNET);
    memcpy(page->m_macAddress, macAddress, 6);

    popPage();
}

void SysSettingsEthernetPage::editMacAddress() {
    char macAddressStr[18];
    macAddressToString(m_macAddress, macAddressStr);
    Keypad::startPush(0, macAddressStr, 0, 32, false, onSetMacAddress, popPage);
}

int SysSettingsEthernetPage::getDirty() {
    return 
        m_enabledOrig != m_enabled ||
        m_dhcpEnabledOrig != m_dhcpEnabled ||
        m_ipAddressOrig != m_ipAddress ||
        m_dnsOrig != m_dns ||
        m_gatewayOrig != m_gateway ||
        m_subnetMaskOrig != m_subnetMask ||
        m_scpiPortOrig != m_scpiPort ||
        memcmp(m_macAddress, m_macAddressOrig, 6) != 0 ||
        strcmp(m_hostName, m_hostNameOrig) != 0;
}

void SysSettingsEthernetPage::applyAndRestart() {
    SysSettingsEthernetPage *page = (SysSettingsEthernetPage *)getActivePage();

    persist_conf::setEthernetSettings(
        page->m_enabled, 
        page->m_dhcpEnabled, 
        page->m_ipAddress, 
        page->m_dns, 
        page->m_gateway, 
        page->m_subnetMask, 
        page->m_scpiPort, 
        page->m_macAddress, 
        page->m_hostName
    );

    eez::restart();
}

void SysSettingsEthernetPage::set() {
    if (getDirty()) {
        yesNoDialog(PAGE_ID_INFO_RESTART, "", applyAndRestart, popPage, popPage);
    }
}

////////////////////////////////////////////////////////////////////////////////

void SysSettingsEthernetStaticPage::pageAlloc() {
    SysSettingsEthernetPage *page = (SysSettingsEthernetPage *)getPage(PAGE_ID_SYS_SETTINGS_ETHERNET);

    m_ipAddressOrig = m_ipAddress = page->m_ipAddress;
    m_dnsOrig = m_dns = page->m_dns;
    m_gatewayOrig = m_gateway = page->m_gateway;
    m_subnetMaskOrig = m_subnetMask = page->m_subnetMask;
}

void SysSettingsEthernetStaticPage::onAddressSet(uint32_t address) {
    popPage();
    SysSettingsEthernetStaticPage *page = (SysSettingsEthernetStaticPage *)getActivePage();
    *page->m_editAddress = address;
}

void SysSettingsEthernetStaticPage::editAddress(uint32_t &address) {
    m_editAddress = &address;
    NumericKeypadOptions options;
    NumericKeypad::start(0, Value((uint32_t)address, VALUE_TYPE_IP_ADDRESS), options, 0, onAddressSet, 0);
}

void SysSettingsEthernetStaticPage::editIpAddress() {
    editAddress(m_ipAddress);
}

void SysSettingsEthernetStaticPage::editDns() {
    editAddress(m_dns);
}

void SysSettingsEthernetStaticPage::editGateway() {
    editAddress(m_gateway);
}

void SysSettingsEthernetStaticPage::editSubnetMask() {
    editAddress(m_subnetMask);
}

int SysSettingsEthernetStaticPage::getDirty() {
    return m_ipAddressOrig != m_ipAddress || m_dnsOrig != m_dns || m_gatewayOrig != m_gateway ||
           m_subnetMaskOrig != m_subnetMask;
}

void SysSettingsEthernetStaticPage::set() {
    if (getDirty()) {
        SysSettingsEthernetPage *page = (SysSettingsEthernetPage *)getPage(PAGE_ID_SYS_SETTINGS_ETHERNET);

        page->m_ipAddress = m_ipAddress;
        page->m_dns = m_dns;
        page->m_gateway = m_gateway;
        page->m_subnetMask = m_subnetMask;

        popPage();
    }
}

////////////////////////////////////////////////////////////////////////////////

void SysSettingsMqttPage::pageAlloc() {
    m_enabledOrig = m_enabled = persist_conf::devConf.mqttEnabled;
    stringCopy(m_hostOrig, sizeof(m_hostOrig), persist_conf::devConf.mqttHost);
    stringCopy(m_host, sizeof(m_host), persist_conf::devConf.mqttHost);
    m_portOrig = m_port = persist_conf::devConf.mqttPort;
    stringCopy(m_usernameOrig, sizeof(m_usernameOrig), persist_conf::devConf.mqttUsername);
    stringCopy(m_username, sizeof(m_username), persist_conf::devConf.mqttUsername);
    stringCopy(m_passwordOrig, sizeof(m_passwordOrig), persist_conf::devConf.mqttPassword);
    stringCopy(m_password, sizeof(m_password), persist_conf::devConf.mqttPassword);
    m_periodOrig = m_period = persist_conf::devConf.mqttPeriod;
}

void SysSettingsMqttPage::toggle() {
    m_enabled = !m_enabled;
}

int SysSettingsMqttPage::getDirty() {
    return m_enabledOrig != m_enabled ||
        strcmp(m_hostOrig, m_host) != 0 ||
        m_portOrig != m_port ||
        strcmp(m_usernameOrig, m_username) != 0 ||
        strcmp(m_passwordOrig, m_password) != 0 ||
        m_periodOrig != m_period;
}

void SysSettingsMqttPage::set() {
    if (getDirty()) {
        if (persist_conf::setMqttSettings(m_enabled, m_host, m_port, m_username, m_password, m_period)) {
            popPage();
        }
    }
}

#endif

////////////////////////////////////////////////////////////////////////////////

void SysSettingsProtectionsPage::toggleOutputProtectionCouple() {
    if (persist_conf::isOutputProtectionCoupleEnabled()) {
        persist_conf::enableOutputProtectionCouple(false);
    } else {
        persist_conf::enableOutputProtectionCouple(true);
    }
}

void SysSettingsProtectionsPage::toggleShutdownWhenProtectionTripped() {
    if (persist_conf::isShutdownWhenProtectionTrippedEnabled()) {
        persist_conf::enableShutdownWhenProtectionTripped(false);
    } else {
        persist_conf::enableShutdownWhenProtectionTripped(true);
    }
}

void SysSettingsProtectionsPage::toggleForceDisablingAllOutputsOnPowerUp() {
    if (persist_conf::isForceDisablingAllOutputsOnPowerUpEnabled()) {
        persist_conf::enableForceDisablingAllOutputsOnPowerUp(false);
    } else {
        persist_conf::enableForceDisablingAllOutputsOnPowerUp(true);
    }
}

////////////////////////////////////////////////////////////////////////////////

void SysSettingsTemperaturePage::pageAlloc() {
    origState = state = temperature::sensors[temp_sensor::AUX].prot_conf.state ? 1 : 0;

    origLevel = level = MakeValue(temperature::sensors[temp_sensor::AUX].prot_conf.level, UNIT_CELSIUS);
    minLevel = OTP_AUX_MIN_LEVEL;
    maxLevel = OTP_AUX_MAX_LEVEL;
    defLevel = OTP_AUX_DEFAULT_LEVEL;

    origDelay = delay = MakeValue(temperature::sensors[temp_sensor::AUX].prot_conf.delay, UNIT_SECOND);
    minDelay = OTP_AUX_MIN_DELAY;
    maxDelay = OTP_AUX_MAX_DELAY;
    defaultDelay = OTP_CH_DEFAULT_DELAY;

    origFanMode = fanMode = persist_conf::devConf.fanMode;

    if (fanMode == FAN_MODE_MANUAL) {
        origFanSpeedPercentage = fanSpeedPercentage = MakeValue(1.0f * persist_conf::devConf.fanSpeedPercentage, UNIT_PERCENT);
        fanSpeedPWM = persist_conf::devConf.fanSpeedPWM;
    } else {
        origFanSpeedPercentage = fanSpeedPercentage = MakeValue(100.0f, UNIT_PERCENT);
        fanSpeedPWM = 255;
    }

    fanPWMMeasuringInProgress = false;
}

int SysSettingsTemperaturePage::getDirty() {
    return (origState != state || origLevel != level || origDelay != delay || origFanMode != fanMode || origFanSpeedPercentage != fanSpeedPercentage) ? 1 : 0;
}

void SysSettingsTemperaturePage::set() {
    if (getDirty()) {
        setParams();
    }
}

void SysSettingsTemperaturePage::toggleState() {
    state = state ? 0 : 1;
}

void SysSettingsTemperaturePage::onLevelSet(float value) {
    popPage();
    SysSettingsTemperaturePage *page = (SysSettingsTemperaturePage *)getActivePage();
    page->level = MakeValue(value, page->level.getUnit());
}

void SysSettingsTemperaturePage::editLevel() {
    NumericKeypadOptions options;

    options.editValueUnit = level.getUnit();

    options.min = minLevel;
    options.max = maxLevel;
    options.def = defLevel;

    options.enableMaxButton();
    options.enableDefButton();
    options.flags.signButtonEnabled = true;
    options.flags.dotButtonEnabled = true;

    NumericKeypad::start(0, level, options, onLevelSet, 0, 0);
}

void SysSettingsTemperaturePage::onDelaySet(float value) {
    popPage();
    SysSettingsTemperaturePage *page = (SysSettingsTemperaturePage *)getActivePage();
    page->delay = MakeValue(value, page->delay.getUnit());
}

void SysSettingsTemperaturePage::editDelay() {
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

void SysSettingsTemperaturePage::toggleFanMode() {
    fanMode = fanMode ? 0 : 1;
}

void SysSettingsTemperaturePage::onFanSpeedSet(float value) {
    popPage();
    SysSettingsTemperaturePage *page = (SysSettingsTemperaturePage *)getActivePage();
    if (value != page->fanSpeedPercentage.getFloat()) {
        page->fanSpeedPercentage = MakeValue(value, UNIT_PERCENT);
        if ((uint8_t)value == 0) {
            page->fanSpeedPWM = 0;
        } else if ((uint8_t)value == 100) {
            page->fanSpeedPWM = FAN_MAX_PWM;
        } else {
            page->fanSpeedPWM = FAN_MAX_PWM;
            page->fanPWMMeasuringInProgress = true;
            showAsyncOperationInProgress("Calibrating PWM...", isFanPWMMeasuringDone);
        }
    }
}

void SysSettingsTemperaturePage::isFanPWMMeasuringDone() {
    auto page = (psu::gui::SysSettingsTemperaturePage *)psu::gui::getPage(PAGE_ID_SYS_SETTINGS_TEMPERATURE);
    if (!page->fanPWMMeasuringInProgress) {
        hideAsyncOperationInProgress();
    }
}

void SysSettingsTemperaturePage::editFanSpeed() {
    NumericKeypadOptions options;

    options.editValueUnit = UNIT_PERCENT;

    options.min = 0;
    options.max = 100;
    options.def = 100;

    options.enableMaxButton();
    options.enableMinButton();
    options.flags.signButtonEnabled = false;
    options.flags.dotButtonEnabled = false;

    options.encoderPrecision = 1.0f;

    NumericKeypad::start(0, fanSpeedPercentage, options, onFanSpeedSet, 0, 0);
}

void SysSettingsTemperaturePage::setParams() {
    temperature::sensors[temp_sensor::AUX].prot_conf.state = state ? true : false;
    temperature::sensors[temp_sensor::AUX].prot_conf.level = level.getFloat();
    temperature::sensors[temp_sensor::AUX].prot_conf.delay = delay.getFloat();

    persist_conf::setFanSettings(fanMode, (uint8_t)roundf(fanSpeedPercentage.getFloat()), fanSpeedPWM);

    popPage();
    infoMessage("Aux temp. protection changed!");
}

void SysSettingsTemperaturePage::clear() {
    temperature::sensors[temp_sensor::AUX].clearProtection();
    popPage();
    infoMessage("Cleared!");
}

////////////////////////////////////////////////////////////////////////////////

void SysSettingsSoundPage::toggleSound() {
    if (persist_conf::isSoundEnabled()) {
        persist_conf::enableSound(false);
    } else {
        persist_conf::enableSound(true);
    }
}

void SysSettingsSoundPage::toggleClickSound() {
    if (persist_conf::isClickSoundEnabled()) {
        persist_conf::enableClickSound(false);
    } else {
        persist_conf::enableClickSound(true);
    }
}

#if OPTION_ENCODER

////////////////////////////////////////////////////////////////////////////////

void SysSettingsEncoderPage::pageAlloc() {
    origConfirmationMode = confirmationMode = persist_conf::devConf.encoderConfirmationMode;
    origMovingSpeedDown = movingSpeedDown = persist_conf::devConf.encoderMovingSpeedDown;
    origMovingSpeedUp = movingSpeedUp = persist_conf::devConf.encoderMovingSpeedUp;
}

void SysSettingsEncoderPage::toggleConfirmationMode() {
    confirmationMode = confirmationMode ? 0 : 1;
}

int SysSettingsEncoderPage::getDirty() {
    return origConfirmationMode != confirmationMode || origMovingSpeedDown != movingSpeedDown || origMovingSpeedUp != movingSpeedUp;
}

void SysSettingsEncoderPage::set() {
    if (getDirty()) {
        persist_conf::setEncoderSettings(confirmationMode, movingSpeedDown, movingSpeedUp);
        popPage();
        infoMessage("Encoder settings changed!");
    }
}

#endif

////////////////////////////////////////////////////////////////////////////////

void SysSettingsTriggerPage::pageAlloc() {
    m_sourceOrig = m_source = trigger::g_triggerSource;
    m_delayOrig = m_delay = trigger::g_triggerDelay;
    m_initiateContinuouslyOrig = m_initiateContinuously = trigger::g_triggerContinuousInitializationEnabled;
}

void SysSettingsTriggerPage::onTriggerSourceSet(uint16_t value) {
    popPage();
    SysSettingsTriggerPage *page = (SysSettingsTriggerPage *)getActivePage();
    page->m_source = (trigger::Source)value;
}

void SysSettingsTriggerPage::selectSource() {
    pushSelectFromEnumPage(ENUM_DEFINITION_TRIGGER_SOURCE, m_source, 0, onTriggerSourceSet);
}

void SysSettingsTriggerPage::onDelaySet(float value) {
    popPage();
    SysSettingsTriggerPage *page = (SysSettingsTriggerPage *)getActivePage();
    page->m_delay = value;
}

void SysSettingsTriggerPage::editDelay() {
    NumericKeypadOptions options;

    options.editValueUnit = UNIT_SECOND;

    options.def = 0;
    options.min = 0;
    options.max = 3600;

    options.flags.signButtonEnabled = true;
    options.flags.dotButtonEnabled = true;

    NumericKeypad::start(0, MakeValue(trigger::g_triggerDelay, UNIT_SECOND), options, onDelaySet, 0, 0);
}

void SysSettingsTriggerPage::toggleInitiateContinuously() {
    m_initiateContinuously = !m_initiateContinuously;
}

int SysSettingsTriggerPage::getDirty() {
    return m_sourceOrig != m_source || m_delayOrig != m_delay ||
           m_initiateContinuouslyOrig != m_initiateContinuously;
}

void SysSettingsTriggerPage::set() {
    if (getDirty()) {
        trigger::setSource(m_source);
        trigger::setDelay(m_delay);
        trigger::enableInitiateContinuous(m_initiateContinuously);

        popPage();
        
        // infoMessage("Trigger settings saved!");
    }
}

////////////////////////////////////////////////////////////////////////////////

void SysSettingsIOPinsPage::pageAlloc() {
    for (int i = 0; i < NUM_IO_PINS; i++) {
        m_polarityOrig[i] = m_polarity[i] = (io_pins::Polarity)io_pins::g_ioPins[i].polarity;
        m_functionOrig[i] = m_function[i] = (io_pins::Function)io_pins::g_ioPins[i].function;
        if (i >= DOUT1) {
            g_pwmFrequencyOrig[i - DOUT1] = g_pwmFrequency[i - DOUT1] = io_pins::getPwmFrequency(i);
            g_pwmDutyOrig[i - DOUT1] = g_pwmDuty[i - DOUT1] = io_pins::getPwmDuty(i);

        }
    }

	m_uartMode = m_uartModeOrig = io_pins::g_uartMode;
}

void SysSettingsIOPinsPage::togglePolarity() {
    int i = getFoundWidgetAtDown().cursor;
    m_polarity[i] = m_polarity[i] == io_pins::POLARITY_NEGATIVE ? io_pins::POLARITY_POSITIVE : io_pins::POLARITY_NEGATIVE;
}

void SysSettingsIOPinsPage::onFunctionSet(uint16_t value) {
    popPage();

	SysSettingsIOPinsPage *page = (SysSettingsIOPinsPage *)getActivePage();

    auto pin = page->pinNumber;
    auto function = (io_pins::Function)value;

    if (page->m_function[pin] != function) {
		using namespace io_pins;

        if (pin == DIN1 && page->m_function[DIN1] == FUNCTION_UART) {
            page->m_function[DOUT1] = FUNCTION_NONE;
        }

        if (pin == DOUT1 && page->m_function[DOUT1] == FUNCTION_UART) {
            page->m_function[DIN1] = FUNCTION_NONE;
        }

        page->m_function[pin] = function;
        
        if (pin == DIN1 && function == FUNCTION_UART) {
            page->m_function[DOUT1] = FUNCTION_UART;
        }

        if (pin == DOUT1 && function == FUNCTION_UART) {
            page->m_function[DIN1] = FUNCTION_UART;
        }
        
        if (value == io_pins::FUNCTION_SYSTRIG) {
            int otherPin = pin == DIN1 ? DIN2 : DIN1;
            if (page->m_function[otherPin] == io_pins::FUNCTION_SYSTRIG) {
                page->m_function[otherPin] = io_pins::FUNCTION_NONE;
            }
        }
    }
}

void SysSettingsIOPinsPage::selectFunction() {
    pinNumber = getFoundWidgetAtDown().cursor;
    if (pinNumber == DIN1) {
        pushSelectFromEnumPage(ENUM_DEFINITION_IO_PINS_INPUT1_FUNCTION, m_function[pinNumber], 0, onFunctionSet);
    } else if (pinNumber == DIN2) {
		pushSelectFromEnumPage(ENUM_DEFINITION_IO_PINS_INPUT2_FUNCTION, m_function[pinNumber], 0, onFunctionSet);
	} else if (pinNumber == DOUT1) {
        pushSelectFromEnumPage(ENUM_DEFINITION_IO_PINS_OUTPUT1_FUNCTION, m_function[pinNumber], 0, onFunctionSet);
    } else {
        pushSelectFromEnumPage(ENUM_DEFINITION_IO_PINS_OUTPUT2_FUNCTION, m_function[pinNumber], 0, onFunctionSet);
    }
}

void SysSettingsIOPinsPage::setPwmFrequency(int pin, float frequency) {
    g_pwmFrequency[pin - DOUT1] = frequency;
}

float SysSettingsIOPinsPage::getPwmFrequency(int pin) {
    return g_pwmFrequency[pin - DOUT1];
}

void SysSettingsIOPinsPage::setPwmDuty(int pin, float duty) {
    g_pwmDuty[pin - DOUT1] = duty;
}

float SysSettingsIOPinsPage::getPwmDuty(int pin) {
    return g_pwmDuty[pin - DOUT1];
}

void SysSettingsIOPinsPage::onUartModeSet(uint16_t value) {
	popPage();

	SysSettingsIOPinsPage *page = (SysSettingsIOPinsPage *)getActivePage();
	page->m_uartMode = (uart::UartMode)value;
}

void SysSettingsIOPinsPage::selectUartMode() {
	pushSelectFromEnumPage(ENUM_DEFINITION_UART_MODE, m_uartMode, 0, onUartModeSet);
}

int SysSettingsIOPinsPage::getDirty() {
    for (int i = 0; i < NUM_IO_PINS; ++i) {
        if (m_polarityOrig[i] != m_polarity[i] || m_functionOrig[i] != m_function[i]) {
            return true;
        }
        if (i >= DOUT1) {
            if (g_pwmFrequencyOrig[i - DOUT1] != g_pwmFrequency[i - DOUT1] || g_pwmDutyOrig[i - DOUT1] != g_pwmDuty[i - DOUT1]) {
                return true;
            }
        }
    }
	if (m_uartMode != m_uartModeOrig) {
		return true;
	}
    return false;
}

void SysSettingsIOPinsPage::set() {
    if (getDirty()) {
		io_pins::g_uartMode = m_uartMode;
		
		for (int i = 0; i < NUM_IO_PINS; i++) {
            io_pins::setPinPolarity(i, m_polarity[i]);
            io_pins::setPinFunction(i, m_function[i]);
            if (i >= DOUT1) {
                if (g_pwmFrequencyOrig[i - DOUT1] != g_pwmFrequency[i - DOUT1]) {
                    io_pins::setPwmFrequency(i, g_pwmFrequency[i - DOUT1]);
                }
                if (g_pwmDutyOrig[i - DOUT1] != g_pwmDuty[i - DOUT1]) {
                    io_pins::setPwmDuty(i, g_pwmDuty[i - DOUT1]);
                }
            }
        }
        
		pageAlloc();
    }
}

////////////////////////////////////////////////////////////////////////////////

void SysSettingsTrackingPage::pageAlloc() {
    m_couplingType = m_couplingTypeOrig = channel_dispatcher::getCouplingType();

    m_trackingEnabled = 0;
    for (int i = 0; i < CH_NUM; i++) {
        if (Channel::get(i).flags.trackingEnabled) {
            m_trackingEnabled |= 1 << i;
        }
    }
    m_trackingEnabledOrig = m_trackingEnabled;
}

int SysSettingsTrackingPage::getDirty() {
    return (m_trackingEnabled != m_trackingEnabledOrig || m_couplingType != m_couplingTypeOrig) && getNumTrackingChannels() != 1;
}

void SysSettingsTrackingPage::set() {
    if (m_couplingType != m_couplingTypeOrig) {
        int err;
        if (channel_dispatcher::setCouplingType(m_couplingType, &err)) {
            showPage(PAGE_ID_MAIN);

            if (m_couplingType == channel_dispatcher::COUPLING_TYPE_NONE) {
                infoMessage("Uncoupled!");
            } else if (m_couplingType == channel_dispatcher::COUPLING_TYPE_PARALLEL) {
                infoMessage("Coupled in parallel!");
            } else if (m_couplingType == channel_dispatcher::COUPLING_TYPE_SERIES) {
                infoMessage("Coupled in series!");
            } else if (m_couplingType == channel_dispatcher::COUPLING_TYPE_SPLIT_RAILS) {
                infoMessage("Coupled in split rails!");
            } else {
                infoMessage("Coupled in common GND!");
            }
        } else {
            event_queue::pushEvent(err);
            return;
        }

        m_trackingEnabledOrig = 0;
        for (int i = 0; i < CH_NUM; i++) {
            if (Channel::get(i).flags.trackingEnabled) {
                m_trackingEnabledOrig |= 1 << i;
            }
        }
    }

    if (m_trackingEnabled != m_trackingEnabledOrig) {
        channel_dispatcher::setTrackingChannels(m_trackingEnabled);

        if (m_couplingType == m_couplingTypeOrig) {
            showPage(PAGE_ID_MAIN);

            if (getNumTrackingChannels() >= 2) {
                infoMessage("Tracking enabled!");
            } else {
                infoMessage("Tracking disabled!");
            }
        }
    }
}

int SysSettingsTrackingPage::getNumTrackingChannels() {
    int count = 0;
    for (int i = 0; i < CH_NUM; i++) {
        if (m_trackingEnabled & (1 << i)) {
            ++count;
        }
    }
    return count;
}

void SysSettingsTrackingPage::toggleChannelTracking(int channelIndex) {
    m_trackingEnabled ^= 1 << channelIndex;
}

void SysSettingsTrackingPage::untrackAll() {
    m_trackingEnabled = 0;
}

SysSettingsTrackingPage g_sysSettingsTrackingPage;

////////////////////////////////////////////////////////////////////////////////

void SysSettingsCouplingPage::pageAlloc() {
    m_couplingType = m_couplingTypeOrig = g_sysSettingsTrackingPage.m_couplingType;

    m_enableTrackingMode = m_enableTrackingModeOrig = 
        (m_couplingType != channel_dispatcher::COUPLING_TYPE_SPLIT_RAILS) ||
        ((g_sysSettingsTrackingPage.m_trackingEnabled & (1 << 0)) &&
        (g_sysSettingsTrackingPage.m_trackingEnabled & (1 << 1)));
}

int SysSettingsCouplingPage::getDirty() {
    return m_couplingType != m_couplingTypeOrig || m_enableTrackingMode != m_enableTrackingModeOrig;
}

void SysSettingsCouplingPage::set() {
    g_sysSettingsTrackingPage.m_couplingType = m_couplingType;

    if (m_couplingType == channel_dispatcher::COUPLING_TYPE_PARALLEL || m_couplingType == channel_dispatcher::COUPLING_TYPE_SERIES) {
            g_sysSettingsTrackingPage.m_trackingEnabled &= ~(1 << 0);
            g_sysSettingsTrackingPage.m_trackingEnabled &= ~(1 << 1);
    } else if (m_couplingType == channel_dispatcher::COUPLING_TYPE_SPLIT_RAILS) {
        if (m_enableTrackingMode) {
            // enable tracking for first two channels
            g_sysSettingsTrackingPage.m_trackingEnabled |= (1 << 0);
            g_sysSettingsTrackingPage.m_trackingEnabled |= (1 << 1);
        }
    }

    popPage();
}

////////////////////////////////////////////////////////////////////////////////

void SysSettingsRampAndDelayPage::pageAlloc() {
    for (int i = 0; i < CH_NUM; i++) {
        auto &channel = Channel::get(i);

        rampState[i] = rampStateOrig[i] = channel_dispatcher::getVoltageTriggerMode(channel) == TRIGGER_MODE_STEP || channel_dispatcher::getCurrentTriggerMode(channel) == TRIGGER_MODE_STEP;
        
        triggerVoltage[i] = triggerVoltageOrig[i] = channel_dispatcher::getTriggerVoltage(channel);
        triggerCurrent[i] = triggerCurrentOrig[i] = channel_dispatcher::getTriggerCurrent(channel);
        
        voltageRampDuration[i] = voltageRampDurationOrig[i] = channel.u.rampDuration;
        currentRampDuration[i] = currentRampDurationOrig[i] = channel.i.rampDuration;

        outputDelayDuration[i] = outputDelayDurationOrig[i] = channel.outputDelayDuration;
    }

    version = 1;
    startChannel = 0;
}

int SysSettingsRampAndDelayPage::getDirty() {
    for (int i = 0; i < CH_NUM; i++) {
        if (rampState[i] != rampStateOrig[i]) {
            return 1;
        }

        if (triggerVoltage[i] != triggerVoltageOrig[i] || triggerCurrent[i] != triggerCurrentOrig[i]) {
            return 1;
        }
        if (voltageRampDuration[i] != voltageRampDurationOrig[i] || currentRampDuration[i] != currentRampDurationOrig[i]) {
            return 1;
        }
        if (outputDelayDuration[i] != outputDelayDurationOrig[i]) {
            return 1;
        }
    }
    return 0;
}

void SysSettingsRampAndDelayPage::set() {
    bool triggerAborted = false;

    for (int i = 0; i < CH_NUM; i++) {
        auto &channel = Channel::get(i);

        if (rampState[i]) {
            channel_dispatcher::setTriggerVoltage(channel, triggerVoltage[i]);
            channel_dispatcher::setVoltageRampDuration(channel, voltageRampDuration[i]);
            channel_dispatcher::setTriggerCurrent(channel, triggerCurrent[i]);
            channel_dispatcher::setCurrentRampDuration(channel, currentRampDuration[i]);
            channel_dispatcher::setOutputDelayDuration(channel, outputDelayDuration[i]);

            if (channel_dispatcher::getVoltageTriggerMode(channel) != TRIGGER_MODE_STEP || channel_dispatcher::getCurrentTriggerMode(channel) != TRIGGER_MODE_STEP) {
                if (!triggerAborted) {
                    trigger::abort();
                    triggerAborted = true;
                }
                channel_dispatcher::setVoltageTriggerMode(channel, TRIGGER_MODE_STEP);
                channel_dispatcher::setCurrentTriggerMode(channel, TRIGGER_MODE_STEP);
                channel_dispatcher::setTriggerOutputState(channel, true);
            }
        } else {
            if (channel_dispatcher::getVoltageTriggerMode(channel) == TRIGGER_MODE_STEP || channel_dispatcher::getCurrentTriggerMode(channel) == TRIGGER_MODE_STEP) {
                if (!triggerAborted) {
                    trigger::abort();
                    triggerAborted = true;
                }
                channel_dispatcher::setVoltageTriggerMode(channel, TRIGGER_MODE_FIXED);
                channel_dispatcher::setCurrentTriggerMode(channel, TRIGGER_MODE_FIXED);
            }
        }
    }

    popPage();
}

void SysSettingsRampAndDelayPage::toggleRampState(int channelIndex) {
    bool newRampState = !rampState[channelIndex];

    if (channelIndex < 2 && (channel_dispatcher::getCouplingType() == channel_dispatcher::COUPLING_TYPE_SERIES || channel_dispatcher::getCouplingType() == channel_dispatcher::COUPLING_TYPE_PARALLEL)) {
        rampState[0] = newRampState;
        rampState[1] = newRampState;
    } else if (Channel::get(channelIndex).flags.trackingEnabled) {
        for (int i = 0; i < CH_NUM; ++i) {
            Channel &trackingChannel = Channel::get(i);
            if (trackingChannel.flags.trackingEnabled) {
                rampState[i] = newRampState;
            }
        }
    } else {
        rampState[channelIndex] = newRampState;
    }

    version++;
}

void SysSettingsRampAndDelayPage::setTriggerVoltage(int channelIndex, float value) {
    if (channelIndex < 2 && (channel_dispatcher::getCouplingType() == channel_dispatcher::COUPLING_TYPE_SERIES || channel_dispatcher::getCouplingType() == channel_dispatcher::COUPLING_TYPE_PARALLEL)) {
        triggerVoltage[0] = value;
        triggerVoltage[1] = value;
    } else if (Channel::get(channelIndex).flags.trackingEnabled) {
        for (int i = 0; i < CH_NUM; ++i) {
            Channel &trackingChannel = Channel::get(i);
            if (trackingChannel.flags.trackingEnabled) {
                triggerVoltage[i] = value;
            }
        }
    } else {
        triggerVoltage[channelIndex] = value;
    }

    version++;
}

void SysSettingsRampAndDelayPage::setTriggerCurrent(int channelIndex, float value) {
    if (channelIndex < 2 && (channel_dispatcher::getCouplingType() == channel_dispatcher::COUPLING_TYPE_SERIES || channel_dispatcher::getCouplingType() == channel_dispatcher::COUPLING_TYPE_PARALLEL)) {
        triggerCurrent[0] = value;
        triggerCurrent[1] = value;
    } else if (Channel::get(channelIndex).flags.trackingEnabled) {
        for (int i = 0; i < CH_NUM; ++i) {
            Channel &trackingChannel = Channel::get(i);
            if (trackingChannel.flags.trackingEnabled) {
                triggerCurrent[i] = value;
            }
        }
    } else {
        triggerCurrent[channelIndex] = value;
    }

    version++;
}

void SysSettingsRampAndDelayPage::setVoltageRampDuration(int channelIndex, float value) {
    if (channelIndex < 2 && (channel_dispatcher::getCouplingType() == channel_dispatcher::COUPLING_TYPE_SERIES || channel_dispatcher::getCouplingType() == channel_dispatcher::COUPLING_TYPE_PARALLEL)) {
        voltageRampDuration[0] = value;
        voltageRampDuration[1] = value;
    } else if (Channel::get(channelIndex).flags.trackingEnabled) {
        for (int i = 0; i < CH_NUM; ++i) {
            Channel &trackingChannel = Channel::get(i);
            if (trackingChannel.flags.trackingEnabled) {
                voltageRampDuration[i] = value;
            }
        }
    } else {
        voltageRampDuration[channelIndex] = value;
    }

    version++;
}

void SysSettingsRampAndDelayPage::setCurrentRampDuration(int channelIndex, float value) {
    if (channelIndex < 2 && (channel_dispatcher::getCouplingType() == channel_dispatcher::COUPLING_TYPE_SERIES || channel_dispatcher::getCouplingType() == channel_dispatcher::COUPLING_TYPE_PARALLEL)) {
        currentRampDuration[0] = value;
        currentRampDuration[1] = value;
    } else if (Channel::get(channelIndex).flags.trackingEnabled) {
        for (int i = 0; i < CH_NUM; ++i) {
            Channel &trackingChannel = Channel::get(i);
            if (trackingChannel.flags.trackingEnabled) {
                currentRampDuration[i] = value;
            }
        }
    } else {
        currentRampDuration[channelIndex] = value;
    }

    version++;
}

void SysSettingsRampAndDelayPage::setOutputDelayDuration(int channelIndex, float value) {
    if (channelIndex < 2 && (channel_dispatcher::getCouplingType() == channel_dispatcher::COUPLING_TYPE_SERIES || channel_dispatcher::getCouplingType() == channel_dispatcher::COUPLING_TYPE_PARALLEL)) {
        outputDelayDuration[0] = value;
        outputDelayDuration[1] = value;
    } else if (Channel::get(channelIndex).flags.trackingEnabled) {
        for (int i = 0; i < CH_NUM; ++i) {
            Channel &trackingChannel = Channel::get(i);
            if (trackingChannel.flags.trackingEnabled) {
                outputDelayDuration[i] = value;
            }
        }
    } else {
        outputDelayDuration[channelIndex] = value;
    }

    version++;
}

Value SysSettingsRampAndDelayPage::getRefreshState() {
    return Value((version << 24) | (g_focusDataId << 8) | (g_focusCursor & 0xFF), VALUE_TYPE_UINT32);
}

void SysSettingsRampAndDelayPage::draw(const WidgetCursor &widgetCursor) {
    auto page = (SysSettingsRampAndDelayPage *)getPage(PAGE_ID_SYS_SETTINGS_RAMP_AND_DELAY);

    const Widget *widget = widgetCursor.widget;
    const Style* style = getStyle(widget->style);
    drawRectangle(widgetCursor.x, widgetCursor.y, (int)widget->w, (int)widget->h, style, false, false, true);

    float drawVoltageRamps = (g_focusDataId != DATA_ID_CHANNEL_I_TRIGGER_VALUE && g_focusDataId != DATA_ID_CHANNEL_CURRENT_RAMP_DURATION);
    float T = 1E-3f;
    float limit = 1E-3f;

    for (int channelIndex = 0; channelIndex < CH_NUM; channelIndex++) {
        if (page->rampState[channelIndex]) {
            T = MAX(T, page->outputDelayDuration[channelIndex] + (drawVoltageRamps ? page->voltageRampDuration[channelIndex] : page->currentRampDuration[channelIndex]));
            limit = MAX(limit, (drawVoltageRamps ? page->triggerVoltage[channelIndex] : page->triggerCurrent[channelIndex]));
        }
    }

    T *= 1.1f;
    limit *= 1.1f;

    for (int channelIndex = 0; channelIndex < CH_NUM; channelIndex++) {
        if (page->rampState[channelIndex] && channelIndex != g_focusCursor) {
            page->drawRamp(widgetCursor, channelIndex, drawVoltageRamps, T, limit);
        }
    }

    if (g_focusCursor != -1 && page->rampState[g_focusCursor]) {
        page->drawRamp(widgetCursor, g_focusCursor, drawVoltageRamps, T, limit);
        page->drawRamp(widgetCursor, g_focusCursor, drawVoltageRamps, T, limit, 1);
    }
}

void SysSettingsRampAndDelayPage::drawRamp(const WidgetCursor &widgetCursor, int channelIndex, bool drawVoltageRamps, float T, float limit, int yOffset) {
    const Widget *widget = widgetCursor.widget;
    const Style* style = getStyle(widget->style);
    font::Font font = styleGetFont(style);
    int textHeight = font.getHeight();

    float delay = outputDelayDuration[channelIndex];
    float duration = (drawVoltageRamps ? voltageRampDuration[channelIndex] : currentRampDuration[channelIndex]);

    float step = (drawVoltageRamps ? triggerVoltage[channelIndex] : triggerCurrent[channelIndex]);

    int x1 = widgetCursor.x;
    int x2 = x1 + (int)roundf(delay * (widget->w - 1) / T);
    int x3 = x1 + (int)roundf((delay + duration)  * (widget->w - 1) / T);
    int x4 = widgetCursor.x + widget->w - 1;

    int y1 = widgetCursor.y + widget->h - 1;
    int y2 = y1 - (int)roundf(step * (widget->h - 1) / limit);

    y1 -= yOffset;
    y2 -= yOffset;

    char label[20];
    snprintf(label, sizeof(label), "Ch%d %s", channelIndex + 1, (drawVoltageRamps ? "U" : "I"));

    auto tmp = g_channelIndex;
    g_channelIndex = channelIndex;
    mcu::display::setColor(COLOR_ID_CHANNEL1);
    g_channelIndex = tmp;

    mcu::display::drawHLine(x1, y1, x2 - x1);
    drawLine(x2, y1, x3, y2);
    mcu::display::drawHLine(x3, y2, x4 - x3);

    if (yOffset) {
        int textWidth = mcu::display::measureStr(label, -1, font, 0);
        mcu::display::drawStr(label, -1,
            x4 - textWidth, y2 - textHeight > widgetCursor.y ? y2 - textHeight : y2,
            widgetCursor.x, widgetCursor.y, widgetCursor.x + widget->w - 1, widgetCursor.y + widget->h - 1,
            font, -1
        );
    }
}

} // namespace gui
} // namespace psu
} // namespace eez

#endif
