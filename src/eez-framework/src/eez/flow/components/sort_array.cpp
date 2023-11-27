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

#include <string.h>
#include <stdlib.h>

#include <eez/core/util.h>
#include <eez/core/debug.h>
#include <eez/core/utf8.h>

#include <eez/flow/components.h>
#include <eez/flow/flow_defs_v3.h>
#include <eez/flow/expression.h>
#include <eez/flow/debugger.h>

#include <eez/flow/components/sort_array.h>

namespace eez {
namespace flow {

SortArrayActionComponent *g_sortArrayActionComponent;

int elementCompare(const void *a, const void *b) {
    auto aValue = *(const Value *)a;
    auto bValue = *(const Value *)b;

    if (g_sortArrayActionComponent->arrayType != -1) {
        if (!aValue.isArray()) {
            return 0;
        }
        auto aArray = aValue.getArray();
        if ((uint32_t)g_sortArrayActionComponent->structFieldIndex >= aArray->arraySize) {
            return 0;
        }
        aValue = aArray->values[g_sortArrayActionComponent->structFieldIndex];

        if (!bValue.isArray()) {
            return 0;
        }
        auto bArray = bValue.getArray();
        if ((uint32_t)g_sortArrayActionComponent->structFieldIndex >= bArray->arraySize) {
            return 0;
        }
        bValue = bArray->values[g_sortArrayActionComponent->structFieldIndex];
    }

    int result;

    if (aValue.isString() && bValue.isString()) {
        if (g_sortArrayActionComponent->flags & SORT_ARRAY_FLAG_IGNORE_CASE) {
            result = utf8casecmp(aValue.getString(), bValue.getString());
        } else {
            result = utf8cmp(aValue.getString(), bValue.getString());
        }
    } else {
        int err;
        float aDouble = aValue.toDouble(&err);
        if (err) {
            return 0;
        }
        float bDouble = bValue.toDouble(&err);
        if (err) {
            return 0;
        }

        auto diff = aDouble - bDouble;
        result = diff < 0 ? -1 : diff > 0 ? 1 : 0;
    }

    if (!(g_sortArrayActionComponent->flags & SORT_ARRAY_FLAG_ASCENDING)) {
        result = -result;
    }

    return result;
}

void sortArray(SortArrayActionComponent *component, ArrayValue *array) {
    g_sortArrayActionComponent = component;
    qsort(&array->values[0], array->arraySize, sizeof(Value), elementCompare);

}

void executeSortArrayComponent(FlowState *flowState, unsigned componentIndex) {
    auto component = (SortArrayActionComponent *)flowState->flow->components[componentIndex];

    Value srcArrayValue;
    if (!evalProperty(flowState, componentIndex, defs_v3::SORT_ARRAY_ACTION_COMPONENT_PROPERTY_ARRAY, srcArrayValue, "Failed to evaluate Array in SortArray\n")) {
        return;
    }

    if (!srcArrayValue.isArray()) {
        throwError(flowState, componentIndex, "SortArray: not an array\n");
        return;
    }

    auto arrayValue = srcArrayValue.clone();
    auto array = arrayValue.getArray();

    if (component->arrayType != -1) {
        if (array->arrayType != (uint32_t)component->arrayType) {
            throwError(flowState, componentIndex, "SortArray: invalid array type\n");
            return;
        }

        if (component->structFieldIndex < 0) {
            throwError(flowState, componentIndex, "SortArray: invalid struct field index\n");
        }
    } else {
        if (array->arrayType != defs_v3::ARRAY_TYPE_INTEGER && array->arrayType != defs_v3::ARRAY_TYPE_FLOAT && array->arrayType != defs_v3::ARRAY_TYPE_DOUBLE && array->arrayType != defs_v3::ARRAY_TYPE_STRING) {
            throwError(flowState, componentIndex, "SortArray: array type is neither array:integer or array:float or array:double or array:string\n");
            return;
        }
    }

    sortArray(component, array);

	propagateValue(flowState, componentIndex, component->outputs.count - 1, arrayValue);
}

} // namespace flow
} // namespace eez
