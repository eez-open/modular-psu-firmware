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

#pragma once

namespace eez {
namespace gui {

struct GaugeWidget : public Widget {
    int16_t min;
	int16_t max;
	int16_t threshold;
	int16_t unit;
    int16_t barStyle;
	int16_t valueStyle;
    int16_t ticksStyle;
	int16_t thresholdStyle;
};

struct GaugeWidgetState : public WidgetState {
	void draw(WidgetState *previousState) override;
};

} // namespace gui
} // namespace eez
