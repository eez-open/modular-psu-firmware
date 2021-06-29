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
#include <eez/modules/psu/trigger.h>
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

////////////////////////////////////////////////////////////////////////////////

static EnumItem g_waveformEnumDefinition[] = {
	{ WAVEFORM_DC, "\xc9 DC" },
	{ WAVEFORM_SINE, "\xc3 Sine" },
	{ WAVEFORM_HALF_RECTIFIED, "\xca Half rectified" },
	{ WAVEFORM_FULL_RECTIFIED, "\xcb Full rectified" },
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

////////////////////////////////////////////////////////////////////////////////

struct AllResources {
	static void reset() {
		g_numResources = -1;
	}

	static int getNumResources() {
		if (g_numResources == -1) {
			g_numResources = countResources();
		}
		return g_numResources;
	}

	static bool findResource(int absoluteResourceIndex, int &slotIndex, int &subchannelIndex, int &resourceIndex)  {
		if (absoluteResourceIndex < getNumResources()) {
			slotIndex = g_resources[absoluteResourceIndex].slotIndex;
			subchannelIndex = g_resources[absoluteResourceIndex].subchannelIndex;
			resourceIndex = g_resources[absoluteResourceIndex].resourceIndex;
			return true;
		}
		slotIndex = -1;
		subchannelIndex = -1;
		resourceIndex = -1;
		return false;
	}

	static int getAbsoluteResourceIndex(int slotIndex, int subchannelIndex, int resourceIndex) {
		for (int i = 0; i < getNumResources(); i++) {
			if (g_resources[i].slotIndex == slotIndex && g_resources[i].subchannelIndex == subchannelIndex && g_resources[i].resourceIndex == resourceIndex) {
				return i;
			}
		}

		return -1;
	}

private:
	struct ResourceAddress {
		int slotIndex;
		int subchannelIndex;
		int resourceIndex;
	};

	static int g_numResources;
	static ResourceAddress g_resources[64];

	static int countResources()  {
		int i = 0;

		for (int slotIndex = 0; slotIndex < NUM_SLOTS; slotIndex++) {
			int numSubchannels = g_slots[slotIndex]->getNumSubchannels();
			for (int subchannelIndex = 0; subchannelIndex < numSubchannels; subchannelIndex++) {
				int numResources = g_slots[slotIndex]->getNumFunctionGeneratorResources(subchannelIndex);
				for (int resourceIndex = 0; resourceIndex < numResources; resourceIndex++) {
					g_resources[i].slotIndex = slotIndex;
					g_resources[i].subchannelIndex = subchannelIndex;
					g_resources[i].resourceIndex = resourceIndex;
					if (++i == 64) {
						return i;
					}
				}
			}
		}

		return i;
	}
};

int AllResources::g_numResources = -1;
AllResources::ResourceAddress AllResources::g_resources[64];

////////////////////////////////////////////////////////////////////////////////

struct SelectedResources {
	WaveformParameters m_waveformParameters[MAX_NUM_WAVEFORMS];
	int m_numResources = 0;

	int findResource(int absoluteResourceIndex) {
		for (int i = 0; i < m_numResources; i++) {
			if (m_waveformParameters[i].absoluteResourceIndex == absoluteResourceIndex) {
				return i;
			}
		}
		return -1;
	}
};

SelectedResources g_selectedResources;

////////////////////////////////////////////////////////////////////////////////

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
float g_freqU[CH_MAX];

WaveformFunction g_waveFormFuncI[CH_MAX];
float g_phiI[CH_MAX];
float g_dphiI[CH_MAX];
float g_amplitudeI[CH_MAX];
float g_offsetI[CH_MAX];
float g_dutyCycleI[CH_MAX];

bool g_dprogStateModified[CH_MAX];

static const float PERIOD = 0.0002f;

static void reloadWaveformParameters();

////////////////////////////////////////////////////////////////////////////////

float dcf(float t) {
	return 0.0f;
}

float sineHalfRectifiedf(float t) {
	if (t < M_PI_F) {
		return 2.0f * sinf(t);
	}

	return 0.0f;
}

float sineFullRectifiedf(float t) {
	return 2.0f * sinf(t / 2);
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
	} else if (waveformParameters.waveform == WAVEFORM_HALF_RECTIFIED) {
		return sineHalfRectifiedf;
	} else if (waveformParameters.waveform == WAVEFORM_FULL_RECTIFIED) {
		return sineFullRectifiedf;
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

class FunctionGeneratorPage : public SetPage {
public:
    static const int PAGE_SIZE = 4;

	bool m_initialized = false;

	void init() {
		memcpy(&m_selectedResources, &g_selectedResources, sizeof(g_selectedResources));

		m_initialized = true;
	}

	int m_savedSlotIndex;
	int m_savedSubchannelIndex;
	Channel *m_savedChannel;

	void pageAlloc() {
		m_savedSlotIndex = hmi::g_selectedSlotIndex;
		m_savedSubchannelIndex = hmi::g_selectedSubchannelIndex;
		m_savedChannel = g_channel;

		hmi::selectSlot(-1);
		selectChannel(nullptr);

		init();
	}

	void restoreSlotIndex() {
		hmi::selectSlot(m_savedSlotIndex);
		hmi::g_selectedSubchannelIndex = m_savedSubchannelIndex;
		selectChannel(m_savedChannel);
	}

    int getDirty() {
		return memcmp(&m_selectedResources, &g_selectedResources, sizeof(g_selectedResources)) != 0;
	}
    
	void apply() {
		bool triggerAbortCalled = false;

		for (int i = 0; i < g_selectedResources.m_numResources;) {
			int j = m_selectedResources.findResource(g_selectedResources.m_waveformParameters[i].absoluteResourceIndex);
			if (j == -1) {
				int slotIndex;
				int subchannelIndex;
				int resourceIndex;
				AllResources::findResource(g_selectedResources.m_waveformParameters[i].absoluteResourceIndex, slotIndex, subchannelIndex, resourceIndex);

				if (!triggerAbortCalled) {
					triggerAbortCalled = true;
					trigger::abort();
				}

				g_slots[slotIndex]->setFunctionGeneratorResourceTriggerMode(subchannelIndex, resourceIndex, TRIGGER_MODE_FIXED, nullptr);
			} else {
				i++;
			}
		}

		memcpy(&g_selectedResources, &m_selectedResources, sizeof(g_selectedResources));

		for (int i = 0; i < g_selectedResources.m_numResources; i++) {
			int slotIndex;
			int subchannelIndex;
			int resourceIndex;
			AllResources::findResource(g_selectedResources.m_waveformParameters[i].absoluteResourceIndex, slotIndex, subchannelIndex, resourceIndex);

			TriggerMode triggerMode;
			g_slots[slotIndex]->getFunctionGeneratorResourceTriggerMode(subchannelIndex, resourceIndex, triggerMode, nullptr);
			if (triggerMode != TRIGGER_MODE_FUNCTION_GENERATOR) {
				if (!triggerAbortCalled) {
					triggerAbortCalled = true;
					trigger::abort();
				}

				g_slots[slotIndex]->setFunctionGeneratorResourceTriggerMode(subchannelIndex, resourceIndex, TRIGGER_MODE_FUNCTION_GENERATOR, nullptr);
			}
		}

		if (g_active) {
			reloadWaveformParameters();
		}

		m_version++;
	}

	void set() {
		apply();
		popPage();
	}

	static void drawStatic(const WidgetCursor &widgetCursor);

	void draw(const WidgetCursor &widgetCursor) {
		const Widget *widget = widgetCursor.widget;
		const Style* style = getStyle(widget->style);
		drawRectangle(widgetCursor.x, widgetCursor.y, (int)widget->w, (int)widget->h, style, false, false, true);

		float T = FLT_MIN;

		float minU = FLT_MAX;
		float maxU = -FLT_MAX;

		float minI = FLT_MAX;
		float maxI = -FLT_MAX;

		int numDigital = 0;

		for (int i = 0; i < m_selectedResources.m_numResources; i++) {
			auto &waveformParameters = m_selectedResources.m_waveformParameters[i];

			if (waveformParameters.resourceType == FUNCTION_GENERATOR_RESOURCE_TYPE_DIGITAL) {
				++numDigital;
			} else {
				int slotIndex;
				int subchannelIndex;
				int resourceIndex;
				AllResources::findResource(waveformParameters.absoluteResourceIndex, slotIndex, subchannelIndex, resourceIndex);

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

			if (waveformParameters.waveform != WAVEFORM_DC) {
				float t = 2 * (1 / waveformParameters.frequency);
				if (t > T) {
					T = t;
				}
			}
		}

		if (T == FLT_MIN) {
			T = 1.0f;
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

		int digitalIndex = 0;
		int selectedItemDigitalIndex = 0;

		for (int i = 0; i < m_selectedResources.m_numResources; i++) {
			auto &waveformParameters = m_selectedResources.m_waveformParameters[i];

			if (i == m_selectedItem) {
				selectedItemDigitalIndex = digitalIndex;
				if (waveformParameters.resourceType == FUNCTION_GENERATOR_RESOURCE_TYPE_DIGITAL) {
					digitalIndex++;
				}
				continue;
			}

			if (waveformParameters.resourceType == FUNCTION_GENERATOR_RESOURCE_TYPE_DIGITAL) {
				auto digitalWaveformParameters = waveformParameters;

				digitalWaveformParameters.offset = ((numDigital - digitalIndex - 1) + 0.5f) / numDigital;
				digitalWaveformParameters.amplitude = 1.0f / numDigital - 4.0f / 118.0f;

				drawWaveform(widgetCursor, digitalWaveformParameters, T, 0, 1.0f);

				digitalIndex++;
			} else {
				float min = waveformParameters.resourceType == FUNCTION_GENERATOR_RESOURCE_TYPE_U ? minU : minI;
				float max = waveformParameters.resourceType == FUNCTION_GENERATOR_RESOURCE_TYPE_U ? maxU : maxI;

				drawWaveform(widgetCursor, waveformParameters, T, min, max);
			}
		}

		auto &waveformParameters = m_selectedResources.m_waveformParameters[m_selectedItem];

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

	static void drawWaveform(const WidgetCursor &widgetCursor, WaveformParameters &waveformParameters, float T, float min, float max, bool selected = false, int yOffset = 0) {
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
		AllResources::findResource(waveformParameters.absoluteResourceIndex, slotIndex, subchannelIndex, resourceIndex);

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

		float range = max - min;

		int xLeft = widgetCursor.x;
		int yBottom = widgetCursor.y + widget->h - 1;

		int yPrev = 0;
		int xPrev = 0;

		for (int xOffset = 0; xOffset < widget->w; xOffset++) {
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
			int yNext = yBottom - roundf((yt - min) / range * widget->h) - yOffset;

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

	int getScrollPosition() {
		return m_scrollPosition;
	}

	void setScrollPosition(int scrollPosition) {
		if (g_selectedResources.m_numResources <= PAGE_SIZE) {
			return;
		}

		if (scrollPosition < 0) {
			scrollPosition = 0;
		} else if (scrollPosition > g_selectedResources.m_numResources - PAGE_SIZE) {
			scrollPosition = g_selectedResources.m_numResources - PAGE_SIZE;
		}
		
		if (scrollPosition != m_scrollPosition) {
			m_scrollPosition = scrollPosition;
			refreshScreen();
		}
	}

	int getRefreshState() {
		return m_version;
	}

	SelectedResources m_selectedResources;
	int m_selectedItem;
	uint32_t m_version;

private:
	int m_scrollPosition;
};

FunctionGeneratorPage g_functionGeneratorPage;
eez::gui::SetPage *g_pFunctionGeneratorPage = &g_functionGeneratorPage;

void FunctionGeneratorPage::drawStatic(const WidgetCursor &widgetCursor) {
	g_functionGeneratorPage.draw(widgetCursor);
}

////////////////////////////////////////////////////////////////////////////////

void initWaveformParameters(WaveformParameters &waveformParameters) {
	int slotIndex;
	int subchannelIndex;
	int resourceIndex;
	AllResources::findResource(waveformParameters.absoluteResourceIndex, slotIndex, subchannelIndex, resourceIndex);

	waveformParameters.resourceType = g_slots[slotIndex]->getFunctionGeneratorResourceType(subchannelIndex, resourceIndex);
	if (waveformParameters.resourceType == FUNCTION_GENERATOR_RESOURCE_TYPE_U_AND_I) {
		SourceMode mode;
		channel_dispatcher::getSourceMode(slotIndex, subchannelIndex, mode, nullptr);
		waveformParameters.resourceType = mode == SOURCE_MODE_CURRENT ? FUNCTION_GENERATOR_RESOURCE_TYPE_I : FUNCTION_GENERATOR_RESOURCE_TYPE_U;
	}

	float minFreq;
	float maxFreq;
	g_slots[slotIndex]->getFunctionGeneratorFrequencyInfo(subchannelIndex, resourceIndex, minFreq, maxFreq);

	float minAmp;
	float maxAmp;
	g_slots[slotIndex]->getFunctionGeneratorAmplitudeInfo(subchannelIndex, resourceIndex, waveformParameters.resourceType, minAmp, maxAmp);

	waveformParameters.waveform = waveformParameters.resourceType == FUNCTION_GENERATOR_RESOURCE_TYPE_DIGITAL ? WAVEFORM_SQUARE : WAVEFORM_DC;
	waveformParameters.frequency = 10.0f;
	waveformParameters.phaseShift = 0;
	if (waveformParameters.waveform == WAVEFORM_DC) {
		waveformParameters.amplitude = (minAmp + maxAmp) / 2.0f;
		waveformParameters.offset = 0;
	} else {
		waveformParameters.amplitude = maxAmp - minAmp;
		waveformParameters.offset = (minAmp + maxAmp) / 2.0f;
	}
	waveformParameters.dutyCycle = 25.0f;
}

////////////////////////////////////////////////////////////////////////////////

class FunctionGeneratorSelectChannelsPage : public SetPage {
public:
    static const int PAGE_SIZE = 7;

    void pageAlloc() {
		AllResources::reset();

		m_selectedChannelsOrig = 0;
		for (int i = 0; i < g_functionGeneratorPage.m_selectedResources.m_numResources; i++) {
			m_selectedChannelsOrig |= ((uint64_t)1 << g_functionGeneratorPage.m_selectedResources.m_waveformParameters[i].absoluteResourceIndex);
		}
		m_selectedChannels = m_selectedChannelsOrig;
	}

    int getDirty() {
		return m_selectedChannels != m_selectedChannelsOrig;
	}

	void apply() {
		SelectedResources selectedResources;

		int i = 0;
		for (int absoluteResourceIndex = 0; absoluteResourceIndex < AllResources::getNumResources(); absoluteResourceIndex++) {
			if (m_selectedChannels & ((uint64_t)1 << absoluteResourceIndex)) {
				int j = g_functionGeneratorPage.m_selectedResources.findResource(absoluteResourceIndex);

				if (j != -1) {
					memcpy(&selectedResources.m_waveformParameters[i], &g_functionGeneratorPage.m_selectedResources.m_waveformParameters[j], sizeof(WaveformParameters));
				} else {
					auto &waveformParameters = selectedResources.m_waveformParameters[i];
					waveformParameters.absoluteResourceIndex = absoluteResourceIndex;
					initWaveformParameters(waveformParameters);
				}

				i++;
			}
		}

		selectedResources.m_numResources = i;

		memcpy(&g_functionGeneratorPage.m_selectedResources, &selectedResources, sizeof(selectedResources));

		g_functionGeneratorPage.m_selectedItem = 0;
		g_functionGeneratorPage.setScrollPosition(0);

		g_functionGeneratorPage.apply();
	}

    void set() {
		apply();
		popPage();
	}

	int getNumSelectedResources() {
		int numSelectedResources = 0;
		for (int absoluteResourceIndex = 0; absoluteResourceIndex < AllResources::getNumResources(); absoluteResourceIndex++) {
			if (m_selectedChannels & ((uint64_t)1 << absoluteResourceIndex)) {
				numSelectedResources++;
			}
		}
		return numSelectedResources;
	}

	int getScrollPosition() {
		return m_scrollPosition;
	}

	void setScrollPosition(int scrollPosition) {
		if (AllResources::getNumResources() <= PAGE_SIZE) {
			return;
		}

		if (scrollPosition < 0) {
			scrollPosition = 0;
		} else if (scrollPosition > AllResources::getNumResources() - PAGE_SIZE) {
			scrollPosition = AllResources::getNumResources() - PAGE_SIZE;
		}
		
		if (scrollPosition != m_scrollPosition) {
			m_scrollPosition = scrollPosition;
			refreshScreen();
		}
	}

    void onEncoder(int counter)  {
#if defined(EEZ_PLATFORM_SIMULATOR)
		counter = -counter;
#endif
		setScrollPosition(m_scrollPosition + counter);
	}

	uint64_t m_selectedChannels;

private:
	uint64_t m_selectedChannelsOrig;
	int m_scrollPosition;
};

FunctionGeneratorSelectChannelsPage g_functionGeneratorSelectChannelsPage;
eez::gui::SetPage *g_pFunctionGeneratorSelectChannelsPage = &g_functionGeneratorSelectChannelsPage;

////////////////////////////////////////////////////////////////////////////////

WaveformParameters *getWaveformParameters(int slotIndex, int subchannelIndex, int resourceIndex) {
	int absoluteResourceIndex = AllResources::getAbsoluteResourceIndex(slotIndex, subchannelIndex, resourceIndex);
	if (absoluteResourceIndex == -1) {
		return nullptr;
	}

	int i = g_selectedResources.findResource(absoluteResourceIndex);
	if (i == -1) {
		return nullptr;
	}

	return g_selectedResources.m_waveformParameters + i;
}

////////////////////////////////////////////////////////////////////////////////

void getProfileParameters(psu::profile::Parameters &profileParams) {
	profileParams.functionGeneratorParameters.options = g_options;

	for (int i = 0; i < MAX_NUM_WAVEFORMS; i++) {
		if (i < g_selectedResources.m_numResources) {
			auto &waveformParameters = g_selectedResources.m_waveformParameters[i];

			int slotIndex;
			int subchannelIndex;
			int resourceIndex;
			AllResources::findResource(waveformParameters.absoluteResourceIndex, slotIndex, subchannelIndex, resourceIndex);

			auto &profileWaveformParameters = profileParams.functionGeneratorParameters.waveformParameters[i];

			profileWaveformParameters.moduleType = g_slots[slotIndex]->moduleType;
			
			profileWaveformParameters.slotIndex = slotIndex;
			profileWaveformParameters.subchannelIndex = subchannelIndex;
			profileWaveformParameters.resourceIndex = resourceIndex;

			profileWaveformParameters.resourceType = waveformParameters.resourceType;
			
			profileWaveformParameters.waveform = waveformParameters.waveform;
			profileWaveformParameters.frequency = waveformParameters.frequency;
			profileWaveformParameters.phaseShift = waveformParameters.phaseShift;
			profileWaveformParameters.amplitude = waveformParameters.amplitude;
			profileWaveformParameters.offset = waveformParameters.offset;
			profileWaveformParameters.dutyCycle = waveformParameters.dutyCycle;
		} else {
			profileParams.functionGeneratorParameters.waveformParameters[i].moduleType = MODULE_TYPE_NONE;
		}
	}
}

void setProfileParameters(const psu::profile::Parameters &profileParams) {
	g_options = profileParams.functionGeneratorParameters.options;

	int j = 0;

	for (int i = 0; i < MAX_NUM_WAVEFORMS; i++) {
		if (profileParams.functionGeneratorParameters.waveformParameters[i].moduleType != MODULE_TYPE_NONE) {
			auto &profileWaveformParameters = profileParams.functionGeneratorParameters.waveformParameters[i];

			if (profileWaveformParameters.moduleType == MODULE_TYPE_NONE) {
				break;
			}

			if (profileWaveformParameters.moduleType != g_slots[profileWaveformParameters.slotIndex]->moduleType) {
				break;
			}

			int absoluteResourceIndex = AllResources::getAbsoluteResourceIndex(
				profileWaveformParameters.slotIndex, profileWaveformParameters.subchannelIndex, profileWaveformParameters.resourceIndex);
			if (absoluteResourceIndex == -1) {
				break;
			}

			TriggerMode triggerMode;
			if (!channel_dispatcher::getTriggerMode(profileWaveformParameters.slotIndex, profileWaveformParameters.subchannelIndex, profileWaveformParameters.resourceIndex, triggerMode, nullptr)) {
				break;
			}
			if (triggerMode != TRIGGER_MODE_FUNCTION_GENERATOR) {
				break;
			}

			auto &waveformParameters = g_selectedResources.m_waveformParameters[j];

			waveformParameters.absoluteResourceIndex = absoluteResourceIndex;

			waveformParameters.resourceType = (FunctionGeneratorResourceType)profileWaveformParameters.resourceType;
			
			waveformParameters.waveform = (Waveform)profileWaveformParameters.waveform;
			waveformParameters.frequency = profileWaveformParameters.frequency;
			waveformParameters.phaseShift = profileWaveformParameters.phaseShift;
			waveformParameters.amplitude = profileWaveformParameters.amplitude;
			waveformParameters.offset = profileWaveformParameters.offset;
			waveformParameters.dutyCycle = profileWaveformParameters.dutyCycle;

			j++;
		}
	}

	g_selectedResources.m_numResources = j;

	for (int i = 0; i < AllResources::getNumResources(); i++) {
		int slotIndex;
		int subchannelIndex;
		int resourceIndex;
		AllResources::findResource(i, slotIndex, subchannelIndex, resourceIndex);

		TriggerMode triggerMode;
		if (channel_dispatcher::getTriggerMode(slotIndex, subchannelIndex, resourceIndex, triggerMode, nullptr)) {
			if (triggerMode == TRIGGER_MODE_FUNCTION_GENERATOR) {
				if (g_selectedResources.findResource(i) == -1) {
					channel_dispatcher::setTriggerMode(slotIndex, subchannelIndex, resourceIndex, TRIGGER_MODE_FIXED, nullptr);
				}
			}
		}
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
	AllResources::reset();
	
	if (g_selectedResources.m_numResources == MAX_NUM_WAVEFORMS) {
		if (err) {
			*err = SCPI_ERROR_OUT_OF_MEMORY_FOR_REQ_OP;
		}
		return false;
	}

	int absoluteResourceIndex = AllResources::getAbsoluteResourceIndex(slotIndex, subchannelIndex, resourceIndex);
	if (absoluteResourceIndex == -1) {
		if (err) {
			*err = SCPI_ERROR_EXECUTION_ERROR;
		}
		return false;
	}

	int i = g_selectedResources.findResource(absoluteResourceIndex);
	if (i != -1) {
		return true;
	}

	for (i = 0; i < g_selectedResources.m_numResources; i++) {
		if (g_selectedResources.m_waveformParameters[i].absoluteResourceIndex > absoluteResourceIndex) {
			break;
		}
	}

	for (int j = g_selectedResources.m_numResources; j >= i + 1; j--) {
		g_selectedResources.m_waveformParameters[j] = g_selectedResources.m_waveformParameters[j - 1];
	}

	g_selectedResources.m_waveformParameters[i].absoluteResourceIndex = absoluteResourceIndex;

	initWaveformParameters(g_selectedResources.m_waveformParameters[i]);

	g_selectedResources.m_numResources++;

	gui::refreshScreen();

	return true;
}

void removeChannelWaveformParameters(int slotIndex, int subchannelIndex, int resourceIndex) {
	int absoluteResourceIndex = AllResources::getAbsoluteResourceIndex(slotIndex, subchannelIndex, resourceIndex);
	if (absoluteResourceIndex == -1) {
		return;
	}

	int i = g_selectedResources.findResource(absoluteResourceIndex);
	if (i == -1) {
		return;
	}

	for (int j = i + 1; j < g_selectedResources.m_numResources; j++) {
		g_selectedResources.m_waveformParameters[j - 1] = g_selectedResources.m_waveformParameters[j];
	}

	g_selectedResources.m_numResources--;

	gui::refreshScreen();
}

void removeAllChannels() {
	g_selectedResources.m_numResources = 0;

	gui::refreshScreen();
}

void removePowerChannels() {
	for (int i = 0; i < g_selectedResources.m_numResources; ) {
		int slotIndex;
		int subchannelIndex;
		int resourceIndex;
		AllResources::findResource(g_selectedResources.m_waveformParameters[i].absoluteResourceIndex, slotIndex, subchannelIndex, resourceIndex);
		
		auto *channel = Channel::getBySlotIndex(slotIndex, subchannelIndex);
		if (!channel) {
			i++;
			continue;
		}
		
		removeChannelWaveformParameters(slotIndex, subchannelIndex, resourceIndex);
	}
}

bool onTriggerModeChanged(int slotIndex, int subchannelIndex, int resourceIndex, TriggerMode triggerMode, int *err) {
	if (triggerMode == TRIGGER_MODE_FUNCTION_GENERATOR) {
		if (!addChannelWaveformParameters(slotIndex, subchannelIndex, resourceIndex, err)) {
            return false;
        }
	} else {
        removeChannelWaveformParameters(slotIndex, subchannelIndex, resourceIndex);
    }
	return true;
}

void selectWaveformParametersForChannel(int slotIndex, int subchannelIndex, int resourceIndex) {
	for (int i = 0; i < g_functionGeneratorPage.m_selectedResources.m_numResources; i++) {
		int tmpSlotIndex;
		int tmpSubchannelIndex;
		int tmpResourceIndex;
		AllResources::findResource(
			g_functionGeneratorPage.m_selectedResources.m_waveformParameters[i].absoluteResourceIndex,
			tmpSlotIndex, tmpSubchannelIndex, tmpResourceIndex);

		if (tmpSlotIndex == slotIndex && tmpSubchannelIndex == subchannelIndex && (resourceIndex == -1 || tmpResourceIndex == resourceIndex)) {
			g_functionGeneratorPage.m_selectedItem = i;
			
			if (i >= FunctionGeneratorPage::PAGE_SIZE) {
				g_functionGeneratorPage.setScrollPosition(i - FunctionGeneratorPage::PAGE_SIZE + 1);
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

	for (int i = 0; i < g_selectedResources.m_numResources; i++) {
		int slotIndex;
		int subchannelIndex;
		int resourceIndex;
		AllResources::findResource(g_selectedResources.m_waveformParameters[i].absoluteResourceIndex, slotIndex, subchannelIndex, resourceIndex);

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

void onWaveformChanged(WaveformParameters &waveformParameters, Waveform oldWaveform, Waveform newWaveform) {
	if (newWaveform == WAVEFORM_DC) {
		if (oldWaveform == WAVEFORM_HALF_RECTIFIED || oldWaveform == WAVEFORM_FULL_RECTIFIED) {
			waveformParameters.amplitude = waveformParameters.offset + waveformParameters.amplitude / 2.0f;
		} else {
			waveformParameters.amplitude = waveformParameters.offset;
		}
		waveformParameters.offset = 0;
	} else if (oldWaveform == WAVEFORM_DC) {
		float amplitude = waveformParameters.amplitude;
		if (newWaveform == WAVEFORM_HALF_RECTIFIED || newWaveform == WAVEFORM_FULL_RECTIFIED) {
			if (amplitude >= 0) {
				waveformParameters.amplitude = amplitude;
				waveformParameters.offset = 0;
			} else {
				waveformParameters.amplitude = -amplitude;
				waveformParameters.offset = amplitude;
			}
		} else {
			if (amplitude >= 0) {
				waveformParameters.amplitude = waveformParameters.offset = amplitude / 2.0f;
			} else {
				waveformParameters.amplitude = -amplitude / 2.0f;
				waveformParameters.offset = amplitude / 2.0f;
			}
		}
	} else if (
		!(oldWaveform == WAVEFORM_HALF_RECTIFIED || oldWaveform == WAVEFORM_FULL_RECTIFIED) &&
		(newWaveform == WAVEFORM_HALF_RECTIFIED || newWaveform == WAVEFORM_FULL_RECTIFIED)
	) {
		waveformParameters.offset -= waveformParameters.amplitude / 2;
	} else if (
		(oldWaveform == WAVEFORM_HALF_RECTIFIED || oldWaveform == WAVEFORM_FULL_RECTIFIED) &&
		!(newWaveform == WAVEFORM_HALF_RECTIFIED || newWaveform == WAVEFORM_FULL_RECTIFIED)
	) {
		waveformParameters.offset += waveformParameters.amplitude / 2;
	}

}

bool setWaveform(int slotIndex, int subchannelIndex, int resourceIndex, Waveform waveform, int *err) {
	if (!addChannelWaveformParameters(slotIndex, subchannelIndex, resourceIndex, err)) {
		return false;
	}
	
	WaveformParameters *waveformParameters = getWaveformParameters(slotIndex, subchannelIndex, resourceIndex);

	auto oldWaveform = waveformParameters->waveform;
	auto newWaveform = waveform;

	if (oldWaveform == newWaveform) {
		return true;
	}

	if (waveformParameters->resourceType == FUNCTION_GENERATOR_RESOURCE_TYPE_DIGITAL) {
		if (newWaveform != WAVEFORM_SQUARE && newWaveform != WAVEFORM_PULSE) {
			if (err) {
				*err = SCPI_ERROR_DATA_OUT_OF_RANGE;
			}
			return false;
		}
	}
	
	waveformParameters->waveform = newWaveform;

	onWaveformChanged(*waveformParameters, oldWaveform, newWaveform);
	
	g_functionGeneratorPage.init();

	if (g_active) {
		reloadWaveformParameters();
	}

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
	
	g_functionGeneratorPage.init();

	if (g_active) {
		reloadWaveformParameters();
	}

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

	g_functionGeneratorPage.init();
	
	if (g_active) {
		reloadWaveformParameters();
	}

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
	} else if (waveformParameters->waveform == WAVEFORM_HALF_RECTIFIED || waveformParameters->waveform == WAVEFORM_FULL_RECTIFIED) {
		if (waveformParameters->offset + amplitude > max) {
			if (err) {
				*err = SCPI_ERROR_DATA_OUT_OF_RANGE;
			}
			return false;
		}
	} else {
		if (waveformParameters->offset - amplitude / 2.0f < min || waveformParameters->offset + amplitude / 2.0f > max) {
			if (err) {
				*err = SCPI_ERROR_DATA_OUT_OF_RANGE;
			}
			return false;
		}
	}
	
	waveformParameters->amplitude = amplitude;

	g_functionGeneratorPage.init();
	
	if (g_active) {
		reloadWaveformParameters();
	}

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

	if (waveformParameters->waveform == WAVEFORM_DC) {
		if (err) {
			*err = SCPI_ERROR_EXECUTION_ERROR;
		}
		return false;
	}

	float min;
	float max;
	g_slots[slotIndex]->getFunctionGeneratorAmplitudeInfo(subchannelIndex, resourceIndex, waveformParameters->resourceType, min, max);

	if (waveformParameters->waveform == WAVEFORM_HALF_RECTIFIED || waveformParameters->waveform == WAVEFORM_FULL_RECTIFIED) {
		if (offset + waveformParameters->amplitude > max) {
			if (err) {
				*err = SCPI_ERROR_DATA_OUT_OF_RANGE;
			}
			return false;
		}
	} else {
		if (offset - waveformParameters->amplitude / 2.0f < min || offset + waveformParameters->amplitude / 2.0f > max) {
			if (err) {
				*err = SCPI_ERROR_DATA_OUT_OF_RANGE;
			}
			return false;
		}
	}

	waveformParameters->offset = offset;

	g_functionGeneratorPage.init();

	if (g_active) {
		reloadWaveformParameters();
	}

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

	g_functionGeneratorPage.init();
	
	if (g_active) {
		reloadWaveformParameters();
	}

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
	channel_dispatcher::setSourceMode(slotIndex, subchannelIndex, 
		resourceType == FUNCTION_GENERATOR_RESOURCE_TYPE_I ? SOURCE_MODE_CURRENT : SOURCE_MODE_VOLTAGE, nullptr);

	g_functionGeneratorPage.init();

	if (g_active) {
		reloadWaveformParameters();
	}

	return true;
}

void reset() {
	AllResources::reset();
	g_options.isFreq = 1;
	g_options.isAmpl = 1;
	removeAllChannels();
}

bool isActive() {
	return g_active;
}

float getMax(WaveformParameters &waveformParameters) {
	if (waveformParameters.waveform == WAVEFORM_DC) {
		return waveformParameters.amplitude;
	}
	
	if (waveformParameters.waveform == WAVEFORM_HALF_RECTIFIED || waveformParameters.waveform == WAVEFORM_FULL_RECTIFIED) {
		return waveformParameters.offset + waveformParameters.amplitude;
	}

	return waveformParameters.offset + waveformParameters.amplitude / 2.0f;
}

int checkLimits(int iChannel) {
    Channel &channel = Channel::get(iChannel);

    int absoluteResourceIndexU = AllResources::getAbsoluteResourceIndex(channel.slotIndex, channel.subchannelIndex, 0);
	int absoluteResourceIndexI = AllResources::getAbsoluteResourceIndex(channel.slotIndex, channel.subchannelIndex, 1);

	int indexU = g_selectedResources.findResource(absoluteResourceIndexU);
	int indexI = g_selectedResources.findResource(absoluteResourceIndexI);

	if (indexU == -1 || indexI == -1) {
		return SCPI_ERROR_INCOMPATIBLE_TRANSIENT_MODES;
	}

	float voltage = getMax(g_selectedResources.m_waveformParameters[indexU]);
	float current = getMax(g_selectedResources.m_waveformParameters[indexI]);

	if (channel.isVoltageLimitExceeded(voltage)) {
		g_errorChannelIndex = channel.channelIndex;
		return SCPI_ERROR_VOLTAGE_LIMIT_EXCEEDED;
	}

	if (channel.isCurrentLimitExceeded(current)) {
		g_errorChannelIndex = channel.channelIndex;
		return SCPI_ERROR_CURRENT_LIMIT_EXCEEDED;
	}

	int err;
	if (channel.isPowerLimitExceeded(voltage, current, &err)) {
		g_errorChannelIndex = channel.channelIndex;
		return err;
	}

    return 0;
}

void executionStart() {
	for (int i = 0; i < CH_NUM; i++) {
		g_dprogStateModified[i] = false;
	}

	for (int i = 0; i < g_selectedResources.m_numResources; i++) {
		int slotIndex;
		int subchannelIndex;
		int resourceIndex;
		AllResources::findResource(
			g_selectedResources.m_waveformParameters[i].absoluteResourceIndex,
			slotIndex, subchannelIndex, resourceIndex);

		TriggerMode triggerMode;
		g_slots[slotIndex]->getFunctionGeneratorResourceTriggerMode(subchannelIndex, resourceIndex, triggerMode, nullptr);

		if (triggerMode == TRIGGER_MODE_FUNCTION_GENERATOR) {
			g_active = true;
			reloadWaveformParameters();
			return;
		}
	}
}

void reloadWaveformParameters() {
	int trackingChannel= -1;

	for (int i = 0; i < CH_MAX; i++) {
		g_waveFormFuncU[i] = dcf;
		if (!g_active) {
			g_phiU[i] = 0;
		}
		g_dphiU[i] = 0;
		g_amplitudeU[i] = 0;
		g_offsetU[i] = 0;
		g_freqU[i] = 0;

		g_waveFormFuncI[i] = dcf;
		if (!g_active) {
			g_phiI[i] = 0;
		}
		g_dphiI[i] = 0;
		g_amplitudeI[i] = 0;
		g_offsetI[i] = 0;
	}

	for (int i = 0; i < g_selectedResources.m_numResources; i++) {
		int slotIndex;
		int subchannelIndex;
		int resourceIndex;
		AllResources::findResource(
			g_selectedResources.m_waveformParameters[i].absoluteResourceIndex,
			slotIndex, subchannelIndex, resourceIndex);

		auto channel = Channel::getBySlotIndex(slotIndex, subchannelIndex);

		if (
			channel &&
			(
				channel->flags.voltageTriggerMode == TRIGGER_MODE_FUNCTION_GENERATOR ||
				channel->flags.currentTriggerMode == TRIGGER_MODE_FUNCTION_GENERATOR
			)
		) {
			if (channel->channelIndex == 1 && (channel_dispatcher::getCouplingType() == channel_dispatcher::COUPLING_TYPE_PARALLEL || channel_dispatcher::getCouplingType() == channel_dispatcher::COUPLING_TYPE_SERIES)) {
				continue;
			}

			if (channel->flags.trackingEnabled) {
				if (trackingChannel != -1) {
					if (trackingChannel != channel->channelIndex) {
						continue;
					}
				} else {
					trackingChannel = channel->channelIndex;
				}
			}

			auto &waveformParameters = g_selectedResources.m_waveformParameters[i];

#if defined(EEZ_PLATFORM_STM32)
			__disable_irq();
#endif
			if (waveformParameters.resourceType == FUNCTION_GENERATOR_RESOURCE_TYPE_U) {
				g_waveFormFuncU[channel->channelIndex] = getWaveformFunction(waveformParameters);
				g_dutyCycleU[channel->channelIndex] = g_dutyCycle;
				g_phiU[channel->channelIndex] = 2.0 * M_PI * waveformParameters.phaseShift / 360.0f;
				g_dphiU[channel->channelIndex] = 2.0 * M_PI * waveformParameters.frequency * PERIOD;

				if (waveformParameters.waveform == WAVEFORM_DC) {
					g_amplitudeU[channel->channelIndex] = 0.0f;
					g_offsetU[channel->channelIndex] = waveformParameters.amplitude;
					g_freqU[channel->channelIndex] = 0;
				} else {
					g_amplitudeU[channel->channelIndex] = waveformParameters.amplitude;
					g_offsetU[channel->channelIndex] = waveformParameters.offset;
					g_freqU[channel->channelIndex] = waveformParameters.frequency;
				}
			} else {
				g_waveFormFuncI[channel->channelIndex] = getWaveformFunction(waveformParameters);
				g_dutyCycleI[channel->channelIndex] = g_dutyCycle;
				g_phiI[channel->channelIndex] = 2.0 * M_PI * waveformParameters.phaseShift / 360.0f;
				g_dphiI[channel->channelIndex] = 2.0 * M_PI * waveformParameters.frequency * PERIOD;

				if (waveformParameters.waveform == WAVEFORM_DC) {
					g_amplitudeI[channel->channelIndex] = 0.0f;
					g_offsetI[channel->channelIndex] = waveformParameters.amplitude;
				} else {
					g_amplitudeI[channel->channelIndex] = waveformParameters.amplitude;
					g_offsetI[channel->channelIndex] = waveformParameters.offset;
				}
			}
#if defined(EEZ_PLATFORM_STM32)
			__enable_irq();
#endif
		}
	}
}

void tick() {
	if (!g_functionGeneratorPage.m_initialized) {
		g_functionGeneratorPage.init();
	}

	if (!g_active) {
		return;
	}

	int trackingChannel = -1;

	for (int i = 0; i < CH_NUM; i++) {
		Channel &channel = Channel::get(i);

		if (channel.flags.trackingEnabled) {
			if (trackingChannel != -1) {
				if (trackingChannel != channel.channelIndex) {
					continue;
				}
			} else {
				trackingChannel = channel.channelIndex;
			}
		}

		if (channel.flags.voltageTriggerMode == TRIGGER_MODE_FUNCTION_GENERATOR) {
			g_dutyCycle = g_dutyCycleU[i];
			float value = g_offsetU[i] + g_amplitudeU[i] * g_waveFormFuncU[i](g_phiU[i]) / 2.0f;

			g_phiU[i] += g_dphiU[i];
			while (g_phiU[i] >= 2.0f * M_PI_F) {
				g_phiU[i] -= 2.0f * M_PI_F;
			}

			if (!io_pins::isInhibited()) {
				if (channel.isVoltageLimitExceeded(value)) {
					g_errorChannelIndex = channel.channelIndex;
					psuErrorMessage(channel.channelIndex, MakeScpiErrorValue(SCPI_ERROR_VOLTAGE_LIMIT_EXCEEDED));
					trigger::abort();
					return;
				}

				int err;
				if (channel.isPowerLimitExceeded(value, channel.i.set, &err)) {
					g_errorChannelIndex = channel.channelIndex;
					psuErrorMessage(channel.channelIndex, MakeScpiErrorValue(err));
					trigger::abort();
					return;
				}

				channel_dispatcher::setVoltage(channel, value);
			}
		}

		if (channel.flags.currentTriggerMode == TRIGGER_MODE_FUNCTION_GENERATOR) {
			g_dutyCycle = g_dutyCycleI[i];
			float value = g_offsetI[i] + g_amplitudeI[i] * g_waveFormFuncI[i](g_phiI[i]) / 2.0f;

			g_phiI[i] += g_dphiI[i];
			if (g_phiI[i] >= 2.0f * M_PI_F) {
				g_phiI[i] -= 2.0f * M_PI_F;
			}

			if (!io_pins::isInhibited()) {
				if (channel.isCurrentLimitExceeded(value)) {
					g_errorChannelIndex = channel.channelIndex;
					psuErrorMessage(channel.channelIndex, MakeScpiErrorValue(SCPI_ERROR_CURRENT_LIMIT_EXCEEDED));
					trigger::abort();
					return;
				}

				int err;
				if (channel.isPowerLimitExceeded(channel.u.set, value, &err)) {
					g_errorChannelIndex = channel.channelIndex;
					psuErrorMessage(channel.channelIndex, MakeScpiErrorValue(err));
					trigger::abort();
					return;
				}

				channel_dispatcher::setCurrent(channel, value);
			}
		}

		// If DCP405 is selected, for 100 Hz or more, and measured current of 1 A or more, DP has to be disabled
		if (g_slots[channel.slotIndex]->moduleType == MODULE_TYPE_DCP405 && g_freqU[i] >= 100.0f && channel.i.mon >= 0.5f) {
			if (channel.flags.dprogState) {
				channel.setDprogState(DPROG_STATE_OFF);
				g_dprogStateModified[channel.channelIndex] = true;
			}
		}
	}
}

void abort() {
	g_active = false;

	for (int i = 0; i < CH_NUM; i++) {
		if (g_dprogStateModified[i]) {
			Channel::get(i).setDprogState(DPROG_STATE_ON);
		}
	}
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
	
	Channel *channel = g_channel;

	pushPage(PAGE_ID_SYS_SETTINGS_FUNCTION_GENERATOR);

	selectWaveformParametersForChannel(*channel);
}

void data_function_generator_channels(DataOperationEnum operation, Cursor cursor, Value &value) {
	if (getActivePageId() == PAGE_ID_SYS_SETTINGS_FUNCTION_GENERATOR) {
		if (operation == DATA_OPERATION_COUNT) {
			value = g_functionGeneratorPage.m_selectedResources.m_numResources;
		}  else if (operation == DATA_OPERATION_SELECT) {
			int slotIndex;
			int subchannelIndex;
			int resourceIndex;
			if (
				AllResources::findResource(
					g_functionGeneratorPage.m_selectedResources.m_waveformParameters[cursor].absoluteResourceIndex,
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
			value = Value(g_functionGeneratorPage.m_selectedResources.m_numResources, VALUE_TYPE_UINT32);
		} else if (operation == DATA_OPERATION_YT_DATA_GET_POSITION) {
			value = Value(g_functionGeneratorPage.getScrollPosition(), VALUE_TYPE_UINT32);
		} else if (operation == DATA_OPERATION_YT_DATA_SET_POSITION) {
			g_functionGeneratorPage.setScrollPosition((int)value.getUInt32());
		} else if (operation == DATA_OPERATION_YT_DATA_GET_POSITION_INCREMENT) {
			value = 1;
		} else if (operation == DATA_OPERATION_YT_DATA_GET_PAGE_SIZE) {
			value = FunctionGeneratorPage::PAGE_SIZE;
		} 
		// scrollbar encoder support
		else if (operation == DATA_OPERATION_GET) {
			value = MakeValue(1.0f * g_functionGeneratorPage.getScrollPosition(), UNIT_NONE);
		} else if (operation == DATA_OPERATION_GET_MIN) {
			value = MakeValue(0, UNIT_NONE);
		} else if (operation == DATA_OPERATION_GET_MAX) {
			value = MakeValue(1.0f * (g_functionGeneratorPage.m_selectedResources.m_numResources - FunctionGeneratorPage::PAGE_SIZE), UNIT_NONE);
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
			g_functionGeneratorPage.setScrollPosition((uint32_t)value.getFloat());
		}
	} else if (getActivePageId() == PAGE_ID_SYS_SETTINGS_FUNCTION_GENERATOR_SELECT_CHANNELS) {
		if (operation == DATA_OPERATION_COUNT) {
			value = AllResources::getNumResources();
		}  else if (operation == DATA_OPERATION_SELECT) {
			int slotIndex;
			int subchannelIndex;
			int resourceIndex;
			AllResources::findResource(cursor, slotIndex, subchannelIndex, resourceIndex);

			value.type_ = VALUE_TYPE_CHANNEL_ID;
			value.pairOfUint16_.first = slotIndex;
			value.pairOfUint16_.second = subchannelIndex;
				
			hmi::g_selectedSlotIndex = slotIndex;
			hmi::g_selectedSubchannelIndex = subchannelIndex;
		} else if (operation == DATA_OPERATION_DESELECT) {
			if (value.getType() == VALUE_TYPE_CHANNEL_ID) {
				hmi::g_selectedSlotIndex = value.pairOfUint16_.first;
				hmi::g_selectedSubchannelIndex = value.pairOfUint16_.second;
			}
		} else if (operation == DATA_OPERATION_YT_DATA_GET_SIZE) {
			value = Value(AllResources::getNumResources(), VALUE_TYPE_UINT32);
		} else if (operation == DATA_OPERATION_YT_DATA_GET_POSITION) {
			value = Value(g_functionGeneratorSelectChannelsPage.getScrollPosition(), VALUE_TYPE_UINT32);
		} else if (operation == DATA_OPERATION_YT_DATA_SET_POSITION) {
			g_functionGeneratorSelectChannelsPage.setScrollPosition((int)value.getUInt32());
		} else if (operation == DATA_OPERATION_YT_DATA_GET_POSITION_INCREMENT) {
			value = 1;
		} else if (operation == DATA_OPERATION_YT_DATA_GET_PAGE_SIZE) {
			value = FunctionGeneratorSelectChannelsPage::PAGE_SIZE;
		}
	}
}

void data_function_generator_canvas(DataOperationEnum operation, Cursor cursor, Value &value) {
	if (operation == DATA_OPERATION_GET) {
		value = g_functionGeneratorPage.getRefreshState();
	} else if (operation == DATA_OPERATION_GET_CANVAS_DRAW_FUNCTION) {
		value = Value((void *)FunctionGeneratorPage::drawStatic, VALUE_TYPE_POINTER);
	} 
}

void data_function_generator_channel(DataOperationEnum operation, Cursor cursor, Value &value) {
	if (getActivePageId() == PAGE_ID_SYS_SETTINGS_FUNCTION_GENERATOR) {
		if (operation == DATA_OPERATION_GET) {
			int slotIndex;
			int subchannelIndex;
			int resourceIndex;
			AllResources::findResource(
				g_functionGeneratorPage.m_selectedResources.m_waveformParameters[cursor].absoluteResourceIndex,
				slotIndex, subchannelIndex, resourceIndex
			);
			value.type_ = VALUE_TYPE_CHANNEL_ID;
			value.pairOfUint16_.first = slotIndex;
			value.pairOfUint16_.second = subchannelIndex;
		}
	} else if (getActivePageId() == PAGE_ID_SYS_SETTINGS_FUNCTION_GENERATOR_SELECT_CHANNELS) {
		if (operation == DATA_OPERATION_GET) {
			int slotIndex;
			int subchannelIndex;
			int resourceIndex;
			AllResources::findResource(cursor, slotIndex, subchannelIndex, resourceIndex);
			value.type_ = VALUE_TYPE_CHANNEL_ID;
			value.pairOfUint16_.first = slotIndex;
			value.pairOfUint16_.second = subchannelIndex;
		}
	}
}

void data_function_generator_any_channel_selected(DataOperationEnum operation, Cursor cursor, Value &value) {
	if (getActivePageId() == PAGE_ID_SYS_SETTINGS_FUNCTION_GENERATOR) {
		if (operation == DATA_OPERATION_GET) {
			value = g_functionGeneratorPage.m_selectedItem >= 0 && 
				g_functionGeneratorPage.m_selectedItem < g_functionGeneratorPage.m_selectedResources.m_numResources;
		}
	}
}

void data_function_generator_item_label(DataOperationEnum operation, Cursor cursor, Value &value) {
	if (getActivePageId() == PAGE_ID_SYS_SETTINGS_FUNCTION_GENERATOR) {
		if (operation == DATA_OPERATION_GET) {
			int slotIndex;
			int subchannelIndex;
			int resourceIndex;
			AllResources::findResource(
				g_functionGeneratorPage.m_selectedResources.m_waveformParameters[cursor].absoluteResourceIndex,
				slotIndex, subchannelIndex, resourceIndex
			);
			value = g_slots[slotIndex]->getFunctionGeneratorResourceLabel(subchannelIndex, resourceIndex);
		}
	} else if (getActivePageId() == PAGE_ID_SYS_SETTINGS_FUNCTION_GENERATOR_SELECT_CHANNELS) {
		if (operation == DATA_OPERATION_GET) {
			int slotIndex;
			int subchannelIndex;
			int resourceIndex;
			AllResources::findResource(cursor, slotIndex, subchannelIndex, resourceIndex);
			value = g_slots[slotIndex]->getFunctionGeneratorResourceLabel(subchannelIndex, resourceIndex);
		}
	}
}
void data_function_generator_selected_item_label(DataOperationEnum operation, Cursor cursor, Value &value) {
	if (operation == DATA_OPERATION_GET) {
		int slotIndex;
		int subchannelIndex;
		int resourceIndex;
		AllResources::findResource(
			g_functionGeneratorPage.m_selectedResources.m_waveformParameters[g_functionGeneratorPage.m_selectedItem].absoluteResourceIndex,
			slotIndex, subchannelIndex, resourceIndex
		);
		value = g_slots[slotIndex]->getFunctionGeneratorResourceLabel(subchannelIndex, resourceIndex);
	}
}


void data_function_generator_item_is_selected(DataOperationEnum operation, Cursor cursor, Value &value) {
		if (operation == DATA_OPERATION_GET) {
			value = g_functionGeneratorPage.m_selectedItem == cursor;
		}
}

void action_function_generator_item_toggle_selected() {
	auto resourceIndex = getFoundWidgetAtDown().cursor;
	g_functionGeneratorPage.m_selectedItem = resourceIndex;
	g_functionGeneratorPage.apply();
}

void data_function_generator_item_is_checked(DataOperationEnum operation, Cursor cursor, Value &value) {
	if (operation == DATA_OPERATION_GET) {
		value = g_functionGeneratorSelectChannelsPage.m_selectedChannels & ((uint64_t)1 << cursor) ? 1 : 0;
	}
}

void data_function_generator_is_max_selected(DataOperationEnum operation, Cursor cursor, Value &value) {
	if (operation == DATA_OPERATION_GET) {
		int slotIndex;
		int subchannelIndex;
		int resourceIndex;
		AllResources::findResource(cursor, slotIndex, subchannelIndex, resourceIndex);

		if (Channel::getBySlotIndex(slotIndex, subchannelIndex)) {
			value = g_functionGeneratorSelectChannelsPage.getNumSelectedResources() >= MAX_NUM_WAVEFORMS - 1;
		} else {
			value = g_functionGeneratorSelectChannelsPage.getNumSelectedResources() == MAX_NUM_WAVEFORMS;
		}
	}
}

void data_function_generator_num_selected(DataOperationEnum operation, Cursor cursor, Value &value) {
	if (operation == DATA_OPERATION_GET) {
		value.type_ = VALUE_TYPE_NUM_SELECTED;
		value.pairOfUint16_.first = g_functionGeneratorSelectChannelsPage.getNumSelectedResources();
		value.pairOfUint16_.second = MIN(MAX_NUM_WAVEFORMS, AllResources::getNumResources());
	}
}

void action_function_generator_item_toggle_checked() {
	auto absoluteResourceIndex = getFoundWidgetAtDown().cursor;
	g_functionGeneratorSelectChannelsPage.m_selectedChannels ^= (uint64_t)1 << absoluteResourceIndex;

	bool isSelected = g_functionGeneratorSelectChannelsPage.m_selectedChannels & (uint64_t)1 << absoluteResourceIndex;

	int slotIndex;
	int subchannelIndex;
	int resourceIndex;
	AllResources::findResource(absoluteResourceIndex, slotIndex, subchannelIndex, resourceIndex);

	if (Channel::getBySlotIndex(slotIndex, subchannelIndex)) {
		if (resourceIndex == 0) {
			if (isSelected) {
				g_functionGeneratorSelectChannelsPage.m_selectedChannels |= (uint64_t)1 << (absoluteResourceIndex + 1);
			} else {
				g_functionGeneratorSelectChannelsPage.m_selectedChannels &= ~((uint64_t)1 << (absoluteResourceIndex + 1));
			}
		} else {
			if (isSelected) {
				g_functionGeneratorSelectChannelsPage.m_selectedChannels |= (uint64_t)1 << (absoluteResourceIndex - 1);
			} else {
				g_functionGeneratorSelectChannelsPage.m_selectedChannels &= ~((uint64_t)1 << (absoluteResourceIndex - 1));
			}
		}
	}
}

void data_function_generator_waveform(DataOperationEnum operation, Cursor cursor, Value &value) {
	if (operation == DATA_OPERATION_GET) {
		value = g_functionGeneratorPage.m_selectedResources.m_waveformParameters[g_functionGeneratorPage.m_selectedItem].waveform;
	}
}

void data_function_generator_waveform_label(DataOperationEnum operation, Cursor cursor, Value &value) {
	if (operation == DATA_OPERATION_GET) {
		value = g_waveformEnumDefinition[g_functionGeneratorPage.m_selectedResources.m_waveformParameters[g_functionGeneratorPage.m_selectedItem].waveform - 1].menuLabel;
	}
}

void data_function_generator_waveform_short_label(DataOperationEnum operation, Cursor cursor, Value &value) {
	if (operation == DATA_OPERATION_GET) {
		value = g_waveformShortLabel[g_functionGeneratorPage.m_selectedResources.m_waveformParameters[g_functionGeneratorPage.m_selectedItem].waveform];
	}
}

void setWaveform(uint16_t value) {
	popPage();

	auto &waveformParameters = g_functionGeneratorPage.m_selectedResources.m_waveformParameters[g_functionGeneratorPage.m_selectedItem];

	auto oldWaveform = waveformParameters.waveform;
	auto newWaveform = (Waveform)value;

	if (oldWaveform == newWaveform) {
		return;
	}

	waveformParameters.waveform = newWaveform;
	onWaveformChanged(waveformParameters, oldWaveform, newWaveform);
	
	g_functionGeneratorPage.apply();
}

bool disabledCallback(uint16_t value) {
	if (g_functionGeneratorPage.m_selectedResources.m_waveformParameters[g_functionGeneratorPage.m_selectedItem].resourceType == FUNCTION_GENERATOR_RESOURCE_TYPE_DIGITAL) {
		return value == WAVEFORM_DC || value == WAVEFORM_SINE || value == WAVEFORM_HALF_RECTIFIED || value == WAVEFORM_FULL_RECTIFIED || value == WAVEFORM_TRIANGLE || value == WAVEFORM_SAWTOOTH || value == WAVEFORM_ARBITRARY;
	}
	return value == WAVEFORM_ARBITRARY;
}

void action_function_generator_select_waveform() {
	pushSelectFromEnumPage(g_waveformEnumDefinition,
		g_functionGeneratorPage.m_selectedResources.m_waveformParameters[g_functionGeneratorPage.m_selectedItem].waveform,
		disabledCallback, setWaveform, true);
}

void getDurationStepValues(StepValues *stepValues) {
	auto &waveformParameters = g_functionGeneratorPage.m_selectedResources.m_waveformParameters[g_functionGeneratorPage.m_selectedItem];

	int slotIndex;
	int subchannelIndex;
	int resourceIndex;
	AllResources::findResource(waveformParameters.absoluteResourceIndex,
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

void getPhaseShiftStepValues(StepValues *stepValues) {
	auto &waveformParameters = g_functionGeneratorPage.m_selectedResources.m_waveformParameters[g_functionGeneratorPage.m_selectedItem];

	float range = 1.0f / waveformParameters.frequency;

	static float g_values[] = { 0.01f * range, 0.02f * range, 0.05f * range, 0.1f * range };

	stepValues->values = g_values;
	stepValues->count = sizeof(g_values) / sizeof(float);
	stepValues->unit = UNIT_SECOND;

	stepValues->encoderSettings.accelerationEnabled = true;

	stepValues->encoderSettings.range = range;
	stepValues->encoderSettings.step = stepValues->values[0];
}

void data_function_generator_frequency(DataOperationEnum operation, Cursor cursor, Value &value) {
	auto &waveformParameters = g_functionGeneratorPage.m_selectedResources.m_waveformParameters[g_functionGeneratorPage.m_selectedItem];

	int slotIndex;
	int subchannelIndex;
	int resourceIndex;
	AllResources::findResource(waveformParameters.absoluteResourceIndex,
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
		g_functionGeneratorPage.apply();
    }
}

void data_function_generator_phase_shift(DataOperationEnum operation, Cursor cursor, Value &value) {
	auto &waveformParameters = g_functionGeneratorPage.m_selectedResources.m_waveformParameters[g_functionGeneratorPage.m_selectedItem];

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
			value = MakeValue(g_options.isFreq ? waveformParameters.phaseShift : (waveformParameters.phaseShift * max / 360.0f), unit);
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
			getPhaseShiftStepValues(stepValues);
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
		g_functionGeneratorPage.apply();
	}
}

void data_function_generator_amplitude(DataOperationEnum operation, Cursor cursor, Value &value) {
	// ... or minimum
	auto &waveformParameters = g_functionGeneratorPage.m_selectedResources.m_waveformParameters[g_functionGeneratorPage.m_selectedItem];

	int slotIndex;
	int subchannelIndex;
	int resourceIndex;
	AllResources::findResource(waveformParameters.absoluteResourceIndex,
		slotIndex, subchannelIndex, resourceIndex);
	float min;
	float max;
	StepValues stepValues;
	g_slots[slotIndex]->getFunctionGeneratorAmplitudeInfo(subchannelIndex, resourceIndex, waveformParameters.resourceType, min, max, &stepValues);

	float range = max - min;

	Unit unit = waveformParameters.resourceType == FUNCTION_GENERATOR_RESOURCE_TYPE_U ? UNIT_VOLT : UNIT_AMPER;

	if (g_options.isAmpl) {
		if (waveformParameters.waveform != WAVEFORM_DC) {
			unit = unit == UNIT_VOLT ? UNIT_VOLT_PP : UNIT_AMPER_PP;
		}
	}

	if (operation == DATA_OPERATION_GET) {
		bool focused = g_focusCursor == cursor && g_focusDataId == DATA_ID_FUNCTION_GENERATOR_AMPLITUDE;
		if (focused && g_focusEditValue.getType() != VALUE_TYPE_NONE) {
			value = g_focusEditValue;
		} else if (focused && getActivePageId() == PAGE_ID_EDIT_MODE_KEYPAD && edit_mode_keypad::g_keypad->isEditing()) {
			data_keypad_text(operation, cursor, value);
		} else {
			if (waveformParameters.waveform == WAVEFORM_DC) {
				value = MakeValue(waveformParameters.amplitude, unit);
			} else if (waveformParameters.waveform == WAVEFORM_HALF_RECTIFIED || waveformParameters.waveform == WAVEFORM_FULL_RECTIFIED) {
				value = MakeValue(g_options.isAmpl ? waveformParameters.amplitude : waveformParameters.offset, unit);
			} else {
				value = MakeValue(g_options.isAmpl ? waveformParameters.amplitude : (waveformParameters.offset - waveformParameters.amplitude / 2.0f), unit);
			}
			value.float_ = roundPrec(value.float_, stepValues.values[0]);
		}
	} else if (operation == DATA_OPERATION_GET_ALLOW_ZERO) {
		value = 0;
	} else if (operation == DATA_OPERATION_GET_MIN) {
		if (waveformParameters.waveform == WAVEFORM_DC) {
			value = MakeValue(min, unit);
		} else if (waveformParameters.waveform == WAVEFORM_HALF_RECTIFIED || waveformParameters.waveform == WAVEFORM_FULL_RECTIFIED) {
			value = MakeValue(g_options.isAmpl ? 0 : min, unit);
		} else {
			value = MakeValue(g_options.isAmpl ? 0 : min, unit);
		}
	} else if (operation == DATA_OPERATION_GET_MAX) {
		if (waveformParameters.waveform == WAVEFORM_DC) {
			value = MakeValue(max, unit);
		} else if (waveformParameters.waveform == WAVEFORM_HALF_RECTIFIED || waveformParameters.waveform == WAVEFORM_FULL_RECTIFIED) {
			value = MakeValue(
				g_options.isAmpl ?
					max - waveformParameters.offset : 
					waveformParameters.offset + waveformParameters.amplitude,
				unit
			);
		} else {
			value = MakeValue(
				g_options.isAmpl ? 
					2.0f * MIN(waveformParameters.offset - min, max - waveformParameters.offset) : 
					waveformParameters.offset + waveformParameters.amplitude / 2,
				unit
			);
		}
	} else if (operation == DATA_OPERATION_GET_NAME) {
		value = g_options.isAmpl ? "Amplitude" : "Minimum";
	} else if (operation == DATA_OPERATION_GET_UNIT) {
		value = unit;
	} else if (operation == DATA_OPERATION_GET_ENCODER_STEP_VALUES) {
		StepValues *pStepValues = value.getStepValues();

		memcpy(pStepValues, &stepValues, sizeof(stepValues));

		pStepValues->encoderSettings.accelerationEnabled = true;

		pStepValues->encoderSettings.range = range;
		pStepValues->encoderSettings.step = pStepValues->values[0];

		pStepValues->encoderSettings.mode = eez::psu::gui::edit_mode_step::g_functionGeneratorAmplitudeEncoderMode;

		value = 1;
	} else if (operation == DATA_OPERATION_SET_ENCODER_MODE) {
		eez::psu::gui::edit_mode_step::g_functionGeneratorAmplitudeEncoderMode = (EncoderMode)value.getInt();
	} else if (operation == DATA_OPERATION_SET) {
		float amplitude = roundPrec(value.getFloat(), stepValues.values[0]);
		if (g_options.isAmpl || waveformParameters.waveform == WAVEFORM_DC) {
			waveformParameters.amplitude = amplitude;
		} else {
			if (waveformParameters.waveform == WAVEFORM_HALF_RECTIFIED || waveformParameters.waveform == WAVEFORM_FULL_RECTIFIED) {
				float min = amplitude;
				float max = waveformParameters.offset + waveformParameters.amplitude;
				waveformParameters.offset = min;
				waveformParameters.amplitude = max - min;
			} else {
				float min = amplitude;
				float max = waveformParameters.offset + waveformParameters.amplitude / 2.0f;
				waveformParameters.amplitude = max - min;
				waveformParameters.offset = (min + max) / 2.0f;
			}
		}
		g_functionGeneratorPage.apply();
	}
}

void data_function_generator_offset(DataOperationEnum operation, Cursor cursor, Value &value) {
	// ... or maximum
	auto &waveformParameters = g_functionGeneratorPage.m_selectedResources.m_waveformParameters[g_functionGeneratorPage.m_selectedItem];

	int slotIndex;
	int subchannelIndex;
	int resourceIndex;
	AllResources::findResource(waveformParameters.absoluteResourceIndex,
		slotIndex, subchannelIndex, resourceIndex);
	float min;
	float max;
	StepValues stepValues;
	g_slots[slotIndex]->getFunctionGeneratorAmplitudeInfo(subchannelIndex, resourceIndex, waveformParameters.resourceType, min, max, &stepValues);

	float range = max - min;

	Unit unit = waveformParameters.resourceType == FUNCTION_GENERATOR_RESOURCE_TYPE_U ? UNIT_VOLT: UNIT_AMPER;

	if (operation == DATA_OPERATION_GET) {
		bool focused = g_focusCursor == cursor && g_focusDataId == DATA_ID_FUNCTION_GENERATOR_OFFSET;
		if (focused && g_focusEditValue.getType() != VALUE_TYPE_NONE) {
			value = g_focusEditValue;
		} else if (focused && getActivePageId() == PAGE_ID_EDIT_MODE_KEYPAD && edit_mode_keypad::g_keypad->isEditing()) {
			data_keypad_text(operation, cursor, value);
		} else {
			if (waveformParameters.waveform == WAVEFORM_HALF_RECTIFIED || waveformParameters.waveform == WAVEFORM_FULL_RECTIFIED) {
				value = MakeValue(g_options.isAmpl ? waveformParameters.offset : waveformParameters.offset + waveformParameters.amplitude, unit);
			} else {
				value = MakeValue(g_options.isAmpl ? waveformParameters.offset : (waveformParameters.offset + waveformParameters.amplitude / 2.0f), unit);
			}
			value.float_ = roundPrec(value.float_, stepValues.values[0]);
		}
	} else if (operation == DATA_OPERATION_GET_ALLOW_ZERO) {
		value = 0;
	} else if (operation == DATA_OPERATION_GET_MIN) {
		if (waveformParameters.waveform == WAVEFORM_HALF_RECTIFIED || waveformParameters.waveform == WAVEFORM_FULL_RECTIFIED) {
			value = MakeValue(g_options.isAmpl ? min : waveformParameters.offset, unit);
		} else {
			value = MakeValue(
				g_options.isAmpl ? 
					min + waveformParameters.amplitude / 2.0f : 
					waveformParameters.offset - waveformParameters.amplitude / 2,
				unit
			);
		}
	} else if (operation == DATA_OPERATION_GET_MAX) {
		if (waveformParameters.waveform == WAVEFORM_HALF_RECTIFIED || waveformParameters.waveform == WAVEFORM_FULL_RECTIFIED) {
			value = MakeValue(
				g_options.isAmpl ? 
					max - waveformParameters.amplitude : 
					max,
				unit
			);
		} else {
			value = MakeValue(
				g_options.isAmpl ?
					max - waveformParameters.amplitude / 2.0f :
					max,
				unit);
		}
	} else if (operation == DATA_OPERATION_GET_NAME) {
		value = g_options.isAmpl ? "Offset" : "Maximum";
	} else if (operation == DATA_OPERATION_GET_UNIT) {
		value = unit;
	} else if (operation == DATA_OPERATION_GET_ENCODER_STEP_VALUES) {
		StepValues *pStepValues = value.getStepValues();

		memcpy(pStepValues, &stepValues, sizeof(stepValues));

		pStepValues->encoderSettings.accelerationEnabled = true;

		pStepValues->encoderSettings.range = range;
		pStepValues->encoderSettings.step = pStepValues->values[0];

		pStepValues->encoderSettings.mode = eez::psu::gui::edit_mode_step::g_functionGeneratorOffsetEncoderMode;

		value = 1;
	} else if (operation == DATA_OPERATION_SET_ENCODER_MODE) {
		eez::psu::gui::edit_mode_step::g_functionGeneratorOffsetEncoderMode = (EncoderMode)value.getInt();
	} else if (operation == DATA_OPERATION_SET) {
		float offset = roundPrec(value.getFloat(), stepValues.values[0]);
		if (g_options.isAmpl) {
			waveformParameters.offset = offset;
		} else {
			if (waveformParameters.waveform == WAVEFORM_HALF_RECTIFIED || waveformParameters.waveform == WAVEFORM_FULL_RECTIFIED) {
				waveformParameters.amplitude = offset - waveformParameters.offset;
			} else {
				float min = waveformParameters.offset - waveformParameters.amplitude / 2.0f;
				float max = offset;
				waveformParameters.amplitude = max - min;
				waveformParameters.offset = (min + max) / 2.0f;
			}
		}
		g_functionGeneratorPage.apply();
	}
}

void data_function_generator_duty_cycle(DataOperationEnum operation, Cursor cursor, Value &value) {
	auto &waveformParameters = g_functionGeneratorPage.m_selectedResources.m_waveformParameters[g_functionGeneratorPage.m_selectedItem];

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
			value = MakeValue(g_options.isFreq ? waveformParameters.dutyCycle : (waveformParameters.dutyCycle * max / 100.0f), unit);
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
			getPhaseShiftStepValues(stepValues);
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
		g_functionGeneratorPage.apply();
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
		if (
			g_functionGeneratorPage.m_selectedItem >= 0 && g_functionGeneratorPage.m_selectedItem < g_functionGeneratorPage.m_selectedResources.m_numResources &&
			AllResources::findResource(
				g_functionGeneratorPage.m_selectedResources.m_waveformParameters[g_functionGeneratorPage.m_selectedItem].absoluteResourceIndex,
				slotIndex, subchannelIndex, resourceIndex
			)
		) {
			value = g_slots[slotIndex]->getFunctionGeneratorResourceType(subchannelIndex, resourceIndex) == FUNCTION_GENERATOR_RESOURCE_TYPE_U_AND_I;
		} else {
			value = 0;
		}
	}
}

void data_function_generator_mode(DataOperationEnum operation, Cursor cursor, Value &value) {
	if (operation == DATA_OPERATION_GET) {
		value = g_functionGeneratorPage.m_selectedResources.m_waveformParameters[g_functionGeneratorPage.m_selectedItem].resourceType == FUNCTION_GENERATOR_RESOURCE_TYPE_U ? "Voltage" : "Current";
	}
}

void action_function_generator_mode_select_mode() {
	auto &waveformParameters = g_functionGeneratorPage.m_selectedResources.m_waveformParameters[g_functionGeneratorPage.m_selectedItem];
	if (waveformParameters.resourceType == FUNCTION_GENERATOR_RESOURCE_TYPE_U) {
		waveformParameters.resourceType = FUNCTION_GENERATOR_RESOURCE_TYPE_I;
	} else {
		waveformParameters.resourceType = FUNCTION_GENERATOR_RESOURCE_TYPE_U;
	}

	int slotIndex;
	int subchannelIndex;
	int resourceIndex;
	AllResources::findResource(waveformParameters.absoluteResourceIndex, slotIndex, subchannelIndex, resourceIndex);

	channel_dispatcher::setSourceMode(slotIndex, subchannelIndex,
		waveformParameters.resourceType == FUNCTION_GENERATOR_RESOURCE_TYPE_I ? SOURCE_MODE_CURRENT : SOURCE_MODE_VOLTAGE, nullptr);

	float minFreq;
	float maxFreq;
	g_slots[slotIndex]->getFunctionGeneratorFrequencyInfo(subchannelIndex, resourceIndex, minFreq, maxFreq);

	float minAmp;
	float maxAmp;
	g_slots[slotIndex]->getFunctionGeneratorAmplitudeInfo(subchannelIndex, resourceIndex, waveformParameters.resourceType, minAmp, maxAmp);

	waveformParameters.frequency = 10.0f;
	waveformParameters.amplitude = maxAmp - minAmp;
	waveformParameters.offset = (minAmp + maxAmp) / 2.0f;

	g_functionGeneratorPage.apply();
}

void data_function_generator_has_amplitude_and_offset(DataOperationEnum operation, Cursor cursor, Value &value) {
	if (operation == DATA_OPERATION_GET) {
		value = g_functionGeneratorPage.m_selectedResources.m_waveformParameters[g_functionGeneratorPage.m_selectedItem].resourceType != FUNCTION_GENERATOR_RESOURCE_TYPE_DIGITAL;
	}
}

void data_function_generator_has_duty_cycle(DataOperationEnum operation, Cursor cursor, Value &value) {
	if (operation == DATA_OPERATION_GET) {
		value = g_functionGeneratorPage.m_selectedResources.m_waveformParameters[g_functionGeneratorPage.m_selectedItem].waveform == WAVEFORM_PULSE;
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
		value = g_functionGeneratorPage.m_selectedResources.m_waveformParameters[g_functionGeneratorPage.m_selectedItem].waveform == WAVEFORM_DC;
	}
}

void data_function_generator_any_selected(DataOperationEnum operation, Cursor cursor, Value &value) {
	if (operation == DATA_OPERATION_GET) {
		if (getActivePageId() == PAGE_ID_SYS_SETTINGS_FUNCTION_GENERATOR) {
			value = g_functionGeneratorPage.m_selectedResources.m_numResources > 0;
		} else {
			value = g_functionGeneratorSelectChannelsPage.m_selectedChannels != 0;
		}
	}
}

void data_function_generator_is_any_channel_active(DataOperationEnum operation, Cursor cursor, Value &value) {
	if (operation == DATA_OPERATION_GET) {
		value = g_functionGeneratorPage.m_selectedResources.m_numResources > 0;
	}
}

void action_function_generator_trigger() {
	if (g_functionGeneratorPage.getDirty()) {
		g_functionGeneratorPage.set();
	}
	action_trigger_manually();
}

void action_function_generator_deselect_all() {
	g_functionGeneratorSelectChannelsPage.m_selectedChannels = 0;
}

void closeFunctionGeneratorPage() {
	g_functionGeneratorPage.restoreSlotIndex();
	popPage();
}

void action_function_generator_show_previous_page() {
    if (g_functionGeneratorPage.getDirty()) {
        areYouSureWithMessage(g_discardMessage, closeFunctionGeneratorPage);
    } else {
        closeFunctionGeneratorPage();
    }	
}



} // namespace gui
} // namespace eez
