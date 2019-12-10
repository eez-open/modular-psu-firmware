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

#if OPTION_DISPLAY
#include <eez/gui/dialogs.h>
#include <eez/gui/gui.h>
#include <eez/gui/dialogs.h>
#include <eez/modules/psu/gui/psu.h>
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

enum {
    INPUT_TYPE_TEXT,
    INPUT_TYPE_NUMBER,
    INPUT_TYPE_MENU
};

static scpi_choice_def_t inputTypeChoice[] = {
    { "TEXT", INPUT_TYPE_TEXT },
    { "NUMBer", INPUT_TYPE_NUMBER },
    { "MENU", INPUT_TYPE_MENU },
    SCPI_CHOICE_LIST_END /* termination of option list */
};

static scpi_choice_def_t menuTypeChoice[] = {
    { "BUTTon", eez::gui::MENU_TYPE_BUTTON },
    SCPI_CHOICE_LIST_END /* termination of option list */
};

scpi_result_t scpi_cmd_displayWindowInputQ(scpi_t *context) {
#if OPTION_DISPLAY
    const char *labelText;
    size_t labelTextLen;
    if (!SCPI_ParamCharacters(context, &labelText, &labelTextLen, true)) {
        return SCPI_RES_ERR;
    }

    if (labelTextLen > MAX_KEYPAD_LABEL_LENGTH - 2) {
        SCPI_ErrorPush(context, SCPI_ERROR_TOO_MUCH_DATA);
        return SCPI_RES_ERR;
    }

    int32_t type;
    if (!SCPI_ParamChoice(context, inputTypeChoice, &type, true)) {
        return SCPI_RES_ERR;
    }

    char label[MAX_KEYPAD_LABEL_LENGTH + 1];
    strncpy(label, labelText, labelTextLen);
    if (type == INPUT_TYPE_TEXT || type == INPUT_TYPE_NUMBER) {
        strcpy(label + labelTextLen, ": ");
        labelTextLen += 2;
    } else {
        label[labelTextLen] = 0;
    }

    if (type == INPUT_TYPE_TEXT) {
        int32_t max;
        if (!SCPI_ParamInt(context, &max, true)) {
            return SCPI_RES_ERR;
        }

        if ((size_t)max > MAX_KEYPAD_TEXT_LENGTH - labelTextLen) {
            SCPI_ErrorPush(context, SCPI_ERROR_ILLEGAL_PARAMETER_VALUE);
            return SCPI_RES_ERR;
        }

        const char *valueText;
        size_t valueTextLen;
        if (!SCPI_ParamCharacters(context, &valueText, &valueTextLen, true)) {
            return SCPI_RES_ERR;
        }

        if (valueTextLen > (size_t)max) {
            SCPI_ErrorPush(context, SCPI_ERROR_TOO_MUCH_DATA);
            return SCPI_RES_ERR;
        }

        char value[MAX_KEYPAD_TEXT_LENGTH + 1];
        strncpy(value, valueText, valueTextLen);
        value[valueTextLen] = 0;

        const char *result = psu::gui::g_psuAppContext.textInput(label, max, value);
        if (result) {
            SCPI_ResultText(context, result);
        }
    } else if (type == INPUT_TYPE_NUMBER)  {
        int32_t unit;
        if (!SCPI_ParamChoice(context, unitChoice, &unit, true)) {
            return SCPI_RES_ERR;
        }
 
        scpi_number_t param;
 
        // min
        if (!SCPI_ParamNumber(context, scpi_special_numbers_def, &param, true)) {
            return SCPI_RES_ERR;
        }
 
        float min;
 
        if (param.special) {
            SCPI_ErrorPush(context, SCPI_ERROR_ILLEGAL_PARAMETER_VALUE);
            return SCPI_RES_ERR;
        } else {
            if (param.unit != SCPI_UNIT_NONE && param.unit != getScpiUnit((Unit)unit)) {
                SCPI_ErrorPush(context, SCPI_ERROR_INVALID_SUFFIX);
                return SCPI_RES_ERR;
            }
 
            min = (float)param.content.value;
        }
 
        // max
        if (!SCPI_ParamNumber(context, scpi_special_numbers_def, &param, true)) {
            return SCPI_RES_ERR;
        }
 
        float max;
 
        if (param.special) {
            SCPI_ErrorPush(context, SCPI_ERROR_ILLEGAL_PARAMETER_VALUE);
            return SCPI_RES_ERR;
        } else {
            if (param.unit != SCPI_UNIT_NONE && param.unit != getScpiUnit((Unit)unit)) {
                SCPI_ErrorPush(context, SCPI_ERROR_INVALID_SUFFIX);
                return SCPI_RES_ERR;
            }
 
            max = (float)param.content.value;
        }
 
        // value
        if (!SCPI_ParamNumber(context, scpi_special_numbers_def, &param, true)) {
            return SCPI_RES_ERR;
        }
 
        float value;
 
        if (param.special) {
            SCPI_ErrorPush(context, SCPI_ERROR_ILLEGAL_PARAMETER_VALUE);
            return SCPI_RES_ERR;
        } else {
            if (param.unit != SCPI_UNIT_NONE && param.unit != getScpiUnit((Unit)unit)) {
                SCPI_ErrorPush(context, SCPI_ERROR_INVALID_SUFFIX);
                return SCPI_RES_ERR;
            }
 
            value = (float)param.content.value;
        }
 
        if (value < min || value > max) {
            SCPI_ErrorPush(context, SCPI_ERROR_DATA_OUT_OF_RANGE);
            return SCPI_RES_ERR;
        }
 
        float result = psu::gui::g_psuAppContext.numberInput(label, (Unit)unit, min, max, value);
        if (!isNaN(result)) {
            SCPI_ResultFloat(context, result);
        }        
    } else {
        int32_t menuType;
        if (!SCPI_ParamChoice(context, menuTypeChoice, &menuType, true)) {
            return SCPI_RES_ERR;
        }

        static const int MAX_MENU_ITEM_TEXT_LENGTH = 20;
        static char menuItemTexts[MAX_MENU_ITEMS][MAX_MENU_ITEM_TEXT_LENGTH + 1];
        static const char *menuItems[MAX_MENU_ITEMS + 1] = {};

        int i;
        for (i = 0; i < MAX_MENU_ITEMS; i++) {
            const char *menuItemText;
            size_t menuItemTextLen;
            if (!SCPI_ParamCharacters(context, &menuItemText, &menuItemTextLen, i == 0 ? true : false)) {
                if (i == 0 || SCPI_ParamErrorOccurred(context)) {
                    return SCPI_RES_ERR;
                }
                break;
            }

            if (menuItemTextLen > MAX_MENU_ITEM_TEXT_LENGTH) {
                SCPI_ErrorPush(context, SCPI_ERROR_TOO_MUCH_DATA);
                return SCPI_RES_ERR;
            }

            strncpy(&menuItemTexts[i][0], menuItemText, menuItemTextLen);
            menuItemTexts[i][menuItemTextLen] = 0;

            menuItems[i] = &menuItemTexts[i][0];
        }

        menuItems[i] = nullptr;

        int result = psu::gui::g_psuAppContext.menuInput(label, (eez::gui::MenuType)menuType, menuItems);
        SCPI_ResultInt(context, result + 1);
    }

    return SCPI_RES_OK;
#else
    SCPI_ErrorPush(context, SCPI_ERROR_HARDWARE_MISSING);
    return SCPI_RES_ERR;
#endif
}

} // namespace scpi
} // namespace psu
} // namespace eez
