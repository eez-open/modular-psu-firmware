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

#if defined(EEZ_PLATFORM_STM32)
static const uint32_t GUI_STATE_BUFFER_SIZE = 32 * 1024;
#endif
#if defined(EEZ_PLATFORM_SIMULATOR)
static const uint32_t GUI_STATE_BUFFER_SIZE = 64 * 1024;
#endif

namespace eez {
namespace gui {

extern WidgetState *g_widgetStateStart;
extern WidgetState *g_widgetStateEnd;
extern bool g_widgetStateStructureChanged;
extern WidgetCursor g_widgetCursor;

void updateScreen();

void enumRootWidget();

} // namespace gui
} // namespace eez