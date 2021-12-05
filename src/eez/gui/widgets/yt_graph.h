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

#pragma once

namespace eez {
namespace gui {

enum {
	YT_GRAPH_UPDATE_METHOD_SCROLL,
	YT_GRAPH_UPDATE_METHOD_SCAN_LINE,
	YT_GRAPH_UPDATE_METHOD_STATIC
};

struct YTGraphWidget : public Widget {
};


struct YTGraphWidgetState : public WidgetState {
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

    YTGraphWidgetState(const WidgetCursor &widgetCursor) : WidgetState(widgetCursor) {
        auto widget = (const YTGraphWidget *)widgetCursor.widget;

        flags.focused = isFocusWidget(widgetCursor);
        data = get(widgetCursor, widget->data);

        refreshCounter = ytDataGetRefreshCounter(widgetCursor, widget->data);
        iChannel = widgetCursor.cursor;
        numHistoryValues = ytDataGetSize(widgetCursor, widget->data);
        historyValuePosition = ytDataGetPosition(widgetCursor, widget->data);
        ytGraphUpdateMethod = ytDataGetGraphUpdateMethod(widgetCursor, widget->data);
        cursorPosition = historyValuePosition + ytDataGetCursorOffset(widgetCursor, widget->data);
        bookmarks = ytDataGetBookmarks(widgetCursor, widget->data);
        showLabels = ytDataGetShowLabels(widgetCursor, widget->data);
        selectedValueIndex = ytDataGetSelectedValueIndex(widgetCursor, widget->data);

        if (ytGraphUpdateMethod == YT_GRAPH_UPDATE_METHOD_STATIC) {
            for (int valueIndex = 0; valueIndex < MAX_NUM_OF_Y_VALUES; valueIndex++) {
                valueIsVisible[valueIndex] = ytDataDataValueIsVisible(widgetCursor, widget->data, valueIndex);
                valueDiv[valueIndex] = ytDataGetDiv(widgetCursor, widget->data, valueIndex);
                valueOffset[valueIndex] = ytDataGetOffset(widgetCursor, widget->data, valueIndex);
            }
        }
    }

    bool operator!=(const YTGraphWidgetState& previousState) {
        if (
            flags.focused != previousState.flags.focused ||
            refreshCounter != previousState.refreshCounter ||
            iChannel != previousState.iChannel ||
            numHistoryValues != previousState.numHistoryValues ||
            historyValuePosition != previousState.historyValuePosition ||
            ytGraphUpdateMethod != previousState.ytGraphUpdateMethod ||
            cursorPosition != previousState.cursorPosition ||
            bookmarks != previousState.bookmarks ||
            showLabels != previousState.showLabels ||
            selectedValueIndex != previousState.selectedValueIndex
        ) {
            return true;
        }

        if (ytGraphUpdateMethod == YT_GRAPH_UPDATE_METHOD_STATIC) {
            for (int valueIndex = 0; valueIndex < MAX_NUM_OF_Y_VALUES; valueIndex++) {
                if (
                    valueIsVisible[valueIndex] != previousState.valueIsVisible[valueIndex] || 
                    valueDiv[valueIndex] != previousState.valueDiv[valueIndex] || 
                    valueOffset[valueIndex] != previousState.valueOffset[valueIndex]
                ) {
                    return true;
                }
            }
        }

        return false;
    }

    uint32_t getHistoryValuePosition();

    void draw(WidgetState *previousState) override;
	bool hasOnTouch() override;
	void onTouch(Event &touchEvent) override;
};

} // gui
} // eez
