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
    auto widget = (const GaugeWidget*)widgetCursor.widget;

    widgetCursor.currentState->size = sizeof(WidgetState);

    widgetCursor.currentState->data = get(widgetCursor, widget->data);

    bool refresh = !widgetCursor.previousState || widgetCursor.previousState->data != widgetCursor.currentState->data;

    if (refresh) {
		const Style* style = getStyle(widget->style);

		mcu::display::setColor(style->background_color);
		mcu::display::fillRect(widgetCursor.x, widgetCursor.y, widgetCursor.x + widget->w - 1, widgetCursor.y + widget->h - 1, 0);

		static const int BORDER_WIDTH = 32;
		static const int BAR_WIDTH = 16;
		float min = get(widgetCursor, widget->min).toFloat();
		float max = get(widgetCursor, widget->max).toFloat();

		const Style* barStyle = getStyle(widget->barStyle);

		//mcu::display::setColor(barStyle->color);
		//fillArcBar(
		//	widgetCursor.x + widget->w / 2,
		//	widgetCursor.y + widget->h - 4,
		//	(widget->w - 8) / 2 - BORDER_WIDTH / 2,
		//	remap(widgetCursor.currentState->data.getFloat(), min, 180.0f, max, 0.0f),
		//	180.0f,
		//	BAR_WIDTH
		//);

		//mcu::display::setColor(style->color);
		//drawArcBar(
		//	widgetCursor.x + widget->w / 2,
		//	widgetCursor.y + widget->h - 4,
		//	(widget->w - 8) / 2 - BORDER_WIDTH / 2,
		//	0.0f,
		//	180.0f,
		//	BORDER_WIDTH
		//);

		uint8_t *pixels = (uint8_t *)alloc(widget->w * widget->h * 4);

		agg::rendering_buffer rbuf;
		rbuf.attach(pixels, widget->w, widget->h, widget->w * 4);

		Agg2D m_graphics;

		m_graphics.attach(rbuf.buf(), rbuf.width(), rbuf.height(), rbuf.stride());

		static int d = 0;
		static int dir = 1;
		d += dir * 2;
		if (d > 100) {
			d = 100;
			dir = -1;
		}
		if (d < 0) {
			d = 0;
			dir = 1;
		}

    	m_graphics.viewport(
			0, 0, 600, 600,
			0 + d, 0 + d, widget->w - d, widget->h - d,
            //Agg2D::Anisotropic
            Agg2D::XMidYMid
		);

		using namespace mcu::display;

		auto colorBackground = getColor16FromIndex(style->background_color);
		auto colorBorder = getColor16FromIndex(style->color);

		auto xCenter = widget->w / 2;
		auto yCenter = widget->h / 2;
		auto rad = 50;

		//// clear background
		//m_graphics.clearAll(COLOR_TO_R(colorBackground), COLOR_TO_G(colorBackground), COLOR_TO_B(colorBackground));

		//// draw border
		//m_graphics.resetPath();
		//m_graphics.noFill();
		//m_graphics.lineColor(COLOR_TO_R(colorBorder), COLOR_TO_G(colorBorder), COLOR_TO_B(colorBorder));
		//m_graphics.lineWidth(8);
		//m_graphics.moveTo(20, 100);
		//m_graphics.arcRel(
		//	50, // rx
		//	50, // ry
		//	Agg2D::deg2Rad(35), // angle
		//	0, // largeArcFlag
		//	1, // sweepFlag
		//	100, // dx
		//	0 // dy
		//);
		//m_graphics.drawPath();

		{
			m_graphics.clearAll(255, 255, 255);


			// Rounded Rect
			m_graphics.lineColor(0, 0, 0);
			m_graphics.noFill();
			m_graphics.roundedRect(0.5, 0.5, 600 - 0.5, 600 - 0.5, 20.0);


			m_graphics.fillColor(0, 0, 0);
			m_graphics.noLine();

			m_graphics.lineColor(50, 0, 0);
			m_graphics.fillColor(180, 200, 100);
			m_graphics.lineWidth(1.0);

			// Text Alignment
			m_graphics.line(250.5 - 150, 150.5, 250.5 + 150, 150.5);
			m_graphics.line(250.5, 150.5 - 20, 250.5, 150.5 + 20);
			m_graphics.line(250.5 - 150, 200.5, 250.5 + 150, 200.5);
			m_graphics.line(250.5, 200.5 - 20, 250.5, 200.5 + 20);
			m_graphics.line(250.5 - 150, 250.5, 250.5 + 150, 250.5);
			m_graphics.line(250.5, 250.5 - 20, 250.5, 250.5 + 20);
			m_graphics.line(250.5 - 150, 300.5, 250.5 + 150, 300.5);
			m_graphics.line(250.5, 300.5 - 20, 250.5, 300.5 + 20);
			m_graphics.line(250.5 - 150, 350.5, 250.5 + 150, 350.5);
			m_graphics.line(250.5, 350.5 - 20, 250.5, 350.5 + 20);
			m_graphics.line(250.5 - 150, 400.5, 250.5 + 150, 400.5);
			m_graphics.line(250.5, 400.5 - 20, 250.5, 400.5 + 20);
			m_graphics.line(250.5 - 150, 450.5, 250.5 + 150, 450.5);
			m_graphics.line(250.5, 450.5 - 20, 250.5, 450.5 + 20);
			m_graphics.line(250.5 - 150, 500.5, 250.5 + 150, 500.5);
			m_graphics.line(250.5, 500.5 - 20, 250.5, 500.5 + 20);
			m_graphics.line(250.5 - 150, 550.5, 250.5 + 150, 550.5);
			m_graphics.line(250.5, 550.5 - 20, 250.5, 550.5 + 20);


			m_graphics.fillColor(100, 50, 50);
			m_graphics.noLine();

			// Gradients (Aqua Buttons)
			//=======================================
			double xb1 = 400;
			double yb1 = 80;
			double xb2 = xb1 + 150;
			double yb2 = yb1 + 36;

			m_graphics.fillColor(Agg2D::Color(0, 50, 180, 180));
			m_graphics.lineColor(Agg2D::Color(0, 0, 80, 255));
			m_graphics.lineWidth(1.0);
			m_graphics.roundedRect(xb1, yb1, xb2, yb2, 12, 18);

			m_graphics.lineColor(Agg2D::Color(0, 0, 0, 0));
			m_graphics.fillLinearGradient(xb1, yb1, xb1, yb1 + 30,
				Agg2D::Color(100, 200, 255, 255),
				Agg2D::Color(255, 255, 255, 0));
			m_graphics.roundedRect(xb1 + 3, yb1 + 2.5, xb2 - 3, yb1 + 30, 9, 18, 1, 1);

			m_graphics.fillColor(Agg2D::Color(0, 0, 50, 200));
			m_graphics.noLine();

			m_graphics.fillLinearGradient(xb1, yb2 - 20, xb1, yb2 - 3,
				Agg2D::Color(0, 0, 255, 0),
				Agg2D::Color(100, 255, 255, 255));
			m_graphics.roundedRect(xb1 + 3, yb2 - 20, xb2 - 3, yb2 - 2, 1, 1, 9, 18);


			// Aqua Button Pressed
			xb1 = 400;
			yb1 = 30;
			xb2 = xb1 + 150;
			yb2 = yb1 + 36;

			m_graphics.fillColor(Agg2D::Color(0, 50, 180, 180));
			m_graphics.lineColor(Agg2D::Color(0, 0, 0, 255));
			m_graphics.lineWidth(2.0);
			m_graphics.roundedRect(xb1, yb1, xb2, yb2, 12, 18);

			m_graphics.lineColor(Agg2D::Color(0, 0, 0, 0));
			m_graphics.fillLinearGradient(xb1, yb1 + 2, xb1, yb1 + 25,
				Agg2D::Color(60, 160, 255, 255),
				Agg2D::Color(100, 255, 255, 0));
			m_graphics.roundedRect(xb1 + 3, yb1 + 2.5, xb2 - 3, yb1 + 30, 9, 18, 1, 1);

			m_graphics.fillColor(Agg2D::Color(0, 0, 50, 255));
			m_graphics.noLine();

			m_graphics.fillLinearGradient(xb1, yb2 - 25, xb1, yb2 - 5,
				Agg2D::Color(0, 180, 255, 0),
				Agg2D::Color(0, 200, 255, 255));
			m_graphics.roundedRect(xb1 + 3, yb2 - 25, xb2 - 3, yb2 - 2, 1, 1, 9, 18);




			// Basic Shapes -- Ellipse
			//===========================================
			m_graphics.lineWidth(3.5);
			m_graphics.lineColor(20, 80, 80);
			m_graphics.fillColor(200, 255, 80, 200);
			m_graphics.ellipse(450, 200, 50, 90);


			// Paths
			//===========================================
			m_graphics.resetPath();
			m_graphics.fillColor(255, 0, 0, 100);
			m_graphics.lineColor(0, 0, 255, 100);
			m_graphics.lineWidth(2);
			m_graphics.moveTo(300 / 2, 200 / 2);
			m_graphics.horLineRel(-150 / 2);
			m_graphics.arcRel(150 / 2, 150 / 2, 0, 1, 0, 150 / 2, -150 / 2);
			m_graphics.closePolygon();
			m_graphics.drawPath();

			m_graphics.resetPath();
			m_graphics.fillColor(255, 255, 0, 100);
			m_graphics.lineColor(0, 0, 255, 100);
			m_graphics.lineWidth(2);
			m_graphics.moveTo(275 / 2, 175 / 2);
			m_graphics.verLineRel(-150 / 2);
			m_graphics.arcRel(150 / 2, 150 / 2, 0, 0, 0, -150 / 2, 150 / 2);
			m_graphics.closePolygon();
			m_graphics.drawPath();


			m_graphics.resetPath();
			m_graphics.noFill();
			m_graphics.lineColor(127, 0, 0);
			m_graphics.lineWidth(5);
			m_graphics.moveTo(600 / 2, 350 / 2);
			m_graphics.lineRel(50 / 2, -25 / 2);
			m_graphics.arcRel(25 / 2, 25 / 2, Agg2D::deg2Rad(-30), 0, 1, 50 / 2, -25 / 2);
			m_graphics.lineRel(50 / 2, -25 / 2);
			m_graphics.arcRel(25 / 2, 50 / 2, Agg2D::deg2Rad(-30), 0, 1, 50 / 2, -25 / 2);
			m_graphics.lineRel(50 / 2, -25 / 2);
			m_graphics.arcRel(25 / 2, 75 / 2, Agg2D::deg2Rad(-30), 0, 1, 50 / 2, -25 / 2);
			m_graphics.lineRel(50, -25);
			m_graphics.arcRel(25 / 2, 100 / 2, Agg2D::deg2Rad(-30), 0, 1, 50 / 2, -25 / 2);
			m_graphics.lineRel(50 / 2, -25 / 2);
			m_graphics.drawPath();


			// Master Alpha. From now on everything will be translucent
			//===========================================
			m_graphics.masterAlpha(0.85);


			// Transform image to the destination path.
			// The scale is determined by a rectangle
			//-----------------
			m_graphics.resetPath();
			m_graphics.moveTo(450, 200);
			m_graphics.cubicCurveTo(595, 220, 575, 350, 595, 350);
			m_graphics.lineTo(470, 340);

			// Add/Sub/Contrast Blending Modes
			m_graphics.noLine();
			m_graphics.fillColor(70, 70, 0);
			m_graphics.blendMode(Agg2D::BlendAdd);
			m_graphics.ellipse(500, 280, 20, 40);

			m_graphics.fillColor(255, 255, 255);
			m_graphics.blendMode(Agg2D::BlendOverlay);
			m_graphics.ellipse(500 + 40, 280, 20, 40);



			// Radial gradient.
			m_graphics.blendMode(Agg2D::BlendAlpha);
			m_graphics.fillRadialGradient(400, 500, 40,
				Agg2D::Color(255, 255, 0, 0),
				Agg2D::Color(0, 0, 127),
				Agg2D::Color(0, 255, 0, 0));
			m_graphics.ellipse(400, 500, 40, 40);
		}

		Image image;
		image.width = widget->w;
		image.height = widget->h;
		image.pixels = pixels;
		image.bpp = 32;
		image.lineOffset = 0;
		mcu::display::drawBitmap(&image, widgetCursor.x, widgetCursor.y);

		free(pixels);
    }
};

OnTouchFunctionType GAUGE_onTouch = nullptr;

OnKeyboardFunctionType GAUGE_onKeyboard = nullptr;

} // namespace gui
} // namespace eez

#endif
