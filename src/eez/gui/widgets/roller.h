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

struct RollerWidget : public Widget {
};

struct RollerWidgetState : public WidgetState {
    Value data;

    int offset = 0;

	bool dirty = false;

	int yAtDown;
	int offsetAtDown;
	uint32_t timeLast;
	int yLast;
	float speed;

	bool target;
	int targetStartOffset;
	int targetEndOffset;
	uint32_t targetStartTime;
	uint32_t targetEndTime;

    bool updateState() override;
    void render() override;

    bool hasOnTouch() override;
	void onTouch(const WidgetCursor &widgetCursor, Event &touchEvent) override;
};

} // namespace gui
} // namespace eez