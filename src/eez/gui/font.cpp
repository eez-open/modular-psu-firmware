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

#include <eez/gui/font.h>

namespace eez {
namespace gui {
namespace font {

Font::Font()
	: fontData(0)
{
}

Font::Font(const FontData *fontData_)
	: fontData(fontData_)
{
}

uint8_t Font::getAscent() {
    return fontData->ascent;
}

uint8_t Font::getDescent() {
    return fontData->descent;
}

uint8_t Font::getHeight() {
    return fontData->ascent + fontData->descent;
}

const GlyphData *Font::getGlyph(int32_t encoding) {
	auto start = fontData->encodingStart;
	auto end = fontData->encodingEnd;

    uint32_t glyphIndex = 0;
	if ((uint32_t)encoding < start || (uint32_t)encoding > end) {
        // TODO use binary search
        uint32_t i;
		for (i = 0; i < fontData->groups.count; i++) {
            auto group = fontData->groups[i];
            if ((uint32_t)encoding >= group->encoding && (uint32_t)encoding < group->encoding + group->length) {
                glyphIndex = group->glyphIndex + (encoding - group->encoding);
                break;
            }
        }
        if (i == fontData->groups.count) {
            return nullptr;
        }
	} else {
        glyphIndex = encoding - start;
    }

	auto glyphData = fontData->glyphs[glyphIndex];

	if (glyphData->dx == -128) {
		// empty glyph
		return nullptr;
	}

	return glyphData;
}

} // namespace font
} // namespace gui
} // namespace eez
