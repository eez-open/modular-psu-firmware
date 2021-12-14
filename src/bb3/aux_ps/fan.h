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

#include <stdint.h>

#include <eez/os.h>
#include <bb3/firmware.h>

#define FAN_MODE_AUTO 0
#define FAN_MODE_MANUAL 1

enum FanStatus {
    FAN_STATUS_INVALID,
    FAN_STATUS_VALID,
    FAN_STATUS_TESTING,
    FAN_STATUS_NOT_INSTALLED
};

namespace eez {
namespace aux_ps {
namespace fan {

extern TestResult g_testResult;
extern int g_rpm;

void init();
bool test();
void tick();

extern double g_Kp;
extern double g_Ki;
extern double g_Kd;
extern int g_POn;

void setPidTunings(double Kp, double Ki, double Kd, int POn);

float readTemperature();

FanStatus getStatus();

} // namespace fan
} // namespace aux_ps
} // namespace eez
