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

#include <eez/core/sound.h>
#include <eez/core/util.h>

#include <eez/gui/gui.h>
#include <eez/gui/widgets/drop_down_list.h>

namespace eez {
namespace gui {

bool DropDownListWidgetState::updateState() {
	const WidgetCursor &widgetCursor = g_widgetCursor;
	bool hasPreviousState = widgetCursor.hasPreviousState;
	auto widget = (const DropDownListWidget *)widgetCursor.widget;

	WIDGET_STATE(flags.active, g_isActiveWidget);
	WIDGET_STATE(data, get(widgetCursor, widget->data));
	WIDGET_STATE(options, get(widgetCursor, widget->options));

	return !hasPreviousState;
}

void DropDownListWidgetState::render() {
    const WidgetCursor &widgetCursor = g_widgetCursor;
    auto widget = (const ButtonWidget *)widgetCursor.widget;
    const Style *style = getStyle(widget->style);

    int x1 = widgetCursor.x;
    int y1 = widgetCursor.y;
    int x2 = widgetCursor.x + widget->w - 1;
    int y2 = widgetCursor.y + widget->h - 1;

    drawBorderAndBackground(x1, y1, x2, y2, style, flags.active ? style->activeBackgroundColor : style->backgroundColor, false);

    double x = x1;
    double y = y1;
    double w = x2 - x1 + 1;
    double h = y2 - y1 + 1;

	char text[64] = { 0 };

	if (options.type == VALUE_TYPE_ARRAY) {
		if (data.getInt() >= 0 && data.getInt() < (int)options.arrayValue->arraySize) {
			options.arrayValue->values[data.getInt()].toText(text, sizeof(text));
		}
	}

	drawText(
		text,
		-1,
		x,
		y,
		w - h + (2 * h) / 6,
		h,
		style,
		flags.active, false, false,
		nullptr, nullptr,
		nullptr, nullptr,
		false, -1, 0,
		true
	);

    x += w - h;
    w = h;

    x += (2 * h) / 6.0;
    y += (4 * h) / 10.0;
    w -= (2 * h) / 3.0;
    h -= (4 * h) / 5.0;

	display::AggDrawing aggDrawing;
	display::aggInit(aggDrawing);

    aggDrawing.graphics.resetPath();
    aggDrawing.graphics.moveTo(x, y);
    aggDrawing.graphics.lineTo(x + w / 2, y + h);
    aggDrawing.graphics.lineTo(x + w, y);
    aggDrawing.graphics.lineColor(COLOR_TO_R(style->color), COLOR_TO_G(style->color), COLOR_TO_B(style->color));
    aggDrawing.graphics.lineWidth(h / 3.0);
    aggDrawing.graphics.noFill();
    aggDrawing.graphics.drawPath();
}

bool DropDownListWidgetState::hasOnTouch() {
    return true;
}

static WidgetCursor g_dropDownListWidgetCursor;

static void enumDefinitionFunc(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
	auto widget = (const DropDownListWidget *)g_dropDownListWidgetCursor.widget;
	auto options = get(g_dropDownListWidgetCursor, widget->options);

	if (options.type == VALUE_TYPE_ARRAY) {
		if (widgetCursor.cursor >= 0 && widgetCursor.cursor < (int)options.arrayValue->arraySize) {
			if (operation == DATA_OPERATION_GET_LABEL) {
				value = options.arrayValue->values[widgetCursor.cursor];
			} else if (operation == DATA_OPERATION_GET_VALUE) {
				value = widgetCursor.cursor;
			}
		}
	}
}

static void onSet(uint16_t value) {
	auto widget = (const DropDownListWidget *)g_dropDownListWidgetCursor.widget;
	set(g_dropDownListWidgetCursor, widget->data, (int)value);
	g_dropDownListWidgetCursor.appContext->popPage();
}

void DropDownListWidgetState::onTouch(const WidgetCursor &widgetCursor, Event &touchEvent) {
	if (touchEvent.type == EVENT_TYPE_TOUCH_UP) {
		g_dropDownListWidgetCursor = widgetCursor;
		SelectFromEnumPage::pushSelectFromEnumPage(widgetCursor.appContext, enumDefinitionFunc, data.getInt(), nullptr, onSet, false, false);
	}
}


} // namespace gui
} // namespace eez
