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

#include <eez/modules/psu/psu.h>

#include <eez/modules/psu/persist_conf.h>
#include <eez/modules/psu/scpi/psu.h>
#include <eez/modules/psu/gui/psu.h>

#if OPTION_DISPLAY
#include <eez/gui/dialogs.h>
#include <eez/gui/gui.h>
#include <eez/modules/mcu/display.h>
#include <eez/system.h>
#endif

#if OPTION_SD_CARD
#include <eez/modules/psu/dlog_view.h>
#endif

#include <eez/libs/image/jpeg.h>

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
    SCPI_ResultInt(context, persist_conf::devConf.displayBrightness);
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

    if (param < 1 || param > 5) {
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
    SCPI_ResultInt(context, persist_conf::getChannelsViewMode() + 1);
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

    persist_conf::setDisplayState(onOff ? 1 : 0);

    return SCPI_RES_OK;
#else
    SCPI_ErrorPush(context, SCPI_ERROR_HARDWARE_MISSING);
    return SCPI_RES_ERR;
#endif
}

scpi_result_t scpi_cmd_displayWindowStateQ(scpi_t *context) {
    // TODO migrate to generic firmware
#if OPTION_DISPLAY
    SCPI_ResultBool(context, persist_conf::devConf.displayState);
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

    eez::psu::gui::g_psuAppContext.setTextMessage(text, len);

    return SCPI_RES_OK;
#else
    SCPI_ErrorPush(context, SCPI_ERROR_HARDWARE_MISSING);
    return SCPI_RES_ERR;
#endif
}

scpi_result_t scpi_cmd_displayWindowTextQ(scpi_t *context) {
    // TODO migrate to generic firmware
#if OPTION_DISPLAY
    SCPI_ResultText(context, eez::psu::gui::g_psuAppContext.getTextMessage());
    return SCPI_RES_OK;
#else
    SCPI_ErrorPush(context, SCPI_ERROR_HARDWARE_MISSING);
    return SCPI_RES_ERR;
#endif
}

scpi_result_t scpi_cmd_displayWindowTextClear(scpi_t *context) {
    // TODO migrate to generic firmware
#if OPTION_DISPLAY
    eez::psu::gui::g_psuAppContext.clearTextMessage();
    return SCPI_RES_OK;
#else
    SCPI_ErrorPush(context, SCPI_ERROR_HARDWARE_MISSING);
    return SCPI_RES_ERR;
#endif
}

scpi_result_t scpi_cmd_displayDataQ(scpi_t *context) {
    // TODO migrate to generic firmware
#if OPTION_DISPLAY
    const uint8_t *screenshotPixels = mcu::display::takeScreenshot();

    unsigned char* imageData;
    size_t imageDataSize;

    if (jpegEncode(screenshotPixels, &imageData, &imageDataSize)) {
    	SCPI_ErrorPush(context, SCPI_ERROR_OUT_OF_MEMORY_FOR_REQ_OP);
    	return SCPI_RES_ERR;
    }

    SCPI_ResultArbitraryBlockHeader(context, imageDataSize);

    static const size_t CHUNK_SIZE = 1024;

    while (imageDataSize > 0) {
        size_t n = MIN(imageDataSize, CHUNK_SIZE);
        SCPI_ResultArbitraryBlockData(context, imageData, n);
        imageData += n;
        imageDataSize -= n;
    }

    return SCPI_RES_OK;
#else
    SCPI_ErrorPush(context, SCPI_ERROR_HARDWARE_MISSING);
    return SCPI_RES_ERR;
#endif
}

scpi_result_t scpi_cmd_displayWindowDlog(scpi_t *context) {
#if OPTION_DISPLAY && OPTION_SD_CARD
    dlog_view::g_showLatest = true;
    psu::gui::g_psuAppContext.pushPageOnNextIter(PAGE_ID_DLOG_VIEW);
    return SCPI_RES_OK;
#else
    SCPI_ErrorPush(context, SCPI_ERROR_HARDWARE_MISSING);
    return SCPI_RES_ERR;
#endif
}

} // namespace scpi
} // namespace psu
} // namespace eez
