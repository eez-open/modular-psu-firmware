/*
 * EEZ Modular Firmware
 * Copyright (C) 2020-present, Envox d.o.o.
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
#include <stdio.h>

#include <eez/core/alloc.h>
#include <eez/core/util.h>

#include <eez/gui/gui.h>
#include <eez/gui/data.h>
#include <eez/gui/widgets/gauge.h>

namespace eez {
namespace gui {

////////////////////////////////////////////////////////////////////////////////

void arcBar(Agg2D &graphics, int xCenter, int yCenter, int radOuter, int radInner, float angleDeg) {
	auto angle = Agg2D::deg2Rad(angleDeg);
	auto cosa = cos(angle);
	auto sina = sin(angle);
	
	graphics.moveTo(xCenter - radOuter, yCenter);

	graphics.arcTo(
		radOuter, // rx
		radOuter, // ry
		Agg2D::deg2Rad(0), // angle
		0, // largeArcFlag
		1, // sweepFlag
		xCenter + radOuter * cosa, // dx
		yCenter - radOuter * sina // dy
	);

	graphics.lineTo(
		xCenter + radInner * cosa,
		yCenter - radInner * sina
	);

	graphics.arcTo(
		radInner,
		radInner,
		Agg2D::deg2Rad(-180),
		0,
		0,
		xCenter - radInner,
		yCenter
	);

	graphics.lineTo(xCenter - radOuter, yCenter);
}

float firstTick(float n) {
	auto p = powf(10, floorf(log10f(n / 6.0f)));
	auto f = n / 6.0f / p;
	int i;
	if (f > 5) {
		i = 10;
	} else if (f > 2) {
		i = 5;
	} else {
		i = 2;
	}
	return i * p;
}

////////////////////////////////////////////////////////////////////////////////

bool GaugeWidgetState::updateState() {
    const WidgetCursor &widgetCursor = g_widgetCursor;

	bool hasPreviousState = widgetCursor.hasPreviousState;
	auto widget = (const GaugeWidget *)widgetCursor.widget;
	
	WIDGET_STATE(flags.active, g_isActiveWidget);
	WIDGET_STATE(data, get(widgetCursor, widget->data));
	WIDGET_STATE(minValue, get(widgetCursor, widget->min));
	WIDGET_STATE(maxValue, get(widgetCursor, widget->max));
	WIDGET_STATE(thresholdValue, get(widgetCursor, widget->threshold));
	WIDGET_STATE(unitValue, get(widgetCursor, widget->unit));

	return !hasPreviousState;
}

void GaugeWidgetState::render() {
    const WidgetCursor &widgetCursor = g_widgetCursor;

	auto widget = (const GaugeWidget*)widgetCursor.widget;

	using namespace display;

	float min = minValue.toFloat();
	float max = maxValue.toFloat();
	float value = data.toFloat();
	float threshold = thresholdValue.toFloat();

	auto unit = unitValue.toString(0xa9ddede3).getString();

	if (isNaN(min) || isNaN(max) || isNaN(value) || isinf(min) || isinf(max) || isinf(value) || min >= max) {
		min = 0.0;
		max = 1.0f;
		value = 0.0f;
	} else {
		if (value < min) {
			value = min;
		} else if (value > max) {
			value = max;
		}
	}

	const Style* style = getStyle(widget->style);
	const Style* barStyle = getStyle(widget->barStyle);
	const Style* valueStyle = getStyle(widget->valueStyle);
	const Style* ticksStyle = getStyle(widget->ticksStyle);
	const Style* thresholdStyle = getStyle(widget->thresholdStyle);

	auto isActive = flags.active;

	// auto colorBackground = getColor16FromIndex(style->background_color);
	auto colorBorder = getColor16FromIndex(isActive ? style->activeColor : style->color);
	auto colorBar = getColor16FromIndex(isActive ? barStyle->activeColor : barStyle->color);

	auto xCenter = widgetCursor.w / 2;
	auto yCenter = widgetCursor.h - 8;

	// clear background
	setColor(isActive ? style->activeBackgroundColor : style->backgroundColor);
	fillRect(widgetCursor.x, widgetCursor.y, widgetCursor.x + widgetCursor.w - 1, widgetCursor.y + widgetCursor.h - 1);

	// init AGG
	display::AggDrawing aggDrawing;
	display::aggInit(aggDrawing);
	auto &graphics = aggDrawing.graphics;

	graphics.clipBox(widgetCursor.x, widgetCursor.y, widgetCursor.x + widgetCursor.w, widgetCursor.y + widgetCursor.h);
	graphics.translate(widgetCursor.x, widgetCursor.y);

	// draw frame
	if (style->borderSizeLeft > 0) {
		graphics.lineWidth(style->borderSizeLeft);
		graphics.lineColor(COLOR_TO_R(colorBorder), COLOR_TO_G(colorBorder), COLOR_TO_B(colorBorder));
		graphics.noFill();
		graphics.roundedRect(
			style->borderSizeLeft / 2.0,
			style->borderSizeLeft / 2.0,
            widgetCursor.w - style->borderSizeLeft,
            widgetCursor.h - style->borderSizeLeft,
			style->borderRadiusTLX, style->borderRadiusTLY, style->borderRadiusTRX, style->borderRadiusTRY,
			style->borderRadiusBRX, style->borderRadiusBRY, style->borderRadiusBLX, style->borderRadiusBLY
		);
	}

	static const int PADDING_HORZ = 56;
	static const int TICK_LINE_LENGTH = 5;
	static const int TICK_LINE_WIDTH = 1;
	static const int TICK_TEXT_GAP = 1;
	static const int THRESHOLD_LINE_WIDTH = 2;

	// draw border
	auto radBorderOuter = (widgetCursor.w - PADDING_HORZ) / 2;

	auto BORDER_WIDTH = radBorderOuter / 3;
	auto BAR_WIDTH = BORDER_WIDTH / 2;

	auto radBorderInner = radBorderOuter - BORDER_WIDTH;
	graphics.resetPath();
	graphics.noFill();
	graphics.lineColor(COLOR_TO_R(colorBorder), COLOR_TO_G(colorBorder), COLOR_TO_B(colorBorder));
	graphics.lineWidth(1.5);
	arcBar(graphics, xCenter, yCenter, radBorderOuter, radBorderInner, 0);
	graphics.drawPath();

	// draw bar
	auto radBarOuter = (widgetCursor.w - PADDING_HORZ) / 2 - (BORDER_WIDTH - BAR_WIDTH) / 2;
	auto radBarInner = radBarOuter - BAR_WIDTH;
	auto angle = remap(value, min, 180.0f, max, 0.0f);
	graphics.resetPath();
	graphics.noLine();
	graphics.fillColor(COLOR_TO_R(colorBar), COLOR_TO_G(colorBar), COLOR_TO_B(colorBar));
	graphics.lineWidth(1.5);
	arcBar(graphics, xCenter, yCenter, radBarOuter, radBarInner, angle);
	graphics.drawPath();

	// draw threshold
	auto thresholdAngleDeg = remap(threshold, min, 180.0f, max, 0);
	if (thresholdAngleDeg >= 0 && thresholdAngleDeg <= 180.0f) {
		auto thresholdAngle = Agg2D::deg2Rad(thresholdAngleDeg);
		float acos = cosf(thresholdAngle);
		float asin = sinf(thresholdAngle);
		int x1 = floorf(xCenter + radBarInner * acos);
		int y1 = floorf(yCenter - radBarInner * asin);
		int x2 = floorf(xCenter + radBarOuter * acos);
		int y2 = floorf(yCenter - radBarOuter * asin);

		graphics.resetPath();
		graphics.noFill();
		auto thresholdColor = getColor16FromIndex(isActive ? thresholdStyle->activeColor : thresholdStyle->color);
		graphics.lineColor(COLOR_TO_R(thresholdColor), COLOR_TO_G(thresholdColor), COLOR_TO_B(thresholdColor));
		graphics.lineWidth(THRESHOLD_LINE_WIDTH);
		graphics.moveTo(x1, y1);
		graphics.lineTo(x2, y2);
		graphics.drawPath();
	}

	// draw ticks
	font::Font ticksFont = styleGetFont(ticksStyle);
	auto ft = firstTick(max - min);
	auto ticksRad = radBorderOuter + 1;
	for (auto tickValue = min; tickValue <= max; tickValue += ft) {
		auto tickAngleDeg = remap(tickValue, min, 180.0f, max, 0);
		if (tickAngleDeg <= 180.0) {
			auto tickAngle = Agg2D::deg2Rad(tickAngleDeg);
			float acos = cosf(tickAngle);
			float asin = sinf(tickAngle);
			int x1 = floorf(xCenter + ticksRad * acos);
			int y1 = floorf(yCenter - ticksRad * asin);
			int x2 = floorf(xCenter + (ticksRad + TICK_LINE_LENGTH) * acos);
			int y2 = floorf(yCenter - (ticksRad + TICK_LINE_LENGTH) * asin);

			graphics.resetPath();
			graphics.noFill();
			auto tickColor = getColor16FromIndex(isActive ? ticksStyle->activeColor : ticksStyle->color);
			graphics.lineColor(COLOR_TO_R(tickColor), COLOR_TO_G(tickColor), COLOR_TO_B(tickColor));
			graphics.lineWidth(TICK_LINE_WIDTH);
			graphics.moveTo(x1, y1);
			graphics.lineTo(x2, y2);
			graphics.drawPath();

			char tickText[50];
			snprintf(tickText, sizeof(tickText), "%g", tickValue);
			if (unit && *unit) {
				stringAppendString(tickText, sizeof(tickText), " ");
				stringAppendString(tickText, sizeof(tickText), unit);
			}

			auto tickTextWidth = display::measureStr(tickText, -1, ticksFont);
			if (tickAngleDeg == 180.0) {
				drawText(
					tickText,
					-1,
					widgetCursor.x + xCenter -
						radBorderOuter -
						TICK_TEXT_GAP -
						tickTextWidth,
					widgetCursor.y + y2 - TICK_TEXT_GAP - ticksFont.getAscent(),
					tickTextWidth,
					ticksFont.getAscent(),
					ticksStyle,
					isActive
				);
			} else if (tickAngleDeg > 90.0) {
				drawText(
					tickText,
					-1,
					widgetCursor.x + x2 - TICK_TEXT_GAP - tickTextWidth,
					widgetCursor.y + y2 - TICK_TEXT_GAP - ticksFont.getAscent(),
					tickTextWidth,
					ticksFont.getAscent(),
					ticksStyle,
					isActive
				);
			} else if (tickAngleDeg == 90.0) {
				drawText(
					tickText,
					-1,
					widgetCursor.x + x2 - tickTextWidth / 2,
					widgetCursor.y + y2 - TICK_TEXT_GAP - ticksFont.getAscent(),
					tickTextWidth,
					ticksFont.getAscent(),
					ticksStyle,
					isActive
				);
			} else if (tickAngleDeg > 0) {
				drawText(
					tickText,
					-1,
					widgetCursor.x + x2 + TICK_TEXT_GAP,
					widgetCursor.y + y2 - TICK_TEXT_GAP - ticksFont.getAscent(),
					tickTextWidth,
					ticksFont.getAscent(),
					ticksStyle,
					isActive
				);
			} else {
				drawText(
					tickText,
					-1,
					widgetCursor.x + xCenter + radBorderOuter + TICK_TEXT_GAP,
					widgetCursor.y + y2 - TICK_TEXT_GAP - ticksFont.getAscent(),
					tickTextWidth,
					ticksFont.getAscent(),
					ticksStyle,
					isActive
				);
			}
		}
	}

	// draw value
	font::Font valueFont = styleGetFont(valueStyle);
	char valueText[50];
	snprintf(valueText, sizeof(valueText), "%g", value);
	if (unit && *unit) {
			stringAppendString(valueText, sizeof(valueText), " ");
			stringAppendString(valueText, sizeof(valueText), unit);
	}
	auto valueTextWidth = display::measureStr(valueText, -1, valueFont);
	drawText(
		valueText,
		-1,
		widgetCursor.x + xCenter - valueTextWidth / 2,
		widgetCursor.y + yCenter - valueFont.getHeight(),
		valueTextWidth,
		valueFont.getHeight(),
		valueStyle,
		isActive
	);
}

} // namespace gui
} // namespace eez
