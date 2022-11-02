/*
 * EEZ Modular Firmware
 * Copyright (C) 2022-present, Envox d.o.o.
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

#pragma once

#include <eez/conf.h>

namespace eez {
namespace gui {

struct Hooks {
    int (*getExtraLongTouchAction)();
    float (*getDefaultAnimationDuration)();
    void (*executeExternalAction)(const WidgetCursor &widgetCursor, int16_t actionId, void *param);
    void (*externalData)(int16_t id, DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value);
    OnTouchFunctionType (*getWidgetTouchFunction)(const WidgetCursor &widgetCursor);
    Page *(*getPageFromId)(int pageId);
    void (*setFocusCursor)(const WidgetCursor& cursor, int16_t dataId);
    void (*stateManagment)();
    bool (*activePageHasBackdrop)();
    void (*executeActionThread)();
    bool (*isEventHandlingDisabled)();
    int (*overrideStyle)(const WidgetCursor &widgetCursor, int styleId);
    uint16_t (*overrideStyleColor)(const WidgetCursor &widgetCursor, const Style *style);
    uint16_t (*overrideActiveStyleColor)(const WidgetCursor &widgetCursor, const Style *style);
    uint16_t (*transformColor)(uint16_t color);
    bool (*styleGetSmallerFont)(font::Font &font);
    uint8_t (*getDisplayBackgroundLuminosityStep)();
    uint8_t (*getSelectedThemeIndex)();
    void (*turnOnDisplayStart)();
    void (*turnOnDisplayTick)();
    void (*turnOffDisplayStart)();
    void (*turnOffDisplayTick)();
    void (*toastMessagePageOnEncoder)(ToastMessagePage *toast, int counter);
#if OPTION_TOUCH_CALIBRATION
    void (*onEnterTouchCalibration)();
    void (*onTouchCalibrationOk)();
    void (*onTouchCalibrationCancel)();
    void (*onTouchCalibrationConfirm)();
    void (*getTouchScreenCalibrationParams)(
        int16_t &touchScreenCalTlx, int16_t &touchScreenCalTly,
        int16_t &touchScreenCalBrx, int16_t &touchScreenCalBry,
        int16_t &touchScreenCalTrx, int16_t &touchScreenCalTry
    );
    void (*setTouchScreenCalibrationParams)(
        int16_t touchScreenCalTlx, int16_t touchScreenCalTly,
        int16_t touchScreenCalBrx, int16_t touchScreenCalBry,
        int16_t touchScreenCalTrx, int16_t touchScreenCalTry
    );
#endif
    void (*onGuiQueueMessage)(uint8_t type, int16_t param);
};

extern Hooks g_hooks;

} // namespace gui
} // namespace eez
