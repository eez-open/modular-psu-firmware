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

#include <eez/flow/private.h>

namespace eez {
namespace gui {

struct InputWidget : public Widget {
	uint16_t flags;
	int16_t min;
	int16_t max;
    int16_t precision;
	int16_t unit;
	uint16_t componentIndex;
};

struct InputWidgetState : public WidgetState {
	WidgetStateFlags flags;
	Value data;

    bool updateState(const WidgetCursor &widgetCursor) override;
	void render(WidgetCursor &widgetCursor) override;
};

struct InputWidgetExecutionState : public flow::ComponenentExecutionState {
	Value min;
	Value max;
    Value precision;
	Unit unit;
};

static const uint16_t INPUT_WIDGET_TYPE_TEXT = 0x0001;
static const uint16_t INPUT_WIDGET_TYPE_NUMBER = 0x0002;
static const uint16_t INPUT_WIDGET_PASSWORD_FLAG = 0x0100;

Value getInputWidgetMin(const gui::WidgetCursor &widgetCursor);
Value getInputWidgetMax(const gui::WidgetCursor &widgetCursor);
Value getInputWidgetPrecision(const gui::WidgetCursor &widgetCursor);
Unit getInputWidgetUnit(const gui::WidgetCursor &widgetCursor);

Value getInputWidgetData(const gui::WidgetCursor &widgetCursor, const Value &dataValue);

} // namespace gui
} // namespace eez