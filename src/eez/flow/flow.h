/*
* EEZ Generic Firmware
* Copyright (C) 2021-present, Envox d.o.o.
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

#include <eez/gui/assets.h>

namespace eez {
namespace flow {

using namespace eez::gui;

unsigned start(eez::gui::Assets *assets);
void tick(unsigned flowHandle);
void scpiResultIsReady();
void stop();

void *getFlowState(int16_t pageId);
void executeFlowAction(unsigned flowHandle, const WidgetCursor &widgetCursor, int16_t actionId);
void doSetFlowValue();
void dataOperation(unsigned flowHandle, int16_t dataId, DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value);

} // flow
} // eez