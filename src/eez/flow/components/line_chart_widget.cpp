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

#if OPTION_GUI || !defined(OPTION_GUI)

#include <eez/core/alloc.h>

#include <eez/flow/components.h>
#include <eez/flow/expression.h>
#include <eez/flow/private.h>
#include <eez/flow/components/line_chart_widget.h>

namespace eez {
namespace flow {

LineChartWidgetComponenentExecutionState::LineChartWidgetComponenentExecutionState()
    : data(nullptr)
{
}

LineChartWidgetComponenentExecutionState::~LineChartWidgetComponenentExecutionState() {
    if (data != nullptr) {
        auto xValues = (Value *)data;
        for (uint32_t i = 0; i < maxPoints; i++) {
            (xValues + i)->~Value();
        }
        eez::free(data);
    }

    for (uint32_t i = 0; i < maxPoints; i++) {
		(lineLabels + i)->~Value();
	}
    eez::free(lineLabels);
}

void LineChartWidgetComponenentExecutionState::init(uint32_t numLines_, uint32_t maxPoints_) {
    numLines = numLines_;
    maxPoints = maxPoints_;

    data = eez::alloc(maxPoints * sizeof(Value) + maxPoints * numLines * sizeof(float), 0xe4945fea);

    auto xValues = (Value *)data;
    for (uint32_t i = 0; i < maxPoints; i++) {
		new (xValues + i) Value();
	}

    numPoints = 0;
    startPointIndex = 0;

    lineLabels = (Value *)eez::alloc(numLines * sizeof(Value *), 0xe8afd215);
    for (uint32_t i = 0; i < numLines; i++) {
		new (lineLabels + i) Value();
	}

    updated = true;
}

Value LineChartWidgetComponenentExecutionState::getX(int pointIndex) {
    auto xValues = (Value *)data;
    return xValues[pointIndex];
}

void LineChartWidgetComponenentExecutionState::setX(int pointIndex, Value& value) {
    auto xValues = (Value *)data;
    xValues[pointIndex] = value;
}

float LineChartWidgetComponenentExecutionState::getY(int pointIndex, int lineIndex) {
    auto yValues = (float *)((Value *)data + maxPoints);
    return *(yValues + pointIndex * numLines + lineIndex);
}

void LineChartWidgetComponenentExecutionState::setY(int pointIndex, int lineIndex, float value) {
    auto yValues = (float *)((Value *)data + maxPoints);
    *(yValues + pointIndex * numLines + lineIndex) = value;
}

bool LineChartWidgetComponenentExecutionState::onInputValue(FlowState *flowState, unsigned componentIndex) {
    auto component = (LineChartWidgetComponenent *)flowState->flow->components[componentIndex];

    uint32_t pointIndex;

    if (numPoints < component->maxPoints) {
        pointIndex = numPoints++;
    } else {
        startPointIndex = (startPointIndex + 1) % component->maxPoints;
        pointIndex = (startPointIndex + component->maxPoints - 1) % component->maxPoints;
    }

    Value value;
    if (!evalExpression(flowState, componentIndex, component->xValue, value, "Failed to evaluate x value in LineChartWidget")) {
        return false;
    }

    int err;
    value.toDouble(&err);
    if (err) {
        throwError(flowState, componentIndex, "X value not an number or date");
        return false;
    }

    setX(pointIndex, value);

    for (uint32_t lineIndex = 0; lineIndex < numLines; lineIndex++) {
        char errorMessage[256];
        snprintf(errorMessage, sizeof(errorMessage), "Failed to evaluate line value no. %d in LineChartWidget", (int)(lineIndex + 1));
        Value value;
        if (!evalExpression(flowState, componentIndex, component->lines[lineIndex]->value, value, errorMessage)) {
            return false;
        }

        int err;
        auto y = value.toFloat(&err);
        if (err) {
            snprintf(errorMessage, sizeof(errorMessage), "Can't convert line value no. %d to float", (int)(lineIndex + 1));
            throwError(flowState, componentIndex, errorMessage);
            return false;
        }

        setY(pointIndex, lineIndex, y);
    }

    return true;
}

void executeLineChartWidgetComponent(FlowState *flowState, unsigned componentIndex) {
    auto component = (LineChartWidgetComponenent *)flowState->flow->components[componentIndex];

    auto executionState = (LineChartWidgetComponenentExecutionState *)flowState->componenentExecutionStates[componentIndex];
    if (!executionState) {
        executionState = allocateComponentExecutionState<LineChartWidgetComponenentExecutionState>(flowState, componentIndex);
        executionState->init(component->lines.count, component->maxPoints);

        for (uint32_t lineIndex = 0; lineIndex < component->lines.count; lineIndex++) {
            char errorMessage[256];
            snprintf(errorMessage, sizeof(errorMessage), "Failed to evaluate line label no. %d in LineChartWidget", (int)(lineIndex + 1));

            if (!evalExpression(flowState, componentIndex, component->lines[lineIndex]->label, executionState->lineLabels[lineIndex], errorMessage)) {
                return;
            }
        }
    }

    if (flowState->values[component->inputs[1]].type != VALUE_TYPE_UNDEFINED) {
        // reset
        executionState->numPoints = 0;
        executionState->startPointIndex = 0;
        executionState->updated = true;

        clearInputValue(flowState, component->inputs[1]);
    }

    auto inputIndex = component->inputs[0];
    auto inputValue = flowState->values[inputIndex];
    if (inputValue.type != VALUE_TYPE_UNDEFINED) {
        // data
        if (inputValue.isArray() && inputValue.getArray()->arrayType == defs_v3::ARRAY_TYPE_ANY) {
            auto array = inputValue.getArray();
            bool updated = false;
            for (uint32_t elementIndex = 0; elementIndex < array->arraySize; elementIndex++) {
                flowState->values[inputIndex] = array->values[elementIndex];
                if (executionState->onInputValue(flowState, componentIndex)) {
                    updated = true;
                } else {
                    break;
                }
            }
            if (updated) {
                executionState->updated = true;
            }
        } else {
            if (executionState->onInputValue(flowState, componentIndex)) {
                executionState->updated = true;
            }
        }

        clearInputValue(flowState, inputIndex);
    }
}

} // namespace flow
} // namespace eez

#endif // OPTION_GUI || !defined(OPTION_GUI)
