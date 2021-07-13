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

#include <stdint.h>
#include <eez/gui/assets_ptr.h>

namespace eez {

namespace gui {
	struct Assets;
}

namespace flow {

template <typename T>
struct List {
    uint32_t count;
    AssetsPtr<T> first;
};

struct ComponentInput {
    uint8_t count;
    uint16_t valueIndex[255];
};

struct Connection {
    uint16_t targetComponentIndex;
    uint8_t targetInputIndex;
};

struct ComponentOutput {
    uint8_t count;
    Connection connections[255];
};

struct Component {
    uint16_t type;
    List<ComponentInput> inputs;
    List<ComponentOutput> outputs;
    AssetsPtr<const void> specific;
};

struct Flow {
    List<const Component> flows;
};

struct Value {
    uint8_t type;
};

struct FlowDefinition {
    List<const Flow> flows;
    List<Value> values;
};

void run();

} // flow
} // eez