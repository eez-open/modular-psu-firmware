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

#include <eez/conf-internal.h>

#if EEZ_OPTION_GUI

#include <eez/gui/gui.h>
#include <eez/gui/display-private.h>
#if OPTION_KEYPAD
#include <eez/gui/keypad.h>
#endif
#include <eez/gui/touch_calibration.h>

#include <eez/flow/flow.h>

namespace eez {
namespace gui {

static int getExtraLongTouchAction() {
    return ACTION_ID_NONE;
}

static float getDefaultAnimationDuration() {
    return 0;
}

static void executeExternalAction(const WidgetCursor &widgetCursor, int16_t actionId, void *param) {
    flow::executeFlowAction(widgetCursor, actionId, param);
}

static void externalData(int16_t id, DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    flow::dataOperation(id, operation, widgetCursor, value);
}

static OnTouchFunctionType getWidgetTouchFunctionHook(const WidgetCursor &widgetCursor) {
#if OPTION_KEYPAD
	auto data = widgetCursor.widget->data < 0 ? (g_mainAssets->flowDefinition ? flow::getNativeVariableId(widgetCursor) : DATA_ID_NONE) : widgetCursor.widget->data;
	if (data == DATA_ID_KEYPAD_TEXT) {
        return eez::gui::onKeypadTextTouch;
    }
#endif
    return nullptr;
}

static Page *getPageFromId(int pageId) {
    return nullptr;
}

static void setFocusCursor(const WidgetCursor& cursor, int16_t dataId) {
}

static void stateManagment() {
#if defined(EEZ_PLATFORM_SIMULATOR) || defined(__EMSCRIPTEN__)
	getAppContextFromId(APP_CONTEXT_ID_SIMULATOR_FRONT_PANEL)->stateManagment();
#endif
	getAppContextFromId(APP_CONTEXT_ID_DEVICE)->stateManagment();
}

static bool activePageHasBackdrop() {
	if (getAppContextFromId(APP_CONTEXT_ID_DEVICE)->getActivePageId() == INTERNAL_PAGE_ID_TOAST_MESSAGE) {
        ToastMessagePage *page = (ToastMessagePage *)getAppContextFromId(APP_CONTEXT_ID_DEVICE)->getActivePage();
        return page->hasAction();
	}
	return true;
}

static void executeActionThread() {
    // why is this required?
    osDelay(1);
}

static bool isEventHandlingDisabled() {
    return false;
}

static uint16_t overrideStyleColor(const WidgetCursor &widgetCursor, const Style *style) {
    return style->color;
}

static uint16_t overrideActiveStyleColor(const WidgetCursor &widgetCursor, const Style *style) {
    return style->activeColor;
}

static uint16_t transformColor(uint16_t color) {
    return color;
}

static bool styleGetSmallerFont(font::Font &font) {
    return false;
}

static uint8_t getDisplayBackgroundLuminosityStep() {
    return DISPLAY_BACKGROUND_LUMINOSITY_STEP_DEFAULT;
}

static uint8_t getSelectedThemeIndex() {
    return eez::gui::THEME_ID_DEFAULT;
}

static void turnOnDisplayStart() {
    display::g_displayState = display::ON;
}

static void turnOnDisplayTick() {
}

static void turnOffDisplayStart() {
    display::g_displayState = display::OFF;
}

static void turnOffDisplayTick() {
}

static void toastMessagePageOnEncoder(ToastMessagePage *toast, int counter) {
	toast->appContext->popPage();
}

#if OPTION_TOUCH_CALIBRATION
static void onEnterTouchCalibration() {
	auto appContext = getAppContextFromId(APP_CONTEXT_ID_DEVICE);
    appContext->replacePage(PAGE_ID_TOUCH_CALIBRATION);
}

static void onTouchCalibrationOk() {
	auto appContext = getAppContextFromId(APP_CONTEXT_ID_DEVICE);
    appContext->popPage();
	appContext->infoMessage("Touch screen is calibrated.");
	appContext->showPage(appContext->getMainPageId());
}

static void onTouchCalibrationCancel() {
	auto appContext = getAppContextFromId(APP_CONTEXT_ID_DEVICE);
    appContext->showPage(appContext->getMainPageId());
}

static void onTouchCalibrationConfirm() {
	auto appContext = getAppContextFromId(APP_CONTEXT_ID_DEVICE);
    appContext->yesNoDialog(PAGE_ID_TOUCH_CALIBRATION_YES_NO, "Save changes?", touchCalibrationDialogYes, touchCalibrationDialogNo, nullptr);
}

static void getTouchScreenCalibrationParams(
    int16_t &touchScreenCalTlx, int16_t &touchScreenCalTly,
    int16_t &touchScreenCalBrx, int16_t &touchScreenCalBry,
    int16_t &touchScreenCalTrx, int16_t &touchScreenCalTry
) {
}

static void setTouchScreenCalibrationParams(
    int16_t touchScreenCalTlx, int16_t touchScreenCalTly,
    int16_t touchScreenCalBrx, int16_t touchScreenCalBry,
    int16_t touchScreenCalTrx, int16_t touchScreenCalTry
) {
}
#endif

static void onGuiQueueMessage(uint8_t type, int16_t param) {
}

////////////////////////////////////////////////////////////////////////////////

Hooks g_hooks = {
    getExtraLongTouchAction,
    getDefaultAnimationDuration,
    executeExternalAction,
    externalData,
	getWidgetTouchFunctionHook,
    getPageFromId,
    setFocusCursor,
    stateManagment,
    activePageHasBackdrop,
    executeActionThread,
    isEventHandlingDisabled,
    nullptr,
    overrideStyleColor,
    overrideActiveStyleColor,
    transformColor,
    styleGetSmallerFont,
    getDisplayBackgroundLuminosityStep,
    getSelectedThemeIndex,
    turnOnDisplayStart,
    turnOnDisplayTick,
    turnOffDisplayStart,
    turnOffDisplayTick,
    toastMessagePageOnEncoder,
#if OPTION_TOUCH_CALIBRATION
    onEnterTouchCalibration,
    onTouchCalibrationOk,
    onTouchCalibrationCancel,
    onTouchCalibrationConfirm,
    getTouchScreenCalibrationParams,
    setTouchScreenCalibrationParams,
#endif
    onGuiQueueMessage
};

} // namespace gui
} // namespace eez

#endif // EEZ_OPTION_GUI
