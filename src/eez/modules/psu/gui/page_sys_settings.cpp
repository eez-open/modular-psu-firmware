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

#include <eez/firmware.h>

#include <eez/modules/psu/psu.h>

#include <math.h>

#include <eez/modules/psu/ethernet.h>
#include <eez/modules/psu/persist_conf.h>
#include <eez/modules/psu/profile.h>
#include <eez/modules/psu/temperature.h>
#if OPTION_ENCODER
#include <eez/modules/mcu/encoder.h>
#endif
#include <eez/modules/psu/util.h>
#if OPTION_ETHERNET
#include <eez/modules/psu/ntp.h>
#endif
#include <eez/modules/psu/channel_dispatcher.h>

#include <eez/modules/psu/gui/data.h>
#include <eez/modules/psu/gui/numeric_keypad.h>
#include <eez/modules/psu/gui/page_sys_settings.h>
#include <eez/modules/psu/gui/psu.h>
#include <eez/gui/gui.h>

#include <eez/modules/aux_ps/fan.h>

using namespace eez::psu::gui;

namespace eez {
namespace psu {
namespace gui {

////////////////////////////////////////////////////////////////////////////////

void SysSettingsDateTimePage::pageAlloc() {
#if OPTION_ETHERNET
    ntpEnabled = origNtpEnabled = persist_conf::isNtpEnabled();
    strcpy(ntpServer, persist_conf::devConf.ntpServer);
    strcpy(origNtpServer, persist_conf::devConf.ntpServer);
#else
    ntpEnabled = origNtpEnabled = false;
    strcpy(ntpServer, "");
    strcpy(origNtpServer, "");
#endif
    dateTimeModified = false;
    timeZone = origTimeZone = persist_conf::devConf.timeZone;
    dstRule = origDstRule = (datetime::DstRule)persist_conf::devConf.dstRule;
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
    strcpy(page->ntpServer, value);

    popPage();
}
#endif

void SysSettingsDateTimePage::edit() {
    const Widget *widget = getFoundWidgetAtDown().widget;
    int id = widget->data;

    NumericKeypadOptions options;

    const char *label = 0;

    data::Value value;

    if (id == DATA_ID_DATE_TIME_YEAR) {
        label = "Year (2016-2036): ";
        options.min = 2017;
        options.max = 2036;
        options.def = 2017;
        value = data::Value((int)dateTime.year);
    } else if (id == DATA_ID_DATE_TIME_MONTH) {
        label = "Month (1-12): ";
        options.min = 1;
        options.max = 12;
        options.def = 1;
        value = data::Value((int)dateTime.month);
    } else if (id == DATA_ID_DATE_TIME_DAY) {
        label = "Day (1-31): ";
        options.min = 1;
        options.max = 31;
        options.def = 1;
        value = data::Value((int)dateTime.day);
    } else if (id == DATA_ID_DATE_TIME_HOUR) {
        label = "Hour (0-23): ";
        options.min = 0;
        options.max = 23;
        options.def = 12;
        value = data::Value((int)dateTime.hour);
    } else if (id == DATA_ID_DATE_TIME_MINUTE) {
        label = "Minute (0-59): ";
        options.min = 0;
        options.max = 59;
        options.def = 0;
        value = data::Value((int)dateTime.minute);
    } else if (id == DATA_ID_DATE_TIME_SECOND) {
        label = "Second (0-59): ";
        options.min = 0;
        options.max = 59;
        options.def = 0;
        value = data::Value((int)dateTime.second);
    } else if (id == DATA_ID_DATE_TIME_TIME_ZONE) {
        label = "Time zone: ";
        options.min = -12.00;
        options.max = 14.00;
        options.def = 0;
        options.flags.dotButtonEnabled = true;
        options.flags.signButtonEnabled = true;
        value = data::Value(timeZone, VALUE_TYPE_TIME_ZONE);
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
    pushSelectFromEnumPage(g_dstRuleEnumDefinition, dstRule, 0, onDstRuleSet);
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
        dateTime.hour = uint8_t(value);
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
    } else {
        if (dateTimeModified) {
            return 1;
        }
    }

    return (timeZone != origTimeZone || dstRule != origDstRule) ? 1 : 0;
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
    if (ntpEnabled != origNtpEnabled || strcmp(ntpServer, origNtpServer)) {
        persist_conf::setNtpSettings(ntpEnabled, ntpServer);
    }
#endif

    if (dstRule != origDstRule) {
        persist_conf::setDstRule(dstRule);
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
    strcpy(m_hostNameOrig, persist_conf::devConf.ethernetHostName);
    strcpy(m_hostName, persist_conf::devConf.ethernetHostName);
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

    NumericKeypad::start(0, data::Value((int)m_scpiPort, VALUE_TYPE_PORT), options, onSetScpiPort, 0, 0);
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

    NumericKeypad::start(0, data::Value((uint32_t)address, VALUE_TYPE_IP_ADDRESS), options, 0,
                         onAddressSet, 0);
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
    strcpy(m_hostOrig, persist_conf::devConf.mqttHost);
    strcpy(m_host, persist_conf::devConf.mqttHost);
    m_portOrig = m_port = persist_conf::devConf.mqttPort;
    strcpy(m_usernameOrig, persist_conf::devConf.mqttUsername);
    strcpy(m_username, persist_conf::devConf.mqttUsername);
    strcpy(m_passwordOrig, persist_conf::devConf.mqttPassword);
    strcpy(m_password, persist_conf::devConf.mqttPassword);
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
    origFanSpeed = fanSpeed = MakeValue(fanMode == FAN_MODE_AUTO ? 100.0f : 1.0f * persist_conf::devConf.fanSpeed, UNIT_PERCENT);
}

int SysSettingsTemperaturePage::getDirty() {
    return (origState != state || origLevel != level || origDelay != delay || origFanMode != fanMode || origFanSpeed != fanSpeed) ? 1 : 0;
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
    page->fanSpeed = MakeValue(value, UNIT_PERCENT);
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

    NumericKeypad::start(0, fanSpeed, options, onFanSpeedSet, 0, 0);
}

void SysSettingsTemperaturePage::setParams() {
    temperature::sensors[temp_sensor::AUX].prot_conf.state = state ? true : false;
    temperature::sensors[temp_sensor::AUX].prot_conf.level = level.getFloat();
    temperature::sensors[temp_sensor::AUX].prot_conf.delay = delay.getFloat();

    persist_conf::setFanSettings(fanMode, (uint8_t)roundf(fanSpeed.getFloat()));

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
    m_sourceOrig = m_source = trigger::getSource();
    m_delayOrig = m_delay = trigger::getDelay();
    m_initiateContinuouslyOrig = m_initiateContinuously =
        trigger::isContinuousInitializationEnabled();
}

void SysSettingsTriggerPage::onTriggerSourceSet(uint16_t value) {
    popPage();
    SysSettingsTriggerPage *page = (SysSettingsTriggerPage *)getActivePage();
    page->m_source = (trigger::Source)value;
}

void SysSettingsTriggerPage::selectSource() {
    pushSelectFromEnumPage(g_triggerSourceEnumDefinition, m_source, 0, onTriggerSourceSet);
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

    NumericKeypad::start(0, MakeValue(trigger::getDelay(), UNIT_SECOND), options, onDelaySet, 0, 0);
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

        if (m_source == trigger::SOURCE_PIN1) {
            persist_conf::setIoPinFunction(0, io_pins::FUNCTION_TINPUT);
        } else if (m_source == trigger::SOURCE_PIN2) {
            persist_conf::setIoPinFunction(1, io_pins::FUNCTION_TINPUT);
        }

        popPage();
        
        // infoMessage("Trigger settings saved!");
    }
}

////////////////////////////////////////////////////////////////////////////////

void SysSettingsIOPinsPage::pageAlloc() {
    for (int i = 0; i < NUM_IO_PINS; i++) {
        m_polarityOrig[i] = m_polarity[i] = (io_pins::Polarity)persist_conf::devConf.ioPins[i].polarity;
        m_functionOrig[i] = m_function[i] = (io_pins::Function)persist_conf::devConf.ioPins[i].function;
    }
}

void SysSettingsIOPinsPage::togglePolarity() {
    int i = getFoundWidgetAtDown().cursor.i;
    m_polarity[i] = m_polarity[i] == io_pins::POLARITY_NEGATIVE ? io_pins::POLARITY_POSITIVE : io_pins::POLARITY_NEGATIVE;
}

void SysSettingsIOPinsPage::onFunctionSet(uint16_t value) {
    popPage();
    SysSettingsIOPinsPage *page = (SysSettingsIOPinsPage *)getActivePage();
    page->m_function[page->pinNumber] = (io_pins::Function)value;
}

void SysSettingsIOPinsPage::selectFunction() {
    pinNumber = getFoundWidgetAtDown().cursor.i;
    pushSelectFromEnumPage(
        pinNumber < 2 ? g_ioPinsInputFunctionEnumDefinition : g_ioPinsOutputFunctionEnumDefinition,
        m_function[pinNumber], 0, onFunctionSet
    );
}

int SysSettingsIOPinsPage::getDirty() {
    for (int i = 0; i < NUM_IO_PINS; ++i) {
        if (m_polarityOrig[i] != m_polarity[i] || m_functionOrig[i] != m_function[i]) {
            return true;
        }
    }
    return false;
}

void SysSettingsIOPinsPage::set() {
    if (getDirty()) {
        for (int i = 0; i < NUM_IO_PINS; i++) {
            persist_conf::setIoPinPolarity(i, m_polarity[i]);
            persist_conf::setIoPinFunction(i, m_function[i]);
        }

        popPage();
    }
}

////////////////////////////////////////////////////////////////////////////////

void SysSettingsSerialPage::pageAlloc() {
    m_enabledOrig = m_enabled = persist_conf::isSerialEnabled();
    m_baudIndexOrig = m_baudIndex = persist_conf::getSerialBaudIndex();
    m_parityOrig = m_parity = (serial::Parity)persist_conf::getSerialParity();
}

void SysSettingsSerialPage::toggle() {
    m_enabled = !m_enabled;
}

void SysSettingsSerialPage::onParitySet(uint16_t value) {
    popPage();
    SysSettingsSerialPage *page = (SysSettingsSerialPage *)getActivePage();
    page->m_parity = (serial::Parity)value;
}

void SysSettingsSerialPage::selectParity() {
    pushSelectFromEnumPage(g_serialParityEnumDefinition, m_parity, 0, onParitySet);
}

int SysSettingsSerialPage::getDirty() {
    return m_enabledOrig != m_enabled || m_baudIndexOrig != m_baudIndex || m_parityOrig != m_parity;
}

void SysSettingsSerialPage::set() {
    if (getDirty()) {
        if (persist_conf::setSerialSettings(m_enabled, m_baudIndex, m_parity)) {
            popPage();
            infoMessage("Serial settings saved!");
        }
    }
}

////////////////////////////////////////////////////////////////////////////////

void SysSettingsTrackingPage::pageAlloc() {
    m_trackingEnabled = 0;
    for (int i = 0; i < CH_MAX; i++) {
        if (Channel::get(i).flags.trackingEnabled) {
            m_trackingEnabled |= 1 << i;
        }
    }
    m_trackingEnabledOrig = m_trackingEnabled;
}

int SysSettingsTrackingPage::getDirty() {
    return m_trackingEnabled != m_trackingEnabledOrig;
}

void SysSettingsTrackingPage::set() {
    if (getDirty()) {
        int n = 0;
        for (int i = 0; i < CH_MAX; i++) {
            if (m_trackingEnabled & (1 << i)) {
                n++;
            }
        }

        if (n == 1) {
            errorMessage("At least 2 channels must be enabled!");
        } else {
            channel_dispatcher::setTrackingChannels(m_trackingEnabled);

            popPage();

            if (n >= 2) {
                infoMessage("Tracking enabled!");
            } else {
                infoMessage("Tracking disabled!");
            }

        }
    }
}

////////////////////////////////////////////////////////////////////////////////

void SysSettingsCouplingPage::pageAlloc() {
    m_couplingType = m_couplingTypeOrig = channel_dispatcher::getCouplingType();

    m_enableTrackingMode = m_enableTrackingModeOrig = 
        m_couplingType != channel_dispatcher::COUPLING_TYPE_SPLIT_RAILS || 
        (Channel::get(0).flags.trackingEnabled && Channel::get(1).flags.trackingEnabled);
}

int SysSettingsCouplingPage::getDirty() {
    return m_couplingType != m_couplingTypeOrig || m_enableTrackingMode != m_enableTrackingModeOrig;
}

void SysSettingsCouplingPage::set() {
    if (getDirty()) {
        int err;
        if (channel_dispatcher::setCouplingType(m_couplingType, &err)) {
            if (m_couplingType == channel_dispatcher::COUPLING_TYPE_SPLIT_RAILS) {
                auto trackingPage = (SysSettingsTrackingPage *)getPage(PAGE_ID_SYS_SETTINGS_TRACKING);
                if (trackingPage) {
                    trackingPage->m_trackingEnabled = 0;
                    if (m_enableTrackingMode) {
                        // enable tracking for first two channels
                        trackingPage->m_trackingEnabled |= (1 << 0);
                        trackingPage->m_trackingEnabled |= (1 << 1);
                    }
                    trackingPage->m_trackingEnabledOrig = trackingPage->m_trackingEnabled;
                   
                    channel_dispatcher::setTrackingChannels(trackingPage->m_trackingEnabled);
                } else {
                    int trackingEnabled = 0;
                    if (m_enableTrackingMode) {
                        // enable tracking for first two channels
                        trackingEnabled |= (1 << 0);
                        trackingEnabled |= (1 << 1);
                    }
                    channel_dispatcher::setTrackingChannels(trackingEnabled);
                }
            }

            popPage();

            if (channel_dispatcher::getCouplingType() == channel_dispatcher::COUPLING_TYPE_NONE) {
                infoMessage("Uncoupled!");
            } else if (channel_dispatcher::getCouplingType() == channel_dispatcher::COUPLING_TYPE_PARALLEL) {
                infoMessage("Coupled in parallel!");
            } else if (channel_dispatcher::getCouplingType() == channel_dispatcher::COUPLING_TYPE_SERIES) {
                infoMessage("Coupled in series!");
            } else if (channel_dispatcher::getCouplingType() == channel_dispatcher::COUPLING_TYPE_SPLIT_RAILS) {
                infoMessage("Coupled in split rails!");
            } else {
                infoMessage("Coupled in common GND!");
            }
        } else {
            event_queue::pushEvent(err);
            infoMessage("Coupling failed!");
        }
    }
}

} // namespace gui
} // namespace psu
} // namespace eez

#endif
