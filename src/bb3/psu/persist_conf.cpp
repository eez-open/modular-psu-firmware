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
#include <ctype.h> 

#if defined(EEZ_PLATFORM_STM32)
#include <main.h>
#endif

#include <eez/conf.h>
#include <eez/core/eeprom.h>

#include <bb3/system.h>
#include <bb3/usb.h>
#include <bb3/tasks.h>
#include <bb3/psu/psu.h>
#include <bb3/mcu/encoder.h>
#include <bb3/bp3c/eeprom.h>
#include <bb3/psu/dlog_record.h>
#include <bb3/psu/event_queue.h>
#include <bb3/psu/serial_psu.h>
#include <bb3/psu/calibration.h>
#include <bb3/psu/ntp.h>
#if OPTION_ENCODER
#include <bb3/mcu/encoder.h>
#endif
#if OPTION_DISPLAY
#include <eez/gui/gui.h>
#include <bb3/gui/document.h>
using namespace eez::gui;
using namespace eez::gui::display;
#endif

#if OPTION_ETHERNET
#include <bb3/psu/ethernet_scpi.h>
#include <bb3/mqtt.h>
#endif

#include <bb3/psu/datetime.h>
#include <bb3/psu/ontime.h>

#include <eez/gui/widgets/yt_graph.h>

#include <bb3/aux_ps/fan.h>

#define NUM_RETRIES 3

namespace eez {
namespace psu {
namespace persist_conf {

#define CONF_MAX_NUMBER_OF_SAVE_ERRORS_ALLOWED 2

////////////////////////////////////////////////////////////////////////////////

static const uint16_t MCU_EEPROM_ONTIME_START_ADDRESS = 64;
static const uint16_t BP3C_EEPROM_ONTIME_START_ADDRESS = 40;

static const uint16_t MODULE_CONF_VERSION = 1;
static const uint16_t MODULE_SERIAL_NO_VERSION = 1;
static const uint16_t CH_CAL_CONF_VERSION = 4;

static const uint16_t PERSIST_CONF_DEV_CONF_ADDRESS = 128;

static const uint32_t ONTIME_MAGIC = 0xA7F31B3CL;
static const uint32_t COUNTER_MAGIC = 0XEF8D43B2;

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
    { offsetof(DeviceConfiguration, dateYear), 1, false, 0, 0, 0 },
    { offsetof(DeviceConfiguration, profileAutoRecallLocation), 1, false, 0, 0, 0 },
    { offsetof(DeviceConfiguration, startOfBlock4), 1, false, 0, 0, 0 },
    { offsetof(DeviceConfiguration, reserved51), 1, false, 0, 0, 0 },
    { offsetof(DeviceConfiguration, ytGraphUpdateMethod), 1, false, 0, 0, 0 },
    { offsetof(DeviceConfiguration, userSwitchAction), 1, false, 0, 60 * 1000, 0 },
    { offsetof(DeviceConfiguration, ethernetHostName), 1, false, 0, 0, 0 },
    { offsetof(DeviceConfiguration, fanMode), 1, false, 0, 0, 0 },
    { offsetof(DeviceConfiguration, slotEnabledBits), 1, false, 0, 0, 0 },
    { sizeof(DeviceConfiguration), 1, false, 0, 0, 0 },
};

////////////////////////////////////////////////////////////////////////////////

void getSystemDerivedMacAddress(uint8_t *macAddress) {
#if defined(EEZ_PLATFORM_STM32)
    uint32_t idw0 = HAL_GetUIDw0();
    uint32_t idw1 = HAL_GetUIDw1();
    uint32_t idw2 = HAL_GetUIDw2();

    uint8_t *pIdw0 = (uint8_t *)&idw0;
    uint8_t *pIdw1 = (uint8_t *)&idw1;
    uint8_t *pIdw2 = (uint8_t *)&idw2;


    macAddress[0] = pIdw0[0] ^ pIdw1[0] ^ pIdw2[3] ^ pIdw2[0];
    macAddress[1] = pIdw0[1] ^ pIdw1[3] ^ pIdw2[2] ^ pIdw2[1];
    macAddress[2] = pIdw0[2] ^ pIdw1[1] ^ pIdw2[1] ^ pIdw0[2];
    macAddress[3] = pIdw0[3] ^ pIdw1[2] ^ pIdw2[0] ^ pIdw0[3];
    macAddress[4] = pIdw0[0] ^ pIdw1[2] ^ pIdw2[3] ^ pIdw1[3];
    macAddress[5] = pIdw0[1] ^ pIdw1[1] ^ pIdw2[2] ^ pIdw1[0];
#endif

#if defined(EEZ_PLATFORM_SIMULATOR)
    macAddress[0] = 0;
    macAddress[1] = 0;
    macAddress[2] = 0;
    macAddress[3] = 0;
    macAddress[4] = 0;
    macAddress[5] = 0;
#endif

    macAddress[0] |= 0x02; // Locally administered
    macAddress[0] &= 0xFE; // Unicast
}

void initDefaultDevConf() {
    memset(&g_defaultDevConf, 0, sizeof(g_defaultDevConf));

    // block 1
    stringCopy(g_defaultDevConf.calibrationPassword, sizeof(g_defaultDevConf.calibrationPassword), CALIBRATION_PASSWORD_DEFAULT);

    g_defaultDevConf.touchScreenCalTlx = 0;
    g_defaultDevConf.touchScreenCalTly = 0;
    g_defaultDevConf.touchScreenCalBrx = 0;
    g_defaultDevConf.touchScreenCalBry = 0;
    g_defaultDevConf.touchScreenCalTrx = 0;
    g_defaultDevConf.touchScreenCalTry = 0;

    // block2
    g_defaultDevConf.dateValid = 0;
    g_defaultDevConf.timeValid = 0;
    g_defaultDevConf.dst = 0;

    g_defaultDevConf.timeZone = 0;
    g_defaultDevConf.dstRule = datetime::DST_RULE_OFF;

    // block 3
    g_defaultDevConf.profileAutoRecallEnabled = 1;

    g_defaultDevConf.outputProtectionCouple = 0;
    g_defaultDevConf.shutdownWhenProtectionTripped = 0;
    g_defaultDevConf.forceDisablingAllOutputsOnPowerUp = 0;

    g_defaultDevConf.profileAutoRecallLocation = 0;

    // block 4
    g_defaultDevConf.usbMode = USB_MODE_DEVICE;

#ifdef EEZ_PLATFORM_SIMULATOR
    g_defaultDevConf.ethernetEnabled = 1;
#else
    g_defaultDevConf.ethernetEnabled = 0;
#endif
    g_defaultDevConf.ethernetDhcpEnabled = 1;

    g_defaultDevConf.encoderConfirmationMode = 0;

    g_defaultDevConf.isSoundEnabled = 1;
    g_defaultDevConf.isClickSoundEnabled = 0;

    g_defaultDevConf.ethernetIpAddress = getIpAddress(192, 168, 1, 100);
    g_defaultDevConf.ethernetDns = getIpAddress(192, 168, 1, 1);
    g_defaultDevConf.ethernetGateway = getIpAddress(192, 168, 1, 1);
    g_defaultDevConf.ethernetSubnetMask = getIpAddress(255, 255, 255, 0);
    g_defaultDevConf.ethernetScpiPort = TCP_PORT;
    stringCopy(g_defaultDevConf.ntpServer, sizeof(g_defaultDevConf.ntpServer), CONF_DEFAULT_NTP_SERVER);
    
    uint8_t macAddress[6];
    getSystemDerivedMacAddress(macAddress);
    memcpy(g_defaultDevConf.ethernetMacAddress, macAddress, 6);

    g_defaultDevConf.displayBrightness = DISPLAY_BRIGHTNESS_DEFAULT;
    g_defaultDevConf.displayBackgroundLuminosityStep = DISPLAY_BACKGROUND_LUMINOSITY_STEP_DEFAULT;
    g_defaultDevConf.selectedThemeIndex = THEME_ID_DARK;
    g_defaultDevConf.animationsDuration = CONF_DEFAULT_ANIMATIONS_DURATION;

    g_defaultDevConf.encoderMovingSpeedDown = mcu::encoder::DEFAULT_MOVING_DOWN_SPEED;
    g_defaultDevConf.encoderMovingSpeedUp = mcu::encoder::DEFAULT_MOVING_UP_SPEED;

    // block 5

    // block 6
    g_defaultDevConf.displayState = 1;
    g_defaultDevConf.channelsViewMode = 0;
    g_defaultDevConf.maxSlotIndex = 0;
    g_defaultDevConf.maxSubchannelIndex = 0;
    g_defaultDevConf.channelsViewModeInMax = 0;

    g_defaultDevConf.ytGraphUpdateMethod = YT_GRAPH_UPDATE_METHOD_SCROLL;

    // block 7
    g_defaultDevConf.userSwitchAction = USER_SWITCH_ACTION_NONE;
    g_defaultDevConf.sortFilesOption = SORT_FILES_BY_TIME_DESC;
    g_defaultDevConf.eventQueueFilter = event_queue::EVENT_TYPE_INFO;
    g_defaultDevConf.viewFlags.dlogViewLegendViewOption = DLOG_VIEW_LEGEND_VIEW_OPTION_DOCK;
    g_defaultDevConf.viewFlags.dlogViewShowLabels = 1;
    g_defaultDevConf.viewFlags.dlogViewDrawerIsOpen = 0;
    g_defaultDevConf.ntpRefreshFrequency = NTP_REFRESH_FREQUENCY_DEF;

    // block 8
    stringCopy(g_defaultDevConf.ethernetHostName, sizeof(g_defaultDevConf.ethernetHostName), DEFAULT_ETHERNET_HOST_NAME);
    
    g_defaultDevConf.mqttEnabled = 0;
    g_defaultDevConf.mqttPort = 1883;
    g_defaultDevConf.mqttPeriod = 15.0f;

    // block 9
    g_defaultDevConf.fanMode = FAN_MODE_AUTO;
    g_defaultDevConf.fanSpeedPercentage = 100;
    g_defaultDevConf.fanSpeedPWM = FAN_MAX_PWM;

    // block 10
    g_defaultDevConf.slotEnabledBits = 0xFF;
}

////////////////////////////////////////////////////////////////////////////////

static bool confRead(uint8_t *buffer, uint16_t bufferSize, uint16_t address, int version) {
	if (bp3c::eeprom::g_testResult != TEST_OK) {
		return false;
    }

    for (int i = 0; i < NUM_RETRIES; i++) {
        bool result = eeprom::read(buffer, bufferSize, address);

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
        bool result = eeprom::write(buffer, bufferSize, address);

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
            	osDelay(1);
        	    event_queue::pushEvent(event_queue::EVENT_ERROR_EEPROM_SLOT1_CRC_CHECK_ERROR + slotIndex);
        	    osDelay(1);
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

		uint8_t verifyBuffer[1024];
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

    if (g_devConf.ntpRefreshFrequency < NTP_REFRESH_FREQUENCY_MIN || g_devConf.ntpRefreshFrequency > NTP_REFRESH_FREQUENCY_MAX) {
        g_devConf.ntpRefreshFrequency = NTP_REFRESH_FREQUENCY_DEF;

        if (g_devConf.usbMode == USB_MODE_DISABLED) {
        	g_devConf.usbMode = USB_MODE_DEVICE;
        }
    }

    static const uint8_t OLD_DEFAULT_MAC_ADDRESS[] = { 0x74, 0x69, 0x69, 0x2D, 0x30, 0x00 };
    if (memcmp(g_devConf.ethernetMacAddress, OLD_DEFAULT_MAC_ADDRESS, 6) == 0) {
        uint8_t macAddress[6];
        getSystemDerivedMacAddress(macAddress);
        memcpy(g_devConf.ethernetMacAddress, macAddress, 6);
    }

    // remember this g_devConf to be used to detect when it changes
    memcpy(&g_savedDevConf, &g_devConf, sizeof(DeviceConfiguration));
}

void initChannels() {
    // load channels calibration parameters
    for (int i = 0; i < CH_NUM; ++i) {
        auto &channel = Channel::get(i);
        loadChannelCalibrationConfiguration(channel.slotIndex, channel.subchannelIndex, channel.cal_conf);
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
    memcpy(g_devConf.systemPassword, new_password, new_password_len);
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
    memset(&g_devConf.calibrationPassword, 0, sizeof(g_devConf.calibrationPassword));
    memcpy(g_devConf.calibrationPassword, new_password, new_password_len);
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
        year = g_devConf.dateYear;
        month = g_devConf.dateMonth;
        day = g_devConf.dateDay;
        return true;
    }
    return false;
}

bool isDst() {
    return datetime::isDst(
        datetime::makeTime(
            2000 + g_devConf.dateYear,
            g_devConf.dateMonth,
            g_devConf.dateDay,
            g_devConf.timeHour,
            g_devConf.timeMinute,
            g_devConf.timeSecond
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
    g_devConf.dateYear = year;
    g_devConf.dateMonth = month;
    g_devConf.dateDay = day;

    g_devConf.dateValid = 1;

    setDst(dst);
}

bool readSystemTime(uint8_t &hour, uint8_t &minute, uint8_t &second) {
    if (g_devConf.timeValid) {
        hour = g_devConf.timeHour;
        minute = g_devConf.timeMinute;
        second = g_devConf.timeSecond;
        return true;
    }
    return false;
}

void writeSystemTime(uint8_t hour, uint8_t minute, uint8_t second, unsigned dst) {
    g_devConf.timeHour = hour;
    g_devConf.timeMinute = minute;
    g_devConf.timeSecond = second;

    g_devConf.timeValid = 1;

    setDst(dst);
}

void writeSystemDateTime(uint8_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t minute,
                         uint8_t second, unsigned dst) {
    g_devConf.dateYear = year;
    g_devConf.dateMonth = month;
    g_devConf.dateDay = day;

    g_devConf.dateValid = 1;

    g_devConf.timeHour = hour;
    g_devConf.timeMinute = minute;
    g_devConf.timeSecond = second;

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
    g_devConf.profileAutoRecallLocation = (int8_t)location;
    event_queue::pushEvent(event_queue::EVENT_INFO_DEFAULE_PROFILE_CHANGED_TO_0 + location);
}

int getProfileAutoRecallLocation() {
    return g_devConf.profileAutoRecallLocation;
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
    if (isMaxView()) {
        if (CH_NUM > 0) {
            setChannelsViewModeInMax(g_devConf.channelsViewModeInMax + 1);
        }
    } else {
        if (CH_NUM > 0) {
            setChannelsViewMode(g_devConf.channelsViewMode + 1);
        } else {
            if (g_devConf.channelsViewMode == CHANNELS_VIEW_MODE_NUMERIC) {
                setChannelsViewMode(CHANNELS_VIEW_MODE_HORZ_BAR);
            } else {
                setChannelsViewMode(CHANNELS_VIEW_MODE_NUMERIC);
            }
        }
    }
}

bool isMaxView() {
    return getMaxSlotIndex() >= 0;
}

int getMaxSlotIndex() {
    return g_devConf.maxSlotIndex - 1;
}

void setMaxSlotIndex(int slotIndex) {
    g_devConf.maxSlotIndex = slotIndex + 1;
    g_devConf.maxSubchannelIndex = 0;
}

int getMaxSubchannelIndex() {
    return g_devConf.maxSubchannelIndex;
}

void toggleMaxSlotIndex(int slotIndex) {
    if (isMaxView() && slotIndex == getMaxSlotIndex()) {
        g_devConf.maxSlotIndex = 0;
        g_devConf.maxSubchannelIndex = 0;
    } else {
        setMaxSlotIndex(slotIndex);
    }
}

int getMaxChannelIndex() {
    auto channel = Channel::getBySlotIndex(getMaxSlotIndex());
    return channel ? channel->channelIndex + g_devConf.maxSubchannelIndex : -1;
}

void setMaxChannelIndex(int channelIndex) {
    auto &channel = Channel::get(channelIndex);
    g_devConf.maxSlotIndex = channel.slotIndex + 1;
    g_devConf.maxSubchannelIndex = channel.subchannelIndex;
}

void toggleMaxChannelIndex(int channelIndex) {
    if (isMaxView() && channelIndex == getMaxChannelIndex()) {
        g_devConf.maxSlotIndex = 0;
        g_devConf.maxSubchannelIndex = 0;
    } else {
        setMaxChannelIndex(channelIndex);
    }
}

////////////////////////////////////////////////////////////////////////////////

void resetAllExceptOnTimeCountersMCU() {
	uint8_t buffer[64];

	memset(buffer, 0xFF, 64);

	uint32_t address;

	for (address = 0; address < MCU_EEPROM_ONTIME_START_ADDRESS; address += 64) {
		WATCHDOG_RESET(WATCHDOG_LONG_OPERATION);
		eeprom::write(buffer, MIN(MCU_EEPROM_ONTIME_START_ADDRESS - address, 64), (uint16_t)address);
	}

	for (address = MCU_EEPROM_ONTIME_START_ADDRESS + 6 * sizeof(uint32_t); address < eeprom::EEPROM_SIZE; address += 64) {
		WATCHDOG_RESET(WATCHDOG_LONG_OPERATION);
		eeprom::write(buffer, MIN(eeprom::EEPROM_SIZE - address, 64), (uint16_t)address);
	}
}

void resetAllExceptOnTimeCountersBP3C(uint8_t slotIndex) {
    uint8_t buffer[64];
    
    memset(buffer, 0xFF, 64);

    uint32_t address;

    for (address = 0; address < BP3C_EEPROM_ONTIME_START_ADDRESS; address += 64) {
        WATCHDOG_RESET(WATCHDOG_LONG_OPERATION);
        bp3c::eeprom::write(slotIndex, buffer, MIN(BP3C_EEPROM_ONTIME_START_ADDRESS - address, 64), (uint16_t)address);
    }

    for (address = BP3C_EEPROM_ONTIME_START_ADDRESS + 6 * sizeof(uint32_t); address < bp3c::eeprom::EEPROM_SIZE; address += 64) {
        WATCHDOG_RESET(WATCHDOG_LONG_OPERATION);
		bp3c::eeprom::write(slotIndex, buffer, MIN(bp3c::eeprom::EEPROM_SIZE - address, 64), (uint16_t)address);
    }

}

uint32_t readTotalOnTime(int type) {
    uint32_t buffer[6];

    bool result;

    for (int i = 0; i < NUM_RETRIES; i++) {
        if (type == ontime::ON_TIME_COUNTER_MCU) {
            result = confRead((uint8_t *)buffer, sizeof(buffer), MCU_EEPROM_ONTIME_START_ADDRESS, -1);
        } else {
            result = moduleConfRead(
                type - ontime::ON_TIME_COUNTER_SLOT1,
                (uint8_t *)buffer, 
                sizeof(buffer), 
                BP3C_EEPROM_ONTIME_START_ADDRESS,
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
        // static uint32_t lastTime = time;
        // DebugTrace("On-time counter: %u, systick: %u\n", time, millis());
        // if (lastTime != 0 && (time - lastTime) >= 2 * WRITE_ONTIME_INTERVAL) {
        //     psu::gui::errorMessage("On time counter alert!");
        // }
        // lastTime = time;

        return confWrite((uint8_t *)buffer, sizeof(buffer), MCU_EEPROM_ONTIME_START_ADDRESS);
    } else {
        return moduleConfWrite(
            type - ontime::ON_TIME_COUNTER_SLOT1,
            (uint8_t *)buffer, 
            sizeof(buffer), 
            BP3C_EEPROM_ONTIME_START_ADDRESS
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

void setUsbMode(int usbMode) {
    g_devConf.usbMode = usbMode;
}

int getUsbMode() {
    return g_devConf.usbMode;
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

const char *validateEthernetHostName(const char *hostName) {
    if (strlen(hostName) > ETHERNET_HOST_NAME_SIZE) {
#define MSG(X) "Max. "#X" characters are allowed."
        return MSG(ETHERNET_HOST_NAME_SIZE);
#undef MSG
    }

    for (const char *p = hostName; *p; p++) {
        if (!isalnum(*p) && !(p > hostName && *p == '-')) {
            return "Valid characters are letters from a to z,\nthe digits from 0 to 9, and the hyphen(-).\nA hostname may not start with a hyphen.";
        }
    }

    return nullptr;
}

bool setEthernetHostName(const char *hostName) {
#if OPTION_ETHERNET
    if (!g_devConf.skipEthernetSetup) {
        stringCopy(g_devConf.ethernetHostName, sizeof(g_devConf.ethernetHostName), hostName);
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
        stringCopy(g_devConf.ethernetHostName, sizeof(g_devConf.ethernetHostName), hostName);

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
    memcpy(g_devConf.ntpServer, ntpServer, ntpServerLength);
    g_devConf.ntpServer[ntpServerLength] = 0;
}

void setNtpRefreshFrequency(uint32_t ntpRefreshFrequency) {
    g_devConf.ntpRefreshFrequency = ntpRefreshFrequency;
}

void setNtpSettings(bool enable, const char *ntpServer, uint32_t ntpRefreshFrequency) {
    g_devConf.ntpEnabled = enable ? 1 : 0;
    stringCopy(g_devConf.ntpServer, sizeof(g_devConf.ntpServer), ntpServer);
    g_devConf.ntpRefreshFrequency = ntpRefreshFrequency;
}

bool setMqttSettings(bool enable, const char *host, uint16_t port, const char *username, const char *password, float period) {
#if OPTION_ETHERNET
    bool reconnectRequired = g_devConf.mqttEnabled != enable ||
        strcmp(g_devConf.mqttHost, host) != 0 ||
        g_devConf.mqttPort != port ||
        strcmp(g_devConf.mqttUsername, username) != 0 ||
        strcmp(g_devConf.mqttPassword, password) != 0;

    g_devConf.mqttEnabled = enable ? 1 : 0;
    stringCopy(g_devConf.mqttHost, sizeof(g_devConf.mqttHost), host);
    g_devConf.mqttPort = port;
    stringCopy(g_devConf.mqttUsername, sizeof(g_devConf.mqttUsername), username);
    stringCopy(g_devConf.mqttPassword, sizeof(g_devConf.mqttPassword), password);
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

void setTouchscreenCalParams(int16_t touchScreenCalTlx, int16_t touchScreenCalTly, int16_t touchScreenCalBrx, int16_t touchScreenCalBry, int16_t touchScreenCalTrx, int16_t touchScreenCalTry) {
    g_devConf.touchScreenCalTlx = touchScreenCalTlx;
    g_devConf.touchScreenCalTly = touchScreenCalTly;
    g_devConf.touchScreenCalBrx = touchScreenCalBrx;
    g_devConf.touchScreenCalBry = touchScreenCalBry;
    g_devConf.touchScreenCalTrx = touchScreenCalTrx;
    g_devConf.touchScreenCalTry = touchScreenCalTry;
}

void setFanSettings(uint8_t fanMode, uint8_t fanSpeedPercentage, uint8_t fanSpeedPWM) {
    g_devConf.fanMode = fanMode;
    g_devConf.fanSpeedPercentage = fanSpeedPercentage;
    g_devConf.fanSpeedPWM = fanSpeedPWM;
}

void setDateValid(unsigned dateValid) {
    g_devConf.dateValid = dateValid;
}

void setTimeValid(unsigned timeValid) {
    g_devConf.timeValid = timeValid;
}

void setTimeZone(int16_t timeZone) {
    g_devConf.timeZone = timeZone;
}

void setDstRule(uint8_t dstRule) {
    g_devConf.dstRule = dstRule;
}

void setDateTimeFormat(unsigned dateTimeFormat) {
    g_devConf.dateTimeFormat = dateTimeFormat;
}

void setSelectedThemeIndex(uint8_t selectedThemeIndex) {
    g_devConf.selectedThemeIndex = selectedThemeIndex;
}

void setSkipChannelCalibrations(unsigned skipChannelCalibrations) {
    g_devConf.skipChannelCalibrations = skipChannelCalibrations;
}

void setSkipDateTimeSetup(unsigned skipDateTimeSetup) {
    g_devConf.skipDateTimeSetup = skipDateTimeSetup;
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

void setEventQueueFilter(int eventQueueFilter) {
    g_devConf.eventQueueFilter = eventQueueFilter;
}

void setIsInhibitedByUser(int isInhibitedByUser) {
    g_devConf.isInhibitedByUser = isInhibitedByUser;
}

void setDlogViewLegendViewOption(DlogViewLegendViewOption dlogViewLegendViewOption) {
    g_devConf.viewFlags.dlogViewLegendViewOption = dlogViewLegendViewOption;
}

void setDlogViewShowLabels(bool showLabels) {
    g_devConf.viewFlags.dlogViewShowLabels = showLabels;
}

void setDlogViewDrawerIsOpen(bool drawerIsOpen) {
    g_devConf.viewFlags.dlogViewDrawerIsOpen = drawerIsOpen;
}

bool isSlotEnabled(int slotIndex) {
    return (devConf.slotEnabledBits & (1 << slotIndex)) != 0;
}

void setSlotEnabled(int slotIndex, bool enabled) {
    if (enabled) {
        g_devConf.slotEnabledBits |= 1 << slotIndex;
    } else {
        g_devConf.slotEnabledBits &= ~(1 << slotIndex);
    }
}

int getPowerLineFrequency() {
    return g_devConf.powerLineFrequencyMode == 0 ? 50 : 60;
}

void setPowerLineFrequency(int powerLineFrequency) {
    if (powerLineFrequency == 50) {
        g_devConf.powerLineFrequencyMode = 0;
    } else if (powerLineFrequency == 60) {
        g_devConf.powerLineFrequencyMode = 1;
    }
}

void setMcuRevision(int mcuRevision) {
    g_devConf.mcuRevisionTag = MCU_REVISION_TAG;
    g_devConf.mcuRevision = mcuRevision;
}

void clearMcuRevision() {
    g_devConf.mcuRevisionTag = 0;
    g_devConf.mcuRevision = 0;
}

void setOverlayVisibility(uint8_t overlayVisibility) {
	g_devConf.overlayVisibility = overlayVisibility;
}

void setOverlayPositions(int xOffsetMinimized, int yOffsetMinimized, int xOffsetMaximized, int yOffsetMaximized) {
	g_devConf.overlayMoved = true;
    g_devConf.overlayXOffsetMinimized = xOffsetMinimized;
    g_devConf.overlayYOffsetMinimized = yOffsetMinimized;
    g_devConf.overlayXOffsetMaximized = xOffsetMaximized;
    g_devConf.overlayYOffsetMaximized = yOffsetMaximized;
}

////////////////////////////////////////////////////////////////////////////////

ModuleConfiguration g_moduleConf[NUM_SLOTS];

struct ModuleSerialNo {
    BlockHeader header;
    uint32_t idw0;
    uint32_t idw1;
    uint32_t idw2;
};

bool loadSerialNo(int slotIndex) {
    assert(sizeof(ModuleSerialNo) <= bp3c::eeprom::EEPROM_SERIAL_NO_BLOCK_SIZE);

    uint8_t buffer[bp3c::eeprom::EEPROM_SERIAL_NO_BLOCK_SIZE];

    if (moduleConfRead(
        slotIndex, 
        buffer, 
        bp3c::eeprom::EEPROM_SERIAL_NO_BLOCK_SIZE,
        bp3c::eeprom::EEPROM_SERIAL_NO_BLOCK_START_ADDRESS,
        MODULE_SERIAL_NO_VERSION
    )) {
        ModuleSerialNo moduleSerialNo;
        memcpy(&moduleSerialNo, buffer, sizeof(ModuleSerialNo));
        g_slots[slotIndex]->idw0 = moduleSerialNo.idw0;
        g_slots[slotIndex]->idw1 = moduleSerialNo.idw1;
        g_slots[slotIndex]->idw2 = moduleSerialNo.idw2;
        return true;
    } else {
        return false;
    }
}

bool saveSerialNo(int slotIndex) {
    if (!isPsuThread()) {
        sendMessageToPsu(PSU_MESSAGE_SAVE_SERIAL_NO, slotIndex);
        return true;
    }

    uint8_t buffer[bp3c::eeprom::EEPROM_SERIAL_NO_BLOCK_SIZE];

    ModuleSerialNo moduleSerialNo;
    moduleSerialNo.idw0 = g_slots[slotIndex]->idw0;
    moduleSerialNo.idw1 = g_slots[slotIndex]->idw1;
    moduleSerialNo.idw2 = g_slots[slotIndex]->idw2;

    memcpy(buffer, &moduleSerialNo, sizeof(ModuleSerialNo));

    size_t len = bp3c::eeprom::EEPROM_SERIAL_NO_BLOCK_SIZE - sizeof(ModuleSerialNo);
    if (len > 0) {
        memset(buffer + sizeof(ModuleSerialNo), 0, len);
    }

    return moduleSave(
        slotIndex, 
        (BlockHeader *)buffer, 
        bp3c::eeprom::EEPROM_SERIAL_NO_BLOCK_SIZE,
        bp3c::eeprom::EEPROM_SERIAL_NO_BLOCK_START_ADDRESS,
        MODULE_SERIAL_NO_VERSION
    );
}

static void initModuleConf(int slotIndex) {
    ModuleConfiguration &moduleConf = g_moduleConf[slotIndex];

    memset(&moduleConf, 0, sizeof(ModuleConfiguration));

    moduleConf.header.checksum = 0;
    moduleConf.header.version = MODULE_CONF_VERSION;

    moduleConf.chCalEnabled = 0x00;
}

void loadModuleConf(int slotIndex) {
    assert(sizeof(ModuleConfiguration) <= bp3c::eeprom::MODULE_PERSIST_CONF_BLOCK_MODULE_CONFIGURATION_SIZE);

    uint8_t buffer[bp3c::eeprom::MODULE_PERSIST_CONF_BLOCK_MODULE_CONFIGURATION_SIZE];

    if (moduleConfRead(
        slotIndex, 
        buffer, 
        bp3c::eeprom::MODULE_PERSIST_CONF_BLOCK_MODULE_CONFIGURATION_SIZE,
        bp3c::eeprom::MODULE_PERSIST_CONF_BLOCK_MODULE_CONFIGURATION_ADDRESS,
        MODULE_CONF_VERSION
    )) {
        ModuleConfiguration &moduleConf = g_moduleConf[slotIndex];
        memcpy(&moduleConf, buffer, sizeof(ModuleConfiguration));
    } else {
    	initModuleConf(slotIndex);
    }
}

bool saveModuleConf(int slotIndex) {
    uint8_t buffer[bp3c::eeprom::MODULE_PERSIST_CONF_BLOCK_MODULE_CONFIGURATION_SIZE];

    ModuleConfiguration &moduleConf = g_moduleConf[slotIndex];
    memcpy(buffer, &moduleConf, sizeof(ModuleConfiguration));

    memset(buffer + sizeof(ModuleConfiguration), 0, bp3c::eeprom::MODULE_PERSIST_CONF_BLOCK_MODULE_CONFIGURATION_SIZE - sizeof(ModuleConfiguration));

    return moduleSave(
        slotIndex, 
        (BlockHeader *)buffer, 
        bp3c::eeprom::MODULE_PERSIST_CONF_BLOCK_MODULE_CONFIGURATION_SIZE,
        bp3c::eeprom::MODULE_PERSIST_CONF_BLOCK_MODULE_CONFIGURATION_ADDRESS,
        MODULE_CONF_VERSION
    );
}

bool isChannelCalibrationEnabled(int slotIndex, int subchannelIndex) {
    ModuleConfiguration &moduleConf = g_moduleConf[slotIndex];
    return moduleConf.chCalEnabled & (1 << subchannelIndex);
}

void saveCalibrationEnabledFlag(int slotIndex, int subchannelIndex, bool enabled) {
    ModuleConfiguration &moduleConf = g_moduleConf[slotIndex];
    if (enabled) {
        moduleConf.chCalEnabled |= 1 << subchannelIndex;
    } else {
        moduleConf.chCalEnabled &= ~(1 << subchannelIndex);
    }
    saveModuleConf(slotIndex);
}

uint8_t getAfeVersion(int slotIndex) {
    ModuleConfiguration &moduleConf = g_moduleConf[slotIndex];
    return moduleConf.afeVersion;
}

void setAfeVersion(int slotIndex, uint8_t afeVersion) {
    ModuleConfiguration &moduleConf = g_moduleConf[slotIndex];
    moduleConf.afeVersion = afeVersion;
    saveModuleConf(slotIndex);
}

bool loadChannelCalibrationConfiguration(int slotIndex, int subchannelIndex, BlockHeader *calConf, uint32_t calConfSize, int version) {
    return moduleConfRead(
        slotIndex,
        (uint8_t *)calConf,
        calConfSize,
        bp3c::eeprom::MODULE_PERSIST_CONF_CH_CAL_ADDRESS + subchannelIndex * (((calConfSize + 63) / 64) * 64),
        version
    );
}

void loadChannelCalibrationConfiguration(int slotIndex, int subchannelIndex, CalibrationConfiguration &calConf) {
    auto size = sizeof(CalibrationConfiguration);
	assert(bp3c::eeprom::MODULE_PERSIST_CONF_CH_CAL_BLOCK_SIZE >= size);

    if (!moduleConfRead(
        slotIndex,
        (uint8_t *)&calConf,
        sizeof(CalibrationConfiguration),
        bp3c::eeprom::MODULE_PERSIST_CONF_CH_CAL_ADDRESS + subchannelIndex * (((bp3c::eeprom::MODULE_PERSIST_CONF_CH_CAL_BLOCK_SIZE + 63) / 64) * 64),
        CH_CAL_CONF_VERSION
    )) {
        calibration::clearCalibrationConf(&calConf);
    }
}

bool saveChannelCalibrationConfiguration(int slotIndex, int subchannelIndex, const BlockHeader *calConf, uint32_t calConfSize, int version) {
    return moduleSave(
        slotIndex,
        (BlockHeader *)calConf,
        calConfSize,
        bp3c::eeprom::MODULE_PERSIST_CONF_CH_CAL_ADDRESS + subchannelIndex * (((calConfSize + 63) / 64) * 64),
        version
    );
}

bool saveChannelCalibrationConfiguration(int slotIndex, int subchannelIndex, const CalibrationConfiguration &calConf) {
    return moduleSave(
        slotIndex,
        (BlockHeader *)&calConf,
        sizeof(CalibrationConfiguration),
        bp3c::eeprom::MODULE_PERSIST_CONF_CH_CAL_ADDRESS + subchannelIndex * (((bp3c::eeprom::MODULE_PERSIST_CONF_CH_CAL_BLOCK_SIZE + 63) / 64) * 64),
        CH_CAL_CONF_VERSION
    );
}

bool saveChannelCalibration(int slotIndex, int subchannelIndex) {
    Channel *channel = Channel::getBySlotIndex(slotIndex, subchannelIndex);
    if (channel) {
        return saveChannelCalibrationConfiguration(slotIndex, subchannelIndex, channel->cal_conf);
    }
    
    return g_slots[slotIndex]->saveChannelCalibration(subchannelIndex, nullptr);
}

uint32_t readCounter(int slotIndex, int counterIndex) {
    uint32_t buffer[6];

    for (int i = 0; i < NUM_RETRIES; i++) {
        bool result = moduleConfRead(
            slotIndex,
            (uint8_t *)buffer, 
            sizeof(buffer), 
            bp3c::eeprom::MODULE_PERSIST_CONF_COUNTERS_ADDRESS + counterIndex * bp3c::eeprom::COUNTER_SIZE,
            -1
        );

        // is counter on first location valid?
        if (result && buffer[0] == COUNTER_MAGIC && buffer[2] == crc32((uint8_t *)(buffer + 0), 8)) {
            // is counter on second location valid?
            if (buffer[3] == COUNTER_MAGIC && buffer[5] == crc32((uint8_t *)(buffer + 3), 8)) {
                // at which location counter is bigger?
                return buffer[1] > buffer[4] ? buffer[1] : buffer[4];
            }

            // only first location is valid
            return buffer[1];
        } 
        
        if (buffer[3] == COUNTER_MAGIC && buffer[5] == crc32((uint8_t *)(buffer + 3), 8)) {
            // only second location is valid
            return buffer[4];
        }
    }

    // no valid counter stored
    return 0;
}

bool writeCounter(int slotIndex, int counterIndex, uint32_t counter) {
    uint32_t buffer[6];

    // store counter at two locations for extra robustness
    buffer[0] = COUNTER_MAGIC;
    buffer[1] = counter;
    buffer[2] = crc32((uint8_t *)(buffer + 0), 8);

    buffer[3] = COUNTER_MAGIC;
    buffer[4] = counter;
    buffer[5] = crc32((uint8_t *)(buffer + 3), 8);

    return moduleConfWrite(
        slotIndex,
        (uint8_t *)buffer, 
        sizeof(buffer), 
        bp3c::eeprom::MODULE_PERSIST_CONF_COUNTERS_ADDRESS + counterIndex * bp3c::eeprom::COUNTER_SIZE
    );
}

} // namespace persist_conf
} // namespace psu
} // namespace eez

extern "C" void getMacAddress(uint8_t macAddress[]) {
    memcpy(macAddress, eez::psu::persist_conf::g_devConf.ethernetMacAddress, 6);
}
