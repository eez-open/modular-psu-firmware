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
    uint16_t textStyle;
    int16_t line1Data;
    uint16_t line1Style;
    int16_t line2Data;
    uint16_t line2Style;
    uint8_t orientation; // BAR_GRAPH_ORIENTATION_...
};

struct BarGraphWidgetState {
    WidgetState genericState;
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
        display::drawVLine(x + p, y, h - 1);
    } else if ((barGraphWidget->orientation & BAR_GRAPH_ORIENTATION_MASK) == BAR_GRAPH_ORIENTATION_RIGHT_LEFT) {
        display::drawVLine(x - p, y, h - 1);
    } else if ((barGraphWidget->orientation & BAR_GRAPH_ORIENTATION_MASK) == BAR_GRAPH_ORIENTATION_TOP_BOTTOM) {
        display::drawHLine(x, y + p, w - 1);
    } else {
        display::drawHLine(x, y - p, w - 1);
    }
}

DrawFunctionType BAR_GRAPH_draw = [](const WidgetCursor &widgetCursor) {
    const Widget *widget = widgetCursor.widget;
    auto barGraphWidget = (const BarGraphWidget *)widget;
    const Style* style = getStyle(overrideStyleHook(widgetCursor, widget->style));

    widgetCursor.currentState->size = sizeof(BarGraphWidgetState);

    widgetCursor.currentState->flags.blinking = g_isBlinkTime && isBlinking(widgetCursor, widget->data);
    widgetCursor.currentState->data = get(widgetCursor.cursor, widget->data);
    
    auto currentState = (BarGraphWidgetState *)widgetCursor.currentState;
    auto previousState = (BarGraphWidgetState *)widgetCursor.previousState;

    currentState->color = getColor(widgetCursor.cursor, widget->data, style);
    currentState->backgroundColor = getBackgroundColor(widgetCursor.cursor, widget->data, style);
    currentState->activeColor = getActiveColor(widgetCursor.cursor, widget->data, style);
    currentState->activeBackgroundColor = getActiveBackgroundColor(widgetCursor.cursor, widget->data, style);

    currentState->line1Data = get(widgetCursor.cursor, barGraphWidget->line1Data);
    currentState->line2Data = get(widgetCursor.cursor, barGraphWidget->line2Data);

    uint32_t currentTime = millis();
    currentState->textData = widgetCursor.currentState->data;
    bool refreshTextData;
    if (previousState) {
        refreshTextData = currentState->textData != previousState->textData;
        if (refreshTextData) {
            uint32_t refreshRate = getTextRefreshRate(widgetCursor.cursor, widget->data);
            if (refreshRate != 0) {
                refreshTextData = (currentTime - previousState->textDataRefreshLastTime) > refreshRate;
                if (!refreshTextData) {
                    currentState->textData = previousState->textData;
                }
            }
        }
    } else {
        refreshTextData = true;
    }
    currentState->textDataRefreshLastTime = refreshTextData ? currentTime : previousState->textDataRefreshLastTime;
   
    bool refresh =
        !widgetCursor.previousState ||
        widgetCursor.previousState->flags.active != widgetCursor.currentState->flags.active ||
        widgetCursor.previousState->flags.blinking != widgetCursor.currentState->flags.blinking ||
        widgetCursor.previousState->data != widgetCursor.currentState->data ||
        currentState->color != previousState->color ||
        currentState->backgroundColor != previousState->backgroundColor ||
        currentState->activeColor != previousState->activeColor ||
        currentState->activeBackgroundColor != previousState->activeBackgroundColor ||
        previousState->line1Data != currentState->line1Data ||
        previousState->line2Data != currentState->line2Data ||
        refreshTextData;

    if (refresh) {
        int x = widgetCursor.x;
        int y = widgetCursor.y;
        const int w = widget->w;
        const int h = widget->h;

        float min = getMin(widgetCursor.cursor, widget->data).getFloat();

        float max;
		Value displayValueRange = getDisplayValueRange(widgetCursor.cursor, widget->data);
		if (displayValueRange.getType() == VALUE_TYPE_FLOAT) {
            max = displayValueRange.getFloat();
        } else {
            max = getMax(widgetCursor.cursor, widget->data).getFloat();
        }

        bool horizontal = 
            (barGraphWidget->orientation & BAR_GRAPH_ORIENTATION_MASK) == BAR_GRAPH_ORIENTATION_LEFT_RIGHT || 
            (barGraphWidget->orientation & BAR_GRAPH_ORIENTATION_MASK) == BAR_GRAPH_ORIENTATION_RIGHT_LEFT;

        int d = horizontal ? w : h;

        // calc bar position (monitored value)
        int pValue = calcValuePosInBarGraphWidgetWithClamp(widgetCursor.currentState->data, min, max, d);

        // calc line 1 position (set value)
        int pLine1 = calcValuePosInBarGraphWidget(currentState->line1Data, min, max, d);
        bool drawLine1 = pLine1 >= 0 && pLine1 < d;

        // calc line 2 position (limit value)
        int pLine2 = calcValuePosInBarGraphWidget(currentState->line2Data, min, max, d);
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
        memcpy(&textStyle, barGraphWidget->textStyle ? getStyle(barGraphWidget->textStyle) : style, sizeof(Style));
        if (style->color != currentState->color) {
            textStyle.color = widgetCursor.currentState->flags.active || widgetCursor.currentState->flags.blinking ? currentState->activeColor : currentState->color;
        }

        uint16_t fg = widgetCursor.currentState->flags.active || widgetCursor.currentState->flags.blinking ? currentState->activeColor : currentState->color;
        uint16_t bg = widgetCursor.currentState->flags.active || widgetCursor.currentState->flags.blinking ? currentState->activeBackgroundColor : currentState->backgroundColor;

        if (horizontal) {
            // calc text position
            char valueText[64];
            int pText = 0;
            int wText = 0;
            if (!(barGraphWidget->orientation & BAR_GRAPH_DO_NOT_DISPLAY_VALUE)) {
                if (barGraphWidget->textStyle) {
                    font::Font font = styleGetFont(&textStyle);

                    currentState->textData.toText(valueText, sizeof(valueText));
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

            if ((barGraphWidget->orientation & BAR_GRAPH_ORIENTATION_MASK) == BAR_GRAPH_ORIENTATION_LEFT_RIGHT) {
                // draw bar
                if (pText > 0) {
                    display::setColor(fg);
                    display::fillRect(x, y, x + pText - 1, y + h - 1);
                }

                if (!(barGraphWidget->orientation & BAR_GRAPH_DO_NOT_DISPLAY_VALUE)) {
                    drawText(valueText, -1, x + pText, y, wText, h, &textStyle, false, false, false, nullptr, nullptr, nullptr, nullptr);
                }

                // draw background, but do not draw over line 1 and line 2
                display::setColor(bg);

                int pBackground = pText + wText;

                if (drawLine1) {
                    if (pBackground <= pLine1) {
                        if (pBackground < pLine1) {
                            display::fillRect(x + pBackground, y, x + pLine1 - 1, y + h - 1);
                        }
                        pBackground = pLine1 + 1;
                    }
                }

                if (drawLine2) {
                    if (pBackground <= pLine2) {
                        if (pBackground < pLine2) {
                            display::fillRect(x + pBackground, y, x + pLine2 - 1, y + h - 1);
                        }
                        pBackground = pLine2 + 1;
                    }
                }

                if (pBackground < d) {
                    display::fillRect(x + pBackground, y, x + d - 1, y + h - 1);
                }
            } else {
                x += w - 1;

                // draw bar
                if (pText > 0) {
                    display::setColor(fg);
                    display::fillRect(x - (pText - 1), y, x, y + h - 1);
                }

                if (!(barGraphWidget->orientation & BAR_GRAPH_DO_NOT_DISPLAY_VALUE)) {
                    drawText(valueText, -1, x - (pText + wText - 1), y, wText, h, &textStyle, false, false, false, nullptr, nullptr, nullptr, nullptr);
                }

                // draw background, but do not draw over line 1 and line 2
                display::setColor(bg);

                int pBackground = pText + wText;

                if (drawLine1) {
                    if (pBackground <= pLine1) {
                        if (pBackground < pLine1) {
                            display::fillRect(x - (pLine1 - 1), y, x - pBackground, y + h - 1);
                        }
                        pBackground = pLine1 + 1;
                    }
                }

                if (drawLine2) {
                    if (pBackground <= pLine2) {
                        if (pBackground < pLine2) {
                            display::fillRect(x - (pLine2 - 1), y, x - pBackground, y + h - 1);
                        }
                        pBackground = pLine2 + 1;
                    }
                }

                if (pBackground < d) {
                    display::fillRect(x - (d - 1), y, x - pBackground, y + h - 1);
                }
            }

            if (drawLine1) {
                drawLineInBarGraphWidget(barGraphWidget, pLine1, barGraphWidget->line1Style, x, y, w, h);
            }
            if (drawLine2) {
                drawLineInBarGraphWidget(barGraphWidget, pLine2, barGraphWidget->line2Style, x, y, w, h);
            }
        } else {
            // calc text position
            char valueText[64];
            int pText = 0;
            int hText = 0;
            if (barGraphWidget->textStyle) {
                font::Font font = styleGetFont(&textStyle);

                currentState->textData.toText(valueText, sizeof(valueText));
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

            if ((barGraphWidget->orientation & BAR_GRAPH_ORIENTATION_MASK) == BAR_GRAPH_ORIENTATION_TOP_BOTTOM) {
                // draw bar
                if (pText > 0) {
                    display::setColor(fg);
                    display::fillRect(x, y, x + w - 1, y + pText - 1);
                }

                drawText(valueText, -1, x, y + pText, w, hText, &textStyle, false, false, false, nullptr, nullptr, nullptr, nullptr);

                // draw background, but do not draw over line 1 and line 2
                display::setColor(bg);

                int pBackground = pText + hText;

                if (drawLine1) {
                    if (pBackground <= pLine1) {
                        if (pBackground < pLine1) {
                            display::fillRect(x, y + pBackground, x + w - 1, y + pLine1 - 1);
                        }
                        pBackground = pLine1 + 1;
                    }
                }

                if (drawLine2) {
                    if (pBackground <= pLine2) {
                        if (pBackground < pLine2) {
                            display::fillRect(x, y + pBackground, x + w - 1, y + pLine2 - 1);
                        }
                        pBackground = pLine2 + 1;
                    }
                }

                if (pBackground < d) {
                    display::fillRect(x, y + pBackground, x + w - 1, y + d - 1);
                }
            } else {
                y += h - 1;

                // draw bar
                if (pText > 0) {
                    display::setColor(fg);
                    display::fillRect(x, y - (pText - 1), x + w - 1, y);
                }

                drawText(valueText, -1, x, y - (pText + hText - 1), w, hText, &textStyle, false, false, false, nullptr, nullptr, nullptr, nullptr);

                // draw background, but do not draw over line 1 and line 2
                display::setColor(bg);

                int pBackground = pText + hText;

                if (drawLine1) {
                    if (pBackground <= pLine1) {
                        if (pBackground < pLine1) {
                            display::fillRect(x, y - (pLine1 - 1), x + w - 1, y - pBackground);
                        }
                        pBackground = pLine1 + 1;
                    }
                }

                if (drawLine2) {
                    if (pBackground <= pLine2) {
                        if (pBackground < pLine2) {
                            display::fillRect(x, y - (pLine2 - 1), x + w - 1, y - (pBackground));
                        }
                        pBackground = pLine2 + 1;
                    }
                }

                if (pBackground < d) {
                    display::fillRect(x, y - (d - 1), x + w - 1, y - pBackground);
                }
            }

            if (drawLine1) {
                drawLineInBarGraphWidget(barGraphWidget, pLine1, barGraphWidget->line1Style, x, y, w, h);
            }
            if (drawLine2) {
                drawLineInBarGraphWidget(barGraphWidget, pLine2, barGraphWidget->line2Style, x, y, w, h);
            }
        }
    }
};

OnTouchFunctionType BAR_GRAPH_onTouch = nullptr;

OnKeyboardFunctionType BAR_GRAPH_onKeyboard = nullptr;

} // namespace gui
} // namespace eez

#endif
