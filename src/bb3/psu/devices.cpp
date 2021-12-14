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

#include <string.h>
#include <stdio.h>

#include <eez/eeprom.h>

#include <bb3/psu/psu.h>
#include <bb3/psu/devices.h>
#include <bb3/psu/sd_card.h>
#if OPTION_ETHERNET
#include <bb3/psu/ethernet_scpi.h>
#endif
#include <bb3/psu/datetime.h>
#include <bb3/psu/rtc.h>
#include <bb3/psu/temp_sensor.h>
#if OPTION_FAN
#include <bb3/aux_ps/fan.h>
#endif

namespace eez {
namespace psu {
namespace devices {

scpi_choice_def_t g_deviceChoice[] = {
    { "EEProm", DEVICE_ID_EEPROM },
    { "SDCard", DEVICE_ID_SD_CARD },
    { "ETHernet", DEVICE_ID_ETHERNET },
    { "RTC", DEVICE_ID_RTC },
    { "DATEtime", DEVICE_ID_DATETIME },
    { "FAN", DEVICE_ID_FAN },
    { "AUXTemp", DEVICE_ID_AUX_TEMP },
    { "CH1Temp", DEVICE_ID_CH1_TEMP },
    { "CH2Temp", DEVICE_ID_CH2_TEMP },
    { "CH3Temp", DEVICE_ID_CH3_TEMP },
    { "CH4Temp", DEVICE_ID_CH4_TEMP },
    { "CH5Temp", DEVICE_ID_CH5_TEMP },
    { "CH6Temp", DEVICE_ID_CH6_TEMP },
    { "CH1", DEVICE_ID_CH1 },
    { "CH2", DEVICE_ID_CH2 },
    { "CH3", DEVICE_ID_CH3 },
    { "CH4", DEVICE_ID_CH4 },
    { "CH5", DEVICE_ID_CH5 },
    { "CH6", DEVICE_ID_CH6 },
    { "SLOT1", DEVICE_ID_SLOT1 },
    { "SLOT2", DEVICE_ID_SLOT2 },
    { "SLOT3", DEVICE_ID_SLOT3 },
    SCPI_CHOICE_LIST_END    
};

bool getDevice(int deviceIndex, Device &device) {
    int i = 0;

    if (deviceIndex == i++) {
        device.id = DEVICE_ID_EEPROM;
        stringCopy(device.name, sizeof(device.name), "EEPROM");
#if OPTION_EXT_EEPROM        
        device.installed = true;
        device.testResult = eeprom::g_testResult;
#else
        device.installed = false;
        device.testResult = TEST_SKIPPED;
#endif
        return true;
    } 
    
    if (deviceIndex == i++) {
        device.id = DEVICE_ID_SD_CARD;
        stringCopy(device.name, sizeof(device.name), "SD card");
        device.installed = true;
        device.testResult = sd_card::g_testResult;
        return true;
    } 
    
    if (deviceIndex == i++) {
        device.id = DEVICE_ID_ETHERNET;
        stringCopy(device.name, sizeof(device.name), "Ethernet");
#if OPTION_ETHERNET
        device.installed = true;
        device.testResult = ethernet::g_testResult;
#else
        device.installed = false;
        device.testResult = TEST_SKIPPED;
#endif
        return true;
    } 
    
    if (deviceIndex == i++) {
        device.id = DEVICE_ID_RTC;
        stringCopy(device.name, sizeof(device.name), "RTC");
#if OPTION_EXT_RTC
        device.installed = true;
        device.testResult = rtc::g_testResult;
#else
        device.installed = false;
        device.testResult = TEST_SKIPPED;
#endif
        return true;
    } 
    
    if (deviceIndex == i++) {
        device.id = DEVICE_ID_DATETIME;
        stringCopy(device.name, sizeof(device.name), "DateTime");
        device.installed = true;
        device.testResult = datetime::g_testResult;
        return true;
    } 
    
    if (deviceIndex == i++) {
        device.id = DEVICE_ID_FAN;
        stringCopy(device.name, sizeof(device.name), "Fan");
#if OPTION_FAN
        device.installed = true;
        device.testResult = aux_ps::fan::g_testResult;
#else
        device.installed = false;
        device.testResult = TEST_SKIPPED;
#endif
        return true;
    }

    if (deviceIndex == i++) {
        device.id = DEVICE_ID_AUX_TEMP;
        stringCopy(device.name, sizeof(device.name), "AUX temp");
        device.installed = true;
        device.testResult = temp_sensor::sensors[temp_sensor::AUX].g_testResult;
        return true;
    }

    for (int channelIndex = 0; channelIndex < 6; channelIndex++) {
        if (deviceIndex == i++) {
            device.id = (DeviceId)(DEVICE_ID_CH1_TEMP + channelIndex);
            snprintf(device.name, sizeof(device.name), "CH%d temp", channelIndex + 1);
            if (channelIndex < CH_NUM) {
                device.installed = true;
                device.testResult = temp_sensor::sensors[temp_sensor::CH1 + channelIndex].g_testResult;
            } else {
                device.installed = false;
                device.testResult = TEST_NONE;
            }
            return true;
        }
    }

    for (int channelIndex = 0; channelIndex < 6; channelIndex++) {
        if (deviceIndex == i++) {
            device.id = (DeviceId)(DEVICE_ID_CH1 + channelIndex);
            snprintf(device.name, sizeof(device.name), "CH%d", channelIndex + 1);
            if (channelIndex < CH_NUM) {
                device.installed = true;
                device.testResult = Channel::get(channelIndex).getTestResult();
            } else {
                device.installed = false;
                device.testResult = TEST_NONE;
            }
            return true;
        }
    }

    for (int slotIndex = 0; slotIndex < 3; slotIndex++) {
        if (deviceIndex == i++) {
            device.id = (DeviceId)(DEVICE_ID_SLOT1 + slotIndex);
            snprintf(device.name, sizeof(device.name), "SLOT%d", slotIndex + 1);
            if (slotIndex < NUM_SLOTS) {
                device.installed = g_slots[slotIndex]->moduleType != MODULE_TYPE_NONE;
                device.testResult = g_slots[slotIndex]->getTestResult();
            } else {
                device.installed = false;
                device.testResult = TEST_NONE;
            }
            return true;
        }
    }

    return false;
}

bool anyFailed() {
    Device device;
    for (int i = 0; getDevice(i, device); ++i) {
        if (device.testResult == TEST_FAILED) {
            return true;
        }
    }

    return false;
}

#define APPEND_CHAR(C)                                                                             \
    nextIndex = index + 1;                                                                         \
    if (nextIndex > MAX_LENGTH)                                                                    \
        break;                                                                                     \
    *(result + index) = C;                                                                         \
    index = nextIndex;

#define APPEND_STRING(S)                                                                           \
    str = S;                                                                                       \
    strLength = strlen(str);                                                                       \
    nextIndex = index + strLength;                                                                 \
    if (nextIndex > MAX_LENGTH)                                                                    \
        break;                                                                                     \
    memcpy(result + index, str, strLength);                                                        \
    index = nextIndex

void getSelfTestResultString(char *result, int MAX_LENGTH) {
    int index = 0;
    int nextIndex;
    const char *str;
    int strLength;

    Device device;
    for (int deviceIndex = 0; getDevice(deviceIndex, device); deviceIndex++) {
        if (device.testResult == TEST_FAILED) {
            if (index > 0) {
                APPEND_CHAR('\n');
            }
            APPEND_CHAR('-');
            APPEND_CHAR(' ');
            APPEND_STRING(device.name);
            APPEND_CHAR(' ');
            APPEND_STRING(getTestResultString(device.testResult));
        }
    }

    *(result + index) = 0;
}

const char *getInstalledString(bool installed) {
    if (installed)
        return "installed";
    return "not installed";
}

const char *getTestResultString(TestResult g_testResult) {
    if (g_testResult == TEST_FAILED)
        return "failed";
    if (g_testResult == TEST_OK)
        return "passed";
    if (g_testResult == TEST_CONNECTING)
        return "testing";
    if (g_testResult == TEST_SKIPPED)
        return "skipped";
    if (g_testResult == TEST_WARNING)
        return "warning";
    return "";
}

} // namespace devices
} // namespace psu
} // namespace eez
