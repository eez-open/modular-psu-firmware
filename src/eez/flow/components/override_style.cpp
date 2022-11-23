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

#if EEZ_OPTION_GUI || !defined(EEZ_OPTION_GUI)

#include <eez/flow/components.h>
#include <eez/flow/flow_defs_v3.h>
#include <eez/flow/expression.h>
#include <eez/flow/private.h>

#include <eez/gui/gui.h>
using namespace eez::gui;

namespace eez {
namespace flow {

struct OverrideStyleComponent : public Component {
	int16_t fromStyle;
    int16_t toStyle;
};

void executeOverrideStyleComponent(FlowState *flowState, unsigned componentIndex) {
	auto component = (OverrideStyleComponent *)flowState->flow->components[componentIndex];
    setOverrideStyleRule(component->fromStyle, component->toStyle);
    propagateValueThroughSeqout(flowState, componentIndex);
}

} // namespace flow
} // namespace eez

#endif // EEZ_OPTION_GUI || !defined(EEZ_OPTION_GUI)
