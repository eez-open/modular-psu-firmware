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

#include <eez/flow/components.h>
#include <eez/flow/flow_defs_v3.h>
#include <eez/flow/expression.h>
#include <eez/flow/private.h>

namespace eez {
namespace flow {

void executeSelectLanguageComponent(FlowState *flowState, unsigned componentIndex) {
	Value languageValue;
	if (!evalProperty(flowState, componentIndex, defs_v3::SELECT_LANGUAGE_ACTION_COMPONENT_PROPERTY_LANGUAGE, languageValue, "Failed to evaluate Language in SelectLanguage")) {
		return;
	}

	const char *language = languageValue.getString();

    auto &languages = flowState->assets->languages;

    for (uint32_t languageIndex = 0; languageIndex < languages.count; languageIndex++) {
        if (strcmp(languages[languageIndex]->languageID, language) == 0) {
            g_selectedLanguage = languageIndex;
	        propagateValueThroughSeqout(flowState, componentIndex);
            return;
        }
    }

    char message[256];
    snprintf(message, sizeof(message), "Unknown language %s", language);
    throwError(flowState, componentIndex, message);
}

} // namespace flow
} // namespace eez
