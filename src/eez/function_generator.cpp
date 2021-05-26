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
	{ WAVEFORM_SINE_WAVE, "\xc3 Sine wave" },
	{ WAVEFORM_TRIANGLE, "\xc4 Triangle" },
	{ WAVEFORM_SQUARE_WAVE, "\xc5 Square wave" },
	{ WAVEFORM_PULSE, "\xc6 Pulse" },
	{ WAVEFORM_SAWTOOTH, "\xc7 Sawtooth" },
	{ WAVEFORM_ARBITRARY, "\xc8 Arbitrary" },
	{ 0, 0 }
};

const char *g_waveformShortLabel[8] = {
	"None",
	"\xc9",
	"\xc3",
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

WaveformFunction g_waveFormFuncU[CH_MAX];
float g_phiU[CH_MAX];
float g_dphiU[CH_MAX];
float g_amplitudeU[CH_MAX];
float g_offsetU[CH_MAX];
float g_pulseWidthsU[CH_MAX];

WaveformFunction g_waveFormFuncI[CH_MAX];
float g_phiI[CH_MAX];
float g_dphiI[CH_MAX];
float g_amplitudeI[CH_MAX];
float g_offsetI[CH_MAX];
float g_pulseWidthsI[CH_MAX];

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

    static void draw(const WidgetCursor &widgetCursor) {
		const Widget *widget = widgetCursor.widget;
		const Style* style = getStyle(widget->style);
		drawRectangle(widgetCursor.x, widgetCursor.y, (int)widget->w, (int)widget->h, style, false, false, true);

		float T = FLT_MIN;
		float min = FLT_MAX;
		float max = -FLT_MAX;

		for (int i = 0; i < MAX_NUM_WAVEFORMS; i++) {
			if (
				FunctionGeneratorPage::g_waveformParameters[i].absoluteResourceIndex != -1 &&
				FunctionGeneratorPage::g_triggerModes[i] == TRIGGER_MODE_FUNCTION_GENERATOR
			) {
				auto &waveformParameters = FunctionGeneratorPage::g_waveformParameters[i];
				float lower = waveformParameters.offset - waveformParameters.amplitude;
				float upper = waveformParameters.offset + waveformParameters.amplitude;
				if (lower < min) {
					min = lower;
				}
				if (upper > max) {
					max = upper;
				}
				float t = 2 * (1 / waveformParameters.frequency);
				if (t > T) {
					T = t;
				}
			}
		}

		float d = max - min;
		if (d == 0) {
			d = 1.0f;
		}
		min = min - d * 0.05f;
		max = max + d * 0.05f;
	
		int itemIndex = 0;
		int selectedItemIndex = -1;
		for (int i = 0; i < MAX_NUM_WAVEFORMS; i++) {
			if (
				FunctionGeneratorPage::g_waveformParameters[i].absoluteResourceIndex != -1 &&
				FunctionGeneratorPage::g_triggerModes[i] == TRIGGER_MODE_FUNCTION_GENERATOR
			) {
				if (itemIndex != g_selectedItem) {
					drawWaveform(widgetCursor, i, T, min, max);
				} else {
					selectedItemIndex = itemIndex;
				}
				itemIndex++;
			}
		}

		if (
			selectedItemIndex != -1 &&
			FunctionGeneratorPage::g_triggerModes[selectedItemIndex] == TRIGGER_MODE_FUNCTION_GENERATOR
		) {
			drawWaveform(widgetCursor, selectedItemIndex, T, min, max, true);
			drawWaveform(widgetCursor, selectedItemIndex, T, min, max, true, 1);
		}

		if (g_active) {
			reloadWaveformParameters();
		}
	}

	static void drawWaveform(const WidgetCursor &widgetCursor, int i, float T, float min, float max, bool selected = false, int yOffset = 0);

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
					waveformParameters[i].waveform = waveformParameters[i].resourceType == FUNCTION_GENERATOR_RESOURCE_TYPE_DIGITAL ? WAVEFORM_SQUARE_WAVE : WAVEFORM_SINE_WAVE;
					waveformParameters[i].frequency = MIN(100.0f, maxFreq);
					waveformParameters[i].phaseShift = 0;
					waveformParameters[i].amplitude = (maxAmp - minAmp) / 2;
					waveformParameters[i].offset = (minAmp + maxAmp) / 2;
					waveformParameters[i].pulseWidth = 25.0f;

					triggerModes[i] = TRIGGER_MODE_FUNCTION_GENERATOR;
				} else {
					if (waveformParameters[i].frequency < minFreq) {
						waveformParameters[i].frequency = minFreq;
					} else if (waveformParameters[i].frequency > maxFreq) {
						waveformParameters[i].frequency = maxFreq;
					}

					if (
						waveformParameters[i].offset + waveformParameters[i].amplitude > maxAmp ||
						waveformParameters[i].offset - waveformParameters[i].amplitude < minAmp
						) {
						waveformParameters[i].amplitude = (maxAmp - minAmp) / 2;
						waveformParameters[i].offset = (minAmp + maxAmp) / 2;
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
	return 1.0f;
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

static float g_pulseWidth;

float pulsef(float t) {
	if (t < g_pulseWidth * 2.0f * M_PI_F / 100.0f) {
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
	} else if (waveformParameters.waveform == WAVEFORM_SINE_WAVE) {
		return sinf;
	} else if (waveformParameters.waveform == WAVEFORM_TRIANGLE) {
		return trianglef;
	} else if (waveformParameters.waveform == WAVEFORM_SQUARE_WAVE) {
		return squaref;
	} else if (waveformParameters.waveform == WAVEFORM_PULSE) {
		g_pulseWidth = waveformParameters.pulseWidth;
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

		g_triggerModes[i] = g_slots[slotIndex]->getFunctionGeneratorResourceTriggerMode(subchannelIndex, resourceIndex);
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
		g_slots[slotIndex]->setFunctionGeneratorResourceTriggerMode(subchannelIndex, resourceIndex, TRIGGER_MODE_FIXED);
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

		g_slots[slotIndex]->setFunctionGeneratorResourceTriggerMode(subchannelIndex, resourceIndex, g_triggerModes[i]);
	}
}

void FunctionGeneratorPage::drawWaveform(const WidgetCursor &widgetCursor, int i, float T, float min, float max, bool selected, int yOffset) {
	const Widget *widget = widgetCursor.widget;
	const Style* style = getStyle(widget->style);
	font::Font font = styleGetFont(style);
	int textHeight = font.getHeight();

	auto &waveformParameters = FunctionGeneratorPage::g_waveformParameters[i];

	float frequency = waveformParameters.frequency;
	float phaseShift = waveformParameters.phaseShift;
	float amplitude = waveformParameters.amplitude;
	float offset = waveformParameters.offset;

	int slotIndex;
	int subchannelIndex;
	int resourceIndex;
	FunctionGeneratorSelectChannelsPage::findResource(waveformParameters.absoluteResourceIndex, slotIndex, subchannelIndex, resourceIndex);

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

		float yt = offset + amplitude * func(fi);

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
			profileWaveformParameters.pulseWidth = g_waveformParameters[i].pulseWidth;
		} else {
			profileParams.functionGeneratorParameters.waveformParameters[i].moduleType = MODULE_TYPE_NONE;
		}
	}
}

void setProfileParameters(const psu::profile::Parameters &profileParams) {
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
			g_waveformParameters[i].pulseWidth = profileWaveformParameters.pulseWidth;
		}
	}

	for (; i < MAX_NUM_WAVEFORMS; i++) {
		g_waveformParameters[i].absoluteResourceIndex = -1;
	}
}

bool writeProfileProperties(psu::profile::WriteContext &ctx, const psu::profile::Parameters &profileParams) {
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
		WRITE_PROPERTY("pulseWidth", profileWaveformParameters.pulseWidth);
	}

	return true;
}

bool readProfileProperties(psu::profile::ReadContext &ctx, psu::profile::Parameters &profileParams) {
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
		READ_PROPERTY("pulseWidth", profileWaveformParameters.pulseWidth);
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////

void addChannelWaveformParameters(int slotIndex, int subchannelIndex, int resourceIndex) {
	g_functionGeneratorPage.pageAlloc();

	g_functionGeneratorSelectChannelsPage.pageAlloc();

	for (int i = 0; i < FunctionGeneratorSelectChannelsPage::getNumFunctionGeneratorResources(); i++) {
		int tmpSlotIndex;
		int tmpSubchannelIndex;
		int tmpResourceIndex;
		FunctionGeneratorSelectChannelsPage::findResource(i, tmpSlotIndex, tmpSubchannelIndex, tmpResourceIndex);
		
		if (tmpSlotIndex == slotIndex && tmpSubchannelIndex == subchannelIndex && (resourceIndex == -1 || tmpResourceIndex == resourceIndex)) {
			g_functionGeneratorSelectChannelsPage.g_selectedChannels |= (uint64_t)1 << i;
		}
	}

	g_functionGeneratorSelectChannelsPage.apply();

	g_functionGeneratorPage.apply();
}

void addChannelWaveformParameters(Channel &channel) {
	addChannelWaveformParameters(channel.slotIndex, channel.subchannelIndex, -1);
}

void removeAllChannels() {
	g_functionGeneratorPage.pageAlloc();

	g_functionGeneratorSelectChannelsPage.pageAlloc();

	g_functionGeneratorSelectChannelsPage.g_selectedChannels = 0;

	g_functionGeneratorSelectChannelsPage.apply();

	g_functionGeneratorPage.apply();
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

	g_functionGeneratorPage.apply();
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

	g_functionGeneratorPage.apply();
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

int getNumChannelsInFunctionGeneratorTriggerMode() {
	int count = 0;
	for (int absoluteResourceIndex = 0; absoluteResourceIndex < FunctionGeneratorSelectChannelsPage::getNumFunctionGeneratorResources(); absoluteResourceIndex++) {
		int slotIndex;
		int subchannelIndex;
		int resourceIndex;
		FunctionGeneratorSelectChannelsPage::findResource(
			absoluteResourceIndex,
			slotIndex, subchannelIndex, resourceIndex);
		if (g_slots[slotIndex]->getFunctionGeneratorResourceTriggerMode(subchannelIndex, resourceIndex) == TRIGGER_MODE_FUNCTION_GENERATOR) {
			count++;
		}
	}
	return count;
}

void selectWaveformParametersForChannel(Channel &channel) {
	selectWaveformParametersForChannel(channel.slotIndex, channel.subchannelIndex, -1);
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

		if (g_slots[slotIndex]->getFunctionGeneratorResourceTriggerMode(subchannelIndex, resourceIndex) == TRIGGER_MODE_FUNCTION_GENERATOR) {
			g_active = true;
			g_functionGeneratorPage.pageAlloc();

			reloadWaveformParameters();

			return;
		}
	}
}

void reloadWaveformParameters() {
	bool trackingChannelsStarted = false;

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

			g_waveFormFuncU[channel->channelIndex] = dcf;
			g_phiU[channel->channelIndex] = 0;
			g_dphiU[channel->channelIndex] = 0;
			g_amplitudeU[channel->channelIndex] = 0;
			g_offsetU[channel->channelIndex] = 0;

			g_waveFormFuncI[channel->channelIndex] = dcf;
			g_phiI[channel->channelIndex] = 0;
			g_dphiI[channel->channelIndex] = 0;
			g_amplitudeI[channel->channelIndex] = 0;
			g_offsetI[channel->channelIndex] = 0;

			auto &waveformParameters = g_waveformParameters[i];

#if defined(EEZ_PLATFORM_STM32)
			__disable_irq();
#endif
			if (waveformParameters.resourceType == FUNCTION_GENERATOR_RESOURCE_TYPE_U) {
				g_waveFormFuncU[channel->channelIndex] = getWaveformFunction(waveformParameters);
				g_pulseWidthsU[i] = g_pulseWidth;
				g_phiU[channel->channelIndex] = waveformParameters.phaseShift / 360.0f;
				g_dphiU[channel->channelIndex] = 2.0f * M_PI_F * waveformParameters.frequency * PERIOD;
				g_amplitudeU[channel->channelIndex] = waveformParameters.amplitude;
				g_offsetU[channel->channelIndex] = waveformParameters.offset;
			} else {
				g_waveFormFuncI[channel->channelIndex] = getWaveformFunction(waveformParameters);
				g_pulseWidthsI[i] = g_pulseWidth;
				g_phiI[channel->channelIndex] = waveformParameters.phaseShift / 360.0f;
				g_dphiI[channel->channelIndex] = 2.0f * M_PI_F * waveformParameters.frequency * PERIOD;
				g_amplitudeI[channel->channelIndex] = waveformParameters.amplitude;
				g_offsetI[channel->channelIndex] = waveformParameters.offset;

				if (g_slots[slotIndex]->moduleType == MODULE_TYPE_DCP405) {
					float max = waveformParameters.offset + waveformParameters.amplitude;
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
			g_pulseWidth = g_pulseWidthsU[i];
			float value = g_offsetU[i] + g_amplitudeU[i] * g_waveFormFuncU[i](g_phiU[i]);

			g_phiU[i] += g_dphiU[i];
			if (g_phiU[i] >= 2.0f * M_PI_F) {
				g_phiU[i] = 0;
			}

			channel_dispatcher::setVoltage(channel, value);
		}

		if (channel.flags.currentTriggerMode == TRIGGER_MODE_FUNCTION_GENERATOR) {
			g_pulseWidth = g_pulseWidthsI[i];
			float value = g_offsetI[i] + g_amplitudeI[i] * g_waveFormFuncI[i](g_phiU[i]);

			g_phiI[i] += g_dphiI[i];
			if (g_phiI[i] >= 2.0f * M_PI_F) {
				g_phiI[i] = 0;
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
	FunctionGeneratorPage::g_version++;
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
		FunctionGeneratorPage::g_version++;
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
	FunctionGeneratorPage::g_waveformParameters[FunctionGeneratorPage::g_selectedItem].waveform = (Waveform)value;
	FunctionGeneratorPage::apply();
}

bool disabledCallback(uint16_t value) {
	if (FunctionGeneratorPage::g_waveformParameters[FunctionGeneratorPage::g_selectedItem].resourceType == FUNCTION_GENERATOR_RESOURCE_TYPE_DIGITAL) {
		return value == WAVEFORM_DC || value == WAVEFORM_SINE_WAVE || value == WAVEFORM_TRIANGLE || value == WAVEFORM_SAWTOOTH || value == WAVEFORM_ARBITRARY;
	}
	return value == WAVEFORM_ARBITRARY;
}

void action_function_generator_select_waveform() {
	pushSelectFromEnumPage(g_waveformEnumDefinition,
		FunctionGeneratorPage::g_waveformParameters[FunctionGeneratorPage::g_selectedItem].waveform,
		disabledCallback, setWaveform, true);
}

void data_function_generator_frequency(DataOperationEnum operation, Cursor cursor, Value &value) {
	auto &waveformParameters = FunctionGeneratorPage::g_waveformParameters[FunctionGeneratorPage::g_selectedItem];

	int slotIndex;
	int subchannelIndex;
	int resourceIndex;
	FunctionGeneratorSelectChannelsPage::findResource(waveformParameters.absoluteResourceIndex,
		slotIndex, subchannelIndex, resourceIndex);
	float min;
	float max;
	g_slots[slotIndex]->getFunctionGeneratorFrequencyInfo(subchannelIndex, resourceIndex, min, max);

	if (operation == DATA_OPERATION_GET) {
        bool focused = g_focusCursor == cursor && g_focusDataId == DATA_ID_FUNCTION_GENERATOR_FREQUENCY;
        if (focused && g_focusEditValue.getType() != VALUE_TYPE_NONE) {
            value = g_focusEditValue;
        } else if (focused && getActivePageId() == PAGE_ID_EDIT_MODE_KEYPAD && edit_mode_keypad::g_keypad->isEditing()) {
            data_keypad_text(operation, cursor, value);
        } else {
            value = MakeValue(waveformParameters.frequency, UNIT_HERTZ);
        }
    } else if (operation == DATA_OPERATION_GET_ALLOW_ZERO) {
        value = 0;
    } else if (operation == DATA_OPERATION_GET_MIN) {
        value = MakeValue(min, UNIT_HERTZ);
    } else if (operation == DATA_OPERATION_GET_MAX) {
        value = MakeValue(max, UNIT_HERTZ);
    } else if (operation == DATA_OPERATION_GET_NAME) {
        value = "Frequency";
    } else if (operation == DATA_OPERATION_GET_UNIT) {
        value = UNIT_HERTZ;
    } else if (operation == DATA_OPERATION_GET_ENCODER_STEP_VALUES) {
        StepValues *stepValues = value.getStepValues();
		g_slots[slotIndex]->getFunctionGeneratorFrequencyInfo(subchannelIndex, resourceIndex, min, max, stepValues);

        stepValues->encoderSettings.accelerationEnabled = true;

        stepValues->encoderSettings.range = 10.0f;
        stepValues->encoderSettings.step = stepValues->values[0];

        stepValues->encoderSettings.mode = eez::psu::gui::edit_mode_step::g_functionGeneratorFrequencyEncoderMode;

        value = 1;
    } else if (operation == DATA_OPERATION_SET_ENCODER_MODE) {
        eez::psu::gui::edit_mode_step::g_functionGeneratorFrequencyEncoderMode = (EncoderMode)value.getInt();
    } else if (operation == DATA_OPERATION_SET) {
        waveformParameters.frequency = value.getFloat();
		FunctionGeneratorPage::g_version++;
		FunctionGeneratorPage::apply();
    }
}

void data_function_generator_phase_shift(DataOperationEnum operation, Cursor cursor, Value &value) {
	auto &waveformParameters = FunctionGeneratorPage::g_waveformParameters[FunctionGeneratorPage::g_selectedItem];

	if (operation == DATA_OPERATION_GET) {
		bool focused = g_focusCursor == cursor && g_focusDataId == DATA_ID_FUNCTION_GENERATOR_PHASE_SHIFT;
		if (focused && g_focusEditValue.getType() != VALUE_TYPE_NONE) {
			value = g_focusEditValue;
		} else if (focused && getActivePageId() == PAGE_ID_EDIT_MODE_KEYPAD && edit_mode_keypad::g_keypad->isEditing()) {
			data_keypad_text(operation, cursor, value);
		} else {
			value = MakeValue(waveformParameters.phaseShift, UNIT_DEGREE);
		}
	} else if (operation == DATA_OPERATION_GET_ALLOW_ZERO) {
		value = 1;
	} else if (operation == DATA_OPERATION_GET_MIN) {
		value = MakeValue(0, UNIT_DEGREE);
	} else if (operation == DATA_OPERATION_GET_MAX) {
		value = MakeValue(360.0f, UNIT_DEGREE);
	} else if (operation == DATA_OPERATION_GET_NAME) {
		value = "Phase shift";
	} else if (operation == DATA_OPERATION_GET_UNIT) {
		value = UNIT_DEGREE;
	} else if (operation == DATA_OPERATION_GET_ENCODER_STEP_VALUES) {
		static float values[] = { 1.0f, 5.0f, 10.0f, 20.0f };

		StepValues *stepValues = value.getStepValues();

		stepValues->values = values;
		stepValues->count = sizeof(values) / sizeof(float);
		stepValues->unit = UNIT_DEGREE;

		stepValues->encoderSettings.accelerationEnabled = true;

		stepValues->encoderSettings.range = 360.0f;
		stepValues->encoderSettings.step = 1.0f;

		stepValues->encoderSettings.mode = eez::psu::gui::edit_mode_step::g_functionGeneratorPhaseShiftEncoderMode;

		value = 1;
	} else if (operation == DATA_OPERATION_SET_ENCODER_MODE) {
		eez::psu::gui::edit_mode_step::g_functionGeneratorPhaseShiftEncoderMode = (EncoderMode)value.getInt();
	} else if (operation == DATA_OPERATION_SET) {
		waveformParameters.phaseShift = value.getFloat();
		FunctionGeneratorPage::g_version++;
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

	float offset = waveformParameters.offset;
	float maxAmplitude = MIN(max - offset, offset - min);

	Unit unit = waveformParameters.resourceType == FUNCTION_GENERATOR_RESOURCE_TYPE_U ? UNIT_VOLT : UNIT_AMPER;

	if (operation == DATA_OPERATION_GET) {
		bool focused = g_focusCursor == cursor && g_focusDataId == DATA_ID_FUNCTION_GENERATOR_AMPLITUDE;
		if (focused && g_focusEditValue.getType() != VALUE_TYPE_NONE) {
			value = g_focusEditValue;
		} else if (focused && getActivePageId() == PAGE_ID_EDIT_MODE_KEYPAD && edit_mode_keypad::g_keypad->isEditing()) {
			data_keypad_text(operation, cursor, value);
		} else {
			value = MakeValue(waveformParameters.amplitude, unit);
		}
	} else if (operation == DATA_OPERATION_GET_ALLOW_ZERO) {
		value = 1;
	} else if (operation == DATA_OPERATION_GET_MIN) {
		value = MakeValue(0, unit);
	} else if (operation == DATA_OPERATION_GET_MAX) {
		value = MakeValue(maxAmplitude, unit);
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
		waveformParameters.amplitude = value.getFloat();
		FunctionGeneratorPage::g_version++;
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

	float amplitude = waveformParameters.amplitude;

	if (operation == DATA_OPERATION_GET) {
		bool focused = g_focusCursor == cursor && g_focusDataId == DATA_ID_FUNCTION_GENERATOR_OFFSET;
		if (focused && g_focusEditValue.getType() != VALUE_TYPE_NONE) {
			value = g_focusEditValue;
		} else if (focused && getActivePageId() == PAGE_ID_EDIT_MODE_KEYPAD && edit_mode_keypad::g_keypad->isEditing()) {
			data_keypad_text(operation, cursor, value);
		} else {
			value = MakeValue(waveformParameters.offset, unit);
		}
	} else if (operation == DATA_OPERATION_GET_ALLOW_ZERO) {
		value = 0;
	} else if (operation == DATA_OPERATION_GET_MIN) {
		value = MakeValue(min + amplitude, unit);
	} else if (operation == DATA_OPERATION_GET_MAX) {
		value = MakeValue(max - amplitude, unit);
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
		waveformParameters.offset = value.getFloat();
		FunctionGeneratorPage::g_version++;
		FunctionGeneratorPage::apply();
	}
}

void data_function_generator_pulse_width(DataOperationEnum operation, Cursor cursor, Value &value) {
	auto &waveformParameters = FunctionGeneratorPage::g_waveformParameters[FunctionGeneratorPage::g_selectedItem];

	if (operation == DATA_OPERATION_GET) {
		bool focused = g_focusCursor == cursor && g_focusDataId == DATA_ID_DIB_MIO168_PWM_DUTY;
		if (focused && g_focusEditValue.getType() != VALUE_TYPE_NONE) {
			value = g_focusEditValue;
		} else if (focused && getActivePageId() == PAGE_ID_EDIT_MODE_KEYPAD && edit_mode_keypad::g_keypad->isEditing()) {
			data_keypad_text(operation, cursor, value);
		} else {
			value = MakeValue(waveformParameters.pulseWidth, UNIT_PERCENT);
		}
	} else if (operation == DATA_OPERATION_GET_MIN) {
		value = MakeValue(0.0f, UNIT_PERCENT);
	} else if (operation == DATA_OPERATION_GET_MAX) {
		value = MakeValue(100.0f, UNIT_PERCENT);
	} else if (operation == DATA_OPERATION_GET_NAME) {
		value = "Pulse width";
	} else if (operation == DATA_OPERATION_GET_UNIT) {
		value = UNIT_PERCENT;
	} else if (operation == DATA_OPERATION_GET_ENCODER_STEP_VALUES) {
		static float values[] = { 0.1f, 0.5f, 1.0f, 5.0f };

		StepValues *stepValues = value.getStepValues();

		stepValues->values = values;
		stepValues->count = sizeof(values) / sizeof(float);
		stepValues->unit = UNIT_PERCENT;

		stepValues->encoderSettings.accelerationEnabled = false;
		stepValues->encoderSettings.range = 100.0f;
		stepValues->encoderSettings.step = 1.0f;

		stepValues->encoderSettings.mode = eez::psu::gui::edit_mode_step::g_functionGeneratorPulseWidthEncoderMode;

		value = 1;
	} else if (operation == DATA_OPERATION_SET_ENCODER_MODE) {
		eez::psu::gui::edit_mode_step::g_functionGeneratorPulseWidthEncoderMode = (EncoderMode)value.getInt();
	} else if (operation == DATA_OPERATION_SET) {
		waveformParameters.pulseWidth = value.getFloat();
		FunctionGeneratorPage::g_version++;
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
	waveformParameters.amplitude = (maxAmp - minAmp) / 2;
	waveformParameters.offset = (minAmp + maxAmp) / 2;

	FunctionGeneratorPage::apply();
}

void data_function_generator_has_amplitude_and_offset(DataOperationEnum operation, Cursor cursor, Value &value) {
	if (operation == DATA_OPERATION_GET) {
		value = FunctionGeneratorPage::g_waveformParameters[FunctionGeneratorPage::g_selectedItem].resourceType != FUNCTION_GENERATOR_RESOURCE_TYPE_DIGITAL;
	}
}

void data_function_generator_has_pulse_width(DataOperationEnum operation, Cursor cursor, Value &value) {
	if (operation == DATA_OPERATION_GET) {
		value = FunctionGeneratorPage::g_waveformParameters[FunctionGeneratorPage::g_selectedItem].waveform == WAVEFORM_PULSE;
	}
}

} // namespace gui
} // namespace eez
