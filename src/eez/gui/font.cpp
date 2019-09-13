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

/*
Font header:

offset
0           ascent              uint8
1           descent             uint8
2           encoding start      uint8
3           encoding end        uint8
4           1st encoding offset uint32
6           2nd encoding offset uint32
...
*/

////////////////////////////////////////////////////////////////////////////////

Font::Font() : fontData(0) {
}

Font::Font(const uint8_t *data) : fontData(data) {
}

uint8_t Font::getAscent() {
    return fontData[0];
}

uint8_t Font::getDescent() {
    return fontData[1];
}

uint8_t Font::getEncodingStart() {
    return fontData[2];
}

uint8_t Font::getEncodingEnd() {
    return fontData[3];
}

uint8_t Font::getHeight() {
    return getAscent() + getDescent();
}

const uint8_t *Font::findGlyphData(uint8_t encoding) {
    uint8_t start = getEncodingStart();
    uint8_t end = getEncodingEnd();

    if (encoding < start || encoding > end) {
        // Not found!
        return nullptr;
    }

    const uint8_t *p = fontData + 4 + (encoding - start) * 4;

    typedef uint32_t u32;
    const uint32_t offset = p[0] | (u32(p[1]) << 8) | (u32(p[2]) << 16) | (u32(p[3]) << 24);

    if (*(int8_t *)(fontData + offset) == -1) {
        return nullptr;
    }

    return fontData + offset;
}

void Font::fillGlyphParameters(Glyph &glyph) {
    /*
    Glyph header:

    offset
    0             DWIDTH                    int8
    1             BBX width                 uint8
    2             BBX height                uint8
    3             BBX xoffset               int8
    4             BBX yoffset               int8

    Note: if first byte is 255 that indicates empty glyph
    */

    glyph.dx = (int8_t)glyph.data[0];
    glyph.width = glyph.data[1];
    glyph.height = glyph.data[2];
    glyph.x = (int8_t)glyph.data[3];
    glyph.y = (int8_t)glyph.data[4];
}

void Font::getGlyph(uint8_t requested_encoding, Glyph &glyph) {
    glyph.data = findGlyphData(requested_encoding);
    if (glyph.data) {
        fillGlyphParameters(glyph);
    }
}

} // namespace font
} // namespace gui
} // namespace eez
