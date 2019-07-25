/*
* EEZ Home Application
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
* along with this program.  If not, see http://www.gnu.org/licenses.
*/

#if OPTION_DISPLAY

#include <eez/apps/home/data.h>

#include <eez/apps/home/home.h>
#include <eez/gui/assets.h>
#include <eez/gui/data.h>
#include <eez/gui/document.h>
#include <eez/index.h>

namespace eez {
namespace home {

// int g_foregroundApplicationIndex = -1;
int g_foregroundApplicationIndex = 0;

} // namespace home
} // namespace eez

using namespace eez::home;

namespace eez {
namespace gui {

void data_applications(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value) {
    if (operation == data::DATA_OPERATION_COUNT) {
        value = g_numApplications;
    }
}

void data_application_title(data::DataOperationEnum operation, data::Cursor &cursor,
                            data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        value = Value(g_applications[cursor.i].title);
    }
}

void data_application_icon(data::DataOperationEnum operation, data::Cursor &cursor,
                           data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        value = Value(g_applications[cursor.i].bitmap);
    }
}

void data_is_home_application_in_foreground(data::DataOperationEnum operation, data::Cursor &cursor,
                                            data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        value = Value(home::g_foregroundApplicationIndex == -1 ? 1 : 0);
    }
}

void data_foreground_application(data::DataOperationEnum operation, data::Cursor &cursor,
                                 data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        value = Value(g_applications[g_foregroundApplicationIndex].appContext);
    }
}

void data_foreground_application_title(data::DataOperationEnum operation, data::Cursor &cursor,
                                       data::Value &value) {
    if (operation == data::DATA_OPERATION_GET) {
        value = Value(g_applications[g_foregroundApplicationIndex].title);
    }
}

} // namespace gui
} // namespace eez

#endif