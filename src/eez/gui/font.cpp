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

Font::Font() : fontData(0) {
}

Font::Font(const FontData *fontData_) : fontData(fontData_) {
}

uint8_t Font::getAscent() {
    return fontData->ascent;
}

uint8_t Font::getDescent() {
    return fontData->descent;
}

uint8_t Font::getEncodingStart() {
	return fontData->encodingStart;
;
}

uint8_t Font::getEncodingEnd() {
    return fontData->encodingEnd;
}

uint8_t Font::getHeight() {
    return getAscent() + getDescent();
}

const GlyphData *Font::getGlyph(uint8_t encoding) {
	auto start = getEncodingStart();
	auto end = getEncodingEnd();

	if (encoding < start || encoding > end) {
		// Not found!
		return nullptr;
	}

	GlyphData *glyphData = ((FontData *)fontData)->glyphs[encoding - start].ptr(g_mainAssets);

	if (glyphData->dx == -128) {
		// empty glyph
		return nullptr;
	}

	return glyphData;
}

} // namespace font
} // namespace gui
} // namespace eez
