/*
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

#pragma once

namespace eez {
namespace psu {

namespace profile {
    struct Parameters;
    class WriteContext;
    class ReadContext;
}

namespace gui {

namespace edit_mode {

bool isActive(eez::gui::AppContext *appContext);
void enter(int tabIndex = -1, bool setFocus = true);
void exit();
bool isInteractiveMode();
void toggleInteractiveMode();

const Value &getEditValue();
Value getCurrentValue();
const Value &getMin();
const Value &getMax();
Unit getUnit();
bool setValue(float value);

void getInfoText(char *infoText, int count);

void nonInteractiveSet();
void nonInteractiveDiscard();

} // namespace edit_mode

////////////////////////////////////////////////////////////////////////////////

class NumericKeypad;

namespace edit_mode_keypad {

void enter(int channelIndex, const Value &editValue, bool allowZero, const Value &minValue, Value &maxValue);
void exit();

extern NumericKeypad *g_keypad;

} // namespace edit_mode_keypad

////////////////////////////////////////////////////////////////////////////////

namespace edit_mode_step {

void getStepValues(StepValues &stepValues);

static const int NUM_STEPS = 4;

int getStepIndex();
void setStepIndex(int value);

#if OPTION_ENCODER
void onEncoder(int counter);
#endif

void onTouchDown();
void onTouchMove();
void onTouchUp();

void switchToNextStepIndex();

extern EncoderMode g_frequencyEncoderMode;
extern EncoderMode g_smallFrequencyEncoderMode;
extern EncoderMode g_dutyEncoderMode;

extern EncoderMode g_protectionDelayEncoderMode;
extern EncoderMode g_rampAndDelayDurationEncoderMode;
extern EncoderMode g_otpLevelEncoderMode;

extern EncoderMode g_listVoltageEncoderMode;
extern EncoderMode g_listCurrentEncoderMode;
extern EncoderMode g_listDwellEncoderMode;

extern EncoderMode g_dcpVoltageEncoderMode;
extern EncoderMode g_dcpCurrentEncoderMode;
extern EncoderMode g_dcpPowerEncoderMode;

extern EncoderMode g_dcmVoltageEncoderMode;
extern EncoderMode g_dcmCurrentEncoderMode;
extern EncoderMode g_dcmPowerEncoderMode;

extern EncoderMode g_recordingEncoderMode;
extern EncoderMode g_visibleValueDivEncoderMode;
extern EncoderMode g_visibleValueOffsetEncoderMode;
extern EncoderMode g_xAxisOffsetEncoderMode;
extern EncoderMode g_xAxisDivEncoderMode;

extern EncoderMode g_smx46DacEncoderMode;

extern EncoderMode g_mio168NplcEncoderMode;
extern EncoderMode g_mio168AinVoltageEncoderMode;
extern EncoderMode g_mio168AinCurrentEncoderMode;
extern EncoderMode g_mio168AoutVoltageEncoderMode;
extern EncoderMode g_mio168AoutCurrentEncoderMode;

extern EncoderMode g_scrollBarEncoderMode;

float getEncoderStepValue();
void switchToNextEncoderMode();

void getProfileParameters(profile::Parameters &profileParams);
void setProfileParameters(const profile::Parameters &profileParams);
bool writeProfileProperties(profile::WriteContext &ctx, const profile::Parameters &profileParams);
bool readProfileProperties(profile::ReadContext &ctx, profile::Parameters &profileParams);

} // namespace edit_mode_step

////////////////////////////////////////////////////////////////////////////////

namespace edit_mode_slider {

void onTouchDown();
void onTouchMove();
void onTouchUp();

}

} // namespace gui
} // namespace psu
} // namespace eez
