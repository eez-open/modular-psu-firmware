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

#include <eez/unit.h>

namespace eez {

enum TestResult {
    TEST_NONE,
    TEST_FAILED,
    TEST_OK,
    TEST_CONNECTING,
    TEST_SKIPPED,
    TEST_WARNING
};

enum AdcDataType {
    ADC_DATA_TYPE_U_MON,
    ADC_DATA_TYPE_I_MON,
    ADC_DATA_TYPE_U_MON_DAC,
    ADC_DATA_TYPE_I_MON_DAC,
};

enum DprogState {
    DPROG_STATE_OFF = 0,
    DPROG_STATE_ON = 1
};

enum ChannelFeatures {
    CH_FEATURE_VOLT = (1 << 0),
    CH_FEATURE_CURRENT = (1 << 1),
    CH_FEATURE_POWER = (1 << 2),
    CH_FEATURE_OE = (1 << 3),
    CH_FEATURE_DPROG = (1 << 4),
    CH_FEATURE_RPROG = (1 << 5),
    CH_FEATURE_RPOL = (1 << 6),
    CH_FEATURE_CURRENT_DUAL_RANGE = (1 << 7),
    CH_FEATURE_HW_OVP = (1 << 8),
    CH_FEATURE_COUPLING = (1 << 9)
};

struct StepValues {
    int count;
    const float *values;
    Unit unit;
};

struct ChannelParams {
    float U_MIN;
    float U_DEF;
    float U_MAX;
    
    float U_MIN_STEP;
    float U_DEF_STEP;
    float U_MAX_STEP;

    float U_CAL_VAL_MIN;
    float U_CAL_VAL_MID;
    float U_CAL_VAL_MAX;
    float U_CURR_CAL;

    float I_MIN;
    float I_DEF;
    float I_MAX;
    
    float I_MIN_STEP;
    float I_DEF_STEP;
    float I_MAX_STEP; 
    
    float I_CAL_VAL_MIN;
    float I_CAL_VAL_MID;
    float I_CAL_VAL_MAX;
    float I_VOLT_CAL;

    bool OVP_DEFAULT_STATE;
    float OVP_MIN_DELAY;
    float OVP_DEFAULT_DELAY;
    float OVP_MAX_DELAY;

    bool OCP_DEFAULT_STATE;
    float OCP_MIN_DELAY;
    float OCP_DEFAULT_DELAY;
    float OCP_MAX_DELAY;

    bool OPP_DEFAULT_STATE;
    float OPP_MIN_DELAY;
    float OPP_DEFAULT_DELAY;
    float OPP_MAX_DELAY;
    float OPP_MIN_LEVEL;
    float OPP_DEFAULT_LEVEL;

    float PTOT;

    float U_RESOLUTION;
    float I_RESOLUTION;
    float I_LOW_RESOLUTION;
    float P_RESOLUTION;

    float VOLTAGE_GND_OFFSET; // [V], (1375 / 65535) * (40V | 50V)
    float CURRENT_GND_OFFSET; // [A]

    /// Maximum difference, in percentage, between ADC
    /// and real value during calibration.
    float CALIBRATION_DATA_TOLERANCE_PERCENT;

    /// Maximum difference, in percentage, between calculated mid value
    /// and real mid value during calibration.
    float CALIBRATION_MID_TOLERANCE_PERCENT;

    /// Returns features present (check ChannelFeatures) in board revision of this channel.
    uint32_t features;

    uint32_t MON_REFRESH_RATE_MS;
};

struct ChannelInterface {
	int slotIndex;

    ChannelInterface(int slotIndex);

    virtual void getParams(int subchannelIndex, ChannelParams &params) = 0;

    virtual void init(int subchannelIndex) = 0;
    virtual void onPowerDown(int subchannelIndex) = 0;
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

    virtual void setOutputEnable(int subchannelIndex, bool enable, uint16_t tasks) = 0;

    virtual void setDprogState(DprogState dprogState) = 0;

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

    virtual void getFirmwareVersion(uint8_t &majorVersion, uint8_t &minorVersion) = 0;
    virtual const char *getBrand() = 0;
    virtual void getSerial(char *text) = 0;

    virtual void getVoltageStepValues(StepValues *stepValues) = 0;
    virtual void getCurrentStepValues(StepValues *stepValues) = 0;
    virtual void getPowerStepValues(StepValues *stepValues) = 0;

#if defined(DEBUG) && defined(EEZ_PLATFORM_STM32)
    virtual int getIoExpBitDirection(int subchannelIndex, int io_bit);
    virtual bool testIoExpBit(int subchannelIndex, int io_bit);
    virtual void changeIoExpBit(int subchannelIndex, int io_bit, bool set);
#endif
};

static const uint16_t MODULE_TYPE_NONE    = 0;
static const uint16_t MODULE_TYPE_DCP405  = 405;
static const uint16_t MODULE_TYPE_DCP405B = 406;
static const uint16_t MODULE_TYPE_DCP505  = 505;
static const uint16_t MODULE_TYPE_DCM220  = 220;

static const uint16_t MODULE_REVISION_DCP405_R1B1  = 0x0101;
static const uint16_t MODULE_REVISION_DCP405_R2B5  = 0x0205;
static const uint16_t MODULE_REVISION_DCP405_R2B7  = 0x0207;
static const uint16_t MODULE_REVISION_DCP405_R3B1  = 0x0301;

static const uint16_t MODULE_REVISION_DCP405B_R2B7 = 0x0207;

static const uint16_t MODULE_REVISION_DCP505_R1B3  = 0x0103;

static const uint16_t MODULE_REVISION_DCM220_R2B4  = 0x0204;

struct ModuleInfo {
    uint16_t moduleType;
    const char *moduleName;
    uint16_t latestModuleRevision;
    uint8_t numChannels;
    ChannelInterface **channelInterfaces;
};

ModuleInfo *getModuleInfo(uint16_t moduleType);

struct SlotInfo {
    ModuleInfo *moduleInfo;
    uint16_t moduleRevision;
    uint8_t channelIndex;
};

static const int NUM_SLOTS = 3;
static const int INVALID_SLOT_INDEX = NUM_SLOTS;
extern SlotInfo g_slots[NUM_SLOTS + 1]; // one more for invalid slot

} // namespace eez
