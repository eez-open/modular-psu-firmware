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

#define NUM_CHANNELS_VIEW_MODES 4

#define CHANNELS_VIEW_MODE_NUMERIC   0
#define CHANNELS_VIEW_MODE_VERT_BAR  1
#define CHANNELS_VIEW_MODE_HORZ_BAR  2
#define CHANNELS_VIEW_MODE_YT        3

#define NUM_CHANNELS_VIEW_MODES_IN_MAX 3

#define CHANNELS_VIEW_MODE_IN_MAX_NUMERIC   0
#define CHANNELS_VIEW_MODE_IN_MAX_HORZ_BAR  1
#define CHANNELS_VIEW_MODE_IN_MAX_YT        2

namespace eez {
namespace psu {

class Channel;

namespace profile {

struct Parameters;

}
} // namespace psu
} // namespace eez

namespace eez {
namespace psu {
/// Store/restore of persistent configuration data (device configuration, calibration parameters,
/// profiles) using external EEPROM.
namespace persist_conf {

/// Header of the every block stored in EEPROM. It contains checksum and version.
struct BlockHeader {
    uint32_t checksum;
    uint16_t version;
};

struct IOPin {
    unsigned polarity : 1;
    unsigned function : 7;
};

/// Device configuration block.
struct DeviceConfiguration {
    // block 1
    char serialNumber[7 + 1];
    char systemPassword[PASSWORD_MAX_LENGTH + 1];
    char calibration_password[PASSWORD_MAX_LENGTH + 1];

    int16_t touch_screen_cal_tlx;
    int16_t touch_screen_cal_tly;
    int16_t touch_screen_cal_brx;
    int16_t touch_screen_cal_bry;
    int16_t touch_screen_cal_trx;
    int16_t touch_screen_cal_try;

    unsigned skipChannelCalibrations : 1;
    unsigned skipDateTimeSetup : 1;
    unsigned skipSerialSetup : 1;
    unsigned skipEthernetSetup : 1;

    // block 2
    uint8_t date_year;
    uint8_t date_month;
    uint8_t date_day;
    uint8_t time_hour;
    uint8_t time_minute;
    uint8_t time_second;
    int16_t time_zone;
    uint8_t dstRule;

    unsigned dateValid : 1;
    unsigned timeValid : 1;
    unsigned dst : 1;

    // block 3
    int8_t profile_auto_recall_location;

    unsigned profileAutoRecallEnabled : 1;

    unsigned outputProtectionCouple : 1;
    unsigned shutdownWhenProtectionTripped : 1;
    unsigned forceDisablingAllOutputsOnPowerUp : 1;

    // block 4
    uint8_t serialBaud;
    uint8_t serialParity;
    
    uint32_t ethernetIpAddress;
    uint32_t ethernetDns;
    uint32_t ethernetGateway;
    uint32_t ethernetSubnetMask;
    uint16_t ethernetScpiPort;
    char ntpServer[32 + 1];
    uint8_t ethernetMacAddress[6];

    uint8_t fanMode;
    uint8_t fanSpeed;

    uint8_t encoderMovingSpeedDown;
    uint8_t encoderMovingSpeedUp;

    uint8_t displayBrightness;
    uint8_t displayBackgroundLuminosityStep;
    uint8_t selectedThemeIndex;
    float animationsDuration;

    unsigned serialEnabled : 1;

    unsigned ethernetEnabled : 1;
    unsigned ntpEnabled : 1;
    unsigned ethernetDhcpEnabled : 1;

    unsigned encoderConfirmationMode : 1;

    unsigned isSoundEnabled : 1;
    unsigned isClickSoundEnabled : 1;

    // block 5
    uint8_t triggerSource;
    float triggerDelay;

    IOPin ioPins[4];

    unsigned triggerContinuousInitializationEnabled : 1;

    unsigned isFrontPanelLocked : 1;

    unsigned sdLocked : 1;

    // block 6
    uint8_t ytGraphUpdateMethod;

    unsigned displayState : 1; // 0: display is OFF, 1: display is ON
    unsigned channelsViewMode : 3;
    unsigned maxChannel : 3; // 0: default view, 1: Ch1 maxed, 2: Ch2 maxed, ...
    unsigned channelsViewModeInMax : 3;
};

extern const DeviceConfiguration &devConf;

void init();
void tick();

bool checkBlock(const BlockHeader *block, uint16_t size, uint16_t version);
uint32_t calcChecksum(const BlockHeader *block, uint16_t size);

bool isSystemPasswordValid(const char *new_password, size_t new_password_len, int16_t &err);
void changeSystemPassword(const char *new_password, size_t new_password_len);

bool isCalibrationPasswordValid(const char *new_password, size_t new_password_len, int16_t &err);
void changeCalibrationPassword(const char *new_password, size_t new_password_len);

void changeSerial(const char *newSerialNumber, size_t newSerialNumberLength);

void enableSound(bool enable);
bool isSoundEnabled();

void enableClickSound(bool enable);
bool isClickSoundEnabled();

bool enableEthernet(bool enable);
bool isEthernetEnabled();

bool readSystemDate(uint8_t &year, uint8_t &month, uint8_t &day);
void writeSystemDate(uint8_t year, uint8_t month, uint8_t day, unsigned dst);

bool readSystemTime(uint8_t &hour, uint8_t &minute, uint8_t &second);
void writeSystemTime(uint8_t hour, uint8_t minute, uint8_t second, unsigned dst);

void writeSystemDateTime(uint8_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t minute,
                         uint8_t second, unsigned dst);

void enableProfileAutoRecall(bool enable);
bool isProfileAutoRecallEnabled();
void setProfileAutoRecallLocation(int location);
int getProfileAutoRecallLocation();

void setChannelsViewMode(unsigned int viewMode);
unsigned int getChannelsViewMode();
void setChannelsViewModeInMax(unsigned int viewModeInMax);

void toggleChannelsViewMode();

bool isMaxChannelView();
int getMaxChannelIndex();
int getMin1ChannelIndex();
int getMin2ChannelIndex();
void setMaxChannelIndex(int channelIndex);
void toggleMaxChannelIndex(int channelIndex);

profile::Parameters *loadProfile(int location);
bool saveProfile(int location, profile::Parameters *profile);

uint32_t readTotalOnTime(int type);
bool writeTotalOnTime(int type, uint32_t time);

void enableOutputProtectionCouple(bool enable);
bool isOutputProtectionCoupleEnabled();

void enableShutdownWhenProtectionTripped(bool enable);
bool isShutdownWhenProtectionTrippedEnabled();

void enableForceDisablingAllOutputsOnPowerUp(bool enable);
bool isForceDisablingAllOutputsOnPowerUpEnabled();

void lockFrontPanel(bool lock);

void setEncoderSettings(uint8_t confirmationMode, uint8_t movingSpeedDown, uint8_t movingSpeedUp);

void setDisplayState(unsigned state);
void setDisplayBrightness(uint8_t displayBrightness);
void setDisplayBackgroundLuminosityStep(uint8_t displayBackgroundLuminosityStep);

bool enableSerial(bool enable);
bool isSerialEnabled();
int getIndexFromBaud(long baud);
long getBaudFromIndex(int index);
int getSerialBaudIndex();
bool setSerialBaudIndex(int baudIndex);
int getSerialParity();
bool setSerialParity(int parity);
bool setSerialSettings(bool enable, int baudIndex, int parity);

bool enableEthernetDhcp(bool enable);
bool isEthernetDhcpEnabled();
bool setEthernetMacAddress(uint8_t macAddress[]);
bool setEthernetIpAddress(uint32_t ipAddress);
bool setEthernetDns(uint32_t dns);
bool setEthernetGateway(uint32_t gateway);
bool setEthernetSubnetMask(uint32_t subnetMask);
bool setEthernetScpiPort(uint16_t scpiPort);
bool setEthernetSettings(bool enable, bool dhcpEnable, uint32_t ipAddress, uint32_t dns,
                         uint32_t gateway, uint32_t subnetMask, uint16_t scpiPort,
                         uint8_t *macAddress);

void enableNtp(bool enable);
bool isNtpEnabled();
void setNtpServer(const char *ntpServer, size_t ntpServerLength);
void setNtpSettings(bool enable, const char *ntpServer);

void setSdLocked(bool sdLocked);
bool isSdLocked();

void setAnimationsDuration(float value);

void setTouchscreenCalParams(int16_t touch_screen_cal_tlx, int16_t touch_screen_cal_tly, int16_t touch_screen_cal_brx, int16_t touch_screen_cal_bry, int16_t touch_screen_cal_trx, int16_t touch_screen_cal_try);

void setFanSettings(uint8_t fanMode, uint8_t fanSpeed);

void setDateValid(unsigned dateValid);
void setTimeValid(unsigned timeValid);
void setTimeZone(int16_t time_zone);
void setDstRule(uint8_t dstRule);

void setIoPinPolarity(int pin, unsigned polarity);
void setIoPinFunction(int pin, unsigned function);

void setSelectedThemeIndex(uint8_t selectedThemeIndex);

void resetTrigger();
void setTriggerContinuousInitializationEnabled(unsigned triggerContinuousInitializationEnabled);
void setTriggerDelay(float triggerDelay);
void setTriggerSource(uint8_t triggerSource);

void setSkipChannelCalibrations(unsigned skipChannelCalibrations);
void setSkipDateTimeSetup(unsigned skipDateTimeSetup);
void setSkipSerialSetup(unsigned skipSerialSetup);
void setSkipEthernetSetup(unsigned skipEthernetSetup);

////////////////////////////////////////////////////////////////////////////////

struct ModuleConfiguration {
    BlockHeader header;
    uint8_t chCalEnabled;
};

extern ModuleConfiguration g_moduleConf[NUM_SLOTS];

void loadModuleConf(int slotIndex);
bool saveModuleConf(int slotIndex);

bool isChannelCalibrationEnabled(Channel &channel);
void saveCalibrationEnabledFlag(Channel &channel, bool enabled);

void loadChannelCalibration(Channel &channel);
bool saveChannelCalibration(Channel &channel);

} // namespace persist_conf
} // namespace psu
} // namespace eez

extern "C" void getMacAddress(uint8_t macAddress[]);
