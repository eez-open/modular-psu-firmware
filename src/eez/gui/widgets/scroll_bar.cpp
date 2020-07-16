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

#include <eez/util.h>
#include <eez/sound.h>
#include <eez/debug.h>
#include <eez/keyboard.h>

#include <eez/gui/gui.h>

using namespace eez::mcu;

namespace eez {
namespace gui {

struct ScrollBarWidget {
    uint16_t thumbStyle;
    uint16_t buttonsStyle;
    const char *leftButtonText;
	const char *rightButtonText;
};

enum ScrollBarWidgetSegment {
    SCROLL_BAR_WIDGET_SEGMENT_NONE,
    SCROLL_BAR_WIDGET_SEGMENT_TRACK_LEFT,
    SCROLL_BAR_WIDGET_SEGMENT_TRACK_RIGHT,
    SCROLL_BAR_WIDGET_SEGMENT_THUMB,
    SCROLL_BAR_WIDGET_SEGMENT_LEFT_BUTTON,
    SCROLL_BAR_WIDGET_SEGMENT_RIGHT_BUTTON
};

struct ScrollBarWidgetState {
    WidgetState genericState;
    int size;
    int position;
    int pageSize;
    ScrollBarWidgetSegment segment;
};

FixPointersFunctionType SCROLL_BAR_fixPointers = [](Widget *widget, Assets *assets) {
    ScrollBarWidget *scrollBarWidget = (ScrollBarWidget *)widget->specific;
    scrollBarWidget->leftButtonText = (const char *)((uint8_t *)assets->document + (uint32_t)scrollBarWidget->leftButtonText);
    scrollBarWidget->rightButtonText = (const char *)((uint8_t *)assets->document + (uint32_t)scrollBarWidget->rightButtonText);
};

EnumFunctionType SCROLL_BAR_enum = nullptr;

static ScrollBarWidgetSegment g_segment;
static WidgetCursor g_selectedWidget;
static int g_dragStartX;
static int g_dragStartPosition;

int getSize(const WidgetCursor &widgetCursor) {
    return ytDataGetSize(((WidgetCursor &)widgetCursor).cursor, widgetCursor.widget->data);
}

int getPosition(const WidgetCursor &widgetCursor) {
    return ytDataGetPosition(((WidgetCursor &)widgetCursor).cursor, widgetCursor.widget->data);
}

int getPositionIncrement(const WidgetCursor &widgetCursor) {
    return ytDataGetPositionIncrement(((WidgetCursor &)widgetCursor).cursor, widgetCursor.widget->data);
}

int getPageSize(const WidgetCursor &widgetCursor) {
    return ytDataGetPageSize(((WidgetCursor &)widgetCursor).cursor, widgetCursor.widget->data);
}

void setPosition(const WidgetCursor &widgetCursor, int position) {
    ytDataSetPosition(((WidgetCursor &)widgetCursor).cursor, widgetCursor.widget->data, position < 0 ? 0 : position);
}

void getThumbGeometry(int size, int position, int pageSize, int xTrack, int wTrack, int minThumbWidth, int &xThumb, int &widthThumb) {
    double x = 1.0 * position * wTrack / size;
    double width = 1.0 * pageSize * wTrack / size;

    if (width < minThumbWidth) {
        x -=  1.0 * (minThumbWidth - width) * position / size;
        width = minThumbWidth;
    }

    xThumb = (int)round(x);
    widthThumb = (int)round(width + x - xThumb);

    xThumb += xTrack;
}

DrawFunctionType SCROLL_BAR_draw = [](const WidgetCursor &widgetCursor) {
    const Widget *widget = widgetCursor.widget;
    const ScrollBarWidget *scrollBarWidget = GET_WIDGET_PROPERTY(widget, specific, const ScrollBarWidget *);

    widgetCursor.currentState->size = sizeof(ScrollBarWidgetState);
    widgetCursor.currentState->flags.active = g_selectedWidget == widgetCursor;

    auto currentState = (ScrollBarWidgetState *)widgetCursor.currentState;
    auto previousState = (ScrollBarWidgetState *)widgetCursor.previousState;

    currentState->size = getSize(widgetCursor);
    currentState->position = getPosition(widgetCursor);
    currentState->pageSize = getPageSize(widgetCursor);
    currentState->segment = g_segment;

    bool refresh =
        !widgetCursor.previousState ||
        widgetCursor.previousState->flags.active != widgetCursor.currentState->flags.active ||
        previousState->size != currentState->size ||
        previousState->position != currentState->position ||
        previousState->pageSize != currentState->pageSize ||
        previousState->segment != currentState->segment;

    if (refresh) {
        if (currentState->pageSize < currentState->size) {
            const Style *buttonsStyle = getStyle(scrollBarWidget->buttonsStyle);
            auto isHorizontal = widget->w > widget->h;

            int buttonSize = isHorizontal ? widget->h : widget->w;

            // draw left button
            drawText(GET_WIDGET_PROPERTY(scrollBarWidget, leftButtonText, const char *), -1, 
                widgetCursor.x, 
                widgetCursor.y, 
                isHorizontal ? buttonSize : (int)widget->w, 
                isHorizontal ? (int)widget->h : buttonSize, buttonsStyle, 
                currentState->segment == SCROLL_BAR_WIDGET_SEGMENT_LEFT_BUTTON, false, false, nullptr, nullptr, nullptr, nullptr);

            // draw track
            int xTrack;
            int yTrack;
            int wTrack;
            int hTrack;

            if (isHorizontal) {
                xTrack = widgetCursor.x + buttonSize;
                yTrack = widgetCursor.y;
                wTrack = widget->w - 2 * buttonSize;
                hTrack = widget->h;
            } else {
                xTrack = widgetCursor.x;
                yTrack = widgetCursor.y + buttonSize;
                wTrack = widget->w;
                hTrack = widget->h - 2 * buttonSize;
            }

            const Style *trackStyle = getStyle(widget->style);
            display::setColor(trackStyle->color);
            display::fillRect(xTrack, yTrack, xTrack + wTrack - 1, yTrack + hTrack - 1, 0);

            // draw thumb
            const Style *thumbStyle = getStyle(scrollBarWidget->thumbStyle);
            display::setColor(thumbStyle->color);
            if (isHorizontal) {
                int xThumb, wThumb;
                getThumbGeometry(currentState->size, currentState->position, currentState->pageSize, xTrack, wTrack, buttonSize, xThumb, wThumb);
                display::fillRect(xThumb, yTrack, xThumb + wThumb - 1, yTrack + hTrack - 1);
            } else {
                int yThumb, hThumb;
                getThumbGeometry(currentState->size, currentState->position, currentState->pageSize, yTrack, hTrack, buttonSize, yThumb, hThumb);
                display::fillRect(xTrack, yThumb, xTrack + wTrack - 1, yThumb + hThumb - 1);
            }

            // draw right button
            drawText(GET_WIDGET_PROPERTY(scrollBarWidget, rightButtonText, const char *), -1, 
                isHorizontal ? widgetCursor.x + widget->w - buttonSize : widgetCursor.x, 
                isHorizontal ? widgetCursor.y : widgetCursor.y + widget->h - buttonSize, 
                isHorizontal ? buttonSize : (int)widget->w, 
                isHorizontal ? (int)widget->h : buttonSize, buttonsStyle, 
                currentState->segment == SCROLL_BAR_WIDGET_SEGMENT_RIGHT_BUTTON, false, false, nullptr, nullptr, nullptr, nullptr);
        } else {
            // scroll bar is hidden
            const Style *trackStyle = getStyle(widget->style);
            display::setColor(trackStyle->color);
            display::fillRect(widgetCursor.x, widgetCursor.y, widgetCursor.x + widget->w - 1, widgetCursor.y + widget->h - 1, 0);
        }
    }
};

OnTouchFunctionType SCROLL_BAR_onTouch = [](const WidgetCursor &widgetCursor, Event &touchEvent) {
    int size = getSize(widgetCursor);
    int pageSize = getPageSize(widgetCursor);

    if (size > pageSize) {
        const Widget *widget = widgetCursor.widget;

        auto isHorizontal = widget->w > widget->h;
        int buttonSize = isHorizontal ? widget->h : widget->w;

        int xTrack;
        int wTrack;
        int x;

        if (isHorizontal) {
            x = touchEvent.x;
            xTrack = widgetCursor.x + buttonSize;
            wTrack = widget->w - 2 * buttonSize;
        } else {
            x = touchEvent.y;
            xTrack = widgetCursor.y + buttonSize;
            wTrack = widget->h - 2 * buttonSize;
        }

        if (touchEvent.type == EVENT_TYPE_TOUCH_DOWN || touchEvent.type == EVENT_TYPE_AUTO_REPEAT) {
            if (touchEvent.type == EVENT_TYPE_TOUCH_DOWN) {
                g_selectedWidget = widgetCursor;
                g_segment = SCROLL_BAR_WIDGET_SEGMENT_NONE;
            }

            if (touchEvent.type == EVENT_TYPE_AUTO_REPEAT && (g_segment == SCROLL_BAR_WIDGET_SEGMENT_NONE || g_segment == SCROLL_BAR_WIDGET_SEGMENT_THUMB)) {
                return;
            }

            if ((isHorizontal && touchEvent.x < widgetCursor.x + buttonSize) || (!isHorizontal && touchEvent.y < widgetCursor.y + buttonSize)) {
                setPosition(widgetCursor, getPosition(widgetCursor) - getPositionIncrement(widgetCursor));
                g_segment = SCROLL_BAR_WIDGET_SEGMENT_LEFT_BUTTON;
                if (touchEvent.type == EVENT_TYPE_TOUCH_DOWN) {
                    sound::playClick();
                }
            } else if ((isHorizontal && (touchEvent.x >= widgetCursor.x + widget->w - buttonSize)) || (!isHorizontal && (touchEvent.y >= widgetCursor.y + widget->h - buttonSize))) {
                setPosition(widgetCursor, getPosition(widgetCursor) + getPositionIncrement(widgetCursor));
                g_segment = SCROLL_BAR_WIDGET_SEGMENT_RIGHT_BUTTON;
                if (touchEvent.type == EVENT_TYPE_TOUCH_DOWN) {
                    sound::playClick();
                }
            } else {
                int xThumb, wThumb;
                
                int position = getPosition(widgetCursor);

                getThumbGeometry(size, position, pageSize, xTrack, wTrack, buttonSize, xThumb, wThumb);

                if (x < xThumb) {
                    setPosition(widgetCursor, getPosition(widgetCursor) - pageSize);
                    g_segment = SCROLL_BAR_WIDGET_SEGMENT_TRACK_LEFT;
                    if (touchEvent.type == EVENT_TYPE_TOUCH_DOWN) {
                        sound::playClick();
                    }
                } else if (x >= xThumb + wThumb) {
                    setPosition(widgetCursor, getPosition(widgetCursor) + pageSize);
                    g_segment = SCROLL_BAR_WIDGET_SEGMENT_TRACK_RIGHT;
                    if (touchEvent.type == EVENT_TYPE_TOUCH_DOWN) {
                        sound::playClick();
                    }
                } else if (x >= xThumb || touchEvent.x < xThumb + wThumb) {
                    g_segment = SCROLL_BAR_WIDGET_SEGMENT_THUMB;
                    g_dragStartX = x;
                    g_dragStartPosition = getPosition(widgetCursor);
                }
            }
        } else if (touchEvent.type == EVENT_TYPE_TOUCH_MOVE) {
            if (g_segment == SCROLL_BAR_WIDGET_SEGMENT_THUMB) {
                int size = getSize(widgetCursor);
                setPosition(widgetCursor, g_dragStartPosition + (int)round(1.0 * (x - g_dragStartX) * size / wTrack));
            }
        } else if (touchEvent.type == EVENT_TYPE_TOUCH_UP) {
            g_selectedWidget = 0;
            g_segment = SCROLL_BAR_WIDGET_SEGMENT_NONE;
        }
    }
};

OnKeyboardFunctionType SCROLL_BAR_onKeyboard = [](const WidgetCursor &widgetCursor, uint8_t key, uint8_t mod) {
    if (mod == 0) {
        int position = getPosition(widgetCursor);
        int increment = getPositionIncrement(widgetCursor);
        int size = getSize(widgetCursor);
        int pageSize = getPageSize(widgetCursor);

        if (key == KEY_LEFTARROW || key == KEY_UPARROW) {
            setPosition(widgetCursor, position - increment);
            return true;
        } else if (key == KEY_RIGHTARROW || key == KEY_DOWNARROW) {
            setPosition(widgetCursor, position + increment);
            return true;
        } else if (key == KEY_PAGEUP) {
            setPosition(widgetCursor, position - pageSize);
            return true;
        } else if (key == KEY_PAGEDOWN) {
            setPosition(widgetCursor, position + pageSize);
            return true;
        } else if (key == KEY_HOME) {
            setPosition(widgetCursor, 0);
            return true;
        } else if (key == KEY_END1) {
            setPosition(widgetCursor, position + size - pageSize);
            return true;
        }
    }
    return false;
};

} // namespace gui
} // namespace eez

#endif
