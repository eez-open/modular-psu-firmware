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

#include <scpi/scpi.h>

namespace eez {
namespace psu {
namespace devices {

enum DeviceId {
    DEVICE_ID_EEPROM,
    DEVICE_ID_SD_CARD,
    DEVICE_ID_ETHERNET,
    DEVICE_ID_RTC,
    DEVICE_ID_DATETIME,
    DEVICE_ID_FAN,
    DEVICE_ID_AUX_TEMP,
    DEVICE_ID_CH1_TEMP,
    DEVICE_ID_CH2_TEMP,
    DEVICE_ID_CH3_TEMP,
    DEVICE_ID_CH4_TEMP,
    DEVICE_ID_CH5_TEMP,
    DEVICE_ID_CH6_TEMP,
    DEVICE_ID_CH1,
    DEVICE_ID_CH2,
    DEVICE_ID_CH3,
    DEVICE_ID_CH4,
    DEVICE_ID_CH5,
    DEVICE_ID_CH6,
};

extern scpi_choice_def_t g_deviceChoice[];

struct Device {
    DeviceId id;
    char name[20];
    bool installed;
    TestResult testResult;
};

bool getDevice(int deviceIndex, Device &device);

bool anyFailed();
void getSelfTestResultString(char *, int MAX_LENGTH);

const char *getInstalledString(bool installed);
const char *getTestResultString(TestResult g_testResult);

} // namespace devices
} // namespace psu
} // namespace eez
