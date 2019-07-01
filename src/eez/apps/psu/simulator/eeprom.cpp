/*
* EEZ PSU Firmware
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

#include <eez/apps/psu/psu.h>

#include <stdio.h>

#include <eez/apps/psu/eeprom.h>

namespace eez {
namespace psu {
namespace eeprom {

TestResult g_testResult = TEST_FAILED;

bool read(uint8_t *buffer, uint16_t buffer_size, uint16_t address) {
    char *file_path = getConfFilePath("EEPROM.state");
    FILE *fp = fopen(file_path, "r+b");
    if (fp == NULL) {
        fp = fopen(file_path, "w+b");
    }
    if (fp == NULL) {
        return false;
    }

    fseek(fp, address, SEEK_SET);
    size_t readBytes = fread(buffer, 1, buffer_size, fp);
    fclose(fp);

    return readBytes == buffer_size;
}

bool write(const uint8_t *buffer, uint16_t buffer_size, uint16_t address) {
    char *file_path = getConfFilePath("EEPROM.state");
    FILE *fp = fopen(file_path, "r+b");
    if (fp == NULL) {
        fp = fopen(file_path, "w+b");
    }
    if (fp == NULL) {
        return false;
    }

    fseek(fp, address, SEEK_SET);
    fwrite(buffer, 1, buffer_size, fp);
    fclose(fp);

    return true;
}

void init() {
}

bool test() {
#if OPTION_EXT_EEPROM
    g_testResult = TEST_OK;
#else
    g_testResult = TEST_SKIPPED;
#endif

    return g_testResult != TEST_FAILED;
}

} // namespace eeprom
} // namespace psu
} // namespace eez
