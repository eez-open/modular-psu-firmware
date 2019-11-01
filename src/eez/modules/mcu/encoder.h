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

#include <eez/gui/data.h>

namespace eez {
namespace mcu {
namespace encoder {

static const uint8_t MAX_MOVING_SPEED = 10;
static const uint8_t MIN_MOVING_SPEED = 1;
static const uint8_t DEFAULT_MOVING_DOWN_SPEED = 8;
static const uint8_t DEFAULT_MOVING_UP_SPEED = 6;

void init();
void read(int &counter, bool &clicked);

enum EncoderMode {
    ENCODER_MODE_AUTO,
    ENCODER_MODE_STEP1,
    ENCODER_MODE_STEP2,
    ENCODER_MODE_STEP3,
    ENCODER_MODE_STEP4,
    ENCODER_MODE_STEP5
};

extern EncoderMode g_encoderMode;

#if defined(EEZ_PLATFORM_SIMULATOR)
void write(int counter, bool clicked);
#endif

void switchEncoderMode();
void setUseSameSpeed(bool enable);

float increment(gui::data::Value value, int counter, float min, float max, int channelIndex, float precision = 1.0f);

} // namespace encoder
} // namespace mcu
} // namespace eez
