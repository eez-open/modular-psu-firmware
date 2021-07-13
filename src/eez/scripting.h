/*
* EEZ Generic Firmware
* Copyright (C) 2018-present, Envox d.o.o.
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
#include <stdlib.h>

#include <eez/modules/psu/psu.h>
#include <eez/modules/psu/profile.h>

namespace eez {
namespace scripting {

enum State {
    STATE_IDLE,
    STATE_STARTING,
    STATE_EXECUTING
};

extern State g_state;
extern char *g_scriptPath;

void initMessageQueue();
void startThread();

void onQueueMessage(uint32_t type, uint32_t param);

bool startScript(const char *filePath, int *err = nullptr);
bool stopScript(int *err = nullptr);
inline bool isIdle() { return g_state == STATE_IDLE; }
bool scpi(const char *commandOrQueryText, const char **resultText, size_t *resultTextLen);

void onUncaughtScriptExceptionHook();

void resetSettings();

bool isAutoStartEnabled();
void autoStart();

void resetProfileParameters(psu::profile::Parameters &profileParams);
void getProfileParameters(psu::profile::Parameters &profileParams);
void setProfileParameters(const psu::profile::Parameters &profileParams);
bool writeProfileProperties(psu::profile::WriteContext &ctx, const psu::profile::Parameters &profileParams);
bool readProfileProperties(psu::profile::ReadContext &ctx, psu::profile::Parameters &profileParams);

void getAutoStartConfirmationMessage(char *text, int count);

} // scripting
} // eez
