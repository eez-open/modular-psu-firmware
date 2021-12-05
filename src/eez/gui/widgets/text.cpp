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

#include <string.h>

#include <eez/util.h>

#include <eez/gui/gui.h>
#include <eez/gui/widgets/text.h>

#define IGNORE_LUMINOSITY_FLAG 1

static const size_t MAX_TEXT_LEN = 128;

namespace eez {
namespace gui {

void TextWidget_autoSize(Assets *assets, TextWidget& widget) {
    const Style *style = getStyle(widget.style);
    font::Font font = styleGetFont(style);
    widget.w = style->border_size_left + style->padding_left + display::measureStr(widget.text.ptr(assets), -1, font, 0) + style->border_size_right + style->padding_right;
    widget.h = style->border_size_top + style->padding_top + font.getHeight() + style->border_size_bottom + style->padding_bottom;
}

void TextWidgetState::draw(WidgetState *previousStateBase) {
    auto previousState = (TextWidgetState *)previousStateBase;
    bool refresh = !previousState || *this != *previousState;
    if (refresh) {
        auto widget = (const TextWidget *)widgetCursor.widget;
        const Style *style = getStyle(overrideStyleHook(widgetCursor, widget->style));
        const char *text = widget->text.ptr(widgetCursor.assets);

        uint16_t overrideColor = flags.focused ? style->focus_color : overrideStyleColorHook(widgetCursor, style);
        uint16_t overrideBackgroundColor = flags.focused ? style->focus_background_color : style->background_color;
        uint16_t overrideActiveColor = flags.focused ? style->focus_background_color : overrideActiveStyleColorHook(widgetCursor, style);
        uint16_t overrideActiveBackgroundColor = flags.focused ? style->focus_color : style->active_background_color;

        bool ignoreLuminosity = (widget->flags & IGNORE_LUMINOSITY_FLAG) != 0;
        if (text && text[0]) {
            drawText(text, -1, widgetCursor.x, widgetCursor.y, (int)widget->w, (int)widget->h,
                style, flags.active,
                flags.blinking,
                ignoreLuminosity, &overrideColor, &overrideBackgroundColor, &overrideActiveColor, &overrideActiveBackgroundColor);
        } else if (widget->data) {
            if (data.isString()) {
                if (data.getOptions() & STRING_OPTIONS_FILE_ELLIPSIS) {
                    const char *fullText = data.getString();
                    int fullTextLength = strlen(fullText);
                    font::Font font = styleGetFont(style);
                    int fullTextWidth = display::measureStr(fullText, fullTextLength, font);
                    if (fullTextWidth <= widget->w) {
                        drawText(fullText, fullTextLength, widgetCursor.x,
                            widgetCursor.y, (int)widget->w, (int)widget->h, style,
                            flags.active,
                            flags.blinking,
                            ignoreLuminosity, &overrideColor, &overrideBackgroundColor, &overrideActiveColor, &overrideActiveBackgroundColor);

                    } else {
                        char text[MAX_TEXT_LEN + 1];
                        int ellipsisWidth = display::measureStr("...", 3, font);
                        int width = ellipsisWidth;
                        int textLength = 3;
                        int iLeft = 0;
                        int iRight = strlen(fullText) - 1;
                        while (iLeft < iRight && textLength < (int)MAX_TEXT_LEN) {
                            int widthLeft = display::measureGlyph(fullText[iLeft], font);
                            if (width + widthLeft > widget->w) {
                                break;
                            }
                            width += widthLeft;
                            iLeft++;
                            textLength++;

                            int widthRight = display::measureGlyph(fullText[iRight], font);
                            if (width + widthRight > widget->w) {
                                break;
                            }
                            width += widthRight;
                            iRight--;
                            textLength++;
                        }

                        memcpy(text, fullText, iLeft);
						text[iLeft] = 0;
                        stringAppendString(text, sizeof(text), "...");
                        stringAppendString(text, sizeof(text), fullText + iRight + 1);

                        drawText(text, textLength, widgetCursor.x,
                            widgetCursor.y, (int)widget->w, (int)widget->h, style,
                            flags.active,
                            flags.blinking,
                            ignoreLuminosity, &overrideColor, &overrideBackgroundColor, &overrideActiveColor, &overrideActiveBackgroundColor);
                    }

                } else {
                    const char *str = data.getString();
                    drawText(str ? str : "", -1, widgetCursor.x,
                        widgetCursor.y, (int)widget->w, (int)widget->h, style,
                        flags.active,
                        flags.blinking,
                        ignoreLuminosity, &overrideColor, &overrideBackgroundColor, &overrideActiveColor, &overrideActiveBackgroundColor);
                }
            } else {
                char text[MAX_TEXT_LEN + 1];
                data.toText(text, sizeof(text));
                drawText(text, -1, widgetCursor.x, widgetCursor.y, (int)widget->w, (int)widget->h,
                    style, flags.active,
                    flags.blinking,
                    ignoreLuminosity, &overrideColor, &overrideBackgroundColor, &overrideActiveColor, &overrideActiveBackgroundColor,
                    data.getType() == VALUE_TYPE_FLOAT);
            }
        }
    }
}

} // namespace gui
} // namespace eez
