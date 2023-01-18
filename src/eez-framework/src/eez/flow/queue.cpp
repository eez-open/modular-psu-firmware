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

#include <eez/flow/queue.h>
#include <eez/flow/debugger.h>
#include <eez/flow/flow_defs_v3.h>

namespace eez {
namespace flow {

static const unsigned QUEUE_SIZE = 1000;
static struct {
	FlowState *flowState;
	unsigned componentIndex;
    bool continuousTask;
} g_queue[QUEUE_SIZE];
static unsigned g_queueHead;
static unsigned g_queueTail;
static bool g_queueIsFull = false;

void queueReset() {
	g_queueHead = 0;
	g_queueTail = 0;
	g_queueIsFull = false;
}

size_t getQueueSize() {
	if (g_queueHead == g_queueTail) {
		if (g_queueIsFull) {
			return QUEUE_SIZE;
		}
		return 0;
	}

	if (g_queueHead < g_queueTail) {
		return g_queueTail - g_queueHead;
	}

	return QUEUE_SIZE - g_queueHead + g_queueTail;
}

bool addToQueue(FlowState *flowState, unsigned componentIndex, int sourceComponentIndex, int sourceOutputIndex, int targetInputIndex, bool continuousTask) {
	if (g_queueIsFull) {
        throwError(flowState, componentIndex, "Execution queue is full\n");
		return false;
	}

	g_queue[g_queueTail].flowState = flowState;
	g_queue[g_queueTail].componentIndex = componentIndex;
    g_queue[g_queueTail].continuousTask = continuousTask;

	g_queueTail = (g_queueTail + 1) % QUEUE_SIZE;

	if (g_queueHead == g_queueTail) {
		g_queueIsFull = true;
	}

    if (!continuousTask) {
	    onAddToQueue(flowState, sourceComponentIndex, sourceOutputIndex, componentIndex, targetInputIndex);
    }

	return true;
}

bool peekNextTaskFromQueue(FlowState *&flowState, unsigned &componentIndex, bool &continuousTask) {
	if (g_queueHead == g_queueTail && !g_queueIsFull) {
		return false;
	}

	flowState = g_queue[g_queueHead].flowState;
	componentIndex = g_queue[g_queueHead].componentIndex;
    continuousTask = g_queue[g_queueHead].continuousTask;

	return true;
}

void removeNextTaskFromQueue() {
    auto continuousTask = g_queue[g_queueHead].continuousTask;

	g_queueHead = (g_queueHead + 1) % QUEUE_SIZE;
	g_queueIsFull = false;

    if (!continuousTask) {
	    onRemoveFromQueue();
    }
}

bool isThereAnyTaskInQueueForFlowState(FlowState *flowState, bool includingWatchVariable) {
	if (g_queueHead == g_queueTail && !g_queueIsFull) {
		return false;
	}

    unsigned int it = g_queueHead;
    while (true) {
		if (g_queue[it].flowState == flowState) {
            auto component = flowState->flow->components[g_queue[it].componentIndex];
            if (includingWatchVariable || component->type != defs_v3::COMPONENT_TYPE_WATCH_VARIABLE_ACTION) {
			    return true;
            }
		}

        it = (it + 1) % QUEUE_SIZE;
        if (it == g_queueTail) {
            break;
        }
	}

    return false;
}

} // namespace flow
} // namespace eez
