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

struct BarGraphWidget : public Widget {
    int16_t textStyle;
    int16_t line1Data;
    int16_t line1Style;
    int16_t line2Data;
    int16_t line2Style;
    uint8_t orientation; // BAR_GRAPH_ORIENTATION_...
};

struct BarGraphWidgetState : public WidgetState {
    uint16_t color;
    uint16_t backgroundColor;
    uint16_t activeColor;
    uint16_t activeBackgroundColor;
    Value line1Data;
    Value line2Data;
    Value textData;
    uint32_t textDataRefreshLastTime;

    void draw(WidgetState *previousState) override;
};

} // namespace gui
} // namespace eez
