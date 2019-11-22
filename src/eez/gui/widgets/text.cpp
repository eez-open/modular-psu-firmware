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

#include <eez/gui/widgets/text.h>

#include <eez/gui/draw.h>
#include <eez/gui/gui.h>
#include <eez/gui/app_context.h>
#include <eez/util.h>
#include <eez/modules/mcu/display.h>

namespace eez {
namespace gui {

#if OPTION_SDRAM
void TextWidget_fixPointers(Widget *widget) {
    TextWidget *textWidget = (TextWidget *)widget->specific;
    textWidget->text = (const char *)((uint8_t *)g_document + (uint32_t)textWidget->text);
}
#endif

void TextWidget_draw(const WidgetCursor &widgetCursor) {
    const Widget *widget = widgetCursor.widget;
    const TextWidget *textWidget = GET_WIDGET_PROPERTY(widget, specific, const TextWidget *);

    widgetCursor.currentState->size = sizeof(WidgetState);
    widgetCursor.currentState->flags.focused = g_appContext->isFocusWidget(widgetCursor);
    
    const Style *style = getStyle(widgetCursor.currentState->flags.focused ? textWidget->focusStyle : widget->style);

    const char *text = GET_WIDGET_PROPERTY(textWidget, text, const char *);

    widgetCursor.currentState->flags.blinking = g_isBlinkTime && styleIsBlink(style);
    widgetCursor.currentState->data = !(text && text[0]) && widget->data ? data::get(widgetCursor.cursor, widget->data) : 0;

    bool refresh =
        !widgetCursor.previousState ||
        widgetCursor.previousState->flags.focused != widgetCursor.currentState->flags.focused ||
        widgetCursor.previousState->flags.active != widgetCursor.currentState->flags.active ||
        widgetCursor.previousState->flags.blinking != widgetCursor.currentState->flags.blinking ||
        widgetCursor.previousState->data != widgetCursor.currentState->data;

    if (refresh) {
        uint16_t overrideColor = overrideStyleColorHook(widgetCursor, style);
        uint16_t overrideActiveColor = overrideActiveStyleColorHook(widgetCursor, style);

        bool ignoreLuminosity = (textWidget->flags & IGNORE_LUMINOSITY_FLAG) != 0;
        if (text && text[0]) {
            drawText(text, -1, widgetCursor.x, widgetCursor.y, (int)widget->w, (int)widget->h,
                style, widgetCursor.currentState->flags.active,
                widgetCursor.currentState->flags.blinking,
                ignoreLuminosity, &overrideColor, nullptr, nullptr, nullptr);
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
                            ignoreLuminosity, &overrideColor, nullptr, nullptr, nullptr);

                    } else {
                        char text[64];
                        int ellipsisWidth = mcu::display::measureStr("...", 3, font);
                        int width = ellipsisWidth;
                        int textLength = 3;
                        int iLeft = 0;
                        int iRight = strlen(fullText) - 1;
                        while (iLeft < iRight && textLength < 64) {
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

                        strncpy(text, fullText, iLeft);
                        strcpy(text + iLeft, "...");
                        strcpy(text + iLeft + 3, fullText + iRight + 1);

                        drawText(text, textLength, widgetCursor.x,
                            widgetCursor.y, (int)widget->w, (int)widget->h, style,
                            widgetCursor.currentState->flags.active,
                            widgetCursor.currentState->flags.blinking,
                            ignoreLuminosity, &overrideColor, nullptr, nullptr, nullptr);
                    }

                } else {
                    drawText(widgetCursor.currentState->data.getString(), -1, widgetCursor.x,
                        widgetCursor.y, (int)widget->w, (int)widget->h, style,
                        widgetCursor.currentState->flags.active,
                        widgetCursor.currentState->flags.blinking,
                        ignoreLuminosity, &overrideColor, nullptr, nullptr, nullptr);
                }
            } else {
                char text[64];
                widgetCursor.currentState->data.toText(text, sizeof(text));
                drawText(text, -1, widgetCursor.x, widgetCursor.y, (int)widget->w, (int)widget->h,
                    style, widgetCursor.currentState->flags.active,
                    widgetCursor.currentState->flags.blinking,
                    ignoreLuminosity, &overrideColor, nullptr, &overrideActiveColor, nullptr);
            }
        }
    }
}

} // namespace gui
} // namespace eez

#endif
