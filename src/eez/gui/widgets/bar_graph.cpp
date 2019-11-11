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

#include <eez/gui/widgets/bar_graph.h>

#include <math.h>
#include <memory.h>

#include <eez/gui/assets.h>
#include <eez/gui/document.h>
#include <eez/gui/draw.h>
#include <eez/gui/gui.h>
#include <eez/gui/widget.h>
#include <eez/modules/mcu/display.h>
#include <eez/util.h>

using namespace eez::mcu;

namespace eez {
namespace gui {

int calcValuePosInBarGraphWidget(data::Value &value, float min, float max, int d) {
    int p = (int)roundf((value.getFloat() - min) * d / (max - min));

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
    if (barGraphWidget->orientation == BAR_GRAPH_ORIENTATION_LEFT_RIGHT) {
        display::drawVLine(x + p, y, h - 1);
    } else if (barGraphWidget->orientation == BAR_GRAPH_ORIENTATION_RIGHT_LEFT) {
        display::drawVLine(x - p, y, h - 1);
    } else if (barGraphWidget->orientation == BAR_GRAPH_ORIENTATION_TOP_BOTTOM) {
        display::drawHLine(x, y + p, w - 1);
    } else {
        display::drawHLine(x, y - p, w - 1);
    }
}

void BarGraphWidget_draw(const WidgetCursor &widgetCursor) {
    bool fullScale = true;

    const Widget *widget = widgetCursor.widget;
    const BarGraphWidget *barGraphWidget = GET_WIDGET_PROPERTY(widget, specific, const BarGraphWidget *);

    widgetCursor.currentState->size = sizeof(BarGraphWidgetState);

    widgetCursor.currentState->data = data::get(widgetCursor.cursor, widget->data);
    
    auto currentState = (BarGraphWidgetState *)widgetCursor.currentState;
    auto previousState = (BarGraphWidgetState *)widgetCursor.previousState;

    currentState->line1Data = data::get(widgetCursor.cursor, barGraphWidget->line1Data);
    currentState->line2Data = data::get(widgetCursor.cursor, barGraphWidget->line2Data);

    bool refresh =
        !widgetCursor.previousState ||
        widgetCursor.previousState->flags.active != widgetCursor.currentState->flags.active ||
        widgetCursor.previousState->data != widgetCursor.currentState->data ||
        previousState->line1Data != currentState->line1Data ||
        previousState->line2Data != currentState->line2Data;

    if (refresh) {
        int x = widgetCursor.x;
        int y = widgetCursor.y;
        const int w = widget->w;
        const int h = widget->h;

        float min = data::getMin(widgetCursor.cursor, widget->data).getFloat();
        float max = fullScale ? currentState->line2Data.getFloat() : data::getMax(widgetCursor.cursor, widget->data).getFloat();

        bool horizontal = barGraphWidget->orientation == BAR_GRAPH_ORIENTATION_LEFT_RIGHT ||
                          barGraphWidget->orientation == BAR_GRAPH_ORIENTATION_RIGHT_LEFT;

        int d = horizontal ? w : h;

        // calc bar  position (monitored value)
        int pValue = calcValuePosInBarGraphWidget(widgetCursor.currentState->data, min, max, d);

        // calc line 1 position (set value)
        int pLine1 = calcValuePosInBarGraphWidget(currentState->line1Data, min, max, d);

        int pLine2;
        if (!fullScale) {
            // calc line 2 position (limit value)
            pLine2 = calcValuePosInBarGraphWidget(currentState->line2Data, min, max, d);

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

        const Style* style = getStyle(widget->style);

        Style textStyle;

        uint16_t inverseColor;
        if (barGraphWidget->textStyle) {
            const Style *textStyleInner = getStyle(barGraphWidget->textStyle);
            memcpy(&textStyle, textStyleInner, sizeof(Style));

            inverseColor = textStyle.background_color;
        } else {
            inverseColor = style->background_color;
        }

        uint16_t fg = widgetCursor.currentState->flags.active ? inverseColor : style->color;
        uint16_t bg =
            widgetCursor.currentState->flags.active ? inverseColor : style->background_color;

        if (horizontal) {
            // calc text position
            char valueText[64];
            int pText = 0;
            int wText = 0;
            if (barGraphWidget->textStyle) {
                font::Font font = styleGetFont(&textStyle);

                widgetCursor.currentState->data.toText(valueText, sizeof(valueText));
                wText = display::measureStr(valueText, -1, font, w);

                int padding = textStyle.padding_left;
                wText += padding;

                if (pValue + wText <= d) {
                    textStyle.background_color = bg;
                    pText = pValue;
                } else {
                    textStyle.background_color = fg;
                    wText += padding;
                    pText = pValue - wText;
                }
            }

            if (barGraphWidget->orientation == BAR_GRAPH_ORIENTATION_LEFT_RIGHT) {
                // draw bar
                if (pText > 0) {
                    display::setColor(fg);
                    display::fillRect(x, y, x + pText - 1, y + h - 1);
                }

                drawText(valueText, -1, x + pText, y, wText, h, &textStyle, false, false, false, nullptr, nullptr, nullptr, nullptr);

                // draw background, but do not draw over line 1 and line 2
                display::setColor(bg);

                int pBackground = pText + wText;

                if (pBackground <= pLine1) {
                    if (pBackground < pLine1) {
                        display::fillRect(x + pBackground, y, x + pLine1 - 1, y + h - 1);
                    }
                    pBackground = pLine1 + 1;
                }

                if (!fullScale) {
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

                drawText(valueText, -1, x - (pText + wText - 1), y, wText, h, &textStyle, false, false, false, nullptr, nullptr, nullptr, nullptr);

                // draw background, but do not draw over line 1 and line 2
                display::setColor(bg);

                int pBackground = pText + wText;

                if (pBackground <= pLine1) {
                    if (pBackground < pLine1) {
                        display::fillRect(x - (pLine1 - 1), y, x - pBackground, y + h - 1);
                    }
                    pBackground = pLine1 + 1;
                }

                if (!fullScale) {
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

            drawLineInBarGraphWidget(barGraphWidget, pLine1, barGraphWidget->line1Style, x, y, w,
                                     h);
            if (!fullScale) {
                drawLineInBarGraphWidget(barGraphWidget, pLine2, barGraphWidget->line2Style, x, y,
                                         w, h);
            }
        } else {
            // calc text position
            char valueText[64];
            int pText = 0;
            int hText = 0;
            if (barGraphWidget->textStyle) {
                font::Font font = styleGetFont(&textStyle);

                widgetCursor.currentState->data.toText(valueText, sizeof(valueText));
                hText = font.getHeight();

                int padding = textStyle.padding_top;
                hText += padding;

                if (pValue + hText <= d) {
                    textStyle.background_color = bg;
                    pText = pValue;
                } else {
                    textStyle.background_color = fg;
                    hText += padding;
                    pText = pValue - hText;
                }
            }

            if (barGraphWidget->orientation == BAR_GRAPH_ORIENTATION_TOP_BOTTOM) {
                // draw bar
                if (pText > 0) {
                    display::setColor(fg);
                    display::fillRect(x, y, x + w - 1, y + pText - 1);
                }

                drawText(valueText, -1, x, y + pText, w, hText, &textStyle, false, false, false, nullptr, nullptr, nullptr, nullptr);

                // draw background, but do not draw over line 1 and line 2
                display::setColor(bg);

                int pBackground = pText + hText;

                if (pBackground <= pLine1) {
                    if (pBackground < pLine1) {
                        display::fillRect(x, y + pBackground, x + w - 1, y + pLine1 - 1);
                    }
                    pBackground = pLine1 + 1;
                }

                if (!fullScale) {
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

                if (pBackground <= pLine1) {
                    if (pBackground < pLine1) {
                        display::fillRect(x, y - (pLine1 - 1), x + w - 1, y - pBackground);
                    }
                    pBackground = pLine1 + 1;
                }

                if (!fullScale) {
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

            drawLineInBarGraphWidget(barGraphWidget, pLine1, barGraphWidget->line1Style, x, y, w,
                                     h);
            if (!fullScale) {
                drawLineInBarGraphWidget(barGraphWidget, pLine2, barGraphWidget->line2Style, x, y,
                                         w, h);
            }
        }
    }
}

} // namespace gui
} // namespace eez

#endif