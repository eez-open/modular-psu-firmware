/*
* EEZ Generic Firmware
* Copyright (C) 2020-present, Envox d.o.o.
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

namespace eez {

enum TestResult {
    TEST_NONE,
    TEST_FAILED,
    TEST_OK,
    TEST_CONNECTING,
    TEST_SKIPPED,
    TEST_WARNING
};

extern bool g_isBooted;
extern bool g_bootTestSuccess;
extern bool g_shutdownInProgress;
extern bool g_shutdown;
    
void boot();
bool test();
bool testMaster();
bool reset();
void standBy();
void restart();
void shutdown();

} // namespace eez

extern "C" int g_mcuRevision;
