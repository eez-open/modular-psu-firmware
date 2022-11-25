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

#include <eez/conf-internal.h>

#if EEZ_OPTION_GUI

#include <eez/core/util.h>

#include <eez/gui/gui.h>
#include <eez/gui/widgets/qr_code.h>

#include <eez/libs/qrcodegen/qrcodegen.h>

namespace eez {
namespace gui {

bool QRCodeWidgetState::updateState() {
    WIDGET_STATE_START(QRCodeWidget);

    WIDGET_STATE(data, get(widgetCursor, widget->data));

    WIDGET_STATE_END()
}

uint8_t qrcode[qrcodegen_BUFFER_LEN_MAX];
uint8_t tempBuffer[qrcodegen_BUFFER_LEN_MAX];

void QRCodeWidgetState::render() {
    const WidgetCursor &widgetCursor = g_widgetCursor;

    auto widget = (const QRCodeWidget *)widgetCursor.widget;
    const Style *style = getStyle(widget->style);

    // Make QR code
    qrcodegen_Ecc errCorLvl;
    if (widget->errorCorrection == 0) errCorLvl = qrcodegen_Ecc_LOW;
    else if (widget->errorCorrection == 1) errCorLvl = qrcodegen_Ecc_MEDIUM;
    else if (widget->errorCorrection == 2) errCorLvl = qrcodegen_Ecc_QUARTILE;
    else errCorLvl = qrcodegen_Ecc_HIGH;

	qrcodegen_encodeText(data.getString(), tempBuffer, qrcode, errCorLvl, qrcodegen_VERSION_MIN, qrcodegen_VERSION_MAX, qrcodegen_Mask_AUTO, true);

    display::setColor(style->backgroundColor);
    display::fillRect(widgetCursor.x, widgetCursor.y, widgetCursor.x + widgetCursor.w - 1, widgetCursor.y + widgetCursor.h - 1);

    int size = qrcodegen_getSize(qrcode);
    int border = 1;

    double sizePx = 1.0 * MIN(widgetCursor.w, widgetCursor.h) / (size + 2 * border);

    double xPadding = (widgetCursor.w - sizePx * size) / 2;
    double yPadding = (widgetCursor.h - sizePx * size) / 2;

	display::AggDrawing aggDrawing;
	display::aggInit(aggDrawing);
	auto &graphics = aggDrawing.graphics;

    auto color16 = display::getColor16FromIndex(style->color);

    graphics.resetPath();

    for (int y = 0; y < size; y++) {
		for (int x = 0; x < size; x++) {
            if (qrcodegen_getModule(qrcode, x, y)) {
                graphics.moveTo(
                    widgetCursor.x + xPadding + x * sizePx,
                    widgetCursor.y + yPadding + y * sizePx
                );
                graphics.horLineRel(sizePx);
                graphics.verLineRel(sizePx);
                graphics.horLineRel(-sizePx);
                graphics.closePolygon();
            }
		}
	}

    graphics.fillColor(COLOR_TO_R(color16), COLOR_TO_G(color16), COLOR_TO_B(color16));
    graphics.noLine();
    graphics.drawPath();
}

} // namespace gui
} // namespace eez

#endif // EEZ_OPTION_GUI
