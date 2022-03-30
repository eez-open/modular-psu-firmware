
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

#if defined(__EMSCRIPTEN__)

#include <stdlib.h>
#include <stdio.h>

#include <eez/flow/flow.h>
#include <eez/flow/private.h>
#include <eez/flow/expression.h>

namespace eez {
namespace flow {

int DashboardComponentContext::getFlowIndex() {
    return flowState->flowIndex;
}

int DashboardComponentContext::getComponentIndex() {
    return componentIndex;
}

DashboardComponentContext *DashboardComponentContext::startAsyncExecution() {
    DashboardComponentContext *dashboardComponentContext = ObjectAllocator<DashboardComponentContext>::allocate(0xe0b815d6);
    dashboardComponentContext->flowState = flowState;
    dashboardComponentContext->componentIndex = componentIndex;
    eez::flow::startAsyncExecution(flowState, componentIndex);
    return dashboardComponentContext;
}

void DashboardComponentContext::endAsyncExecution() {
    eez::flow::endAsyncExecution(flowState, componentIndex);
    ObjectAllocator<DashboardComponentContext>::deallocate(this);
}

Value *DashboardComponentContext::evalProperty(int propertyIndex) {
    auto pValue = ObjectAllocator<Value>::allocate(0x0de3a60d);
    if (!pValue) {
        eez::flow::throwError(flowState, componentIndex, "Out of memory\n");
        return nullptr;
    }

    if (!eez::flow::evalProperty(flowState, componentIndex, propertyIndex, *pValue)) {
        ObjectAllocator<Value>::deallocate(pValue);
        eez::flow::throwError(flowState, componentIndex, "Failed to evaluate Milliseconds in Delay\n");
        return nullptr;
    }

    return pValue;
}

const char *DashboardComponentContext::getStringParam(int offset) {
    auto component = flowState->flow->components[componentIndex];
    auto ptr = (const uint32_t *)((const uint8_t *)component + sizeof(Component) + offset);
    return (const char *)(MEMORY_BEGIN + 4 + *ptr);
}

struct ExpressionList {
    uint32_t count;
    Value values[1];
};
void *DashboardComponentContext::getExpressionListParam(int offset) {
    auto component = flowState->flow->components[componentIndex];

    struct List {
        uint32_t count;
        uint32_t items;
    };
    auto list = (const List *)((const uint8_t *)component + sizeof(Component) + offset);

    auto expressionList = (ExpressionList *)::malloc((list->count + 1) * sizeof(Value));

    expressionList->count = list->count;

    auto items = (const uint32_t *)(MEMORY_BEGIN + 4 + list->items);

    for (uint32_t i = 0; i < list->count; i++) {
        // call Value constructor
        new (expressionList->values + i) Value();

        auto valueExpression = (const uint8_t *)(MEMORY_BEGIN + 4 + items[i]);
        if (!evalExpression(flowState, componentIndex, valueExpression, expressionList->values[i])) {
            eez::flow::throwError(flowState, componentIndex, "Failed to evaluate expression");
            return nullptr;
        }
    }

    return expressionList;
}

void DashboardComponentContext::freeExpressionListParam(void *ptr) {
    auto expressionList = (ExpressionList*)ptr;

    for (uint32_t i = 0; i < expressionList->count; i++) {
        // call Value desctructor
        (expressionList->values + i)->~Value();
    }

    ::free(ptr);
}

void DashboardComponentContext::propagateValue(unsigned outputIndex, Value *valuePtr) {
	eez::flow::propagateValue(flowState, componentIndex, outputIndex, *valuePtr);
}

void DashboardComponentContext::propagateIntValue(unsigned outputIndex, int value) {
    Value intValue = value;
	eez::flow::propagateValue(flowState, componentIndex, outputIndex, intValue);
}

void DashboardComponentContext::propagateDoubleValue(unsigned outputIndex, double value) {
    Value doubleValue(value, VALUE_TYPE_DOUBLE);
	eez::flow::propagateValue(flowState, componentIndex, outputIndex, doubleValue);
}

void DashboardComponentContext::propagateBooleanValue(unsigned outputIndex, bool value) {
    Value booleanValue(value, VALUE_TYPE_BOOLEAN);
	eez::flow::propagateValue(flowState, componentIndex, outputIndex, booleanValue);
}

void DashboardComponentContext::propagateStringValue(unsigned outputIndex, const char *value) {
    Value stringValue = Value::makeStringRef(value, strlen(value), 0x4d05952a);
	eez::flow::propagateValue(flowState, componentIndex, outputIndex, stringValue);
}

void DashboardComponentContext::propagateUndefinedValue(unsigned outputIndex) {
    Value value(0, VALUE_TYPE_UNDEFINED);
	eez::flow::propagateValue(flowState, componentIndex, outputIndex, value);
}

void DashboardComponentContext::propagateNullValue(unsigned outputIndex) {
    Value value(0, VALUE_TYPE_NULL);
	eez::flow::propagateValue(flowState, componentIndex, outputIndex, value);
}

void DashboardComponentContext::propagateValueThroughSeqout() {
	eez::flow::propagateValueThroughSeqout(flowState, componentIndex);
}

void DashboardComponentContext::executeCallAction(int flowIndex) {
    eez::flow::executeCallAction(flowState, componentIndex, flowIndex);
}

void DashboardComponentContext::throwError(const char *errorMessage) {
	eez::flow::throwError(flowState, componentIndex, errorMessage);
}

} // flow
} // eez

using namespace eez::flow;

EM_PORT_API(int) DashboardContext_getFlowIndex(DashboardComponentContext *context) {
    return context->getFlowIndex();
}

EM_PORT_API(int) DashboardContext_getComponentIndex(DashboardComponentContext *context) {
    return context->getComponentIndex();
}

EM_PORT_API(DashboardComponentContext *) DashboardContext_startAsyncExecution(DashboardComponentContext *context) {
    return context->startAsyncExecution();
}

EM_PORT_API(void) DashboardContext_endAsyncExecution(DashboardComponentContext *context) {
    context->endAsyncExecution();
}

EM_PORT_API(Value *) DashboardContext_evalProperty(DashboardComponentContext *context, int propertyIndex) {
    return context->evalProperty(propertyIndex);
}

EM_PORT_API(const char *) DashboardContext_getStringParam(DashboardComponentContext *context, int offset) {
    return context->getStringParam(offset);
}

EM_PORT_API(void *) DashboardContext_getExpressionListParam(DashboardComponentContext *context, int offset) {
    return context->getExpressionListParam(offset);
}

EM_PORT_API(void) DashboardContext_freeExpressionListParam(DashboardComponentContext *context, void *ptr) {
    return context->freeExpressionListParam(ptr);
}

EM_PORT_API(void) DashboardContext_propagateValue(DashboardComponentContext *context, unsigned outputIndex, Value *value) {
    context->propagateValue(outputIndex, value);
}

EM_PORT_API(void) DashboardContext_propagateIntValue(DashboardComponentContext *context, unsigned outputIndex, int value) {
    context->propagateIntValue(outputIndex, value);
}

EM_PORT_API(void) DashboardContext_propagateDoubleValue(DashboardComponentContext *context, unsigned outputIndex, double value) {
    context->propagateDoubleValue(outputIndex, value);
}

EM_PORT_API(void) DashboardContext_propagateBooleanValue(DashboardComponentContext *context, unsigned outputIndex, bool value) {
    context->propagateBooleanValue(outputIndex, value);
}

EM_PORT_API(void) DashboardContext_propagateStringValue(DashboardComponentContext *context, unsigned outputIndex, const char *value) {
    context->propagateStringValue(outputIndex, value);
}

EM_PORT_API(void) DashboardContext_propagateUndefinedValue(DashboardComponentContext *context, unsigned outputIndex) {
    context->propagateUndefinedValue(outputIndex);
}

EM_PORT_API(void) DashboardContext_propagateNullValue(DashboardComponentContext *context, unsigned outputIndex) {
    context->propagateNullValue(outputIndex);
}

EM_PORT_API(void) DashboardContext_propagateValueThroughSeqout(DashboardComponentContext *context) {
    context->propagateValueThroughSeqout();
}

EM_PORT_API(void) DashboardContext_executeCallAction(DashboardComponentContext *context, int flowIndex) {
    context->executeCallAction(flowIndex);
}

EM_PORT_API(void) DashboardContext_throwError(DashboardComponentContext *context, const char *errorMessage) {
    context->throwError(errorMessage);
}

EM_PORT_API(Value *) arrayValueAlloc(int arraySize, int arrayType) {
    using namespace eez;
    using namespace eez::gui;
    Value value = Value::makeArrayRef(arraySize, arrayType, 0xeabb7edc);
    auto pValue = ObjectAllocator<Value>::allocate(0xbab14c6a);
    if (pValue) {
        *pValue = value;
    }
    return pValue;
}

EM_PORT_API(void) arrayValueSetElementValue(Value *arrayValuePtr, int elementIndex, Value *valuePtr) {
    auto array = arrayValuePtr->getArray();
    array->values[elementIndex] = *valuePtr;
}

EM_PORT_API(void) arrayValueSetElementInt(Value *arrayValuePtr, int elementIndex, int value) {
    auto array = arrayValuePtr->getArray();
    array->values[elementIndex] = value;
}

EM_PORT_API(void) arrayValueSetElementDouble(Value *arrayValuePtr, int elementIndex, double value) {
    using namespace eez;
    using namespace eez::gui;
    Value doubleValue(value, VALUE_TYPE_DOUBLE);
    auto array = arrayValuePtr->getArray();
    array->values[elementIndex] = doubleValue;
}

EM_PORT_API(void) arrayValueSetElementBool(Value *arrayValuePtr, int elementIndex, bool value) {
    using namespace eez;
    using namespace eez::gui;
    Value booleanValue(value, VALUE_TYPE_BOOLEAN);
    auto array = arrayValuePtr->getArray();
    array->values[elementIndex] = booleanValue;
}

EM_PORT_API(void) arrayValueSetElementString(Value *arrayValuePtr, int elementIndex, const char *value) {
    using namespace eez;
    using namespace eez::gui;
    Value stringValue = Value::makeStringRef(value, strlen(value), 0x78dea30d);
    auto array = arrayValuePtr->getArray();
    array->values[elementIndex] = stringValue;
}

EM_PORT_API(void) arrayValueSetElementNull(Value *arrayValuePtr, int elementIndex) {
    using namespace eez;
    using namespace eez::gui;
    Value nullValue = Value(0, VALUE_TYPE_NULL);
    auto array = arrayValuePtr->getArray();
    array->values[elementIndex] = nullValue;
}

EM_PORT_API(Value *) evalProperty(int flowStateIndex, int componentIndex, int propertyIndex, int32_t *iterators) {
    using namespace eez;
    using namespace eez::gui;
    using namespace eez::flow;

    auto pValue = ObjectAllocator<Value>::allocate(0xb7e697b8);
    if (!pValue) {
        return nullptr;
    }

    auto flowState = getFlowState(g_mainAssets, flowStateIndex);

    if (!eez::flow::evalProperty(flowState, componentIndex, propertyIndex, *pValue, nullptr, iterators)) {
        ObjectAllocator<Value>::deallocate(pValue);
        return nullptr;
    }

    return pValue;
}

EM_PORT_API(void) propagateValue(int flowStateIndex, int componentIndex, int outputIndex, Value *valuePtr) {
    using namespace eez;
    using namespace eez::gui;
    using namespace eez::flow;

    auto flowState = getFlowState(g_mainAssets, flowStateIndex);

    eez::flow::propagateValue(flowState, componentIndex, outputIndex, *valuePtr);
}

EM_PORT_API(void) valueFree(Value *valuePtr) {
    using namespace eez;
    ObjectAllocator<Value>::deallocate(valuePtr);
}

EM_PORT_API(void) setGlobalVariable(int globalVariableIndex, Value *valuePtr) {
    using namespace eez::gui;
    auto flowDefinition = static_cast<FlowDefinition *>(g_mainAssets->flowDefinition);
    Value *globalVariableValuePtr = flowDefinition->globalVariables[globalVariableIndex];
    *globalVariableValuePtr = *valuePtr;
}

#endif // __EMSCRIPTEN__
