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

#include <eez/flow/queue.h>

namespace eez {
namespace flow {

static const unsigned QUEUE_SIZE = 100;
static struct {
	FlowState *flowState;
	unsigned componentIndex;
	ComponenentExecutionState *componentExecutionState;
} g_queue[QUEUE_SIZE];
static unsigned g_queueHead;
static unsigned g_queueTail;
static bool g_queueIsFull = false;

void queueInit() {
	g_queueHead = 0;
	g_queueTail = 0;
	g_queueIsFull = false;
}

void addToQueue(FlowState *flowState, unsigned componentIndex, ComponenentExecutionState *componentExecutionState) {
	g_queue[g_queueTail].flowState = flowState;
	g_queue[g_queueTail].componentIndex = componentIndex;
	g_queue[g_queueTail].componentExecutionState = componentExecutionState;

	g_queueTail = (g_queueTail + 1) % QUEUE_SIZE;

	if (g_queueHead == g_queueTail) {
		g_queueIsFull = true;
	}
}

bool removeFromQueue(FlowState *&flowState, unsigned &componentIndex, ComponenentExecutionState *&componentExecutionState) {
	if (g_queueHead == g_queueTail && !g_queueIsFull) {
		return false;
	}

	flowState = g_queue[g_queueHead].flowState;
	componentIndex = g_queue[g_queueHead].componentIndex;
	componentExecutionState = g_queue[g_queueHead].componentExecutionState;

	g_queueHead = (g_queueHead + 1) % QUEUE_SIZE;
	g_queueIsFull = false;

	return true;
}

} // namespace flow
} // namespace eez