/*
 * EEZ Modular Firmware
 * Copyright (C) 2022-present, Envox d.o.o.
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

#define Y_AXIS_RANGE_OPTION_FIXED 0
#define Y_AXIS_RANGE_OPTION_FLOATING 1

struct LineChartWidget : public Widget {
    int16_t title;
    int16_t showLegend;

    int16_t yAxisRangeOption;
    int16_t yAxisRangeFrom;
    int16_t yAxisRangeTo;

    int16_t marginLeft;
    int16_t marginTop;
    int16_t marginRight;
    int16_t marginBottom;

    int16_t legendStyle;
	int16_t xAxisStyle;
    int16_t yAxisStyle;

    uint16_t componentIndex;

    uint16_t reserved;
};

struct LineChartWidgetState : public WidgetState {
    WidgetStateFlags flags;
    Value title;
    Value showLegendValue;
    Value yAxisRangeFrom;
    Value yAxisRangeTo;

    bool updateState() override;
	void render() override;
};

} // namespace gui
} // namespace eez
