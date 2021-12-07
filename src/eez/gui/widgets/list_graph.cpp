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

#include <math.h>
#include <stdlib.h>

#include <eez/sound.h>

#include <eez/gui/gui.h>
#include <eez/gui/widgets/list_graph.h>

namespace eez {
namespace gui {

bool ListGraphWidgetState::updateState() {
    const WidgetCursor &widgetCursor = g_widgetCursor;

    bool hasPreviousState = widgetCursor.hasPreviousState;
    auto widget = (const ListGraphWidget *)widgetCursor.widget;
    
    WIDGET_STATE(data, get(widgetCursor, widget->data));
    WIDGET_STATE(cursorData, get(widgetCursor, widget->cursorData));

    return !hasPreviousState;
}

void ListGraphWidgetState::render() {
    const WidgetCursor &widgetCursor = g_widgetCursor;

    auto widget = (const ListGraphWidget *)widgetCursor.widget;

    const Style* style = getStyle(widget->style);
    const Style* y1Style = getStyle(widget->y1Style);
    const Style* y2Style = getStyle(widget->y2Style);
    const Style* cursorStyle = getStyle(widget->cursorStyle);

    int iCursor = cursorData.getInt();
    int iRow = iCursor / 3;

    // draw background
    display::setColor(style->background_color);
    display::fillRect(widgetCursor.x, widgetCursor.y, widgetCursor.x + (int)widget->w - 1,
                        widgetCursor.y + (int)widget->h - 1);

    int dwellListLength = getFloatListLength(widgetCursor, widget->dwellData);
    if (dwellListLength > 0) {
        float *dwellList = getFloatList(widgetCursor, widget->dwellData);

        const Style *styles[2] = { y1Style, y2Style };

        int listLength[2] = { getFloatListLength(widgetCursor, widget->y1Data),
                                getFloatListLength(widgetCursor, widget->y2Data) };

        float *list[2] = { getFloatList(widgetCursor, widget->y1Data),
                            getFloatList(widgetCursor, widget->y2Data) };

        float min[2] = {
            getMin(widgetCursor, widget->y1Data).getFloat(),
            getMin(widgetCursor, widget->y2Data).getFloat()
        };

        float max[2] = {
            getMax(widgetCursor, widget->y1Data).getFloat(),
            getMax(widgetCursor, widget->y2Data).getFloat()
        };

        int maxListLength = getFloatListLength(widgetCursor, widget->data);

        float dwellSum = 0;
        for (int i = 0; i < maxListLength; ++i) {
            if (i < dwellListLength) {
                dwellSum += dwellList[i];
            } else {
                dwellSum += dwellList[dwellListLength - 1];
            }
        }

        float currentDwellSum = 0;
        int xPrev = widgetCursor.x;
        int yPrev[2];
        for (int i = 0; i < maxListLength; ++i) {
            currentDwellSum +=
                i < dwellListLength ? dwellList[i] : dwellList[dwellListLength - 1];
            int x1 = xPrev;
            int x2;
            if (i == maxListLength - 1) {
                x2 = widgetCursor.x + (int)widget->w - 1;
            } else {
                x2 = widgetCursor.x + int(currentDwellSum * (int)widget->w / dwellSum);
            }
            if (x2 < x1)
                x2 = x1;
            if (x2 >= widgetCursor.x + (int)widget->w)
                x2 = widgetCursor.x + (int)widget->w - 1;

            if (i == iRow) {
                display::setColor(cursorStyle->background_color);
                display::fillRect(x1, widgetCursor.y, x2 - 1,
                    widgetCursor.y + (int)widget->h - 1);
            }


            for (int k = 0; k < 2; ++k) {
                int j = iCursor % 3 == 2 ? k : 1 - k;

                if (listLength[j] > 0) {
                    display::setColor(styles[j]->color);

                    float value = i < listLength[j] ? list[j][i] : list[j][listLength[j] - 1];
                    int y = int((value - min[j]) * widget->h / (max[j] - min[j]));
                    if (y < 0)
                        y = 0;
                    if (y >= (int)widget->h)
                        y = (int)widget->h - 1;

                    y = widgetCursor.y + ((int)widget->h - 1) - y;

                    if (i > 0 && abs(yPrev[j] - y) > 1) {
                        if (yPrev[j] < y) {
                            display::drawVLine(x1, yPrev[j] + 1, y - yPrev[j] - 1);
                        } else {
                            display::drawVLine(x1, y, yPrev[j] - y - 1);
                        }
                    }

                    yPrev[j] = y;

                    display::drawHLine(x1, y, x2 - x1);
                }
            }

            xPrev = x2;
        }
    }
}

bool ListGraphWidgetState::hasOnTouch() {
    return true;
}

void ListGraphWidgetState::onTouch(const WidgetCursor &widgetCursor, Event &touchEvent) {
    if (touchEvent.type == EVENT_TYPE_TOUCH_DOWN || touchEvent.type == EVENT_TYPE_TOUCH_MOVE) {
        auto widget = (const ListGraphWidget *)widgetCursor.widget;

        if (touchEvent.x < widgetCursor.x || touchEvent.x >= widgetCursor.x + (int)widget->w) {
            return;
        }
        if (touchEvent.y < widgetCursor.y || touchEvent.y >= widgetCursor.y + (int)widget->h) {
            return;
        }

        int dwellListLength = getFloatListLength(widgetCursor, widget->dwellData);
        if (dwellListLength > 0) {
            int iCursor = -1;

            float *dwellList = getFloatList(widgetCursor, widget->dwellData);

            int maxListLength = getFloatListLength(widgetCursor, widget->data);

            float dwellSum = 0;
            for (int i = 0; i < maxListLength; ++i) {
                if (i < dwellListLength) {
                    dwellSum += dwellList[i];
                } else {
                    dwellSum += dwellList[dwellListLength - 1];
                }
            }

            float currentDwellSum = 0;
            int xPrev = widgetCursor.x;
            for (int i = 0; i < maxListLength; ++i) {
                currentDwellSum +=
                    i < dwellListLength ? dwellList[i] : dwellList[dwellListLength - 1];
                int x1 = xPrev;
                int x2;
                if (i == maxListLength - 1) {
                    x2 = widgetCursor.x + (int)widget->w - 1;
                } else {
                    x2 = widgetCursor.x + int(currentDwellSum * (int)widget->w / dwellSum);
                }
                if (x2 < x1)
                    x2 = x1;
                if (x2 >= widgetCursor.x + (int)widget->w)
                    x2 = widgetCursor.x + (int)widget->w - 1;

                if (touchEvent.x >= x1 && touchEvent.x < x2) {
                    int iCurrentCursor =
                        get(widgetCursor, widget->cursorData).getInt();
                    iCursor = i * 3 + iCurrentCursor % 3;
                    break;
                }
            }

            if (iCursor >= 0) {
                set(widgetCursor, widget->cursorData, Value(iCursor));
                if (touchEvent.type == EVENT_TYPE_TOUCH_DOWN) {
                    sound::playClick();
                }
            }
        }
    }
}

} // namespace gui
} // namespace eez
