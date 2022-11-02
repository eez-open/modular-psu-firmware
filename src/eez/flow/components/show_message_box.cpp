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
#include <eez/flow/flow_defs_v3.h>
#include <eez/flow/expression.h>

#if OPTION_GUI || !defined(OPTION_GUI)
#include <eez/gui/gui.h>
#endif

namespace eez {
namespace flow {

const uint8_t MESSAGE_BOX_TYPE_INFO = 1;
const uint8_t MESSAGE_BOX_TYPE_ERROR = 2;

struct ShowMessagePageActionComponent : public Component {
	uint8_t type;
};

void executeShowMessageBoxComponent(FlowState *flowState, unsigned componentIndex) {
#if OPTION_GUI || !defined(OPTION_GUI)
	auto component = (ShowMessagePageActionComponent *)flowState->flow->components[componentIndex];

    Value messageValue;
    if (!evalProperty(flowState, componentIndex, defs_v3::SHOW_MESSAGE_BOX_ACTION_COMPONENT_PROPERTY_MESSAGE, messageValue, "Failed to evaluate Message in ShowMessageBox")) {
        return;
    }

	if (component->type == MESSAGE_BOX_TYPE_INFO) {
		getAppContextFromId(APP_CONTEXT_ID_DEVICE)->infoMessage(messageValue);
	} else if (component->type == MESSAGE_BOX_TYPE_ERROR) {
		getAppContextFromId(APP_CONTEXT_ID_DEVICE)->errorMessage(messageValue);
	}
#endif
}

} // namespace flow
} // namespace eez
