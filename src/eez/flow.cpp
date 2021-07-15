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

#include <stdio.h>

#include <eez/flow.h>
#include <eez/flow_defs_v3.h>

using namespace eez::gui;

namespace eez {
namespace flow {

static Assets *g_assets;

static const unsigned QUEUE_SIZE = 100;
static struct {
    unsigned flowIndex;
	unsigned componentIndex;
} g_queue[QUEUE_SIZE];
static unsigned g_queueHead;
static unsigned g_queueTail;
static bool g_queueIsFull = false;

void addToQueue(unsigned flowIndex, unsigned componentIndex) {
	g_queue[g_queueTail].flowIndex = flowIndex;
	g_queue[g_queueTail].componentIndex = componentIndex;

	g_queueTail = (g_queueTail + 1) % QUEUE_SIZE;

	if (g_queueHead == g_queueTail) {
		g_queueIsFull = true;
	}
}

bool removeFromQueue(unsigned &flowIndex, unsigned &componentIndex) {
	if (g_queueHead == g_queueTail && !g_queueIsFull) {
		return false;
	}

	flowIndex = g_queue[g_queueHead].flowIndex;
	componentIndex = g_queue[g_queueHead].componentIndex;

	g_queueHead = (g_queueHead + 1) % QUEUE_SIZE;
	g_queueIsFull = false;

	return true;
}

bool allInputsDefined(FlowDefinition &flowDefinition, unsigned flowIndex, unsigned componentIndex) {
	auto &flow = flowDefinition.flows.first[flowIndex];
	auto &component = flow.components.first[componentIndex];

	for (unsigned inputIndex = 0; inputIndex < component.inputs.count; inputIndex++) {
		auto &componentInput = component.inputs.first[inputIndex];

		auto valueIndex = componentInput.values.first[0].valueIndex;

		auto &value = flowDefinition.flowValues.first[valueIndex];
		if (value.header.type == FLOW_VALUE_TYPE_UNDEFINED) {
			return false;
		}
	}

	return true;
}

unsigned start(Assets *assets) {
	FlowDefinition &flowDefinition = *assets->flowDefinition;
	if (flowDefinition.flows.count == 0) {
		return 0;
	}

	g_assets = assets;

	unsigned flowIndex = 0;

	const Flow &flow = flowDefinition.flows.first[flowIndex];
	for (unsigned componentIndex = 0; componentIndex < flow.components.count; componentIndex++) {
		if (allInputsDefined(flowDefinition, flowIndex, componentIndex)) {
			addToQueue(flowIndex, componentIndex);
		}
	}

	return 1;
}

void executeComponent(FlowDefinition &flowDefinition, unsigned flowIndex, unsigned componentIndex) {
	printf("Execute component %d\n", componentIndex);
}

void tick(unsigned flowHandle) {
	if (flowHandle != 1) {
		return;
	}

	Assets *assets = g_assets;
    FlowDefinition &flowDefinition = *assets->flowDefinition;

	unsigned flowIndex;
	unsigned componentIndex;
	if (removeFromQueue(flowIndex, componentIndex)) {
		executeComponent(flowDefinition, flowIndex, componentIndex);
	}
}


void dumpFlow(FlowDefinition &flowDefinition) {
	// printf("Flows:\n");
	// for (unsigned i = 0; i < flowDefinition.flows.count; i++) {
	// 	printf("\t%d:\n", i);
	// 	const Flow &flow = flowDefinition.flows.first[i];
	// 	for (unsigned j = 0; j < flow.components.count; j++) {
	// 		const Component &component = flow.components.first[j];
	// 		printf("\t\t%d\n", j);
	//         printf("\t\t\tType: %d\n", (int)component.type);

	//         printf("\t\t\tInputs:\n");
	//         for (unsigned k = 0; k < component.inputs.count; k++) {
	//             const ComponentInput &componentInput = component.inputs.first[k];
	//             printf("\t\t\t\t%d\n", componentInput.values.count);
	//         }

	//         printf("\t\t\tOutputs:\n");
	//         for (unsigned k = 0; k < component.outputs.count; k++) {
	//             const ComponentOutput &componentOutput = component.outputs.first[k];
	//             printf("\t\t\t\t%d\n", componentOutput.connections.count);
	//         }
	// 	}
	// }

	// printf("Values:\n");
	// for (unsigned i = 0; i < flowDefinition.flowValues.count; i++) {
	// 	const FlowValue &flowValue = flowDefinition.flowValues.first[i];
	//     printf("\t%d: %d\n", i, (int)flowValue.header.type);
	// }
}

} // namespace flow
} // namespace eez
