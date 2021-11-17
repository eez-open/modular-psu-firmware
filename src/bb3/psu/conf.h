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

/** @page conf_h Configuration
@brief Compile time configuration.
This file is used to define compile time configuration options.
Use `conf_user.h` file to override anything from here.
option.
*/

// clang-format off

#pragma once

/// Wait until serial port is ready before starting firmware.
#define CONF_WAIT_SERIAL 0

// Data rate in bits per second (baud) for serial data transmission.
#define SERIAL_SPEED 115200

/// Maximum number of channels existing.
#define CH_MAX 6

/// Min. delay between power down and power up.
#define MIN_POWER_UP_DELAY 5

/// Default calibration password.
#define CALIBRATION_PASSWORD_DEFAULT "eezbb3"

/// Is OTP enabled by default?
#define OTP_AUX_DEFAULT_STATE 1

/// Default OTP delay
#define OTP_AUX_DEFAULT_DELAY 30.0f

/// Default OTP level
#define OTP_AUX_DEFAULT_LEVEL 50.0f

/// Is channel OTP enabled by default?
#define OTP_CH_DEFAULT_STATE 1

/// Default channel OTP delay
#define OTP_CH_DEFAULT_DELAY 30.0f

/// Default channel OTP level
#define OTP_CH_DEFAULT_LEVEL 60.0f
