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

#pragma once

#include <eez/gui/page.h>
using namespace eez::gui;

#include <eez/modules/psu/datetime.h>
#include <eez/modules/psu/io_pins.h>
#include <eez/modules/psu/serial_psu.h>
#include <eez/modules/psu/trigger.h>
#include <eez/modules/psu/channel_dispatcher.h>

namespace eez {
namespace psu {
namespace gui {

class SysSettingsDateTimePage : public SetPage {
  public:
    void pageAlloc();

    void edit();
    void setValue(float value);
    int getDirty();
    void set();

    void toggleNtp();
    void editNtpServer();
    void selectDstRule();

    bool ntpEnabled;
    char ntpServer[32 + 1];
    datetime::DateTime dateTime;
    int16_t timeZone;
    datetime::DstRule dstRule;
    bool dateTimeModified;

  private:
    bool origNtpEnabled;
    char origNtpServer[32 + 1];
    int16_t origTimeZone;
    datetime::DstRule origDstRule;

    static void onDstRuleSet(uint16_t value);

#if OPTION_ETHERNET
    enum {
        TEST_NTP_SERVER_OPERATION_STATE_INIT = -1,
        TEST_NTP_SERVER_OPERATION_STATE_STARTED = -2
    };

    static void onSetNtpServer(char *value);

    static void checkTestNtpServerStatus();
    void testNtpServer();
#endif

    void doSet();
};

#if OPTION_ETHERNET

class SysSettingsEthernetPage : public SetPage {
    friend class SysSettingsEthernetStaticPage;

  public:
    void pageAlloc();

    void toggle();
    void toggleDhcp();
    void editStaticAddress();
    void editScpiPort();
    void editMacAddress();

    int getDirty();
    void set();

    bool m_enabled;
    bool m_dhcpEnabled;
    uint16_t m_scpiPort;

    uint8_t m_macAddress[6];

  private:
    bool m_enabledOrig;

    bool m_dhcpEnabledOrig;

    uint32_t m_ipAddressOrig;
    uint32_t m_ipAddress;

    uint32_t m_dnsOrig;
    uint32_t m_dns;

    uint32_t m_gatewayOrig;
    uint32_t m_gateway;

    uint32_t m_subnetMaskOrig;
    uint32_t m_subnetMask;

    uint16_t m_scpiPortOrig;

    uint8_t m_macAddressOrig[6];

    static void onSetScpiPort(float value);
    static void onSetMacAddress(char *value);
};

class SysSettingsEthernetStaticPage : public SetPage {
  public:
    void pageAlloc();

    void editIpAddress();
    void editDns();
    void editGateway();
    void editSubnetMask();

    int getDirty();
    void set();

    uint32_t m_ipAddress;
    uint32_t m_dns;
    uint32_t m_gateway;
    uint32_t m_subnetMask;

  private:
    enum Field { FIELD_IP_ADDRESS, FIELD_DNS, FIELD_GATEWAY, FIELD_SUBNET_MASK };

    uint32_t m_ipAddressOrig;
    uint32_t m_dnsOrig;
    uint32_t m_gatewayOrig;
    uint32_t m_subnetMaskOrig;
    uint32_t *m_editAddress;

    void editAddress(uint32_t &address);
    static void onAddressSet(uint32_t address);
};

#endif

class SysSettingsProtectionsPage : public Page {
  public:
    static void toggleOutputProtectionCouple();
    static void toggleShutdownWhenProtectionTripped();
    static void toggleForceDisablingAllOutputsOnPowerUp();
};

class SysSettingsTemperaturePage : public SetPage {
  public:
    void pageAlloc();

    int getDirty();
    void set();

    void toggleState();
    void editLevel();
    void editDelay();

    void toggleFanMode();
    void editFanSpeed();

    static void clear();

    int state;
    data::Value level;
    data::Value delay;

    int fanMode;
    data::Value fanSpeed;

  protected:
    int origState;

    data::Value origLevel;
    float minLevel;
    float maxLevel;
    float defLevel;

    data::Value origDelay;
    float minDelay;
    float maxDelay;
    float defaultDelay;

    int origFanMode;
    data::Value origFanSpeed;

    virtual void setParams();

    static void onLevelSet(float value);
    static void onDelaySet(float value);
    static void onFanSpeedSet(float value);
};

class SysSettingsSoundPage : public Page {
  public:
    static void toggleSound();
    static void toggleClickSound();
};

#if OPTION_ENCODER

class SysSettingsEncoderPage : public SetPage {
  public:
    void pageAlloc();

    void toggleConfirmationMode();

    int getDirty();
    void set();

    uint8_t confirmationMode;
    uint8_t movingSpeedDown;
    uint8_t movingSpeedUp;

  private:
    uint8_t origConfirmationMode;
    uint8_t origMovingSpeedDown;
    uint8_t origMovingSpeedUp;
};

#endif

class SysSettingsTriggerPage : public SetPage {
  public:
    void pageAlloc();

    void selectSource();
    void editDelay();
    void toggleInitiateContinuously();

    int getDirty();
    void set();

    trigger::Source m_source;
    float m_delay;
    bool m_initiateContinuously;

  private:
    trigger::Source m_sourceOrig;
    float m_delayOrig;
    bool m_initiateContinuouslyOrig;

    static void onTriggerSourceSet(uint16_t value);
    static void onDelaySet(float value);
};

class SysSettingsIOPinsPage : public SetPage {
  public:
    void pageAlloc();

    void togglePolarity();
    void selectFunction();

    int getDirty();
    void set();

    io_pins::Polarity m_polarity[NUM_IO_PINS];
    io_pins::Function m_function[NUM_IO_PINS];

  private:
    int pinNumber;

    io_pins::Polarity m_polarityOrig[NUM_IO_PINS];
    io_pins::Function m_functionOrig[NUM_IO_PINS];

    static void onFunctionSet(uint16_t value);
};

class SysSettingsSerialPage : public SetPage {
  public:
    void pageAlloc();

    void toggle();
    void selectParity();

    int getDirty();
    void set();

    bool m_enabled;
    uint8_t m_baudIndex;
    serial::Parity m_parity;

  private:
    bool m_enabledOrig;
    uint8_t m_baudIndexOrig;
    serial::Parity m_parityOrig;

    static void onParitySet(uint16_t value);
};

class SysSettingsTrackingPage : public SetPage {
    friend class SysSettingsCouplingPage;

public:
    void pageAlloc();

    int getDirty();
    void set();

    int m_trackingEnabled[CH_MAX];

private:
    int m_trackingEnabledOrig[CH_MAX];
}; 

class SysSettingsCouplingPage : public SetPage {
public:
    void pageAlloc();

    int getDirty();
    void set();

    channel_dispatcher::CouplingType m_couplingType;
    bool m_enableTrackingMode;

private:
    channel_dispatcher::CouplingType m_couplingTypeOrig;
    bool m_enableTrackingModeOrig;
};

} // namespace gui
} // namespace psu
} // namespace eez
