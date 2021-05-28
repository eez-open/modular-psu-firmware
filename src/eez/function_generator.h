/*
* EEZ PSU Firmware
* Copyright (C) 2021-present, Envox d.o.o.
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

#include <stdint.h>

#include <eez/gui/gui.h>

#include <eez/modules/psu/psu.h>
#include <eez/modules/psu/channel.h>
#include <eez/modules/psu/profile.h>

namespace eez {
namespace function_generator {

enum Waveform {
	WAVEFORM_NONE,
	WAVEFORM_DC,
	WAVEFORM_SINE_WAVE,
	WAVEFORM_TRIANGLE,
	WAVEFORM_SQUARE_WAVE,
	WAVEFORM_PULSE,
	WAVEFORM_SAWTOOTH,
	WAVEFORM_ARBITRARY
};

struct WaveformParameters {
	int absoluteResourceIndex;
	FunctionGeneratorResourceType resourceType;
	Waveform waveform;
	float frequency;
	float phaseShift;
	float amplitude;
	float offset;
	float pulseWidth;
};

WaveformParameters *getWaveformParameters(int slotIndex, int subchannelIndex, int resourceIndex);

void getProfileParameters(psu::profile::Parameters &profileParams);
void setProfileParameters(const psu::profile::Parameters &profileParams);
bool writeProfileProperties(psu::profile::WriteContext &ctx, const psu::profile::Parameters &profileParams);
bool readProfileProperties(psu::profile::ReadContext &ctx, psu::profile::Parameters &profileParams);

bool addChannelWaveformParameters(int slotIndex, int subchannelIndex, int resourceIndex, int *err = nullptr);
void addChannelWaveformParameters(psu::Channel &channel);

void removeChannelWaveformParameters(int slotIndex, int subchannelIndex, int resourceIndex);
void removeAllChannels();
void removePowerChannels();
void removeOtherTrackingChannels();

void selectWaveformParametersForChannel(int slotIndex, int subchannelIndex, int resourceIndex);

int getNumChannelsInFunctionGeneratorTriggerMode();

bool getWaveform(int slotIndex, int subchannelIndex, int resourceIndex, Waveform &waveform, int *err);
bool setWaveform(int slotIndex, int subchannelIndex, int resourceIndex, Waveform waveform, int *err);

bool getFrequency(int slotIndex, int subchannelIndex, int resourceIndex, float &frequency, int *err);
bool setFrequency(int slotIndex, int subchannelIndex, int resourceIndex, float frequency, int *err);

bool getPhaseShift(int slotIndex, int subchannelIndex, int resourceIndex, float &phaseShift, int *err);
bool setPhaseShift(int slotIndex, int subchannelIndex, int resourceIndex, float phaseShift, int *err);

bool getAmplitude(int slotIndex, int subchannelIndex, int resourceIndex, float &amplitude, int *err);
bool setAmplitude(int slotIndex, int subchannelIndex, int resourceIndex, float amplitude, int *err);

bool getOffset(int slotIndex, int subchannelIndex, int resourceIndex, float &offset, int *err);
bool setOffset(int slotIndex, int subchannelIndex, int resourceIndex, float offset, int *err);

bool getPulseWidth(int slotIndex, int subchannelIndex, int resourceIndex, float &pulseWidth, int *err);
bool setPulseWidth(int slotIndex, int subchannelIndex, int resourceIndex, float pulseWidth, int *err);

bool getResourceType(int slotIndex, int subchannelIndex, int resourceIndex, FunctionGeneratorResourceType &resourceType, int *err);
bool setResourceType(int slotIndex, int subchannelIndex, int resourceIndex, FunctionGeneratorResourceType resourceType, int *err);

bool isActive();
void executionStart();
void tick();
void abort();

extern eez::gui::SetPage *g_pFunctionGeneratorPage;
extern eez::gui::SetPage *g_pFunctionGeneratorSelectChannelsPage;

extern const char *g_waveformShortLabel[8];

} // namespace function_generator
} // namespace eez
