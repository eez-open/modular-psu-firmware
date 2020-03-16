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

namespace eez {
namespace sound {

void init();
void doPlay();

/// Play power up tune.
enum PlayPowerUpCondition {
    PLAY_POWER_UP_CONDITION_NONE,
    PLAY_POWER_UP_CONDITION_TEST_SUCCESSFUL,
    PLAY_POWER_UP_CONDITION_WELCOME_PAGE_IS_ACTIVE
};

void playPowerUp(PlayPowerUpCondition condition);

/// Play power down tune.
void playPowerDown();

/// Play beep sound.
void playBeep(bool force = false);

/// Play click sound
void playClick();

/// Play shutter sound
void playShutter();

} // namespace sound
} // namespace eez
