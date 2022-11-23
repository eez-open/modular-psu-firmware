/*
 * EEZ Modular Firmware
 * Copyright (C) 2022-present, Envox d.o.o.
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

#include <eez/core/action.h>

namespace eez {

#if EEZ_OPTION_GUI || !defined(EEZ_OPTION_GUI)
namespace gui {
#endif

void executeActionFunction(int actionId) {
#if defined(EEZ_FOR_LVGL)
	g_actionExecFunctions[actionId](0);
#else
    g_actionExecFunctions[actionId]();
#endif
}

#if EEZ_OPTION_GUI || !defined(EEZ_OPTION_GUI)
} // gui
#endif

} // eez
