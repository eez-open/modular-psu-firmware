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

#include <eez/modules/psu/psu.h>

#include <string.h>

#include <eez/modules/psu/devices.h>

#include <eez/modules/mcu/eeprom.h>

#if OPTION_SD_CARD
#include <eez/modules/psu/sd_card.h>
#endif

#if OPTION_ETHERNET
#include <eez/modules/psu/ethernet.h>
#endif

#include <eez/modules/psu/datetime.h>
#include <eez/modules/psu/rtc.h>
#include <eez/modules/psu/temp_sensor.h>

#if OPTION_FAN
#include <eez/modules/aux_ps/fan.h>
#endif

namespace eez {
namespace psu {
namespace devices {

#define TEMP_SENSOR(NAME, QUES_REG_BIT, SCPI_ERROR) \
    { #NAME " temp", true, &temp_sensor::sensors[temp_sensor::NAME].g_testResult }

#define CHANNEL(INDEX) \
    { "CH" #INDEX " IOEXP", true, &(Channel::get(INDEX - 1).ioexp.g_testResult) }, \
    { "CH" #INDEX " DAC", true, &(Channel::get(INDEX - 1).dac.g_testResult) }, \
	{ "CH" #INDEX " ADC", true, &(Channel::get(INDEX - 1).adc.g_testResult) }

Device devices[] = {
#if OPTION_EXT_EEPROM
    { "EEPROM", OPTION_EXT_EEPROM, &mcu::eeprom::g_testResult },
#else
    { "EEPROM", 0, 0 },
#endif

#if OPTION_SD_CARD
    { "SD card", OPTION_SD_CARD, &sd_card::g_testResult },
#else
    { "SD card", 0, 0 },
#endif

#if OPTION_ETHERNET
    { "Ethernet", 1, &ethernet::g_testResult },
#else
    { "Ethernet", 0, 0 },
#endif

#if OPTION_EXT_RTC
    { "RTC", OPTION_EXT_RTC, &rtc::g_testResult },
#else
    { "RTC", 0, 0 },
#endif

    { "DateTime", true, &datetime::g_testResult },

#if OPTION_BP
    { "BP option", OPTION_BP, &bp::g_testResult },
#else
    { "BP option", 0, 0 },
#endif

#if OPTION_FAN
    { "Fan", OPTION_FAN, &aux_ps::fan::g_testResult },
#endif

    TEMP_SENSORS
};

#undef TEMP_SENSOR
#undef CHANNEL

int numDevices = sizeof(devices) / sizeof(Device);

bool anyFailed() {
    for (int i = 0; i < numDevices; ++i) {
        Device &device = devices[i];
        if (device.testResult != 0 && *device.testResult == TEST_FAILED) {
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
    strncpy(result + index, str, strLength);                                                       \
    index = nextIndex

void getSelfTestResultString(char *result, int MAX_LENGTH) {
    int index = 0;
    int nextIndex;
    const char *str;
    int strLength;

    for (int deviceIndex = 0; deviceIndex < numDevices; ++deviceIndex) {
        Device &device = devices[deviceIndex];
        if (device.testResult && *device.testResult == TEST_FAILED) {
            if (index > 0) {
                APPEND_CHAR('\n');
            }
            APPEND_CHAR('-');
            APPEND_CHAR(' ');
            APPEND_STRING(device.deviceName);
            APPEND_CHAR(' ');
            APPEND_STRING(getTestResultString(*device.testResult));
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
    if (g_testResult == TEST_OK)
        return "passed";
    if (g_testResult == TEST_SKIPPED)
        return "skipped";
    if (g_testResult == TEST_WARNING)
        return "warning";
    return "failed";
}

} // namespace devices
} // namespace psu
} // namespace eez
