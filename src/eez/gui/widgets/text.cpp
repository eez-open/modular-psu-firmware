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

#include <stdio.h>
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
    widget.w = style->borderSizeLeft + style->paddingLeft + display::measureStr(widget.text.ptr(assets), -1, font, 0) + style->borderSizeRight + style->paddingRight;
    widget.h = style->borderSizeTop + style->paddingTop + font.getHeight() + style->borderSizeBottom + style->paddingBottom;
}

bool TextWidgetState::updateState() {
    const WidgetCursor &widgetCursor = g_widgetCursor;

    bool hasPreviousState = widgetCursor.hasPreviousState;
    auto widget = (const TextWidget *)widgetCursor.widget;
    const Style *style = getStyle(overrideStyleHook(widgetCursor, widget->style));

    WIDGET_STATE(flags.active, g_isActiveWidget);
    WIDGET_STATE(flags.focused, isFocusWidget(widgetCursor));

    WIDGET_STATE(flags.blinking, g_isBlinkTime && styleIsBlink(style));
    
    const char *text = widget->text ? widget->text.ptr(widgetCursor.assets) : nullptr;
	WIDGET_STATE(data, !(text && text[0]) && widget->data ? get(widgetCursor, widget->data) : 0);

    return !hasPreviousState;
}

void TextWidgetState::render() {
    const WidgetCursor &widgetCursor = g_widgetCursor;

    auto widget = (const TextWidget *)widgetCursor.widget;
    const Style *style = getStyle(overrideStyleHook(widgetCursor, widget->style));
    const char *text = widget->text ? widget->text.ptr(widgetCursor.assets) : nullptr;

    uint16_t overrideColor                 = flags.focused ? style->focusColor           : overrideStyleColorHook(widgetCursor, style);
    uint16_t overrideBackgroundColor       = flags.focused ? style->focusBackgroundColor : style->backgroundColor;
    uint16_t overrideActiveColor           = flags.focused ? style->focusBackgroundColor : overrideActiveStyleColorHook(widgetCursor, style);
    uint16_t overrideActiveBackgroundColor = flags.focused ? style->focusColor           : style->activeBackgroundColor;

    bool ignoreLuminosity = (widget->flags & IGNORE_LUMINOSITY_FLAG) != 0;
    if (text && text[0]) {
        drawText(
            text, -1,
            widgetCursor.x, widgetCursor.y, (int)widget->w, (int)widget->h,
            style,
            flags.active, flags.blinking, ignoreLuminosity,
            &overrideColor, &overrideBackgroundColor, &overrideActiveColor, &overrideActiveBackgroundColor
        );
    } else if (widget->data) {
        if (data.isString()) {
            if (data.getOptions() & STRING_OPTIONS_FILE_ELLIPSIS) {
                const char *fullText = data.getString();
                int fullTextLength = strlen(fullText);
                font::Font font = styleGetFont(style);
                int fullTextWidth = display::measureStr(fullText, fullTextLength, font);
                if (fullTextWidth <= widget->w) {
                    drawText(
                        fullText, fullTextLength,
                        widgetCursor.x, widgetCursor.y, (int)widget->w, (int)widget->h,
                        style,
                        flags.active, flags.blinking, ignoreLuminosity,
                        &overrideColor, &overrideBackgroundColor, &overrideActiveColor, &overrideActiveBackgroundColor
                    );
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

                    drawText(
                        text, textLength,
                        widgetCursor.x, widgetCursor.y, (int)widget->w, (int)widget->h,
                        style,
                        flags.active, flags.blinking, ignoreLuminosity,
                        &overrideColor, &overrideBackgroundColor, &overrideActiveColor, &overrideActiveBackgroundColor
                    );
                }

            } else {
                const char *str = data.getString();
                drawText(
                    str ? str : "", -1,
                    widgetCursor.x, widgetCursor.y, (int)widget->w, (int)widget->h,
                    style,
                    flags.active, flags.blinking, ignoreLuminosity,
                    &overrideColor, &overrideBackgroundColor, &overrideActiveColor, &overrideActiveBackgroundColor
                );
            }
        } else {
            char text[MAX_TEXT_LEN + 1];
            data.toText(text, sizeof(text));
            drawText(
                text, -1,
                widgetCursor.x, widgetCursor.y, (int)widget->w, (int)widget->h,
                style,
                flags.active, flags.blinking, ignoreLuminosity,
                &overrideColor, &overrideBackgroundColor, &overrideActiveColor, &overrideActiveBackgroundColor,
                data.getType() == VALUE_TYPE_FLOAT
            );
        }
    }
}

} // namespace gui
} // namespace eez
