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

#if OPTION_DISPLAY
#include <eez/gui/app_context.h>
#endif

namespace eez {

typedef bool (*OnSystemStateChangedCallback)();
extern OnSystemStateChangedCallback g_onSystemStateChangedCallbacks[];
extern int g_numOnSystemStateChangedCallbacks;

static const uint8_t MODULE_TYPE_NONE = 0;
static const uint8_t MODULE_TYPE_DCP505 = 1;
static const uint8_t MODULE_TYPE_DCP405 = 2;

struct SlotInfo {
    uint8_t moduleType; // MODULE_TYPE_...
};

static const int NUM_SLOTS = 3;
extern SlotInfo g_slots[NUM_SLOTS];

#if OPTION_DISPLAY

struct ApplicationInfo {
    const char *title;
    int bitmap;
    gui::AppContext *appContext;
};

typedef bool (*OnSystemStateChangedCallback)();
extern ApplicationInfo g_applications[];
extern int g_numApplications;

namespace gui {

class Page;

Page *getPageFromId(int pageId);

} // namespace gui

#endif

} // namespace eez