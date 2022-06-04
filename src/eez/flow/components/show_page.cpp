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

#include <eez/flow/flow.h>
#include <eez/flow/components.h>
#include <eez/flow/debugger.h>
#include <eez/flow/hooks.h>

namespace eez {
namespace flow {

struct ShowPageActionComponent : public Component {
	int16_t page;
};

void executeShowPageComponent(FlowState *flowState, unsigned componentIndex) {
	auto component = (ShowPageActionComponent *)flowState->flow->components[componentIndex];

	replacePageHook(component->page);

	propagateValueThroughSeqout(flowState, componentIndex);
}

} // namespace flow
} // namespace eez
