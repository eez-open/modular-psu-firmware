/*
 * EEZ Modular Firmware
 * Copyright (C) 2015-present, Envox d.o.o.
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

#pragma once

namespace eez {
namespace gui {

struct LayoutViewWidget : public Widget {
    int16_t layout; // page ID
    int16_t context; // data ID
	uint16_t componentIndex;
};

struct LayoutViewWidgetState : public WidgetState {
    Value context;

    LayoutViewWidgetState(const WidgetCursor &widgetCursor) : WidgetState(widgetCursor) {
    }

	void draw(WidgetState *previousState) override;
};

} // namespace gui
} // namespace eez