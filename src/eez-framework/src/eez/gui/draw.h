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

namespace eez {
namespace gui {

inline font::Font styleGetFont(const Style *style) {
    return font::Font(getFontData(style->font));
}

inline bool styleIsBlink(const Style *style) {
    return style->flags & STYLE_FLAGS_BLINK;
}

void drawBorderAndBackground(int &x1, int &y1, int &x2, int &y2, const Style *style, uint16_t color, bool ignoreLuminocity = false);

void drawText(
    const char *text, int textLength,
    int x, int y, int w, int h,
    const Style *style,
    bool active = false, bool blink = false, bool ignoreLuminocity = false,
    uint16_t *overrideColor = nullptr, uint16_t *overrideBackgroundColor = nullptr,
    uint16_t *overrideActiveColor = nullptr, uint16_t *overrideActiveBackgroundColor = nullptr,
    bool useSmallerFontIfDoesNotFit = false, int cursorPosition = -1, int xScroll = 0,
	bool boolSkipBackground = false
);
int getCharIndexAtPosition(int xPos, const char *text, int textLength, int x, int y, int w, int h, const Style *style);
int getCursorXPosition(int cursorPosition, const char *text, int textLength, int x, int y, int w, int h, const Style *style);

void drawMultilineText(const char *text, int x, int y, int w, int h, const Style *style, bool active, bool blinking, int firstLineIndent, int hangingIndent);
int measureMultilineText(const char *text, int x, int y, int w, int h, const Style *style, int firstLineIndent, int hangingIndent);

void drawBitmap(Image *image, int x, int y, int w, int h, const Style *style, bool active);
void drawRectangle(int x, int y, int w, int h, const Style *style, bool active = false, bool ignoreLuminocity = false, bool invertColors = true);

void drawShadow(int x1, int y1, int x2, int y2);
void expandRectWithShadow(int &x1, int &y1, int &x2, int &y2);

void drawLine(int x1, int y1, int x2, int y2);
void drawAntialiasedLine(int x1, int y1, int x2, int y2);

} // namespace gui
} // namespace eez
