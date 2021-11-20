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

#include <math.h>
#include <memory.h>

#include <eez/system.h>
#include <eez/util.h>

#include <eez/gui/gui.h>

using namespace eez::mcu;

namespace eez {
namespace gui {

#define BAR_GRAPH_ORIENTATION_LEFT_RIGHT 1
#define BAR_GRAPH_ORIENTATION_RIGHT_LEFT 2
#define BAR_GRAPH_ORIENTATION_TOP_BOTTOM 3
#define BAR_GRAPH_ORIENTATION_BOTTOM_TOP 4
#define BAR_GRAPH_ORIENTATION_MASK 0x0F
#define BAR_GRAPH_DO_NOT_DISPLAY_VALUE (1 << 4)

struct BarGraphWidget : public Widget {
    int16_t textStyle;
    int16_t line1Data;
    int16_t line1Style;
    int16_t line2Data;
    int16_t line2Style;
    uint8_t orientation; // BAR_GRAPH_ORIENTATION_...
};

struct BarGraphWidgetState : public WidgetState {
    uint16_t color;
    uint16_t backgroundColor;
    uint16_t activeColor;
    uint16_t activeBackgroundColor;
    Value line1Data;
    Value line2Data;
    Value textData;
    uint32_t textDataRefreshLastTime;
};

EnumFunctionType BAR_GRAPH_enum = nullptr;

int calcValuePosInBarGraphWidget(Value &value, float min, float max, int d) {
    return (int)roundf((value.getFloat() - min) * d / (max - min));
}

int calcValuePosInBarGraphWidgetWithClamp(Value &value, float min, float max, int d) {
    int p = calcValuePosInBarGraphWidget(value, min, max, d);

    if (p < 0) {
        p = 0;
    } else if (p >= d) {
        p = d - 1;
    }

    return p;
}

void drawLineInBarGraphWidget(const BarGraphWidget *barGraphWidget, int p, uint16_t lineStyleID, int x, int y, int w, int h) {
    const Style *style = getStyle(lineStyleID);

    display::setColor(style->color);
    if ((barGraphWidget->orientation & BAR_GRAPH_ORIENTATION_MASK) == BAR_GRAPH_ORIENTATION_LEFT_RIGHT) {
        display::drawVLine(x + p, y, h);
    } else if ((barGraphWidget->orientation & BAR_GRAPH_ORIENTATION_MASK) == BAR_GRAPH_ORIENTATION_RIGHT_LEFT) {
        display::drawVLine(x - p, y, h);
    } else if ((barGraphWidget->orientation & BAR_GRAPH_ORIENTATION_MASK) == BAR_GRAPH_ORIENTATION_TOP_BOTTOM) {
        display::drawHLine(x, y + p, w);
    } else {
        display::drawHLine(x, y - p, w);
    }
}

DrawFunctionType BAR_GRAPH_draw = [](const WidgetCursor &widgetCursor) {
    auto widget = (const BarGraphWidget *)widgetCursor.widget;
    const Style* style = getStyle(overrideStyleHook(widgetCursor, widget->style));

    auto blinking = g_isBlinkTime && isBlinking(widgetCursor, widget->data);
    auto data = get(widgetCursor, widget->data);
    
    auto color = getColor(widgetCursor, widget->data, style);
    auto backgroundColor = getBackgroundColor(widgetCursor, widget->data, style);
    auto activeColor = getActiveColor(widgetCursor, widget->data, style);
    auto activeBackgroundColor = getActiveBackgroundColor(widgetCursor, widget->data, style);

    auto line1Data = get(widgetCursor, widget->line1Data);
    
    auto line2Data = get(widgetCursor, widget->line2Data);

	// TODO refresh getTextRefreshRate
    auto textData = data;
   
    int x = widgetCursor.x;
    int y = widgetCursor.y;
    const int w = widget->w;
    const int h = widget->h;

    float min = getMin(widgetCursor, widget->data).getFloat();

    float max;
    Value displayValueRange = getDisplayValueRange(widgetCursor, widget->data);
    if (displayValueRange.getType() == VALUE_TYPE_FLOAT) {
        max = displayValueRange.getFloat();
    } else {
        max = getMax(widgetCursor, widget->data).getFloat();
    }

    bool horizontal = 
        (widget->orientation & BAR_GRAPH_ORIENTATION_MASK) == BAR_GRAPH_ORIENTATION_LEFT_RIGHT ||
        (widget->orientation & BAR_GRAPH_ORIENTATION_MASK) == BAR_GRAPH_ORIENTATION_RIGHT_LEFT;

    int d = horizontal ? w : h;

    // calc bar position (monitored value)
    int pValue = calcValuePosInBarGraphWidgetWithClamp(data, min, max, d);

    // calc line 1 position (set value)
    int pLine1 = calcValuePosInBarGraphWidget(line1Data, min, max, d);
    bool drawLine1 = pLine1 >= 0 && pLine1 < d;

    // calc line 2 position (limit value)
    int pLine2 = calcValuePosInBarGraphWidget(line2Data, min, max, d);
    bool drawLine2 = pLine2 >= 0 && pLine2 < d;

    if (drawLine1 && drawLine2) {
        // make sure line positions don't overlap
        if (pLine1 == pLine2) {
            pLine1 = pLine2 - 1;
        }

        // make sure all lines are visible
        if (pLine1 < 0) {
            pLine2 -= pLine1;
            pLine1 = 0;
        }
    }

    Style textStyle;
    memcpy(&textStyle, widget->textStyle ? getStyle(widget->textStyle) : style, sizeof(Style));
    textStyle.color = g_isActiveWidget || blinking ? activeColor : color;

    uint16_t fg = g_isActiveWidget || blinking ? activeColor : color;
    uint16_t bg = g_isActiveWidget || blinking ? activeBackgroundColor : backgroundColor;

    if (horizontal) {
        // calc text position
        char valueText[64];
        int pText = 0;
        int wText = 0;
        if (!(widget->orientation & BAR_GRAPH_DO_NOT_DISPLAY_VALUE)) {
            if (widget->textStyle) {
                font::Font font = styleGetFont(&textStyle);

                textData.toText(valueText, sizeof(valueText));
                wText = display::measureStr(valueText, -1, font, w);

                int padding = textStyle.padding_left;
                wText += padding;

                if (pValue + wText <= d) {
                    textStyle.background_color = bg;
                    pText = pValue;
                } else {
                    textStyle.background_color = fg;
                    textStyle.color = textStyle.active_color;
                    wText += padding;
                    pText = MAX(pValue - wText, 0);
                }
            }
        } else {
            pText = pValue;
        }

        if ((widget->orientation & BAR_GRAPH_ORIENTATION_MASK) == BAR_GRAPH_ORIENTATION_LEFT_RIGHT) {
            // draw bar
            if (pText > 0) {
                display::setColor(fg);
                display::fillRect(x, y, pText, h);
            }

            if (!(widget->orientation & BAR_GRAPH_DO_NOT_DISPLAY_VALUE)) {
                drawText(valueText, -1, x + pText, y, wText, h, &textStyle, false, false, false, nullptr, nullptr, nullptr, nullptr);
            }

            // draw background, but do not draw over line 1 and line 2
            display::setColor(bg);

            int pBackground = pText + wText;

            if (drawLine1) {
                if (pBackground <= pLine1) {
                    if (pBackground < pLine1) {
                        display::fillRect(x + pBackground, y, pLine1 - pBackground, y + h - 1);
                    }
                    pBackground = pLine1 + 1;
                }
            }

            if (drawLine2) {
                if (pBackground <= pLine2) {
                    if (pBackground < pLine2) {
                        display::fillRect(x + pBackground, y, pLine2 - pBackground, h);
                    }
                    pBackground = pLine2 + 1;
                }
            }

            if (pBackground < d) {
                display::fillRect(x + pBackground, y, d - pBackground, h);
            }
        } else {
            x += w - 1;

            // draw bar
            if (pText > 0) {
                display::setColor(fg);
                display::fillRect(x - (pText - 1), y, pText, h);
            }

            if (!(widget->orientation & BAR_GRAPH_DO_NOT_DISPLAY_VALUE)) {
                drawText(valueText, -1, x - (pText + wText - 1), y, wText, h, &textStyle, false, false, false, nullptr, nullptr, nullptr, nullptr);
            }

            // draw background, but do not draw over line 1 and line 2
            display::setColor(bg);

            int pBackground = pText + wText;

            if (drawLine1) {
                if (pBackground <= pLine1) {
                    if (pBackground < pLine1) {
                        display::fillRect(x - (pLine1 - 1), y, pLine1 - pBackground, h);
                    }
                    pBackground = pLine1 + 1;
                }
            }

            if (drawLine2) {
                if (pBackground <= pLine2) {
                    if (pBackground < pLine2) {
                        display::fillRect(x - (pLine2 - 1), y, pLine2 - pBackground, h);
                    }
                    pBackground = pLine2 + 1;
                }
            }

            if (pBackground < d) {
                display::fillRect(x - (d - 1), y, d - pBackground, h);
            }
        }

        if (drawLine1) {
            drawLineInBarGraphWidget(widget, pLine1, widget->line1Style, x, y, w, h);
        }
        if (drawLine2) {
            drawLineInBarGraphWidget(widget, pLine2, widget->line2Style, x, y, w, h);
        }
    } else {
        // calc text position
        char valueText[64];
        int pText = 0;
        int hText = 0;
        if (widget->textStyle) {
            font::Font font = styleGetFont(&textStyle);

            textData.toText(valueText, sizeof(valueText));
            hText = font.getHeight();

            int padding = textStyle.padding_top;
            hText += padding;

            if (pValue + hText <= d) {
                textStyle.background_color = bg;
                pText = pValue;
            } else {
                textStyle.background_color = fg;
                textStyle.color = textStyle.active_color;
                hText += padding;
                pText = MAX(pValue - hText, 0);
            }
        }

        if ((widget->orientation & BAR_GRAPH_ORIENTATION_MASK) == BAR_GRAPH_ORIENTATION_TOP_BOTTOM) {
            // draw bar
            if (pText > 0) {
                display::setColor(fg);
                display::fillRect(x, y, w, pText);
            }

            drawText(valueText, -1, x, y + pText, w, hText, &textStyle, false, false, false, nullptr, nullptr, nullptr, nullptr);

            // draw background, but do not draw over line 1 and line 2
            display::setColor(bg);

            int pBackground = pText + hText;

            if (drawLine1) {
                if (pBackground <= pLine1) {
                    if (pBackground < pLine1) {
                        display::fillRect(x, y + pBackground, w, pLine1 - pBackground);
                    }
                    pBackground = pLine1 + 1;
                }
            }

            if (drawLine2) {
                if (pBackground <= pLine2) {
                    if (pBackground < pLine2) {
                        display::fillRect(x, y + pBackground, w, pLine2 - pBackground);
                    }
                    pBackground = pLine2 + 1;
                }
            }

            if (pBackground < d) {
                display::fillRect(x, y + pBackground, w, d - pBackground);
            }
        } else {
            y += h - 1;

            // draw bar
            if (pText > 0) {
                display::setColor(fg);
                display::fillRect(x, y - (pText - 1), w, pText);
            }

            drawText(valueText, -1, x, y - (pText + hText - 1), w, hText, &textStyle, false, false, false, nullptr, nullptr, nullptr, nullptr);

            // draw background, but do not draw over line 1 and line 2
            display::setColor(bg);

            int pBackground = pText + hText;

            if (drawLine1) {
                if (pBackground <= pLine1) {
                    if (pBackground < pLine1) {
                        display::fillRect(x, y - (pLine1 - 1), w, pLine1 - pBackground);
                    }
                    pBackground = pLine1 + 1;
                }
            }

            if (drawLine2) {
                if (pBackground <= pLine2) {
                    if (pBackground < pLine2) {
                        display::fillRect(x, y - (pLine2 - 1), w, pLine2 - pBackground);
                    }
                    pBackground = pLine2 + 1;
                }
            }

            if (pBackground < d) {
                display::fillRect(x, y - (d - 1), w, d - pBackground);
            }
        }

        if (drawLine1) {
            drawLineInBarGraphWidget(widget, pLine1, widget->line1Style, x, y, w, h);
        }
        if (drawLine2) {
            drawLineInBarGraphWidget(widget, pLine2, widget->line2Style, x, y, w, h);
        }
    }
};

OnTouchFunctionType BAR_GRAPH_onTouch = nullptr;

OnKeyboardFunctionType BAR_GRAPH_onKeyboard = nullptr;

} // namespace gui
} // namespace eez

#endif
