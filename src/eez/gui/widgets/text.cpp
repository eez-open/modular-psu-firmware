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

#if OPTION_DISPLAY

#include <string.h>

#include <eez/util.h>

#include <eez/gui/gui.h>
#include <eez/gui/widgets/text.h>

#define IGNORE_LUMINOSITY_FLAG 1

namespace eez {
namespace gui {

FixPointersFunctionType TEXT_fixPointers = [](Widget *widget, Assets *assets) {
    TextWidgetSpecific *textWidget = (TextWidgetSpecific *)widget->specific;
    textWidget->text = (const char *)((uint8_t *)(void *)assets->document + (uint32_t)textWidget->text);
};

EnumFunctionType TEXT_enum = nullptr;

void TextWidget_autoSize(TextWidget& widget) {
    const Style *style = getStyle(widget.common.style);
    font::Font font = styleGetFont(style);
    widget.common.w = style->border_size_left + style->padding_left + mcu::display::measureStr(widget.specific.text, -1, font, 0) + style->border_size_right + style->padding_right;
    widget.common.h = style->border_size_top + style->padding_top + font.getHeight() + style->border_size_bottom + style->padding_bottom;
}

DrawFunctionType TEXT_draw = [](const WidgetCursor &widgetCursor) {
    const Widget *widget = widgetCursor.widget;
    const TextWidgetSpecific *textWidget = GET_WIDGET_PROPERTY(widget, specific, const TextWidgetSpecific *);

    widgetCursor.currentState->size = sizeof(WidgetState);
    widgetCursor.currentState->flags.focused = isFocusWidget(widgetCursor);
    
    const Style *style = getStyle(overrideStyleHook(widgetCursor, widget->style));

    const char *text = GET_WIDGET_PROPERTY(textWidget, text, const char *);

    widgetCursor.currentState->flags.blinking = g_isBlinkTime && styleIsBlink(style);
    widgetCursor.currentState->data = !(text && text[0]) && widget->data ? get(widgetCursor.cursor, widget->data) : 0;

    bool refresh =
        !widgetCursor.previousState ||
        widgetCursor.previousState->flags.focused != widgetCursor.currentState->flags.focused ||
        widgetCursor.previousState->flags.active != widgetCursor.currentState->flags.active ||
        widgetCursor.previousState->flags.blinking != widgetCursor.currentState->flags.blinking ||
        widgetCursor.previousState->data != widgetCursor.currentState->data;

    static const size_t MAX_TEXT_LEN = 128;

    if (refresh) {
        uint16_t overrideColor = widgetCursor.currentState->flags.focused ? style->focus_color : overrideStyleColorHook(widgetCursor, style);
        uint16_t overrideBackgroundColor = widgetCursor.currentState->flags.focused ? style->focus_background_color : style->background_color;
        uint16_t overrideActiveColor =  widgetCursor.currentState->flags.focused ? style->focus_background_color : overrideActiveStyleColorHook(widgetCursor, style);

        bool ignoreLuminosity = (textWidget->flags & IGNORE_LUMINOSITY_FLAG) != 0;
        if (text && text[0]) {
            drawText(text, -1, widgetCursor.x, widgetCursor.y, (int)widget->w, (int)widget->h,
                style, widgetCursor.currentState->flags.active,
                widgetCursor.currentState->flags.blinking,
                ignoreLuminosity, &overrideColor, &overrideBackgroundColor, nullptr, nullptr);
        } else if (widget->data) {
            if (widgetCursor.currentState->data.isString()) {
                if (widgetCursor.currentState->data.getOptions() & STRING_OPTIONS_FILE_ELLIPSIS) {
                    const char *fullText = widgetCursor.currentState->data.getString();
                    int fullTextLength = strlen(fullText);
                    font::Font font = styleGetFont(style);
                    int fullTextWidth = mcu::display::measureStr(fullText, fullTextLength, font);
                    if (fullTextWidth <= widget->w) {
                        drawText(fullText, fullTextLength, widgetCursor.x,
                            widgetCursor.y, (int)widget->w, (int)widget->h, style,
                            widgetCursor.currentState->flags.active,
                            widgetCursor.currentState->flags.blinking,
                            ignoreLuminosity, &overrideColor, &overrideBackgroundColor, nullptr, nullptr);

                    } else {
                        char text[MAX_TEXT_LEN + 1];
                        int ellipsisWidth = mcu::display::measureStr("...", 3, font);
                        int width = ellipsisWidth;
                        int textLength = 3;
                        int iLeft = 0;
                        int iRight = strlen(fullText) - 1;
                        while (iLeft < iRight && textLength < (int)MAX_TEXT_LEN) {
                            int widthLeft = mcu::display::measureGlyph(fullText[iLeft], font);
                            if (width + widthLeft > widget->w) {
                                break;
                            }
                            width += widthLeft;
                            iLeft++;
                            textLength++;

                            int widthRight = mcu::display::measureGlyph(fullText[iRight], font);
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
                            widgetCursor.currentState->flags.active,
                            widgetCursor.currentState->flags.blinking,
                            ignoreLuminosity, &overrideColor, &overrideBackgroundColor, nullptr, nullptr);
                    }

                } else {
                    const char *str = widgetCursor.currentState->data.getString();
                    drawText(str ? str : "", -1, widgetCursor.x,
                        widgetCursor.y, (int)widget->w, (int)widget->h, style,
                        widgetCursor.currentState->flags.active,
                        widgetCursor.currentState->flags.blinking,
                        ignoreLuminosity, &overrideColor, &overrideBackgroundColor, nullptr, nullptr);
                }
            } else {
                char text[MAX_TEXT_LEN + 1];
                widgetCursor.currentState->data.toText(text, sizeof(text));
                drawText(text, -1, widgetCursor.x, widgetCursor.y, (int)widget->w, (int)widget->h,
                    style, widgetCursor.currentState->flags.active,
                    widgetCursor.currentState->flags.blinking,
                    ignoreLuminosity, &overrideColor, &overrideBackgroundColor, &overrideActiveColor, nullptr,
                    widgetCursor.currentState->data.getType() == VALUE_TYPE_FLOAT);
            }
        }
    }
};

OnTouchFunctionType TEXT_onTouch = nullptr;

OnKeyboardFunctionType TEXT_onKeyboard = nullptr;

} // namespace gui
} // namespace eez

#endif
