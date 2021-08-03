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

#if OPTION_DISPLAY

#include <math.h>

#include <eez/alloc.h>
#include <eez/util.h>

#include <eez/gui/gui.h>
#include <eez/gui/data.h>

#include <agg2d.h>
#include <agg_rendering_buffer.h>

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
		radInner, // rx
		radInner, // ry
		Agg2D::deg2Rad(-180), // angle
		0, // largeArcFlag
		0, // sweepFlag
		xCenter - radInner, // dx
		yCenter // dy
	);

	graphics.lineTo(xCenter - radOuter, yCenter);
}

////////////////////////////////////////////////////////////////////////////////

struct GaugeWidget : public Widget {
    int16_t min;
	int16_t max;
	int16_t threashold;
    int16_t barStyle;
	int16_t valueStyle;
    int16_t ticksStyle;
	int16_t thresholdStyle;
};

EnumFunctionType GAUGE_enum = nullptr;

DrawFunctionType GAUGE_draw = [](const WidgetCursor &widgetCursor) {
	using namespace mcu::display;

	static const int BORDER_WIDTH = 32;
	static const int BAR_WIDTH = 16;
	
	auto widget = (const GaugeWidget*)widgetCursor.widget;

    widgetCursor.currentState->size = sizeof(WidgetState);

    widgetCursor.currentState->data = get(widgetCursor, widget->data);

    bool refresh = !widgetCursor.previousState || widgetCursor.previousState->data != widgetCursor.currentState->data;

    if (refresh) {
		float min = get(widgetCursor, widget->min).toFloat();
		float max = get(widgetCursor, widget->max).toFloat();
		float value = widgetCursor.currentState->data.getFloat();

		const Style* style = getStyle(widget->style);
		const Style* barStyle = getStyle(widget->barStyle);

		// auto colorBackground = getColor16FromIndex(style->background_color);
		auto colorBorder = getColor16FromIndex(style->color);
		auto colorBar = getColor16FromIndex(barStyle->color);

		auto xCenter = widget->w / 2;
		auto yCenter = widget->h - 12;

		// init AGG
		agg::rendering_buffer rbuf;

		// uint8_t *pixels = (uint8_t *)alloc(widget->w * widget->h * 4);
		// rbuf.attach(pixels, widget->w, widget->h, widget->w * 4);


#ifdef EEZ_PLATFORM_STM32
		rbuf.attach((uint8_t *)getBufferPointer(), getDisplayWidth(), getDisplayHeight(), getDisplayWidth() * 2);
#else
		rbuf.attach((uint8_t *)getBufferPointer(), getDisplayWidth(), getDisplayHeight(), getDisplayWidth() * 4);
#endif

		Agg2D graphics;
		graphics.attach(rbuf.buf(), rbuf.width(), rbuf.height(), rbuf.stride());

		graphics.clipBox(widgetCursor.x, widgetCursor.y, widgetCursor.x + widget->w, widgetCursor.y + widget->h);
		graphics.translate(widgetCursor.x, widgetCursor.y);

		// clear background
		//graphics.clearClipBox(COLOR_TO_R(colorBackground), COLOR_TO_G(colorBackground), COLOR_TO_B(colorBackground));
		//graphics.clearAll(COLOR_TO_R(colorBackground), COLOR_TO_G(colorBackground), COLOR_TO_B(colorBackground));
		setColor(style->background_color);
		fillRect(widgetCursor.x, widgetCursor.y, widgetCursor.x + widget->w - 1, widgetCursor.y + widget->h - 1, 0);

		// frame
		graphics.lineWidth(1.5);
		graphics.lineColor(COLOR_TO_R(colorBorder), COLOR_TO_G(colorBorder), COLOR_TO_B(colorBorder));
		graphics.noFill();
		graphics.roundedRect(1, 1, widget->w - 2, widget->h - 2, 4);

		// draw border
		auto radBorderOuter = (widget->w - 12) / 2;
		auto radBorderInner = radBorderOuter - BORDER_WIDTH;
		graphics.resetPath();
		graphics.noFill();
		graphics.lineColor(COLOR_TO_R(colorBorder), COLOR_TO_G(colorBorder), COLOR_TO_B(colorBorder));
		graphics.lineWidth(1.5);
		arcBar(graphics, xCenter, yCenter, radBorderOuter, radBorderInner, 0);
		graphics.drawPath();

		// draw bar
		auto radBarOuter = (widget->w - 12) / 2 - (BORDER_WIDTH - BAR_WIDTH) / 2;
		auto radBarInner = radBarOuter - BAR_WIDTH;
		auto angle = remap(value, min, 180.0f, max, 0.0f);
		if (!isNaN(angle)) {
			graphics.resetPath();
			graphics.noLine();
			graphics.fillColor(COLOR_TO_R(colorBar), COLOR_TO_G(colorBar), COLOR_TO_B(colorBar));
			graphics.lineWidth(1.5);
			arcBar(graphics, xCenter, yCenter, radBarOuter, radBarInner, angle);
			graphics.drawPath();
		}

		// // output generated image and free resources
		// Image image;
		// image.width = widget->w;
		// image.height = widget->h;
		// image.pixels = pixels;
		// image.bpp = 32;
		// image.lineOffset = 0;
		// mcu::display::drawBitmap(&image, widgetCursor.x, widgetCursor.y);
		// free(pixels);
    }
};

OnTouchFunctionType GAUGE_onTouch = nullptr;

OnKeyboardFunctionType GAUGE_onKeyboard = nullptr;

} // namespace gui
} // namespace eez

#endif
