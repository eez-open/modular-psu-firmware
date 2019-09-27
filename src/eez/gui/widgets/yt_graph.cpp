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

#include <eez/gui/widgets/yt_graph.h>

#include <math.h>

#include <eez/gui/app_context.h>
#include <eez/gui/draw.h>
#include <eez/gui/gui.h>
#include <eez/modules/mcu/display.h>
#include <eez/util.h>

#include <eez/apps/psu/psu.h>
#include <eez/apps/psu/persist_conf.h>

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

void drawYTGraph(const WidgetCursor &widgetCursor, const Widget *widget, uint32_t startPosition, uint32_t endPosition,
                 int xGraphOffset, uint16_t graphWidth, uint16_t data1, float min1, float max1,
                 uint16_t data1Color, uint16_t data2, float min2, float max2, uint16_t data2Color,
                 uint16_t color, uint16_t backgroundColor) {

    uint16_t color16 = display::getColor16FromIndex(color);
    uint16_t data1Color16 = display::getColor16FromIndex(data1Color);
    uint16_t data2Color16 = display::getColor16FromIndex(data2Color);

    for (uint32_t position = startPosition; position < endPosition; ++position) {
        int x = widgetCursor.x + xGraphOffset + position % graphWidth;

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

void drawYTGraphWithScrolling(const WidgetCursor &widgetCursor, const Widget *widget,
                 uint32_t previousHistoryValuePosition, uint32_t currentHistoryValuePosition, uint32_t numPositions,
                 int xGraphOffset, uint16_t graphWidth, uint16_t data1, float min1, float max1,
                 uint16_t data1Color, uint16_t data2, float min2, float max2, uint16_t data2Color,
                 uint16_t color, uint16_t backgroundColor) {

    uint16_t color16 = display::getColor16FromIndex(color);
    uint16_t data1Color16 = display::getColor16FromIndex(data1Color);
    uint16_t data2Color16 = display::getColor16FromIndex(data2Color);

	uint32_t numPointsToDraw = currentHistoryValuePosition - previousHistoryValuePosition;
    if (numPointsToDraw > graphWidth) {
        numPointsToDraw = graphWidth;
    }

	if (numPointsToDraw < graphWidth) {
		display::bitBlt(
			widgetCursor.x + xGraphOffset + numPointsToDraw,
			widgetCursor.y,
			widgetCursor.x + xGraphOffset + graphWidth - 1,
			widgetCursor.y + widgetCursor.widget->h - 1,
			widgetCursor.x + xGraphOffset,
			widgetCursor.y);
	}

    int endX = widgetCursor.x + xGraphOffset + graphWidth;
    int startX = endX - numPointsToDraw;

    int valuePosition = previousHistoryValuePosition + 1;

    int y1Prev, y2Prev;

    y1Prev = getYValue(widgetCursor, widget, data1, min1, max1, previousHistoryValuePosition);
    y2Prev = getYValue(widgetCursor, widget, data2, min2, max2, previousHistoryValuePosition);

    for (int x = startX; x < endX; x++, valuePosition++) {
        int y1 = getYValue(widgetCursor, widget, data1, min1, max1, valuePosition);
        int y2 = getYValue(widgetCursor, widget, data2, min2, max2, valuePosition);

        display::setColor16(color16);
        display::drawVLine(x, widgetCursor.y, widget->h - 1);

        if (abs(y1Prev - y1) <= 1 && abs(y2Prev - y2) <= 1) {
            if (y1 == y2) {
                display::setColor16(valuePosition % 2 ? data2Color16 : data1Color16);
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

        y1Prev = y1; 
        y2Prev = y2;
    }
}

void YTGraphWidget_draw(const WidgetCursor &widgetCursor) {
    const Widget *widget = widgetCursor.widget;
    YTGraphWidget *ytGraphWidget = GET_WIDGET_PROPERTY(widget, specific, YTGraphWidget *);
    const Style* style = getWidgetStyle(widget);
	const Style* y1Style = getStyle(ytGraphWidget->y1Style);
	const Style* y2Style = getStyle(ytGraphWidget->y2Style);

    YTGraphWidgetState *currentState = (YTGraphWidgetState *)widgetCursor.currentState;
    YTGraphWidgetState *previousState = (YTGraphWidgetState *)widgetCursor.previousState;

    widgetCursor.currentState->size = sizeof(YTGraphWidgetState);
    widgetCursor.currentState->data = data::get(widgetCursor.cursor, widget->data);
    currentState->y2Data = data::get(widgetCursor.cursor, ytGraphWidget->y2Data);

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
    if (refresh || !widgetCursor.previousState || previousState->y2Data != currentState->y2Data) {
        char text[64];
        currentState->y2Data.toText(text, sizeof(text));
        drawText(text, -1, widgetCursor.x, widgetCursor.y + textHeight, textWidth, textHeight,
                 y2Style, nullptr, widgetCursor.currentState->flags.active, false, false, nullptr);
    }

    // draw graph
    uint16_t graphWidth = (uint16_t)(widget->w - textWidth);
    int numHistoryValues = g_appContext->getNumHistoryValues(widget->data);
    uint32_t currentHistoryValuePosition = g_appContext->getCurrentHistoryValuePosition(widgetCursor.cursor, widget->data) - 1;

    currentState->iChannel = widgetCursor.cursor.i;
    currentState->historyValuePosition = currentHistoryValuePosition; 
    currentState->ytGraphUpdateMethod = psu::persist_conf::devConf2.ytGraphUpdateMethod;

    float min1 = data::getMin(widgetCursor.cursor, widget->data).getFloat();
    float max1 = data::getLimit(widgetCursor.cursor, widget->data).getFloat();

    float min2 = data::getMin(widgetCursor.cursor, ytGraphWidget->y2Data).getFloat();
    float max2 = data::getLimit(widgetCursor.cursor, ytGraphWidget->y2Data).getFloat();

    uint32_t previousHistoryValuePosition = widgetCursor.previousState && 
        previousState->iChannel == currentState->iChannel &&
        previousState->ytGraphUpdateMethod == currentState->ytGraphUpdateMethod && 
        g_appContext->isActivePageTopPage() ? previousState->historyValuePosition : currentHistoryValuePosition - graphWidth;

    if (previousHistoryValuePosition != currentHistoryValuePosition) {
        if (psu::persist_conf::devConf2.ytGraphUpdateMethod == YT_GRAPH_UPDATE_METHOD_SCROLL) {
            drawYTGraphWithScrolling(
                widgetCursor, widget, previousHistoryValuePosition, currentHistoryValuePosition, numHistoryValues,
                textWidth, graphWidth, widget->data, min1, max1, y1Style->color,
                ytGraphWidget->y2Data, min2, max2, y2Style->color,
                widgetCursor.currentState->flags.active ? style->color : style->background_color,
                widgetCursor.currentState->flags.active ? style->background_color : style->color);
        }
        else {
            drawYTGraph(
                widgetCursor, widget, previousHistoryValuePosition, currentHistoryValuePosition,
                textWidth, graphWidth, widget->data, min1, max1, y1Style->color,
                ytGraphWidget->y2Data, min2, max2, y2Style->color,
                widgetCursor.currentState->flags.active ? style->color : style->background_color,
                widgetCursor.currentState->flags.active ? style->background_color : style->color);

            int x = widgetCursor.x + textWidth;

            // draw cursor
            display::setColor(style->color);
            display::drawVLine(x + currentHistoryValuePosition % graphWidth, widgetCursor.y, (int)widget->h - 1);

            // draw blank lines
            int x1 = x + (currentHistoryValuePosition + 1) % graphWidth;
            int x2 = x + (currentHistoryValuePosition + CONF_GUI_YT_GRAPH_BLANK_PIXELS_AFTER_CURSOR) % graphWidth;

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

#endif