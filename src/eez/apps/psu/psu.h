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

#pragma once

#include <stdint.h>

#include <eez/util.h>

#include <eez/apps/psu/conf_all.h>

#include <eez/apps/psu/ontime.h>

/// Namespace for the everything from the EEZ.
namespace eez {

enum TestResult { 
    TEST_FAILED, 
    TEST_OK, 
    TEST_CONNECTING, 
    TEST_SKIPPED, 
    TEST_WARNING 
};

/// PSU firmware.
namespace psu {

void boot();

extern bool g_isBooted;

bool powerUp();
void powerDown();
bool isPowerUp();
bool changePowerState(bool up);
void powerDownBySensor();

bool reset();

bool test();

void onProtectionTripped();

void tick();

void setQuesBits(int bit_mask, bool on);
void setOperBits(int bit_mask, bool on);

void generateError(int16_t error);

const char *getCpuModel();
const char *getCpuType();
const char *getCpuEthernetType();

enum MaxCurrentLimitCause {
    MAX_CURRENT_LIMIT_CAUSE_NONE,
    MAX_CURRENT_LIMIT_CAUSE_FAN,
    MAX_CURRENT_LIMIT_CAUSE_TEMPERATURE
};
bool isMaxCurrentLimited();
MaxCurrentLimitCause getMaxCurrentLimitCause();
void limitMaxCurrent(MaxCurrentLimitCause cause);
void unlimitMaxCurrent();

extern volatile bool g_insideInterruptHandler;

extern ontime::Counter g_powerOnTimeCounter;

enum RLState { RL_STATE_LOCAL = 0, RL_STATE_REMOTE = 1, RL_STATE_RW_LOCK = 2 };

extern RLState g_rlState;

extern bool g_rprogAlarm;

} // namespace psu
} // namespace eez

#include <eez/apps/psu/channel.h>
#include <eez/apps/psu/debug.h>
#include <eez/apps/psu/unit.h>
#include <eez/apps/psu/util.h>
