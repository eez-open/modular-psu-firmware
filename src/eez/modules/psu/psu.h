/*
 * EEZ Modular Firmware
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

#include <stdint.h>

#if !defined(EEZ_PLATFORM_SIMULATOR) && !defined(EEZ_PLATFORM_STM32)
#define EEZ_PLATFORM_STM32
#endif

#include <eez/tasks.h>

#include <eez/modules/psu/conf.h>
#include <eez/modules/psu/conf_advanced.h>
#include <eez/modules/psu/conf_user.h>

#include <eez/index.h>
#include <eez/firmware.h>
#include <eez/util.h>
#include <eez/debug.h>

#include <eez/modules/psu/channel.h>
#include <eez/modules/psu/serial.h>

/// Namespace for the everything from the EEZ.
namespace eez {

extern TestResult g_masterTestResult;
extern uint8_t g_numMasterErrors;
static const size_t MASTER_ERROR_MESSAGE_SIZE = 100;
extern char g_masterErrorMessage[MASTER_ERROR_MESSAGE_SIZE];

#if defined(EEZ_PLATFORM_SIMULATOR)
char *getConfFilePath(const char *file_name);
#endif

void generateError(int16_t error);
void generateChannelError(int16_t error, int channelIndex);

void onSdCardFileChangeHook(const char *filePath1, const char *filePath2 = nullptr);

/// PSU firmware.
namespace psu {

struct Channel;

struct PsuModule : public Module {
public:
    TestResult getTestResult() override;

    void setEnabled(bool value) override;

    int getSlotSettingsPageId() override;

    void resetPowerChannelProfileToDefaults(int channelIndex, uint8_t *buffer) override;
    void getPowerChannelProfileParameters(int channelIndex, uint8_t *buffer) override;
    void setPowerChannelProfileParameters(int channelIndex, uint8_t *buffer, bool mismatch, int recallOptions, int &numTrackingChannels) override;
    bool writePowerChannelProfileProperties(profile::WriteContext &ctx, const uint8_t *buffer) override;
    bool readPowerChannelProfileProperties(profile::ReadContext &ctx, uint8_t *buffer) override;
    bool getProfileOutputEnable(uint8_t *buffer) override;
    float getProfileUSet(uint8_t *buffer) override;
    float getProfileISet(uint8_t *buffer) override;
    bool testAutoRecallValuesMatch(uint8_t *bufferRecall, uint8_t *bufferDefault) override;

    eez_err_t getLabel(const char *&label) override;
    eez_err_t setLabel(const char *label, int length = -1) override;
    eez_err_t getColor(uint8_t &color) override;
    eez_err_t setColor(uint8_t color) override;

    size_t getChannelLabelMaxLength(int subchannelIndex) override;
    const char *getChannelLabel(int subchannelIndex) override;
    const char *getDefaultChannelLabel(int subchannelIndex) override;

    eez_err_t getChannelLabel(int subchannelIndex, const char *&label) override;
    eez_err_t setChannelLabel(int subchannelIndex, const char *label, int length) override;

    uint8_t getChannelColor(int subchannelIndex) override;

    eez_err_t getChannelColor(int subchannelIndex, uint8_t &color) override;
    eez_err_t setChannelColor(int subchannelIndex, uint8_t color) override;

    bool getCalibrationConfiguration(int subchannelIndex, CalibrationConfiguration &calConf, int *err) override;
    bool setCalibrationConfiguration(int subchannelIndex, const CalibrationConfiguration &calConf, int *err) override;
    bool getCalibrationRemark(int subchannelIndex, const char *&calibrationRemark, int *err) override;
    bool getCalibrationDate(int subchannelIndex, uint32_t &calibrationDate, int *err) override;

    int getNumDlogResources(int subchannelIndex) override;
	DlogResourceType getDlogResourceType(int subchannelIndex, int resourceIndex) override;
	const char *getDlogResourceLabel(int subchannelIndex, int resourceIndex) override;

    int getNumFunctionGeneratorResources(int subchannelIndex) override;
    FunctionGeneratorResourceType getFunctionGeneratorResourceType(int subchannelIndex, int resourceIndex) override;
    bool getFunctionGeneratorResourceTriggerMode(int subchannelIndex, int resourceIndex, TriggerMode &triggerMode, int *err) override;
    bool setFunctionGeneratorResourceTriggerMode(int subchannelIndex, int resourceIndex, TriggerMode triggerMode, int *err) override;
    const char *getFunctionGeneratorResourceLabel(int subchannelIndex, int resourceIndex) override;
	virtual void getFunctionGeneratorAmplitudeInfo(int subchannelIndex, int resourceIndex, FunctionGeneratorResourceType resourceType, float &min, float &max, StepValues *stepValues) override;
	virtual void getFunctionGeneratorFrequencyInfo(int subchannelIndex, int resourceIndex, float &min, float &max, StepValues *stepValues) override;
};

/// Channel binary flags stored in profile.
struct PowerChannelProfileChannelFlags {
    unsigned output_enabled : 1;
    unsigned sense_enabled : 1;
    unsigned u_state : 1;
    unsigned i_state : 1;
    unsigned p_state : 1;
    unsigned rprog_enabled : 1;
    unsigned u_triggerMode : 2;
    unsigned i_triggerMode : 2;
    unsigned currentRangeSelectionMode : 2;
    unsigned autoSelectCurrentRange : 1;
    unsigned triggerOutputState : 1;
    unsigned triggerOnListStop : 3;
    unsigned u_type : 1;
    unsigned dprogState : 2;
    unsigned trackingEnabled : 1;
};

struct PowerChannelProfileParameters {
    PowerChannelProfileChannelFlags flags;
    float u_set;
    float u_step;
    float u_limit;
    float u_delay;
    float u_level;
    float i_set;
    float i_step;
    float i_limit;
    float i_delay;
    float p_limit;
    float p_delay;
    float p_level;
    float ytViewRate;
    float u_triggerValue;
    float i_triggerValue;
    uint16_t listCount;
    float u_rampDuration;
    float i_rampDuration;
    float outputDelayDuration;
    char label[Channel::CHANNEL_LABEL_MAX_LENGTH + 1];
    uint8_t color;
	DisplayValue displayValues[2];
#ifdef EEZ_PLATFORM_SIMULATOR
    bool load_enabled;
    float load;
    float voltProgExt;
#endif
};

void init();

void onThreadMessage(uint8_t type, uint32_t param);

bool measureAllAdcValuesOnChannel(int channelIndex);

void initChannels();
bool testChannels();

bool powerUp();
void powerDown();
void powerDownOnlyPowerChannels();
void powerDownChannels(bool onlyPowerChannels = false);
bool isPowerUp();
void changePowerState(bool up);
void powerDownBySensor();

bool psuReset();

bool autoRecall(int recallOptions = 0);

void onProtectionTripped();

void tick();

void setQuesBits(int bit_mask, bool on);
void setOperBits(int bit_mask, bool on);

bool isMaxCurrentLimited();
MaxCurrentLimitCause getMaxCurrentLimitCause();
void limitMaxCurrent(MaxCurrentLimitCause cause);
void unlimitMaxCurrent();

extern volatile bool g_insideInterruptHandler;

enum RLState { RL_STATE_LOCAL = 0, RL_STATE_REMOTE = 1, RL_STATE_RW_LOCK = 2 };

extern RLState g_rlState;

extern bool g_rprogAlarm;

extern void (*g_diagCallback)();

#if defined(EEZ_PLATFORM_SIMULATOR)
namespace simulator {

void init();
void tick();

void setTemperature(int sensor, float value);
float getTemperature(int sensor);

bool getPwrgood(int pin);
void setPwrgood(int pin, bool on);

bool getRPol(int pin);
void setRPol(int pin, bool on);

bool getCV(int pin);
void setCV(int pin, bool on);

bool getCC(int pin);
void setCC(int pin, bool on);

void exit();

} // namespace simulator
#endif

} // namespace psu
} // namespace eez
