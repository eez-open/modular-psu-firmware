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

#include <stdio.h>

#include <eez/core/assets.h>

#include <eez/flow/flow_defs_v3.h>

namespace eez {

void fixValue(Assets *assets, Value &value) {
    if (value.getType() == VALUE_TYPE_STRING) {
        value.strValue = (const char *)((uint8_t *)&value.int32Value + value.int32Value);
    } else if (value.getType() == VALUE_TYPE_ARRAY) {
        value.arrayValue = (ArrayValue *)((uint8_t *)&value.int32Value + value.int32Value);
        for (uint32_t i = 0; i < value.arrayValue->arraySize; i++) {
            fixValue(assets, value.arrayValue->values[i]);
        }
    }
}

void fixValues(Assets *assets, ListOfAssetsPtr<Value> &values) {
    for (uint32_t i = 0; i < values.count; i++) {
        auto value = values[i];
        fixValue(assets, *value);
    }
}

void fixOffsets(Assets *assets) {
    if (assets->flowDefinition) {
        auto flowDefinition = static_cast<FlowDefinition *>(assets->flowDefinition);

        for (uint32_t i = 0; i < flowDefinition->flows.count; i++) {
            auto flow = flowDefinition->flows[i];
            fixValues(assets, flow->localVariables);
        }

        fixValues(assets, flowDefinition->constants);
        fixValues(assets, flowDefinition->globalVariables);
    }
}

} // namespace eez
