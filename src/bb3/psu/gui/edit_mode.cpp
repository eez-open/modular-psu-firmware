/* / mcu / sound.h
* EEZ PSU Firmware
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

#if OPTION_DISPLAY

#include <stdio.h>
#include <string.h>
#include <math.h>

#include <eez/sound.h>

#if OPTION_ENCODER
#include <bb3/mcu/encoder.h>
#endif

#include <bb3/psu/psu.h>
#include <bb3/psu/channel_dispatcher.h>
#include <bb3/psu/dlog_record.h>
#include <bb3/psu/profile.h>

#include <bb3/psu/gui/psu.h>
#include <bb3/psu/gui/edit_mode.h>
#include <bb3/psu/gui/keypad.h>
#include <bb3/psu/gui/page_ch_settings.h>

#define CONF_GUI_EDIT_MODE_STEP_THRESHOLD_PX 5

namespace eez {
namespace psu {
namespace gui {

WidgetCursor getFocusCursor() {
	WidgetCursor widgetCursor = g_focusCursor;
	if (widgetCursor.cursor == -1 && g_channel) {
		widgetCursor.cursor = g_channel->channelIndex;
	}
    return widgetCursor;
}

namespace edit_mode {

static int g_tabIndex = PAGE_ID_EDIT_MODE_KEYPAD;

static Value g_editValue;
static Value g_undoValue;
static Value g_minValue;
static Value g_maxValue;
static bool g_isInteractiveMode = true;

////////////////////////////////////////////////////////////////////////////////

static void update(WidgetCursor &cursor, int16_t dataId);

////////////////////////////////////////////////////////////////////////////////

bool isActive(AppContext *appContext) {
    return appContext->getActivePageId() == PAGE_ID_EDIT_MODE_KEYPAD ||
           appContext->getActivePageId() == PAGE_ID_EDIT_MODE_STEP ||
           appContext->getActivePageId() == PAGE_ID_EDIT_MODE_SLIDER;
}

void initEditValue(WidgetCursor &widgetCursor, int16_t dataId) {
    g_editValue = eez::gui::getEditValue(widgetCursor, dataId);
    g_undoValue = g_editValue;
}

void enter(int tabIndex, bool setFocus) {
#if OPTION_ENCODER
    if (setFocus && !isActive(&g_psuAppContext)) {
        if (!g_psuAppContext.isFocusWidget(getFoundWidgetAtDown()) || g_focusEditValue.getType() != VALUE_TYPE_UNDEFINED) {
            setFocusCursor(getFoundWidgetAtDown(), getFoundWidgetAtDown().widget->data);
            return;
        }
    }
#endif

	WidgetCursor widgetCursor;

    if (tabIndex == -1) {
        gui::selectChannelByCursor();
        widgetCursor = getFoundWidgetAtDown();
        int16_t newDataId = widgetCursor.widget->data;
        setFocusCursor(widgetCursor, newDataId);
        update(widgetCursor, newDataId);

        if (!isActive(&g_psuAppContext)) {
            pushPage(g_tabIndex);
        }
    } else {
        g_tabIndex = tabIndex;
        replacePage(g_tabIndex);

		widgetCursor = getFocusCursor();
    }

    if (g_tabIndex == PAGE_ID_EDIT_MODE_KEYPAD) {
        int slotIndex = -1;
        int subchannelIndex = -1;
        if (isChannelData(widgetCursor, g_focusDataId)) {
            auto &channel = Channel::get(widgetCursor.cursor);
            slotIndex = channel.slotIndex;
            subchannelIndex = channel.subchannelIndex;
		} else {
			getSlotAndSubchannelIndex(widgetCursor, g_focusDataId, slotIndex, subchannelIndex);
		}
        edit_mode_keypad::enter(slotIndex, subchannelIndex, g_editValue, getAllowZero(widgetCursor, g_focusDataId), g_minValue, g_maxValue);
    } else {
        edit_mode_keypad::exit();
    }
}

void exit() {
    edit_mode_keypad::exit();
    popPage();
}

void nonInteractiveSet() {
	WidgetCursor widgetCursor = getFocusCursor();
	Value result = set(widgetCursor, g_focusDataId, g_editValue);
    if (result.getType() == VALUE_TYPE_SCPI_ERROR) {
        psuErrorMessage(widgetCursor.cursor, result);
    } else {
        popPage();
    }
}

void nonInteractiveDiscard() {
    g_editValue = g_undoValue;
	WidgetCursor widgetCursor = getFocusCursor();
	set(widgetCursor, g_focusDataId, g_undoValue);
}

bool isInteractiveMode() {
    return g_isInteractiveMode;
}

void toggleInteractiveMode() {
    g_isInteractiveMode = !g_isInteractiveMode;
	WidgetCursor widgetCursor = getFocusCursor();
    initEditValue(widgetCursor, g_focusDataId);
}

const Value &getEditValue() {
    return g_editValue;
}

Value getCurrentValue() {
	WidgetCursor widgetCursor = getFocusCursor();
	return get(widgetCursor, g_focusDataId);
}

const Value &getMin() {
    return g_minValue;
}

const Value &getMax() {
    return g_maxValue;
}

Unit getUnit() {
    if (g_editValue.getType() == VALUE_TYPE_FLOAT) {
        return g_editValue.getUnit();
    }
    return getMin().getUnit();
}

bool setValue(float floatValue) {
	WidgetCursor widgetCursor = getFocusCursor();
	if (isChannelData(widgetCursor, g_focusDataId)) {
        floatValue = channel_dispatcher::roundChannelValue(Channel::get(widgetCursor.cursor), getUnit(), floatValue);
    }

    Value value = MakeValue(floatValue, getUnit());
    if (g_isInteractiveMode || g_tabIndex == PAGE_ID_EDIT_MODE_KEYPAD) {
        Value result = set(widgetCursor, g_focusDataId, value);
        if (result.getType() == VALUE_TYPE_SCPI_ERROR) {
            psuErrorMessage(widgetCursor.cursor, result);
            return false;
        }
    }
    g_editValue = value;
    return true;
}

#define NUM_PARTS 15

void getInfoText(char *infoText, int count) {
	WidgetCursor widgetCursor = getFocusCursor();
	const char *dataName = getName(widgetCursor, g_focusDataId);
    if (!dataName) {
        dataName = "Unknown";
    }

    Value minValue = eez::gui::getMin(widgetCursor, g_focusDataId);
    Value maxValue = (g_focusDataId == DATA_ID_CHANNEL_U_EDIT || g_focusDataId == DATA_ID_CHANNEL_I_EDIT) ?
        getLimit(widgetCursor, g_focusDataId) : eez::gui::getMax(widgetCursor, g_focusDataId);

    if (isChannelData(widgetCursor, g_focusDataId)) {
        Channel& channel = Channel::get(widgetCursor.cursor);
        if ((channel.channelIndex < 2 && channel_dispatcher::getCouplingType() != channel_dispatcher::COUPLING_TYPE_NONE) || channel.flags.trackingEnabled) {
            stringCopy(infoText, count, "Set ");
        } else {
            snprintf(infoText, count, "Set Ch%d ", widgetCursor.cursor + 1);
        }
        
        stringAppendString(infoText, count, dataName);
    } else {
        stringAppendString(infoText, count, dataName);
    }

    stringAppendString(infoText, count, " [");
    minValue.toText(infoText + strlen(infoText), count - strlen(infoText));
    stringAppendString(infoText, count, " - ");
    maxValue.toText(infoText + strlen(infoText), count - strlen(infoText));
    stringAppendString(infoText, count, "]");
}

static void update(WidgetCursor &widgetCursor, int16_t dataId) {
    initEditValue(widgetCursor, dataId);
	g_minValue = eez::gui::getMin(widgetCursor, dataId);
    g_maxValue = eez::gui::getMax(widgetCursor, dataId);
    if (edit_mode_keypad::g_keypad) {
        edit_mode_keypad::g_keypad->m_options.editValueUnit = g_editValue.getUnit();
    }
}

} // namespace edit_mode

namespace edit_mode_keypad {

NumericKeypad g_theKeypad;
NumericKeypad *g_keypad;

////////////////////////////////////////////////////////////////////////////////

void onKeypadOk(float value) {
    if (edit_mode::setValue(value)) {
        g_keypad->getAppContext()->popPage();
    }
}

void enter(int slotIndex, int subchannelIndex, const Value &editValue, bool allowZero, const Value &minValue, Value &maxValue) {
    g_keypad = &g_theKeypad;

    NumericKeypadOptions options;

    options.slotIndex = slotIndex;
	options.subchannelIndex = subchannelIndex;

    if (editValue.getType() == VALUE_TYPE_FLOAT) {
        options.editValueUnit = editValue.getUnit();
    } else {
        options.editValueUnit = minValue.getUnit();
    }

    options.allowZero = allowZero;
    options.min = minValue.getFloat();
    options.max = maxValue.getFloat();
    options.def = options.min;

    options.enableMaxButton();
    options.enableMinButton();
    options.flags.signButtonEnabled = options.min < 0;
    options.flags.dotButtonEnabled = true;

    g_keypad->init(&g_psuAppContext, 0, editValue, options, onKeypadOk, 0, 0);
}

void exit() {
    if (g_keypad) {
        g_keypad = 0;
    }
}

} // namespace edit_mode_keypad

////////////////////////////////////////////////////////////////////////////////

namespace edit_mode_step {

struct DataIdStepIndex {
    uint16_t dataId;
    uint16_t stepIndex;
};

static int g_stepIndex;

static bool g_changed;
static int g_startPos;

int getStepIndex() {
    return g_stepIndex;
}

void setStepIndex(int value) {
    g_stepIndex = value;
}

void getStepValues(StepValues &stepValues) {
	stepValues.count = 0;

	WidgetCursor widgetCursor;

    ChSettingsListsPage *page = (ChSettingsListsPage *)getPage(PAGE_ID_CH_SETTINGS_LISTS);
    if (page) {
		widgetCursor.cursor = g_channel->channelIndex;
        getEncoderStepValues(widgetCursor, page->getDataIdAtCursor(), stepValues);
    } else {
		widgetCursor = getFocusCursor();
		getEncoderStepValues(widgetCursor, g_focusDataId, stepValues);
    }
}

void switchToNextStepIndex() {
    setStepIndex(getStepIndex() + 1);
}

void increment(int counter, bool playClick) {
    float min = edit_mode::getMin().getFloat();
    float max = edit_mode::getMax().getFloat();

    StepValues stepValues;
    getStepValues(stepValues);
    float stepValue = stepValues.values[getStepIndex() % stepValues.count];

    float value = edit_mode::getEditValue().getFloat();

    for (int i = 0; i < abs(counter); ++i) {
        if (counter > 0) {
            value += stepValue;
            if (value > max) {
                value = max;
            }
        } else {
            value -= stepValue;
            if (value < min) {
                value = min;
            }
        }
    }

    if (edit_mode::setValue(value)) {
        g_changed = true;
        if (playClick) {
            sound::playClick();
        }
    }
}

#if OPTION_ENCODER

void onEncoder(int counter) {
    increment(counter, false);
}

#endif

void test() {
    if (!g_changed) {
        int d = eez::gui::touch::getX() - g_startPos;
        if (abs(d) >= CONF_GUI_EDIT_MODE_STEP_THRESHOLD_PX) {
            increment(d > 0 ? 1 : -1, true);
        }
    }
}

void onTouchDown() {
    g_startPos = eez::gui::touch::getX();
    g_changed = false;
}

void onTouchMove() {
    test();
}

void onTouchUp() {
}

////////////////////////////////////////////////////////////////////////////////

EncoderMode g_frequencyEncoderMode = ENCODER_MODE_AUTO;
EncoderMode g_smallFrequencyEncoderMode = ENCODER_MODE_AUTO;
EncoderMode g_dutyEncoderMode = ENCODER_MODE_AUTO;

EncoderMode g_protectionDelayEncoderMode = ENCODER_MODE_AUTO;
EncoderMode g_rampAndDelayDurationEncoderMode = ENCODER_MODE_AUTO;
EncoderMode g_otpLevelEncoderMode = ENCODER_MODE_AUTO;

EncoderMode g_listVoltageEncoderMode = ENCODER_MODE_AUTO;
EncoderMode g_listCurrentEncoderMode = ENCODER_MODE_AUTO;
EncoderMode g_listDwellEncoderMode = ENCODER_MODE_AUTO;

EncoderMode g_dcpVoltageEncoderMode = ENCODER_MODE_AUTO;
EncoderMode g_dcpCurrentEncoderMode = ENCODER_MODE_AUTO;
EncoderMode g_dcpPowerEncoderMode = ENCODER_MODE_AUTO;

EncoderMode g_dcmVoltageEncoderMode = ENCODER_MODE_AUTO;
EncoderMode g_dcmCurrentEncoderMode = ENCODER_MODE_AUTO;
EncoderMode g_dcmPowerEncoderMode = ENCODER_MODE_AUTO;

EncoderMode g_recordingEncoderMode = ENCODER_MODE_AUTO;
EncoderMode g_visibleValueDivEncoderMode = ENCODER_MODE_AUTO;
EncoderMode g_visibleValueOffsetEncoderMode = ENCODER_MODE_AUTO;
EncoderMode g_xAxisOffsetEncoderMode = ENCODER_MODE_AUTO;
EncoderMode g_xAxisDivEncoderMode = ENCODER_MODE_AUTO;

EncoderMode g_smx46DacEncoderMode = ENCODER_MODE_AUTO;

EncoderMode g_mio168NplcEncoderMode = ENCODER_MODE_AUTO;
EncoderMode g_mio168AinVoltageEncoderMode = ENCODER_MODE_AUTO;
EncoderMode g_mio168AinCurrentEncoderMode = ENCODER_MODE_AUTO;
EncoderMode g_mio168AoutVoltageEncoderMode = ENCODER_MODE_AUTO;
EncoderMode g_mio168AoutCurrentEncoderMode = ENCODER_MODE_AUTO;

EncoderMode g_scrollBarEncoderMode = ENCODER_MODE_AUTO;

EncoderMode g_functionGeneratorFrequencyEncoderMode = ENCODER_MODE_AUTO;
EncoderMode g_functionGeneratorPhaseShiftEncoderMode = ENCODER_MODE_AUTO;
EncoderMode g_functionGeneratorAmplitudeEncoderMode = ENCODER_MODE_AUTO;
EncoderMode g_functionGeneratorOffsetEncoderMode = ENCODER_MODE_AUTO;
EncoderMode g_functionGeneratorDutyCycleEncoderMode = ENCODER_MODE_AUTO;
EncoderMode g_functionGeneratorPreviewPeriodEncoderMode = ENCODER_MODE_AUTO;

static Value getCurrentEncoderStepValue() {
    StepValues stepValues;
    getStepValues(stepValues);

	if (stepValues.count == 0) {
		return Value();
	}

    int stepValueIndex = stepValues.encoderSettings.mode - ENCODER_MODE_STEP1;
	if (stepValueIndex < 0) {
		stepValueIndex = 0;
	} else if (stepValueIndex >= stepValues.count) {
        stepValueIndex = stepValues.count - 1;
    }

	return Value(stepValues.values[stepValueIndex], stepValues.unit);
}

float getEncoderStepValue() {
    StepValues stepValues;

    edit_mode_step::getStepValues(stepValues);

	if (stepValues.encoderSettings.mode == ENCODER_MODE_AUTO) {
		mcu::encoder::enableAcceleration(stepValues.encoderSettings.accelerationEnabled, stepValues.encoderSettings.range, stepValues.encoderSettings.step);
	} else {
		mcu::encoder::enableAcceleration(false);
	}

    if (stepValues.encoderSettings.mode == ENCODER_MODE_AUTO) {
        return stepValues.encoderSettings.step;
    }

	return edit_mode_step::getCurrentEncoderStepValue().getFloat();
}

void switchToNextEncoderMode() {
    if (psu::gui::isEncoderEnabledInActivePage() || getActivePageId() == PAGE_ID_CH_SETTINGS_LISTS) {
		StepValues stepValues;
		getStepValues(stepValues);

		EncoderMode encoderMode = (EncoderMode)(stepValues.encoderSettings.mode + 1);
		if (encoderMode - ENCODER_MODE_STEP1 >= stepValues.count) {
			encoderMode = ENCODER_MODE_AUTO;
		}

		ChSettingsListsPage *page = (ChSettingsListsPage *)getPage(PAGE_ID_CH_SETTINGS_LISTS);
		WidgetCursor widgetCursor;
		if (page) {
			widgetCursor.cursor = g_channel->channelIndex;
			setEncoderMode(widgetCursor, page->getDataIdAtCursor(), encoderMode);
		} else {
			widgetCursor = getFocusCursor();
			setEncoderMode(widgetCursor, g_focusDataId, encoderMode);
		}

		if (encoderMode == ENCODER_MODE_AUTO) {
			g_psuAppContext.infoMessage("Auto");
		} else {
			g_psuAppContext.infoMessage(getCurrentEncoderStepValue());
		}
	}
}

void getProfileParameters(profile::Parameters &parameters) {
    parameters.encoderModes.frequency = g_frequencyEncoderMode;
    parameters.encoderModes.smallFrequency = g_smallFrequencyEncoderMode;
    parameters.encoderModes.duty = g_dutyEncoderMode;

    parameters.encoderModes.protectionDelay = g_protectionDelayEncoderMode;
    parameters.encoderModes.rampAndDelayDuration = g_rampAndDelayDurationEncoderMode;
    parameters.encoderModes.otpLevel = g_otpLevelEncoderMode;

    parameters.encoderModes.listVoltage = g_listVoltageEncoderMode;
    parameters.encoderModes.listCurrent = g_listCurrentEncoderMode;
    parameters.encoderModes.listDwell = g_listDwellEncoderMode;

    parameters.encoderModes.dcpVoltage = g_dcpVoltageEncoderMode;
    parameters.encoderModes.dcpCurrent = g_dcpCurrentEncoderMode;
    parameters.encoderModes.dcpPower = g_dcpPowerEncoderMode;

    parameters.encoderModes.dcmVoltage = g_dcmVoltageEncoderMode;
    parameters.encoderModes.dcmCurrent = g_dcmCurrentEncoderMode;
    parameters.encoderModes.dcmPower = g_dcmPowerEncoderMode;

    parameters.encoderModes.recording = g_recordingEncoderMode;
    parameters.encoderModes.visibleValueDiv = g_visibleValueDivEncoderMode;
    parameters.encoderModes.visibleValueOffset = g_visibleValueOffsetEncoderMode;
    parameters.encoderModes.xAxisOffset = g_xAxisOffsetEncoderMode;
    parameters.encoderModes.xAxisDiv = g_xAxisDivEncoderMode;

    parameters.encoderModes.smx46Dac = g_smx46DacEncoderMode;

    parameters.encoderModes.mio168Nplc = g_mio168NplcEncoderMode;
    parameters.encoderModes.mio168AinVoltage = g_mio168AinVoltageEncoderMode;
    parameters.encoderModes.mio168AinCurrent = g_mio168AinCurrentEncoderMode;
    parameters.encoderModes.mio168AoutVoltage = g_mio168AoutVoltageEncoderMode;
    parameters.encoderModes.mio168AoutCurrent = g_mio168AoutCurrentEncoderMode;

    parameters.encoderModes.scrollBar = g_scrollBarEncoderMode;

    parameters.encoderModes.functionGeneratorFrequency = g_functionGeneratorFrequencyEncoderMode;
    parameters.encoderModes.functionGeneratorPhaseShift = g_functionGeneratorPhaseShiftEncoderMode;
    parameters.encoderModes.functionGeneratorAmplitude = g_functionGeneratorAmplitudeEncoderMode;
    parameters.encoderModes.functionGeneratorOffset = g_functionGeneratorOffsetEncoderMode;
    parameters.encoderModes.functionGeneratorDutyCycle = g_functionGeneratorDutyCycleEncoderMode;
    parameters.encoderModes.functionGeneratorPreviewPeriod = g_functionGeneratorPreviewPeriodEncoderMode;
}

void setProfileParameters(const profile::Parameters &parameters) {
    g_frequencyEncoderMode = (EncoderMode)parameters.encoderModes.frequency;
    g_smallFrequencyEncoderMode = (EncoderMode)parameters.encoderModes.smallFrequency;
    g_dutyEncoderMode = (EncoderMode)parameters.encoderModes.duty;

    g_protectionDelayEncoderMode = (EncoderMode)parameters.encoderModes.protectionDelay;
    g_rampAndDelayDurationEncoderMode = (EncoderMode)parameters.encoderModes.rampAndDelayDuration;
    g_otpLevelEncoderMode = (EncoderMode)parameters.encoderModes.otpLevel;

    g_listVoltageEncoderMode = (EncoderMode)parameters.encoderModes.listVoltage;
    g_listCurrentEncoderMode = (EncoderMode)parameters.encoderModes.listCurrent;
    g_listDwellEncoderMode = (EncoderMode)parameters.encoderModes.listDwell;

    g_dcpVoltageEncoderMode = (EncoderMode)parameters.encoderModes.dcpVoltage;
    g_dcpCurrentEncoderMode = (EncoderMode)parameters.encoderModes.dcpCurrent;
    g_dcpPowerEncoderMode = (EncoderMode)parameters.encoderModes.dcpPower;

    g_dcmVoltageEncoderMode = (EncoderMode)parameters.encoderModes.dcmVoltage;
    g_dcmCurrentEncoderMode = (EncoderMode)parameters.encoderModes.dcmCurrent;
    g_dcmPowerEncoderMode = (EncoderMode)parameters.encoderModes.dcmPower;

    g_recordingEncoderMode = (EncoderMode)parameters.encoderModes.recording;
    g_visibleValueDivEncoderMode = (EncoderMode)parameters.encoderModes.visibleValueDiv;
    g_visibleValueOffsetEncoderMode = (EncoderMode)parameters.encoderModes.visibleValueOffset;
    g_xAxisOffsetEncoderMode = (EncoderMode)parameters.encoderModes.xAxisOffset;
    g_xAxisDivEncoderMode = (EncoderMode)parameters.encoderModes.xAxisDiv;

    g_smx46DacEncoderMode = (EncoderMode)parameters.encoderModes.smx46Dac;

    g_mio168NplcEncoderMode = (EncoderMode)parameters.encoderModes.mio168Nplc;
    g_mio168AinVoltageEncoderMode = (EncoderMode)parameters.encoderModes.mio168AinVoltage;
    g_mio168AinCurrentEncoderMode = (EncoderMode)parameters.encoderModes.mio168AinCurrent;
    g_mio168AoutVoltageEncoderMode = (EncoderMode)parameters.encoderModes.mio168AoutVoltage;
    g_mio168AoutCurrentEncoderMode = (EncoderMode)parameters.encoderModes.mio168AoutCurrent;

    g_scrollBarEncoderMode = (EncoderMode)parameters.encoderModes.scrollBar;

    g_functionGeneratorFrequencyEncoderMode = (EncoderMode)parameters.encoderModes.functionGeneratorFrequency;
    g_functionGeneratorPhaseShiftEncoderMode = (EncoderMode)parameters.encoderModes.functionGeneratorPhaseShift;
    g_functionGeneratorAmplitudeEncoderMode = (EncoderMode)parameters.encoderModes.functionGeneratorAmplitude;
    g_functionGeneratorOffsetEncoderMode = (EncoderMode)parameters.encoderModes.functionGeneratorOffset;
    g_functionGeneratorDutyCycleEncoderMode = (EncoderMode)parameters.encoderModes.functionGeneratorDutyCycle;
    g_functionGeneratorPreviewPeriodEncoderMode = (EncoderMode)parameters.encoderModes.functionGeneratorPreviewPeriod;
}

bool writeProfileProperties(profile::WriteContext &ctx, const profile::Parameters &parameters) {
    ctx.group("encoderModes");

    if (parameters.encoderModes.frequency) WRITE_PROPERTY("frequency", parameters.encoderModes.frequency);
    if (parameters.encoderModes.smallFrequency) WRITE_PROPERTY("smallFrequency", parameters.encoderModes.smallFrequency);
    if (parameters.encoderModes.duty) WRITE_PROPERTY("duty", parameters.encoderModes.duty);

    if (parameters.encoderModes.protectionDelay) WRITE_PROPERTY("protectionDelay", parameters.encoderModes.protectionDelay);
    if (parameters.encoderModes.rampAndDelayDuration) WRITE_PROPERTY("rampAndDelayDuration", parameters.encoderModes.rampAndDelayDuration);
    if (parameters.encoderModes.otpLevel) WRITE_PROPERTY("otpLevel", parameters.encoderModes.otpLevel);

    if (parameters.encoderModes.listVoltage) WRITE_PROPERTY("listVoltage", parameters.encoderModes.listVoltage);
    if (parameters.encoderModes.listCurrent) WRITE_PROPERTY("listCurrent", parameters.encoderModes.listCurrent);
    if (parameters.encoderModes.listDwell) WRITE_PROPERTY("listDwell", parameters.encoderModes.listDwell);

    if (parameters.encoderModes.dcpVoltage) WRITE_PROPERTY("dcpVoltage", parameters.encoderModes.dcpVoltage);
    if (parameters.encoderModes.dcpCurrent) WRITE_PROPERTY("dcpCurrent", parameters.encoderModes.dcpCurrent);
    if (parameters.encoderModes.dcpPower) WRITE_PROPERTY("dcpPower", parameters.encoderModes.dcpPower);

    if (parameters.encoderModes.dcmVoltage) WRITE_PROPERTY("dcmVoltage", parameters.encoderModes.dcmVoltage);
    if (parameters.encoderModes.dcmCurrent) WRITE_PROPERTY("dcmCurrent", parameters.encoderModes.dcmCurrent);
    if (parameters.encoderModes.dcmPower) WRITE_PROPERTY("dcmPower", parameters.encoderModes.dcmPower);

    if (parameters.encoderModes.recording) WRITE_PROPERTY("recording", parameters.encoderModes.recording);
    if (parameters.encoderModes.visibleValueDiv) WRITE_PROPERTY("visibleValueDiv", parameters.encoderModes.visibleValueDiv);
    if (parameters.encoderModes.visibleValueOffset) WRITE_PROPERTY("visibleValueOffset", parameters.encoderModes.visibleValueOffset);
    if (parameters.encoderModes.xAxisOffset) WRITE_PROPERTY("xAxisOffset", parameters.encoderModes.xAxisOffset);
    if (parameters.encoderModes.xAxisDiv) WRITE_PROPERTY("xAxisDiv", parameters.encoderModes.xAxisDiv);

    if (parameters.encoderModes.smx46Dac) WRITE_PROPERTY("smx46Dac", parameters.encoderModes.smx46Dac);

    if (parameters.encoderModes.mio168Nplc) WRITE_PROPERTY("mio168Nplc", parameters.encoderModes.mio168Nplc);
    if (parameters.encoderModes.mio168AinVoltage) WRITE_PROPERTY("mio168AinVoltage", parameters.encoderModes.mio168AinVoltage);
    if (parameters.encoderModes.mio168AinCurrent) WRITE_PROPERTY("mio168AinCurrent", parameters.encoderModes.mio168AinCurrent);
    if (parameters.encoderModes.mio168AoutVoltage) WRITE_PROPERTY("mio168AoutVoltage", parameters.encoderModes.mio168AoutVoltage);
    if (parameters.encoderModes.mio168AoutCurrent) WRITE_PROPERTY("mio168AoutCurrent", parameters.encoderModes.mio168AoutCurrent);

    if (parameters.encoderModes.scrollBar) WRITE_PROPERTY("scrollBar", parameters.encoderModes.scrollBar);

    if (parameters.encoderModes.functionGeneratorFrequency) WRITE_PROPERTY("functionGeneratorFrequency", parameters.encoderModes.functionGeneratorFrequency);
    if (parameters.encoderModes.functionGeneratorPhaseShift) WRITE_PROPERTY("functionGeneratorPhaseShift", parameters.encoderModes.functionGeneratorPhaseShift);
    if (parameters.encoderModes.functionGeneratorAmplitude) WRITE_PROPERTY("functionGeneratorAmplitude", parameters.encoderModes.functionGeneratorAmplitude);
    if (parameters.encoderModes.functionGeneratorOffset) WRITE_PROPERTY("functionGeneratorOffset", parameters.encoderModes.functionGeneratorOffset);
    if (parameters.encoderModes.functionGeneratorDutyCycle) WRITE_PROPERTY("functionGeneratorDutyCycle", parameters.encoderModes.functionGeneratorDutyCycle);
    if (parameters.encoderModes.functionGeneratorPreviewPeriod) WRITE_PROPERTY("functionGeneratorPreviewPeriod", parameters.encoderModes.functionGeneratorPreviewPeriod);

    return true;
}

bool readProfileProperties(profile::ReadContext &ctx, profile::Parameters &parameters) {
    if (!ctx.matchGroup("encoderModes")) {
        return false;
    }

    READ_FLAG("frequency", parameters.encoderModes.frequency);
    READ_FLAG("smallFrequency", parameters.encoderModes.smallFrequency);
    READ_FLAG("duty", parameters.encoderModes.duty);

    READ_FLAG("protectionDelay", parameters.encoderModes.protectionDelay);
    READ_FLAG("rampAndDelayDuration", parameters.encoderModes.rampAndDelayDuration);
    READ_FLAG("otpLevel", parameters.encoderModes.otpLevel);

    READ_FLAG("listVoltage", parameters.encoderModes.listVoltage);
    READ_FLAG("listCurrent", parameters.encoderModes.listCurrent);
    READ_FLAG("listDwell", parameters.encoderModes.listDwell);

    READ_FLAG("dcpVoltage", parameters.encoderModes.dcpVoltage);
    READ_FLAG("dcpCurrent", parameters.encoderModes.dcpCurrent);
    READ_FLAG("dcpPower", parameters.encoderModes.dcpPower);

    READ_FLAG("dcmVoltage", parameters.encoderModes.dcmVoltage);
    READ_FLAG("dcmCurrent", parameters.encoderModes.dcmCurrent);
    READ_FLAG("dcmPower", parameters.encoderModes.dcmPower);

    READ_FLAG("recording", parameters.encoderModes.recording);
    READ_FLAG("visibleValueDiv", parameters.encoderModes.visibleValueDiv);
    READ_FLAG("visibleValueOffset", parameters.encoderModes.visibleValueOffset);
    READ_FLAG("xAxisOffset", parameters.encoderModes.xAxisOffset);
    READ_FLAG("xAxisDiv", parameters.encoderModes.xAxisDiv);

    READ_FLAG("smx46Dac", parameters.encoderModes.smx46Dac);

    READ_FLAG("mio168Nplc", parameters.encoderModes.mio168Nplc);
    READ_FLAG("mio168AinVoltage", parameters.encoderModes.mio168AinVoltage);
    READ_FLAG("mio168AinCurrent", parameters.encoderModes.mio168AinCurrent);
    READ_FLAG("mio168AoutVoltage", parameters.encoderModes.mio168AoutVoltage);
    READ_FLAG("mio168AoutCurrent", parameters.encoderModes.mio168AoutCurrent);

    READ_FLAG("scrollBar", parameters.encoderModes.scrollBar);

    READ_FLAG("functionGeneratorFrequency", parameters.encoderModes.functionGeneratorFrequency);
    READ_FLAG("functionGeneratorPhaseShift", parameters.encoderModes.functionGeneratorPhaseShift);
    READ_FLAG("functionGeneratorAmplitude", parameters.encoderModes.functionGeneratorAmplitude);
    READ_FLAG("functionGeneratorOffset", parameters.encoderModes.functionGeneratorOffset);
    READ_FLAG("functionGeneratorDutyCycle", parameters.encoderModes.functionGeneratorDutyCycle);
    READ_FLAG("functionGeneratorPreviewPeriod", parameters.encoderModes.functionGeneratorPreviewPeriod);

    return false;
}

} // namespace edit_mode_step

////////////////////////////////////////////////////////////////////////////////

namespace edit_mode_slider {

static const int TOP_BORDER = 51;
static const int BOTTOM_BORDER = 255;

static const int DX = 10;

static float startValue;
static int startX;
static float stepValue;

void onTouchDown() {
    startValue = edit_mode::getEditValue().getFloat();
    startX = eez::gui::touch::getX();

    int y = eez::gui::touch::getY() - g_psuAppContext.rect.y;
    if (y < TOP_BORDER) {
        y = TOP_BORDER;
    } else if (y > BOTTOM_BORDER) {
        y = BOTTOM_BORDER;
    }
    y -= TOP_BORDER;

    int stepIndex = edit_mode_step::NUM_STEPS * y / (BOTTOM_BORDER - TOP_BORDER);
    stepIndex = edit_mode_step::NUM_STEPS - 1 - MAX(MIN(stepIndex, edit_mode_step::NUM_STEPS - 1), 0);
    StepValues stepValues;
    edit_mode_step::getStepValues(stepValues);
    stepValue = stepValues.values[MIN(stepIndex, stepValues.count - 1)];
}

void onTouchMove() {
    float min = edit_mode::getMin().getFloat();
    float max = edit_mode::getMax().getFloat();

    int counter = (eez::gui::touch::getX() - startX) / DX;

    float value = roundPrec(startValue + counter * stepValue, stepValue);

    if (value < min) {
        value = min;
    }
    if (value > max) {
        value = max;
    }

    edit_mode::setValue(value);
}

void onTouchUp() {
}

} // namespace edit_mode_slider

} // namespace gui
} // namespace psu
} // namespace eez

#endif
