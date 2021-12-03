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

struct ScrollBarWidget : public Widget {
    int16_t thumbStyle;
    int16_t buttonsStyle;
    AssetsPtr<const char> leftButtonText;
	AssetsPtr<const char> rightButtonText;
};

enum ScrollBarWidgetSegment {
    SCROLL_BAR_WIDGET_SEGMENT_NONE,
    SCROLL_BAR_WIDGET_SEGMENT_TRACK_LEFT,
    SCROLL_BAR_WIDGET_SEGMENT_TRACK_RIGHT,
    SCROLL_BAR_WIDGET_SEGMENT_THUMB,
    SCROLL_BAR_WIDGET_SEGMENT_LEFT_BUTTON,
    SCROLL_BAR_WIDGET_SEGMENT_RIGHT_BUTTON
};

struct ScrollBarWidgetState : public WidgetState {
    int size;
    int position;
    int pageSize;
    ScrollBarWidgetSegment segment;

    void draw() override;
	bool hasOnTouch() override;
	void onTouch(Event &touchEvent) override;
	bool hasOnKeyboard() override;
	bool onKeyboard(uint8_t key, uint8_t mod) override;
};

} // namespace gui
} // namespace eez
