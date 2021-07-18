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
#include <math.h>

#include <eez/flow.h>
#include <eez/system.h>
#include <eez/flow_defs_v3.h>
#include <eez/scripting.h>

using namespace eez::gui;

namespace eez {
namespace flow {

static Assets *g_assets;

static const int UNDEFINED_VALUE_INDEX = 0;
static const int NULL_VALUE_INDEX = 1;

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

bool allInputsDefined(Assets *assets, unsigned flowIndex, unsigned componentIndex) {
	auto flowDefinition = assets->flowDefinition.ptr(assets);
	auto flow = flowDefinition->flows.item(assets, flowIndex);
	auto component = flow->components.item(assets, componentIndex);

	for (unsigned inputIndex = 0; inputIndex < component->inputs.count; inputIndex++) {
		auto componentInput = component->inputs.item(assets, inputIndex);

		auto valueIndex = componentInput->valueIndex;

		auto &value = *flowDefinition->flowValues.item(assets, valueIndex);
		if (value.type_ == VALUE_TYPE_UNDEFINED) {
			return false;
		}
	}

	return true;
}

void pingComponent(Assets *assets, unsigned flowIndex, unsigned componentIndex) {
    if (allInputsDefined(assets, flowIndex, componentIndex)) {
        addToQueue(flowIndex, componentIndex);
    }
}

unsigned start(Assets *assets) {
	auto flowDefinition = assets->flowDefinition.ptr(assets);
	if (flowDefinition->flows.count == 0) {
		return 0;
	}

	g_assets = assets;

	g_queueHead = 0;
	g_queueTail = 0;
	g_queueIsFull = false;

	unsigned flowIndex = 0;

	auto flow = flowDefinition->flows.item(assets, flowIndex);
	for (unsigned componentIndex = 0; componentIndex < flow->components.count; componentIndex++) {
        pingComponent(assets, flowIndex, componentIndex);
	}

	return 1;
}

void propagateValue(Assets *assets, int flowIndex, ComponentOutput &componentOutput, const gui::Value &value) {
	auto flowDefinition = assets->flowDefinition.ptr(assets);
	auto flow = flowDefinition->flows.item(assets, flowIndex);

	for (unsigned connectionIndex = 0; connectionIndex < componentOutput.connections.count; connectionIndex++) {
		auto connection = componentOutput.connections.item(assets, connectionIndex);

		auto targetComponent = flow->components.item(assets, connection->targetComponentIndex);

		auto targetComponentInput = targetComponent->inputs.item(assets, connection->targetInputIndex);

		auto valueIndex = targetComponentInput->valueIndex;

		auto &targetValue = *flowDefinition->flowValues.item(assets, valueIndex);
		targetValue = value;

		pingComponent(assets, flowIndex, connection->targetComponentIndex);
	}
}

void executeComponent(Assets *assets, unsigned flowIndex, unsigned componentIndex) {
	auto flowDefinition = assets->flowDefinition.ptr(assets);
	auto flow = flowDefinition->flows.item(assets, flowIndex);
	auto component = flow->components.item(assets, componentIndex);
    if (component->type == defs_v3::COMPONENT_TYPE_START_ACTION) {
	    //printf("Execute START component at index = %d\n", componentIndex);

		auto &nullValue = *flowDefinition->flowValues.item(assets, NULL_VALUE_INDEX);

		propagateValue(assets, flowIndex, *component->outputs.item(assets, 0), nullValue);
    } else if (component->type == defs_v3::COMPONENT_TYPE_DELAY_ACTION) {
	    //printf("Execute DELAY component at index = %d\n", componentIndex);

		auto componentInput = component->inputs.item(assets, defs_v3::DELAY_ACTION_COMPONENT_INPUT_MILLISECONDS);
		auto &value = *flowDefinition->flowValues.item(assets, componentInput->valueIndex);

		osDelay(roundf(value.getFloat()));

		auto &nullValue = *flowDefinition->flowValues.item(assets, NULL_VALUE_INDEX);
		propagateValue(assets, flowIndex, *component->outputs.item(assets, 0), nullValue);
    } else if (component->type == defs_v3::COMPONENT_TYPE_END_ACTION) {
	    //printf("Execute END component at index = %d\n", componentIndex);

		scripting::stopScript();
    } else if (component->type == defs_v3::COMPONENT_TYPE_CONSTANT_ACTION) {
	    //printf("Execute CONSTANT component at index = %d\n", componentIndex);

        struct ConstantActionComponent : public Component {
            uint16_t valueIndex;
        };
        auto constantActionComponent = (ConstantActionComponent *)component;
		auto &sourceValue = *flowDefinition->flowValues.item(assets, constantActionComponent->valueIndex);

		// TODO ovdje treba poslati null value za 0-ti output
		for (unsigned outputIndex = 0; outputIndex < component->outputs.count; outputIndex++) {
			auto &componentOutput = *component->outputs.item(assets, outputIndex);
			propagateValue(assets, flowIndex, componentOutput, sourceValue);
		}
	} else if (component->type == defs_v3::COMPONENT_TYPE_SCPI_ACTION) {
	    //printf("Execute SCPI component at index = %d\n", componentIndex);

		struct ScpiActionComponent : public Component {
			uint8_t instructions[1];
		};
		auto specific = (ScpiActionComponent *)component;
		auto instructions = ((ScpiActionComponent *)component)->instructions;

		static const int SCPI_PART_STRING = 1;
		static const int SCPI_PART_INPUT = 2;
		static const int SCPI_PART_QUERY_WITH_ASSIGNMENT = 3;
		static const int SCPI_PART_QUERY = 4;
		static const int SCPI_PART_COMMAND = 5;
		static const int SCPI_PART_END = 6;

		char commandOrQueryText[256] = {0};
		const char *resultText;
		size_t resultTextLen;

		int i = 0;
		while (true) {
			uint8_t op = instructions[i++];

			if (op == SCPI_PART_STRING) {
				//printf("SCPI_PART_STRING\n");
				uint16_t sizeLowByte = instructions[i++];
				uint16_t sizeHighByte = instructions[i++];
				uint16_t stringLength = sizeLowByte | (sizeHighByte << 8);
				stringAppendStringLength(commandOrQueryText, sizeof(commandOrQueryText), (const char *)specific + i, (size_t)stringLength);
				i += stringLength;
			} else if (op == SCPI_PART_INPUT) {
				//printf("SCPI_PART_INPUT\n");
				//uint8_t inputIndex = specific[i++];
				// TODO
			} else if (op == SCPI_PART_QUERY_WITH_ASSIGNMENT) {
				//printf("SCPI_PART_QUERY_WITH_ASSIGNMENT\n");
				stringAppendString(commandOrQueryText, sizeof(commandOrQueryText), "\n");
				scripting::scpi(commandOrQueryText, &resultText, &resultTextLen);
				
				// TODO!!!
				static char buffers[2][256];
				static int k = 0;
				char *buffer = &buffers[k][0];
				k = (k + 1) % 2;
				stringCopyLength(buffer, 256, resultText, resultTextLen);
				gui::Value value;
				value.type_ = VALUE_TYPE_STRING;
				value.str_ = buffer;

				uint8_t outputIndex = instructions[i++];
				propagateValue(assets, flowIndex, *component->outputs.item(assets, outputIndex), value);
				commandOrQueryText[0] = 0;
			} else if (op == SCPI_PART_QUERY) {
				//printf("SCPI_PART_QUERY\n");
				stringAppendString(commandOrQueryText, sizeof(commandOrQueryText), "\n");
				scripting::scpi(commandOrQueryText, &resultText, &resultTextLen);
				commandOrQueryText[0] = 0;
			} else if (op == SCPI_PART_COMMAND) {
				//printf("SCPI_PART_COMMAND\n");
				stringAppendString(commandOrQueryText, sizeof(commandOrQueryText), "\n");
				scripting::scpi(commandOrQueryText, &resultText, &resultTextLen);
				commandOrQueryText[0] = 0;
			} else if (op == SCPI_PART_END) {
				//printf("SCPI_PART_END\n");
				break;
			}
		}

		auto &nullValue = *flowDefinition->flowValues.item(assets, NULL_VALUE_INDEX);
		propagateValue(assets, flowIndex, *component->outputs.item(assets, 0), nullValue);
    } else {
	    //printf("Unknow component at index = %d, type = %d\n", componentIndex, component.type);
    }
}

void tick(unsigned flowHandle) {
	if (flowHandle != 1) {
		return;
	}

	Assets *assets = g_assets;

	unsigned flowIndex;
	unsigned componentIndex;
	if (removeFromQueue(flowIndex, componentIndex)) {
		executeComponent(assets, flowIndex, componentIndex);
	}
}

void executeFlowAction(unsigned flowHandle, int16_t actionId) {
	actionId = -actionId - 1;

	Assets *assets = g_assets;
	auto flowDefinition = assets->flowDefinition.ptr(assets);

	if (actionId >= 0 && actionId < (int16_t)flowDefinition->widgetActions.count) {
		auto &componentOutput = *flowDefinition->widgetActions.item(assets, actionId);
		auto &nullValue = *flowDefinition->flowValues.item(assets, NULL_VALUE_INDEX);
		propagateValue(assets, 0, componentOutput, nullValue);
	}
}

void dataOperation(unsigned flowHandle, int16_t dataId, DataOperationEnum operation, Cursor cursor, Value &value) {
	dataId = -dataId - 1;

	Assets *assets = g_assets;
	auto flowDefinition = assets->flowDefinition.ptr(assets);

	if (dataId >= 0 && dataId < (int16_t)flowDefinition->widgetDataItems.count) {
		auto componentInput = flowDefinition->widgetDataItems.item(assets, dataId);
		auto valueIndex = componentInput->valueIndex;
		auto &flowValue = *flowDefinition->flowValues.item(assets, valueIndex);
		value = flowValue;
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
