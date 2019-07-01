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

#if !defined(EEZ_PLATFORM_SIMULATOR) && !defined(EEZ_PLATFORM_STM32)
#define EEZ_PLATFORM_STM32
#endif

#if defined(EEZ_PLATFORM_SIMULATOR)
#include <eez/apps/psu/simulator/psu.h>
#elif defined(EEZ_PLATFORM_STM32)
#include <eez/apps/psu/stm32/psu.h>
#endif

#include <eez/apps/psu/conf_user_revision.h>

#include <eez/apps/psu/conf.h>
#include <eez/apps/psu/conf_advanced.h>
#include <eez/apps/psu/conf_channel.h>
#include <eez/apps/psu/conf_user.h>

#if defined(EEZ_PLATFORM_SIMULATOR)
#include <eez/apps/psu/simulator/conf.h>
#elif defined(EEZ_PLATFORM_STM32)
#include <eez/apps/psu/stm32/conf.h>
#endif
