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

#include <bb3/psu/psu.h>

#include <bb3/psu/persist_conf.h>
#include <bb3/psu/scpi/psu.h>

#if OPTION_DISPLAY
#include <eez/gui/gui.h>
#include <bb3/psu/gui/psu.h>
#include <bb3/system.h>
#endif

#include <bb3/psu/sd_card.h>
#include <bb3/psu/dlog_record.h>
#include <bb3/psu/dlog_view.h>

#include <bb3/libs/image/jpeg.h>

namespace eez {
namespace psu {
namespace scpi {

scpi_result_t scpi_cmd_displayBrightness(scpi_t *context) {
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
#if OPTION_DISPLAY
    SCPI_ResultInt(context, persist_conf::devConf.displayBrightness);
    return SCPI_RES_OK;
#else
    SCPI_ErrorPush(context, SCPI_ERROR_HARDWARE_MISSING);
    return SCPI_RES_ERR;
#endif
}

scpi_result_t scpi_cmd_displayView(scpi_t *context) {
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
#if OPTION_DISPLAY
    SCPI_ResultInt(context, persist_conf::getChannelsViewMode() + 1);
    return SCPI_RES_OK;
#else
    SCPI_ErrorPush(context, SCPI_ERROR_HARDWARE_MISSING);
    return SCPI_RES_ERR;
#endif
}

scpi_result_t scpi_cmd_displayWindowState(scpi_t *context) {
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
#if OPTION_DISPLAY
    SCPI_ResultBool(context, persist_conf::devConf.displayState);
    return SCPI_RES_OK;
#else
    SCPI_ErrorPush(context, SCPI_ERROR_HARDWARE_MISSING);
    return SCPI_RES_ERR;
#endif
}

scpi_result_t scpi_cmd_displayWindowText(scpi_t *context) {
#if OPTION_DISPLAY
    const char *text;
    size_t len;
    if (!SCPI_ParamCharacters(context, &text, &len, true)) {
        return SCPI_RES_ERR;
    }

    if (len > psu::gui::PsuAppContext::MAX_TEXT_MESSAGE_LEN) {
        SCPI_ErrorPush(context, SCPI_ERROR_TOO_MUCH_DATA);
        return SCPI_RES_ERR;
    }

    psu::gui::setTextMessage(text, len);

    return SCPI_RES_OK;
#else
    SCPI_ErrorPush(context, SCPI_ERROR_HARDWARE_MISSING);
    return SCPI_RES_ERR;
#endif
}

scpi_result_t scpi_cmd_displayWindowTextQ(scpi_t *context) {
#if OPTION_DISPLAY
    SCPI_ResultText(context, psu::gui::getTextMessage());
    return SCPI_RES_OK;
#else
    SCPI_ErrorPush(context, SCPI_ERROR_HARDWARE_MISSING);
    return SCPI_RES_ERR;
#endif
}

scpi_result_t scpi_cmd_displayWindowTextClear(scpi_t *context) {
#if OPTION_DISPLAY
    psu::gui::clearTextMessage();
    return SCPI_RES_OK;
#else
    SCPI_ErrorPush(context, SCPI_ERROR_HARDWARE_MISSING);
    return SCPI_RES_ERR;
#endif
}

scpi_result_t scpi_cmd_displayDataQ(scpi_t *context) {
#if OPTION_DISPLAY
    if (dlog_record::isExecuting() && dlog_record::g_recordingParameters.period < 1.0f) {
    	SCPI_ErrorPush(context, SCPI_ERROR_EXECUTION_ERROR);
        return SCPI_RES_ERR;
    }

    const uint8_t *screenshotPixels = display::takeScreenshot();

    unsigned char* imageData;
    size_t imageDataSize;

    if (jpegEncode(screenshotPixels, &imageData, &imageDataSize)) {
    	SCPI_ErrorPush(context, SCPI_ERROR_OUT_OF_MEMORY_FOR_REQ_OP);
        display::releaseScreenshot();
    	return SCPI_RES_ERR;
    }

    SCPI_ResultArbitraryBlockHeader(context, imageDataSize);

    static const size_t CHUNK_SIZE = 1024;

    while (imageDataSize > 0) {
        WATCHDOG_RESET(WATCHDOG_LONG_OPERATION);

        size_t n = MIN(imageDataSize, CHUNK_SIZE);
        SCPI_ResultArbitraryBlockData(context, imageData, n);
        imageData += n;
        imageDataSize -= n;
    }

    display::releaseScreenshot();

    return SCPI_RES_OK;
#else
    SCPI_ErrorPush(context, SCPI_ERROR_HARDWARE_MISSING);
    return SCPI_RES_ERR;
#endif
}

scpi_result_t scpi_cmd_displayWindowHome(scpi_t *context) {
#if OPTION_DISPLAY
    psu::gui::showMainPage();
    return SCPI_RES_OK;
#else
    SCPI_ErrorPush(context, SCPI_ERROR_HARDWARE_MISSING);
    return SCPI_RES_ERR;
#endif
}

scpi_result_t scpi_cmd_displayWindowDlog(scpi_t *context) {
#if OPTION_DISPLAY
    char filePath[MAX_PATH_LENGTH + 1];
    bool isFilePathSpecified;
    if (!getFilePath(context, filePath, false, &isFilePathSpecified)) {
        return SCPI_RES_ERR;
    }

    if (isFilePathSpecified) {
        psu::dlog_view::g_showLatest = false;
        int err;
        if (!psu::dlog_view::openFile(filePath, &err)) {
            SCPI_ErrorPush(context, err);
            return SCPI_RES_ERR;
        }

    } else {
        dlog_view::g_showLatest = true;
        if (!dlog_record::isExecuting()) {
            if (dlog_record::getLatestFilePath()) {
                dlog_view::openFile(dlog_record::getLatestFilePath());
            }
        }
    }

    psu::gui::pushPage(PAGE_ID_DLOG_VIEW);
    return SCPI_RES_OK;
#else
    SCPI_ErrorPush(context, SCPI_ERROR_HARDWARE_MISSING);
    return SCPI_RES_ERR;
#endif
}

enum {
    INPUT_TYPE_TEXT,
    INPUT_TYPE_NUMBER,
    INPUT_TYPE_INTEGER,
    INPUT_TYPE_MENU
};

static scpi_choice_def_t inputTypeChoice[] = {
    { "TEXT", INPUT_TYPE_TEXT },
    { "NUMBer", INPUT_TYPE_NUMBER },
    { "INTeger", INPUT_TYPE_INTEGER },
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

    static char label[MAX_KEYPAD_LABEL_LENGTH + 1];
    memcpy(label, labelText, labelTextLen);
    if (type == INPUT_TYPE_TEXT || type == INPUT_TYPE_NUMBER) {
        if (labelTextLen > 0) {
            stringCopy(label + labelTextLen, sizeof(label) - labelTextLen, ": ");
            labelTextLen += 2;
        } else {
            label[labelTextLen] = 0;
        }
    } else {
        label[labelTextLen] = 0;
    }

    if (type == INPUT_TYPE_TEXT) {
        // min
        int32_t min;
        if (!SCPI_ParamInt(context, &min, true)) {
            return SCPI_RES_ERR;
        }

        if ((size_t)min > MAX_KEYPAD_TEXT_LENGTH - labelTextLen) {
            SCPI_ErrorPush(context, SCPI_ERROR_ILLEGAL_PARAMETER_VALUE);
            return SCPI_RES_ERR;
        }

        // max
        int32_t max;
        if (!SCPI_ParamInt(context, &max, true)) {
            return SCPI_RES_ERR;
        }

        if (max < min || (size_t)max > MAX_KEYPAD_TEXT_LENGTH - labelTextLen) {
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
        memcpy(value, valueText, valueTextLen);
        value[valueTextLen] = 0;

        const char *result = psu::gui::g_psuAppContext.textInput(label, min, max, value);
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
    } else if (type == INPUT_TYPE_INTEGER) {
        // min
        int32_t min;
        if (!SCPI_ParamInt(context, &min, true)) {
            return SCPI_RES_ERR;
        }

        // max
        int32_t max;
        if (!SCPI_ParamInt(context, &max, true)) {
            return SCPI_RES_ERR;
        }

        // value
        int32_t value;
        if (!SCPI_ParamInt(context, &value, true)) {
            return SCPI_RES_ERR;
        }

        if (value < min || value > max) {
            SCPI_ErrorPush(context, SCPI_ERROR_DATA_OUT_OF_RANGE);
            return SCPI_RES_ERR;
        }

        if (psu::gui::g_psuAppContext.integerInput(label, min, max, value)) {
            SCPI_ResultInt32(context, value);
        }
    } else {
        int32_t menuType;
        if (!SCPI_ParamChoice(context, menuTypeChoice, &menuType, true)) {
            return SCPI_RES_ERR;
        }

        using namespace eez::psu::gui;

        static const int MAX_MENU_ITEM_TEXT_LENGTH = 20;
        static char menuItemTexts[MAX_MENU_ITEMS][MAX_MENU_ITEM_TEXT_LENGTH + 1];
        static const char *menuItems[MAX_MENU_ITEMS + 1] = {};

        size_t i;
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

            memcpy(&menuItemTexts[i][0], menuItemText, menuItemTextLen);
            menuItemTexts[i][menuItemTextLen] = 0;

            menuItems[i] = &menuItemTexts[i][0];
        }

        menuItems[i] = nullptr;

        int result = psu::gui::g_psuAppContext.menuInput(label, (MenuType)menuType, menuItems);
        SCPI_ResultInt(context, result + 1);
    }

    return SCPI_RES_OK;
#else
    SCPI_ErrorPush(context, SCPI_ERROR_HARDWARE_MISSING);
    return SCPI_RES_ERR;
#endif
}

scpi_result_t scpi_cmd_displayWindowSelectQ(scpi_t *context) {
#if OPTION_DISPLAY
    int32_t defaultSelection;
    if (!SCPI_ParamInt(context, &defaultSelection, true)) {
        return SCPI_RES_ERR;
    }

    static const int MAX_OPTION_TEXTS_LENGTH = 256;
    static char optionTexts[MAX_OPTION_TEXTS_LENGTH];
    static const int MAX_OPTIONS = 8;
    static const char *options[MAX_OPTIONS + 1] = {};

    int optionTextsLen = 0;

    size_t i;
    for (i = 0; i < MAX_OPTIONS; i++) {
        const char *optionText;
        size_t optionTextLen;
        if (!SCPI_ParamCharacters(context, &optionText, &optionTextLen, i == 0 ? true : false)) {
            if (SCPI_ParamErrorOccurred(context)) {
                return SCPI_RES_ERR;
            }
            if (i < 2) {
                SCPI_ErrorPush(context, SCPI_ERROR_MISSING_PARAMETER);
                return SCPI_RES_ERR;
            }
            break;
        }

        if (optionTextsLen + optionTextLen + 1 > MAX_OPTION_TEXTS_LENGTH) {
            SCPI_ErrorPush(context, SCPI_ERROR_TOO_MUCH_DATA);
            return SCPI_RES_ERR;
        }

        options[i] = optionTexts + optionTextsLen;
        memcpy((char *)options[i], optionText, optionTextLen);
        optionTextsLen += optionTextLen;
        optionTexts[optionTextsLen++] = 0;
    }

    options[i] = nullptr;

    if (defaultSelection < 0 || defaultSelection > (int32_t)i) {
        SCPI_ErrorPush(context, SCPI_ERROR_ILLEGAL_PARAMETER_VALUE);
        return SCPI_RES_ERR;
    }

    int result = psu::gui::g_psuAppContext.select(options, defaultSelection);
    SCPI_ResultInt(context, result);

    return SCPI_RES_OK;
#else
    SCPI_ErrorPush(context, SCPI_ERROR_HARDWARE_MISSING);
    return SCPI_RES_ERR;
#endif
}

scpi_result_t scpi_cmd_displayWindowDialogOpen(scpi_t *context) {
#if OPTION_DISPLAY
    char filePath[MAX_PATH_LENGTH + 1];
    if (!getFilePath(context, filePath, true)) {
        return SCPI_RES_ERR;
    }

    int err;
    if (!eez::loadExternalAssets(filePath, &err) || !psu::gui::g_psuAppContext.dialogOpen(&err)) {
        SCPI_ErrorPush(context, err);
        return SCPI_RES_ERR;
    }

    return SCPI_RES_OK;
#else
    SCPI_ErrorPush(context, SCPI_ERROR_HARDWARE_MISSING);
    return SCPI_RES_ERR;
#endif
}

scpi_result_t scpi_cmd_displayWindowDialogActionQ(scpi_t *context) {
#if OPTION_DISPLAY
    // timeout
    uint32_t timeoutMs = 0;
    scpi_number_t param;
    if (SCPI_ParamNumber(context, scpi_special_numbers_def, &param, false)) {
        if (param.unit != SCPI_UNIT_NONE && param.unit != SCPI_UNIT_SECOND) {
            SCPI_ErrorPush(context, SCPI_ERROR_INVALID_SUFFIX);
            return SCPI_RES_ERR;
        }
        timeoutMs = (uint32_t)round(param.content.value * 1000);
    } else {
        if (SCPI_ParamErrorOccurred(context)) {
            return SCPI_RES_ERR;
        }
    }

    const char *selectedActionName;
    int result = psu::gui::g_psuAppContext.dialogAction(timeoutMs, selectedActionName);
    if (result == psu::gui::DIALOG_ACTION_RESULT_SELECTED_ACTION) {
        SCPI_ResultText(context, selectedActionName);
    } else {
        SCPI_ResultBool(context, result);
    }

    return SCPI_RES_OK;
#else
    SCPI_ErrorPush(context, SCPI_ERROR_HARDWARE_MISSING);
    return SCPI_RES_ERR;
#endif
}

scpi_choice_def_t dataTypeChoice[] = {
    { "INTeger", VALUE_TYPE_INT32 },
    { "FLOat", VALUE_TYPE_FLOAT },
    { "STRing", VALUE_TYPE_STRING },
    SCPI_CHOICE_LIST_END /* termination of option list */
};

scpi_result_t scpi_cmd_displayWindowDialogData(scpi_t *context) {
#if OPTION_DISPLAY
    const char *valueText;
    size_t valueTextLen;
    if (!SCPI_ParamCharacters(context, &valueText, &valueTextLen, true)) {
        return SCPI_RES_ERR;
    }
    if (valueTextLen > 128) {
        SCPI_ErrorPush(context, SCPI_ERROR_TOO_MUCH_DATA);
        return SCPI_RES_ERR;
    }
    char dataItemName[128 + 1];
    memcpy(dataItemName, valueText, valueTextLen);
    dataItemName[valueTextLen] = 0;

	using namespace eez::gui;
	using namespace eez::psu::gui;
	
	WidgetCursor widgetCursor;
	widgetCursor.assets = g_externalAssets;
	widgetCursor.appContext = &g_psuAppContext;
	getPageAsset(getActivePageId(), widgetCursor);
    
	int16_t dataId = getDataIdFromName(widgetCursor, dataItemName);
    
	if (dataId == 0) {
        SCPI_ErrorPush(context, SCPI_ERROR_ILLEGAL_PARAMETER_VALUE);
        return SCPI_RES_ERR;
    }

    int32_t type;
    if (!SCPI_ParamChoice(context, dataTypeChoice, &type, true)) {
        return SCPI_RES_ERR;
    }

    if (type == VALUE_TYPE_FLOAT) {
        int32_t unit;
        if (!SCPI_ParamChoice(context, unitChoice, &unit, true)) {
            return SCPI_RES_ERR;
        }

        scpi_number_t param;
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

        Value dataValue = eez::gui::MakeValue(value, (Unit)unit);
        psu::gui::g_psuAppContext.dialogSetDataItemValue(dataId, dataValue);
    } else if (type == VALUE_TYPE_INT32) {
        int32_t value;
        if (!SCPI_ParamInt(context, &value, true)) {
            return SCPI_RES_ERR;
        }

        Value dataValue(value);
        psu::gui::g_psuAppContext.dialogSetDataItemValue(dataId, dataValue);
    } else if (type == VALUE_TYPE_STRING) {
        const char *valueText;
        size_t valueTextLen;
        if (!SCPI_ParamCharacters(context, &valueText, &valueTextLen, true)) {
            return SCPI_RES_ERR;
        }
        if (valueTextLen > 128) {
            SCPI_ErrorPush(context, SCPI_ERROR_TOO_MUCH_DATA);
            return SCPI_RES_ERR;
        }
        char dataItemValue[128 + 1];
        memcpy(dataItemValue, valueText, valueTextLen);
        dataItemValue[valueTextLen] = 0;

        psu::gui::g_psuAppContext.dialogSetDataItemValue(dataId, dataItemValue);
    }

    return SCPI_RES_OK;
#else
    SCPI_ErrorPush(context, SCPI_ERROR_HARDWARE_MISSING);
    return SCPI_RES_ERR;
#endif
}

scpi_result_t scpi_cmd_displayWindowDialogClose(scpi_t *context) {
#if OPTION_DISPLAY
    psu::gui::g_psuAppContext.dialogClose();

    return SCPI_RES_OK;
#else
    SCPI_ErrorPush(context, SCPI_ERROR_HARDWARE_MISSING);
    return SCPI_RES_ERR;
#endif
}

scpi_result_t scpi_cmd_displayWindowError(scpi_t *context) {
#if OPTION_DISPLAY
    const char *valueText;
    size_t valueTextLen;
    if (!SCPI_ParamCharacters(context, &valueText, &valueTextLen, true)) {
        return SCPI_RES_ERR;
    }
    if (valueTextLen > 128) {
        SCPI_ErrorPush(context, SCPI_ERROR_TOO_MUCH_DATA);
        return SCPI_RES_ERR;
    }
    char message[128 + 1];
    memcpy(message, valueText, valueTextLen);
    message[valueTextLen] = 0;
    
    psu::gui::g_psuAppContext.errorMessage(message);

    return SCPI_RES_OK;
#else
    SCPI_ErrorPush(context, SCPI_ERROR_HARDWARE_MISSING);
    return SCPI_RES_ERR;
#endif
}

} // namespace scpi
} // namespace psu
} // namespace eez
