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

#include <memory.h>
#include <float.h>
#define _USE_MATH_DEFINES
#include <math.h>

#if defined(EEZ_PLATFORM_STM32)
#include <main.h>
#endif

#include <eez/function_generator.h>

#include <eez/index.h>
#include <eez/hmi.h>
#include <eez/util.h>
#include <eez/gui/gui.h>
#include <eez/modules/psu/psu.h>
#include <eez/modules/psu/channel_dispatcher.h>
#include <eez/modules/psu/profile.h>
#include <eez/modules/psu/gui/psu.h>
#include "eez/modules/psu/gui/edit_mode.h"

#include <eez/modules/psu/scpi/psu.h>

#define M_PI_F ((float)M_PI)

using namespace eez::function_generator;
using namespace eez::gui;
using namespace eez::psu;
using namespace eez::psu::gui;
using namespace eez::psu::profile;

namespace eez {
namespace function_generator {

static EnumItem g_waveformEnumDefinition[] = {
	{ WAVEFORM_DC, "\xc9 DC" },
	{ WAVEFORM_SINE, "\xc3 Sine" },
	{ WAVEFORM_SINE_HALF, "\xca Sine Half" },
	{ WAVEFORM_SINE_RECTIFIED, "\xcb Sine Rectified" },
	{ WAVEFORM_TRIANGLE, "\xc4 Triangle" },
	{ WAVEFORM_SQUARE, "\xc5 Square" },
	{ WAVEFORM_PULSE, "\xc6 Pulse" },
	{ WAVEFORM_SAWTOOTH, "\xc7 Sawtooth" },
	{ WAVEFORM_ARBITRARY, "\xc8 Arbitrary" },
	{ 0, 0 }
};

const char *g_waveformShortLabel[10] = {
	"None",
	"\xc9",
	"\xc3",
	"\xca",
	"\xcb",
	"\xc4",
	"\xc5",
	"\xc6",
	"\xc7",
	"\xc8"
};

WaveformParameters g_waveformParameters[MAX_NUM_WAVEFORMS] = {
	{ -1 }, { -1 }, { -1 }, { -1 }, 
	{ -1 }, { -1 }, { -1 }, { -1 },
	{ -1 }, { -1 }, { -1 }, { -1 },
	{ -1 }, { -1 }, { -1 }, { -1 },
};

typedef float(*WaveformFunction)(float);

bool g_active;

FunctionGeneratorOptions g_options = {
	1, /* isFreq */
	1, /* isAmpl */
};

WaveformFunction g_waveFormFuncU[CH_MAX];
float g_phiU[CH_MAX];
float g_dphiU[CH_MAX];
float g_amplitudeU[CH_MAX];
float g_offsetU[CH_MAX];
float g_dutyCycleU[CH_MAX];

WaveformFunction g_waveFormFuncI[CH_MAX];
float g_phiI[CH_MAX];
float g_dphiI[CH_MAX];
float g_amplitudeI[CH_MAX];
float g_offsetI[CH_MAX];
float g_dutyCycleI[CH_MAX];

static const float PERIOD = 0.0002f;

static void reloadWaveformParameters();

////////////////////////////////////////////////////////////////////////////////

class FunctionGeneratorPage : public SetPage {
public:
    static const int PAGE_SIZE;

    void pageAlloc();

    int getDirty() {
		return memcmp(g_waveformParameters, &function_generator::g_waveformParameters,
			sizeof(function_generator::g_waveformParameters)) != 0;
	}
    
    static void apply();

	void set() {
		apply();
		popPage();
	}

	static void draw(const WidgetCursor &widgetCursor);
	static void drawWaveform(const WidgetCursor &widgetCursor, WaveformParameters &waveformParameters, float T, float min, float max, bool selected = false, int yOffset = 0);

	static int getNumFunctionGeneratorResources() {
		uint32_t numResources = 0;
		for (int i = 0; i < MAX_NUM_WAVEFORMS; i++) {
			if (FunctionGeneratorPage::g_waveformParameters[i].absoluteResourceIndex != -1) {
				numResources++;
			}
		}
		return numResources;
	}

	static int getScrollPosition() {
		return g_scrollPosition;
	}

	static void setScrollPosition(int scrollPosition) {
		auto numResources = getNumFunctionGeneratorResources();

		if (numResources <= PAGE_SIZE) {
			return;
		}

		if (scrollPosition < 0) {
			scrollPosition = 0;
		} else if (scrollPosition > numResources - PAGE_SIZE) {
			scrollPosition = numResources - PAGE_SIZE;
		}
		
		if (scrollPosition != g_scrollPosition) {
			g_scrollPosition = scrollPosition;
			refreshScreen();
		}
	}

	static int getRefreshState() {
		return g_version;
	}

	static WaveformParameters g_waveformParameters[MAX_NUM_WAVEFORMS];
	static TriggerMode g_triggerModes[MAX_NUM_WAVEFORMS];
	static int g_selectedItem;
	static uint32_t g_version;

private:
	static int g_scrollPosition;
};

WaveformParameters FunctionGeneratorPage::g_waveformParameters[MAX_NUM_WAVEFORMS];
TriggerMode FunctionGeneratorPage::g_triggerModes[MAX_NUM_WAVEFORMS];
const int FunctionGeneratorPage::PAGE_SIZE = 4;
int FunctionGeneratorPage::g_selectedItem = 0;
int FunctionGeneratorPage::g_scrollPosition = 0;
uint32_t FunctionGeneratorPage::g_version = 0;

FunctionGeneratorPage g_functionGeneratorPage;
eez::gui::SetPage *g_pFunctionGeneratorPage = &g_functionGeneratorPage;

////////////////////////////////////////////////////////////////////////////////

class FunctionGeneratorSelectChannelsPage : public SetPage {
public:
    static const int PAGE_SIZE;

    void pageAlloc() {
		g_selectedChannelsOrig = 0;
		for (int i = 0; i < MAX_NUM_WAVEFORMS; i++) {
			if (FunctionGeneratorPage::g_waveformParameters[i].absoluteResourceIndex != -1) {
				g_selectedChannelsOrig |= ((uint64_t)1 << FunctionGeneratorPage::g_waveformParameters[i].absoluteResourceIndex);
			}
		}
		g_selectedChannels = g_selectedChannelsOrig;

		g_numDlogResources = countDlogResources();
	}

    int getDirty() {
		return g_selectedChannels != g_selectedChannelsOrig;
	}

	void apply() {
		WaveformParameters waveformParameters[MAX_NUM_WAVEFORMS];
		memset(&waveformParameters, 0, sizeof(waveformParameters));

		TriggerMode triggerModes[MAX_NUM_WAVEFORMS];

		int i = 0;
		for (int absoluteResourceIndex = 0; absoluteResourceIndex < 64; absoluteResourceIndex++) {
			if (g_selectedChannels & ((uint64_t)1 << absoluteResourceIndex)) {
				int j;
				for (j = 0; j < MAX_NUM_WAVEFORMS; j++) {
					if (FunctionGeneratorPage::g_waveformParameters[j].absoluteResourceIndex == absoluteResourceIndex) {
						triggerModes[i] = FunctionGeneratorPage::g_triggerModes[j];
						memcpy(&waveformParameters[i], &FunctionGeneratorPage::g_waveformParameters[j], sizeof(WaveformParameters));
						break;
					}
				}

				int slotIndex;
				int subchannelIndex;
				int resourceIndex;
				FunctionGeneratorSelectChannelsPage::findResource(absoluteResourceIndex, slotIndex, subchannelIndex, resourceIndex);

				if (j == MAX_NUM_WAVEFORMS) {
					waveformParameters[i].resourceType = g_slots[slotIndex]->getFunctionGeneratorResourceType(subchannelIndex, resourceIndex);
					if (waveformParameters[i].resourceType == FUNCTION_GENERATOR_RESOURCE_TYPE_U_AND_I) {
						waveformParameters[i].resourceType = FUNCTION_GENERATOR_RESOURCE_TYPE_U;
					}
				}

				float minFreq;
				float maxFreq;
				g_slots[slotIndex]->getFunctionGeneratorFrequencyInfo(subchannelIndex, resourceIndex, minFreq, maxFreq);

				float minAmp;
				float maxAmp;
				g_slots[slotIndex]->getFunctionGeneratorAmplitudeInfo(subchannelIndex, resourceIndex, waveformParameters[i].resourceType, minAmp, maxAmp);

				if (j == MAX_NUM_WAVEFORMS) {
					waveformParameters[i].absoluteResourceIndex = absoluteResourceIndex;
					waveformParameters[i].waveform = waveformParameters[i].resourceType == FUNCTION_GENERATOR_RESOURCE_TYPE_DIGITAL ? WAVEFORM_SQUARE : WAVEFORM_SINE;
					waveformParameters[i].frequency = MIN(100.0f, maxFreq);
					waveformParameters[i].phaseShift = 0;
					waveformParameters[i].amplitude = maxAmp - minAmp;
					waveformParameters[i].offset = (minAmp + maxAmp) / 2.0f;
					waveformParameters[i].dutyCycle = 25.0f;

					triggerModes[i] = TRIGGER_MODE_FUNCTION_GENERATOR;
				} else {
					if (waveformParameters[i].frequency < minFreq) {
						waveformParameters[i].frequency = minFreq;
					} else if (waveformParameters[i].frequency > maxFreq) {
						waveformParameters[i].frequency = maxFreq;
					}

					if (
						waveformParameters[i].offset + waveformParameters[i].amplitude / 2.0f > maxAmp ||
						waveformParameters[i].offset - waveformParameters[i].amplitude / 2.0f < minAmp
						) {
						waveformParameters[i].amplitude = maxAmp - minAmp;
						waveformParameters[i].offset = (minAmp + maxAmp) / 2.0f;
					}
				}

				i++;
			}
		}

		for (; i < MAX_NUM_WAVEFORMS; i++) {
			waveformParameters[i].absoluteResourceIndex = -1;
		}

		memcpy(FunctionGeneratorPage::g_waveformParameters, &waveformParameters, sizeof(waveformParameters));
		memcpy(FunctionGeneratorPage::g_triggerModes, triggerModes, sizeof(triggerModes));

		FunctionGeneratorPage::g_selectedItem = 0;
		FunctionGeneratorPage::setScrollPosition(0);

		FunctionGeneratorPage::apply();
	}

    void set() {
		apply();
		popPage();
	}

	static int getNumFunctionGeneratorResources() {
		if (g_numDlogResources == -1) {
			g_numDlogResources = countDlogResources();
		}
		return g_numDlogResources;
	}

	static int getNumSelectedResources() {
		int numSelectedResources = 0;
		for (int absoluteResourceIndex = 0; absoluteResourceIndex < 64; absoluteResourceIndex++) {
			if (g_selectedChannels & ((uint64_t)1 << absoluteResourceIndex)) {
				numSelectedResources++;
			}
		}
		return numSelectedResources;
	}

	static bool findResource(int absoluteResourceIndex, int &slotIndex, int &subchannelIndex, int &resourceIndex)  {
		int index = 0;
		for (slotIndex = 0; slotIndex < NUM_SLOTS; slotIndex++) {
			int numSubchannels = g_slots[slotIndex]->getNumSubchannels();
			for (subchannelIndex = 0; subchannelIndex < numSubchannels; subchannelIndex++) {
				int numResources = g_slots[slotIndex]->getNumFunctionGeneratorResources(subchannelIndex);
				for (resourceIndex = 0; resourceIndex < numResources; resourceIndex++) {
					if (index == absoluteResourceIndex) {
						return true;
					}
					index++;
				}
			}
		}

		return false;
	}

	static int getAbsoluteResourceIndex(int slotIndexToFound, int subchannelIndexToFound, int resourceIndexToFound) {
		int absoluteResourceIndex = 0;
		for (int slotIndex = 0; slotIndex < NUM_SLOTS; slotIndex++) {
			int numSubchannels = g_slots[slotIndex]->getNumSubchannels();
			for (int subchannelIndex = 0; subchannelIndex < numSubchannels; subchannelIndex++) {
				int numResources = g_slots[slotIndex]->getNumFunctionGeneratorResources(subchannelIndex);
				for (int resourceIndex = 0; resourceIndex < numResources; resourceIndex++) {
					if (
						slotIndex == slotIndexToFound &&
						subchannelIndex == subchannelIndexToFound &&
						resourceIndex == resourceIndexToFound
					) {
						return absoluteResourceIndex;
					}
					absoluteResourceIndex++;
				}
			}
		}

		return -1;
	}

	static int getScrollPosition() {
		return g_scrollPosition;
	}

	static void setScrollPosition(int scrollPosition) {
		if (g_numDlogResources <= PAGE_SIZE) {
			return;
		}

		if (scrollPosition < 0) {
			scrollPosition = 0;
		} else if (scrollPosition > g_numDlogResources - PAGE_SIZE) {
			scrollPosition = g_numDlogResources - PAGE_SIZE;
		}
		
		if (scrollPosition != g_scrollPosition) {
			g_scrollPosition = scrollPosition;
			refreshScreen();
		}
	}

    void onEncoder(int counter)  {
#if defined(EEZ_PLATFORM_SIMULATOR)
		counter = -counter;
#endif
		setScrollPosition(g_scrollPosition + counter);
	}

	static uint64_t g_selectedChannels;

private:
	uint64_t g_selectedChannelsOrig;
	static int g_numDlogResources;
    static int g_scrollPosition;

	static int countDlogResources()  {
		int count = 0;
		for (int slotIndex = 0; slotIndex < NUM_SLOTS; slotIndex++) {
			int numSubchannels = g_slots[slotIndex]->getNumSubchannels();
			for (int subchannelIndex = 0; subchannelIndex < numSubchannels; subchannelIndex++) {
				count += g_slots[slotIndex]->getNumFunctionGeneratorResources(subchannelIndex);
			}
		}
		return count;
	}
};

int FunctionGeneratorSelectChannelsPage::g_numDlogResources = -1;
int FunctionGeneratorSelectChannelsPage::g_scrollPosition = 0;
const int FunctionGeneratorSelectChannelsPage::PAGE_SIZE = 7;
uint64_t FunctionGeneratorSelectChannelsPage::g_selectedChannels = 0;

FunctionGeneratorSelectChannelsPage g_functionGeneratorSelectChannelsPage;
eez::gui::SetPage *g_pFunctionGeneratorSelectChannelsPage = &g_functionGeneratorSelectChannelsPage;

////////////////////////////////////////////////////////////////////////////////

float dcf(float t) {
	return 0.0f;
}

float sineHalff(float t) {
	if (t < M_PI_F) {
		return sinf(t);
	}

	return 0.0f;
}

float sineRectifiedf(float t) {
	if (t < M_PI_F) {
		return sinf(t);
	}

	return sinf(t - M_PI_F);
}

float trianglef(float t) {
	float a, b, c;

	if (t < M_PI_F / 2.0f) {
		a = 0;
		b = 1;
		c = 0;
	} else if (t < 3.0f * M_PI_F / 2.0f) {
		a = 1;
		b = -1;
		c = M_PI_F / 2.0f;
	} else {
		a = -1;
		b = 1;
		c = 3.0f * M_PI_F / 2.0f;
	}

	return a + b * (t - c) / (M_PI_F / 2.0f);
}

float squaref(float t) {
	if (t < M_PI_F) {
		return 1.0f;
	}
	return -1.0f;
}

static float g_dutyCycle;

float pulsef(float t) {
	if (t < g_dutyCycle * 2.0f * M_PI_F / 100.0f) {
		return 1.0f;
	}
	return -1.0f;
}

float sawtoothf(float t) {
	return -1.0f + t / M_PI_F;
}

float arbitraryf(float t) {
	return 0.0f;
}

WaveformFunction getWaveformFunction(WaveformParameters &waveformParameters) {
	if (waveformParameters.waveform == WAVEFORM_DC) {
		return dcf;
	} else if (waveformParameters.waveform == WAVEFORM_SINE) {
		return sinf;
	} else if (waveformParameters.waveform == WAVEFORM_SINE_HALF) {
		return sineHalff;
	} else if (waveformParameters.waveform == WAVEFORM_SINE_RECTIFIED) {
		return sineRectifiedf;
	} else if (waveformParameters.waveform == WAVEFORM_TRIANGLE) {
		return trianglef;
	} else if (waveformParameters.waveform == WAVEFORM_SQUARE) {
		return squaref;
	} else if (waveformParameters.waveform == WAVEFORM_PULSE) {
		g_dutyCycle = waveformParameters.dutyCycle;
		return pulsef;
	} else if (waveformParameters.waveform == WAVEFORM_SAWTOOTH) {
		return sawtoothf;
	} else {
		return arbitraryf;
	}
}

////////////////////////////////////////////////////////////////////////////////

void FunctionGeneratorPage::pageAlloc() {
	memcpy(g_waveformParameters, &function_generator::g_waveformParameters,
		sizeof(function_generator::g_waveformParameters));

	for (int i = 0; i < MAX_NUM_WAVEFORMS; i++) {
		if (FunctionGeneratorPage::g_waveformParameters[i].absoluteResourceIndex == -1) {
			break;
		}

		int slotIndex;
		int subchannelIndex;
		int resourceIndex;
		FunctionGeneratorSelectChannelsPage::findResource(
			FunctionGeneratorPage::g_waveformParameters[i].absoluteResourceIndex,
			slotIndex, subchannelIndex, resourceIndex);

		TriggerMode triggerMode;
		g_slots[slotIndex]->getFunctionGeneratorResourceTriggerMode(subchannelIndex, resourceIndex, triggerMode, nullptr);

		g_triggerModes[i] = triggerMode;
	}

	g_selectedItem = 0;
	g_scrollPosition = 0;
}

void FunctionGeneratorPage::apply() {
	memcpy(function_generator::g_waveformParameters, &g_waveformParameters,
		sizeof(g_waveformParameters));

	for (int i = 0; i < FunctionGeneratorSelectChannelsPage::getNumFunctionGeneratorResources(); i++) {
		int slotIndex;
		int subchannelIndex;
		int resourceIndex;
		FunctionGeneratorSelectChannelsPage::findResource(i, slotIndex, subchannelIndex, resourceIndex);
		g_slots[slotIndex]->setFunctionGeneratorResourceTriggerMode(subchannelIndex, resourceIndex, TRIGGER_MODE_FIXED, nullptr);
	}

	for (int i = 0; i < MAX_NUM_WAVEFORMS; i++) {
		if (FunctionGeneratorPage::g_waveformParameters[i].absoluteResourceIndex == -1) {
			break;
		}

		int slotIndex;
		int subchannelIndex;
		int resourceIndex;
		FunctionGeneratorSelectChannelsPage::findResource(
			FunctionGeneratorPage::g_waveformParameters[i].absoluteResourceIndex,
			slotIndex, subchannelIndex, resourceIndex);

		g_slots[slotIndex]->setFunctionGeneratorResourceTriggerMode(subchannelIndex, resourceIndex, g_triggerModes[i], nullptr);
	}

	FunctionGeneratorPage::g_version++;
}

////////////////////////////////////////////////////////////////////////////////

void FunctionGeneratorPage::draw(const WidgetCursor &widgetCursor) {
	const Widget *widget = widgetCursor.widget;
	const Style* style = getStyle(widget->style);
	drawRectangle(widgetCursor.x, widgetCursor.y, (int)widget->w, (int)widget->h, style, false, false, true);

	float T = FLT_MIN;

	float minU = FLT_MAX;
	float maxU = -FLT_MAX;

	float minI = FLT_MAX;
	float maxI = -FLT_MAX;

	int numDigital = 0;

	for (int i = 0; i < MAX_NUM_WAVEFORMS; i++) {
		if (
			FunctionGeneratorPage::g_waveformParameters[i].absoluteResourceIndex != -1 &&
			FunctionGeneratorPage::g_triggerModes[i] == TRIGGER_MODE_FUNCTION_GENERATOR
		) {
			auto &waveformParameters = FunctionGeneratorPage::g_waveformParameters[i];

			if (waveformParameters.resourceType == FUNCTION_GENERATOR_RESOURCE_TYPE_DIGITAL) {
				++numDigital;
			} else {
				int slotIndex;
				int subchannelIndex;
				int resourceIndex;
				FunctionGeneratorSelectChannelsPage::findResource(FunctionGeneratorPage::g_waveformParameters[i].absoluteResourceIndex,
					slotIndex, subchannelIndex, resourceIndex);

				float lower;
				float upper;
				g_slots[slotIndex]->getFunctionGeneratorAmplitudeInfo(subchannelIndex, resourceIndex, waveformParameters.resourceType, lower, upper);

				if (waveformParameters.resourceType == FUNCTION_GENERATOR_RESOURCE_TYPE_U) {
					if (lower < minU) {
						minU = lower;
					}
					if (upper > maxU) {
						maxU = upper;
					}
				} else {
					if (lower < minI) {
						minI = lower;
					}
					if (upper > maxI) {
						maxI = upper;
					}
				}
			}

			float t = 2 * (1 / waveformParameters.frequency);
			if (t > T) {
				T = t;
			}
		}
	}

	float dU = maxU - minU;
	if (dU == 0) {
		dU = 1.0f;
	}
	minU = minU - dU * 0.05f;
	maxU = maxU + dU * 0.05f;

	float dI = maxI - minI;
	if (dI == 0) {
		dI = 1.0f;
	}
	minI = minI - dI * 0.05f;
	maxI = maxI + dI * 0.05f;

	int itemIndex = 0;
	int digitalIndex = 0;
	int selectedItemIndex = -1;
	int selectedItemDigitalIndex = -1;

	for (int i = 0; i < MAX_NUM_WAVEFORMS; i++) {
		auto &waveformParameters = FunctionGeneratorPage::g_waveformParameters[i];
		auto triggerMode = FunctionGeneratorPage::g_triggerModes[i];

		if (waveformParameters.absoluteResourceIndex != -1 && triggerMode == TRIGGER_MODE_FUNCTION_GENERATOR) {
			if (itemIndex != g_selectedItem) {
				if (waveformParameters.resourceType == FUNCTION_GENERATOR_RESOURCE_TYPE_DIGITAL) {
					auto digitalWaveformParameters = waveformParameters;
					
					digitalWaveformParameters.offset = ((numDigital - digitalIndex - 1) + 0.5f) / numDigital;
					digitalWaveformParameters.amplitude = 1.0f / numDigital - 4.0f / 118.0f;
					
					drawWaveform(widgetCursor, digitalWaveformParameters, T, 0, 1.0f);
				} else {
					float min = waveformParameters.resourceType == FUNCTION_GENERATOR_RESOURCE_TYPE_U ? minU : minI;
					float max = waveformParameters.resourceType == FUNCTION_GENERATOR_RESOURCE_TYPE_U ? maxU : maxI;
					
					drawWaveform(widgetCursor, waveformParameters, T, min, max);
				}
			} else {
				selectedItemIndex = itemIndex;
				selectedItemDigitalIndex = digitalIndex;
			}
			
			itemIndex++;

			if (waveformParameters.resourceType == FUNCTION_GENERATOR_RESOURCE_TYPE_DIGITAL) {
				digitalIndex++;
			}
		}
	}

	auto triggerMode = FunctionGeneratorPage::g_triggerModes[selectedItemIndex];
	if (selectedItemIndex != -1 && triggerMode == TRIGGER_MODE_FUNCTION_GENERATOR) {
		auto &waveformParameters = FunctionGeneratorPage::g_waveformParameters[selectedItemIndex];

		if (waveformParameters.resourceType == FUNCTION_GENERATOR_RESOURCE_TYPE_DIGITAL) {
			auto digitalWaveformParameters = waveformParameters;

			digitalWaveformParameters.offset = ((numDigital - selectedItemDigitalIndex - 1) + 0.5f) / numDigital;
			digitalWaveformParameters.amplitude = 1.0f / numDigital - 4.0f / 118.0f;

			drawWaveform(widgetCursor, digitalWaveformParameters, T, 0, 1.0f, true);
			drawWaveform(widgetCursor, digitalWaveformParameters, T, 0, 1.0f, true, 1);
		} else {
			float min = waveformParameters.resourceType == FUNCTION_GENERATOR_RESOURCE_TYPE_U ? minU : minI;
			float max = waveformParameters.resourceType == FUNCTION_GENERATOR_RESOURCE_TYPE_U ? maxU : maxI;
			drawWaveform(widgetCursor, waveformParameters, T, min, max, true);
			drawWaveform(widgetCursor, waveformParameters, T, min, max, true, 1);
		}
	}

	if (g_active) {
		reloadWaveformParameters();
	}
}

void FunctionGeneratorPage::drawWaveform(const WidgetCursor &widgetCursor, WaveformParameters &waveformParameters, float T, float min, float max, bool selected, int yOffset) {
	const Widget *widget = widgetCursor.widget;
	const Style* style = getStyle(widget->style);
	font::Font font = styleGetFont(style);
	int textHeight = font.getHeight();

	float frequency = waveformParameters.frequency;
	float phaseShift = waveformParameters.phaseShift;
	float amplitude = waveformParameters.amplitude;
	float offset = waveformParameters.offset;

	if (waveformParameters.waveform == WAVEFORM_DC) {
		offset = amplitude;
		amplitude = 0;
	}

	int slotIndex;
	int subchannelIndex;
	int resourceIndex;
	FunctionGeneratorSelectChannelsPage::findResource(waveformParameters.absoluteResourceIndex, slotIndex, subchannelIndex, resourceIndex);

	float lower;
	float upper;
	g_slots[slotIndex]->getFunctionGeneratorAmplitudeInfo(subchannelIndex, resourceIndex, waveformParameters.resourceType, lower, upper);

	auto tmpChannelIndex = g_channelIndex;
	auto tmpSlotIndex = hmi::g_selectedSlotIndex;
	auto tmpSubchannelIndex = hmi::g_selectedSubchannelIndex;
	g_channelIndex = -1;
	hmi::g_selectedSlotIndex = slotIndex;
	hmi::g_selectedSubchannelIndex = subchannelIndex;
	mcu::display::setColor(COLOR_ID_CHANNEL1);
	g_channelIndex = tmpChannelIndex;
	hmi::g_selectedSlotIndex = tmpSlotIndex;
	hmi::g_selectedSubchannelIndex = tmpSubchannelIndex;

	auto func = getWaveformFunction(waveformParameters);

	float d = max - min;

	int xLeft = widgetCursor.x;
	int yBottom = widgetCursor.y + widget->h - 1;

	int yPrev = 0;
	int xPrev = 0;

	for (int xOffset = 0;  xOffset < widget->w; xOffset++) {
		float t = xOffset * T / widget->w;

		int k;

		static const float sampleRate = 1 / 100000.0f;
		k = floorf(t / sampleRate);
		t = k * sampleRate;

		float fi = (2 * frequency * t + phaseShift / 180) * M_PI;

		k = floorf(fi / (2 * M_PI));
		fi = fi - k * (2 * M_PI);

		float yt = offset + amplitude * func(fi) / 2.0f;
		if (yt < lower) {
			yt = lower;
		} else if (yt > upper) {
			yt = upper;
		}

		int xNext = xLeft + xOffset;
		int yNext = yBottom - roundf((yt - min) / d * widget->h) - yOffset;

		if (xOffset == 0 || abs(yNext - yPrev) <= 1) {
			mcu::display::drawPixel(xNext, yNext);
		} else {
			if (yNext < yPrev) {
				mcu::display::drawVLine(xNext, yNext, yPrev - yNext - 1);
			} else {
				mcu::display::drawVLine(xNext, yPrev, yNext - yPrev - 1);
			}
		}

		xPrev = xNext;
		yPrev = yNext;
	}

	if (selected && yOffset == 1) {
		const char *label = g_slots[slotIndex]->getFunctionGeneratorResourceLabel(subchannelIndex, resourceIndex);
		int textWidth = mcu::display::measureStr(label, -1, font, 0);
		mcu::display::drawStr(label, -1,
			xPrev - textWidth, yPrev - textHeight > widgetCursor.y ? yPrev - textHeight : yPrev,
			widgetCursor.x, widgetCursor.y, widgetCursor.x + widget->w - 1, widgetCursor.y + widget->h - 1,
			font, -1
		);
	}
}

////////////////////////////////////////////////////////////////////////////////

WaveformParameters *getWaveformParameters(int slotIndex, int subchannelIndex, int resourceIndex) {
	int absoluteResourceIndex = FunctionGeneratorSelectChannelsPage::getAbsoluteResourceIndex(
		slotIndex, subchannelIndex, resourceIndex
	);

	for (int i = 0; i < MAX_NUM_WAVEFORMS; i++) {
		if (g_waveformParameters[i].absoluteResourceIndex == absoluteResourceIndex) {
			return g_waveformParameters + i;
		}
		if (g_waveformParameters[i].absoluteResourceIndex == -1) {
			break;
		}
	}

	return nullptr;
}

////////////////////////////////////////////////////////////////////////////////

void getProfileParameters(psu::profile::Parameters &profileParams) {
	profileParams.functionGeneratorParameters.options = g_options;

	for (int i = 0; i < MAX_NUM_WAVEFORMS; i++) {
		if (g_waveformParameters[i].absoluteResourceIndex != -1) {
			int slotIndex;
			int subchannelIndex;
			int resourceIndex;
			FunctionGeneratorSelectChannelsPage::findResource(g_waveformParameters[i].absoluteResourceIndex,
				slotIndex, subchannelIndex, resourceIndex);

			auto &profileWaveformParameters = profileParams.functionGeneratorParameters.waveformParameters[i];

			profileWaveformParameters.moduleType = g_slots[slotIndex]->moduleType;
			
			profileWaveformParameters.slotIndex = slotIndex;
			profileWaveformParameters.subchannelIndex = subchannelIndex;
			profileWaveformParameters.resourceIndex = resourceIndex;

			profileWaveformParameters.resourceType = g_waveformParameters[i].resourceType;
			
			profileWaveformParameters.waveform = g_waveformParameters[i].waveform;
			profileWaveformParameters.frequency = g_waveformParameters[i].frequency;
			profileWaveformParameters.phaseShift = g_waveformParameters[i].phaseShift;
			profileWaveformParameters.amplitude = g_waveformParameters[i].amplitude;
			profileWaveformParameters.offset = g_waveformParameters[i].offset;
			profileWaveformParameters.dutyCycle = g_waveformParameters[i].dutyCycle;
		} else {
			profileParams.functionGeneratorParameters.waveformParameters[i].moduleType = MODULE_TYPE_NONE;
		}
	}
}

void setProfileParameters(const psu::profile::Parameters &profileParams) {
	g_options = profileParams.functionGeneratorParameters.options;


	int i;
	for (i = 0; i < MAX_NUM_WAVEFORMS; i++) {
		if (profileParams.functionGeneratorParameters.waveformParameters[i].moduleType != MODULE_TYPE_NONE) {
			auto &profileWaveformParameters = profileParams.functionGeneratorParameters.waveformParameters[i];

			if (profileWaveformParameters.moduleType == MODULE_TYPE_NONE) {
				break;
			}

			if (profileWaveformParameters.moduleType != g_slots[profileWaveformParameters.slotIndex]->moduleType) {
				break;
			}

			int absoluteResourceIndex = FunctionGeneratorSelectChannelsPage::getAbsoluteResourceIndex(
				profileWaveformParameters.slotIndex, profileWaveformParameters.subchannelIndex, profileWaveformParameters.resourceIndex);
			if (absoluteResourceIndex == -1) {
				break;
			}

			g_waveformParameters[i].absoluteResourceIndex = absoluteResourceIndex;

			g_waveformParameters[i].resourceType = (FunctionGeneratorResourceType)profileWaveformParameters.resourceType;
			
			g_waveformParameters[i].waveform = (Waveform)profileWaveformParameters.waveform;
			g_waveformParameters[i].frequency = profileWaveformParameters.frequency;
			g_waveformParameters[i].phaseShift = profileWaveformParameters.phaseShift;
			g_waveformParameters[i].amplitude = profileWaveformParameters.amplitude;
			g_waveformParameters[i].offset = profileWaveformParameters.offset;
			g_waveformParameters[i].dutyCycle = profileWaveformParameters.dutyCycle;
		}
	}

	for (; i < MAX_NUM_WAVEFORMS; i++) {
		g_waveformParameters[i].absoluteResourceIndex = -1;
	}
}

bool writeProfileProperties(psu::profile::WriteContext &ctx, const psu::profile::Parameters &profileParams) {
	ctx.group("funcgen_options");
	WRITE_PROPERTY("isFreq", profileParams.functionGeneratorParameters.options.isFreq);
	WRITE_PROPERTY("isAmpl", profileParams.functionGeneratorParameters.options.isAmpl);

	for (int i = 0; i < MAX_NUM_WAVEFORMS; i++) {
		auto &profileWaveformParameters = profileParams.functionGeneratorParameters.waveformParameters[i];

		if (profileWaveformParameters.moduleType == MODULE_TYPE_NONE) {
			break;
		}

		ctx.group("funcgen_waveform", i + 1);

		WRITE_PROPERTY("moduleType", profileWaveformParameters.moduleType);
		
		WRITE_PROPERTY("slotIndex", profileWaveformParameters.slotIndex);
		WRITE_PROPERTY("subchannelIndex", profileWaveformParameters.subchannelIndex);
		WRITE_PROPERTY("resourceIndex", profileWaveformParameters.resourceIndex);

		WRITE_PROPERTY("resourceType", profileWaveformParameters.resourceType);

		WRITE_PROPERTY("waveform", profileWaveformParameters.waveform);
		WRITE_PROPERTY("frequency", profileWaveformParameters.frequency);
		WRITE_PROPERTY("phaseShift", profileWaveformParameters.phaseShift);
		WRITE_PROPERTY("amplitude", profileWaveformParameters.amplitude);
		WRITE_PROPERTY("offset", profileWaveformParameters.offset);
		WRITE_PROPERTY("dutyCycle", profileWaveformParameters.dutyCycle);
	}

	return true;
}

bool readProfileProperties(psu::profile::ReadContext &ctx, psu::profile::Parameters &profileParams) {
	if (ctx.matchGroup("funcgen_options")) {
		READ_FLAG("isFreq", profileParams.functionGeneratorParameters.options.isFreq);
		READ_FLAG("isAmpl", profileParams.functionGeneratorParameters.options.isAmpl);
	}

    int i;
    if (ctx.matchGroup("funcgen_waveform", i)) {
        --i;

		auto &profileWaveformParameters = profileParams.functionGeneratorParameters.waveformParameters[i];

		READ_PROPERTY("moduleType", profileWaveformParameters.moduleType);
		
		READ_PROPERTY("slotIndex", profileWaveformParameters.slotIndex);
		READ_PROPERTY("subchannelIndex", profileWaveformParameters.subchannelIndex);
		READ_PROPERTY("resourceIndex", profileWaveformParameters.resourceIndex);

		READ_PROPERTY("resourceType", profileWaveformParameters.resourceType);

		READ_PROPERTY("waveform", profileWaveformParameters.waveform);
		READ_PROPERTY("frequency", profileWaveformParameters.frequency);
		READ_PROPERTY("phaseShift", profileWaveformParameters.phaseShift);
		READ_PROPERTY("amplitude", profileWaveformParameters.amplitude);
		READ_PROPERTY("offset", profileWaveformParameters.offset);
		READ_PROPERTY("dutyCycle", profileWaveformParameters.dutyCycle);
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////

bool addChannelWaveformParameters(int slotIndex, int subchannelIndex, int resourceIndex, int *err) {
	g_functionGeneratorPage.pageAlloc();

	g_functionGeneratorSelectChannelsPage.pageAlloc();

	uint64_t selectedChannels = g_functionGeneratorSelectChannelsPage.g_selectedChannels;
	for (int i = 0; i < FunctionGeneratorSelectChannelsPage::getNumFunctionGeneratorResources(); i++) {
		int tmpSlotIndex;
		int tmpSubchannelIndex;
		int tmpResourceIndex;
		FunctionGeneratorSelectChannelsPage::findResource(i, tmpSlotIndex, tmpSubchannelIndex, tmpResourceIndex);
		
		if (tmpSlotIndex == slotIndex && tmpSubchannelIndex == subchannelIndex && (resourceIndex == -1 || tmpResourceIndex == resourceIndex)) {
			selectedChannels |= (uint64_t)1 << i;
		}
	}

	int numSelectedChannels = 0;
	for (int i = 0; i < 64; i++) {
		if (selectedChannels & ((uint64_t)1 << i)) {
			numSelectedChannels++;
		}
	}

	if (numSelectedChannels > MAX_NUM_WAVEFORMS) {
		if (err) {
			*err = SCPI_ERROR_OUT_OF_MEMORY_FOR_REQ_OP;
		}
		return false;
	}

	g_functionGeneratorSelectChannelsPage.g_selectedChannels = selectedChannels;

	g_functionGeneratorSelectChannelsPage.apply();

	gui::refreshScreen();

	return true;
}

void addChannelWaveformParameters(Channel &channel) {
	addChannelWaveformParameters(channel.slotIndex, channel.subchannelIndex, -1);
}

void removeChannelWaveformParameters(int slotIndex, int subchannelIndex, int resourceIndex) {
	g_functionGeneratorPage.pageAlloc();

	g_functionGeneratorSelectChannelsPage.pageAlloc();

	for (int i = 0; i < FunctionGeneratorSelectChannelsPage::getNumFunctionGeneratorResources(); i++) {
		int tmpSlotIndex;
		int tmpSubchannelIndex;
		int tmpResourceIndex;
		FunctionGeneratorSelectChannelsPage::findResource(i, tmpSlotIndex, tmpSubchannelIndex, tmpResourceIndex);
		
		if (tmpSlotIndex == slotIndex && tmpSubchannelIndex == subchannelIndex && tmpResourceIndex == resourceIndex) {
			g_functionGeneratorSelectChannelsPage.g_selectedChannels &= ~((uint64_t)1 << i);
			break;
		}
	}

	g_functionGeneratorSelectChannelsPage.apply();
	
	gui::refreshScreen();
}

void removeAllChannels() {
	g_functionGeneratorPage.pageAlloc();

	g_functionGeneratorSelectChannelsPage.pageAlloc();

	g_functionGeneratorSelectChannelsPage.g_selectedChannels = 0;

	g_functionGeneratorSelectChannelsPage.apply();
}

void removePowerChannels() {
	g_functionGeneratorPage.pageAlloc();

	g_functionGeneratorSelectChannelsPage.pageAlloc();

	for (int i = 0; i < FunctionGeneratorSelectChannelsPage::getNumFunctionGeneratorResources(); i++) {
		int slotIndex;
		int subchannelIndex;
		int resourceIndex;
		FunctionGeneratorSelectChannelsPage::findResource(i, slotIndex, subchannelIndex, resourceIndex);
		
		if (Channel::getBySlotIndex(slotIndex, subchannelIndex)) {
			g_functionGeneratorSelectChannelsPage.g_selectedChannels &= ~((uint64_t)1 << i);
		}
	}

	g_functionGeneratorSelectChannelsPage.apply();
}

void removeOtherTrackingChannels() {
	g_functionGeneratorPage.pageAlloc();

	g_functionGeneratorSelectChannelsPage.pageAlloc();

	int firstTrackingChannel = -1;

	for (int i = 0; i < FunctionGeneratorSelectChannelsPage::getNumFunctionGeneratorResources(); i++) {
		int slotIndex;
		int subchannelIndex;
		int resourceIndex;
		FunctionGeneratorSelectChannelsPage::findResource(i, slotIndex, subchannelIndex, resourceIndex);
		
		auto *channel = Channel::getBySlotIndex(slotIndex, subchannelIndex);
		if (channel) {
			if (g_functionGeneratorSelectChannelsPage.g_selectedChannels & ((uint64_t)1 << i)) {
				if (channel->flags.trackingEnabled) {
					if (firstTrackingChannel != -1 && firstTrackingChannel != channel->channelIndex) {
						g_functionGeneratorSelectChannelsPage.g_selectedChannels &= ~((uint64_t)1 << i);
					} else {
						firstTrackingChannel = channel->channelIndex;
					}
				}
			}
		}
	}

	g_functionGeneratorSelectChannelsPage.apply();
}

void selectWaveformParametersForChannel(int slotIndex, int subchannelIndex, int resourceIndex) {
	for (int i = 0; i < MAX_NUM_WAVEFORMS; i++) {
		if (FunctionGeneratorPage::g_waveformParameters[i].absoluteResourceIndex == -1) {
			break;
		}

		int tmpSlotIndex;
		int tmpSubchannelIndex;
		int tmpResourceIndex;
		FunctionGeneratorSelectChannelsPage::findResource(
			FunctionGeneratorPage::g_waveformParameters[i].absoluteResourceIndex,
			tmpSlotIndex, tmpSubchannelIndex, tmpResourceIndex);

		if (tmpSlotIndex == slotIndex && tmpSubchannelIndex == subchannelIndex && (resourceIndex == -1 || tmpResourceIndex == resourceIndex)) {
			FunctionGeneratorPage::g_selectedItem = i;
			if (i >= FunctionGeneratorPage::PAGE_SIZE) {
				FunctionGeneratorPage::setScrollPosition(i - FunctionGeneratorPage::PAGE_SIZE + 1);
			}
			break;
		}
	}
}

void selectWaveformParametersForChannel(Channel &channel) {
	selectWaveformParametersForChannel(channel.slotIndex, channel.subchannelIndex, -1);
}

int getNumChannelsInFunctionGeneratorTriggerMode() {
	int count = 0;
	for (int absoluteResourceIndex = 0; absoluteResourceIndex < FunctionGeneratorSelectChannelsPage::getNumFunctionGeneratorResources(); absoluteResourceIndex++) {
		int slotIndex;
		int subchannelIndex;
		int resourceIndex;
		FunctionGeneratorSelectChannelsPage::findResource(
			absoluteResourceIndex,
			slotIndex, subchannelIndex, resourceIndex);

		TriggerMode triggerMode;
		g_slots[slotIndex]->getFunctionGeneratorResourceTriggerMode(subchannelIndex, resourceIndex, triggerMode, nullptr);

		if (triggerMode == TRIGGER_MODE_FUNCTION_GENERATOR) {
			count++;
		}
	}
	return count;
}

bool getWaveform(int slotIndex, int subchannelIndex, int resourceIndex, Waveform &waveform, int *err) {
	WaveformParameters *waveformParameters = getWaveformParameters(slotIndex, subchannelIndex, resourceIndex);
	if (!waveformParameters) {
		if (err) {
			*err = SCPI_ERROR_QUERY_ERROR;
		}
		return false;
	}
	waveform = waveformParameters->waveform;
	return true;
}

bool setWaveform(int slotIndex, int subchannelIndex, int resourceIndex, Waveform waveform, int *err) {
	if (!addChannelWaveformParameters(slotIndex, subchannelIndex, resourceIndex, err)) {
		return false;
	}
	WaveformParameters *waveformParameters = getWaveformParameters(slotIndex, subchannelIndex, resourceIndex);
	if (waveformParameters->resourceType == FUNCTION_GENERATOR_RESOURCE_TYPE_DIGITAL) {
		if (waveform != WAVEFORM_SQUARE && waveform != WAVEFORM_PULSE) {
			if (err) {
				*err = SCPI_ERROR_DATA_OUT_OF_RANGE;
			}
			return false;
		}
	}
	
	if (waveformParameters->waveform == WAVEFORM_DC) {
		return true;
	} else {
		waveformParameters->offset = 0;
	}
	
	waveformParameters->waveform = waveform;

	g_functionGeneratorPage.pageAlloc();

	return true;
}

bool getFrequency(int slotIndex, int subchannelIndex, int resourceIndex, float &frequency, int *err) {
	WaveformParameters *waveformParameters = getWaveformParameters(slotIndex, subchannelIndex, resourceIndex);
	if (!waveformParameters) {
		if (err) {
			*err = SCPI_ERROR_QUERY_ERROR;
		}
		return false;
	}
	frequency = waveformParameters->frequency;
	return true;
}

bool setFrequency(int slotIndex, int subchannelIndex, int resourceIndex, float frequency, int *err) {
	if (!addChannelWaveformParameters(slotIndex, subchannelIndex, resourceIndex, err)) {
		return false;
	}

	WaveformParameters *waveformParameters = getWaveformParameters(slotIndex, subchannelIndex, resourceIndex);

	float min;
	float max;
	g_slots[slotIndex]->getFunctionGeneratorFrequencyInfo(subchannelIndex, resourceIndex, min, max);

	if (frequency < min || frequency > max) {
		if (err) {
			*err = SCPI_ERROR_DATA_OUT_OF_RANGE;
		}
		return false;
	}

	waveformParameters->frequency = frequency;
	
	g_functionGeneratorPage.pageAlloc();

	return true;
}

bool getPhaseShift(int slotIndex, int subchannelIndex, int resourceIndex, float &phaseShift, int *err) {
	WaveformParameters *waveformParameters = getWaveformParameters(slotIndex, subchannelIndex, resourceIndex);
	if (!waveformParameters) {
		if (err) {
			*err = SCPI_ERROR_QUERY_ERROR;
		}
		return false;
	}
	phaseShift = waveformParameters->phaseShift;
	return true;
}

bool setPhaseShift(int slotIndex, int subchannelIndex, int resourceIndex, float phaseShift, int *err) {
	if (!addChannelWaveformParameters(slotIndex, subchannelIndex, resourceIndex, err)) {
		return false;
	}
	
	WaveformParameters *waveformParameters = getWaveformParameters(slotIndex, subchannelIndex, resourceIndex);
	
	if (phaseShift < 0 || phaseShift > 360.0f) {
		if (err) {
			*err = SCPI_ERROR_DATA_OUT_OF_RANGE;
		}
		return false;
	}

	waveformParameters->phaseShift = phaseShift;

	g_functionGeneratorPage.pageAlloc();
	
	return true;
}

bool getAmplitude(int slotIndex, int subchannelIndex, int resourceIndex, float &amplitude, int *err) {
	WaveformParameters *waveformParameters = getWaveformParameters(slotIndex, subchannelIndex, resourceIndex);

	if (!waveformParameters) {
		if (err) {
			*err = SCPI_ERROR_QUERY_ERROR;
		}
		return false;
	}

	if (waveformParameters->resourceType == FUNCTION_GENERATOR_RESOURCE_TYPE_DIGITAL) {
		if (err) {
			*err = SCPI_ERROR_QUERY_ERROR;
		}
		return false;
	}
	
	amplitude = waveformParameters->amplitude;

	return true;
}

bool setAmplitude(int slotIndex, int subchannelIndex, int resourceIndex, float amplitude, int *err) {
	if (!addChannelWaveformParameters(slotIndex, subchannelIndex, resourceIndex, err)) {
		return false;
	}
	
	WaveformParameters *waveformParameters = getWaveformParameters(slotIndex, subchannelIndex, resourceIndex);
	
	if (waveformParameters->resourceType == FUNCTION_GENERATOR_RESOURCE_TYPE_DIGITAL) {
		if (err) {
			*err = SCPI_ERROR_EXECUTION_ERROR;
		}
		return false;
	}

	float min;
	float max;
	g_slots[slotIndex]->getFunctionGeneratorAmplitudeInfo(subchannelIndex, resourceIndex, waveformParameters->resourceType, min, max);

	if (waveformParameters->waveform == WAVEFORM_DC) {
		if (amplitude < min || amplitude > max) {
			if (err) {
				*err = SCPI_ERROR_DATA_OUT_OF_RANGE;
			}
			return false;
		}
	} else {
		float maxAmplitude = max - min;

		if (amplitude < 0 || amplitude > maxAmplitude) {
			if (err) {
				*err = SCPI_ERROR_DATA_OUT_OF_RANGE;
			}
			return false;
		}
	}
	
	waveformParameters->amplitude = amplitude;

	g_functionGeneratorPage.pageAlloc();
	
	return true;
}

bool getOffset(int slotIndex, int subchannelIndex, int resourceIndex, float &offset, int *err) {
	WaveformParameters *waveformParameters = getWaveformParameters(slotIndex, subchannelIndex, resourceIndex);

	if (!waveformParameters) {
		if (err) {
			*err = SCPI_ERROR_QUERY_ERROR;
		}
		return false;
	}

	if (waveformParameters->resourceType == FUNCTION_GENERATOR_RESOURCE_TYPE_DIGITAL) {
		if (err) {
			*err = SCPI_ERROR_QUERY_ERROR;
		}
		return false;
	}

	offset = waveformParameters->offset;

	return true;
}

bool setOffset(int slotIndex, int subchannelIndex, int resourceIndex, float offset, int *err) {
	if (!addChannelWaveformParameters(slotIndex, subchannelIndex, resourceIndex, err)) {
		return false;
	}
	
	WaveformParameters *waveformParameters = getWaveformParameters(slotIndex, subchannelIndex, resourceIndex);
	
	if (waveformParameters->resourceType == FUNCTION_GENERATOR_RESOURCE_TYPE_DIGITAL) {
		if (err) {
			*err = SCPI_ERROR_EXECUTION_ERROR;
		}
		return false;
	}

	float min;
	float max;
	g_slots[slotIndex]->getFunctionGeneratorAmplitudeInfo(subchannelIndex, resourceIndex, waveformParameters->resourceType, min, max);

	if (offset < min || offset > max) {
		if (err) {
			*err = SCPI_ERROR_DATA_OUT_OF_RANGE;
		}
		return false;
	}
	
	waveformParameters->offset = offset;

	g_functionGeneratorPage.pageAlloc();

	return true;
}

bool getDutyCycle(int slotIndex, int subchannelIndex, int resourceIndex, float &dutyCycle, int *err) {
	WaveformParameters *waveformParameters = getWaveformParameters(slotIndex, subchannelIndex, resourceIndex);
	if (!waveformParameters) {
		if (err) {
			*err = SCPI_ERROR_QUERY_ERROR;
		}
		return false;
	}
	dutyCycle = waveformParameters->dutyCycle;
	return true;
}

bool setDutyCycle(int slotIndex, int subchannelIndex, int resourceIndex, float dutyCycle, int *err) {
	if (!addChannelWaveformParameters(slotIndex, subchannelIndex, resourceIndex, err)) {
		return false;
	}
	
	WaveformParameters *waveformParameters = getWaveformParameters(slotIndex, subchannelIndex, resourceIndex);
	
	if (dutyCycle < 0 || dutyCycle > 100.0f) {
		if (err) {
			*err = SCPI_ERROR_DATA_OUT_OF_RANGE;
		}
		return false;
	}

	waveformParameters->dutyCycle = dutyCycle;

	g_functionGeneratorPage.pageAlloc();
	
	return true;
}

bool getResourceType(int slotIndex, int subchannelIndex, int resourceIndex, FunctionGeneratorResourceType &resourceType, int *err) {
	WaveformParameters *waveformParameters = getWaveformParameters(slotIndex, subchannelIndex, resourceIndex);

	if (!waveformParameters) {
		if (err) {
			*err = SCPI_ERROR_QUERY_ERROR;
		}
		return false;
	}

	if (g_slots[slotIndex]->getFunctionGeneratorResourceType(subchannelIndex, resourceIndex) != FUNCTION_GENERATOR_RESOURCE_TYPE_U_AND_I) {
		if (err) {
			*err = SCPI_ERROR_QUERY_ERROR;
		}
		return false;
	}

	resourceType = waveformParameters->resourceType;

	return true;
}

bool setResourceType(int slotIndex, int subchannelIndex, int resourceIndex, FunctionGeneratorResourceType resourceType, int *err) {
	if (!addChannelWaveformParameters(slotIndex, subchannelIndex, resourceIndex, err)) {
		return false;
	}
	
	WaveformParameters *waveformParameters = getWaveformParameters(slotIndex, subchannelIndex, resourceIndex);
	
	if (g_slots[slotIndex]->getFunctionGeneratorResourceType(subchannelIndex, resourceIndex) != FUNCTION_GENERATOR_RESOURCE_TYPE_U_AND_I) {
		if (err) {
			*err = SCPI_ERROR_EXECUTION_ERROR;
		}
		return false;
	}

	float minFreq;
	float maxFreq;
	g_slots[slotIndex]->getFunctionGeneratorFrequencyInfo(subchannelIndex, resourceIndex, minFreq, maxFreq);

	if (waveformParameters->frequency < minFreq) {
		waveformParameters->frequency = minFreq;
	} else if (waveformParameters->frequency > minFreq) {
		waveformParameters->frequency = maxFreq;
	}

	float minAmpl;
	float maxAmpl;
	g_slots[slotIndex]->getFunctionGeneratorAmplitudeInfo(subchannelIndex, resourceIndex, waveformParameters->resourceType, minAmpl, maxAmpl);
	if (waveformParameters->frequency > maxAmpl - minAmpl) {
		waveformParameters->frequency = maxAmpl - minAmpl;
	}
	if (waveformParameters->waveform != WAVEFORM_DC) {
		if (waveformParameters->offset < minAmpl) {
			waveformParameters->offset = minAmpl;
		} else if (waveformParameters->offset > maxAmpl) {
			waveformParameters->offset = maxAmpl;
		}
	}

 	waveformParameters->resourceType = resourceType;

	g_functionGeneratorPage.pageAlloc();

	return true;
}

void reset() {
	g_options.isFreq = 1;
	g_options.isAmpl = 1;
	removeAllChannels();
}

bool isActive() {
	return g_active;
}

void executionStart() {
	g_functionGeneratorPage.pageAlloc();
	g_functionGeneratorSelectChannelsPage.pageAlloc();

	for (int i = 0; i < MAX_NUM_WAVEFORMS; i++) {
		if (FunctionGeneratorPage::g_waveformParameters[i].absoluteResourceIndex == -1) {
			break;
		}

		int slotIndex;
		int subchannelIndex;
		int resourceIndex;
		FunctionGeneratorSelectChannelsPage::findResource(
			FunctionGeneratorPage::g_waveformParameters[i].absoluteResourceIndex,
			slotIndex, subchannelIndex, resourceIndex);

		TriggerMode triggerMode;
		g_slots[slotIndex]->getFunctionGeneratorResourceTriggerMode(subchannelIndex, resourceIndex, triggerMode, nullptr);

		if (triggerMode == TRIGGER_MODE_FUNCTION_GENERATOR) {
			g_active = true;
			g_functionGeneratorPage.pageAlloc();

			reloadWaveformParameters();

			return;
		}
	}
}

void reloadWaveformParameters() {
	bool trackingChannelsStarted = false;

	for (int i = 0; i < CH_MAX; i++) {
		g_waveFormFuncU[i] = dcf;
		g_phiU[i] = 0;
		g_dphiU[i] = 0;
		g_amplitudeU[i] = 0;
		g_offsetU[i] = 0;

		g_waveFormFuncI[i] = dcf;
		g_phiI[i] = 0;
		g_dphiI[i] = 0;
		g_amplitudeI[i] = 0;
		g_offsetI[i] = 0;
	}

	for (int i = 0; i < MAX_NUM_WAVEFORMS; i++) {
		if (FunctionGeneratorPage::g_waveformParameters[i].absoluteResourceIndex == -1) {
			break;
		}

		int slotIndex;
		int subchannelIndex;
		int resourceIndex;
		FunctionGeneratorSelectChannelsPage::findResource(
			FunctionGeneratorPage::g_waveformParameters[i].absoluteResourceIndex,
			slotIndex, subchannelIndex, resourceIndex);

		auto channel = Channel::getBySlotIndex(slotIndex, subchannelIndex);

		if (
			channel &&
			(
				channel->flags.voltageTriggerMode == TRIGGER_MODE_FUNCTION_GENERATOR ||
				channel->flags.currentTriggerMode == TRIGGER_MODE_FUNCTION_GENERATOR
			)
		) {
			if (i == 1 && (channel_dispatcher::getCouplingType() == channel_dispatcher::COUPLING_TYPE_PARALLEL || channel_dispatcher::getCouplingType() == channel_dispatcher::COUPLING_TYPE_SERIES)) {
				continue;
			}

			if (channel->flags.trackingEnabled) {
				if (trackingChannelsStarted) {
					continue;
				}
				trackingChannelsStarted = true;
			}

			auto &waveformParameters = g_waveformParameters[i];

#if defined(EEZ_PLATFORM_STM32)
			__disable_irq();
#endif
			if (waveformParameters.resourceType == FUNCTION_GENERATOR_RESOURCE_TYPE_U) {
				g_waveFormFuncU[channel->channelIndex] = getWaveformFunction(waveformParameters);
				g_dutyCycleU[channel->channelIndex] = g_dutyCycle;
				g_phiU[channel->channelIndex] = waveformParameters.phaseShift / 360.0f;
				g_dphiU[channel->channelIndex] = 2.0f * M_PI_F * waveformParameters.frequency * PERIOD;

				if (waveformParameters.waveform == WAVEFORM_DC) {
					g_amplitudeU[channel->channelIndex] = 0.0f;
					g_offsetU[channel->channelIndex] = waveformParameters.amplitude;
				} else {
					g_amplitudeU[channel->channelIndex] = waveformParameters.amplitude;
					g_offsetU[channel->channelIndex] = waveformParameters.offset;
				}
			} else {
				g_waveFormFuncI[channel->channelIndex] = getWaveformFunction(waveformParameters);
				g_dutyCycleI[channel->channelIndex] = g_dutyCycle;
				g_phiI[channel->channelIndex] = waveformParameters.phaseShift / 360.0f;
				g_dphiI[channel->channelIndex] = 2.0f * M_PI_F * waveformParameters.frequency * PERIOD;
				g_amplitudeI[channel->channelIndex] = waveformParameters.amplitude;
				g_offsetI[channel->channelIndex] = waveformParameters.offset;

				if (waveformParameters.waveform == WAVEFORM_DC) {
					g_amplitudeI[channel->channelIndex] = 0.0f;
					g_offsetI[channel->channelIndex] = waveformParameters.amplitude;
				} else {
					g_amplitudeI[channel->channelIndex] = waveformParameters.amplitude;
					g_offsetI[channel->channelIndex] = waveformParameters.offset;
				}

				if (g_slots[slotIndex]->moduleType == MODULE_TYPE_DCP405) {
					float max = waveformParameters.offset + waveformParameters.amplitude / 2.0f;
					if (max > 0.05f) {
						channel_dispatcher::setCurrentRangeSelectionMode(*channel, CURRENT_RANGE_SELECTION_ALWAYS_HIGH);
					} else {
						channel_dispatcher::setCurrentRangeSelectionMode(*channel, CURRENT_RANGE_SELECTION_ALWAYS_LOW);
					}
				}
			}
#if defined(EEZ_PLATFORM_STM32)
			__enable_irq();
#endif
		}
	}
}

void tick() {
	if (!g_active) {
		return;
	}

	bool trackingChannelsStarted = false;

	for (int i = 0; i < CH_NUM; i++) {
		Channel &channel = Channel::get(i);

        if (channel.flags.trackingEnabled) {
            if (trackingChannelsStarted) {
                continue;
            }
            trackingChannelsStarted = true;
        }

		if (channel.flags.voltageTriggerMode == TRIGGER_MODE_FUNCTION_GENERATOR) {
			g_dutyCycle = g_dutyCycleU[i];
			float value = g_offsetU[i] + g_amplitudeU[i] * g_waveFormFuncU[i](g_phiU[i]) / 2.0f;

			g_phiU[i] += g_dphiU[i];
			if (g_phiU[i] >= 2.0f * M_PI_F) {
				g_phiU[i] -= 2.0f * M_PI_F;
			}

			channel_dispatcher::setVoltage(channel, value);
		}

		if (channel.flags.currentTriggerMode == TRIGGER_MODE_FUNCTION_GENERATOR) {
			g_dutyCycle = g_dutyCycleI[i];
			float value = g_offsetI[i] + g_amplitudeI[i] * g_waveFormFuncI[i](g_phiU[i]) / 2.0f;

			g_phiI[i] += g_dphiI[i];
			if (g_phiI[i] >= 2.0f * M_PI_F) {
				g_phiI[i] -= 2.0f * M_PI_F;
			}

			channel_dispatcher::setCurrent(channel, value);
		}
	}
}

void abort() {
	g_active = false;
}

} // namespace function_generator
} // namespace eez

////////////////////////////////////////////////////////////////////////////////


namespace eez {
namespace gui {

void action_show_sys_settings_function_generator() {
	pushPage(PAGE_ID_SYS_SETTINGS_FUNCTION_GENERATOR);
}

void action_show_sys_settings_function_generator_select_channel() {
	Page *page = getActivePage();
	if (page && page->getDirty()) {
		page->set();
	}
	
	pushPage(PAGE_ID_SYS_SETTINGS_FUNCTION_GENERATOR);

	selectWaveformParametersForChannel(*g_channel);
}

void data_function_generator_channels(DataOperationEnum operation, Cursor cursor, Value &value) {
	if (getActivePageId() == PAGE_ID_SYS_SETTINGS_FUNCTION_GENERATOR) {
		if (operation == DATA_OPERATION_COUNT) {
			value = FunctionGeneratorPage::getNumFunctionGeneratorResources();
		}  else if (operation == DATA_OPERATION_SELECT) {
			int slotIndex;
			int subchannelIndex;
			int resourceIndex;
			if (
				FunctionGeneratorSelectChannelsPage::findResource(
					FunctionGeneratorPage::g_waveformParameters[cursor].absoluteResourceIndex,
					slotIndex, subchannelIndex, resourceIndex
				)
			) {
				value.type_ = VALUE_TYPE_CHANNEL_ID;
				value.pairOfUint16_.first = slotIndex;
				value.pairOfUint16_.second = subchannelIndex;
				
				hmi::g_selectedSlotIndex = slotIndex;
				hmi::g_selectedSubchannelIndex = subchannelIndex;
			}
		} else if (operation == DATA_OPERATION_DESELECT) {
			if (value.getType() == VALUE_TYPE_CHANNEL_ID) {
				hmi::g_selectedSlotIndex = value.pairOfUint16_.first;
				hmi::g_selectedSubchannelIndex = value.pairOfUint16_.second;
			}
		} else if (operation == DATA_OPERATION_YT_DATA_GET_SIZE) {
			value = Value(FunctionGeneratorPage::getNumFunctionGeneratorResources(), VALUE_TYPE_UINT32);
		} else if (operation == DATA_OPERATION_YT_DATA_GET_POSITION) {
			value = Value(FunctionGeneratorPage::getScrollPosition(), VALUE_TYPE_UINT32);
		} else if (operation == DATA_OPERATION_YT_DATA_SET_POSITION) {
			FunctionGeneratorPage::setScrollPosition((int)value.getUInt32());
		} else if (operation == DATA_OPERATION_YT_DATA_GET_POSITION_INCREMENT) {
			value = 1;
		} else if (operation == DATA_OPERATION_YT_DATA_GET_PAGE_SIZE) {
			value = FunctionGeneratorPage::PAGE_SIZE;
		} 
		// scrollbar encoder support
		else if (operation == DATA_OPERATION_GET) {
			value = MakeValue(1.0f * FunctionGeneratorPage::getScrollPosition(), UNIT_NONE);
		} else if (operation == DATA_OPERATION_GET_MIN) {
			value = MakeValue(0, UNIT_NONE);
		} else if (operation == DATA_OPERATION_GET_MAX) {
			value = MakeValue(1.0f * (FunctionGeneratorPage::getNumFunctionGeneratorResources() - FunctionGeneratorPage::PAGE_SIZE), UNIT_NONE);
		} else if (operation == DATA_OPERATION_GET_ENCODER_STEP_VALUES) {
			auto stepValues = value.getStepValues();

			static float values[] = { 1.0f, 1.0f * FunctionGeneratorPage::PAGE_SIZE, 2.0f * FunctionGeneratorPage::PAGE_SIZE, 5.0f * FunctionGeneratorPage::PAGE_SIZE, 10.0f * FunctionGeneratorPage::PAGE_SIZE };

			stepValues->values = values;
			stepValues->count = sizeof(values) / sizeof(float);
			stepValues->unit = UNIT_NONE;

			stepValues->encoderSettings.accelerationEnabled = true;
			stepValues->encoderSettings.range = 10.0f * FunctionGeneratorPage::PAGE_SIZE;
			stepValues->encoderSettings.step = 1.0f;
			stepValues->encoderSettings.mode = edit_mode_step::g_scrollBarEncoderMode;

			value = 1;
		} else if (operation == DATA_OPERATION_SET_ENCODER_MODE) {
			psu::gui::edit_mode_step::g_scrollBarEncoderMode = (EncoderMode)value.getInt();
		} else if (operation == DATA_OPERATION_SET) {
			FunctionGeneratorPage::setScrollPosition((uint32_t)value.getFloat());
		}
	} else if (getActivePageId() == PAGE_ID_SYS_SETTINGS_FUNCTION_GENERATOR_SELECT_CHANNELS) {
		if (operation == DATA_OPERATION_COUNT) {
			value = FunctionGeneratorSelectChannelsPage::getNumFunctionGeneratorResources();
		}  else if (operation == DATA_OPERATION_SELECT) {
			int slotIndex;
			int subchannelIndex;
			int resourceIndex;
			if (FunctionGeneratorSelectChannelsPage::findResource(cursor, slotIndex, subchannelIndex, resourceIndex)) {
				value.type_ = VALUE_TYPE_CHANNEL_ID;
				value.pairOfUint16_.first = slotIndex;
				value.pairOfUint16_.second = subchannelIndex;
				
				hmi::g_selectedSlotIndex = slotIndex;
				hmi::g_selectedSubchannelIndex = subchannelIndex;
			}
		} else if (operation == DATA_OPERATION_DESELECT) {
			if (value.getType() == VALUE_TYPE_CHANNEL_ID) {
				hmi::g_selectedSlotIndex = value.pairOfUint16_.first;
				hmi::g_selectedSubchannelIndex = value.pairOfUint16_.second;
			}
		} else if (operation == DATA_OPERATION_YT_DATA_GET_SIZE) {
			value = Value(FunctionGeneratorSelectChannelsPage::getNumFunctionGeneratorResources(), VALUE_TYPE_UINT32);
		} else if (operation == DATA_OPERATION_YT_DATA_GET_POSITION) {
			value = Value(FunctionGeneratorSelectChannelsPage::getScrollPosition(), VALUE_TYPE_UINT32);
		} else if (operation == DATA_OPERATION_YT_DATA_SET_POSITION) {
			FunctionGeneratorSelectChannelsPage::setScrollPosition((int)value.getUInt32());
		} else if (operation == DATA_OPERATION_YT_DATA_GET_POSITION_INCREMENT) {
			value = 1;
		} else if (operation == DATA_OPERATION_YT_DATA_GET_PAGE_SIZE) {
			value = FunctionGeneratorSelectChannelsPage::PAGE_SIZE;
		}
	}
}

void data_function_generator_canvas(DataOperationEnum operation, Cursor cursor, Value &value) {
	if (operation == DATA_OPERATION_GET) {
		value = FunctionGeneratorPage::getRefreshState();
	} else if (operation == DATA_OPERATION_GET_CANVAS_DRAW_FUNCTION) {
		value = Value((void *)FunctionGeneratorPage::draw, VALUE_TYPE_POINTER);
	} 
}

void data_function_generator_channel(DataOperationEnum operation, Cursor cursor, Value &value) {
	if (getActivePageId() == PAGE_ID_SYS_SETTINGS_FUNCTION_GENERATOR) {
		if (operation == DATA_OPERATION_GET) {
			int slotIndex;
			int subchannelIndex;
			int resourceIndex;
			if (
				FunctionGeneratorSelectChannelsPage::findResource(
					FunctionGeneratorPage::g_waveformParameters[cursor].absoluteResourceIndex,
					slotIndex, subchannelIndex, resourceIndex
				)
			) {
				value.type_ = VALUE_TYPE_CHANNEL_ID;
				value.pairOfUint16_.first = slotIndex;
				value.pairOfUint16_.second = subchannelIndex;
			}
		}
	} else if (getActivePageId() == PAGE_ID_SYS_SETTINGS_FUNCTION_GENERATOR_SELECT_CHANNELS) {
		if (operation == DATA_OPERATION_GET) {
			int slotIndex;
			int subchannelIndex;
			int resourceIndex;
			if (FunctionGeneratorSelectChannelsPage::findResource(cursor, slotIndex, subchannelIndex, resourceIndex)) {
				value.type_ = VALUE_TYPE_CHANNEL_ID;
				value.pairOfUint16_.first = slotIndex;
				value.pairOfUint16_.second = subchannelIndex;
			}
		}
	}
}

void data_function_generator_any_channel_selected(DataOperationEnum operation, Cursor cursor, Value &value) {
	if (getActivePageId() == PAGE_ID_SYS_SETTINGS_FUNCTION_GENERATOR) {
		if (operation == DATA_OPERATION_GET) {
			value = FunctionGeneratorPage::g_selectedItem >= 0 && 
				FunctionGeneratorPage::g_selectedItem < FunctionGeneratorPage::getNumFunctionGeneratorResources();
		}
	}
}

void data_function_generator_item_label(DataOperationEnum operation, Cursor cursor, Value &value) {
	if (getActivePageId() == PAGE_ID_SYS_SETTINGS_FUNCTION_GENERATOR) {
		if (operation == DATA_OPERATION_GET) {
			int slotIndex;
			int subchannelIndex;
			int resourceIndex;
			if (
				FunctionGeneratorSelectChannelsPage::findResource(
					FunctionGeneratorPage::g_waveformParameters[cursor].absoluteResourceIndex,
					slotIndex, subchannelIndex, resourceIndex
				)
			) {
				value = g_slots[slotIndex]->getFunctionGeneratorResourceLabel(subchannelIndex, resourceIndex);
			}
		}
	} else if (getActivePageId() == PAGE_ID_SYS_SETTINGS_FUNCTION_GENERATOR_SELECT_CHANNELS) {
		if (operation == DATA_OPERATION_GET) {
			int slotIndex;
			int subchannelIndex;
			int resourceIndex;
			if (FunctionGeneratorSelectChannelsPage::findResource(cursor, slotIndex, subchannelIndex, resourceIndex)) {
				value = g_slots[slotIndex]->getFunctionGeneratorResourceLabel(subchannelIndex, resourceIndex);
			}
		}
	}
}

void data_function_generator_item_is_selected(DataOperationEnum operation, Cursor cursor, Value &value) {
		if (operation == DATA_OPERATION_GET) {
			value = FunctionGeneratorPage::g_selectedItem == cursor;
		}
}

void action_function_generator_item_toggle_selected() {
	auto resourceIndex = getFoundWidgetAtDown().cursor;
	FunctionGeneratorPage::g_selectedItem = resourceIndex;
	FunctionGeneratorPage::apply();
}

void data_function_generator_item_is_checked(DataOperationEnum operation, Cursor cursor, Value &value) {
	if (getActivePageId() == PAGE_ID_SYS_SETTINGS_FUNCTION_GENERATOR) {
		if (operation == DATA_OPERATION_GET) {
			value = FunctionGeneratorPage::g_triggerModes[cursor] == TRIGGER_MODE_FUNCTION_GENERATOR;
		}
	} else if (getActivePageId() == PAGE_ID_SYS_SETTINGS_FUNCTION_GENERATOR_SELECT_CHANNELS) {
		if (operation == DATA_OPERATION_GET) {
			value = FunctionGeneratorSelectChannelsPage::g_selectedChannels & ((uint64_t)1 << cursor) ? 1 : 0;
		}
	}
}

void data_function_generator_is_max_selected(DataOperationEnum operation, Cursor cursor, Value &value) {
	if (operation == DATA_OPERATION_GET) {
		value = FunctionGeneratorSelectChannelsPage::getNumSelectedResources() == MAX_NUM_WAVEFORMS;
	}
}

void data_function_generator_num_selected(DataOperationEnum operation, Cursor cursor, Value &value) {
	if (operation == DATA_OPERATION_GET) {
		value.type_ = VALUE_TYPE_NUM_SELECTED;
		value.pairOfUint16_.first = FunctionGeneratorSelectChannelsPage::getNumSelectedResources();
		value.pairOfUint16_.second = MIN(MAX_NUM_WAVEFORMS, FunctionGeneratorSelectChannelsPage::getNumFunctionGeneratorResources());
	}
}

void action_function_generator_item_toggle_checked() {
	if (getActivePageId() == PAGE_ID_SYS_SETTINGS_FUNCTION_GENERATOR) {
		auto index = getFoundWidgetAtDown().cursor;
		if (FunctionGeneratorPage::g_triggerModes[index] == TRIGGER_MODE_FUNCTION_GENERATOR) {
			FunctionGeneratorPage::g_triggerModes[index] = TRIGGER_MODE_FIXED;
		} else {
			FunctionGeneratorPage::g_triggerModes[index] = TRIGGER_MODE_FUNCTION_GENERATOR;
		}
		FunctionGeneratorPage::apply();
	} else if (getActivePageId() == PAGE_ID_SYS_SETTINGS_FUNCTION_GENERATOR_SELECT_CHANNELS) {
		auto resourceIndex = getFoundWidgetAtDown().cursor;
		FunctionGeneratorSelectChannelsPage::g_selectedChannels ^= (uint64_t)1 << resourceIndex;
	}
}

void data_function_generator_waveform(DataOperationEnum operation, Cursor cursor, Value &value) {
	if (operation == DATA_OPERATION_GET) {
		value = FunctionGeneratorPage::g_waveformParameters[FunctionGeneratorPage::g_selectedItem].waveform;
	}
}

void data_function_generator_waveform_label(DataOperationEnum operation, Cursor cursor, Value &value) {
	if (operation == DATA_OPERATION_GET) {
		value = g_waveformEnumDefinition[FunctionGeneratorPage::g_waveformParameters[FunctionGeneratorPage::g_selectedItem].waveform - 1].menuLabel;
	}
}

void data_function_generator_waveform_short_label(DataOperationEnum operation, Cursor cursor, Value &value) {
	if (operation == DATA_OPERATION_GET) {
		value = g_waveformShortLabel[FunctionGeneratorPage::g_waveformParameters[FunctionGeneratorPage::g_selectedItem].waveform];
	}
}

void setWaveform(uint16_t value) {
	popPage();

	auto &waveformParameters = FunctionGeneratorPage::g_waveformParameters[FunctionGeneratorPage::g_selectedItem];

	auto oldWaveform = waveformParameters.waveform;
	auto newWaveform = (Waveform)value;

	if (newWaveform == WAVEFORM_DC) {
		waveformParameters.amplitude = 0;
		waveformParameters.offset = 0;
	} else if (oldWaveform == WAVEFORM_DC) {
		int slotIndex;
		int subchannelIndex;
		int resourceIndex;
		FunctionGeneratorSelectChannelsPage::findResource(waveformParameters.absoluteResourceIndex,
			slotIndex, subchannelIndex, resourceIndex);

		float min;
		float max;
		g_slots[slotIndex]->getFunctionGeneratorAmplitudeInfo(subchannelIndex, resourceIndex, waveformParameters.resourceType, min, max);

		waveformParameters.amplitude = max - min;
		waveformParameters.offset = (min + max) / 2.0f;
	}

	waveformParameters.waveform = newWaveform;

	FunctionGeneratorPage::apply();
}

bool disabledCallback(uint16_t value) {
	if (FunctionGeneratorPage::g_waveformParameters[FunctionGeneratorPage::g_selectedItem].resourceType == FUNCTION_GENERATOR_RESOURCE_TYPE_DIGITAL) {
		return value == WAVEFORM_DC || value == WAVEFORM_SINE || value == WAVEFORM_SINE_HALF || value == WAVEFORM_SINE_RECTIFIED || value == WAVEFORM_TRIANGLE || value == WAVEFORM_SAWTOOTH || value == WAVEFORM_ARBITRARY;
	}
	return value == WAVEFORM_ARBITRARY;
}

void action_function_generator_select_waveform() {
	pushSelectFromEnumPage(g_waveformEnumDefinition,
		FunctionGeneratorPage::g_waveformParameters[FunctionGeneratorPage::g_selectedItem].waveform,
		disabledCallback, setWaveform, true);
}

void getDurationStepValues(StepValues *stepValues) {
	auto &waveformParameters = FunctionGeneratorPage::g_waveformParameters[FunctionGeneratorPage::g_selectedItem];

	int slotIndex;
	int subchannelIndex;
	int resourceIndex;
	FunctionGeneratorSelectChannelsPage::findResource(waveformParameters.absoluteResourceIndex,
		slotIndex, subchannelIndex, resourceIndex);
	float minFreq;
	float maxFreq;
	g_slots[slotIndex]->getFunctionGeneratorFrequencyInfo(subchannelIndex, resourceIndex, minFreq, maxFreq);

	static float g_values1[] = { 0.001f, 0.01f, 0.1f, 1.0f };
	static float g_values2[] = { 0.0001f, 0.001f, 0.01f, 0.1f };

	static float *values = maxFreq < 1000.0f ? g_values1 : g_values2;

	stepValues->values = values;
	stepValues->count = sizeof(g_values1) / sizeof(float);
	stepValues->unit = UNIT_SECOND;

	stepValues->encoderSettings.accelerationEnabled = true;

	stepValues->encoderSettings.range = g_values1[stepValues->count - 1];
	stepValues->encoderSettings.step = stepValues->values[0];
}

void data_function_generator_frequency(DataOperationEnum operation, Cursor cursor, Value &value) {
	auto &waveformParameters = FunctionGeneratorPage::g_waveformParameters[FunctionGeneratorPage::g_selectedItem];

	int slotIndex;
	int subchannelIndex;
	int resourceIndex;
	FunctionGeneratorSelectChannelsPage::findResource(waveformParameters.absoluteResourceIndex,
		slotIndex, subchannelIndex, resourceIndex);
	float minFreq;
	float maxFreq;
	g_slots[slotIndex]->getFunctionGeneratorFrequencyInfo(subchannelIndex, resourceIndex, minFreq, maxFreq);

	Unit unit = UNIT_HERTZ;

	float min;
	float max;
	if (g_options.isFreq) {
		min = minFreq;
		max = maxFreq;
	} else {
		unit = UNIT_SECOND;
		min = 1.0f / maxFreq;
		max = 1.0f / minFreq;
	}

	if (operation == DATA_OPERATION_GET) {
        bool focused = g_focusCursor == cursor && g_focusDataId == DATA_ID_FUNCTION_GENERATOR_FREQUENCY;
        if (focused && g_focusEditValue.getType() != VALUE_TYPE_NONE) {
            value = g_focusEditValue;
        } else if (focused && getActivePageId() == PAGE_ID_EDIT_MODE_KEYPAD && edit_mode_keypad::g_keypad->isEditing()) {
            data_keypad_text(operation, cursor, value);
        } else {
            value = MakeValue(g_options.isFreq ? waveformParameters.frequency : (1.0f / waveformParameters.frequency), unit);
        }
    } else if (operation == DATA_OPERATION_GET_ALLOW_ZERO) {
        value = 0;
    } else if (operation == DATA_OPERATION_GET_MIN) {
        value = MakeValue(min, unit);
    } else if (operation == DATA_OPERATION_GET_MAX) {
        value = MakeValue(max, unit);
    } else if (operation == DATA_OPERATION_GET_NAME) {
        value = g_options.isFreq ? "Frequency" : "Duration";
    } else if (operation == DATA_OPERATION_GET_UNIT) {
        value = unit;
    } else if (operation == DATA_OPERATION_GET_ENCODER_STEP_VALUES) {
		StepValues *stepValues = value.getStepValues();
		if (g_options.isFreq) {
			g_slots[slotIndex]->getFunctionGeneratorFrequencyInfo(subchannelIndex, resourceIndex, min, max, stepValues);

			stepValues->encoderSettings.accelerationEnabled = true;

			stepValues->encoderSettings.range = 10.0f;
			stepValues->encoderSettings.step = stepValues->values[0];
		} else {
			getDurationStepValues(stepValues);
		}
		stepValues->encoderSettings.mode = eez::psu::gui::edit_mode_step::g_functionGeneratorFrequencyEncoderMode;
		value = 1;
	} else if (operation == DATA_OPERATION_SET_ENCODER_MODE) {
		eez::psu::gui::edit_mode_step::g_functionGeneratorFrequencyEncoderMode = (EncoderMode)value.getInt();
    } else if (operation == DATA_OPERATION_SET) {
		if (g_options.isFreq) {
			waveformParameters.frequency = value.getFloat();
		} else {
			waveformParameters.frequency = 1.0f / value.getFloat();
		}
		FunctionGeneratorPage::apply();
    }
}

void data_function_generator_phase_shift(DataOperationEnum operation, Cursor cursor, Value &value) {
	auto &waveformParameters = FunctionGeneratorPage::g_waveformParameters[FunctionGeneratorPage::g_selectedItem];

	float max;
	Unit unit;

	if (g_options.isFreq) {
		max = 360.0f;
		unit = UNIT_DEGREE;
	} else {
		max = 1.0f / waveformParameters.frequency;
		unit = UNIT_SECOND;
	}

	if (operation == DATA_OPERATION_GET) {
		bool focused = g_focusCursor == cursor && g_focusDataId == DATA_ID_FUNCTION_GENERATOR_PHASE_SHIFT;
		if (focused && g_focusEditValue.getType() != VALUE_TYPE_NONE) {
			value = g_focusEditValue;
		} else if (focused && getActivePageId() == PAGE_ID_EDIT_MODE_KEYPAD && edit_mode_keypad::g_keypad->isEditing()) {
			data_keypad_text(operation, cursor, value);
		} else {
			value = MakeValue(g_options.isFreq ? waveformParameters.phaseShift : (value.float_ * max / 360.0f), unit);
		}
	} else if (operation == DATA_OPERATION_GET_ALLOW_ZERO) {
		value = 1;
	} else if (operation == DATA_OPERATION_GET_MIN) {
		value = MakeValue(0, unit);
	} else if (operation == DATA_OPERATION_GET_MAX) {
		value = MakeValue(max, unit);
	} else if (operation == DATA_OPERATION_GET_NAME) {
		value = "Phase shift";
	} else if (operation == DATA_OPERATION_GET_UNIT) {
		value = unit;
	} else if (operation == DATA_OPERATION_GET_ENCODER_STEP_VALUES) {
		StepValues *stepValues = value.getStepValues();

		if (g_options.isFreq) {
			static float g_values[] = { 1.0f, 5.0f, 10.0f, 20.0f };

			stepValues->values = g_values;
			stepValues->count = sizeof(g_values) / sizeof(float);
			stepValues->unit = unit;

			stepValues->encoderSettings.accelerationEnabled = true;

			stepValues->encoderSettings.range = 360.0f;
			stepValues->encoderSettings.step = 1.0f;
		} else {
			getDurationStepValues(stepValues);
		}

		stepValues->encoderSettings.mode = eez::psu::gui::edit_mode_step::g_functionGeneratorPhaseShiftEncoderMode;

		value = 1;
	} else if (operation == DATA_OPERATION_SET_ENCODER_MODE) {
		eez::psu::gui::edit_mode_step::g_functionGeneratorPhaseShiftEncoderMode = (EncoderMode)value.getInt();
	} else if (operation == DATA_OPERATION_SET) {
		if (g_options.isFreq) {
			waveformParameters.phaseShift = value.getFloat();
		} else {
			waveformParameters.phaseShift = 360.0f * value.getFloat() / max;
		}
		FunctionGeneratorPage::apply();
	}
}

void data_function_generator_amplitude(DataOperationEnum operation, Cursor cursor, Value &value) {
	auto &waveformParameters = FunctionGeneratorPage::g_waveformParameters[FunctionGeneratorPage::g_selectedItem];

	int slotIndex;
	int subchannelIndex;
	int resourceIndex;
	FunctionGeneratorSelectChannelsPage::findResource(waveformParameters.absoluteResourceIndex,
		slotIndex, subchannelIndex, resourceIndex);
	float min;
	float max;
	g_slots[slotIndex]->getFunctionGeneratorAmplitudeInfo(subchannelIndex, resourceIndex, waveformParameters.resourceType, min, max);

	Unit unit = waveformParameters.resourceType == FUNCTION_GENERATOR_RESOURCE_TYPE_U ? UNIT_VOLT : UNIT_AMPER;

	if (operation == DATA_OPERATION_GET) {
		bool focused = g_focusCursor == cursor && g_focusDataId == DATA_ID_FUNCTION_GENERATOR_AMPLITUDE;
		if (focused && g_focusEditValue.getType() != VALUE_TYPE_NONE) {
			value = g_focusEditValue;
		} else if (focused && getActivePageId() == PAGE_ID_EDIT_MODE_KEYPAD && edit_mode_keypad::g_keypad->isEditing()) {
			data_keypad_text(operation, cursor, value);
		} else {
			value = MakeValue(g_options.isAmpl ? waveformParameters.amplitude : (waveformParameters.offset - waveformParameters.amplitude / 2.0f), unit);
		}
	} else if (operation == DATA_OPERATION_GET_ALLOW_ZERO) {
		value = 0;
	} else if (operation == DATA_OPERATION_GET_MIN) {
		value = MakeValue(waveformParameters.waveform == WAVEFORM_DC ? min : (g_options.isAmpl ? 0 : min), unit);
	} else if (operation == DATA_OPERATION_GET_MAX) {
		value = MakeValue(waveformParameters.waveform == WAVEFORM_DC ? max : (g_options.isAmpl ? (max - min) : (waveformParameters.offset + waveformParameters.amplitude / 2.0f)), unit);
	} else if (operation == DATA_OPERATION_GET_NAME) {
		value = "Amplitude";
	} else if (operation == DATA_OPERATION_GET_UNIT) {
		value = unit;
	} else if (operation == DATA_OPERATION_GET_ENCODER_STEP_VALUES) {
		StepValues *stepValues = value.getStepValues();

		g_slots[slotIndex]->getFunctionGeneratorAmplitudeInfo(subchannelIndex, resourceIndex, waveformParameters.resourceType, min, max, stepValues);

		stepValues->encoderSettings.accelerationEnabled = true;

		stepValues->encoderSettings.range = max - min;
		stepValues->encoderSettings.step = stepValues->values[0];

		stepValues->encoderSettings.mode = eez::psu::gui::edit_mode_step::g_functionGeneratorAmplitudeEncoderMode;

		value = 1;
	} else if (operation == DATA_OPERATION_SET_ENCODER_MODE) {
		eez::psu::gui::edit_mode_step::g_functionGeneratorAmplitudeEncoderMode = (EncoderMode)value.getInt();
	} else if (operation == DATA_OPERATION_SET) {
		if (g_options.isAmpl) {
			waveformParameters.amplitude = value.getFloat();
		} else {
			float min = value.getFloat();
			float max = waveformParameters.offset + waveformParameters.amplitude / 2.0f;
			waveformParameters.amplitude = max - min;
			waveformParameters.offset = (min + max) / 2.0f;
		}
		FunctionGeneratorPage::apply();
	}
}

void data_function_generator_offset(DataOperationEnum operation, Cursor cursor, Value &value) {
	auto &waveformParameters = FunctionGeneratorPage::g_waveformParameters[FunctionGeneratorPage::g_selectedItem];

	int slotIndex;
	int subchannelIndex;
	int resourceIndex;
	FunctionGeneratorSelectChannelsPage::findResource(waveformParameters.absoluteResourceIndex,
		slotIndex, subchannelIndex, resourceIndex);
	float min;
	float max;
	g_slots[slotIndex]->getFunctionGeneratorAmplitudeInfo(subchannelIndex, resourceIndex, waveformParameters.resourceType, min, max);

	Unit unit = waveformParameters.resourceType == FUNCTION_GENERATOR_RESOURCE_TYPE_U ? UNIT_VOLT: UNIT_AMPER;

	if (operation == DATA_OPERATION_GET) {
		bool focused = g_focusCursor == cursor && g_focusDataId == DATA_ID_FUNCTION_GENERATOR_OFFSET;
		if (focused && g_focusEditValue.getType() != VALUE_TYPE_NONE) {
			value = g_focusEditValue;
		} else if (focused && getActivePageId() == PAGE_ID_EDIT_MODE_KEYPAD && edit_mode_keypad::g_keypad->isEditing()) {
			data_keypad_text(operation, cursor, value);
		} else {
			value = MakeValue(g_options.isAmpl ? waveformParameters.offset : (waveformParameters.offset + waveformParameters.amplitude / 2.0f), unit);
		}
	} else if (operation == DATA_OPERATION_GET_ALLOW_ZERO) {
		value = 0;
	} else if (operation == DATA_OPERATION_GET_MIN) {
		value = MakeValue(g_options.isAmpl ? min : (waveformParameters.offset - waveformParameters.amplitude / 2.0f), unit);
	} else if (operation == DATA_OPERATION_GET_MAX) {
		value = MakeValue(max, unit);
	} else if (operation == DATA_OPERATION_GET_NAME) {
		value = "Offset";
	} else if (operation == DATA_OPERATION_GET_UNIT) {
		value = unit;
	} else if (operation == DATA_OPERATION_GET_ENCODER_STEP_VALUES) {
		StepValues *stepValues = value.getStepValues();

		g_slots[slotIndex]->getFunctionGeneratorAmplitudeInfo(subchannelIndex, resourceIndex, waveformParameters.resourceType, min, max, stepValues);

		stepValues->encoderSettings.accelerationEnabled = true;

		stepValues->encoderSettings.range = max - min;
		stepValues->encoderSettings.step = stepValues->values[0];

		stepValues->encoderSettings.mode = eez::psu::gui::edit_mode_step::g_functionGeneratorOffsetEncoderMode;

		value = 1;
	} else if (operation == DATA_OPERATION_SET_ENCODER_MODE) {
		eez::psu::gui::edit_mode_step::g_functionGeneratorOffsetEncoderMode = (EncoderMode)value.getInt();
	} else if (operation == DATA_OPERATION_SET) {
		if (g_options.isAmpl) {
			waveformParameters.offset = value.getFloat();
		} else {
			float min = waveformParameters.offset - waveformParameters.amplitude / 2.0f;
			float max = value.getFloat();
			waveformParameters.amplitude = max - min;
			waveformParameters.offset = (min + max) / 2.0f;
		}
		FunctionGeneratorPage::apply();
	}
}

void data_function_generator_duty_cycle(DataOperationEnum operation, Cursor cursor, Value &value) {
	auto &waveformParameters = FunctionGeneratorPage::g_waveformParameters[FunctionGeneratorPage::g_selectedItem];

	float max;
	Unit unit;

	if (g_options.isFreq) {
		max = 100.0f;
		unit = UNIT_PERCENT;
	} else {
		max = 1.0f / waveformParameters.frequency;
		unit = UNIT_SECOND;
	}

	if (operation == DATA_OPERATION_GET) {
		bool focused = g_focusCursor == cursor && g_focusDataId == DATA_ID_DIB_MIO168_PWM_DUTY;
		if (focused && g_focusEditValue.getType() != VALUE_TYPE_NONE) {
			value = g_focusEditValue;
		} else if (focused && getActivePageId() == PAGE_ID_EDIT_MODE_KEYPAD && edit_mode_keypad::g_keypad->isEditing()) {
			data_keypad_text(operation, cursor, value);
		} else {
			value = MakeValue(g_options.isFreq ? waveformParameters.dutyCycle : (value.float_ * max / 100.0f), unit);
		}
	} else if (operation == DATA_OPERATION_GET_MIN) {
		value = MakeValue(0.0f, unit);
	} else if (operation == DATA_OPERATION_GET_MAX) {
		value = MakeValue(max, unit);
	} else if (operation == DATA_OPERATION_GET_NAME) {
		value = g_options.isFreq ? "Duty cycle" : "Pulse width";
	} else if (operation == DATA_OPERATION_GET_UNIT) {
		value = unit;
	} else if (operation == DATA_OPERATION_GET_ENCODER_STEP_VALUES) {
		StepValues *stepValues = value.getStepValues();

		if (g_options.isFreq) {
			static float values[] = { 0.1f, 0.5f, 1.0f, 5.0f };

			stepValues->values = values;
			stepValues->count = sizeof(values) / sizeof(float);
			stepValues->unit = unit;

			stepValues->encoderSettings.accelerationEnabled = false;
			stepValues->encoderSettings.range = 100.0f;
			stepValues->encoderSettings.step = 1.0f;
		} else {
			getDurationStepValues(stepValues);
		}

		stepValues->encoderSettings.mode = eez::psu::gui::edit_mode_step::g_functionGeneratorDutyCycleEncoderMode;

		value = 1;
	} else if (operation == DATA_OPERATION_SET_ENCODER_MODE) {
		eez::psu::gui::edit_mode_step::g_functionGeneratorDutyCycleEncoderMode = (EncoderMode)value.getInt();
	} else if (operation == DATA_OPERATION_SET) {
		if (g_options.isFreq) {
			waveformParameters.dutyCycle = value.getFloat();
		} else {
			waveformParameters.dutyCycle = 100.0f * value.getFloat() / max;
		}
		FunctionGeneratorPage::apply();
	}
}

void action_function_generator_select_channels() {
	pushPage(PAGE_ID_SYS_SETTINGS_FUNCTION_GENERATOR_SELECT_CHANNELS);
}

void data_function_generator_has_mode_select(DataOperationEnum operation, Cursor cursor, Value &value) {
	if (operation == DATA_OPERATION_GET) {
		int slotIndex;
		int subchannelIndex;
		int resourceIndex;
		FunctionGeneratorSelectChannelsPage::findResource(FunctionGeneratorPage::g_waveformParameters[FunctionGeneratorPage::g_selectedItem].absoluteResourceIndex,
			slotIndex, subchannelIndex, resourceIndex);
		value = g_slots[slotIndex]->getFunctionGeneratorResourceType(subchannelIndex, resourceIndex) == FUNCTION_GENERATOR_RESOURCE_TYPE_U_AND_I;
	}
}

void data_function_generator_mode(DataOperationEnum operation, Cursor cursor, Value &value) {
	if (operation == DATA_OPERATION_GET) {
		value = FunctionGeneratorPage::g_waveformParameters[FunctionGeneratorPage::g_selectedItem].resourceType == FUNCTION_GENERATOR_RESOURCE_TYPE_U ? "Voltage" : "Current";
	}
}

void action_function_generator_mode_select_mode() {
	auto &waveformParameters = FunctionGeneratorPage::g_waveformParameters[FunctionGeneratorPage::g_selectedItem];
	if (waveformParameters.resourceType == FUNCTION_GENERATOR_RESOURCE_TYPE_U) {
		waveformParameters.resourceType = FUNCTION_GENERATOR_RESOURCE_TYPE_I;
	} else {
		waveformParameters.resourceType = FUNCTION_GENERATOR_RESOURCE_TYPE_U;
	}

	int slotIndex;
	int subchannelIndex;
	int resourceIndex;
	FunctionGeneratorSelectChannelsPage::findResource(waveformParameters.absoluteResourceIndex, slotIndex, subchannelIndex, resourceIndex);

	float minFreq;
	float maxFreq;
	g_slots[slotIndex]->getFunctionGeneratorFrequencyInfo(subchannelIndex, resourceIndex, minFreq, maxFreq);

	float minAmp;
	float maxAmp;
	g_slots[slotIndex]->getFunctionGeneratorAmplitudeInfo(subchannelIndex, resourceIndex, waveformParameters.resourceType, minAmp, maxAmp);

	waveformParameters.frequency = MIN(100.0f, maxFreq);
	waveformParameters.amplitude = maxAmp - minAmp;
	waveformParameters.offset = (minAmp + maxAmp) / 2.0f;

	FunctionGeneratorPage::apply();
}

void data_function_generator_has_amplitude_and_offset(DataOperationEnum operation, Cursor cursor, Value &value) {
	if (operation == DATA_OPERATION_GET) {
		value = FunctionGeneratorPage::g_waveformParameters[FunctionGeneratorPage::g_selectedItem].resourceType != FUNCTION_GENERATOR_RESOURCE_TYPE_DIGITAL;
	}
}

void data_function_generator_has_duty_cycle(DataOperationEnum operation, Cursor cursor, Value &value) {
	if (operation == DATA_OPERATION_GET) {
		value = FunctionGeneratorPage::g_waveformParameters[FunctionGeneratorPage::g_selectedItem].waveform == WAVEFORM_PULSE;
	}
}

void data_function_generator_is_freq(DataOperationEnum operation, Cursor cursor, Value &value) {
	if (operation == DATA_OPERATION_GET) {
		value = g_options.isFreq ? 1 : 0;
	}
}

void action_function_generator_toggle_freq() {
	g_options.isFreq = !g_options.isFreq;
}

void data_function_generator_is_ampl(DataOperationEnum operation, Cursor cursor, Value &value) {
	if (operation == DATA_OPERATION_GET) {
		value = g_options.isAmpl ? 1 : 0;
	}
}

void action_function_generator_toggle_ampl() {
	g_options.isAmpl = !g_options.isAmpl;
}

void data_function_generator_is_dc(DataOperationEnum operation, Cursor cursor, Value &value) {
	if (operation == DATA_OPERATION_GET) {
		value = FunctionGeneratorPage::g_waveformParameters[FunctionGeneratorPage::g_selectedItem].waveform == WAVEFORM_DC;
	}
}


} // namespace gui
} // namespace eez
