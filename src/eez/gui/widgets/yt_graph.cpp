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
#include <limits.h>

#include <eez/gui/app_context.h>
#include <eez/gui/draw.h>
#include <eez/gui/gui.h>
#include <eez/modules/mcu/display.h>
#include <eez/util.h>

using namespace eez::mcu;

#define CONF_GUI_YT_GRAPH_BLANK_PIXELS_AFTER_CURSOR 10

namespace eez {
namespace gui {

////////////////////////////////////////////////////////////////////////////////

struct YTGraphDrawHelper {
    const WidgetCursor &widgetCursor;
    const Widget *widget;

    float min[2];
    float max[2];

    uint16_t color16;
    uint16_t dataColor16[2];

    uint32_t numPositions;
    uint32_t position;

    int x;

    int yPrev[2];
    int y[2];

    YTGraphDrawHelper(const WidgetCursor &widgetCursor_) : widgetCursor(widgetCursor_), widget(widgetCursor.widget) {
        min[0] = data::ytDataGetMin(widgetCursor.cursor, widget->data, 0).getFloat();
        max[0] = data::ytDataGetMax(widgetCursor.cursor, widget->data, 0).getFloat();

        min[1] = data::ytDataGetMin(widgetCursor.cursor, widget->data, 1).getFloat();
        max[1] = data::ytDataGetMax(widgetCursor.cursor, widget->data, 1).getFloat();

        const Style* y1Style = data::ytDataGetStyle(widgetCursor.cursor, widget->data, 0);
        const Style* y2Style = data::ytDataGetStyle(widgetCursor.cursor, widget->data, 1);
        dataColor16[0] = display::getColor16FromIndex(y1Style->color);
        dataColor16[1] = display::getColor16FromIndex(y2Style->color);

    }

    int getYValue(int valueIndex, uint32_t position) {
        if (position >= numPositions) {
            return INT_MIN;
        }

        float value = data::ytDataGetValue(widgetCursor.cursor, widget->data, position, valueIndex).getFloat();

        if (isNaN(value)) {
            return INT_MIN;
        }

        int y = (int)round((widget->h - 1) * (value - min[valueIndex]) / (max[valueIndex] - min[valueIndex]));

        if (y < 0 || y >= widget->h) {
            return INT_MIN;
        }

        return widget->h - 1 - y;
    }

    void drawValue(int valueIndex) {
        if (y[valueIndex] == INT_MIN) {
            return;
        }

        display::setColor16(dataColor16[valueIndex]);

        if (yPrev[valueIndex] == INT_MIN || abs(yPrev[valueIndex] - y[valueIndex]) <= 1) {
            display::drawPixel(x, widgetCursor.y + y[valueIndex]);
        } else {
            if (yPrev[valueIndex] < y[valueIndex]) {
                display::drawVLine(x, widgetCursor.y + yPrev[valueIndex] + 1, y[valueIndex] - yPrev[valueIndex] - 1);
            } else {
                display::drawVLine(x, widgetCursor.y + y[valueIndex], yPrev[valueIndex] - y[valueIndex] - 1);
            }
        }
    }

    void drawStep() {
        // clear vertical line
        display::setColor16(color16);
        display::drawVLine(x, widgetCursor.y, widget->h - 1);

        if (y[0] != INT_MIN && y[1] != INT_MIN && abs(yPrev[0] - y[0]) <= 1 && abs(yPrev[1] - y[1]) <= 1 && y[0] == y[1]) {
            display::setColor16(position % 2 ? dataColor16[1] : dataColor16[0]);
            display::drawPixel(x, widgetCursor.y + y[0]);
        } else {
            drawValue(0);
            drawValue(1);
        }
    }

    void drawScanLine(uint32_t startPosition, uint32_t endPosition, uint16_t graphWidth) {
        numPositions = endPosition;

        for (position = startPosition; position < endPosition; ++position) {
            x = widgetCursor.x + position % graphWidth;

            y[0] = getYValue(0, position);
            yPrev[0] = getYValue(0, position == 0 ? position : position - 1);

            y[1] = getYValue(1, position);
            yPrev[1] = getYValue(1, position == 0 ? position : position - 1);

            drawStep();
        }
    }

    void drawScrolling(uint32_t previousHistoryValuePosition, uint32_t currentHistoryValuePosition, uint32_t numPositions_, uint16_t graphWidth) {
        uint32_t numPointsToDraw = currentHistoryValuePosition - previousHistoryValuePosition;
        if (numPointsToDraw > graphWidth) {
            numPointsToDraw = graphWidth;
        }

        if (numPointsToDraw < graphWidth) {
            display::bitBlt(
                widgetCursor.x + numPointsToDraw,
                widgetCursor.y,
                widgetCursor.x + graphWidth - 1,
                widgetCursor.y + widgetCursor.widget->h - 1,
                widgetCursor.x,
                widgetCursor.y);
        }

        int endX = widgetCursor.x + graphWidth;
        int startX = endX - numPointsToDraw;

        position = previousHistoryValuePosition + 1;

        numPositions = position + numPointsToDraw;

        yPrev[0] = getYValue(0, previousHistoryValuePosition);
        yPrev[1] = getYValue(1, previousHistoryValuePosition);

        for (x = startX; x < endX; x++, position++) {
            y[0] = getYValue(0, position);
            y[1] = getYValue(1, position);

            drawStep();

            yPrev[0] = y[0];
            yPrev[1] = y[1];
        }
    }

    void drawStatic(uint32_t previousHistoryValuePosition, uint32_t currentHistoryValuePosition, uint32_t numPositions_, uint16_t graphWidth) {
        numPositions = numPositions_;

        int startX = widgetCursor.x;
        int endX = startX + graphWidth;

        position = currentHistoryValuePosition;

        yPrev[0] = getYValue(0, currentHistoryValuePosition > 0 ? currentHistoryValuePosition - 1 : 0);
        yPrev[1] = getYValue(1, currentHistoryValuePosition > 0 ? currentHistoryValuePosition - 1 : 0);

        for (x = startX; x < endX; x++, position++) {
            y[0] = getYValue(0, position);
            y[1] = getYValue(1, position);

            drawStep();

            yPrev[0] = y[0];
            yPrev[1] = y[1];
        }
    }
};

////////////////////////////////////////////////////////////////////////////////

struct YTGraphStaticDrawHelper {
    const WidgetCursor &widgetCursor;
    const Widget *widget;

    Style* style;

    uint16_t dataColor16;

    uint32_t numPositions;
    uint32_t position;

    float offset;
    float scale;

    int valueIndex;

    int x;

    int yPrev;
    int y;

    uint32_t cursorPosition;

    YTGraphStaticDrawHelper(const WidgetCursor &widgetCursor_) : widgetCursor(widgetCursor_), widget(widgetCursor.widget) {
    }

    int getYValue(uint32_t position) {
        if (position >= numPositions) {
            return INT_MIN;
        }

        float value = data::ytDataGetValue(widgetCursor.cursor, widget->data, position, valueIndex).getFloat();

        if (isNaN(value)) {
            return INT_MIN;
        }

        return widget->h - 1 - (int)floor(widget->h / 2.0f + (value + offset) * scale);
    }

    void drawValue() {
        if (y == INT_MIN) {
            return;
        }

        display::setColor16(dataColor16);

        int yFrom;
        int yTo;

        if (yPrev == INT_MIN) {
            yFrom = y;
            yTo = y;
        } else {
            if (yPrev < y) {
                yFrom = yPrev + 1;
                yTo = y;
            } else if (y < yPrev) {
                yFrom = y;
                yTo = yPrev - 1;
            } else {
                yFrom = y;
                yTo = y;
            }
        }

        if ((yFrom < 0 && yTo < 0) || (yFrom >= widget->h && yTo >= widget->h)) {
            return;
        }

        if (yFrom < 0) {
            yFrom = 0;
        }

        if (yTo >= widget->h) {
            yTo = widget->h - 1;
        }

        if (yFrom == yTo) {
            display::drawPixel(x, widgetCursor.y + y);
        } else {
            display::drawVLine(x, widgetCursor.y + yFrom, yTo - yFrom);
        }
    }

    void drawStatic(uint32_t previousHistoryValuePosition, uint32_t currentHistoryValuePosition, uint32_t numPositions_, uint16_t graphWidth) {
        // draw background
        const Style* style = getStyle(widget->style);
        display::setColor(style->background_color);
        display::fillRect(widgetCursor.x, widgetCursor.y, widgetCursor.x + (int)widget->w - 1, widgetCursor.y + (int)widget->h - 1);

        numPositions = numPositions_;

        int startX = widgetCursor.x;
        int endX = startX + graphWidth;

        int horzDivisions = data::ytDataGetHorzDivisions(widgetCursor.cursor, widget->data);
        int vertDivisions = data::ytDataGetVertDivisions(widgetCursor.cursor, widget->data);

        // draw grid
        display::setColor(style->border_color);
        for (int x = 1; x < horzDivisions; x++) {
            display::drawVLine(widgetCursor.x + x * widget->w / horzDivisions, widgetCursor.y, widget->h - 1);
        }
        for (int y = 1; y < vertDivisions; y++) {
            display::drawHLine(widgetCursor.x, widgetCursor.y + y * widget->h / vertDivisions, widget->w - 1);
        }

        // draw charts
        YTGraphWidgetState *currentState = (YTGraphWidgetState *)widgetCursor.currentState;

        int numValues = data::ytDataGetNumValues(widgetCursor.cursor, widget->data);
        for (valueIndex = 0; valueIndex < numValues; valueIndex++) {

            position = currentHistoryValuePosition;

            scale = (widget->h - 1)  / currentState->valuePerDiv[valueIndex] / vertDivisions;
            offset = currentState->valueOffset[valueIndex];

            const Style* style = data::ytDataGetStyle(widgetCursor.cursor, widget->data, valueIndex);
            dataColor16 = display::getColor16FromIndex(style->color);

            yPrev = getYValue(previousHistoryValuePosition);

            for (x = startX; x < endX; x++, position++) {
                y = getYValue(position);
                drawValue();
                yPrev = y;
            }
        }

        // draw cursor
        if (data::ytDataIsCursorVisible(widgetCursor.cursor, widgetCursor.widget->data)) {
            display::setColor(style->color);
            display::drawVLine(startX + cursorPosition - currentHistoryValuePosition, widgetCursor.y, widget->h - 1);

            int timeTextWidth = 75;
            int timeTextHeight = 22;
            const int PADDING = 0;
            int xTimeText = widgetCursor.x + cursorPosition - currentHistoryValuePosition - timeTextWidth / 2;
            if (xTimeText < widgetCursor.x + PADDING) {
                xTimeText = widgetCursor.x + PADDING;
            } else if (xTimeText + timeTextWidth > widgetCursor.x + widgetCursor.widget->w - PADDING) {
                xTimeText = widgetCursor.x + widgetCursor.widget->w - PADDING - timeTextWidth;
            }
            int yTimeText = widgetCursor.y + widgetCursor.widget->h - timeTextHeight - PADDING;

            char text[64];
            data::ytDataGetCursorTime(widgetCursor.cursor, widgetCursor.widget->data).toText(text, sizeof(text));
            drawText(text, -1, xTimeText, yTimeText, timeTextWidth, timeTextHeight, style, false, false, false, nullptr, nullptr);
        }
    }
};

////////////////////////////////////////////////////////////////////////////////

void YTGraphWidget_draw(const WidgetCursor &widgetCursor) {
    const Widget *widget = widgetCursor.widget;

    YTGraphWidgetState *currentState = (YTGraphWidgetState *)widgetCursor.currentState;
    YTGraphWidgetState *previousState = (YTGraphWidgetState *)widgetCursor.previousState;

    widgetCursor.currentState->size = sizeof(YTGraphWidgetState);
    widgetCursor.currentState->data = data::get(widgetCursor.cursor, widget->data);

    currentState->iChannel = widgetCursor.cursor.i;
    currentState->numHistoryValues = data::ytDataGetSize(widgetCursor.cursor, widget->data);
    currentState->historyValuePosition = data::ytDataGetPosition(widgetCursor.cursor, widget->data);
    currentState->ytGraphUpdateMethod = data::ytDataGetGraphUpdateMethod(widgetCursor.cursor, widget->data);
    currentState->cursorPosition = currentState->historyValuePosition + data::ytDataGetCursorOffset(widgetCursor.cursor, widget->data);

    int numValues = data::ytDataGetNumValues(widgetCursor.cursor, widget->data);
    bool transformChanged = false;
    for (int valueIndex = 0; valueIndex < numValues; valueIndex++) {
        currentState->valuePerDiv[valueIndex] = data::ytDataGetPerDiv(widgetCursor.cursor, widget->data, valueIndex);
        currentState->valueOffset[valueIndex] = data::ytDataGetOffset(widgetCursor.cursor, widget->data, valueIndex);
        if (previousState && (previousState->valuePerDiv[valueIndex] != currentState->valuePerDiv[valueIndex] || previousState->valueOffset[valueIndex] != currentState->valueOffset[valueIndex])) {
            transformChanged = true;
        }
    }

    uint16_t graphWidth = (uint16_t)widget->w;

    uint32_t previousHistoryValuePosition = widgetCursor.previousState && 
        previousState->iChannel == currentState->iChannel &&
        previousState->ytGraphUpdateMethod == currentState->ytGraphUpdateMethod && 
        g_appContext->isActivePageTopPage() ? previousState->historyValuePosition : currentState->historyValuePosition - graphWidth;

    bool refreshBackground = !widgetCursor.previousState;

    if (refreshBackground || transformChanged || previousHistoryValuePosition != currentState->historyValuePosition || (!previousState || previousState->numHistoryValues != currentState->numHistoryValues || previousState->cursorPosition != currentState->cursorPosition)) {
        if (currentState->ytGraphUpdateMethod == YT_GRAPH_UPDATE_METHOD_STATIC) {
            YTGraphStaticDrawHelper drawHelper(widgetCursor);

            drawHelper.cursorPosition = currentState->cursorPosition;
            drawHelper.drawStatic(previousHistoryValuePosition, currentState->historyValuePosition, currentState->numHistoryValues, graphWidth);
        } else {
            const Style* style = getStyle(widget->style);

            if (refreshBackground) {
                display::setColor(style->background_color);
                display::fillRect(widgetCursor.x, widgetCursor.y, widgetCursor.x + (int)widget->w - 1, widgetCursor.y + (int)widget->h - 1);
            }

            YTGraphDrawHelper drawHelper(widgetCursor);
            drawHelper.color16 = display::getColor16FromIndex(widgetCursor.currentState->flags.active ? style->color : style->background_color);
            if (currentState->ytGraphUpdateMethod == YT_GRAPH_UPDATE_METHOD_SCAN_LINE) {
                drawHelper.drawScanLine(previousHistoryValuePosition, currentState->historyValuePosition, graphWidth);

                int x = widgetCursor.x;

                // draw cursor
                display::setColor(style->color);
                display::drawVLine(x + currentState->historyValuePosition % graphWidth, widgetCursor.y, (int)widget->h - 1);

                // draw blank lines
                int x1 = x + (currentState->historyValuePosition + 1) % graphWidth;
                int x2 = x + (currentState->historyValuePosition + CONF_GUI_YT_GRAPH_BLANK_PIXELS_AFTER_CURSOR) % graphWidth;

                display::setColor(style->background_color);
                if (x1 < x2) {
                    display::fillRect(x1, widgetCursor.y, x2, widgetCursor.y + (int)widget->h - 1);
                } else {
                    display::fillRect(x1, widgetCursor.y, x + graphWidth - 1, widgetCursor.y + (int)widget->h - 1);
                    display::fillRect(x, widgetCursor.y, x2, widgetCursor.y + (int)widget->h - 1);
                }
            } else if (currentState->ytGraphUpdateMethod == YT_GRAPH_UPDATE_METHOD_SCROLL) {
                drawHelper.drawScrolling(previousHistoryValuePosition, currentState->historyValuePosition, currentState->numHistoryValues, graphWidth);
            }
        }
    }
}

void YTGraphWidget_onTouch(const WidgetCursor &widgetCursor, Event &touchEvent) {
    if (data::ytDataGetGraphUpdateMethod(widgetCursor.cursor, widgetCursor.widget->data) == YT_GRAPH_UPDATE_METHOD_STATIC) {
        if (touchEvent.type == EVENT_TYPE_TOUCH_DOWN || touchEvent.type == EVENT_TYPE_TOUCH_MOVE) {
            int32_t cursorOffset = touchEvent.x - widgetCursor.x;
            if (cursorOffset < 0) {
                cursorOffset = 0;
            }
            if (cursorOffset >= widgetCursor.widget->w) {
                cursorOffset = widgetCursor.widget->w - 1;
            }
            data::ytDataSetCursorOffset(widgetCursor.cursor, widgetCursor.widget->data, cursorOffset);
        }
    } else {
        if (touchEvent.type == EVENT_TYPE_TOUCH_DOWN) {
            if (widgetCursor.widget->action) {
                executeAction(widgetCursor.widget->action);
            }
        }
    }
}

} // namespace gui
} // namespace eez

#endif
