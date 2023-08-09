/*
 * EEZ Modular Firmware
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

#include <eez/conf-internal.h>

#include <eez/core/alloc.h>
#include <eez/core/os.h>
#include <eez/core/util.h>

#include <eez/flow/components.h>
#include <eez/flow/flow_defs_v3.h>
#include <eez/flow/expression.h>
#include <eez/flow/queue.h>

namespace eez {
namespace flow {

void executeTestAndSetComponent(FlowState *flowState, unsigned componentIndex) {
    Value dstValue;
    if (!evalAssignableProperty(flowState, componentIndex, defs_v3::TEST_AND_SET_ACTION_COMPONENT_PROPERTY_VARIABLE, dstValue, "Failed to evaluate Variable in TestAndSet")) {
        return;
    }

    if (dstValue.getValue().type != VALUE_TYPE_BOOLEAN) {
        throwError(flowState, componentIndex, "Variable in TestAndSet must be of type Boolean");
        return;
    }

    // waits for the variable to be false, then sets it to true and propagates through the Seqout

    // if the variable is false, set it to true and propagate through the Seqout
    if (!dstValue.getValue().getBoolean()) {
        assignValue(flowState, componentIndex, dstValue, Value(true, VALUE_TYPE_BOOLEAN));
        propagateValueThroughSeqout(flowState, componentIndex);
    } else {
        // otherwise, wait, i.e. add the component to the queue, so that it is executed again
        addToQueue(flowState, componentIndex, -1, -1, -1, true);
    }
}

} // namespace flow
} // namespace eez
