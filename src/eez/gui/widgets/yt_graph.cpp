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
#include <limits.h>

#include <eez/util.h>

#include <eez/gui/gui.h>
#include <eez/gui/widgets/yt_graph.h>

using namespace eez::mcu;

#define CONF_GUI_YT_GRAPH_BLANK_PIXELS_AFTER_CURSOR 10

namespace eez {
namespace gui {

struct YTGraphWidgetState : public WidgetState {
    bool active;
    bool focused;
    uint32_t refreshCounter;
    uint8_t iChannel;
    uint32_t numHistoryValues;
    uint32_t historyValuePosition;
    uint8_t ytGraphUpdateMethod;
    uint32_t cursorPosition;
    uint8_t *bookmarks;
    bool showLabels;
    int8_t selectedValueIndex;
    bool valueIsVisible[MAX_NUM_OF_Y_VALUES];
    float valueDiv[MAX_NUM_OF_Y_VALUES];
    float valueOffset[MAX_NUM_OF_Y_VALUES];
};

EnumFunctionType YT_GRAPH_enum = nullptr;

// used for YT_GRAPH_UPDATE_METHOD_SCROLL and YT_GRAPH_UPDATE_METHOD_SCAN_LINE
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

    Value::YtDataGetValueFunctionPointer ytDataGetValue;

    YTGraphDrawHelper(const WidgetCursor &widgetCursor_) : widgetCursor(widgetCursor_), widget(widgetCursor.widget) {
        min[0] = ytDataGetMin(widgetCursor, widget->data, 0).getFloat();
        max[0] = ytDataGetMax(widgetCursor, widget->data, 0).getFloat();

        min[1] = ytDataGetMin(widgetCursor, widget->data, 1).getFloat();
        max[1] = ytDataGetMax(widgetCursor, widget->data, 1).getFloat();

        const Style* y1Style = ytDataGetStyle(widgetCursor, widget->data, 0);
        const Style* y2Style = ytDataGetStyle(widgetCursor, widget->data, 1);
        dataColor16[0] = display::getColor16FromIndex(y1Style->color);
        dataColor16[1] = display::getColor16FromIndex(y2Style->color);

        ytDataGetValue = ytDataGetGetValueFunc(widgetCursor, widget->data);
    }

    int getYValue(int valueIndex, uint32_t position) {
        if (position >= numPositions) {
            return INT_MIN;
        }

        float value = ytDataGetValue(position, valueIndex, nullptr);

        if (isNaN(value)) {
            return INT_MIN;
        }

        int y = (int)round((widget->h - 1) * (value - min[valueIndex]) / (max[valueIndex] - min[valueIndex]));
        return widget->h - 1 - y;
    }

    void drawValue(int valueIndex) {
		int yPrevValue = yPrev[valueIndex];
		int yValue = y[valueIndex];
		
		if (yValue == INT_MIN) {
            return;
        }

		// clipping
		if (yPrevValue >= 0 && yValue < 0) {
            yValue = 0;
        } else if (yPrevValue < 0 && yValue >= 0) {
            yPrevValue = 0;
        } else if (yPrevValue < widget->h && yValue >= widget->h) {
            yValue = widget->h - 1;
        } else if (yPrevValue >= widget->h && yValue < widget->h) {
            yPrevValue = widget->h - 1;
        }

		if (yValue < 0 || yPrevValue < 0 || yValue >= widget->h || yPrevValue >= widget->h) {
			return;
		}

        display::setColor16(dataColor16[valueIndex]);

        if (yPrevValue == INT_MIN || abs(yPrevValue - yValue) <= 1) {
            display::drawPixel(x, widgetCursor.y + yValue);
        } else {
            if (yPrevValue < yValue) {
                display::drawVLine(x, widgetCursor.y + yPrevValue + 1, yValue - yPrevValue);
            } else {
                display::drawVLine(x, widgetCursor.y + yValue, yPrevValue - yValue);
            }
        }
    }

    void drawStep() {
        if (y[0] != INT_MIN && y[1] != INT_MIN && abs(yPrev[0] - y[0]) <= 1 && abs(yPrev[1] - y[1]) <= 1 && y[0] == y[1]) {
			if (y[0] >= 0 && y[0] < widget->h) {
				display::setColor16(position % 2 ? dataColor16[1] : dataColor16[0]);
				display::drawPixel(x, widgetCursor.y + y[0]);
			}
        } else {
            drawValue(0);
            drawValue(1);
        }
    }

    void drawScanLine(uint32_t startPosition, uint32_t endPosition, uint16_t graphWidth) {
        numPositions = endPosition;

        int x1 = widgetCursor.x + startPosition % graphWidth;
        int x2 = widgetCursor.x + (endPosition - 1) % graphWidth;
        display::setColor16(color16);
        if (x1 <= x2) {
            display::fillRect(x1, widgetCursor.y, x2 - x1 + 1, widget->h);
        } else {
            display::fillRect(x1, widgetCursor.y, widget->w, widget->h);
            display::fillRect(widgetCursor.x, widgetCursor.y, x2 - widgetCursor.x + 1, widget->h);
        }

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
                graphWidth - numPointsToDraw,
                widgetCursor.widget->h,
                widgetCursor.x,
                widgetCursor.y);
        }

        int endX = widgetCursor.x + graphWidth;
        int startX = endX - numPointsToDraw;

        position = previousHistoryValuePosition + 1;

        numPositions = position + numPointsToDraw;

        yPrev[0] = getYValue(0, previousHistoryValuePosition);
        yPrev[1] = getYValue(1, previousHistoryValuePosition);

        display::setColor16(color16);
        display::fillRect(startX, widgetCursor.y, endX - startX, widget->h);

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

// used for YT_GRAPH_UPDATE_METHOD_STATIC
struct YTGraphStaticDrawHelper {
    const WidgetCursor &widgetCursor;
    const Widget *widget;

    Style* style;

    uint16_t dataColor16;

    uint32_t numPositions;
    uint32_t position;

    float offset;
    float scale;

    int m_valueIndex;

    int x;

    int yPrevMin;
    int yPrevMax;
    int yMin;
    int yMax;

    uint32_t cursorPosition;
    uint8_t *bookmarks;

    Value::YtDataGetValueFunctionPointer ytDataGetValue;

    int xLabels[MAX_NUM_OF_Y_VALUES];
    int yLabels[MAX_NUM_OF_Y_VALUES];

    YTGraphWidgetState *state;

    YTGraphStaticDrawHelper(const WidgetCursor &widgetCursor_, YTGraphWidgetState *state_) : widgetCursor(widgetCursor_), widget(widgetCursor.widget), state(state_) {
        ytDataGetValue = ytDataGetGetValueFunc(widgetCursor, widget->data);
    }

    void getYValue(uint32_t position, int &min, int &max) {
        if (position >= numPositions) {
            max = INT_MIN;
            min = INT_MIN;
        } else {
            float fMax;
            float fMin = ytDataGetValue(position, m_valueIndex, &fMax);

            if (isNaN(fMin)) {
                max = INT_MIN;
            } else {
                max = widget->h - 1 - (int)floor(widget->h / 2.0f + (fMin + offset) * scale);
            }

            if (isNaN(fMax)) {
                min = INT_MIN;
            } else {
                min = widget->h - 1 - (int)floor(widget->h / 2.0f + (fMax + offset) * scale);
            }
        }
    }

    void drawValue() {
        if (yMin == INT_MIN) {
            return;
        }

        display::setColor16(dataColor16);

        int yFrom;
        int yTo;

        if (yPrevMax == INT_MIN) {
            yFrom = yMin;
            yTo = yMax;
        } else {
            if (yPrevMax < yMin) {
                yFrom = yPrevMax + 1;
                yTo = yMax;
            } else if (yMax < yPrevMin) {
                yFrom = yMin;
                yTo = yPrevMin - 1;
            } else {
                yFrom = yMin;
                yTo = yMax;
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
            display::drawPixel(x, widgetCursor.y + yFrom);
        } else {
            display::drawVLine(x, widgetCursor.y + yFrom, yTo - yFrom + 1);
        }
    }

    void getMinMax(int *yLabels, int n, int &yMin, int &yMax) {
        yMin = INT_MAX;
        yMax = INT_MIN;
        for (int i = 0; i < n; i++) {
            if (yLabels[i] < yMin) {
                yMin = yLabels[i];
            }
            if (yLabels[i] > yMax) {
                yMax = yLabels[i];
            }
        }
    }

    void repositionLabels(int labelHeight) {
        for (int valueIndex = 0; valueIndex < MAX_NUM_OF_Y_VALUES; valueIndex++) {
            if (yLabels[valueIndex] != INT_MIN) {
                yLabels[valueIndex] -= labelHeight / 2;
                if (yLabels[valueIndex] < widgetCursor.y) {
                    yLabels[valueIndex] = widgetCursor.y;
                } else if (yLabels[valueIndex] > widgetCursor.y + widgetCursor.widget->h - labelHeight) {
                    yLabels[valueIndex] = widgetCursor.y + widgetCursor.widget->h - labelHeight;
                }
            }
        }
    }

    void drawGraph(uint32_t currentHistoryValuePosition, int startX, int endX, int vertDivisions) {
        //xLabels[m_valueIndex] = INT_MIN;
        yLabels[m_valueIndex] = INT_MIN;

        if (ytDataDataValueIsVisible(widgetCursor, widget->data, m_valueIndex)) {
            position = currentHistoryValuePosition;

            scale = (widget->h - 1) / state->valueDiv[m_valueIndex] / vertDivisions;
            offset = state->valueOffset[m_valueIndex];

            const Style* style = ytDataGetStyle(widgetCursor, widget->data, m_valueIndex);
            dataColor16 = display::getColor16FromIndex(style->color);

            getYValue(position > 0 ? position - 1 : 0, yPrevMin, yPrevMax);

            for (x = startX; x < endX; x++, position++) {
                getYValue(position, yMin, yMax);
                drawValue();
                yPrevMin = yMin;
                yPrevMax = yMax;

                if (yMin != INT_MIN) {
                    //xLabels[m_valueIndex] = x;
                    yLabels[m_valueIndex] = widgetCursor.y + yMin;
                }
            }
        }

		xLabels[m_valueIndex] = endX;
    }

    void drawLabel(font::Font &font, bool transparent) {
        if (yLabels[m_valueIndex] != INT_MIN) {
            const Style *labelStyle = ytDataGetStyle(widgetCursor, widget->data, m_valueIndex);

            char labelText[64];
            ytDataGetLabel(widgetCursor, widget->data, m_valueIndex, labelText, sizeof(labelText));
            int labelWidth = display::measureStr(labelText, -1, font, widgetCursor.widget->w);

            int xLabel = xLabels[m_valueIndex];
            if (xLabel < widgetCursor.x) {
                xLabel = widgetCursor.x;
            } else if (xLabel > widgetCursor.x + widgetCursor.widget->w - labelWidth) {
                xLabel = widgetCursor.x + widgetCursor.widget->w - labelWidth;
            }

            if (!transparent) {
                display::setColor(labelStyle->background_color, false);
                display::fillRect(xLabel, yLabels[m_valueIndex], labelWidth, font.getHeight());
            }

            display::setColor(labelStyle->color, false);
            display::drawStr(labelText, -1, xLabel, yLabels[m_valueIndex], widgetCursor.x, widgetCursor.y, widgetCursor.widget->w, widgetCursor.widget->h, font, -1);
        }
    }

    void drawStatic(uint32_t previousHistoryValuePosition, uint32_t currentHistoryValuePosition, uint32_t numPositions_, uint16_t graphWidth, bool showLabels, int selectedValueIndex) {
        // draw background
        const Style* style = getStyle(widget->style);
        display::setColor(style->background_color);
        display::fillRect(widgetCursor.x, widgetCursor.y, (int)widget->w, (int)widget->h);

        numPositions = numPositions_;

        int startX = widgetCursor.x;
        int endX = startX + graphWidth;

        int horzDivisions = ytDataGetHorzDivisions(widgetCursor, widget->data);
        int vertDivisions = ytDataGetVertDivisions(widgetCursor, widget->data);

        // draw grid
        display::setColor(style->border_color);
        for (int x = 1; x < horzDivisions; x++) {
            display::drawVLine(widgetCursor.x + x * widget->w / horzDivisions, widgetCursor.y, widget->h);
        }
        for (int y = 1; y < vertDivisions; y++) {
            display::drawHLine(widgetCursor.x, widgetCursor.y + y * widget->h / vertDivisions, widget->w);
        }

		// draw bookmarks
		if (bookmarks) {
			for (int x = 0; x < widget->w; x++) {
				if (bookmarks[x]) {
					display::setColor(COLOR_ID_BOOKMARK);
					display::drawVLine(startX + x, widgetCursor.y, widget->h);
				}
			}
		}

        // draw graphs
        for (m_valueIndex = 0; m_valueIndex < MAX_NUM_OF_Y_VALUES; m_valueIndex++) {
            if (m_valueIndex != selectedValueIndex) {
                drawGraph(currentHistoryValuePosition, startX, endX, vertDivisions);
            }
        }
        if (selectedValueIndex != -1) {
            m_valueIndex = selectedValueIndex;
            drawGraph(currentHistoryValuePosition, startX, endX, vertDivisions);
        }

		// draw cursor
		if (ytDataIsCursorVisible(widgetCursor, widgetCursor.widget->data)) {
			display::setColor(style->color);
			display::drawVLine(startX + cursorPosition - currentHistoryValuePosition, widgetCursor.y, widget->h);

			char text[64];
			ytDataGetCursorXValue(widgetCursor, widgetCursor.widget->data).toText(text, sizeof(text));

			font::Font font = styleGetFont(style);
			int MIN_CURSOR_TEXT_WIDTH = 80;
			int cursorTextWidth = MAX(display::measureStr(text, -1, font), MIN_CURSOR_TEXT_WIDTH);
			int cursorTextHeight = font.getHeight();
			const int PADDING = 0;
			int xCursorText = widgetCursor.x + cursorPosition - currentHistoryValuePosition - cursorTextWidth / 2;
			if (xCursorText < widgetCursor.x + PADDING) {
				xCursorText = widgetCursor.x + PADDING;
			} else if (xCursorText + cursorTextWidth > widgetCursor.x + widgetCursor.widget->w - PADDING) {
				xCursorText = widgetCursor.x + widgetCursor.widget->w - PADDING - cursorTextWidth;
			}
			int yCursorText = widgetCursor.y + widgetCursor.widget->h - cursorTextHeight - PADDING;

			drawText(text, -1,
				xCursorText, yCursorText, cursorTextWidth, cursorTextHeight,
				style, state->focused, false, false, nullptr, nullptr, nullptr, nullptr
			);
		}
		
		// draw labels
        if (showLabels) {
            font::Font font = styleGetFont(style);
            int labelHeight = font.getHeight();

            repositionLabels(labelHeight);

            for (m_valueIndex = 0; m_valueIndex < MAX_NUM_OF_Y_VALUES; m_valueIndex++) {
                if (m_valueIndex != selectedValueIndex) {
                    drawLabel(font, true);
                }
            }

            if (selectedValueIndex != -1) {
                m_valueIndex = selectedValueIndex;
                drawLabel(font, false);
            }
        }
    }
};

////////////////////////////////////////////////////////////////////////////////

DrawFunctionType YT_GRAPH_draw = [](const WidgetCursor &widgetCursor) {
    const Widget *widget = widgetCursor.widget;

	YTGraphWidgetState state;
    state.active = g_isActiveWidget;
    state.focused = isFocusWidget(widgetCursor);
    state.data = get(widgetCursor, widget->data);

    state.refreshCounter = ytDataGetRefreshCounter(widgetCursor, widget->data);
    state.iChannel = widgetCursor.cursor;
    state.numHistoryValues = ytDataGetSize(widgetCursor, widget->data);
    state.historyValuePosition = ytDataGetPosition(widgetCursor, widget->data);
    state.ytGraphUpdateMethod = ytDataGetGraphUpdateMethod(widgetCursor, widget->data);
    state.cursorPosition = state.historyValuePosition + ytDataGetCursorOffset(widgetCursor, widget->data);
    state.bookmarks = ytDataGetBookmarks(widgetCursor, widget->data);

    state.showLabels = ytDataGetShowLabels(widgetCursor, widget->data);
    state.selectedValueIndex = ytDataGetSelectedValueIndex(widgetCursor, widget->data);

    if (state.ytGraphUpdateMethod == YT_GRAPH_UPDATE_METHOD_STATIC) {
        for (int valueIndex = 0; valueIndex < MAX_NUM_OF_Y_VALUES; valueIndex++) {
            state.valueIsVisible[valueIndex] = ytDataDataValueIsVisible(widgetCursor, widget->data, valueIndex);
            state.valueDiv[valueIndex] = ytDataGetDiv(widgetCursor, widget->data, valueIndex);
            state.valueOffset[valueIndex] = ytDataGetOffset(widgetCursor, widget->data, valueIndex);
        }
    }
    uint16_t graphWidth = (uint16_t)widget->w;

    uint32_t previousHistoryValuePosition;
    previousHistoryValuePosition = state.historyValuePosition - graphWidth;

    if (state.ytGraphUpdateMethod == YT_GRAPH_UPDATE_METHOD_STATIC) {
        YTGraphStaticDrawHelper drawHelper(widgetCursor, &state);

        drawHelper.cursorPosition = state.cursorPosition;
        drawHelper.bookmarks = state.bookmarks;
        drawHelper.drawStatic(previousHistoryValuePosition, state.historyValuePosition, state.numHistoryValues, graphWidth, state.showLabels, state.selectedValueIndex);
    } else {
        const Style* style = getStyle(widget->style);

        display::setColor(style->background_color);
        display::fillRect(widgetCursor.x, widgetCursor.y, (int)widget->w, (int)widget->h);

        YTGraphDrawHelper drawHelper(widgetCursor);
        drawHelper.color16 = display::getColor16FromIndex(state.active ? style->color : style->background_color);
        if (state.ytGraphUpdateMethod == YT_GRAPH_UPDATE_METHOD_SCAN_LINE) {
            drawHelper.drawScanLine(previousHistoryValuePosition, state.historyValuePosition, graphWidth);

            int x = widgetCursor.x;

            // draw cursor
            display::setColor(style->color);
            display::drawVLine(x + state.historyValuePosition % graphWidth, widgetCursor.y, (int)widget->h);
            display::drawVLine(x + (state.historyValuePosition + 1) % graphWidth, widgetCursor.y, (int)widget->h);

            // draw blank lines
            int x1 = x + (state.historyValuePosition + 2) % graphWidth;
            int x2 = x + (state.historyValuePosition + CONF_GUI_YT_GRAPH_BLANK_PIXELS_AFTER_CURSOR) % graphWidth;

            display::setColor(style->background_color);
            if (x1 < x2) {
                display::fillRect(x1, widgetCursor.y, x2 - x1 + 1, (int)widget->h);
            } else {
                display::fillRect(x1, widgetCursor.y, x + graphWidth - x1, (int)widget->h);
                display::fillRect(x, widgetCursor.y, x2 - x +1, (int)widget->h);
            }
        } else if (state.ytGraphUpdateMethod == YT_GRAPH_UPDATE_METHOD_SCROLL) {
            drawHelper.drawScrolling(previousHistoryValuePosition, state.historyValuePosition, state.numHistoryValues, graphWidth);
        }
    }
};

OnTouchFunctionType YT_GRAPH_onTouch = [](const WidgetCursor &widgetCursor, Event &touchEvent) {
    if (ytDataGetGraphUpdateMethod(widgetCursor, widgetCursor.widget->data) == YT_GRAPH_UPDATE_METHOD_STATIC) {
        if (touchEvent.type == EVENT_TYPE_TOUCH_DOWN || touchEvent.type == EVENT_TYPE_TOUCH_MOVE) {
			TouchDrag touchDrag = {
				widgetCursor,
				touchEvent.type,
				touchEvent.x - widgetCursor.x,
				touchEvent.y - widgetCursor.y
			};
            ytDataTouchDrag(widgetCursor, widgetCursor.widget->data, &touchDrag);
        }
    } else {
        if (touchEvent.type == EVENT_TYPE_TOUCH_DOWN) {
            if (widgetCursor.appContext->isWidgetActionEnabled(widgetCursor)) {
                auto action = getWidgetAction(widgetCursor);        
                executeAction(widgetCursor, action);
            }
        }
    }
};

OnKeyboardFunctionType YT_GRAPH_onKeyboard = nullptr;

} // namespace gui
} // namespace eez

#endif
