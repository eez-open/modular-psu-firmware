/*
 * EEZ PSU Firmware
 * Copyright (C) 2017-present, Envox d.o.o.
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

#include <eez/apps/psu/psu.h>

#include <eez/apps/psu/persist_conf.h>
#include <eez/apps/psu/scpi/psu.h>
#if OPTION_DISPLAY
#include <eez/gui/dialogs.h>
#include <eez/gui/gui.h>
#include <eez/modules/mcu/display.h>
#include <eez/system.h>
#endif

namespace eez {
namespace psu {
namespace scpi {

scpi_result_t scpi_cmd_displayBrightness(scpi_t *context) {
    // TODO migrate to generic firmware
#if OPTION_DISPLAY
    int32_t param;
    if (!SCPI_ParamInt(context, &param, true)) {
        return SCPI_RES_ERR;
    }

    if (param < DISPLAY_BRIGHTNESS_MIN || param > DISPLAY_BRIGHTNESS_MAX) {
        SCPI_ErrorPush(context, SCPI_ERROR_DATA_OUT_OF_RANGE);
        return SCPI_RES_ERR;
    }

    persist_conf::setDisplayBrightness(param);

    return SCPI_RES_OK;
#else
    SCPI_ErrorPush(context, SCPI_ERROR_HARDWARE_MISSING);
    return SCPI_RES_ERR;
#endif
}

scpi_result_t scpi_cmd_displayBrightnessQ(scpi_t *context) {
    // TODO migrate to generic firmware
#if OPTION_DISPLAY
    SCPI_ResultInt(context, persist_conf::devConf2.displayBrightness);
    return SCPI_RES_OK;
#else
    SCPI_ErrorPush(context, SCPI_ERROR_HARDWARE_MISSING);
    return SCPI_RES_ERR;
#endif
}

scpi_result_t scpi_cmd_displayView(scpi_t *context) {
    // TODO migrate to generic firmware
#if OPTION_DISPLAY
    int32_t param;
    if (!SCPI_ParamInt(context, &param, true)) {
        return SCPI_RES_ERR;
    }

    if (param < 1 || param > 4) {
        SCPI_ErrorPush(context, SCPI_ERROR_ILLEGAL_PARAMETER_VALUE);
        return SCPI_RES_ERR;
    }

    persist_conf::setChannelsViewMode(param - 1);

    return SCPI_RES_OK;
#else
    SCPI_ErrorPush(context, SCPI_ERROR_HARDWARE_MISSING);
    return SCPI_RES_ERR;
#endif
}

scpi_result_t scpi_cmd_displayViewQ(scpi_t *context) {
    // TODO migrate to generic firmware
#if OPTION_DISPLAY
    SCPI_ResultInt(context, persist_conf::devConf.flags.channelsViewMode + 1);
    return SCPI_RES_OK;
#else
    SCPI_ErrorPush(context, SCPI_ERROR_HARDWARE_MISSING);
    return SCPI_RES_ERR;
#endif
}

scpi_result_t scpi_cmd_displayWindowState(scpi_t *context) {
    // TODO migrate to generic firmware
#if OPTION_DISPLAY
    bool onOff;
    if (!SCPI_ParamBool(context, &onOff, TRUE)) {
        return SCPI_RES_ERR;
    }

    if (!persist_conf::setDisplayState(onOff ? 1 : 0)) {
        SCPI_ErrorPush(context, SCPI_ERROR_EXECUTION_ERROR);
        return SCPI_RES_ERR;
    }

    return SCPI_RES_OK;
#else
    SCPI_ErrorPush(context, SCPI_ERROR_HARDWARE_MISSING);
    return SCPI_RES_ERR;
#endif
}

scpi_result_t scpi_cmd_displayWindowStateQ(scpi_t *context) {
    // TODO migrate to generic firmware
#if OPTION_DISPLAY
    SCPI_ResultBool(context, persist_conf::devConf2.flags.displayState);
    return SCPI_RES_OK;
#else
    SCPI_ErrorPush(context, SCPI_ERROR_HARDWARE_MISSING);
    return SCPI_RES_ERR;
#endif
}

scpi_result_t scpi_cmd_displayWindowText(scpi_t *context) {
    // TODO migrate to generic firmware
#if OPTION_DISPLAY
    const char *text;
    size_t len;
    if (!SCPI_ParamCharacters(context, &text, &len, true)) {
        return SCPI_RES_ERR;
    }

    if (len > 32) {
        SCPI_ErrorPush(context, SCPI_ERROR_TOO_MUCH_DATA);
        return SCPI_RES_ERR;
    }

    eez::gui::setTextMessage(text, len);

    return SCPI_RES_OK;
#else
    SCPI_ErrorPush(context, SCPI_ERROR_HARDWARE_MISSING);
    return SCPI_RES_ERR;
#endif
}

scpi_result_t scpi_cmd_displayWindowTextQ(scpi_t *context) {
    // TODO migrate to generic firmware
#if OPTION_DISPLAY
    SCPI_ResultText(context, eez::gui::getTextMessage());
    return SCPI_RES_OK;
#else
    SCPI_ErrorPush(context, SCPI_ERROR_HARDWARE_MISSING);
    return SCPI_RES_ERR;
#endif
}

scpi_result_t scpi_cmd_displayWindowTextClear(scpi_t *context) {
    // TODO migrate to generic firmware
#if OPTION_DISPLAY
    eez::gui::clearTextMessage();
    return SCPI_RES_OK;
#else
    SCPI_ErrorPush(context, SCPI_ERROR_HARDWARE_MISSING);
    return SCPI_RES_ERR;
#endif
}

scpi_result_t scpi_cmd_displayDataQ(scpi_t *context) {
    // TODO migrate to generic firmware
#if OPTION_DISPLAY
    const uint8_t buffer[] = {
        // BMP Header (14 bytes)

        // ID field (42h, 4Dh): BM
        0x42, 0x4D,
        // size of BMP file
        0x36, 0xFA, 0x05, 0x00, // 14 + 40 + (480 * 272 * 3) = 391734‬ = 0x5FA36
        // unused
        0x00, 0x00,
        // unused
        0x00, 0x00,
        // Offset where the pixel array (bitmap data) can be found
        0x36, 0x00, 0x00, 0x00, // 53 = 0x36

        // DIB Header (40 bytes)

        // Number of bytes in the DIB header (from this point)
        0x28, 0x00, 0x00, 0x00, // 40 = 0x28
        // Width of the bitmap in pixels
        0xE0, 0x01, 0x00, 0x00, // 480 = 0x1E0
        // Height of the bitmap in pixels. Positive for bottom to top pixel order.
        0x10, 0x01, 0x00, 0x00, // 272 = 0x110
        // Number of color planes being used
        0x01, 0x00,
        // Number of bits per pixel
        0x18, 0x00, // 24 = 0x18
        // BI_RGB, no pixel array compression used
        0x00, 0x00, 0x00, 0x00,
        // Size of the raw bitmap data (including padding)
        0x00, 0xFA, 0x05, 0x00, // 480 * 272 * 3 = 0x5FA00
        // Print resolution of the image,
        // 72 DPI × 39.3701 inches per metre yields 2834.6472
        0x13, 0x0B, 0x00, 0x00, // 2835 pixels/metre horizontal
        0x13, 0x0B, 0x00, 0x00, // 2835 pixels/metre vertical
        // Number of colors in the palette
        0x00, 0x00, 0x00, 0x00,
        // 0 means all colors are important
        0x00, 0x00, 0x00, 0x00,
    };

    SCPI_ResultArbitraryBlockHeader(context, sizeof(buffer) + 480 * 272 * 3);
    SCPI_ResultArbitraryBlockData(context, buffer, sizeof(buffer));

    mcu::display::screanshotBegin();

    uint8_t line[480 * 3];
    while (mcu::display::screanshotGetLine(line)) {
        SCPI_ResultArbitraryBlockData(context, line, sizeof(line));
        osDelay(1);
    }

    mcu::display::screanshotEnd();

    return SCPI_RES_OK;
#else
    SCPI_ErrorPush(context, SCPI_ERROR_HARDWARE_MISSING);
    return SCPI_RES_ERR;
#endif
}

} // namespace scpi
} // namespace psu
} // namespace eez