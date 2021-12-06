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

struct MultilineTextWidget : public Widget {
    AssetsPtr<const char> text;
    int16_t firstLineIndent;
    int16_t hangingIndent;
};

struct MultilineTextWidgetState : public WidgetState {
	WidgetStateFlags flags;
	Value data;

    bool updateState(const WidgetCursor &widgetCursor) override;
    void render(WidgetCursor &widgetCursor) override;
};

} // namespace gui
} // namespace eez
