/*
* EEZ Generic Firmware
* Copyright (C) 2018-present, Envox d.o.o.
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

#if OPTION_DISPLAY
#include <eez/gui/app_context.h>
#endif

namespace eez {

typedef bool (*OnSystemStateChangedCallback)();
extern OnSystemStateChangedCallback g_onSystemStateChangedCallbacks[];
extern int g_numOnSystemStateChangedCallbacks;

enum TestResult {
    TEST_NONE,
    TEST_FAILED,
    TEST_OK,
    TEST_CONNECTING,
    TEST_SKIPPED,
    TEST_WARNING
};

enum AdcDataType {
    ADC_DATA_TYPE_NONE,
    ADC_DATA_TYPE_U_MON,
    ADC_DATA_TYPE_I_MON,
    ADC_DATA_TYPE_U_MON_DAC,
    ADC_DATA_TYPE_I_MON_DAC,
};

struct ChannelInterface {
	int slotIndex;

    ChannelInterface(int slotIndex);

    virtual void init(int subchannelIndex) = 0;
    virtual void reset(int subchannelIndex) = 0;
    virtual void test(int subchannelIndex) = 0;
    virtual void tick(int subchannelIndex, uint32_t tickCount) = 0;

    virtual TestResult getTestResult(int subchannelIndex) = 0;

    virtual unsigned getRPol(int subchannelIndex);

    virtual bool isCcMode(int subchannelIndex) = 0;
    virtual bool isCvMode(int subchannelIndex) = 0;

    virtual void adcMeasureUMon(int subchannelIndex) = 0;
    virtual void adcMeasureIMon(int subchannelIndex) = 0;
    virtual void adcMeasureMonDac(int subchannelIndex) = 0;
    virtual void adcMeasureAll(int subchannelIndex) = 0;

    virtual void setOutputEnable(int subchannelIndex, bool enable) = 0;

    virtual void setDacVoltage(int subchannelIndex, uint16_t value) = 0;
    virtual void setDacVoltageFloat(int subchannelIndex, float value) = 0;
    virtual void setDacCurrent(int subchannelIndex, uint16_t value) = 0;
    virtual void setDacCurrentFloat(int subchannelIndex, float value) = 0;

    virtual bool isDacTesting(int subchannelIndex) = 0;

    virtual void setRemoteSense(int subchannelIndex, bool enable);
    virtual void setRemoteProgramming(int subchannelIndex, bool enable);

    virtual void setCurrentRange(int subchannelIndex);

    virtual bool isVoltageBalanced(int subchannelIndex);
    virtual bool isCurrentBalanced(int subchannelIndex);
    virtual float getUSetUnbalanced(int subchannelIndex);
    virtual float getISetUnbalanced(int subchannelIndex);

    virtual void readAllRegisters(int subchannelIndex, uint8_t ioexpRegisters[], uint8_t adcRegisters[]);

    virtual void onSpiIrq();

#if defined(DEBUG) && defined(EEZ_PLATFORM_STM32)
    virtual int getIoExpBitDirection(int subchannelIndex, int io_bit);
    virtual bool testIoExpBit(int subchannelIndex, int io_bit);
    virtual void changeIoExpBit(int subchannelIndex, int io_bit, bool set);
#endif
};

static const uint8_t MODULE_TYPE_NONE = 0;
static const uint8_t MODULE_TYPE_DCP405 = 1;
static const uint8_t MODULE_TYPE_DCM220 = 2;
static const uint8_t MODULE_TYPE_DCP505 = 3;

struct ModuleInfo {
    uint16_t moduleId;
    uint8_t lasestBoardRevision; // TODO should be lasestModuleRevision
    uint8_t numChannels;
    ChannelInterface **channelInterfaces;
};

extern ModuleInfo g_modules[];

struct SlotInfo {
    uint8_t moduleType; // MODULE_TYPE_...
    uint8_t boardRevision; // TODO should be moduleRevision;
    uint8_t channelIndex;
};

static const int NUM_SLOTS = 3;
extern SlotInfo g_slots[NUM_SLOTS];

#if OPTION_DISPLAY

struct ApplicationInfo {
    const char *title;
    int bitmap;
    gui::AppContext *appContext;
};

typedef bool (*OnSystemStateChangedCallback)();
extern ApplicationInfo g_applications[];
extern int g_numApplications;

namespace gui {

class Page;

Page *getPageFromId(int pageId);

} // namespace gui

#endif

} // namespace eez
