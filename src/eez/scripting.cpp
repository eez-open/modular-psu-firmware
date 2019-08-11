/*
 * EEZ Modular PSU Firmware
 * Copyright (C) 2019-present, Envox d.o.o.
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

#include <eez/scripting.h>

#include <eez/util.h>

namespace eez {
namespace scripting {

Script g_scripts[MAX_NUM_SCRIPTS];
int g_numScripts;
int g_currentPageIndex;
ScriptsPageMode g_scriptsPageMode = SCRIPTS_PAGE_MODE_SCRIPTS;

int getNumPages() {
    return g_numScripts / SCRIPTS_PER_PAGE;
}

int getNumScriptsInCurrentPage() {
    return MIN(g_numScripts - g_currentPageIndex * SCRIPTS_PER_PAGE, SCRIPTS_PER_PAGE);
}

const char *getScriptName(int indexInsidePage) {
    return g_scripts[g_currentPageIndex * SCRIPTS_PER_PAGE + indexInsidePage].fileName;
}

} // namespace scripting
} // namespace eez
