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

#include <assert.h>

#include <eez/apps/psu/psu.h>

#include <eez/modules/mcu/eeprom.h>
#include <eez/modules/bp3c/eeprom.h>

#include <eez/apps/psu/event_queue.h>
#include <eez/apps/psu/profile.h>
#include <eez/apps/psu/serial_psu.h>

#if OPTION_ENCODER
#include <eez/modules/mcu/encoder.h>
#endif

#if OPTION_DISPLAY
#include <eez/apps/psu/gui/psu.h>
#include <eez/modules/mcu/display.h>
using namespace eez::mcu::display;
#endif

#if OPTION_ETHERNET
#include <eez/apps/psu/ethernet.h>
#endif

#include <eez/apps/psu/datetime.h>
#include <eez/apps/psu/trigger.h>
#include <eez/apps/psu/ontime.h>

#include <eez/gui/widgets/yt_graph.h>

#define NUM_RETRIES 2

namespace eez {
namespace psu {

#if OPTION_SD_CARD
namespace sd_card {
bool confRead(uint8_t *buffer, uint16_t bufferSize, uint16_t address);
bool confWrite(const uint8_t *buffer, uint16_t bufferSize, uint16_t address);
} // namespace sd_card
#endif

namespace persist_conf {

////////////////////////////////////////////////////////////////////////////////

enum PersistConfSection {
    PERSIST_CONF_BLOCK_DEVICE,
    PERSIST_CONF_BLOCK_DEVICE2,
    PERSIST_CONF_BLOCK_FIRST_PROFILE,
};

static const uint16_t DEV_CONF_VERSION = 9;
static const uint16_t DEV_CONF2_VERSION = 11;
static const uint16_t MODULE_CONF_VERSION = 1;
static const uint16_t CH_CAL_CONF_VERSION = 3;

static const uint16_t PERSIST_CONF_DEVICE_ADDRESS = 1024;
static const uint16_t PERSIST_CONF_DEVICE2_ADDRESS = 1536;

static const uint16_t PERSIST_CONF_FIRST_PROFILE_ADDRESS = 5120;
static const uint16_t PERSIST_CONF_PROFILE_BLOCK_SIZE = 1024;

static const uint32_t ONTIME_MAGIC = 0xA7F31B3CL;

////////////////////////////////////////////////////////////////////////////////

enum ModulePersistConfSection {
    MODULE_PERSIST_CONF_BLOCK_MODULE_CONFIGURATION,
    MODULE_PERSIST_CONF_BLOCK_CH_CAL,
};

static const uint16_t MODULE_PERSIST_CONF_BLOCK_MODULE_CONFIGURATION_ADDRESS = 64;
static const uint16_t MODULE_PERSIST_CONF_BLOCK_MODULE_CONFIGURATION_SIZE = 64;

static const uint16_t MODULE_PERSIST_CONF_CH_CAL_ADDRESS = 128;
static const uint16_t MODULE_PERSIST_CONF_CH_CAL_BLOCK_SIZE = 144;

////////////////////////////////////////////////////////////////////////////////

DeviceConfiguration devConf;
DeviceConfiguration2 devConf2;

////////////////////////////////////////////////////////////////////////////////

bool confRead(uint8_t *buffer, uint16_t bufferSize, uint16_t address, int version) {
    for (int i = 0; i < NUM_RETRIES; i++) {
        bool result = false;
        if (mcu::eeprom::g_testResult == TEST_OK) {
            result = mcu::eeprom::read(buffer, bufferSize, address);
        }
#if OPTION_SD_CARD
        else {
            result = sd_card::confRead(buffer, bufferSize, address);
        }
#endif

        if (version == -1) {
            return result;
        }

        if (result && checkBlock((const BlockHeader *)buffer, bufferSize, version)) {
            return true;
        }
    }

    return false;
}

bool confWrite(const uint8_t *buffer, uint16_t bufferSize, uint16_t address) {
    for (int i = 0; i < NUM_RETRIES; i++) {
        bool result = false;

        if (mcu::eeprom::g_testResult == TEST_OK) {
            result = mcu::eeprom::write(buffer, bufferSize, address);
        }
#if OPTION_SD_CARD
        else {
            result = sd_card::confWrite(buffer, bufferSize, address);
        }
#endif

        if (result) {
            uint8_t verifyBuffer[768];
            assert(sizeof(verifyBuffer) >= bufferSize);
            if (confRead(verifyBuffer, bufferSize, address, -1) && memcmp(buffer, verifyBuffer, bufferSize) == 0) {
                return true;
            }
        }
    }

    return false;
}

////////////////////////////////////////////////////////////////////////////////

bool moduleConfRead(int slotIndex, uint8_t *buffer, uint16_t bufferSize, uint16_t address, int version) {
    for (int i = 0; i < NUM_RETRIES; i++) {
        bool result = false;
        if (bp3c::eeprom::g_testResult == TEST_OK) {
            result = bp3c::eeprom::read(slotIndex, buffer, bufferSize, address);
        }

        if (version == -1) {
            return result;
        }

        if (result && checkBlock((const BlockHeader *)buffer, bufferSize, version)) {
            return true;
        }
    }

	return false;
}

bool moduleConfWrite(int slotIndex, const uint8_t *buffer, uint16_t bufferSize, uint16_t address) {
    for (int i = 0; i < NUM_RETRIES; i++) {
        bool result = false;

        if (bp3c::eeprom::g_testResult == TEST_OK) {
            return bp3c::eeprom::write(slotIndex, buffer, bufferSize, address);
        }

        if (result) {
            uint8_t verifyBuffer[512];
            assert(sizeof(verifyBuffer) >= bufferSize);
            if (moduleConfRead(slotIndex, verifyBuffer, bufferSize, address, -1) && memcmp(buffer, verifyBuffer, bufferSize) == 0) {
                return true;
            }
        }
    }

    return false;
}

////////////////////////////////////////////////////////////////////////////////

uint32_t calcChecksum(const BlockHeader *block, uint16_t size) {
    return crc32(((const uint8_t *)block) + sizeof(uint32_t), size - sizeof(uint32_t));
}

bool checkBlock(const BlockHeader *block, uint16_t size, uint16_t version) {
    return block->checksum == calcChecksum(block, size) && block->version <= version;
}

////////////////////////////////////////////////////////////////////////////////

bool save(BlockHeader *block, uint16_t size, uint16_t address, uint16_t version) {
    block->version = version;
    block->checksum = calcChecksum(block, size);
    return confWrite((const uint8_t *)block, size, address);
}

uint16_t getConfSectionAddress(PersistConfSection section) {
    switch (section) {
    case PERSIST_CONF_BLOCK_DEVICE:
        return PERSIST_CONF_DEVICE_ADDRESS;
    case PERSIST_CONF_BLOCK_DEVICE2:
        return PERSIST_CONF_DEVICE2_ADDRESS;
    case PERSIST_CONF_BLOCK_FIRST_PROFILE:
        return PERSIST_CONF_FIRST_PROFILE_ADDRESS;
    }
    return -1;
}

uint16_t getProfileAddress(int location) {
    return getConfSectionAddress(PERSIST_CONF_BLOCK_FIRST_PROFILE) + location * PERSIST_CONF_PROFILE_BLOCK_SIZE;
}

////////////////////////////////////////////////////////////////////////////////

uint16_t getModuleConfSectionAddress(ModulePersistConfSection section, int channelIndex) {
    switch (section) {
    case MODULE_PERSIST_CONF_BLOCK_MODULE_CONFIGURATION:
        return MODULE_PERSIST_CONF_BLOCK_MODULE_CONFIGURATION_ADDRESS;
    case MODULE_PERSIST_CONF_BLOCK_CH_CAL:
        return MODULE_PERSIST_CONF_CH_CAL_ADDRESS + channelIndex * MODULE_PERSIST_CONF_CH_CAL_BLOCK_SIZE;
    }
    return -1;
}

bool moduleSave(int slotIndex, BlockHeader *block, uint16_t size, uint16_t address, uint16_t version) {
    block->version = version;
    block->checksum = calcChecksum(block, size);
    return moduleConfWrite(slotIndex, (const uint8_t *)block, size, address);
}

////////////////////////////////////////////////////////////////////////////////

static void initDevice() {
    memset(&devConf, 0, sizeof(devConf));

    devConf.header.checksum = 0;
    devConf.header.version = DEV_CONF_VERSION;

    strcpy(devConf.serialNumber, PSU_SERIAL);

    strcpy(devConf.calibration_password, CALIBRATION_PASSWORD_DEFAULT);

    devConf.flags.isSoundEnabled = 1;
    devConf.flags.isClickSoundEnabled = 1;

    devConf.flags.dateValid = 0;
    devConf.flags.timeValid = 0;
    devConf.flags.dst = 0;

    devConf.time_zone = 0;

    devConf.flags.profileAutoRecallEnabled = 0;
    devConf.profile_auto_recall_location = 0;

    devConf.touch_screen_cal_orientation = -1;
    devConf.touch_screen_cal_tlx = 0;
    devConf.touch_screen_cal_tly = 0;
    devConf.touch_screen_cal_brx = 0;
    devConf.touch_screen_cal_bry = 0;
    devConf.touch_screen_cal_trx = 0;
    devConf.touch_screen_cal_try = 0;

    devConf.flags.channelsViewMode = 0;

#ifdef EEZ_PLATFORM_SIMULATOR
    devConf.flags.ethernetEnabled = 1;
#else
    devConf.flags.ethernetEnabled = 0;
#endif // EEZ_PLATFORM_SIMULATOR

    devConf.flags.outputProtectionCouple = 0;
    devConf.flags.shutdownWhenProtectionTripped = 0;
    devConf.flags.forceDisablingAllOutputsOnPowerUp = 0;
}

void loadDevice() {
    if (confRead((uint8_t *)&devConf, sizeof(DeviceConfiguration), getConfSectionAddress(PERSIST_CONF_BLOCK_DEVICE), DEV_CONF_VERSION)) {
        if (devConf.flags.channelsViewMode < 0 || devConf.flags.channelsViewMode >= NUM_CHANNELS_VIEW_MODES) {
            devConf.flags.channelsViewMode = 0;
        }

        if (devConf.flags.channelsViewModeInMax < 0 || devConf.flags.channelsViewModeInMax >= NUM_CHANNELS_VIEW_MODES_IN_MAX) {
            devConf.flags.channelsViewModeInMax = 0;
        }
    } else {
        initDevice();
    }
}

void saveDevice() {
    if (!save((BlockHeader *)&devConf, sizeof(DeviceConfiguration), getConfSectionAddress(PERSIST_CONF_BLOCK_DEVICE), DEV_CONF_VERSION)) {
        generateError(SCPI_ERROR_EXTERNAL_EEPROM_SAVE_FAILED);
    }
}

static void initEthernetSettings() {
    devConf2.flags.ethernetDhcpEnabled = 1;
    devConf2.ethernetIpAddress = getIpAddress(192, 168, 1, 100);
    devConf2.ethernetDns = getIpAddress(192, 168, 1, 1);
    devConf2.ethernetGateway = getIpAddress(192, 168, 1, 1);
    devConf2.ethernetSubnetMask = getIpAddress(255, 255, 255, 0);
    devConf2.ethernetScpiPort = TCP_PORT;
}

static void initDevice2() {
    memset(&devConf2, 0, sizeof(devConf2));
    devConf2.header.version = DEV_CONF2_VERSION;

    devConf2.flags.encoderConfirmationMode = 0;
    devConf2.flags.displayState = 1;

    devConf2.displayBrightness = DISPLAY_BRIGHTNESS_DEFAULT;

    devConf2.triggerSource = trigger::SOURCE_IMMEDIATE;
    devConf2.triggerDelay = 0;
    devConf2.flags.triggerContinuousInitializationEnabled = 0;

    devConf2.flags.serialEnabled = 1;
    devConf2.serialBaud = getIndexFromBaud(SERIAL_SPEED);
    devConf2.serialParity = serial::PARITY_NONE;

    initEthernetSettings();

    strcpy(devConf2.ntpServer, CONF_DEFAULT_NTP_SERVER);

    uint8_t macAddress[] = ETHERNET_MAC_ADDRESS;
    memcpy(devConf2.ethernetMacAddress, macAddress, 6);

    devConf2.dstRule = datetime::DST_RULE_OFF;

    devConf2.displayBackgroundLuminosityStep = DISPLAY_BACKGROUND_LUMINOSITY_STEP_DEFAULT;

    devConf2.animationsDuration = CONF_DEFAULT_ANIMATIONS_DURATION;

#if OPTION_DISPLAY
    onLuminocityChanged();
    onThemeChanged();
#endif
}

void loadDevice2() {
    if (!confRead((uint8_t *)&devConf2, sizeof(DeviceConfiguration2), getConfSectionAddress(PERSIST_CONF_BLOCK_DEVICE2), DEV_CONF2_VERSION)) {
        initDevice2();
    } else {
        if (devConf2.header.version < 9) {
            uint8_t macAddress[] = ETHERNET_MAC_ADDRESS;
            memcpy(devConf2.ethernetMacAddress, macAddress, 6);
        }

        if (devConf2.header.version < 10) {
            devConf2.displayBackgroundLuminosityStep = DISPLAY_BACKGROUND_LUMINOSITY_STEP_DEFAULT;
        }

        if (devConf2.header.version < 11) {
            devConf2.animationsDuration = CONF_DEFAULT_ANIMATIONS_DURATION;
        }

#if OPTION_DISPLAY
        onLuminocityChanged();
        onThemeChanged();
#endif
    }

    if (devConf2.serialBaud < 1 || devConf2.serialBaud > serial::g_baudsSize) {
        devConf2.serialBaud = getIndexFromBaud(SERIAL_SPEED);
    }

#if OPTION_ENCODER
    if (!devConf2.encoderMovingSpeedDown) {
        devConf2.encoderMovingSpeedDown = mcu::encoder::DEFAULT_MOVING_DOWN_SPEED;
    }
    if (!devConf2.encoderMovingSpeedUp) {
        devConf2.encoderMovingSpeedUp = mcu::encoder::DEFAULT_MOVING_UP_SPEED;
    }
#endif
}

void saveDevice2() {
    if (!save((BlockHeader *)&devConf2, sizeof(DeviceConfiguration2), getConfSectionAddress(PERSIST_CONF_BLOCK_DEVICE2), DEV_CONF2_VERSION)) {
        generateError(SCPI_ERROR_EXTERNAL_EEPROM_SAVE_FAILED);
    }
}

bool isSystemPasswordValid(const char *new_password, size_t new_password_len, int16_t &err) {
    if (new_password_len < PASSWORD_MIN_LENGTH) {
        err = SCPI_ERROR_PASSWORD_TOO_SHORT;
        return false;
    }

    if (new_password_len > PASSWORD_MAX_LENGTH) {
        err = SCPI_ERROR_PASSWORD_TOO_LONG;
        return false;
    }

    return true;
}

void changeSystemPassword(const char *new_password, size_t new_password_len) {
    memset(&devConf2.systemPassword, 0, sizeof(devConf2.systemPassword));
    strncpy(devConf2.systemPassword, new_password, new_password_len);
    saveDevice2();
    event_queue::pushEvent(event_queue::EVENT_INFO_SYSTEM_PASSWORD_CHANGED);
}

bool isCalibrationPasswordValid(const char *new_password, size_t new_password_len, int16_t &err) {
    if (new_password_len < PASSWORD_MIN_LENGTH) {
        err = SCPI_ERROR_PASSWORD_TOO_SHORT;
        return false;
    }

    if (new_password_len > PASSWORD_MAX_LENGTH) {
        err = SCPI_ERROR_PASSWORD_TOO_LONG;
        return false;
    }

    return true;
}

void changeCalibrationPassword(const char *new_password, size_t new_password_len) {
    memset(&devConf.calibration_password, 0, sizeof(devConf.calibration_password));
    strncpy(devConf.calibration_password, new_password, new_password_len);
    saveDevice();
    event_queue::pushEvent(event_queue::EVENT_INFO_CALIBRATION_PASSWORD_CHANGED);
}

void changeSerial(const char *newSerialNumber, size_t newSerialNumberLength) {
    // copy up to 7 characters from newSerialNumber, fill the rest with zero's
    for (size_t i = 0; i < 7; ++i) {
        if (i < newSerialNumberLength) {
            devConf.serialNumber[i] = newSerialNumber[i];
        } else {
            devConf.serialNumber[i] = 0;
        }
    }
    devConf.serialNumber[7] = 0;

    saveDevice();
}

void enableSound(bool enable) {
    devConf.flags.isSoundEnabled = enable ? 1 : 0;
    saveDevice();
    event_queue::pushEvent(enable ? event_queue::EVENT_INFO_SOUND_ENABLED : event_queue::EVENT_INFO_SOUND_DISABLED);
}

bool isSoundEnabled() {
    return devConf.flags.isSoundEnabled ? true : false;
}

void enableClickSound(bool enable) {
    devConf.flags.isClickSoundEnabled = enable ? 1 : 0;
    saveDevice();
}

bool isClickSoundEnabled() {
    return devConf.flags.isClickSoundEnabled ? true : false;
}

bool readSystemDate(uint8_t &year, uint8_t &month, uint8_t &day) {
    if (devConf.flags.dateValid) {
        year = devConf.date_year;
        month = devConf.date_month;
        day = devConf.date_day;
        return true;
    }
    return false;
}

bool isDst() {
    return datetime::isDst(datetime::makeTime(2000 + devConf.date_year, devConf.date_month,
                                              devConf.date_day, devConf.time_hour,
                                              devConf.time_minute, devConf.time_second),
                           (datetime::DstRule)devConf2.dstRule);
}

void setDst(unsigned dst) {
    if (dst == 0) {
        devConf.flags.dst = 0;
    } else if (dst == 1) {
        devConf.flags.dst = 1;
    } else {
        devConf.flags.dst = isDst();
    }
}

void writeSystemDate(uint8_t year, uint8_t month, uint8_t day, unsigned dst) {
    devConf.date_year = year;
    devConf.date_month = month;
    devConf.date_day = day;

    devConf.flags.dateValid = 1;

    setDst(dst);

    saveDevice();
}

bool readSystemTime(uint8_t &hour, uint8_t &minute, uint8_t &second) {
    if (devConf.flags.timeValid) {
        hour = devConf.time_hour;
        minute = devConf.time_minute;
        second = devConf.time_second;
        return true;
    }
    return false;
}

void writeSystemTime(uint8_t hour, uint8_t minute, uint8_t second, unsigned dst) {
    devConf.time_hour = hour;
    devConf.time_minute = minute;
    devConf.time_second = second;

    devConf.flags.timeValid = 1;

    setDst(dst);

    saveDevice();
}

void writeSystemDateTime(uint8_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t minute,
                         uint8_t second, unsigned dst) {
    devConf.date_year = year;
    devConf.date_month = month;
    devConf.date_day = day;

    devConf.flags.dateValid = 1;

    devConf.time_hour = hour;
    devConf.time_minute = minute;
    devConf.time_second = second;

    devConf.flags.timeValid = 1;

    setDst(dst);

    saveDevice();
}

void enableProfileAutoRecall(bool enable) {
    devConf.flags.profileAutoRecallEnabled = enable ? 1 : 0;
    saveDevice();
}

bool isProfileAutoRecallEnabled() {
    return devConf.flags.profileAutoRecallEnabled ? true : false;
}

void setProfileAutoRecallLocation(int location) {
    devConf.profile_auto_recall_location = (int8_t)location;
    saveDevice();
    event_queue::pushEvent(event_queue::EVENT_INFO_DEFAULE_PROFILE_CHANGED_TO_0 + location);
    if (location == 0) {
        profile::save();
    }
}

int getProfileAutoRecallLocation() {
    return devConf.profile_auto_recall_location;
}

void setChannelsViewMode(unsigned int channelsViewMode) {
    uint8_t ytGraphUpdateMethod = devConf2.ytGraphUpdateMethod;

    if (channelsViewMode == CHANNELS_VIEW_MODE_YT) {
        ytGraphUpdateMethod = YT_GRAPH_UPDATE_METHOD_SCROLL;
    } else if (channelsViewMode == CHANNELS_VIEW_MODE_YT + 1) {
        if (ytGraphUpdateMethod == YT_GRAPH_UPDATE_METHOD_SCROLL) {
            channelsViewMode = CHANNELS_VIEW_MODE_YT;
            ytGraphUpdateMethod = YT_GRAPH_UPDATE_METHOD_SCAN_LINE;
        } else {
            channelsViewMode = CHANNELS_VIEW_MODE_NUMERIC;
        }
    }

    if (channelsViewMode != devConf.flags.channelsViewMode) {
        devConf.flags.channelsViewMode = channelsViewMode;
        saveDevice();
    }

    if (ytGraphUpdateMethod != devConf2.ytGraphUpdateMethod) {
        devConf2.ytGraphUpdateMethod = ytGraphUpdateMethod;
        saveDevice2();
    }
}

unsigned int getChannelsViewMode() {
    if (devConf.flags.channelsViewMode == CHANNELS_VIEW_MODE_YT && devConf2.ytGraphUpdateMethod == YT_GRAPH_UPDATE_METHOD_SCAN_LINE) {
        return CHANNELS_VIEW_MODE_YT + 1;
    }
    return devConf.flags.channelsViewMode;
}

void setChannelsViewModeInMax(unsigned int channelsViewModeInMax) {
    uint8_t ytGraphUpdateMethod = devConf2.ytGraphUpdateMethod;

    if (channelsViewModeInMax == CHANNELS_VIEW_MODE_IN_MAX_YT) {
        ytGraphUpdateMethod = YT_GRAPH_UPDATE_METHOD_SCROLL;
    } else if (channelsViewModeInMax == CHANNELS_VIEW_MODE_IN_MAX_YT + 1) {
        if (ytGraphUpdateMethod == YT_GRAPH_UPDATE_METHOD_SCROLL) {
            channelsViewModeInMax = CHANNELS_VIEW_MODE_IN_MAX_YT;
            ytGraphUpdateMethod = YT_GRAPH_UPDATE_METHOD_SCAN_LINE;
        } else {
            channelsViewModeInMax = CHANNELS_VIEW_MODE_IN_MAX_NUMERIC;
        }
    }

    if (channelsViewModeInMax != devConf.flags.channelsViewModeInMax) {
        devConf.flags.channelsViewModeInMax = channelsViewModeInMax;
        saveDevice();
    }

    if (ytGraphUpdateMethod != devConf2.ytGraphUpdateMethod) {
        devConf2.ytGraphUpdateMethod = ytGraphUpdateMethod;
        saveDevice2();
    }
}

void toggleChannelsViewMode() {
    if (devConf.flags.channelsIsMaxView) {
        setChannelsViewModeInMax(devConf.flags.channelsViewModeInMax + 1);
    } else {
        setChannelsViewMode(devConf.flags.channelsViewMode + 1);
    }
}

void setSlotMin() {
    if (devConf.flags.slotMax == 1) {
        devConf.flags.slotMin1 = 2;
        devConf.flags.slotMin2 = 3;
    } else if (devConf.flags.slotMax == 2) {
        devConf.flags.slotMin1 = 1;
        devConf.flags.slotMin2 = 3;
    } else {
        devConf.flags.slotMin1 = 1;
        devConf.flags.slotMin2 = 2;
    }
}

void setChannelsMaxView(int slotIndex) {
    auto channelsIsMaxView = devConf.flags.channelsIsMaxView;
    auto slotMax = devConf.flags.slotMax;

    devConf.flags.channelsIsMaxView = 1;
    devConf.flags.slotMax = slotIndex;
    setSlotMin();

    if (channelsIsMaxView != devConf.flags.channelsIsMaxView || slotMax != devConf.flags.slotMax) {
        saveDevice();
    }
}

void toggleChannelsMaxView(int slotIndex) {
    if (devConf.flags.channelsIsMaxView) {
        if (slotIndex == devConf.flags.slotMax) {
            devConf.flags.channelsIsMaxView = 0;
        } else {
            devConf.flags.slotMax = slotIndex;
        }
    } else {
        devConf.flags.channelsIsMaxView = 1;
        devConf.flags.slotMax = slotIndex;
    }
    
    if (devConf.flags.channelsIsMaxView) {
        setSlotMin();
    }

    saveDevice();
}

////////////////////////////////////////////////////////////////////////////////

static struct {
    bool loaded;
    profile::Parameters profile;
} g_profilesCache[NUM_PROFILE_LOCATIONS];

profile::Parameters *loadProfile(int location) {
    assert(location < NUM_PROFILE_LOCATIONS && sizeof(profile::Parameters) <= PERSIST_CONF_PROFILE_BLOCK_SIZE);
    
    if (!g_profilesCache[location].loaded) {
        profile::Parameters profile;
        if (confRead((uint8_t *)&profile, sizeof(profile::Parameters), getProfileAddress(location), profile::PROFILE_VERSION)) {
           memcpy(&g_profilesCache[location].profile, &profile, sizeof(profile::Parameters));
        }
        g_profilesCache[location].loaded = true;
    }

    return &g_profilesCache[location].profile;
}

bool saveProfile(int location, profile::Parameters *profile) {
    if (!save((BlockHeader *)profile, sizeof(profile::Parameters), getProfileAddress(location), profile::PROFILE_VERSION)) {
        return false;
    }

    memcpy(&g_profilesCache[location].profile, profile, sizeof(profile::Parameters));
    g_profilesCache[location].loaded = true;

    return true;
}

////////////////////////////////////////////////////////////////////////////////

uint32_t readTotalOnTime(int type) {
    uint32_t buffer[6];

    bool result;

    for (int i = 0; i < NUM_RETRIES; i++) {
        if (type == ontime::ON_TIME_COUNTER_MCU) {
            result = confRead((uint8_t *)buffer, sizeof(buffer), mcu::eeprom::EEPROM_ONTIME_START_ADDRESS, -1);
        } else {
            result = moduleConfRead(
                type - ontime::ON_TIME_COUNTER_SLOT1,
                (uint8_t *)buffer, 
                sizeof(buffer), 
                bp3c::eeprom::EEPROM_ONTIME_START_ADDRESS,
                -1
            );
        }

        // is counter on first location valid?
        if (result && buffer[0] == ONTIME_MAGIC && buffer[2] == crc32((uint8_t *)(buffer + 0), 8)) {
            // is counter on second location valid?
            if (buffer[3] == ONTIME_MAGIC && buffer[5] == crc32((uint8_t *)(buffer + 3), 8)) {
                // at which location time is bigger?
                return buffer[1] > buffer[4] ? buffer[1] : buffer[4];
            }

            // only first location is valid
            return buffer[1];
        } 
        
        if (buffer[3] == ONTIME_MAGIC && buffer[5] == crc32((uint8_t *)(buffer + 3), 8)) {
            // only second location is valid
            return buffer[4];
        }
    }

    // no valid time stored
    return 0;
}

bool writeTotalOnTime(int type, uint32_t time) {
    uint32_t buffer[6];

    // store time at two locations for extra robustness
    buffer[0] = ONTIME_MAGIC;
    buffer[1] = time;
    buffer[2] = crc32((uint8_t *)(buffer + 0), 8);

    buffer[3] = ONTIME_MAGIC;
    buffer[4] = time;
    buffer[5] = crc32((uint8_t *)(buffer + 3), 8);

    if (type == ontime::ON_TIME_COUNTER_MCU) {
        return confWrite((uint8_t *)buffer, sizeof(buffer), mcu::eeprom::EEPROM_ONTIME_START_ADDRESS);
    } else {
        return moduleConfWrite(
            type - ontime::ON_TIME_COUNTER_SLOT1,
            (uint8_t *)buffer, 
            sizeof(buffer), 
            bp3c::eeprom::EEPROM_ONTIME_START_ADDRESS
        );
    }
}

void enableOutputProtectionCouple(bool enable) {
    int outputProtectionCouple = enable ? 1 : 0;

    if (devConf.flags.outputProtectionCouple == outputProtectionCouple) {
        return;
    }

    devConf.flags.outputProtectionCouple = outputProtectionCouple;

    saveDevice();

    if (devConf.flags.outputProtectionCouple) {
        event_queue::pushEvent(event_queue::EVENT_INFO_OUTPUT_PROTECTION_COUPLED);
    } else {
        event_queue::pushEvent(event_queue::EVENT_INFO_OUTPUT_PROTECTION_DECOUPLED);
    }
}

bool isOutputProtectionCoupleEnabled() {
    return devConf.flags.outputProtectionCouple ? true : false;
}

void enableShutdownWhenProtectionTripped(bool enable) {
    int shutdownWhenProtectionTripped = enable ? 1 : 0;

    if (devConf.flags.shutdownWhenProtectionTripped == shutdownWhenProtectionTripped) {
        return;
    }

    devConf.flags.shutdownWhenProtectionTripped = shutdownWhenProtectionTripped;

    saveDevice();

    if (devConf.flags.shutdownWhenProtectionTripped) {
        event_queue::pushEvent(
            event_queue::EVENT_INFO_SHUTDOWN_WHEN_PROTECTION_TRIPPED_ENABLED);
    } else {
        event_queue::pushEvent(
            event_queue::EVENT_INFO_SHUTDOWN_WHEN_PROTECTION_TRIPPED_DISABLED);
    }
}

bool isShutdownWhenProtectionTrippedEnabled() {
    return devConf.flags.shutdownWhenProtectionTripped ? true : false;
}

void enableForceDisablingAllOutputsOnPowerUp(bool enable) {
    int forceDisablingAllOutputsOnPowerUp = enable ? 1 : 0;

    if (devConf.flags.forceDisablingAllOutputsOnPowerUp == forceDisablingAllOutputsOnPowerUp) {
        return;
    }

    devConf.flags.forceDisablingAllOutputsOnPowerUp = forceDisablingAllOutputsOnPowerUp;

    saveDevice();

    if (devConf.flags.forceDisablingAllOutputsOnPowerUp) {
        event_queue::pushEvent(
            event_queue::EVENT_INFO_FORCE_DISABLING_ALL_OUTPUTS_ON_POWERUP_ENABLED);
    } else {
        event_queue::pushEvent(
            event_queue::EVENT_INFO_FORCE_DISABLING_ALL_OUTPUTS_ON_POWERUP_DISABLED);
    }
}

bool isForceDisablingAllOutputsOnPowerUpEnabled() {
    return devConf.flags.forceDisablingAllOutputsOnPowerUp ? true : false;
}

void lockFrontPanel(bool lock) {
    g_rlState = lock ? RL_STATE_REMOTE : RL_STATE_LOCAL;

    int isFrontPanelLocked = lock ? 1 : 0;

    if (devConf.flags.isFrontPanelLocked == isFrontPanelLocked) {
        return;
    }

    devConf.flags.isFrontPanelLocked = isFrontPanelLocked;

    saveDevice();

    if (devConf.flags.isFrontPanelLocked) {
        event_queue::pushEvent(event_queue::EVENT_INFO_FRONT_PANEL_LOCKED);
    } else {
        event_queue::pushEvent(event_queue::EVENT_INFO_FRONT_PANEL_UNLOCKED);
    }
}

void setEncoderSettings(uint8_t confirmationMode, uint8_t movingSpeedDown, uint8_t movingSpeedUp) {
    devConf2.flags.encoderConfirmationMode = confirmationMode;
    devConf2.encoderMovingSpeedDown = movingSpeedDown;
    devConf2.encoderMovingSpeedUp = movingSpeedUp;

    saveDevice2();
}

void setDisplayState(unsigned newState) {
    unsigned currentDisplayState = devConf2.flags.displayState;

    if (currentDisplayState != newState) {
        devConf2.flags.displayState = newState;
        saveDevice2();
    }
}

void setDisplayBrightness(uint8_t displayBrightness) {
    devConf2.displayBrightness = displayBrightness;

#if OPTION_DISPLAY
    updateBrightness();
#endif

    saveDevice2();
}

void setDisplayBackgroundLuminosityStep(uint8_t displayBackgroundLuminosityStep) {
    devConf2.displayBackgroundLuminosityStep = displayBackgroundLuminosityStep;

#if OPTION_DISPLAY
    onLuminocityChanged();
    refreshScreen();
#endif

    saveDevice2();
}

bool enableSerial(bool enable) {
    unsigned serialEnabled = enable ? 1 : 0;
    if (!devConf2.flags.skipSerialSetup || devConf2.flags.serialEnabled != serialEnabled) {
        devConf2.flags.serialEnabled = serialEnabled;
        devConf2.flags.skipSerialSetup = 1;
        saveDevice2();
        serial::update();
    }
    return true;
}

bool isSerialEnabled() {
    return devConf2.flags.serialEnabled ? true : false;
}

int getIndexFromBaud(long baud) {
    for (size_t i = 0; i < serial::g_baudsSize; ++i) {
        if (serial::g_bauds[i] == baud) {
            return i + 1;
        }
    }
    return 0;
}

long getBaudFromIndex(int index) {
    return serial::g_bauds[index - 1];
}

int getSerialBaudIndex() {
    return devConf2.serialBaud;
}

bool setSerialBaudIndex(int baudIndex) {
    uint8_t serialBaud = (uint8_t)baudIndex;
    if (!devConf2.flags.skipSerialSetup || devConf2.serialBaud != serialBaud) {
        devConf2.serialBaud = serialBaud;
        devConf2.flags.skipSerialSetup = 1;
        saveDevice2();
        serial::update();
    }
    return true;
}

int getSerialParity() {
    return devConf2.serialParity;
}

bool setSerialParity(int parity) {
    unsigned serialParity = (unsigned)parity;
    if (!devConf2.flags.skipSerialSetup || devConf2.serialParity != serialParity) {
        devConf2.serialParity = serialParity;
        devConf2.flags.skipSerialSetup = 1;
        saveDevice2();
        serial::update();
    }
    return true;
}

bool setSerialSettings(bool enabled, int baudIndex, int parity) {
    unsigned serialEnabled = enabled ? 1 : 0;
    uint8_t serialBaud = (uint8_t)baudIndex;
    unsigned serialParity = (unsigned)parity;
    if (!devConf2.flags.skipSerialSetup || devConf2.flags.serialEnabled != serialEnabled ||
        devConf2.serialBaud != serialBaud || devConf2.serialParity != serialParity) {
        devConf2.flags.serialEnabled = enabled;
        devConf2.serialBaud = serialBaud;
        devConf2.serialParity = serialParity;
        devConf2.flags.skipSerialSetup = 1;
        saveDevice2();
        serial::update();
    }
    return true;
}

bool enableEthernet(bool enable) {
#if OPTION_ETHERNET
    unsigned ethernetEnabled = enable ? 1 : 0;
    if (!devConf2.flags.skipEthernetSetup || devConf.flags.ethernetEnabled != ethernetEnabled) {
        devConf.flags.ethernetEnabled = ethernetEnabled;
        saveDevice();
        devConf2.flags.skipEthernetSetup = 1;
        saveDevice2();
        event_queue::pushEvent(enable ? event_queue::EVENT_INFO_ETHERNET_ENABLED
                                      : event_queue::EVENT_INFO_ETHERNET_DISABLED);
        ethernet::update();
    }
    return true;
#else
    return false;
#endif
}

bool isEthernetEnabled() {
    return devConf.flags.ethernetEnabled ? true : false;
}

bool enableEthernetDhcp(bool enable) {
#if OPTION_ETHERNET
    unsigned ethernetDhcpEnabled = enable ? 1 : 0;
    if (!devConf2.flags.skipEthernetSetup ||
        devConf2.flags.ethernetDhcpEnabled != ethernetDhcpEnabled) {
        devConf2.flags.ethernetDhcpEnabled = ethernetDhcpEnabled;
        devConf2.flags.skipEthernetSetup = 1;
        saveDevice2();
        ethernet::update();
    }
    return true;
#else
    return false;
#endif
}

bool isEthernetDhcpEnabled() {
    return devConf2.flags.ethernetDhcpEnabled ? true : false;
}

bool setEthernetMacAddress(uint8_t macAddress[]) {
#if OPTION_ETHERNET
    if (!devConf2.flags.skipEthernetSetup ||
        memcmp(devConf2.ethernetMacAddress, macAddress, 6) != 0) {
        memcpy(devConf2.ethernetMacAddress, macAddress, 6);
        devConf2.flags.skipEthernetSetup = 1;
        saveDevice2();
        ethernet::update();
    }
    return true;
#else
    return false;
#endif
}

bool setEthernetIpAddress(uint32_t ipAddress) {
#if OPTION_ETHERNET
    if (!devConf2.flags.skipEthernetSetup || devConf2.ethernetIpAddress != ipAddress) {
        devConf2.ethernetIpAddress = ipAddress;
        devConf2.flags.skipEthernetSetup = 1;
        saveDevice2();
        ethernet::update();
    }
    return true;
#else
    return false;
#endif
}

bool setEthernetDns(uint32_t dns) {
#if OPTION_ETHERNET
    if (!devConf2.flags.skipEthernetSetup || devConf2.ethernetDns != dns) {
        devConf2.ethernetDns = dns;
        devConf2.flags.skipEthernetSetup = 1;
        saveDevice2();
        ethernet::update();
    }
    return true;
#else
    return false;
#endif
}

bool setEthernetGateway(uint32_t gateway) {
#if OPTION_ETHERNET
    if (!devConf2.flags.skipEthernetSetup || devConf2.ethernetGateway != gateway) {
        devConf2.ethernetGateway = gateway;
        devConf2.flags.skipEthernetSetup = 1;
        saveDevice2();
        ethernet::update();
    }
    return true;
#else
    return false;
#endif
}

bool setEthernetSubnetMask(uint32_t subnetMask) {
#if OPTION_ETHERNET
    if (!devConf2.flags.skipEthernetSetup || devConf2.ethernetSubnetMask != subnetMask) {
        devConf2.ethernetSubnetMask = subnetMask;
        devConf2.flags.skipEthernetSetup = 1;
        saveDevice2();
        ethernet::update();
    }
    return true;
#else
    return false;
#endif
}

bool setEthernetScpiPort(uint16_t scpiPort) {
#if OPTION_ETHERNET
    if (!devConf2.flags.skipEthernetSetup || devConf2.ethernetScpiPort != scpiPort) {
        devConf2.ethernetScpiPort = scpiPort;
        devConf2.flags.skipEthernetSetup = 1;
        saveDevice2();
        ethernet::update();
    }
    return true;
#else
    return false;
#endif
}

bool setEthernetSettings(bool enable, bool dhcpEnable, uint32_t ipAddress, uint32_t dns,
                         uint32_t gateway, uint32_t subnetMask, uint16_t scpiPort,
                         uint8_t *macAddress) {
#if OPTION_ETHERNET
    unsigned ethernetEnabled = enable ? 1 : 0;
    unsigned ethernetDhcpEnabled = dhcpEnable ? 1 : 0;

    if (!devConf2.flags.skipEthernetSetup || devConf.flags.ethernetEnabled != ethernetEnabled ||
        devConf2.flags.ethernetDhcpEnabled != ethernetDhcpEnabled ||
        memcmp(devConf2.ethernetMacAddress, macAddress, 6) != 0 ||
        devConf2.ethernetIpAddress != ipAddress || devConf2.ethernetDns != dns ||
        devConf2.ethernetGateway != gateway || devConf2.ethernetSubnetMask != subnetMask ||
        devConf2.ethernetScpiPort != scpiPort) {

        if (devConf.flags.ethernetEnabled != ethernetEnabled) {
            devConf.flags.ethernetEnabled = ethernetEnabled;
            saveDevice();
            event_queue::pushEvent(devConf.flags.ethernetEnabled
                                       ? event_queue::EVENT_INFO_ETHERNET_ENABLED
                                       : event_queue::EVENT_INFO_ETHERNET_DISABLED);
        }

        devConf2.flags.ethernetDhcpEnabled = ethernetDhcpEnabled;
        memcpy(devConf2.ethernetMacAddress, macAddress, 6);
        devConf2.ethernetIpAddress = ipAddress;
        devConf2.ethernetDns = dns;
        devConf2.ethernetGateway = gateway;
        devConf2.ethernetSubnetMask = subnetMask;
        devConf2.ethernetScpiPort = scpiPort;
        devConf2.flags.skipEthernetSetup = 1;
        saveDevice2();

        ethernet::update();
    }

    return true;
#else
    return false;
#endif
}

void enableNtp(bool enable) {
    devConf2.flags.ntpEnabled = enable ? 1 : 0;
    saveDevice2();
}

bool isNtpEnabled() {
    return devConf2.flags.ntpEnabled ? true : false;
}

void setNtpServer(const char *ntpServer, size_t ntpServerLength) {
    strncpy(devConf2.ntpServer, ntpServer, ntpServerLength);
    devConf2.ntpServer[ntpServerLength] = 0;
    saveDevice2();
}

void setNtpSettings(bool enable, const char *ntpServer) {
    devConf2.flags.ntpEnabled = enable ? 1 : 0;
    strcpy(devConf2.ntpServer, ntpServer);
    saveDevice2();
}

void setSdLocked(bool sdLocked) {
    devConf2.flags.sdLocked = sdLocked ? 1 : 0;
    saveDevice2();
}

bool isSdLocked() {
    return devConf2.flags.sdLocked ? true : false;
}

void setAnimationsDuration(float value) {
    devConf2.animationsDuration = value;
    saveDevice2();
}

////////////////////////////////////////////////////////////////////////////////

ModuleConfiguration g_moduleConf[NUM_SLOTS];

static void initModuleConf(int slotIndex) {
    ModuleConfiguration &moduleConf = g_moduleConf[slotIndex];

    memset(&moduleConf, 0, sizeof(ModuleConfiguration));

    moduleConf.header.checksum = 0;
    moduleConf.header.version = MODULE_CONF_VERSION;

    moduleConf.chCalEnabled = 0xFF;
}

void loadModuleConf(int slotIndex) {
    assert(sizeof(ModuleConfiguration) <= MODULE_PERSIST_CONF_BLOCK_MODULE_CONFIGURATION_SIZE);

    uint8_t buffer[MODULE_PERSIST_CONF_BLOCK_MODULE_CONFIGURATION_SIZE];

    if (moduleConfRead(
        slotIndex, 
        buffer, 
        MODULE_PERSIST_CONF_BLOCK_MODULE_CONFIGURATION_SIZE, 
        getModuleConfSectionAddress(MODULE_PERSIST_CONF_BLOCK_MODULE_CONFIGURATION, -1),
        MODULE_CONF_VERSION
    )) {
        ModuleConfiguration &moduleConf = g_moduleConf[slotIndex];
        memcpy(&moduleConf, buffer, sizeof(ModuleConfiguration));
    } else {
    	initModuleConf(slotIndex);
    }
}

bool saveModuleConf(int slotIndex) {
    uint8_t buffer[MODULE_PERSIST_CONF_BLOCK_MODULE_CONFIGURATION_SIZE];

    ModuleConfiguration &moduleConf = g_moduleConf[slotIndex];
    memcpy(&moduleConf, buffer, sizeof(ModuleConfiguration));

    memset(buffer + sizeof(ModuleConfiguration), 0, MODULE_PERSIST_CONF_BLOCK_MODULE_CONFIGURATION_SIZE - sizeof(ModuleConfiguration));

    return moduleSave(
        slotIndex, 
        (BlockHeader *)buffer, 
        MODULE_PERSIST_CONF_BLOCK_MODULE_CONFIGURATION_SIZE, 
        getModuleConfSectionAddress(MODULE_PERSIST_CONF_BLOCK_MODULE_CONFIGURATION, -1),
        MODULE_CONF_VERSION
    );
}

bool isChannelCalibrationEnabled(Channel &channel) {
    ModuleConfiguration &moduleConf = g_moduleConf[channel.slotIndex];
    return moduleConf.chCalEnabled & (1 << channel.subchannelIndex);
}

void saveCalibrationEnabledFlag(Channel &channel, bool enabled) {
    ModuleConfiguration &moduleConf = g_moduleConf[channel.slotIndex];
    if (enabled) {
        moduleConf.chCalEnabled |= 1 << channel.subchannelIndex;
    } else {
        moduleConf.chCalEnabled &= ~(1 << channel.subchannelIndex);
    }
    saveDevice();
}

void loadChannelCalibration(Channel &channel) {
    if (!moduleConfRead(
        channel.slotIndex,
        (uint8_t *)&channel.cal_conf,
        sizeof(Channel::CalibrationConfiguration),
        getModuleConfSectionAddress(MODULE_PERSIST_CONF_BLOCK_CH_CAL, channel.subchannelIndex),
        CH_CAL_CONF_VERSION
    )) {
        channel.clearCalibrationConf();
    }
}

bool saveChannelCalibration(Channel &channel) {
    return moduleSave(
        channel.slotIndex,
        (BlockHeader *)&channel.cal_conf,
        sizeof(Channel::CalibrationConfiguration),
        getModuleConfSectionAddress(MODULE_PERSIST_CONF_BLOCK_CH_CAL, channel.subchannelIndex), 
        CH_CAL_CONF_VERSION
    );
}

} // namespace persist_conf
} // namespace psu
} // namespace eez

extern "C" void getMacAddress(uint8_t macAddress[]) {
    memcpy(macAddress, eez::psu::persist_conf::devConf2.ethernetMacAddress, 6);
}
