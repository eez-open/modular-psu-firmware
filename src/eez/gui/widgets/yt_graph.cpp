/*
 * EEZ Generic Firmware
 * Copyright (C) 2018-present, Envox d.o.o.
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

#include <eez/gui/widgets/yt_graph.h>

#include <math.h>

#include <eez/gui/app_context.h>
#include <eez/gui/draw.h>
#include <eez/gui/gui.h>
#include <eez/modules/mcu/display.h>
#include <eez/util.h>

using namespace eez::mcu;

#define CONF_GUI_YT_GRAPH_BLANK_PIXELS_AFTER_CURSOR 10

namespace eez {
namespace gui {

int getYValue(const WidgetCursor &widgetCursor, const Widget *widget, uint16_t data, float min,
              float max, int position) {
    float value = g_appContext->getHistoryValue(widgetCursor.cursor, data, position).getFloat();
    int y = (int)floor(widget->h * (value - min) / (max - min));
    if (y < 0)
        y = 0;
    if (y >= widget->h)
        y = widget->h - 1;
    return widget->h - 1 - y;
}

void drawYTGraph(const WidgetCursor &widgetCursor, const Widget *widget, int startPosition,
                 int endPosition, int numPositions, int currentHistoryValuePosition,
                 int xGraphOffset, int graphWidth, uint16_t data1, float min1, float max1,
                 uint16_t data1Color, uint16_t data2, float min2, float max2, uint16_t data2Color,
                 uint16_t color, uint16_t backgroundColor) {

    uint16_t color16 = display::getColor16FromIndex(color);
    uint16_t data1Color16 = display::getColor16FromIndex(data1Color);
    uint16_t data2Color16 = display::getColor16FromIndex(data2Color);

    for (int position = startPosition; position < endPosition; ++position) {
        if (position < graphWidth) {
            int x = widgetCursor.x + xGraphOffset + position;

            display::setColor16(color16);
            display::drawVLine(x, widgetCursor.y, widget->h - 1);

            int y1 = getYValue(widgetCursor, widget, data1, min1, max1, position);
            int y1Prev = getYValue(widgetCursor, widget, data1, min1, max1,
                                   position == 0 ? position : position - 1);

            int y2 = getYValue(widgetCursor, widget, data2, min2, max2, position);
            int y2Prev = getYValue(widgetCursor, widget, data2, min2, max2,
                                   position == 0 ? position : position - 1);

            if (abs(y1Prev - y1) <= 1 && abs(y2Prev - y2) <= 1) {
                if (y1 == y2) {
                    display::setColor16(position % 2 ? data2Color16 : data1Color16);
                    display::drawPixel(x, widgetCursor.y + y1);
                } else {
                    display::setColor16(data1Color16);
                    display::drawPixel(x, widgetCursor.y + y1);

                    display::setColor16(data2Color16);
                    display::drawPixel(x, widgetCursor.y + y2);
                }
            } else {
                display::setColor16(data1Color16);
                if (abs(y1Prev - y1) <= 1) {
                    display::drawPixel(x, widgetCursor.y + y1);
                } else {
                    if (y1Prev < y1) {
                        display::drawVLine(x, widgetCursor.y + y1Prev + 1, y1 - y1Prev - 1);
                    } else {
                        display::drawVLine(x, widgetCursor.y + y1, y1Prev - y1 - 1);
                    }
                }

                display::setColor16(data2Color16);
                if (abs(y2Prev - y2) <= 1) {
                    display::drawPixel(x, widgetCursor.y + y2);
                } else {
                    if (y2Prev < y2) {
                        display::drawVLine(x, widgetCursor.y + y2Prev + 1, y2 - y2Prev - 1);
                    } else {
                        display::drawVLine(x, widgetCursor.y + y2, y2Prev - y2 - 1);
                    }
                }
            }
        }
    }
}

void drawYTGraphWithScrolling(const WidgetCursor &widgetCursor, const Widget *widget,
                 int previousHistoryValuePosition, int currentHistoryValuePosition, int numPositions,
                 int xGraphOffset, int graphWidth, uint16_t data1, float min1, float max1,
                 uint16_t data1Color, uint16_t data2, float min2, float max2, uint16_t data2Color,
                 uint16_t color, uint16_t backgroundColor) {

    uint16_t color16 = display::getColor16FromIndex(color);
    uint16_t data1Color16 = display::getColor16FromIndex(data1Color);
    uint16_t data2Color16 = display::getColor16FromIndex(data2Color);

    int numPointsToDraw = currentHistoryValuePosition - previousHistoryValuePosition;
    if (numPointsToDraw < 0) {
        numPointsToDraw += numPositions;
    }

    if (numPointsToDraw < numPositions) {
        display::bitBlt(
            widgetCursor.x + xGraphOffset + numPointsToDraw, 
            widgetCursor.y, 
            widgetCursor.x + xGraphOffset + numPositions - 1,
            widgetCursor.y + widgetCursor.widget->h - 1, 
            widgetCursor.x + xGraphOffset,
            widgetCursor.y);
    }

    int endX = widgetCursor.x + xGraphOffset + numPositions;
    int startX = endX - numPointsToDraw;

    int previousValuePositionLoop = previousHistoryValuePosition % numPositions;
    int valuePositionLoop = (previousValuePositionLoop + 1) % numPositions;
    for (int x = startX; x < endX; x++) {
        display::setColor16(color16);
        display::drawVLine(x, widgetCursor.y, widget->h - 1);

        int y1 = getYValue(widgetCursor, widget, data1, min1, max1, valuePositionLoop);
        int y1Prev = getYValue(widgetCursor, widget, data1, min1, max1, previousValuePositionLoop);

        int y2 = getYValue(widgetCursor, widget, data2, min2, max2, valuePositionLoop);
        int y2Prev = getYValue(widgetCursor, widget, data2, min2, max2, previousValuePositionLoop);

        if (abs(y1Prev - y1) <= 1 && abs(y2Prev - y2) <= 1) {
            if (y1 == y2) {
                display::setColor16(valuePositionLoop % 2 ? data2Color16 : data1Color16);
                display::drawPixel(x, widgetCursor.y + y1);
            } else {
                display::setColor16(data1Color16);
                display::drawPixel(x, widgetCursor.y + y1);

                display::setColor16(data2Color16);
                display::drawPixel(x, widgetCursor.y + y2);
            }
        } else {
            display::setColor16(data1Color16);
            if (abs(y1Prev - y1) <= 1) {
                display::drawPixel(x, widgetCursor.y + y1);
            } else {
                if (y1Prev < y1) {
                    display::drawVLine(x, widgetCursor.y + y1Prev + 1, y1 - y1Prev - 1);
                } else {
                    display::drawVLine(x, widgetCursor.y + y1, y1Prev - y1 - 1);
                }
            }

            display::setColor16(data2Color16);
            if (abs(y2Prev - y2) <= 1) {
                display::drawPixel(x, widgetCursor.y + y2);
            } else {
                if (y2Prev < y2) {
                    display::drawVLine(x, widgetCursor.y + y2Prev + 1, y2 - y2Prev - 1);
                } else {
                    display::drawVLine(x, widgetCursor.y + y2, y2Prev - y2 - 1);
                }
            }
        }

        previousValuePositionLoop = valuePositionLoop;
        valuePositionLoop = (valuePositionLoop + 1) % numPositions;
    }
}

void YTGraphWidget_draw(const WidgetCursor &widgetCursor) {
    const Widget *widget = widgetCursor.widget;
    YTGraphWidget *ytGraphWidget = (YTGraphWidget *)widget->specific;
    const Style* style = getWidgetStyle(widget);
	const Style* y1Style = getStyle(ytGraphWidget->y1Style);
	const Style* y2Style = getStyle(ytGraphWidget->y2Style);

    widgetCursor.currentState->size = sizeof(YTGraphWidgetState);
    widgetCursor.currentState->data = data::get(widgetCursor.cursor, widget->data);
    ((YTGraphWidgetState *)widgetCursor.currentState)->y2Data =
        data::get(widgetCursor.cursor, ytGraphWidget->y2Data);

    bool refresh = !widgetCursor.previousState || 
        widgetCursor.previousState->flags.active != widgetCursor.currentState->flags.active;

    if (refresh) {
        // draw background
        uint16_t color = widgetCursor.currentState->flags.active ? style->color : style->background_color;
        display::setColor(color);
        display::fillRect(
            widgetCursor.x, 
            widgetCursor.y, 
            widgetCursor.x + (int)widget->w - 1,
            widgetCursor.y + (int)widget->h - 1);
    }

    int textWidth = 62; // TODO this is hardcoded value
    int textHeight = widget->h / 2;

    // draw first value text
    if (refresh || !widgetCursor.previousState || widgetCursor.previousState->data != widgetCursor.currentState->data) {
        char text[64];
        widgetCursor.currentState->data.toText(text, sizeof(text));
        drawText(text, -1, widgetCursor.x, widgetCursor.y, textWidth, textHeight, y1Style, nullptr,
                 widgetCursor.currentState->flags.active, false, false, nullptr);
    }

    // draw second value text
    if (refresh || !widgetCursor.previousState || ((YTGraphWidgetState *)widgetCursor.previousState)->y2Data != ((YTGraphWidgetState *)widgetCursor.currentState)->y2Data) {
        char text[64];
        ((YTGraphWidgetState *)widgetCursor.currentState)->y2Data.toText(text, sizeof(text));
        drawText(text, -1, widgetCursor.x, widgetCursor.y + textHeight, textWidth, textHeight,
                 y2Style, nullptr, widgetCursor.currentState->flags.active, false, false, nullptr);
    }

    // draw graph
    int graphWidth = widget->w - textWidth;
    int numHistoryValues = g_appContext->getNumHistoryValues(widget->data);
    int currentHistoryValuePosition =
            g_appContext->getCurrentHistoryValuePosition(widgetCursor.cursor, widget->data) - 1;
    if (currentHistoryValuePosition < 0) {
        currentHistoryValuePosition = numHistoryValues - 1;
    }

    ((YTGraphWidgetState *)widgetCursor.currentState)->position = currentHistoryValuePosition;

    float min1 = data::getMin(widgetCursor.cursor, widget->data).getFloat();
    float max1 = data::getLimit(widgetCursor.cursor, widget->data).getFloat();

    float min2 = data::getMin(widgetCursor.cursor, ytGraphWidget->y2Data).getFloat();
    float max2 = data::getLimit(widgetCursor.cursor, ytGraphWidget->y2Data).getFloat();

    int iChannel = widgetCursor.cursor.i >= 0 ? widgetCursor.cursor.i : 0;

    int previousHistoryValuePosition = widgetCursor.previousState ?
        ((YTGraphWidgetState *)widgetCursor.previousState)->position : (currentHistoryValuePosition + 1) % numHistoryValues;

    if (previousHistoryValuePosition != currentHistoryValuePosition) {
        if (1) {
            // a new way of drawing yt graph using scrolling
            drawYTGraphWithScrolling(
                widgetCursor, widget, previousHistoryValuePosition, currentHistoryValuePosition, numHistoryValues,
                textWidth, graphWidth, widget->data, min1, max1, y1Style->color,
                ytGraphWidget->y2Data, min2, max2, y2Style->color,
                widgetCursor.currentState->flags.active ? style->color : style->background_color,
                widgetCursor.currentState->flags.active ? style->background_color : style->color);
        }
        else {
            // an old way of drawing yt graph which draws graph from left to right in a loop with moving vertical line

            int startPosition;
            int endPosition;
            if (refresh || iChannel >= 10) {
                startPosition = 0;
                endPosition = numHistoryValues;
            }
            else {
                startPosition = previousHistoryValuePosition;
                if (startPosition == currentHistoryValuePosition) {
                    return;
                }
                endPosition = currentHistoryValuePosition;
            }

            if (startPosition < endPosition) {
                drawYTGraph(
                    widgetCursor, widget, startPosition, endPosition, currentHistoryValuePosition,
                    numHistoryValues, textWidth, graphWidth, widget->data, min1, max1, y1Style->color,
                    ytGraphWidget->y2Data, min2, max2, y2Style->color,
                    widgetCursor.currentState->flags.active ? style->color : style->background_color,
                    widgetCursor.currentState->flags.active ? style->background_color : style->color);
            }
            else {
                drawYTGraph(
                    widgetCursor, widget, startPosition, numHistoryValues, currentHistoryValuePosition,
                    numHistoryValues, textWidth, graphWidth, widget->data, min1, max1, y1Style->color,
                    ytGraphWidget->y2Data, min2, max2, y2Style->color,
                    widgetCursor.currentState->flags.active ? style->color : style->background_color,
                    widgetCursor.currentState->flags.active ? style->background_color : style->color);

                drawYTGraph(
                    widgetCursor, widget, 0, endPosition, currentHistoryValuePosition, numHistoryValues,
                    textWidth, graphWidth, widget->data, min1, max1, y1Style->color, ytGraphWidget->y2Data,
                    min2, max2, y2Style->color,
                    widgetCursor.currentState->flags.active ? style->color : style->background_color,
                    widgetCursor.currentState->flags.active ? style->background_color : style->color);
            }

            int x = widgetCursor.x + textWidth;

            // draw cursor
            display::setColor(style->color);
            display::drawVLine(x + currentHistoryValuePosition, widgetCursor.y, (int)widget->h - 1);

            // draw blank lines
            int x1 = x + (currentHistoryValuePosition + 1) % numHistoryValues;
            int x2 = x + (currentHistoryValuePosition + CONF_GUI_YT_GRAPH_BLANK_PIXELS_AFTER_CURSOR) %
                numHistoryValues;

            display::setColor(style->background_color);
            if (x1 < x2) {
                display::fillRect(x1, widgetCursor.y, x2, widgetCursor.y + (int)widget->h - 1);
            }
            else {
                display::fillRect(x1, widgetCursor.y, x + graphWidth - 1, widgetCursor.y + (int)widget->h - 1);
                display::fillRect(x, widgetCursor.y, x2, widgetCursor.y + (int)widget->h - 1);
            }
        }
    }
}

} // namespace gui
} // namespace eez
