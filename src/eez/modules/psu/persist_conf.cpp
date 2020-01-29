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

#include <eez/system.h>

#include <eez/modules/psu/psu.h>

#include <eez/modules/mcu/eeprom.h>
#include <eez/modules/bp3c/eeprom.h>

#include <eez/modules/psu/event_queue.h>
#include <eez/modules/psu/profile.h>
#include <eez/modules/psu/serial_psu.h>

#if OPTION_ENCODER
#include <eez/modules/mcu/encoder.h>
#endif

#if OPTION_DISPLAY
#include <eez/modules/psu/gui/psu.h>
#include <eez/modules/mcu/display.h>
using namespace eez::mcu::display;
#endif

#if OPTION_ETHERNET
#include <eez/modules/psu/ethernet.h>
#include <eez/mqtt.h>
#endif

#include <eez/modules/psu/datetime.h>
#include <eez/modules/psu/trigger.h>
#include <eez/modules/psu/ontime.h>
#include <eez/modules/psu/io_pins.h>

#include <eez/gui/widgets/yt_graph.h>

#define NUM_RETRIES 3

namespace eez {
namespace psu {
namespace persist_conf {

#define CONF_MAX_NUMBER_OF_SAVE_ERRORS_ALLOWED 2

////////////////////////////////////////////////////////////////////////////////

static const uint16_t MODULE_CONF_VERSION = 1;
static const uint16_t CH_CAL_CONF_VERSION = 3;

static const uint16_t PERSIST_CONF_DEV_CONF_ADDRESS = 128;
static const uint16_t PERSIST_CONF_FIRST_PROFILE_ADDRESS = 5120;
static const uint16_t PERSIST_CONF_PROFILE_BLOCK_SIZE = 1024;

static const uint32_t ONTIME_MAGIC = 0xA7F31B3CL;

////////////////////////////////////////////////////////////////////////////////

DeviceConfiguration g_devConf;
const DeviceConfiguration &devConf = g_devConf;
DeviceConfiguration g_defaultDevConf;
DeviceConfiguration g_savedDevConf;

struct DevConfBlock {
    uint16_t end;
    uint16_t version;
    bool dirty;
    unsigned numSaveErrors;
    uint32_t minTickCountsBetweenSaves;
    uint32_t lastSaveTickCount;
};

static DevConfBlock g_devConfBlocks[] = {
    { offsetof(DeviceConfiguration, date_year), 1, false, 0, 0, 0 },
    { offsetof(DeviceConfiguration, profile_auto_recall_location), 1, false, 0, 0, 0 },
    { offsetof(DeviceConfiguration, serialBaud), 1, false, 0, 0, 0 },
    { offsetof(DeviceConfiguration, triggerSource), 1, false, 0, 0, 0 },
    { offsetof(DeviceConfiguration, ytGraphUpdateMethod), 1, false, 0, 0, 0 },
    { offsetof(DeviceConfiguration, userSwitchAction), 1, false, 0, 60 * 1000, 0 },
    { offsetof(DeviceConfiguration, ethernetHostName), 1, false, 0, 0, 0 },
    { sizeof(DeviceConfiguration), 1, false, 0, 0, 0 },
};

static struct {
    bool loaded;
    profile::Parameters profile;
    bool dirty;
    unsigned numSaveErrors;
} g_profilesCache[NUM_PROFILE_LOCATIONS];

////////////////////////////////////////////////////////////////////////////////

void initDefaultDevConf() {
    memset(&g_defaultDevConf, 0, sizeof(g_defaultDevConf));

    // block 1
    strcpy(g_defaultDevConf.calibration_password, CALIBRATION_PASSWORD_DEFAULT);

    g_defaultDevConf.touch_screen_cal_tlx = 0;
    g_defaultDevConf.touch_screen_cal_tly = 0;
    g_defaultDevConf.touch_screen_cal_brx = 0;
    g_defaultDevConf.touch_screen_cal_bry = 0;
    g_defaultDevConf.touch_screen_cal_trx = 0;
    g_defaultDevConf.touch_screen_cal_try = 0;

    // block2
    g_defaultDevConf.dateValid = 0;
    g_defaultDevConf.timeValid = 0;
    g_defaultDevConf.dst = 0;

    g_defaultDevConf.time_zone = 0;
    g_defaultDevConf.dstRule = datetime::DST_RULE_OFF;

    // block 3
    g_defaultDevConf.profileAutoRecallEnabled = 0;

    g_defaultDevConf.outputProtectionCouple = 0;
    g_defaultDevConf.shutdownWhenProtectionTripped = 0;
    g_defaultDevConf.forceDisablingAllOutputsOnPowerUp = 0;

    g_defaultDevConf.profile_auto_recall_location = 0;

    // block 4
    g_defaultDevConf.serialEnabled = 1;

#ifdef EEZ_PLATFORM_SIMULATOR
    g_defaultDevConf.ethernetEnabled = 1;
#else
    g_defaultDevConf.ethernetEnabled = 0;
#endif
    g_defaultDevConf.ethernetDhcpEnabled = 1;

    g_defaultDevConf.encoderConfirmationMode = 0;

    g_defaultDevConf.isSoundEnabled = 1;
    g_defaultDevConf.isClickSoundEnabled = 0;

    g_defaultDevConf.serialBaud = getIndexFromBaud(SERIAL_SPEED);
    g_defaultDevConf.serialParity = serial::PARITY_NONE;

    g_defaultDevConf.ethernetIpAddress = getIpAddress(192, 168, 1, 100);
    g_defaultDevConf.ethernetDns = getIpAddress(192, 168, 1, 1);
    g_defaultDevConf.ethernetGateway = getIpAddress(192, 168, 1, 1);
    g_defaultDevConf.ethernetSubnetMask = getIpAddress(255, 255, 255, 0);
    g_defaultDevConf.ethernetScpiPort = TCP_PORT;
    strcpy(g_defaultDevConf.ntpServer, CONF_DEFAULT_NTP_SERVER);
    uint8_t macAddress[] = ETHERNET_MAC_ADDRESS;
    memcpy(g_defaultDevConf.ethernetMacAddress, macAddress, 6);

    g_defaultDevConf.displayBrightness = DISPLAY_BRIGHTNESS_DEFAULT;
    g_defaultDevConf.displayBackgroundLuminosityStep = DISPLAY_BACKGROUND_LUMINOSITY_STEP_DEFAULT;
    g_defaultDevConf.selectedThemeIndex = THEME_ID_DARK;
    g_defaultDevConf.animationsDuration = CONF_DEFAULT_ANIMATIONS_DURATION;

    // block 5
    g_defaultDevConf.triggerContinuousInitializationEnabled = 0;

    g_defaultDevConf.triggerSource = trigger::SOURCE_IMMEDIATE;
    g_defaultDevConf.triggerDelay = 0;

    // block 6
    g_defaultDevConf.displayState = 1;
    g_defaultDevConf.channelsViewMode = 0;
    g_defaultDevConf.maxChannel = 0;
    g_defaultDevConf.channelsViewModeInMax = 0;

    g_defaultDevConf.ytGraphUpdateMethod = YT_GRAPH_UPDATE_METHOD_SCROLL;

    // block 7
    g_defaultDevConf.userSwitchAction = USER_SWITCH_ACTION_ENCODER_STEP;
    g_defaultDevConf.sortFilesOption = SORT_FILES_BY_TIME_DESC;

    // block 7
    strcpy(g_defaultDevConf.ethernetHostName, DEFAULT_ETHERNET_HOST_NAME);
    
    g_defaultDevConf.mqttEnabled = 0;
    g_defaultDevConf.mqttPort = 1883;
    g_defaultDevConf.mqttPeriod = 1.0f;
};

////////////////////////////////////////////////////////////////////////////////

static bool confRead(uint8_t *buffer, uint16_t bufferSize, uint16_t address, int version) {
	if (bp3c::eeprom::g_testResult != TEST_OK) {
		return false;
    }

    for (int i = 0; i < NUM_RETRIES; i++) {
        bool result = mcu::eeprom::read(buffer, bufferSize, address);

        if (!result) {
        	pushEvent(event_queue::EVENT_ERROR_EEPROM_MCU_READ_ERROR);
        	continue;
        }

        if (version == -1) {
            return result;
        }

        if (!checkBlock((const BlockHeader *)buffer, bufferSize, version)) {
            continue;
        }

        return true;
    }

    return false;
}

static bool confWrite(const uint8_t *buffer, uint16_t bufferSize, uint16_t address) {
	if (bp3c::eeprom::g_testResult != TEST_OK) {
		return false;
    }

    for (int i = 0; i < NUM_RETRIES; i++) {
        bool result = mcu::eeprom::write(buffer, bufferSize, address);

        if (!result) {
        	pushEvent(event_queue::EVENT_ERROR_EEPROM_MCU_WRITE_ERROR);
        	continue;
        }

		uint8_t verifyBuffer[768];
		assert(sizeof(verifyBuffer) >= bufferSize);
		if (!confRead(verifyBuffer, bufferSize, address, -1)) {
			continue;
		}
		if (memcmp(buffer, verifyBuffer, bufferSize) != 0) {
        	pushEvent(event_queue::EVENT_ERROR_EEPROM_MCU_WRITE_VERIFY_ERROR);
			continue;
		}

		return true;
    }

    return false;
}

////////////////////////////////////////////////////////////////////////////////

static bool moduleConfRead(int slotIndex, uint8_t *buffer, uint16_t bufferSize, uint16_t address, int version) {
	if (bp3c::eeprom::g_testResult != TEST_OK) {
		return false;
    }

	for (int i = 0; i < NUM_RETRIES; i++) {
        bool result = bp3c::eeprom::read(slotIndex, buffer, bufferSize, address);

        if (!result) {
#if defined(EEZ_PLATFORM_STM32)
        	event_queue::pushEvent(event_queue::EVENT_ERROR_EEPROM_SLOT1_READ_ERROR + slotIndex);
#endif
        	continue;
        }

        if (version == -1) {
            return result;
        }

        if (!checkBlock((const BlockHeader *)buffer, bufferSize, version)) {
            if (((const BlockHeader *)buffer)->checksum != 0xFFFFFFFF) {
#if defined(EEZ_PLATFORM_STM32)
        	    event_queue::pushEvent(event_queue::EVENT_ERROR_EEPROM_SLOT1_CRC_CHECK_ERROR + slotIndex);
#endif
            }
			continue;
        }

        return true;
    }

	return false;
}

static bool moduleConfWrite(int slotIndex, const uint8_t *buffer, uint16_t bufferSize, uint16_t address) {
	if (bp3c::eeprom::g_testResult != TEST_OK) {
		return false;
    }

	for (int i = 0; i < NUM_RETRIES; i++) {
        bool result = bp3c::eeprom::write(slotIndex, buffer, bufferSize, address);

        if (!result) {
#if defined(EEZ_PLATFORM_STM32)
        	event_queue::pushEvent(event_queue::EVENT_ERROR_EEPROM_SLOT1_WRITE_ERROR + slotIndex);
#endif
        	continue;
        }

		uint8_t verifyBuffer[512];
		assert(sizeof(verifyBuffer) >= bufferSize);
		if (!moduleConfRead(slotIndex, verifyBuffer, bufferSize, address, -1)) {
			continue;
		}
		if (memcmp(buffer, verifyBuffer, bufferSize) != 0) {
#if defined(EEZ_PLATFORM_STM32)
			event_queue::pushEvent(event_queue::EVENT_ERROR_EEPROM_SLOT1_WRITE_VERIFY_ERROR + slotIndex);
#endif
        	continue;
		}

		return true;
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

static bool save(BlockHeader *block, uint16_t size, uint16_t address, uint16_t version) {
    block->version = version;
    block->checksum = calcChecksum(block, size);
    return confWrite((const uint8_t *)block, size, address);
}

////////////////////////////////////////////////////////////////////////////////

static bool moduleSave(int slotIndex, BlockHeader *block, uint16_t size, uint16_t address, uint16_t version) {
    block->version = version;
    block->checksum = calcChecksum(block, size);
    return moduleConfWrite(slotIndex, (const uint8_t *)block, size, address);
}

////////////////////////////////////////////////////////////////////////////////

uint16_t getProfileAddress(int location) {
    return PERSIST_CONF_FIRST_PROFILE_ADDRESS + location * PERSIST_CONF_PROFILE_BLOCK_SIZE;
}

////////////////////////////////////////////////////////////////////////////////

static const unsigned PERSISTENT_STORAGE_ADDRESS_ALIGNMENT = 32;

void init() {
    initDefaultDevConf();

    //bool storageInitialized = true;

    uint8_t blockData[sizeof(BlockHeader) + sizeof(DeviceConfiguration)];
    uint16_t blockAddress = PERSIST_CONF_DEV_CONF_ADDRESS;
    uint16_t blockStart = 0;
    for (unsigned i = 0; i < sizeof(g_devConfBlocks) / sizeof(DevConfBlock); i++) {
        uint16_t blockEnd = g_devConfBlocks[i].end;
        uint16_t blockSize = blockEnd - blockStart;
        uint16_t blockStorageSize = PERSISTENT_STORAGE_ADDRESS_ALIGNMENT * ((sizeof(BlockHeader) + blockSize + PERSISTENT_STORAGE_ADDRESS_ALIGNMENT - 1) / PERSISTENT_STORAGE_ADDRESS_ALIGNMENT);

        if (!confRead(blockData, blockStorageSize, blockAddress, g_devConfBlocks[i].version)) {
            if (!confRead(blockData, blockStorageSize, blockAddress + blockStorageSize, g_devConfBlocks[i].version)) {
                //if (i == 0) {
                //    // if we can't read first block at both locations we assume storage is not initialized
                //    storageInitialized = false;
                //} else if (storageInitialized) {
                //    // generate error only if storage is intialized and we failed to read block at both locations
                //    pushEvent(event_queue::EVENT_ERROR_EEPROM_MCU_CRC_CHECK_ERROR);
                //}

                // use default g_devConf data for this block
                memcpy(blockData + sizeof(BlockHeader), (uint8_t *)&g_defaultDevConf + blockStart, blockSize);

                // mark this block dirty, so it will be saved to persistent storage
                g_devConfBlocks[i].dirty = true;
            }
        }

        // copy this block to g_devConf
        memcpy((uint8_t *)&g_devConf + blockStart, blockData + sizeof(BlockHeader), blockSize);

        blockAddress += 2 * blockStorageSize;
        blockStart = blockEnd;
    }

    // remember this g_devConf to be used to detect when it changes
    memcpy(&g_savedDevConf, &g_devConf, sizeof(DeviceConfiguration));

#if OPTION_DISPLAY
    onLuminocityChanged();
    onThemeChanged();
#endif

    // load channels calibration parameters
    for (int i = 0; i < CH_NUM; ++i) {
        if (Channel::get(i).isInstalled()) {
            persist_conf::loadChannelCalibration(Channel::get(i));
        }
    }
}

bool saveAll(bool force) {
    bool moreDirtyBlocks = false;

    uint32_t tickCountMillis = millis();

    // write dirty device configuration blocks
    DeviceConfiguration devConf;
    memcpy(&devConf, &g_devConf, sizeof(DeviceConfiguration));

    uint8_t blockData[sizeof(BlockHeader) + sizeof(DeviceConfiguration)];
    uint16_t blockAddress = PERSIST_CONF_DEV_CONF_ADDRESS;
    uint16_t blockStart = 0;
    for (unsigned i = 0; i < sizeof(g_devConfBlocks) / sizeof(DevConfBlock); i++) {
        uint16_t blockEnd = g_devConfBlocks[i].end;
        uint16_t blockSize = blockEnd - blockStart;
        uint16_t blockStorageSize = PERSISTENT_STORAGE_ADDRESS_ALIGNMENT * ((sizeof(BlockHeader) + blockSize + PERSISTENT_STORAGE_ADDRESS_ALIGNMENT - 1) / PERSISTENT_STORAGE_ADDRESS_ALIGNMENT);

        if (!g_devConfBlocks[i].dirty) {
            // compare devConf with last saved g_devConf
            g_devConfBlocks[i].dirty = memcmp((uint8_t *)&devConf + blockStart, (uint8_t *)&g_savedDevConf + blockStart, blockSize) != 0;
        }

        if (
            g_devConfBlocks[i].dirty && 
            g_devConfBlocks[i].numSaveErrors < CONF_MAX_NUMBER_OF_SAVE_ERRORS_ALLOWED && 
			(force || (tickCountMillis - g_devConfBlocks[i].lastSaveTickCount >= g_devConfBlocks[i].minTickCountsBetweenSaves))
        ) {
        	memset(blockData, 0, blockStorageSize);
            memcpy(blockData + sizeof(BlockHeader), (uint8_t *)&devConf + blockStart, blockSize);

        	bool saved = save((BlockHeader *)blockData, blockStorageSize, blockAddress, g_devConfBlocks[i].version);
        	saved |= save((BlockHeader *)blockData, blockStorageSize, blockAddress + blockStorageSize, g_devConfBlocks[i].version);

            if (saved) {
                memcpy((uint8_t *)&g_savedDevConf + blockStart, (uint8_t *)&devConf + blockStart, blockSize);
                g_devConfBlocks[i].dirty = false;
                g_devConfBlocks[i].numSaveErrors = 0;
                g_devConfBlocks[i].lastSaveTickCount = tickCountMillis;
            } else {
                if (++g_devConfBlocks[i].numSaveErrors == CONF_MAX_NUMBER_OF_SAVE_ERRORS_ALLOWED) {
                    event_queue::pushEvent(event_queue::EVENT_ERROR_SAVE_DEV_CONF_BLOCK_0 + i);
                } else {
                    moreDirtyBlocks = true;
                }
            }
        }

        blockAddress += 2 * blockStorageSize;
        blockStart = blockEnd;
    }

    // write dirty profiles, last location should not be stored in EEPROM
    for (unsigned i = 0; i < NUM_PROFILE_LOCATIONS - 1; i++) {
        if (g_profilesCache[i].dirty && g_profilesCache[i].numSaveErrors < CONF_MAX_NUMBER_OF_SAVE_ERRORS_ALLOWED) {
            if (save((BlockHeader *)&g_profilesCache[i].profile, sizeof(profile::Parameters), getProfileAddress(i), profile::PROFILE_VERSION)) {
                g_profilesCache[i].dirty = false;
                g_profilesCache[i].numSaveErrors = 0;
            } else {
                if (++g_profilesCache[i].numSaveErrors == CONF_MAX_NUMBER_OF_SAVE_ERRORS_ALLOWED) {
                    event_queue::pushEvent(event_queue::EVENT_ERROR_SAVE_PROFILE_0 + i);
                } else {
                    moreDirtyBlocks = true;
                }
            }
        }
    }

    return moreDirtyBlocks;
}

void tick() {
    saveAll(false);
}

bool saveAllDirtyBlocks() {
    return saveAll(true);
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
    memset(&g_devConf.systemPassword, 0, sizeof(g_devConf.systemPassword));
    strncpy(g_devConf.systemPassword, new_password, new_password_len);
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
    memset(&g_devConf.calibration_password, 0, sizeof(g_devConf.calibration_password));
    strncpy(g_devConf.calibration_password, new_password, new_password_len);
    event_queue::pushEvent(event_queue::EVENT_INFO_CALIBRATION_PASSWORD_CHANGED);
}

void enableSound(bool enable) {
    g_devConf.isSoundEnabled = enable ? 1 : 0;
    event_queue::pushEvent(enable ? event_queue::EVENT_INFO_SOUND_ENABLED : event_queue::EVENT_INFO_SOUND_DISABLED);
}

bool isSoundEnabled() {
    return g_devConf.isSoundEnabled ? true : false;
}

void enableClickSound(bool enable) {
    g_devConf.isClickSoundEnabled = enable ? 1 : 0;
}

bool isClickSoundEnabled() {
    return g_devConf.isClickSoundEnabled ? true : false;
}

bool readSystemDate(uint8_t &year, uint8_t &month, uint8_t &day) {
    if (g_devConf.dateValid) {
        year = g_devConf.date_year;
        month = g_devConf.date_month;
        day = g_devConf.date_day;
        return true;
    }
    return false;
}

bool isDst() {
    return datetime::isDst(
        datetime::makeTime(
            2000 + g_devConf.date_year,
            g_devConf.date_month,
            g_devConf.date_day,
            g_devConf.time_hour,
            g_devConf.time_minute,
            g_devConf.time_second
        ),
        (datetime::DstRule)g_devConf.dstRule
    );
}

void setDst(unsigned dst) {
    if (dst == 0) {
        g_devConf.dst = 0;
    } else if (dst == 1) {
        g_devConf.dst = 1;
    } else {
        g_devConf.dst = isDst();
    }
}

void writeSystemDate(uint8_t year, uint8_t month, uint8_t day, unsigned dst) {
    g_devConf.date_year = year;
    g_devConf.date_month = month;
    g_devConf.date_day = day;

    g_devConf.dateValid = 1;

    setDst(dst);
}

bool readSystemTime(uint8_t &hour, uint8_t &minute, uint8_t &second) {
    if (g_devConf.timeValid) {
        hour = g_devConf.time_hour;
        minute = g_devConf.time_minute;
        second = g_devConf.time_second;
        return true;
    }
    return false;
}

void writeSystemTime(uint8_t hour, uint8_t minute, uint8_t second, unsigned dst) {
    g_devConf.time_hour = hour;
    g_devConf.time_minute = minute;
    g_devConf.time_second = second;

    g_devConf.timeValid = 1;

    setDst(dst);
}

void writeSystemDateTime(uint8_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t minute,
                         uint8_t second, unsigned dst) {
    g_devConf.date_year = year;
    g_devConf.date_month = month;
    g_devConf.date_day = day;

    g_devConf.dateValid = 1;

    g_devConf.time_hour = hour;
    g_devConf.time_minute = minute;
    g_devConf.time_second = second;

    g_devConf.timeValid = 1;

    setDst(dst);
}

void enableProfileAutoRecall(bool enable) {
    g_devConf.profileAutoRecallEnabled = enable ? 1 : 0;
}

bool isProfileAutoRecallEnabled() {
    return g_devConf.profileAutoRecallEnabled ? true : false;
}

void setProfileAutoRecallLocation(int location) {
    g_devConf.profile_auto_recall_location = (int8_t)location;
    event_queue::pushEvent(event_queue::EVENT_INFO_DEFAULE_PROFILE_CHANGED_TO_0 + location);
    if (location == 0) {
        profile::save();
    }
}

int getProfileAutoRecallLocation() {
    return g_devConf.profile_auto_recall_location;
}

void setChannelsViewMode(unsigned int channelsViewMode) {
    uint8_t ytGraphUpdateMethod = g_devConf.ytGraphUpdateMethod;

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

    g_devConf.channelsViewMode = channelsViewMode;
    g_devConf.ytGraphUpdateMethod = ytGraphUpdateMethod;
}

unsigned int getChannelsViewMode() {
    if (g_devConf.channelsViewMode == CHANNELS_VIEW_MODE_YT && g_devConf.ytGraphUpdateMethod == YT_GRAPH_UPDATE_METHOD_SCAN_LINE) {
        return CHANNELS_VIEW_MODE_YT + 1;
    }
    return g_devConf.channelsViewMode;
}

void setChannelsViewModeInMax(unsigned int channelsViewModeInMax) {
    uint8_t ytGraphUpdateMethod = g_devConf.ytGraphUpdateMethod;

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

    g_devConf.channelsViewModeInMax = channelsViewModeInMax;
    g_devConf.ytGraphUpdateMethod = ytGraphUpdateMethod;
}

void toggleChannelsViewMode() {
    if (isMaxChannelView()) {
        setChannelsViewModeInMax(g_devConf.channelsViewModeInMax + 1);
    } else {
        setChannelsViewMode(g_devConf.channelsViewMode + 1);
    }
}

bool isMaxChannelView() {
    return getMaxChannelIndex() >= 0;
}

int getMaxChannelIndex() {
    int channelIndex = g_devConf.maxChannel - 1;
    if (channelIndex < CH_NUM) {
        return channelIndex;
    }
    return -1;
}

int getMin1ChannelIndex() {
    int maxChannelSlotIndex = Channel::get(getMaxChannelIndex()).slotIndex;
    if (maxChannelSlotIndex == 0) {
        return Channel::getBySlotIndex(1).channelIndex;
    } else {
        return Channel::getBySlotIndex(0).channelIndex;
    }
}

int getMin2ChannelIndex() {
    int maxChannelSlotIndex = Channel::get(getMaxChannelIndex()).slotIndex;
    if (maxChannelSlotIndex == 2) {
        return Channel::getBySlotIndex(1).channelIndex;
    } else {
        return Channel::getBySlotIndex(2).channelIndex;
    }
}

void setMaxChannelIndex(int channelIndex) {
    g_devConf.maxChannel = channelIndex + 1;
}

void toggleMaxChannelIndex(int channelIndex) {
    if (isMaxChannelView() && channelIndex == getMaxChannelIndex()) {
        g_devConf.maxChannel = 0;
    } else {
        g_devConf.maxChannel = channelIndex + 1;
    }
}

////////////////////////////////////////////////////////////////////////////////

profile::Parameters *loadProfile(int location) {
    assert(location < NUM_PROFILE_LOCATIONS && sizeof(profile::Parameters) <= PERSIST_CONF_PROFILE_BLOCK_SIZE);
    
    if (!g_profilesCache[location].loaded) {
        if (location != NUM_PROFILE_LOCATIONS - 1) { // last location is not stored in EEPROM
            profile::Parameters profile;
            if (confRead((uint8_t *)&profile, sizeof(profile::Parameters), getProfileAddress(location), profile::PROFILE_VERSION)) {
                memcpy(&g_profilesCache[location].profile, &profile, sizeof(profile::Parameters));
            }
        }
        g_profilesCache[location].loaded = true;
    }

    return &g_profilesCache[location].profile;
}

void saveProfile(int location, profile::Parameters *profile) {
    memcpy(&g_profilesCache[location].profile, profile, sizeof(profile::Parameters));
    g_profilesCache[location].loaded = true;
    g_profilesCache[location].dirty = true;
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
    unsigned outputProtectionCouple = enable ? 1 : 0;

    if (g_devConf.outputProtectionCouple != outputProtectionCouple) {
        g_devConf.outputProtectionCouple = outputProtectionCouple;

        if (g_devConf.outputProtectionCouple) {
            event_queue::pushEvent(event_queue::EVENT_INFO_OUTPUT_PROTECTION_COUPLED);
        } else {
            event_queue::pushEvent(event_queue::EVENT_INFO_OUTPUT_PROTECTION_DECOUPLED);
        }
    }
}

bool isOutputProtectionCoupleEnabled() {
    return g_devConf.outputProtectionCouple ? true : false;
}

void enableShutdownWhenProtectionTripped(bool enable) {
    unsigned shutdownWhenProtectionTripped = enable ? 1 : 0;

    if (g_devConf.shutdownWhenProtectionTripped == shutdownWhenProtectionTripped) {
        return;
    }

    g_devConf.shutdownWhenProtectionTripped = shutdownWhenProtectionTripped;

    if (g_devConf.shutdownWhenProtectionTripped) {
        event_queue::pushEvent(event_queue::EVENT_INFO_SHUTDOWN_WHEN_PROTECTION_TRIPPED_ENABLED);
    } else {
        event_queue::pushEvent(event_queue::EVENT_INFO_SHUTDOWN_WHEN_PROTECTION_TRIPPED_DISABLED);
    }
}

bool isShutdownWhenProtectionTrippedEnabled() {
    return g_devConf.shutdownWhenProtectionTripped ? true : false;
}

void enableForceDisablingAllOutputsOnPowerUp(bool enable) {
    unsigned forceDisablingAllOutputsOnPowerUp = enable ? 1 : 0;

    if (g_devConf.forceDisablingAllOutputsOnPowerUp == forceDisablingAllOutputsOnPowerUp) {
        return;
    }

    g_devConf.forceDisablingAllOutputsOnPowerUp = forceDisablingAllOutputsOnPowerUp;

    if (g_devConf.forceDisablingAllOutputsOnPowerUp) {
        event_queue::pushEvent(event_queue::EVENT_INFO_FORCE_DISABLING_ALL_OUTPUTS_ON_POWERUP_ENABLED);
    } else {
        event_queue::pushEvent(event_queue::EVENT_INFO_FORCE_DISABLING_ALL_OUTPUTS_ON_POWERUP_DISABLED);
    }
}

bool isForceDisablingAllOutputsOnPowerUpEnabled() {
    return g_devConf.forceDisablingAllOutputsOnPowerUp ? true : false;
}

void lockFrontPanel(bool lock) {
    g_rlState = lock ? RL_STATE_REMOTE : RL_STATE_LOCAL;

    unsigned isFrontPanelLocked = lock ? 1 : 0;

    if (g_devConf.isFrontPanelLocked == isFrontPanelLocked) {
        return;
    }

    g_devConf.isFrontPanelLocked = isFrontPanelLocked;

    if (g_devConf.isFrontPanelLocked) {
        event_queue::pushEvent(event_queue::EVENT_INFO_FRONT_PANEL_LOCKED);
    } else {
        event_queue::pushEvent(event_queue::EVENT_INFO_FRONT_PANEL_UNLOCKED);
    }
}

void setEncoderSettings(uint8_t confirmationMode, uint8_t movingSpeedDown, uint8_t movingSpeedUp) {
    g_devConf.encoderConfirmationMode = confirmationMode;
    g_devConf.encoderMovingSpeedDown = movingSpeedDown;
    g_devConf.encoderMovingSpeedUp = movingSpeedUp;
}

void setDisplayState(unsigned newState) {
    g_devConf.displayState = newState;
}

void setDisplayBrightness(uint8_t displayBrightness) {
    g_devConf.displayBrightness = displayBrightness;

#if OPTION_DISPLAY
    updateBrightness();
#endif
}

void setDisplayBackgroundLuminosityStep(uint8_t displayBackgroundLuminosityStep) {
    g_devConf.displayBackgroundLuminosityStep = displayBackgroundLuminosityStep;

#if OPTION_DISPLAY
    onLuminocityChanged();
    refreshScreen();
#endif
}

bool enableSerial(bool enable) {
    unsigned serialEnabled = enable ? 1 : 0;
    if (!g_devConf.skipSerialSetup || g_devConf.serialEnabled != serialEnabled) {
        g_devConf.serialEnabled = serialEnabled;
        g_devConf.skipSerialSetup = 1;
        serial::update();
    }
    return true;
}

bool isSerialEnabled() {
    return g_devConf.serialEnabled ? true : false;
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
    return g_devConf.serialBaud;
}

bool setSerialBaudIndex(int baudIndex) {
    uint8_t serialBaud = (uint8_t)baudIndex;
    if (!g_devConf.skipSerialSetup || g_devConf.serialBaud != serialBaud) {
        g_devConf.serialBaud = serialBaud;
        g_devConf.skipSerialSetup = 1;
        serial::update();
    }
    return true;
}

int getSerialParity() {
    return g_devConf.serialParity;
}

bool setSerialParity(int parity) {
    unsigned serialParity = (unsigned)parity;
    if (!g_devConf.skipSerialSetup || g_devConf.serialParity != serialParity) {
        g_devConf.serialParity = serialParity;
        g_devConf.skipSerialSetup = 1;
        serial::update();
    }
    return true;
}

bool setSerialSettings(bool enabled, int baudIndex, int parity) {
    unsigned serialEnabled = enabled ? 1 : 0;
    uint8_t serialBaud = (uint8_t)baudIndex;
    unsigned serialParity = (unsigned)parity;
    if (!g_devConf.skipSerialSetup || g_devConf.serialEnabled != serialEnabled ||
        g_devConf.serialBaud != serialBaud || g_devConf.serialParity != serialParity) {
        g_devConf.serialEnabled = enabled;
        g_devConf.serialBaud = serialBaud;
        g_devConf.serialParity = serialParity;
        g_devConf.skipSerialSetup = 1;
        serial::update();
    }
    return true;
}

bool enableEthernet(bool enable) {
#if OPTION_ETHERNET
    unsigned ethernetEnabled = enable ? 1 : 0;
    if (!g_devConf.skipEthernetSetup || g_devConf.ethernetEnabled != ethernetEnabled) {
        g_devConf.ethernetEnabled = ethernetEnabled;
        g_devConf.skipEthernetSetup = 1;
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
    return g_devConf.ethernetEnabled ? true : false;
}

bool enableEthernetDhcp(bool enable) {
#if OPTION_ETHERNET
    unsigned ethernetDhcpEnabled = enable ? 1 : 0;
    if (!g_devConf.skipEthernetSetup ||
        g_devConf.ethernetDhcpEnabled != ethernetDhcpEnabled) {
        g_devConf.ethernetDhcpEnabled = ethernetDhcpEnabled;
        g_devConf.skipEthernetSetup = 1;
        ethernet::update();
    }
    return true;
#else
    return false;
#endif
}

bool isEthernetDhcpEnabled() {
    return g_devConf.ethernetDhcpEnabled ? true : false;
}

bool setEthernetMacAddress(uint8_t macAddress[]) {
#if OPTION_ETHERNET
    if (!g_devConf.skipEthernetSetup ||
        memcmp(g_devConf.ethernetMacAddress, macAddress, 6) != 0) {
        memcpy(g_devConf.ethernetMacAddress, macAddress, 6);
        g_devConf.skipEthernetSetup = 1;
        ethernet::update();
    }
    return true;
#else
    return false;
#endif
}

bool setEthernetIpAddress(uint32_t ipAddress) {
#if OPTION_ETHERNET
    if (!g_devConf.skipEthernetSetup || g_devConf.ethernetIpAddress != ipAddress) {
        g_devConf.ethernetIpAddress = ipAddress;
        g_devConf.skipEthernetSetup = 1;
        ethernet::update();
    }
    return true;
#else
    return false;
#endif
}

bool setEthernetDns(uint32_t dns) {
#if OPTION_ETHERNET
    if (!g_devConf.skipEthernetSetup || g_devConf.ethernetDns != dns) {
        g_devConf.ethernetDns = dns;
        g_devConf.skipEthernetSetup = 1;
        ethernet::update();
    }
    return true;
#else
    return false;
#endif
}

bool setEthernetGateway(uint32_t gateway) {
#if OPTION_ETHERNET
    if (!g_devConf.skipEthernetSetup || g_devConf.ethernetGateway != gateway) {
        g_devConf.ethernetGateway = gateway;
        g_devConf.skipEthernetSetup = 1;
        ethernet::update();
    }
    return true;
#else
    return false;
#endif
}

bool setEthernetHostName(const char *hostName) {
#if OPTION_ETHERNET
    if (!g_devConf.skipEthernetSetup) {
        strcpy(g_devConf.ethernetHostName, hostName);
        g_devConf.skipEthernetSetup = 1;
        ethernet::update();
    }
    return true;
#else
    return false;
#endif
}

bool setEthernetSubnetMask(uint32_t subnetMask) {
#if OPTION_ETHERNET
    if (!g_devConf.skipEthernetSetup || g_devConf.ethernetSubnetMask != subnetMask) {
        g_devConf.ethernetSubnetMask = subnetMask;
        g_devConf.skipEthernetSetup = 1;
        ethernet::update();
    }
    return true;
#else
    return false;
#endif
}

bool setEthernetScpiPort(uint16_t scpiPort) {
#if OPTION_ETHERNET
    if (!g_devConf.skipEthernetSetup || g_devConf.ethernetScpiPort != scpiPort) {
        g_devConf.ethernetScpiPort = scpiPort;
        g_devConf.skipEthernetSetup = 1;
        ethernet::update();
    }
    return true;
#else
    return false;
#endif
}

bool setEthernetSettings(bool enable, bool dhcpEnable, uint32_t ipAddress, uint32_t dns,
                         uint32_t gateway, uint32_t subnetMask, uint16_t scpiPort,
                         uint8_t *macAddress, const char *hostName) {
#if OPTION_ETHERNET
    unsigned ethernetEnabled = enable ? 1 : 0;
    unsigned ethernetDhcpEnabled = dhcpEnable ? 1 : 0;

    if (!g_devConf.skipEthernetSetup || g_devConf.ethernetEnabled != ethernetEnabled ||
        g_devConf.ethernetDhcpEnabled != ethernetDhcpEnabled ||
        memcmp(g_devConf.ethernetMacAddress, macAddress, 6) != 0 ||
        g_devConf.ethernetIpAddress != ipAddress || g_devConf.ethernetDns != dns ||
        g_devConf.ethernetGateway != gateway || g_devConf.ethernetSubnetMask != subnetMask ||
        g_devConf.ethernetScpiPort != scpiPort || strcmp(hostName, g_devConf.ethernetHostName)) {

        if (g_devConf.ethernetEnabled != ethernetEnabled) {
            g_devConf.ethernetEnabled = ethernetEnabled;
            event_queue::pushEvent(g_devConf.ethernetEnabled
                                       ? event_queue::EVENT_INFO_ETHERNET_ENABLED
                                       : event_queue::EVENT_INFO_ETHERNET_DISABLED);
        }

        g_devConf.ethernetDhcpEnabled = ethernetDhcpEnabled;
        memcpy(g_devConf.ethernetMacAddress, macAddress, 6);
        g_devConf.ethernetIpAddress = ipAddress;
        g_devConf.ethernetDns = dns;
        g_devConf.ethernetGateway = gateway;
        g_devConf.ethernetSubnetMask = subnetMask;
        g_devConf.ethernetScpiPort = scpiPort;
        strcpy(g_devConf.ethernetHostName, hostName);

        g_devConf.skipEthernetSetup = 1;
        ethernet::update();
    }

    return true;
#else
    return false;
#endif
}

void enableNtp(bool enable) {
    g_devConf.ntpEnabled = enable ? 1 : 0;
}

bool isNtpEnabled() {
    return g_devConf.ntpEnabled ? true : false;
}

void setNtpServer(const char *ntpServer, size_t ntpServerLength) {
    strncpy(g_devConf.ntpServer, ntpServer, ntpServerLength);
    g_devConf.ntpServer[ntpServerLength] = 0;
}

void setNtpSettings(bool enable, const char *ntpServer) {
    g_devConf.ntpEnabled = enable ? 1 : 0;
    strcpy(g_devConf.ntpServer, ntpServer);
}

bool setMqttSettings(bool enable, const char *host, uint16_t port, const char *username, const char *password, float period) {
#if OPTION_ETHERNET
    bool reconnectRequired = g_devConf.mqttEnabled != enable ||
        strcmp(g_devConf.mqttHost, host) != 0 ||
        g_devConf.mqttPort != port ||
        strcmp(g_devConf.mqttUsername, username) != 0 ||
        strcmp(g_devConf.mqttPassword, password) != 0;

    g_devConf.mqttEnabled = enable ? 1 : 0;
    strcpy(g_devConf.mqttHost, host);
    g_devConf.mqttPort = port;
    strcpy(g_devConf.mqttUsername, username);
    strcpy(g_devConf.mqttPassword, password);
    g_devConf.mqttPeriod = period;

    if (reconnectRequired) {
        mqtt::reconnect();
    }

    return true;
#else
    return false;
#endif
}

void enableMqtt(bool enable) {
    setMqttSettings(enable, persist_conf::devConf.mqttHost, persist_conf::devConf.mqttPort, persist_conf::devConf.mqttUsername, persist_conf::devConf.mqttPassword, persist_conf::devConf.mqttPeriod);
}

void setSdLocked(bool sdLocked) {
    g_devConf.sdLocked = sdLocked ? 1 : 0;
}

bool isSdLocked() {
    return g_devConf.sdLocked ? true : false;
}

void setAnimationsDuration(float value) {
    g_devConf.animationsDuration = value;
}

void setTouchscreenCalParams(int16_t touch_screen_cal_tlx, int16_t touch_screen_cal_tly, int16_t touch_screen_cal_brx, int16_t touch_screen_cal_bry, int16_t touch_screen_cal_trx, int16_t touch_screen_cal_try) {
    g_devConf.touch_screen_cal_tlx = touch_screen_cal_tlx;
    g_devConf.touch_screen_cal_tly = touch_screen_cal_tly;
    g_devConf.touch_screen_cal_brx = touch_screen_cal_brx;
    g_devConf.touch_screen_cal_bry = touch_screen_cal_bry;
    g_devConf.touch_screen_cal_trx = touch_screen_cal_trx;
    g_devConf.touch_screen_cal_try = touch_screen_cal_try;
}

void setFanSettings(uint8_t fanMode, uint8_t fanSpeed) {
    g_devConf.fanMode = fanMode;
    g_devConf.fanSpeed = fanSpeed;
}

void setDateValid(unsigned dateValid) {
    g_devConf.dateValid = dateValid;
}

void setTimeValid(unsigned timeValid) {
    g_devConf.timeValid = timeValid;
}

void setTimeZone(int16_t time_zone) {
    g_devConf.time_zone = time_zone;
}

void setDstRule(uint8_t dstRule) {
    g_devConf.dstRule = dstRule;
}

void setIoPinPolarity(int pin, unsigned polarity) {
    g_devConf.ioPins[pin].polarity = polarity;
    io_pins::refresh();
}

void setIoPinFunction(int pin, unsigned function) {
    g_devConf.ioPins[pin].function = function;
    io_pins::refresh();
}

void setSelectedThemeIndex(uint8_t selectedThemeIndex) {
    g_devConf.selectedThemeIndex = selectedThemeIndex;
}

void resetTrigger() {
    g_devConf.triggerDelay = trigger::DELAY_DEFAULT;
    g_devConf.triggerSource = trigger::SOURCE_IMMEDIATE;
    g_devConf.triggerContinuousInitializationEnabled = 0;
}

void setTriggerContinuousInitializationEnabled(unsigned triggerContinuousInitializationEnabled) {
    g_devConf.triggerContinuousInitializationEnabled = triggerContinuousInitializationEnabled;
}

void setTriggerDelay(float triggerDelay) {
    g_devConf.triggerDelay = triggerDelay;
}

void setTriggerSource(uint8_t triggerSource) {
    g_devConf.triggerSource = triggerSource;
}

void setSkipChannelCalibrations(unsigned skipChannelCalibrations) {
    g_devConf.skipChannelCalibrations = skipChannelCalibrations;
}

void setSkipDateTimeSetup(unsigned skipDateTimeSetup) {
    g_devConf.skipDateTimeSetup = skipDateTimeSetup;
}

void setSkipSerialSetup(unsigned skipSerialSetup) {
    g_devConf.skipSerialSetup = skipSerialSetup;
}

void setSkipEthernetSetup(unsigned skipEthernetSetup) {
    g_devConf.skipEthernetSetup = skipEthernetSetup;
}

void setUserSwitchAction(UserSwitchAction userSwitchAction) {
    g_devConf.userSwitchAction = userSwitchAction;
}

void setSortFilesOption(SortFilesOption sortFilesOption) {
    g_devConf.sortFilesOption = sortFilesOption;
}

////////////////////////////////////////////////////////////////////////////////

static const uint16_t MODULE_PERSIST_CONF_BLOCK_MODULE_CONFIGURATION_ADDRESS = 64;
static const uint16_t MODULE_PERSIST_CONF_BLOCK_MODULE_CONFIGURATION_SIZE = 64;

static const uint16_t MODULE_PERSIST_CONF_CH_CAL_ADDRESS = 128;
static const uint16_t MODULE_PERSIST_CONF_CH_CAL_BLOCK_SIZE = 256;

ModuleConfiguration g_moduleConf[NUM_SLOTS];

static void initModuleConf(int slotIndex) {
    ModuleConfiguration &moduleConf = g_moduleConf[slotIndex];

    memset(&moduleConf, 0, sizeof(ModuleConfiguration));

    moduleConf.header.checksum = 0;
    moduleConf.header.version = MODULE_CONF_VERSION;

    moduleConf.chCalEnabled = 0x00;
}

void loadModuleConf(int slotIndex) {
    assert(sizeof(ModuleConfiguration) <= MODULE_PERSIST_CONF_BLOCK_MODULE_CONFIGURATION_SIZE);

    uint8_t buffer[MODULE_PERSIST_CONF_BLOCK_MODULE_CONFIGURATION_SIZE];

    if (moduleConfRead(
        slotIndex, 
        buffer, 
        MODULE_PERSIST_CONF_BLOCK_MODULE_CONFIGURATION_SIZE, 
        MODULE_PERSIST_CONF_BLOCK_MODULE_CONFIGURATION_ADDRESS,
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
    memcpy(buffer, &moduleConf, sizeof(ModuleConfiguration));

    memset(buffer + sizeof(ModuleConfiguration), 0, MODULE_PERSIST_CONF_BLOCK_MODULE_CONFIGURATION_SIZE - sizeof(ModuleConfiguration));

    return moduleSave(
        slotIndex, 
        (BlockHeader *)buffer, 
        MODULE_PERSIST_CONF_BLOCK_MODULE_CONFIGURATION_SIZE, 
        MODULE_PERSIST_CONF_BLOCK_MODULE_CONFIGURATION_ADDRESS,
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
    saveModuleConf(channel.slotIndex);
}

void loadChannelCalibration(Channel &channel) {
	assert(MODULE_PERSIST_CONF_CH_CAL_BLOCK_SIZE >= sizeof(Channel::CalibrationConfiguration));

    if (!moduleConfRead(
        channel.slotIndex,
        (uint8_t *)&channel.cal_conf,
        sizeof(Channel::CalibrationConfiguration),
        MODULE_PERSIST_CONF_CH_CAL_ADDRESS + channel.subchannelIndex * MODULE_PERSIST_CONF_CH_CAL_BLOCK_SIZE,
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
        MODULE_PERSIST_CONF_CH_CAL_ADDRESS + channel.subchannelIndex * MODULE_PERSIST_CONF_CH_CAL_BLOCK_SIZE,
        CH_CAL_CONF_VERSION
    );
}

} // namespace persist_conf
} // namespace psu
} // namespace eez

extern "C" void getMacAddress(uint8_t macAddress[]) {
    memcpy(macAddress, eez::psu::persist_conf::g_devConf.ethernetMacAddress, 6);
}
