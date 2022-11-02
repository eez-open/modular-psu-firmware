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

#include <stdint.h>

#include <eez/core/assets.h>

namespace eez {
namespace gui {
namespace font {

struct Font {
    const FontData *fontData;

    Font();
    Font(const FontData *fontData_);

	explicit operator bool() const {
		return fontData != nullptr;
	}

    const GlyphData *getGlyph(int32_t encoding);

    uint8_t getAscent();
    uint8_t getDescent();
    uint8_t getHeight();
};

} // namespace font
} // namespace gui
} // namespace eez
