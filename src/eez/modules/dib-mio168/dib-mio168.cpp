/*
 * EEZ DIB MIO168
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

#include <new>

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <memory.h>

#if defined(EEZ_PLATFORM_STM32)
#include <spi.h>
#include <eez/platform/stm32/spi.h>
#include <ff_gen_drv.h>
#include <usbd_def.h>
#endif

#include <eez/debug.h>
#include <eez/firmware.h>
#include <eez/index.h>
#include <eez/hmi.h>

#include <eez/fs_driver.h>

#include <eez/gui/gui.h>
#include <eez/modules/psu/psu.h>
#include "eez/modules/psu/profile.h"
#include "eez/modules/psu/calibration.h"
#include <eez/modules/psu/event_queue.h>
#include <eez/modules/psu/datetime.h>
#include <eez/modules/psu/dlog_record.h>
#include <eez/modules/psu/gui/psu.h>
#include "eez/modules/psu/gui/keypad.h"
#include "eez/modules/psu/gui/labels_and_colors.h"
#include "eez/modules/psu/gui/edit_mode.h"
#include "eez/modules/psu/gui/animations.h"
#include <eez/modules/bp3c/comm.h>
#include <eez/modules/bp3c/flash_slave.h>

#include "./dib-mio168.h"

#include <scpi/scpi.h>

using namespace eez::psu;
using namespace eez::psu::gui;
using namespace eez::gui;

#if defined(EEZ_PLATFORM_STM32)
extern "C" DWORD get_fattime(void);
#endif

namespace eez {
namespace dib_mio168 {

enum Mio168HighPriorityThreadMessage {
    PSU_MESSAGE_DIN_CONFIGURE = PSU_MESSAGE_MODULE_SPECIFIC,
    PSU_MESSAGE_AIN_CONFIGURE,
    PSU_MESSAGE_AOUT_DAC7760_CONFIGURE,
    PSU_MESSAGE_DISK_DRIVE_OPERATION,
};

static const uint16_t MODULE_REVISION_R1B2  = 0x0102;

static const int DIN_SUBCHANNEL_INDEX = 0;
static const int DOUT_SUBCHANNEL_INDEX = 1;
static const int AIN_1_SUBCHANNEL_INDEX = 2;
static const int AIN_2_SUBCHANNEL_INDEX = 3;
static const int AIN_3_SUBCHANNEL_INDEX = 4;
static const int AIN_4_SUBCHANNEL_INDEX = 5;
static const int AOUT_1_SUBCHANNEL_INDEX = 6;
static const int AOUT_2_SUBCHANNEL_INDEX = 7;
static const int AOUT_3_SUBCHANNEL_INDEX = 8;
static const int AOUT_4_SUBCHANNEL_INDEX = 9;
static const int PWM_1_SUBCHANNEL_INDEX = 10;
static const int PWM_2_SUBCHANNEL_INDEX = 11;

static float AIN_VOLTAGE_RESOLUTION = 0.0001f;
static float AIN_CURRENT_RESOLUTION = 0.00001f;

static float AOUT_DAC7760_ENCODER_STEP_VALUES[] = { 0.01f, 0.1f, 0.2f, 0.5f };
static float AOUT_DAC7760_AMPER_ENCODER_STEP_VALUES[] = { 0.001f, 0.005f, 0.01f, 0.05f };

static float AOUT_DAC7760_ENCODER_STEP_VALUES_CAL[] = { 0.001f, 0.01f, 0.02f, 0.05f };
static float AOUT_DAC7760_AMPER_ENCODER_STEP_VALUES_CAL[] = { 0.0001f, 0.0005f, 0.001f, 0.005f };

static float AOUT_DAC7563_MIN = -10.0f;
static float AOUT_DAC7563_MAX = 10.0f;
static float AOUT_DAC7563_ENCODER_STEP_VALUES[] = { 0.01f, 0.1f, 0.2f, 0.5f };
static float AOUT_DAC7563_ENCODER_STEP_VALUES_CAL[] = { 0.001f, 0.01f, 0.02f, 0.05f };

static float PWM_MIN_FREQUENCY = 0.1f;
static float PWM_MAX_FREQUENCY = 1000000.0f;

static const size_t CHANNEL_LABEL_MAX_LENGTH = 5;

static const uint32_t REFRESH_TIME_MS = 250;
static const uint32_t TIMEOUT_TIME_MS = 350;
static const uint32_t TIMEOUT_UNTIL_OUT_OF_SYNC_MS = 10000;

////////////////////////////////////////////////////////////////////////////////

enum Command {
	COMMAND_NONE,

    COMMAND_GET_INFO,
    COMMAND_GET_STATE,
    COMMAND_SET_PARAMS,

    COMMAND_DLOG_RECORDING_START,
    COMMAND_DLOG_RECORDING_STOP,

    COMMAND_DISK_DRIVE_INITIALIZE,
    COMMAND_DISK_DRIVE_STATUS,
    COMMAND_DISK_DRIVE_READ,
    COMMAND_DISK_DRIVE_WRITE,
    COMMAND_DISK_DRIVE_IOCTL
};

#define GET_STATE_COMMAND_FLAG_SD_CARD_PRESENT (1 << 0)

#define DLOG_STATE_IDLE 0
#define DLOG_STATE_EXECUTING 1
#define DLOG_STATE_FINISH_RESULT_OK 2
#define DLOG_STATE_FINISH_RESULT_BUFFER_OVERFLOW 3
#define DLOG_STATE_FINISH_RESULT_MASS_STORAGE_ERROR 4

struct DlogState {
    uint8_t state; // DLOG_STATE_...
    uint32_t fileLength;
    uint32_t numSamples;
};

struct SetParams {
	uint8_t dinRanges;
	uint8_t dinSpeeds;

	uint8_t doutStates;

	struct {
		uint8_t mode; // enum SourceMode
		uint8_t range;
	} ain[4];

	struct {
		uint8_t outputEnabled;
		uint8_t outputRange;
		float outputValue;
	} aout_dac7760[2];

	struct {
		float voltage;
	} aout_dac7563[2];

	struct {
		float freq;
		float duty;
	} pwm[2];
};

struct DlogRecordingStart {
    float period;
    float duration;
    uint32_t resources;
    char filePath[MAX_PATH_LENGTH + 1];
    char dinLabels[8 * (CHANNEL_LABEL_MAX_LENGTH + 1)];
    char doutLabels[8 * (CHANNEL_LABEL_MAX_LENGTH + 1)];
    char ainLabels[4 * (CHANNEL_LABEL_MAX_LENGTH + 1)];
};

#define DISK_DRIVER_IOCTL_BUFFER_MAX_SIZE 4

struct Request {
    uint8_t command;

    union {
        struct {
            uint32_t fatTime;
        } getState;

		SetParams setParams;

        DlogRecordingStart dlogRecordingStart;

        struct {
            uint32_t sector;
        } diskDriveRead;

        struct {
            uint32_t sector;
            uint8_t buffer[512];
        } diskDriveWrite;

        struct {
            uint8_t cmd;
            uint8_t buffer[DISK_DRIVER_IOCTL_BUFFER_MAX_SIZE];
        } diskDriveIoctl;
    };
};

struct Response {
	uint8_t command;

    union {
        struct {
            uint8_t firmwareMajorVersion;
            uint8_t firmwareMinorVersion;
            uint32_t idw0;
            uint32_t idw1;
            uint32_t idw2;
            uint8_t afeVersion;
        } getInfo;

        struct {
            uint8_t flags; // GET_STATE_COMMAND_FLAG_...
            uint8_t dinStates;
            float ainValues[4];
            DlogState dlogState;
        } getState;

        struct {
            uint8_t result; // 1 - success, 0 - failure
        } setParams;

        struct {
            uint8_t result;
        } diskDriveInitialize;

        struct {
            uint8_t result;
        } diskDriveStatus;

        struct {
            uint8_t result;
            uint8_t buffer[512];
        } diskDriveRead;

        struct {
            uint8_t result;
        } diskDriveWrite;

        struct {
            uint8_t result;
            uint8_t buffer[DISK_DRIVER_IOCTL_BUFFER_MAX_SIZE];
        } diskDriveIoctl;
	};
};

////////////////////////////////////////////////////////////////////////////////

static const size_t MIO_CALIBRATION_REMARK_MAX_LENGTH = 28;

struct CalConf {
    static const int VERSION = 1;

    BlockHeader header;

    struct {
        uint16_t calState: 1;   // is channel calibrated?
        uint16_t calEnabled: 1; // is channel calibration enabled?
        uint16_t reserved: 14;
    } state;

    struct {
        float uncalValue;
        float calValue;
    } points[2];

    /// Date when calibration is saved.
    uint32_t calibrationDate;

    /// Remark about calibration set by user.
    char calibrationRemark[MIO_CALIBRATION_REMARK_MAX_LENGTH + 1];

    void clear() {
        memset(this, 0, sizeof(CalConf));
    }

    float toCalibratedValue(float value) {
        return state.calEnabled ? remap(value, points[0].calValue, points[0].uncalValue, points[1].calValue, points[1].uncalValue) : value;
    }
};

////////////////////////////////////////////////////////////////////////////////

struct DinChannel {
    uint8_t m_pinStates = 0;

    // Valid for all 8 digital inputs
    // 0 - LOW, 1 - HIGH
    uint8_t m_pinRanges = 0;

    // Valid only for first two digital inputs.
    // 0 - FAST, 1 - SLOW
    uint8_t m_pinSpeeds = 0;

    char m_pinLabels[8 * (CHANNEL_LABEL_MAX_LENGTH + 1)];

    struct ProfileParameters {
        uint8_t pinRanges;
        uint8_t pinSpeeds;
        char pinLabels[8 * (CHANNEL_LABEL_MAX_LENGTH + 1)];
    };

    void resetProfileToDefaults(ProfileParameters &parameters) {
        parameters.pinRanges = 0;
        parameters.pinSpeeds = 0;
        memset(parameters.pinLabels, 0, sizeof(m_pinLabels));
    }

    void getProfileParameters(ProfileParameters &parameters) {
        parameters.pinRanges = m_pinRanges;
        parameters.pinSpeeds = m_pinSpeeds;
        memcpy(parameters.pinLabels, m_pinLabels, sizeof(m_pinLabels));
    }

    void setProfileParameters(ProfileParameters &parameters) {
        m_pinRanges = parameters.pinRanges;
        m_pinSpeeds = parameters.pinSpeeds;
        memcpy(m_pinLabels, parameters.pinLabels, sizeof(m_pinLabels));
    }

    bool writeProfileProperties(psu::profile::WriteContext &ctx, ProfileParameters &parameters) {
        WRITE_PROPERTY("din_pinRanges", parameters.pinRanges);
        WRITE_PROPERTY("din_pinSpeeds", parameters.pinSpeeds);
        for (int i = 0; i < 8; i++) {
            char propName[32];
            sprintf(propName, "din_pin%dLabel", i);
            WRITE_PROPERTY(propName, parameters.pinLabels + i * (CHANNEL_LABEL_MAX_LENGTH + 1));
        }
        return true;
    }

    bool readProfileProperties(psu::profile::ReadContext &ctx, ProfileParameters &parameters) {
        READ_PROPERTY("din_pinRanges", parameters.pinRanges);
        READ_PROPERTY("din_pinSpeeds", parameters.pinSpeeds);
        for (int i = 0; i < 8; i++) {
            char propName[32];
            sprintf(propName, "din_pin%dLabel", i);
            READ_STRING_PROPERTY(propName, parameters.pinLabels + i * (CHANNEL_LABEL_MAX_LENGTH + 1), CHANNEL_LABEL_MAX_LENGTH);
        }
        return false;
    }

    void resetConfiguration() {
        m_pinRanges = 0;
        m_pinSpeeds = 0;
        memset(&m_pinLabels, 0, sizeof(m_pinLabels));
    }

    uint8_t getDigitalInputData() {
        return m_pinStates;
    }

#ifdef EEZ_PLATFORM_SIMULATOR
    void setDigitalInputData(uint8_t data) {
        m_pinStates = data;
    }
#endif

    int getPinState(int pin) {
        return m_pinStates & (1 << pin) ? 1 : 0;
    }

    int getPinRange(int pin) {
        return m_pinRanges & (1 << pin) ? 1 : 0;
    }
    
    int getPinSpeed(int pin) {
        return m_pinSpeeds & (1 << pin) ? 1 : 0;
    }
};

struct DoutChannel {
    uint8_t m_pinStates = 0;

    char m_pinLabels[8 * (CHANNEL_LABEL_MAX_LENGTH + 1)];
    
    struct ProfileParameters {
        uint8_t pinStates;
        char pinLabels[8 * (CHANNEL_LABEL_MAX_LENGTH + 1)];
    };

    void resetProfileToDefaults(ProfileParameters &parameters) {
        parameters.pinStates = 0;
        memset(parameters.pinLabels, 0, sizeof(m_pinLabels));
    }

    void getProfileParameters(ProfileParameters &parameters) {
        parameters.pinStates = m_pinStates;
        memcpy(parameters.pinLabels, m_pinLabels, sizeof(m_pinLabels));
    }

    void setProfileParameters(ProfileParameters &parameters) {
        m_pinStates = parameters.pinStates;
        memcpy(m_pinLabels, parameters.pinLabels, sizeof(m_pinLabels));
    }

    bool writeProfileProperties(psu::profile::WriteContext &ctx, ProfileParameters &parameters) {
        WRITE_PROPERTY("dout_pinStates", parameters.pinStates);
        for (int i = 0; i < 8; i++) {
            char propName[32];
            sprintf(propName, "dout_pin%dLabel", i);
            WRITE_PROPERTY(propName, parameters.pinLabels + i * (CHANNEL_LABEL_MAX_LENGTH + 1));
        }
        return true;
    }

    bool readProfileProperties(psu::profile::ReadContext &ctx, ProfileParameters &parameters) {
        READ_PROPERTY("dout_pinStates", parameters.pinStates);
        for (int i = 0; i < 8; i++) {
            char propName[32];
            sprintf(propName, "dout_pin%dLabel", i);
            READ_STRING_PROPERTY(propName, parameters.pinLabels + i * (CHANNEL_LABEL_MAX_LENGTH + 1), CHANNEL_LABEL_MAX_LENGTH);
        }
        return false;
    }

   void resetConfiguration() {
        m_pinStates = 0;
        memset(&m_pinLabels, 0, sizeof(m_pinLabels));
    }

    uint8_t getDigitalOutputData() {
        return m_pinStates;
    }

    void setDigitalOutputData(uint8_t data) {
        m_pinStates = data;
    }

    int getPinState(int pin) {
        return m_pinStates & (1 << pin) ? 1 : 0;
    }

    void setPinState(int pin, int state) {
        if (state) {
            m_pinStates |= 1 << pin;
        } else {
            m_pinStates &= ~(1 << pin);
        }
    }
};

struct AinChannel {
    float m_value = 0;
    uint8_t m_mode = 1;
    uint8_t m_range = 0;
    char m_label[CHANNEL_LABEL_MAX_LENGTH + 1];

    struct ProfileParameters {
        uint8_t mode;
        uint8_t range;
        char label[CHANNEL_LABEL_MAX_LENGTH + 1];
    };

    void resetProfileToDefaults(ProfileParameters &parameters) {
        parameters.mode = 1;
        parameters.range = 0;
        *parameters.label = 0;
    }

    void getProfileParameters(ProfileParameters &parameters) {
        parameters.mode = m_mode;
        parameters.range = m_range;
        memcpy(parameters.label, m_label, sizeof(m_label));
    }

    void setProfileParameters(ProfileParameters &parameters) {
        m_mode = parameters.mode;
        m_range = parameters.range;
        memcpy(m_label, parameters.label, sizeof(m_label));
    }

    bool writeProfileProperties(psu::profile::WriteContext &ctx, int i, ProfileParameters &parameters) {
        char propName[32];

        sprintf(propName, "ain_%d_mode", i+1);
        WRITE_PROPERTY(propName, parameters.mode);

        sprintf(propName, "ain_%d_range", i+1);
        WRITE_PROPERTY(propName, parameters.range);

        sprintf(propName, "ain_%d_label", i+1);
        WRITE_PROPERTY(propName, parameters.label);

        return true;
    }

    bool readProfileProperties(psu::profile::ReadContext &ctx, int i, ProfileParameters &parameters) {
        char propName[32];

        sprintf(propName, "ain_%d_mode", i+1);
        READ_PROPERTY(propName, parameters.mode);

        sprintf(propName, "ain_%d_range", i+1);
        READ_PROPERTY(propName, parameters.range);

        sprintf(propName, "ain_%d_label", i+1);
        READ_STRING_PROPERTY(propName, parameters.label, CHANNEL_LABEL_MAX_LENGTH);

        return false;
    }

    void resetConfiguration() {
        m_mode = 1;
        m_range = 0;
        *m_label = 0;
    }

    float getResolution() {
        return m_mode == MEASURE_MODE_VOLTAGE ? AIN_VOLTAGE_RESOLUTION : m_mode == MEASURE_MODE_CURRENT ? AIN_CURRENT_RESOLUTION : 1.0f;
    }

    void addValue(float value) {
        m_value = value;
    }

	static uint8_t getRangeMaxValue(int subchannelIndex, uint8_t mode) {
		if (subchannelIndex == AIN_1_SUBCHANNEL_INDEX || subchannelIndex == AIN_2_SUBCHANNEL_INDEX) {
			return mode == SOURCE_MODE_VOLTAGE ? 2 : 0;
		} else {
			return mode == SOURCE_MODE_VOLTAGE ? 1 : mode == SOURCE_MODE_CURRENT ? 2 : 0;
		}
	}
};

struct AoutDac7760Channel {
    bool m_outputEnabled = false;
    uint8_t m_mode = SOURCE_MODE_VOLTAGE;

    // 5: 4 mA to 20 mA
    // 6: 0 mA to 20 mA
    // 7: 0 mA to 24 mA
    uint8_t m_currentRange = 5;

    // 0: 0 V to +5 V
    // 1: 0 V to +10 V
    // 2: +/- 5 V
    // 3: +/- 10 V
    uint8_t m_voltageRange = 0;

    float m_currentValue = 0;
    float m_voltageValue = 0;

    CalConf calConf[7];
    bool ongoingCal = false;

    char m_label[CHANNEL_LABEL_MAX_LENGTH + 1];

    AoutDac7760Channel() {
        memset(calConf, 0, sizeof(CalConf));
    }

    struct ProfileParameters {
        bool outputEnabled;
        uint8_t mode;
        uint8_t currentRange;
        uint8_t voltageRange;
        float currentValue;
        float voltageValue;
        char label[CHANNEL_LABEL_MAX_LENGTH + 1];
    };

    void resetProfileToDefaults(ProfileParameters &parameters) {
        parameters.outputEnabled = false;
        parameters.mode = SOURCE_MODE_VOLTAGE;
        parameters.currentRange = 5;
        parameters.voltageRange = 0;
        parameters.currentValue = 0;
        parameters.voltageValue = 0;
        *parameters.label = 0;
    }

    void getProfileParameters(ProfileParameters &parameters) {
        parameters.outputEnabled = m_outputEnabled;
        parameters.mode = m_mode;
        parameters.currentRange = m_currentRange;
        parameters.voltageRange = m_voltageRange;
        parameters.currentValue = m_currentValue;
        parameters.voltageValue = m_voltageValue;
        memcpy(parameters.label, m_label, sizeof(m_label));
    }

    void setProfileParameters(ProfileParameters &parameters) {
        m_outputEnabled = parameters.outputEnabled;
        m_mode = parameters.mode;
        m_currentRange = parameters.currentRange;
        m_voltageRange = parameters.voltageRange;
        m_currentValue = parameters.currentValue;
        m_voltageValue = parameters.voltageValue;
        memcpy(m_label, parameters.label, sizeof(m_label));
    }

    bool writeProfileProperties(psu::profile::WriteContext &ctx, int i, ProfileParameters &parameters) {
        char propName[32];

        sprintf(propName, "aout_dac7760_%d_outputEnabled", i+1);
        WRITE_PROPERTY(propName, parameters.outputEnabled);

        sprintf(propName, "aout_dac7760_%d_mode", i+1);
        WRITE_PROPERTY(propName, parameters.mode);

        sprintf(propName, "aout_dac7760_%d_currentRange", i+1);
        WRITE_PROPERTY(propName, parameters.currentRange);

        sprintf(propName, "aout_dac7760_%d_voltageRange", i+1);
        WRITE_PROPERTY(propName, parameters.voltageRange);

        sprintf(propName, "aout_dac7760_%d_currentValue", i+1);
        WRITE_PROPERTY(propName, parameters.currentValue);

        sprintf(propName, "aout_dac7760_%d_voltageValue", i+1);
        WRITE_PROPERTY(propName, parameters.voltageValue);

        sprintf(propName, "aout_dac7760_%d_label", i+1);
        WRITE_PROPERTY(propName, parameters.label);

        return true;
    }

    bool readProfileProperties(psu::profile::ReadContext &ctx, int i, ProfileParameters &parameters) {
        char propName[32];

        sprintf(propName, "aout_dac7760_%d_outputEnabled", i+1);
        READ_PROPERTY(propName, parameters.outputEnabled);

        sprintf(propName, "aout_dac7760_%d_mode", i+1);
        READ_PROPERTY(propName, parameters.mode);

        sprintf(propName, "aout_dac7760_%d_currentRange", i+1);
        READ_PROPERTY(propName, parameters.currentRange);

        sprintf(propName, "aout_dac7760_%d_voltageRange", i+1);
        READ_PROPERTY(propName, parameters.voltageRange);

        sprintf(propName, "aout_dac7760_%d_currentValue", i+1);
        READ_PROPERTY(propName, parameters.currentValue);

        sprintf(propName, "aout_dac7760_%d_voltageValue", i+1);
        READ_PROPERTY(propName, parameters.voltageValue);

        sprintf(propName, "aout_dac7760_%d_label", i+1);
        READ_STRING_PROPERTY(propName, parameters.label, CHANNEL_LABEL_MAX_LENGTH);

        return false;
    }

    void resetConfiguration() {
        m_outputEnabled = false;
        m_mode = SOURCE_MODE_VOLTAGE;
        m_currentRange = 5;
        m_voltageRange = 0;
        m_currentValue = 0;
        m_voltageValue = 0;
        *m_label = 0;
    }

    SourceMode getMode() {
        return (SourceMode)m_mode;
    }

    void setMode(SourceMode mode) {
        m_mode = mode;
    }

    int8_t getCurrentRange() {
        return m_currentRange;
    }
    
    void setCurrentRange(int8_t range) {
        m_currentRange = range;
    }

    int8_t getVoltageRange() {
        return m_voltageRange;
    }
    
    void setVoltageRange(int8_t range) {
        m_voltageRange = range;
    }

    float getValue() {
        return getMode() == SOURCE_MODE_VOLTAGE ? m_voltageValue : m_currentValue;
    }

    int getCalConfIndex() {
        if (getMode() == SOURCE_MODE_VOLTAGE) {
            return getVoltageRange();
        } else {
            return getCurrentRange() - 1;
        }
    }

    CalConf *getCalConf() {
        return &calConf[getCalConfIndex()];
    }

    float getCalibratedValue() {
        return getCalConf()->toCalibratedValue(getValue());
    }

    Unit getUnit() {
        return getMode() == SOURCE_MODE_VOLTAGE ? UNIT_VOLT : UNIT_AMPER;
    }

    void setValue(float value) {
        if (getMode() == SOURCE_MODE_VOLTAGE) {
            m_voltageValue = value;
        } else {
            m_currentValue = value;
        }
    }

    float getVoltageMinValue() {
        uint8_t voltageRange = getVoltageRange();
        if (voltageRange == 0) {
            return 0;
        } 
        if (voltageRange == 1) {
            return 0;
        } 
        if (voltageRange == 2) {
            return -5.0f;
        } 
        return -10.0f;
    }

    float getCurrentMinValue() {
        if (m_currentRange == 5) {
            return 0.004f;
        }
        if (m_currentRange == 6) {
            return 0;
        }
        return 0;
    }

    float getMinValue() {
        if (getMode() == SOURCE_MODE_VOLTAGE) {
            return getVoltageMinValue();
        } else {
            return getCurrentMinValue();
        }
    }

    float getVoltageMaxValue() {
        uint8_t voltageRange = getVoltageRange();
        if (voltageRange == 0) {
            return 5.0f;
        } 
        if (voltageRange == 1) {
            return 10.0f;
        } 
        if (voltageRange == 2) {
            return 5.0f;
        } 
        return 10.0f;
    }

    float getCurrentMaxValue() {
        if (m_currentRange == 5) {
            return 0.02f;
        }
        if (m_currentRange == 6) {
            return 0.02f;
        }
        return 0.024f;
    }

    float getMaxValue() {
        if (getMode() == SOURCE_MODE_VOLTAGE) {
            return getVoltageMaxValue();
        } else {
            return getCurrentMaxValue();
        }
    }

    float getVoltageResolution() {
        return ongoingCal ? 0.0001f : 0.001f;
    }

    float getCurrentResolution() {
        return ongoingCal ? 0.0001f : 0.001f;
    }

    float getResolution() {
        if (getMode() == SOURCE_MODE_VOLTAGE) {
            return getVoltageResolution();
        } else {
            return getCurrentResolution();
        }
    }

    void getStepValues(StepValues *stepValues) {
        if (ongoingCal) {
            if (getMode() == SOURCE_MODE_VOLTAGE) {
                stepValues->values = AOUT_DAC7760_ENCODER_STEP_VALUES_CAL;
                stepValues->count = sizeof(AOUT_DAC7760_ENCODER_STEP_VALUES_CAL) / sizeof(float);
                stepValues->unit = UNIT_VOLT;
            } else {
                stepValues->values = AOUT_DAC7760_AMPER_ENCODER_STEP_VALUES_CAL;
                stepValues->count = sizeof(AOUT_DAC7760_AMPER_ENCODER_STEP_VALUES_CAL) / sizeof(float);
                stepValues->unit = UNIT_AMPER;
            }
        } else {
            if (getMode() == SOURCE_MODE_VOLTAGE) {
                stepValues->values = AOUT_DAC7760_ENCODER_STEP_VALUES;
                stepValues->count = sizeof(AOUT_DAC7760_ENCODER_STEP_VALUES) / sizeof(float);
                stepValues->unit = UNIT_VOLT;
            } else {
                stepValues->values = AOUT_DAC7760_AMPER_ENCODER_STEP_VALUES;
                stepValues->count = sizeof(AOUT_DAC7760_AMPER_ENCODER_STEP_VALUES) / sizeof(float);
                stepValues->unit = UNIT_AMPER;
            }
        }

        stepValues->encoderSettings.accelerationEnabled = true;
        stepValues->encoderSettings.range = getMaxValue() - getMinValue();
        stepValues->encoderSettings.step = getResolution();
    }

    void getDefaultCalibrationPoints(unsigned int &numPoints, float *&points) {
        static float AOUT_POINTS[7][2] = {
            { 0.1f, 4.9f },
            { 0.1f, 9.9f },
            { -4.9f, 4.9f },
            { -9.9f, 9.9f },
            { 5E-3f, 19E-3f },
            { 1E-3f, 19E-3f },
            { 1E-3f, 23E-3f }
        };
        numPoints = 2;
        points = &AOUT_POINTS[getCalConfIndex()][0];
    }
};

struct AoutDac7563Channel {
    float m_value = 0;

    bool ongoingCal = false;
    CalConf calConf;

    char m_label[CHANNEL_LABEL_MAX_LENGTH + 1];

    AoutDac7563Channel() {
        memset(&calConf, 0, sizeof(CalConf));
    }

    struct ProfileParameters {
        float value;
        char label[CHANNEL_LABEL_MAX_LENGTH + 1];
    };

    float getCalibratedValue() {
        return calConf.toCalibratedValue(m_value);
    }

    void resetProfileToDefaults(ProfileParameters &parameters) {
        parameters.value = 0;
        *parameters.label = 0;
    }

    void getProfileParameters(ProfileParameters &parameters) {
        parameters.value = m_value;
        memcpy(parameters.label, m_label, sizeof(m_label));
    }

    void setProfileParameters(ProfileParameters &parameters) {
        m_value = parameters.value;
        memcpy(m_label, parameters.label, sizeof(m_label));
    }

    bool writeProfileProperties(psu::profile::WriteContext &ctx, int i, ProfileParameters &parameters) {
        char propName[32];

        sprintf(propName, "aout_dac7563_%d_value", i+1);
        WRITE_PROPERTY(propName, parameters.value);

        sprintf(propName, "aout_dac7563_%d_label", i+1);
        WRITE_PROPERTY(propName, parameters.label);

        return true;
    }

    bool readProfileProperties(psu::profile::ReadContext &ctx, int i, ProfileParameters &parameters) {
        char propName[32];

        sprintf(propName, "aout_dac7563_%d_value", i+1);
        READ_PROPERTY(propName, parameters.value);

        sprintf(propName, "aout_dac7563_%d_label", i+1);
        READ_STRING_PROPERTY(propName, parameters.label, CHANNEL_LABEL_MAX_LENGTH);

        return false;
    }

    void resetConfiguration() {
        m_value = 0;
        *m_label = 0;
    }

    float getResolution() {
        return ongoingCal ? 0.001f : 0.01f;
    }

    void getStepValues(StepValues *stepValues) {
        if (ongoingCal) {
            stepValues->values = AOUT_DAC7563_ENCODER_STEP_VALUES_CAL;
            stepValues->count = sizeof(AOUT_DAC7563_ENCODER_STEP_VALUES_CAL) / sizeof(float);
            stepValues->unit = UNIT_VOLT;
        } else {
            stepValues->values = AOUT_DAC7563_ENCODER_STEP_VALUES;
            stepValues->count = sizeof(AOUT_DAC7563_ENCODER_STEP_VALUES) / sizeof(float);
            stepValues->unit = UNIT_VOLT;
        }

        stepValues->encoderSettings.accelerationEnabled = true;
        stepValues->encoderSettings.range = AOUT_DAC7563_MAX - AOUT_DAC7563_MIN;
        stepValues->encoderSettings.step = getResolution();
    }

    void getDefaultCalibrationPoints(unsigned int &numPoints, float *&points) {
        static float AOUT_POINTS[] = { -9.9f, 9.9f };
        numPoints = 2;
        points = AOUT_POINTS;
    }
};

struct PwmChannel {
    float m_freq = 0;
    float m_duty = 0;

    char m_label[CHANNEL_LABEL_MAX_LENGTH + 1];

    struct ProfileParameters {
        float freq;
        float duty;
        char label[CHANNEL_LABEL_MAX_LENGTH + 1];
    };
    
    void resetProfileToDefaults(ProfileParameters &parameters) {
        parameters.freq = 0;
        parameters.duty = 0;
        *parameters.label = 0;
    }

    void getProfileParameters(ProfileParameters &parameters) {
        parameters.freq = m_freq;
        parameters.duty = m_duty;
        memcpy(parameters.label, m_label, sizeof(m_label));
    }

    void setProfileParameters(ProfileParameters &parameters) {
        m_freq = parameters.freq;
        m_duty = parameters.duty;
        memcpy(m_label, parameters.label, sizeof(m_label));
    }

    bool writeProfileProperties(psu::profile::WriteContext &ctx, int i, ProfileParameters &parameters) {
        char propName[32];

        sprintf(propName, "pwm_%d_freq", i+1);
        WRITE_PROPERTY(propName, parameters.freq);

        sprintf(propName, "pwm_%d_duty", i+1);
        WRITE_PROPERTY(propName, parameters.duty);

        sprintf(propName, "pwm_%d_label", i+1);
        WRITE_PROPERTY(propName, parameters.label);

        return true;
    }

    bool readProfileProperties(psu::profile::ReadContext &ctx, int i, ProfileParameters &parameters) {
        char propName[32];

        sprintf(propName, "pwm_%d_freq", i+1);
        READ_PROPERTY(propName, parameters.freq);

        sprintf(propName, "pwm_%d_duty", i+1);
        READ_PROPERTY(propName, parameters.duty);

        sprintf(propName, "pwm_%d_label", i+1);
        READ_STRING_PROPERTY(propName, parameters.label, CHANNEL_LABEL_MAX_LENGTH);

        return false;
    }

    void resetConfiguration() {
        m_freq = 0;
        m_duty = 0;
        *m_label = 0;
    }
};

////////////////////////////////////////////////////////////////////////////////

struct Mio168Module : public Module {
public:
    TestResult testResult = TEST_NONE;

    uint8_t afeVersion;

    bool synchronized = false;

    uint32_t input[(sizeof(Request) + 3) / 4 + 1];
    uint32_t output[(sizeof(Request) + 3) / 4];

    bool spiReady = false;
    bool spiDmaTransferCompleted = false;
    int spiDmaTransferStatus;

    uint32_t lastTransferTime = 0;
	SetParams lastTransferredParams;
    uint32_t operationStateTransitionTime;

	struct CommandDef {
		uint8_t command;
		void (Mio168Module::*fillRequest)(Request &request);
		void (Mio168Module::*done)(Response &response, bool isSuccess);
	};

    static const CommandDef getInfo_command;
    static const CommandDef getState_command;
    static const CommandDef setParams_command;

    static const CommandDef dlogRecordingStart_command;
    static const CommandDef dlogRecordingStop_command;

    static const CommandDef diskDriveInitialize_command;
    static const CommandDef diskDriveStatus_command;
    static const CommandDef diskDriveRead_command;
    static const CommandDef diskDriveWrite_command;
    static const CommandDef diskDriveIoctl_command;

    enum State {
        STATE_IDLE,

        STATE_WAIT_SLAVE_READY_BEFORE_REQUEST,
        STATE_WAIT_DMA_TRANSFER_COMPLETED_FOR_REQUEST,

        STATE_WAIT_SLAVE_READY_BEFORE_RESPONSE,
        STATE_WAIT_DMA_TRANSFER_COMPLETED_FOR_RESPONSE
    };

    enum Event {
        EVENT_SLAVE_READY,
        EVENT_DMA_TRANSFER_COMPLETED,
        EVENT_DMA_TRANSFER_FAILED,
        EVENT_TIMEOUT
    };

    const CommandDef *currentCommand;
    uint32_t refreshStartTime;
    State state;
    uint32_t lastStateTransitionTime;
    uint32_t lastRefreshTime;
    int retry;

	struct ExecuteDiskDriveOperationParams {
        const CommandDef *command;

        uint32_t sector;
        uint8_t* buff;
        uint8_t cmd;

        uint32_t *blockNum;
        uint16_t *blockSize;

        uint32_t result;
    };
    ExecuteDiskDriveOperationParams diskOperationParams;
    enum {
    	DISK_OPERATION_IDLE,
        DISK_OPERATION_NOT_FINISHED,
        DISK_OPERATION_SUCCESSFULLY_FINISHED,
        DISK_OPERATION_UNSUCCESSFULLY_FINISHED
    } diskOperationStatus = DISK_OPERATION_IDLE;

    DinChannel dinChannel;
    DoutChannel doutChannel;
    AinChannel ainChannels[4];
    AoutDac7760Channel aoutDac7760Channels[2];
    AoutDac7563Channel aoutDac7563Channels[2];
    PwmChannel pwmChannels[2];

    uint8_t dac7760CalibrationChannelMode;
    uint8_t dac7760CalibrationChannelCurrentRange;
    uint8_t dac7760CalibrationChannelVoltageRange;

    const CommandDef *nextDlogCommand = nullptr;
    DlogRecordingStart nextDlogRecordingStart;
    DlogRecordingStart dlogRecordingStart;

    Mio168Module() {
		assert(sizeof(Request) == sizeof(Response));

        moduleType = MODULE_TYPE_DIB_MIO168;
        moduleName = "MIO168";
        moduleBrand = "Envox";
        latestModuleRevision = MODULE_REVISION_R1B2;
        flashMethod = FLASH_METHOD_STM32_BOOTLOADER_SPI;
#if defined(EEZ_PLATFORM_STM32)        
        spiBaudRatePrescaler = SPI_BAUDRATEPRESCALER_4;
        spiCrcCalculationEnable = true;
#else
        spiBaudRatePrescaler = 0;
        spiCrcCalculationEnable = false;
#endif
        numPowerChannels = 0;
        numOtherChannels = 12;
        isResyncSupported = true;

        resetConfiguration();

        memset(input, 0, sizeof(input));
        memset(output, 0, sizeof(output));

        //dlog.period = 0;
        //dlog.duration = 0;
        //dlog.resources = 0;
        //*dlog.filePath = 0;
    }

    Module *createModule() override {
        return new Mio168Module();
    }

    TestResult getTestResult() override {
        return testResult;
    }

    void initChannels() override {
        if (!synchronized) {
            // give some time for MIO168 module to boot
        	auto t = millis();
        	if (t < 1700) {
        		osDelay(1700 - t);
        	}

			executeCommand(&getInfo_command);

			if (!g_isBooted) {
				while (state != STATE_IDLE) {
#if defined(EEZ_PLATFORM_STM32)
					if (HAL_GPIO_ReadPin(spi::IRQ_GPIO_Port[slotIndex], spi::IRQ_Pin[slotIndex]) == GPIO_PIN_RESET) {
						spiReady = true;
					}
#endif
					tick();
				}
			}

            memset(&lastTransferredParams, 0, sizeof(SetParams));
        }
    }

    ////////////////////////////////////////

    void Command_GetInfo_Done(Response &response, bool isSuccess) {
        if (isSuccess) {
            auto &data = response.getInfo;

            firmwareMajorVersion = data.firmwareMajorVersion;
            firmwareMinorVersion = data.firmwareMinorVersion;
            idw0 = data.idw0;
            idw1 = data.idw1;
            idw2 = data.idw2;
            afeVersion = data.afeVersion;

			firmwareVersionAcquired = true;

            synchronized = true;
            testResult = TEST_OK;
        } else {
            synchronized = false;
            if (g_slots[slotIndex]->firmwareInstalled) {
                event_queue::pushEvent(event_queue::EVENT_ERROR_SLOT1_SYNC_ERROR + slotIndex);
            }
            testResult = TEST_FAILED;
        }
    }

    ////////////////////////////////////////

	void Command_GetState_FillRequest(Request &request) {
#if defined(EEZ_PLATFORM_STM32)
		request.getState.fatTime = get_fattime();
#endif

#if defined(EEZ_PLATFORM_SIMULATOR)
		request.getState.fatTime = 0;
#endif
	}

    void Command_GetState_Done(Response &response, bool isSuccess) {
        if (isSuccess) {
            lastRefreshTime = refreshStartTime;

			auto &data = response.getState;

            dinChannel.m_pinStates = data.dinStates;

            for (int i = 0; i < 4; i++) {
                auto &channel = ainChannels[i];
                channel.addValue(data.ainValues[i]);
            }

            if (data.flags & GET_STATE_COMMAND_FLAG_SD_CARD_PRESENT) {
                if (!fs_driver::isDriverLinked(slotIndex)) {
                    sendMessageToLowPriorityThread(THREAD_MESSAGE_FS_DRIVER_LINK, slotIndex, 0);
                }
            } else {
                if (fs_driver::isDriverLinked(slotIndex)) {
                    sendMessageToLowPriorityThread(THREAD_MESSAGE_FS_DRIVER_UNLINK, slotIndex, 0);
                }
            }

            if (dlog_record::isExecuting() && dlog_record::getModuleLocalRecordingSlotIndex() == slotIndex) {
                dlog_record::setFileLength(data.dlogState.fileLength);
                dlog_record::setNumSamples(data.dlogState.numSamples);

				switch (data.dlogState.state) {
					case DLOG_STATE_FINISH_RESULT_OK:
						dlog_record::abort();
						break;
					case DLOG_STATE_FINISH_RESULT_BUFFER_OVERFLOW:
						dlog_record::abortAfterBufferOverflowError();
						break;
					case DLOG_STATE_FINISH_RESULT_MASS_STORAGE_ERROR:
						dlog_record::abortAfterMassStorageError();
						break;
				}
            }
        }
    }

    ////////////////////////////////////////

	void fillSetParams(SetParams &params) {
		memset(&params, 0, sizeof(SetParams));

		params.dinRanges = dinChannel.m_pinRanges;
		params.dinSpeeds = dinChannel.m_pinSpeeds;

		params.doutStates = doutChannel.m_pinStates;

		for (int i = 0; i < 4; i++) {
			auto channel = &ainChannels[i];
			params.ain[i].mode = channel->m_mode;
			params.ain[i].range = channel->m_range;
		}

		for (int i = 0; i < 2; i++) {
			auto channel = &aoutDac7760Channels[i];
			params.aout_dac7760[i].outputEnabled = channel->m_outputEnabled;
			params.aout_dac7760[i].outputRange = channel->getMode() == SOURCE_MODE_VOLTAGE ? channel->getVoltageRange() : channel->m_currentRange;
			params.aout_dac7760[i].outputValue = channel->getCalibratedValue();
		}

		for (int i = 0; i < 2; i++) {
			auto channel = &aoutDac7563Channels[i];
			params.aout_dac7563[i].voltage = channel->getCalibratedValue();
		}

		for (int i = 0; i < 2; i++) {
			auto channel = &pwmChannels[i];
			params.pwm[i].freq = channel->m_freq;
			params.pwm[i].duty = channel->m_duty;
		}
	}

    void Command_SetParams_FillRequest(Request &request) {
        fillSetParams(request.setParams);
    }

    void Command_SetParams_Done(Response &response, bool isSuccess) {
        if (isSuccess) {
            auto &data = response.setParams;
            if (data.result) {
                memcpy(&lastTransferredParams, &((Request *)output)->setParams, sizeof(SetParams));
            }
        }
    }

    ////////////////////////////////////////

    void Command_DlogRecordingStart_FillRequest(Request &request) {
        memcpy(&request.dlogRecordingStart, &dlogRecordingStart, sizeof(dlogRecordingStart));
    }

    void Command_DlogRecordingStart_Done(Response &response, bool isSuccess) {
        if (isSuccess) {
        }
    }

    ////////////////////////////////////////

    void Command_DlogRecordingStop_Done(Response &response, bool isSuccess) {
        if (isSuccess) {
        }
    }

    ////////////////////////////////////////

    void Command_DiskDriveInitialize_Done(Response &response, bool isSuccess) {
        if (isSuccess) {
            diskOperationParams.result = response.diskDriveInitialize.result;
            diskOperationStatus = DISK_OPERATION_SUCCESSFULLY_FINISHED;
        } else {
            diskOperationParams.result = 1; // STA_NOINIT, i.e. RES_ERROR
            diskOperationStatus = DISK_OPERATION_UNSUCCESSFULLY_FINISHED;
        }
#if defined(EEZ_PLATFORM_STM32)
        osThreadYield();
#endif
    }


    ////////////////////////////////////////

    void Command_DiskDriveStatus_Done(Response &response, bool isSuccess) {
        if (isSuccess) {
            diskOperationParams.result = response.diskDriveStatus.result;
            diskOperationStatus = DISK_OPERATION_SUCCESSFULLY_FINISHED;
        } else {
            diskOperationStatus = DISK_OPERATION_UNSUCCESSFULLY_FINISHED;
        }
#if defined(EEZ_PLATFORM_STM32)
        osThreadYield();
#endif
    }


    ////////////////////////////////////////

    void Command_DiskDriveRead_FillRequest(Request &request) {
        request.diskDriveRead.sector = diskOperationParams.sector;
    }

    void Command_DiskDriveRead_Done(Response &response, bool isSuccess) {
        if (isSuccess) {
            memcpy(diskOperationParams.buff, response.diskDriveRead.buffer, 512);
            diskOperationParams.result = response.diskDriveRead.result;
            diskOperationStatus = DISK_OPERATION_SUCCESSFULLY_FINISHED;
        } else {
            diskOperationStatus = DISK_OPERATION_UNSUCCESSFULLY_FINISHED;
        }
#if defined(EEZ_PLATFORM_STM32)
        osThreadYield();
#endif
    }


    ////////////////////////////////////////

    void Command_DiskDriveWrite_FillRequest(Request &request) {
        request.diskDriveWrite.sector = diskOperationParams.sector;
        memcpy(request.diskDriveWrite.buffer, diskOperationParams.buff, 512);
    }

    void Command_DiskDriveWrite_Done(Response &response, bool isSuccess) {
        if (isSuccess) {
            diskOperationParams.result = response.diskDriveWrite.result;
            diskOperationStatus = DISK_OPERATION_SUCCESSFULLY_FINISHED;
        } else {
            diskOperationStatus = DISK_OPERATION_UNSUCCESSFULLY_FINISHED;
        }
#if defined(EEZ_PLATFORM_STM32)
        osThreadYield();
#endif
    }

    ////////////////////////////////////////

    void Command_DiskDriveIoctl_FillRequest(Request &request) {
        request.diskDriveIoctl.cmd = diskOperationParams.cmd;
        memcpy(request.diskDriveIoctl.buffer, diskOperationParams.buff, DISK_DRIVER_IOCTL_BUFFER_MAX_SIZE);
    }

    void Command_DiskDriveIoctl_Done(Response &response, bool isSuccess) {
        if (isSuccess) {
            memcpy(diskOperationParams.buff, response.diskDriveIoctl.buffer, DISK_DRIVER_IOCTL_BUFFER_MAX_SIZE);
            diskOperationParams.result = response.diskDriveIoctl.result;
            diskOperationStatus = DISK_OPERATION_SUCCESSFULLY_FINISHED;
        } else {
            diskOperationStatus = DISK_OPERATION_UNSUCCESSFULLY_FINISHED;
        }
#if defined(EEZ_PLATFORM_STM32)
        osThreadYield();
#endif
    }

    ////////////////////////////////////////

	uint32_t getRefreshTimeMs() {
		if (dlog_record::isExecuting()) {
			uint32_t dlogPeriodMs = (uint32_t)(dlog_record::g_recording.parameters.period * 1000);
			if (dlogPeriodMs < REFRESH_TIME_MS) {
				if (dlog_record::isModuleAtSlotRecording(slotIndex)) {
					return dlogPeriodMs;
				}
			}
		}

		return REFRESH_TIME_MS;
	}

    void executeCommand(const CommandDef *command) {
        currentCommand = command;
        retry = 0;
        setState(STATE_WAIT_SLAVE_READY_BEFORE_REQUEST);
	}

    bool startCommand() {
		Request &request = *(Request *)output;

		request.command = currentCommand->command;

		if (currentCommand->fillRequest) {
			(this->*currentCommand->fillRequest)(request);
		}
        spiDmaTransferCompleted = false;
        auto status = bp3c::comm::transferDMA(slotIndex, (uint8_t *)output, (uint8_t *)input, sizeof(Request));
        return status == bp3c::comm::TRANSFER_STATUS_OK;
    }

    bool getCommandResult() {
        Request &request = *(Request *)output;
        request.command = COMMAND_NONE;
        spiDmaTransferCompleted = false;
        auto status = bp3c::comm::transferDMA(slotIndex, (uint8_t *)output, (uint8_t *)input, sizeof(Request));
        return status == bp3c::comm::TRANSFER_STATUS_OK;
    }

    bool isCommandResponse() {
        Response &response = *(Response *)input;
        return response.command == (0x80 | currentCommand->command);
    }

    void doRetry() {
    	bp3c::comm::abortTransfer(slotIndex);
        
        static const int NUM_REQUEST_RETRIES = 10;

        if (++retry < NUM_REQUEST_RETRIES) {
            // try again
            setState(STATE_WAIT_SLAVE_READY_BEFORE_REQUEST);
        } else {
            // give up
            doCommandDone(false);
        }
    }

    void doCommandDone(bool isSuccess) {
        if (isSuccess) {
            lastTransferTime = millis();
        }

		if (currentCommand->done) {
			Response &response = *(Response *)input;
			(this->*currentCommand->done)(response, isSuccess);
		}

		currentCommand = nullptr;
        setState(STATE_IDLE);
    }

    void setState(State newState) {
        state = newState;
        lastStateTransitionTime = millis();
    }

    void stateTransition(Event event) {
    	if (event == EVENT_DMA_TRANSFER_COMPLETED) {
            numCrcErrors = 0;
            numTransferErrors = 0;
    	}
 
        if (state == STATE_WAIT_SLAVE_READY_BEFORE_REQUEST) {
            if (event == EVENT_TIMEOUT) {
				doRetry();
            } else if (event == EVENT_SLAVE_READY) {
                if (startCommand()) {
                    setState(STATE_WAIT_DMA_TRANSFER_COMPLETED_FOR_REQUEST);
                } else {
                    doRetry();
                }
            }
        } else if (state == STATE_WAIT_DMA_TRANSFER_COMPLETED_FOR_REQUEST) {
            if (event == EVENT_TIMEOUT) {
                doRetry();
            } else if (event == EVENT_DMA_TRANSFER_COMPLETED) {
                setState(STATE_WAIT_SLAVE_READY_BEFORE_RESPONSE);
            } else if (event == EVENT_DMA_TRANSFER_FAILED) {
                doRetry();
            }
        } else if (state == STATE_WAIT_SLAVE_READY_BEFORE_RESPONSE) {
            if (event == EVENT_TIMEOUT) {
				doRetry();
            } else if (event == EVENT_SLAVE_READY) {
                if (getCommandResult()) {
                    setState(STATE_WAIT_DMA_TRANSFER_COMPLETED_FOR_RESPONSE);
                } else {
                    doRetry();
                }
            }
        } else if (state == STATE_WAIT_DMA_TRANSFER_COMPLETED_FOR_RESPONSE) {
            if (event == EVENT_TIMEOUT) {
                doRetry();
            } else if (event == EVENT_DMA_TRANSFER_COMPLETED) {
                if (isCommandResponse()) {
                    doCommandDone(true);
                } else {
                    doRetry();
                }
            } else if (event == EVENT_DMA_TRANSFER_FAILED) {
                doRetry();
            }
        } 
    }

    int numCrcErrors = 0;
    int numTransferErrors = 0;

    void reportDmaTransferFailed(int status) {
        if (status == bp3c::comm::TRANSFER_STATUS_CRC_ERROR) {
            numCrcErrors++;
            if (numCrcErrors >= 5) {
                event_queue::pushEvent(event_queue::EVENT_ERROR_SLOT1_CRC_CHECK_ERROR + slotIndex);
                synchronized = false;
                testResult = TEST_FAILED;
            }
            //else if (numCrcErrors > 5) {
            //    DebugTrace("Slot %d CRC error no. %d\n", slotIndex + 1, numCrcErrors);
            //}
        } else {
            numTransferErrors++;
            if (numTransferErrors >= 5) {
                event_queue::pushEvent(event_queue::EVENT_ERROR_SLOT1_SYNC_ERROR + slotIndex);
                synchronized = false;
                testResult = TEST_FAILED;
            }
            //else if (numTransferErrors > 5) {
            //    DebugTrace("Slot %d SPI transfer error %d no. %d\n", slotIndex + 1, status, numTransferErrors);
            //}
        }
    }

    void tick() override {
        if (currentCommand) {
            if (!synchronized && currentCommand->command != COMMAND_GET_INFO) {
                doCommandDone(false);
            } else {
                if (
                    state == STATE_WAIT_SLAVE_READY_BEFORE_REQUEST ||
                    state == STATE_WAIT_SLAVE_READY_BEFORE_RESPONSE
                ) {
                    #if defined(EEZ_PLATFORM_STM32)
                	if (spiReady) {
                        spiReady = false;
                		stateTransition(EVENT_SLAVE_READY);
                	}
                    #endif

                    #if defined(EEZ_PLATFORM_SIMULATOR)
                    stateTransition(EVENT_SLAVE_READY);
                    #endif
                } 
                
                if (
                    state == STATE_WAIT_DMA_TRANSFER_COMPLETED_FOR_REQUEST ||
                    state == STATE_WAIT_DMA_TRANSFER_COMPLETED_FOR_RESPONSE
                ) {
                
                    #if defined(EEZ_PLATFORM_STM32)
                    if (spiDmaTransferCompleted) {
                        if (spiDmaTransferStatus == bp3c::comm::TRANSFER_STATUS_OK) {
                            stateTransition(EVENT_DMA_TRANSFER_COMPLETED);
                        } else {
                            reportDmaTransferFailed(spiDmaTransferStatus);
                            stateTransition(EVENT_DMA_TRANSFER_FAILED);
                        }
                    }
                    #endif                

                    #if defined(EEZ_PLATFORM_SIMULATOR)
                    auto response = (Response *)input;

                    response->command = 0x80 | currentCommand->command;

                    if (currentCommand->command == COMMAND_GET_INFO) {
                        response->getInfo.firmwareMajorVersion = 1;
                        response->getInfo.firmwareMinorVersion = 0;
                        response->getInfo.idw0 = 0;
                        response->getInfo.idw1 = 0;
                        response->getInfo.idw2 = 0;
                        response->getInfo.afeVersion = 1;
                    } else if (currentCommand->command == COMMAND_GET_STATE) {
                        response->getState.flags |= GET_STATE_COMMAND_FLAG_SD_CARD_PRESENT;
                    }

                    stateTransition(EVENT_DMA_TRANSFER_COMPLETED);
                    #endif
                } 
                
                uint32_t tickCountMs = millis();
                if (tickCountMs - lastStateTransitionTime >= TIMEOUT_TIME_MS) {
                    stateTransition(EVENT_TIMEOUT);
                }
            }
        } else {
            if (!synchronized) {
                if (nextDlogCommand) {
                    executeCommand(nextDlogCommand);
                    doCommandDone(false);
                }
                if (diskOperationStatus == DISK_OPERATION_NOT_FINISHED) {
                    executeCommand(diskOperationParams.command);
                    doCommandDone(false);
                }
            } else {
            	uint32_t tickCountMs = millis();
                if (nextDlogCommand) {
                    memcpy(&dlogRecordingStart, &nextDlogRecordingStart, sizeof(DlogRecordingStart));
                    executeCommand(nextDlogCommand);
                    nextDlogCommand = nullptr;
                } else if (diskOperationStatus == DISK_OPERATION_NOT_FINISHED) {
                    executeCommand(diskOperationParams.command);
                } else if (tickCountMs - lastTransferTime >= TIMEOUT_UNTIL_OUT_OF_SYNC_MS) {
                    event_queue::pushEvent(event_queue::EVENT_ERROR_SLOT1_SYNC_ERROR + slotIndex);
                    synchronized = false;
                    testResult = TEST_FAILED;
                } else if (tickCountMs - lastRefreshTime >= getRefreshTimeMs()) {
                    refreshStartTime = tickCountMs;
                    executeCommand(&getState_command);
                } else {
                    SetParams params;
                    fillSetParams(params);
                    if (memcmp(&params, &lastTransferredParams, sizeof(SetParams)) != 0) {
                        executeCommand(&setParams_command);
                    }
                }
            }
        }
    }

    void onSpiIrq() {
        spiReady = true;
		if (g_isBooted) {
			stateTransition(EVENT_SLAVE_READY);
		}
    }

    void onSpiDmaTransferCompleted(int status) override {
         if (g_isBooted) {
             if (status == bp3c::comm::TRANSFER_STATUS_OK) {
                 stateTransition(EVENT_DMA_TRANSFER_COMPLETED);
             } else {
                 reportDmaTransferFailed(status);
                 stateTransition(EVENT_DMA_TRANSFER_FAILED);
             }
         } else {
             spiDmaTransferCompleted = true;
             spiDmaTransferStatus = status;
         }
    }

    void onPowerDown() override {
        synchronized = false;
#ifdef EEZ_PLATFORM_SIMULATOR
        sendMessageToLowPriorityThread(THREAD_MESSAGE_FS_DRIVER_UNLINK, slotIndex, 0);
#endif
    }

    void resync() override {
        if (!synchronized) {
            executeCommand(&getInfo_command);
        }
    }

    Page *getPageFromId(int pageId) override;

    virtual void animatePageAppearance(int previousPageId, int activePageId) {
        if (
            (previousPageId == PAGE_ID_SYS_SETTINGS_LABELS_AND_COLORS && activePageId == PAGE_ID_DIB_MIO168_CHANNEL_LABELS) ||
            (previousPageId == PAGE_ID_DIB_MIO168_CHANNEL_LABELS && activePageId == PAGE_ID_DIB_MIO168_DIN_CHANNEL_LABELS) ||
            (previousPageId == PAGE_ID_DIB_MIO168_CHANNEL_LABELS && activePageId == PAGE_ID_DIB_MIO168_DOUT_CHANNEL_LABELS)
        ) {
            psu::gui::animateSettingsSlideLeft(true);
        } else if (
            (previousPageId == PAGE_ID_DIB_MIO168_CHANNEL_LABELS && activePageId == PAGE_ID_SYS_SETTINGS_LABELS_AND_COLORS) ||
            (previousPageId == PAGE_ID_DIB_MIO168_DIN_CHANNEL_LABELS && activePageId == PAGE_ID_DIB_MIO168_CHANNEL_LABELS) ||
            (previousPageId == PAGE_ID_DIB_MIO168_DOUT_CHANNEL_LABELS && activePageId == PAGE_ID_DIB_MIO168_CHANNEL_LABELS)
        ) {
            psu::gui::animateSettingsSlideRight(true);
        }
    }

    int getSlotView(SlotViewType slotViewType, int slotIndex, int cursor) override {
        if (slotViewType == SLOT_VIEW_TYPE_DEFAULT) {
            return isDefaultViewVertical() ? PAGE_ID_DIB_MIO168_SLOT_VIEW_DEF : PAGE_ID_SLOT_DEF_HORZ_EMPTY;
        }
        if (slotViewType == SLOT_VIEW_TYPE_DEFAULT_2COL) {
            return isDefaultViewVertical() ? PAGE_ID_DIB_MIO168_SLOT_VIEW_DEF_2COL : PAGE_ID_SLOT_DEF_HORZ_EMPTY_2COL;
        }
        if (slotViewType == SLOT_VIEW_TYPE_MAX) {
            return PAGE_ID_DIB_MIO168_SLOT_VIEW_MAX;
        }
        if (slotViewType == SLOT_VIEW_TYPE_MIN) {
            return PAGE_ID_DIB_MIO168_SLOT_VIEW_MIN;
        }
        assert(slotViewType == SLOT_VIEW_TYPE_MICRO);
        return PAGE_ID_DIB_MIO168_SLOT_VIEW_MICRO;
    }

    int getSlotSettingsPageId() override {
        return PAGE_ID_DIB_MIO168_SETTINGS;
    }

    int getLabelsAndColorsPageId() override {
        return PAGE_ID_DIB_MIO168_LABELS_AND_COLORS;
    }

    void onHighPriorityThreadMessage(uint8_t type, uint32_t param) override;

    struct ProfileParameters : public Module::ProfileParameters  {
        DinChannel::ProfileParameters dinChannel;
        DoutChannel::ProfileParameters doutChannel;
        AinChannel::ProfileParameters ainChannels[4];
        AoutDac7760Channel::ProfileParameters aoutDac7760Channels[2];
        AoutDac7563Channel::ProfileParameters aoutDac7563Channels[2];
        PwmChannel::ProfileParameters pwmChannels[2];
    };

    void resetProfileToDefaults(uint8_t *buffer) override {
        Module::resetProfileToDefaults(buffer);

        auto parameters = (ProfileParameters *)buffer;

        dinChannel.resetProfileToDefaults(parameters->dinChannel);
        doutChannel.resetProfileToDefaults(parameters->doutChannel);
        for (int i = 0; i < 4; i++) {
            ainChannels[i].resetProfileToDefaults(parameters->ainChannels[i]);
        }
        for (int i = 0; i < 2; i++) {
            aoutDac7760Channels[i].resetProfileToDefaults(parameters->aoutDac7760Channels[i]);
        }
        for (int i = 0; i < 2; i++) {
            aoutDac7563Channels[i].resetProfileToDefaults(parameters->aoutDac7563Channels[i]);
        }
        for (int i = 0; i < 2; i++) {
            pwmChannels[i].resetProfileToDefaults(parameters->pwmChannels[i]);
        }
    }

    void getProfileParameters(uint8_t *buffer) override {
        Module::getProfileParameters(buffer);

        assert(sizeof(ProfileParameters) < MAX_CHANNEL_PARAMETERS_SIZE);

        auto parameters = (ProfileParameters *)buffer;

        dinChannel.getProfileParameters(parameters->dinChannel);
        doutChannel.getProfileParameters(parameters->doutChannel);
        for (int i = 0; i < 4; i++) {
            ainChannels[i].getProfileParameters(parameters->ainChannels[i]);
        }
        for (int i = 0; i < 2; i++) {
            aoutDac7760Channels[i].getProfileParameters(parameters->aoutDac7760Channels[i]);
        }
        for (int i = 0; i < 2; i++) {
            aoutDac7563Channels[i].getProfileParameters(parameters->aoutDac7563Channels[i]);
        }
        for (int i = 0; i < 2; i++) {
            pwmChannels[i].getProfileParameters(parameters->pwmChannels[i]);
        }
    }
    
    void setProfileParameters(uint8_t *buffer, bool mismatch, int recallOptions) override {
        Module::setProfileParameters(buffer, mismatch, recallOptions);

        auto parameters = (ProfileParameters *)buffer;

        dinChannel.setProfileParameters(parameters->dinChannel);
        doutChannel.setProfileParameters(parameters->doutChannel);
        for (int i = 0; i < 4; i++) {
            ainChannels[i].setProfileParameters(parameters->ainChannels[i]);
			if (ainChannels[i].m_range > AinChannel::getRangeMaxValue(AIN_1_SUBCHANNEL_INDEX + i, ainChannels[i].m_mode)) {
				ainChannels[i].m_range = 0;
			}
        }
        for (int i = 0; i < 2; i++) {
            aoutDac7760Channels[i].setProfileParameters(parameters->aoutDac7760Channels[i]);
        }
        for (int i = 0; i < 2; i++) {
            aoutDac7563Channels[i].setProfileParameters(parameters->aoutDac7563Channels[i]);
        }
        for (int i = 0; i < 2; i++) {
            pwmChannels[i].setProfileParameters(parameters->pwmChannels[i]);
        }
    }
    
    bool writeProfileProperties(psu::profile::WriteContext &ctx, const uint8_t *buffer) override {
        if (!Module::writeProfileProperties(ctx, buffer)) {
            return false;
        }

        auto parameters = (ProfileParameters *)buffer;

        if (!dinChannel.writeProfileProperties(ctx, parameters->dinChannel)) {
            return false;
        }
        if (!doutChannel.writeProfileProperties(ctx, parameters->doutChannel)) {
            return false;
        }
        for (int i = 0; i < 4; i++) {
            if (!ainChannels[i].writeProfileProperties(ctx, i, parameters->ainChannels[i])) {
                return false;
            }
        }
        for (int i = 0; i < 2; i++) {
            if (!aoutDac7760Channels[i].writeProfileProperties(ctx, i, parameters->aoutDac7760Channels[i])) {
                return false;
            }
        }
        for (int i = 0; i < 2; i++) {
            if (!aoutDac7563Channels[i].writeProfileProperties(ctx, i, parameters->aoutDac7563Channels[i])) {
                return false;
            }
        }
        for (int i = 0; i < 2; i++) {
            if (!pwmChannels[i].writeProfileProperties(ctx, i, parameters->pwmChannels[i])) {
                return false;
            }
        }

        return true;
    }
    
    bool readProfileProperties(psu::profile::ReadContext &ctx, uint8_t *buffer) override {
        if (Module::readProfileProperties(ctx, buffer)) {
            return true;
        }

        auto parameters = (ProfileParameters *)buffer;

        if (dinChannel.readProfileProperties(ctx, parameters->dinChannel)) {
            return true;
        }
        if (doutChannel.readProfileProperties(ctx, parameters->doutChannel)) {
            return true;
        }
        for (int i = 0; i < 4; i++) {
            if (ainChannels[i].readProfileProperties(ctx, i, parameters->ainChannels[i])) {
                return true;
            }
        }
        for (int i = 0; i < 2; i++) {
            if (aoutDac7760Channels[i].readProfileProperties(ctx, i, parameters->aoutDac7760Channels[i])) {
                return true;
            }
        }
        for (int i = 0; i < 2; i++) {
            if (aoutDac7563Channels[i].readProfileProperties(ctx, i, parameters->aoutDac7563Channels[i])) {
                return true;
            }
        }
        for (int i = 0; i < 2; i++) {
            if (pwmChannels[i].readProfileProperties(ctx, i, parameters->pwmChannels[i])) {
                return true;
            }
        }

        return false;
    }

    void resetConfiguration() {
        Module::resetConfiguration();

        dinChannel.resetConfiguration();
        doutChannel.resetConfiguration();
        for (int i = 0; i < 4; i++) {
            ainChannels[i].resetConfiguration();
        }
        for (int i = 0; i < 2; i++) {
            aoutDac7760Channels[i].resetConfiguration();
        }
        for (int i = 0; i < 2; i++) {
            aoutDac7563Channels[i].resetConfiguration();
        }
        for (int i = 0; i < 2; i++) {
            pwmChannels[i].resetConfiguration();
        }
    }

    size_t getChannelLabelMaxLength(int subchannelIndex) override {
        if (subchannelIndex == DIN_SUBCHANNEL_INDEX || subchannelIndex == DOUT_SUBCHANNEL_INDEX) {
            return 8 * (CHANNEL_LABEL_MAX_LENGTH + 1);
        }
        return CHANNEL_LABEL_MAX_LENGTH;
    }

    eez_err_t getChannelLabel(int subchannelIndex, const char *&label) override {
        if (subchannelIndex >= AIN_1_SUBCHANNEL_INDEX && subchannelIndex <= AIN_4_SUBCHANNEL_INDEX) {
            label = ainChannels[subchannelIndex - AIN_1_SUBCHANNEL_INDEX].m_label;
            return SCPI_RES_OK;
        }

        if (subchannelIndex >= AOUT_1_SUBCHANNEL_INDEX && subchannelIndex <= AOUT_2_SUBCHANNEL_INDEX) {
            label = aoutDac7760Channels[subchannelIndex - AOUT_1_SUBCHANNEL_INDEX].m_label;
            return SCPI_RES_OK;
        }

        if (subchannelIndex >= AOUT_3_SUBCHANNEL_INDEX && subchannelIndex <= AOUT_4_SUBCHANNEL_INDEX) {
            label = aoutDac7563Channels[subchannelIndex - AOUT_3_SUBCHANNEL_INDEX].m_label;
            return SCPI_RES_OK;
        }

        if (subchannelIndex >= PWM_1_SUBCHANNEL_INDEX && subchannelIndex <= PWM_2_SUBCHANNEL_INDEX) {
            label = pwmChannels[subchannelIndex - PWM_1_SUBCHANNEL_INDEX].m_label;
            return SCPI_RES_OK;
        }

        return SCPI_ERROR_HARDWARE_MISSING;
    }

    const char *getChannelLabel(int subchannelIndex) override {
        if (subchannelIndex == DIN_SUBCHANNEL_INDEX) {
            return dinChannel.m_pinLabels;
        } 
        if (subchannelIndex == DOUT_SUBCHANNEL_INDEX) {
            return doutChannel.m_pinLabels;
        }

        const char *label;
        auto err = getChannelLabel(subchannelIndex, label);
        if (err == SCPI_RES_OK) {
            return label;
        } 
        return "";
    }

    const char *getDefaultChannelLabel(int subchannelIndex) override {
        if (subchannelIndex == DIN_SUBCHANNEL_INDEX) {
            return "DIN1\0\0DIN2\0\0DIN3\0\0DIN4\0\0DIN5\0\0DIN6\0\0DIN7\0\0DIN8\0\0";
        } 
        if (subchannelIndex == DOUT_SUBCHANNEL_INDEX) {
            return "DOUT1\0DOUT2\0DOUT3\0DOUT4\0DOUT5\0DOUT6\0DOUT7\0DOUT8\0";
        }
        if (subchannelIndex == AIN_1_SUBCHANNEL_INDEX) {
            return "AIN1";
        }
        if (subchannelIndex == AIN_2_SUBCHANNEL_INDEX) {
            return "AIN2";
        }
        if (subchannelIndex == AIN_3_SUBCHANNEL_INDEX) {
            return "AIN3";
        }
        if (subchannelIndex == AIN_4_SUBCHANNEL_INDEX) {
            return "AIN4";
        }
        if (subchannelIndex == AOUT_1_SUBCHANNEL_INDEX) {
            return "AOUT1";
        }
        if (subchannelIndex == AOUT_2_SUBCHANNEL_INDEX) {
            return "AOUT2";
        }
        if (subchannelIndex == AOUT_3_SUBCHANNEL_INDEX) {
            return "AOUT3";
        }
        if (subchannelIndex == AOUT_4_SUBCHANNEL_INDEX) {
            return "AOUT4";
        }
        if (subchannelIndex == PWM_1_SUBCHANNEL_INDEX) {
            return "PWM1";
        }
        if (subchannelIndex == PWM_2_SUBCHANNEL_INDEX) {
            return "PWM2";
        }
        return "";
    }

    eez_err_t setChannelLabel(int subchannelIndex, const char *label, int length) override {
        if (subchannelIndex == DIN_SUBCHANNEL_INDEX) {
            if (length != -1) {
                return SCPI_ERROR_HARDWARE_MISSING;
            }
            memcpy(dinChannel.m_pinLabels, label, 8 * (CHANNEL_LABEL_MAX_LENGTH + 1));
            return SCPI_RES_OK;
        }
        
        if (subchannelIndex == DOUT_SUBCHANNEL_INDEX) {
            if (length != -1) {
                return SCPI_ERROR_HARDWARE_MISSING;
            }
            memcpy(doutChannel.m_pinLabels, label, 8 * (CHANNEL_LABEL_MAX_LENGTH + 1));
            return SCPI_RES_OK;
        } 

        if (length == -1) {
            length = strlen(label);
        }

        int maxLength = getChannelLabelMaxLength(subchannelIndex);
        if (length > maxLength) {
            length = maxLength;
        }
        
        if (subchannelIndex >= AIN_1_SUBCHANNEL_INDEX && subchannelIndex <= AIN_4_SUBCHANNEL_INDEX) {
            strncpy(ainChannels[subchannelIndex - AIN_1_SUBCHANNEL_INDEX].m_label, label, length);
            ainChannels[subchannelIndex - AIN_1_SUBCHANNEL_INDEX].m_label[length] = 0;
            return SCPI_RES_OK;
        }
        
        if (subchannelIndex >= AOUT_1_SUBCHANNEL_INDEX && subchannelIndex <= AOUT_2_SUBCHANNEL_INDEX) {
            strncpy(aoutDac7760Channels[subchannelIndex - AOUT_1_SUBCHANNEL_INDEX].m_label, label, length);
            aoutDac7760Channels[subchannelIndex - AOUT_1_SUBCHANNEL_INDEX].m_label[length] = 0;
            return SCPI_RES_OK;
        }
        
        if (subchannelIndex >= AOUT_3_SUBCHANNEL_INDEX && subchannelIndex <= AOUT_4_SUBCHANNEL_INDEX) {
            strncpy(aoutDac7563Channels[subchannelIndex - AOUT_3_SUBCHANNEL_INDEX].m_label, label, length);
            aoutDac7563Channels[subchannelIndex - AOUT_3_SUBCHANNEL_INDEX].m_label[length] = 0;
            return SCPI_RES_OK;
        }
        
        if (subchannelIndex >= PWM_1_SUBCHANNEL_INDEX && subchannelIndex <= PWM_2_SUBCHANNEL_INDEX) {
            strncpy(pwmChannels[subchannelIndex - PWM_1_SUBCHANNEL_INDEX].m_label, label, length);
            pwmChannels[subchannelIndex - PWM_1_SUBCHANNEL_INDEX].m_label[length] = 0;
            return SCPI_RES_OK;
        }

        return SCPI_ERROR_HARDWARE_MISSING;
    }

    size_t getChannelPinLabelMaxLength(int subchannelIndex, int pin) override {
        return CHANNEL_LABEL_MAX_LENGTH;
    }

    const char *getDefaultChannelPinLabel(int subchannelIndex, int pin) override {
        if (subchannelIndex == DIN_SUBCHANNEL_INDEX || subchannelIndex == DOUT_SUBCHANNEL_INDEX) {
            if (pin >= 1 && pin <= 8) {
                const char *label = getDefaultChannelLabel(subchannelIndex);
                return label + (pin - 1) * (CHANNEL_LABEL_MAX_LENGTH + 1);
            }
        }
        return "";
    }

    eez_err_t getChannelPinLabel(int subchannelIndex, int pin, const char *&label) override {
        if (subchannelIndex == DIN_SUBCHANNEL_INDEX || subchannelIndex == DOUT_SUBCHANNEL_INDEX) {
            if (pin >= 1 && pin <= 8) {
                const char *channelLabel = getChannelLabel(subchannelIndex);
                label = channelLabel + (pin - 1) * (CHANNEL_LABEL_MAX_LENGTH + 1);
                return SCPI_RES_OK;
            }
        }
        return SCPI_ERROR_HARDWARE_MISSING;
    }

    eez_err_t setChannelPinLabel(int subchannelIndex, int pin, const char *label, int length) override {
        if (subchannelIndex == DIN_SUBCHANNEL_INDEX || subchannelIndex == DOUT_SUBCHANNEL_INDEX) {
            if (pin >= 1 && pin <= 8) {
                if (length == -1) {
                    length = strlen(label);
                }

                auto maxLength = getChannelPinLabelMaxLength(subchannelIndex, pin);
                if (length > (int)maxLength) {
                    length = maxLength;
                }

                const char *channelLabel = getChannelLabel(subchannelIndex);
                char *pinLabel = (char *)(channelLabel + (pin - 1) * (CHANNEL_LABEL_MAX_LENGTH + 1));

                strncpy(pinLabel, label, length);
                pinLabel[length] = 0;

                return SCPI_RES_OK;
            }
        }
        return SCPI_ERROR_HARDWARE_MISSING;
    }

    const char *getPinLabelOrDefault(char subchannelIndex, int pin) {
        const char *label = getChannelLabel(subchannelIndex) + pin * (CHANNEL_LABEL_MAX_LENGTH + 1);
        if (*label) {
            return label;
        }
        return getDefaultChannelLabel(subchannelIndex) + pin * (CHANNEL_LABEL_MAX_LENGTH + 1);
    }

    bool getDigitalInputData(int subchannelIndex, uint8_t &data, int *err) override {
        if (subchannelIndex != DIN_SUBCHANNEL_INDEX) {
            if (err) {
                *err = SCPI_ERROR_HARDWARE_MISSING;
            }
            return false;
        }
        data = dinChannel.getDigitalInputData();
        return true;
    }

#ifdef EEZ_PLATFORM_SIMULATOR
    bool setDigitalInputData(int subchannelIndex, uint8_t data, int *err) override {
        if (subchannelIndex != DIN_SUBCHANNEL_INDEX) {
            if (err) {
                *err = SCPI_ERROR_HARDWARE_MISSING;
            }
            return false;
        }
        dinChannel.setDigitalInputData(data);
        return true;
    }
#endif

    bool getDigitalInputRange(int subchannelIndex, uint8_t pin, uint8_t &range, int *err) override {
        if (subchannelIndex != DIN_SUBCHANNEL_INDEX) {
            if (err) {
                *err = SCPI_ERROR_HARDWARE_MISSING;
            }
            return false;
        }

        if (pin < 0 || pin > 7) {
            if (err) {
                *err = SCPI_ERROR_ILLEGAL_PARAMETER_VALUE;
            }
            return false;
        }

        range = dinChannel.m_pinRanges & (1 << pin) ? 1 : 0;

        return true;
    }
    
    bool setDigitalInputRange(int subchannelIndex, uint8_t pin, uint8_t range, int *err) override {
        if (subchannelIndex != DIN_SUBCHANNEL_INDEX) {
            if (err) {
                *err = SCPI_ERROR_HARDWARE_MISSING;
            }
            return false;
        }

        if (pin < 0 || pin > 7) {
            if (err) {
                *err = SCPI_ERROR_ILLEGAL_PARAMETER_VALUE;
            }
            return false;
        }

        if (range) {
            dinChannel.m_pinRanges |= 1 << pin;
        } else {
            dinChannel.m_pinRanges &= ~(1 << pin);
        }

        return true;
    }

    bool getDigitalInputSpeed(int subchannelIndex, uint8_t pin, uint8_t &speed, int *err) override {
        if (subchannelIndex != DIN_SUBCHANNEL_INDEX) {
            if (err) {
                *err = SCPI_ERROR_HARDWARE_MISSING;
            }
            return false;
        }

        if (pin < 0 || pin > 1) {
            if (err) {
                *err = SCPI_ERROR_ILLEGAL_PARAMETER_VALUE;
            }
            return false;
        }

        speed = dinChannel.m_pinSpeeds & (1 << pin) ? 1 : 0;

        return true;
    }
    
    bool setDigitalInputSpeed(int subchannelIndex, uint8_t pin, uint8_t speed, int *err) override {
        if (subchannelIndex != DIN_SUBCHANNEL_INDEX) {
            if (err) {
                *err = SCPI_ERROR_HARDWARE_MISSING;
            }
            return false;
        }

        if (pin < 0 || pin > 1) {
            if (err) {
                *err = SCPI_ERROR_ILLEGAL_PARAMETER_VALUE;
            }
            return false;
        }

        if (speed) {
            dinChannel.m_pinSpeeds |= 1 << pin;
        } else {
            dinChannel.m_pinSpeeds &= ~(1 << pin);
        }

        return true;
    }

    bool getDigitalOutputData(int subchannelIndex, uint8_t &data, int *err) override {
        if (subchannelIndex != DOUT_SUBCHANNEL_INDEX) {
            if (err) {
                *err = SCPI_ERROR_HARDWARE_MISSING;
            }
            return false;
        }
        data = doutChannel.getDigitalOutputData();
        return true;
    }
    
    bool setDigitalOutputData(int subchannelIndex, uint8_t data, int *err) override {
        if (subchannelIndex != DOUT_SUBCHANNEL_INDEX) {
            if (err) {
                *err = SCPI_ERROR_HARDWARE_MISSING;
            }
            return false;
        }
        doutChannel.setDigitalOutputData(data);
        return true;
    }

    bool getMode(int subchannelIndex, SourceMode &mode, int *err) override {
        if (subchannelIndex != AOUT_1_SUBCHANNEL_INDEX && subchannelIndex != AOUT_2_SUBCHANNEL_INDEX) {
            if (err) {
                *err = SCPI_ERROR_HARDWARE_MISSING;
            }
            return false;
        }
        mode = aoutDac7760Channels[subchannelIndex - AOUT_1_SUBCHANNEL_INDEX].getMode();
        return true;
    }
    
    bool setMode(int subchannelIndex, SourceMode mode, int *err) override {
        if (subchannelIndex != AOUT_1_SUBCHANNEL_INDEX && subchannelIndex != AOUT_2_SUBCHANNEL_INDEX) {
            if (err) {
                *err = SCPI_ERROR_HARDWARE_MISSING;
            }
            return false;
        }
        aoutDac7760Channels[subchannelIndex - AOUT_1_SUBCHANNEL_INDEX].setMode(mode);
        return true;
    }

    bool getCurrentRange(int subchannelIndex, uint8_t &range, int *err) override {
        if (subchannelIndex != AOUT_1_SUBCHANNEL_INDEX && subchannelIndex != AOUT_2_SUBCHANNEL_INDEX) {
            if (err) {
                *err = SCPI_ERROR_HARDWARE_MISSING;
            }
            return false;
        }
        range = aoutDac7760Channels[subchannelIndex - AOUT_1_SUBCHANNEL_INDEX].getCurrentRange();
        return true;
    }
    
    bool setCurrentRange(int subchannelIndex, uint8_t range, int *err) override {
        if (subchannelIndex != AOUT_1_SUBCHANNEL_INDEX && subchannelIndex != AOUT_2_SUBCHANNEL_INDEX) {
            if (err) {
                *err = SCPI_ERROR_HARDWARE_MISSING;
            }
            return false;
        }
        aoutDac7760Channels[subchannelIndex - AOUT_1_SUBCHANNEL_INDEX].setCurrentRange(range);
        return true;
    }

    bool getVoltageRange(int subchannelIndex, uint8_t &range, int *err) override {
        if (subchannelIndex != AOUT_1_SUBCHANNEL_INDEX && subchannelIndex != AOUT_2_SUBCHANNEL_INDEX) {
            if (err) {
                *err = SCPI_ERROR_HARDWARE_MISSING;
            }
            return false;
        }
        range = aoutDac7760Channels[subchannelIndex - AOUT_1_SUBCHANNEL_INDEX].getVoltageRange();
        return true;
    }
    
    bool setVoltageRange(int subchannelIndex, uint8_t range, int *err) override {
        if (subchannelIndex != AOUT_1_SUBCHANNEL_INDEX && subchannelIndex != AOUT_2_SUBCHANNEL_INDEX) {
            if (err) {
                *err = SCPI_ERROR_HARDWARE_MISSING;
            }
            return false;
        }
        aoutDac7760Channels[subchannelIndex - AOUT_1_SUBCHANNEL_INDEX].setVoltageRange(range);
        return true;
    }

    bool getMeasureMode(int subchannelIndex, MeasureMode &mode, int *err) override {
        if (subchannelIndex != AIN_1_SUBCHANNEL_INDEX && subchannelIndex != AIN_4_SUBCHANNEL_INDEX) {
            if (err) {
                *err = SCPI_ERROR_HARDWARE_MISSING;
            }
            return false;
        }
        mode = (MeasureMode)ainChannels[subchannelIndex - AIN_1_SUBCHANNEL_INDEX].m_mode;
        return true;
    }
    
    bool setMeasureMode(int subchannelIndex, MeasureMode mode, int *err) override {
        if (subchannelIndex != AIN_1_SUBCHANNEL_INDEX && subchannelIndex != AIN_4_SUBCHANNEL_INDEX) {
            if (err) {
                *err = SCPI_ERROR_HARDWARE_MISSING;
            }
            return false;
        }
        ainChannels[subchannelIndex - AIN_1_SUBCHANNEL_INDEX].m_mode = mode;
        return true;
    }

    bool getMeasureRange(int subchannelIndex, uint8_t &range, int *err) override {
        if (subchannelIndex != AIN_1_SUBCHANNEL_INDEX && subchannelIndex != AIN_4_SUBCHANNEL_INDEX) {
            if (err) {
                *err = SCPI_ERROR_HARDWARE_MISSING;
            }
            return false;
        }
        range = ainChannels[subchannelIndex - AIN_1_SUBCHANNEL_INDEX].m_range;
        return true;
    }
    
    bool setMeasureRange(int subchannelIndex, uint8_t range, int *err) override {
        if (subchannelIndex != AIN_1_SUBCHANNEL_INDEX && subchannelIndex != AIN_4_SUBCHANNEL_INDEX) {
            if (err) {
                *err = SCPI_ERROR_HARDWARE_MISSING;
            }
            return false;
        }

        if (range > AinChannel::getRangeMaxValue(subchannelIndex, ainChannels[subchannelIndex - AIN_1_SUBCHANNEL_INDEX].m_mode)) {
            if (err) {
                *err = SCPI_ERROR_ILLEGAL_PARAMETER_VALUE;
            }
            return false;
        }

        ainChannels[subchannelIndex - AIN_1_SUBCHANNEL_INDEX].m_range = range;

        return true;
    }

    bool outputEnable(int subchannelIndex, bool enable, int *err) override {
        if (subchannelIndex < AOUT_1_SUBCHANNEL_INDEX || subchannelIndex > AOUT_2_SUBCHANNEL_INDEX) {
            if (err) {
                *err = SCPI_ERROR_HARDWARE_MISSING;
            }
            return false;
        }

        aoutDac7760Channels[subchannelIndex - AOUT_1_SUBCHANNEL_INDEX].m_outputEnabled = enable;
        return true;
    }

    bool isOutputEnabled(int subchannelIndex, bool &enabled, int *err) override {
        if (subchannelIndex < AOUT_1_SUBCHANNEL_INDEX || subchannelIndex > AOUT_2_SUBCHANNEL_INDEX) {
            if (err) {
                *err = SCPI_ERROR_HARDWARE_MISSING;
            }
            return false;
        }

        enabled = aoutDac7760Channels[subchannelIndex - AOUT_1_SUBCHANNEL_INDEX].m_outputEnabled;

        return true;
    }
    
    bool getVoltage(int subchannelIndex, float &value, int *err) override {
        if (subchannelIndex < AOUT_1_SUBCHANNEL_INDEX || subchannelIndex > AOUT_4_SUBCHANNEL_INDEX) {
            if (err) {
                *err = SCPI_ERROR_HARDWARE_MISSING;
            }
            return false;
        }

        if (subchannelIndex < AOUT_3_SUBCHANNEL_INDEX) {
            value = aoutDac7760Channels[subchannelIndex - AOUT_1_SUBCHANNEL_INDEX].m_voltageValue;
        } else {
            value = aoutDac7563Channels[subchannelIndex - AOUT_3_SUBCHANNEL_INDEX].m_value;
        }

        return true;
    }

    bool setVoltage(int subchannelIndex, float value, int *err) override {
        if (subchannelIndex < AOUT_1_SUBCHANNEL_INDEX || subchannelIndex > AOUT_4_SUBCHANNEL_INDEX) {
            if (err) {
                *err = SCPI_ERROR_HARDWARE_MISSING;
            }
            return false;
        }

        if (subchannelIndex < AOUT_3_SUBCHANNEL_INDEX) {
            aoutDac7760Channels[subchannelIndex - AOUT_1_SUBCHANNEL_INDEX].m_voltageValue = value;
        } else {
            aoutDac7563Channels[subchannelIndex - AOUT_3_SUBCHANNEL_INDEX].m_value = value;
        }

        return true;
    }

    bool getMeasuredVoltage(int subchannelIndex, float &value, int *err) override {
        if (subchannelIndex < AIN_1_SUBCHANNEL_INDEX || subchannelIndex > AIN_4_SUBCHANNEL_INDEX) {
            if (err) {
                *err = SCPI_ERROR_HARDWARE_MISSING;
            }
            return false;
        }

        auto &channel = ainChannels[subchannelIndex - AIN_1_SUBCHANNEL_INDEX];

        if (channel.m_mode != MEASURE_MODE_VOLTAGE) {
            if (err) {
                *err = SCPI_ERROR_EXECUTION_ERROR;
            }
            return false;
        }

        value = channel.m_value;
        return true;
    }

    void getVoltageStepValues(int subchannelIndex, StepValues *stepValues, bool calibrationMode) override {
        if (subchannelIndex >= AOUT_1_SUBCHANNEL_INDEX && subchannelIndex <= AOUT_2_SUBCHANNEL_INDEX) {
            aoutDac7760Channels[subchannelIndex - AOUT_1_SUBCHANNEL_INDEX].getStepValues(stepValues);
        } else if (subchannelIndex >= AOUT_3_SUBCHANNEL_INDEX && subchannelIndex <= AOUT_4_SUBCHANNEL_INDEX) {
        	aoutDac7563Channels[subchannelIndex - AOUT_3_SUBCHANNEL_INDEX].getStepValues(stepValues);
        }

        if (calibrationMode) {
            stepValues->encoderSettings.step /= 10.0f;
            stepValues->encoderSettings.range = stepValues->encoderSettings.step * 10.0f;
        }
    }
    
    float getVoltageResolution(int subchannelIndex) override {
        if (subchannelIndex >= AOUT_1_SUBCHANNEL_INDEX && subchannelIndex <= AOUT_2_SUBCHANNEL_INDEX) {
            return aoutDac7760Channels[subchannelIndex - AOUT_1_SUBCHANNEL_INDEX].getVoltageResolution();
        } else {
            return aoutDac7563Channels[subchannelIndex - AOUT_3_SUBCHANNEL_INDEX].getResolution();
        }
    }

    float getVoltageMinValue(int subchannelIndex) override {
        if (subchannelIndex >= AOUT_1_SUBCHANNEL_INDEX && subchannelIndex <= AOUT_2_SUBCHANNEL_INDEX) {
            return aoutDac7760Channels[subchannelIndex - AOUT_1_SUBCHANNEL_INDEX].getVoltageMinValue();
        } else {
            return AOUT_DAC7563_MIN;
        }    
    }

    float getVoltageMaxValue(int subchannelIndex) override {
        if (subchannelIndex >= AOUT_1_SUBCHANNEL_INDEX && subchannelIndex <= AOUT_2_SUBCHANNEL_INDEX) {
            return aoutDac7760Channels[subchannelIndex - AOUT_1_SUBCHANNEL_INDEX].getVoltageMaxValue();
        } else {
            return AOUT_DAC7563_MAX;
        }    
    }

    bool isConstantVoltageMode(int subchannelIndex) override {
        return true;
    }

    bool isVoltageCalibrationExists(int subchannelIndex) override {
        if (subchannelIndex >= AOUT_1_SUBCHANNEL_INDEX && subchannelIndex <= AOUT_2_SUBCHANNEL_INDEX) {
            return aoutDac7760Channels[subchannelIndex - AOUT_1_SUBCHANNEL_INDEX].getCalConf()->state.calState;
        } else if (subchannelIndex >= AOUT_3_SUBCHANNEL_INDEX && subchannelIndex <= AOUT_4_SUBCHANNEL_INDEX) {
            return aoutDac7563Channels[subchannelIndex - AOUT_3_SUBCHANNEL_INDEX].calConf.state.calState;
        }
        return false;
    }

    bool isVoltageCalibrationEnabled(int subchannelIndex) override {
        if (subchannelIndex >= AOUT_1_SUBCHANNEL_INDEX && subchannelIndex <= AOUT_2_SUBCHANNEL_INDEX) {
            return aoutDac7760Channels[subchannelIndex - AOUT_1_SUBCHANNEL_INDEX].getCalConf()->state.calEnabled;
        } else if (subchannelIndex >= AOUT_3_SUBCHANNEL_INDEX && subchannelIndex <= AOUT_4_SUBCHANNEL_INDEX) {
            return aoutDac7563Channels[subchannelIndex - AOUT_3_SUBCHANNEL_INDEX].calConf.state.calEnabled;
        }
        return false;
    }
    
    void enableVoltageCalibration(int subchannelIndex, bool enabled) override {
        if (subchannelIndex >= AOUT_1_SUBCHANNEL_INDEX && subchannelIndex <= AOUT_2_SUBCHANNEL_INDEX) {
            aoutDac7760Channels[subchannelIndex - AOUT_1_SUBCHANNEL_INDEX].getCalConf()->state.calEnabled = enabled;
        } else if (subchannelIndex >= AOUT_3_SUBCHANNEL_INDEX && subchannelIndex <= AOUT_4_SUBCHANNEL_INDEX) {
            aoutDac7563Channels[subchannelIndex - AOUT_3_SUBCHANNEL_INDEX].calConf.state.calEnabled = enabled;
        } else {
            return;
        }

        saveChannelCalibration(subchannelIndex, nullptr);
    }

    bool loadChannelCalibration(int subchannelIndex, int *err) override {
        assert(sizeof(CalConf) <= 64);

        if (subchannelIndex >= AOUT_1_SUBCHANNEL_INDEX && subchannelIndex <= AOUT_2_SUBCHANNEL_INDEX) {
            for (int i = 0; i < 7; i++) {
                CalConf *calConf = &aoutDac7760Channels[subchannelIndex - AOUT_1_SUBCHANNEL_INDEX].calConf[i];
                if (!persist_conf::loadChannelCalibrationConfiguration(slotIndex, (subchannelIndex - AOUT_1_SUBCHANNEL_INDEX) * 7 + i, &calConf->header, sizeof(CalConf), CalConf::VERSION)) {
                    calConf->clear();
                }
            }
        } else if (subchannelIndex >= AOUT_3_SUBCHANNEL_INDEX && subchannelIndex <= AOUT_4_SUBCHANNEL_INDEX) {
            CalConf *calConf = &aoutDac7563Channels[subchannelIndex - AOUT_3_SUBCHANNEL_INDEX].calConf;
            if (!persist_conf::loadChannelCalibrationConfiguration(slotIndex, 2 * 7 + (subchannelIndex - AOUT_3_SUBCHANNEL_INDEX), &calConf->header, sizeof(CalConf), CalConf::VERSION)) {
                calConf->clear();
            }
        } else {
        }

        return true;
    }

    bool saveChannelCalibration(int subchannelIndex, int *err) override {
        CalConf *calConf = nullptr;
        int calConfIndex = 0;

        if (subchannelIndex >= AOUT_1_SUBCHANNEL_INDEX && subchannelIndex <= AOUT_2_SUBCHANNEL_INDEX) {
            int dac7760ChannelIndex = subchannelIndex - AOUT_1_SUBCHANNEL_INDEX;
            calConf = aoutDac7760Channels[dac7760ChannelIndex].getCalConf();
            calConfIndex = dac7760ChannelIndex * 7 + aoutDac7760Channels[dac7760ChannelIndex].getCalConfIndex();
        } else if (subchannelIndex >= AOUT_3_SUBCHANNEL_INDEX && subchannelIndex <= AOUT_4_SUBCHANNEL_INDEX) {
            calConf = &aoutDac7563Channels[subchannelIndex - AOUT_3_SUBCHANNEL_INDEX].calConf;
            calConfIndex = 2 * 7 + (subchannelIndex - AOUT_3_SUBCHANNEL_INDEX);
        }

        if (calConf) {
            return persist_conf::saveChannelCalibrationConfiguration(slotIndex, calConfIndex, &calConf->header, sizeof(CalConf), CalConf::VERSION);
        }

        if (err) {
            *err = SCPI_ERROR_HARDWARE_MISSING;
        }
        return false;
    }

    void startChannelCalibration(int subchannelIndex) override {
        if (subchannelIndex >= AOUT_1_SUBCHANNEL_INDEX && subchannelIndex <= AOUT_2_SUBCHANNEL_INDEX) {
            auto &channel = aoutDac7760Channels[subchannelIndex - AOUT_1_SUBCHANNEL_INDEX];
            channel.ongoingCal = 1;
            channel.m_mode = dac7760CalibrationChannelMode;
            channel.m_voltageRange = dac7760CalibrationChannelVoltageRange;
            channel.m_currentRange = dac7760CalibrationChannelCurrentRange;
        } else if (subchannelIndex >= AOUT_3_SUBCHANNEL_INDEX && subchannelIndex <= AOUT_4_SUBCHANNEL_INDEX) {
            aoutDac7563Channels[subchannelIndex - AOUT_3_SUBCHANNEL_INDEX].ongoingCal = 1;
        }
    }
    
    void stopChannelCalibration(int subchannelIndex) override {
        if (subchannelIndex >= AOUT_1_SUBCHANNEL_INDEX && subchannelIndex <= AOUT_2_SUBCHANNEL_INDEX) {
            aoutDac7760Channels[subchannelIndex - AOUT_1_SUBCHANNEL_INDEX].ongoingCal = 0;
        } else if (subchannelIndex >= AOUT_3_SUBCHANNEL_INDEX && subchannelIndex <= AOUT_4_SUBCHANNEL_INDEX) {
            aoutDac7563Channels[subchannelIndex - AOUT_3_SUBCHANNEL_INDEX].ongoingCal = 0;
        }
    }

    unsigned int getMaxCalibrationPoints(int subchannelIndex) override {
        return 2;
    }

    CalibrationValueType getCalibrationValueType(int subchannelIndex) override {
        if (subchannelIndex >= AOUT_1_SUBCHANNEL_INDEX && subchannelIndex <= AOUT_2_SUBCHANNEL_INDEX) {
            if (aoutDac7760Channels[subchannelIndex - AOUT_1_SUBCHANNEL_INDEX].getMode() == SOURCE_MODE_CURRENT) {
                return CALIBRATION_VALUE_I_HI_RANGE;
            }
        }
        return CALIBRATION_VALUE_U;
    }

    void getDefaultCalibrationPoints(int subchannelIndex, CalibrationValueType type, unsigned int &numPoints, float *&points) override {
        if (subchannelIndex >= AOUT_1_SUBCHANNEL_INDEX && subchannelIndex <= AOUT_2_SUBCHANNEL_INDEX) {
            aoutDac7760Channels[subchannelIndex - AOUT_1_SUBCHANNEL_INDEX].getDefaultCalibrationPoints(numPoints, points);
        } else if (subchannelIndex >= AOUT_3_SUBCHANNEL_INDEX && subchannelIndex <= AOUT_4_SUBCHANNEL_INDEX) {
            aoutDac7563Channels[subchannelIndex - AOUT_3_SUBCHANNEL_INDEX].getDefaultCalibrationPoints(numPoints, points);
        } else {
            numPoints = 0;
            points = nullptr;
        }
    }

    bool getCalibrationConfiguration(int subchannelIndex, CalibrationConfiguration &calConf, int *err) override {
        CalConf *mioCalConf = nullptr;

        if (subchannelIndex >= AOUT_1_SUBCHANNEL_INDEX && subchannelIndex <= AOUT_2_SUBCHANNEL_INDEX) {
            mioCalConf = aoutDac7760Channels[subchannelIndex - AOUT_1_SUBCHANNEL_INDEX].getCalConf();
        } else if (subchannelIndex >= AOUT_3_SUBCHANNEL_INDEX && subchannelIndex <= AOUT_4_SUBCHANNEL_INDEX) {
            mioCalConf = &aoutDac7563Channels[subchannelIndex - AOUT_3_SUBCHANNEL_INDEX].calConf;
        }

        if (mioCalConf) {
            memset(&calConf, 0, sizeof(CalibrationConfiguration));

            if (mioCalConf->state.calState) {
                bool isVoltage = getCalibrationValueType(subchannelIndex) == CALIBRATION_VALUE_U;

                if (isVoltage) {
                    calConf.u.numPoints = 2;
                } else {
                    calConf.i[0].numPoints = 2;
                }

                for (unsigned i = 0; i < 2; i++) {
                    if (isVoltage) {
                        calConf.u.points[i].dac = mioCalConf->points[i].uncalValue;
                        calConf.u.points[i].value = mioCalConf->points[i].calValue;
                    } else {
                        calConf.i[0].points[i].dac = mioCalConf->points[i].uncalValue;
                        calConf.i[0].points[i].value = mioCalConf->points[i].calValue;
                    }
                }
            }

            calConf.calibrationDate = mioCalConf->calibrationDate;
            strncpy(calConf.calibrationRemark, mioCalConf->calibrationRemark, MIO_CALIBRATION_REMARK_MAX_LENGTH);
            calConf.calibrationRemark[MIO_CALIBRATION_REMARK_MAX_LENGTH] = 0;

            return true;
        }

        if (err) {
            *err = SCPI_ERROR_HARDWARE_MISSING;
        }
        return false;
    }
    
    bool setCalibrationConfiguration(int subchannelIndex, const CalibrationConfiguration &calConf, int *err) override {
        CalConf *mioCalConf = nullptr;

        if (subchannelIndex >= AOUT_1_SUBCHANNEL_INDEX && subchannelIndex <= AOUT_2_SUBCHANNEL_INDEX) {
            mioCalConf = aoutDac7760Channels[subchannelIndex - AOUT_1_SUBCHANNEL_INDEX].getCalConf();
        } else if (subchannelIndex >= AOUT_3_SUBCHANNEL_INDEX && subchannelIndex <= AOUT_4_SUBCHANNEL_INDEX) {
            mioCalConf = &aoutDac7563Channels[subchannelIndex - AOUT_3_SUBCHANNEL_INDEX].calConf;
        }

        if (mioCalConf) {
            memset(mioCalConf, 0, sizeof(CalConf));

            bool isVoltage = getCalibrationValueType(subchannelIndex) == CALIBRATION_VALUE_U;

            for (unsigned i = 0; i < 2; i++) {
                if (isVoltage) {
                    mioCalConf->points[i].uncalValue = calConf.u.points[i].dac;
                    mioCalConf->points[i].calValue = calConf.u.points[i].value;
                } else {
                    mioCalConf->points[i].uncalValue = calConf.i[0].points[i].dac;
                    mioCalConf->points[i].calValue = calConf.i[0].points[i].value;
                }
            }

            mioCalConf->state.calState = calConf.u.numPoints == 2;
            mioCalConf->state.calEnabled = mioCalConf->state.calState;

            mioCalConf->calibrationDate = calConf.calibrationDate;
            strncpy(mioCalConf->calibrationRemark, calConf.calibrationRemark, MIO_CALIBRATION_REMARK_MAX_LENGTH);
            mioCalConf->calibrationRemark[MIO_CALIBRATION_REMARK_MAX_LENGTH] = 0;

            return true;
        }

        if (err) {
            *err = SCPI_ERROR_HARDWARE_MISSING;
        }
        return false;
    }

    bool getCalibrationRemark(int subchannelIndex, const char *&calibrationRemark, int *err) override {
        CalConf *mioCalConf = nullptr;

        if (subchannelIndex >= AOUT_1_SUBCHANNEL_INDEX && subchannelIndex <= AOUT_2_SUBCHANNEL_INDEX) {
            mioCalConf = aoutDac7760Channels[subchannelIndex - AOUT_1_SUBCHANNEL_INDEX].getCalConf();
        } else if (subchannelIndex >= AOUT_3_SUBCHANNEL_INDEX && subchannelIndex <= AOUT_4_SUBCHANNEL_INDEX) {
            mioCalConf = &aoutDac7563Channels[subchannelIndex - AOUT_3_SUBCHANNEL_INDEX].calConf;
        }

        if (mioCalConf) {
            calibrationRemark = mioCalConf->calibrationRemark;
        } else {
            calibrationRemark = "";
        }

        return true;
    }

    bool getCalibrationDate(int subchannelIndex, uint32_t &calibrationDate, int *err) override {
        CalConf *mioCalConf = nullptr;

        if (subchannelIndex >= AOUT_1_SUBCHANNEL_INDEX && subchannelIndex <= AOUT_2_SUBCHANNEL_INDEX) {
            mioCalConf = aoutDac7760Channels[subchannelIndex - AOUT_1_SUBCHANNEL_INDEX].getCalConf();
        } else if (subchannelIndex >= AOUT_3_SUBCHANNEL_INDEX && subchannelIndex <= AOUT_4_SUBCHANNEL_INDEX) {
            mioCalConf = &aoutDac7563Channels[subchannelIndex - AOUT_3_SUBCHANNEL_INDEX].calConf;
        }

        if (mioCalConf) {
            calibrationDate = mioCalConf->calibrationDate;
        } else {
            calibrationDate = 0;
        }
        
        return true;
    }

    bool getCurrent(int subchannelIndex, float &value, int *err) override {
        if (subchannelIndex < AOUT_1_SUBCHANNEL_INDEX || subchannelIndex > AOUT_2_SUBCHANNEL_INDEX) {
            if (err) {
                *err = SCPI_ERROR_HARDWARE_MISSING;
            }
            return false;
        }

        value = aoutDac7760Channels[subchannelIndex - AOUT_1_SUBCHANNEL_INDEX].m_currentValue;

        return true;
    }

    bool setCurrent(int subchannelIndex, float value, int *err) override {
        if (subchannelIndex < AOUT_1_SUBCHANNEL_INDEX || subchannelIndex > AOUT_2_SUBCHANNEL_INDEX) {
            if (err) {
                *err = SCPI_ERROR_HARDWARE_MISSING;
            }
            return false;
        }

        aoutDac7760Channels[subchannelIndex - AOUT_1_SUBCHANNEL_INDEX].m_currentValue = value;

        return true;
    }

    bool getMeasuredCurrent(int subchannelIndex, float &value, int *err) override {
        if (subchannelIndex < AIN_1_SUBCHANNEL_INDEX || subchannelIndex > AIN_4_SUBCHANNEL_INDEX) {
            if (err) {
                *err = SCPI_ERROR_HARDWARE_MISSING;
            }
            return false;
        }

        auto &channel = ainChannels[subchannelIndex - AIN_1_SUBCHANNEL_INDEX];

        if (channel.m_mode != MEASURE_MODE_CURRENT) {
            if (err) {
                *err = SCPI_ERROR_EXECUTION_ERROR;
            }
            return false;
        }

        value = channel.m_value;
        return true;
    }

    void getCurrentStepValues(int subchannelIndex, StepValues *stepValues, bool calibrationMode) override {
        if (subchannelIndex >= AOUT_1_SUBCHANNEL_INDEX && subchannelIndex <= AOUT_2_SUBCHANNEL_INDEX) {
            aoutDac7760Channels[subchannelIndex - AOUT_1_SUBCHANNEL_INDEX].getStepValues(stepValues);
            if (calibrationMode) {
                stepValues->encoderSettings.step /= 10.0f;
                stepValues->encoderSettings.range = stepValues->encoderSettings.step * 10.0f;
            }
        }
    }
    
    float getCurrentResolution(int subchannelIndex) override {
        if (subchannelIndex >= AOUT_1_SUBCHANNEL_INDEX && subchannelIndex <= AOUT_2_SUBCHANNEL_INDEX) {
            return aoutDac7760Channels[subchannelIndex - AOUT_1_SUBCHANNEL_INDEX].getCurrentResolution();
        }
        return 0.0f;
    }

    float getCurrentMinValue(int subchannelIndex) override {
        if (subchannelIndex >= AOUT_1_SUBCHANNEL_INDEX && subchannelIndex <= AOUT_2_SUBCHANNEL_INDEX) {
            return aoutDac7760Channels[subchannelIndex - AOUT_1_SUBCHANNEL_INDEX].getCurrentMinValue();
        }
        return 0.0f;
    }

    float getCurrentMaxValue(int subchannelIndex) override {
        if (subchannelIndex >= AOUT_1_SUBCHANNEL_INDEX && subchannelIndex <= AOUT_2_SUBCHANNEL_INDEX) {
            return aoutDac7760Channels[subchannelIndex - AOUT_1_SUBCHANNEL_INDEX].getCurrentMaxValue();
        }
        return 0.0f;    
    }

    bool isCurrentCalibrationExists(int subchannelIndex) override {
        return false;
    }

    bool isCurrentCalibrationEnabled(int subchannelIndex) override {
        return false;
    }
    
    void enableCurrentCalibration(int subchannelIndex, bool enabled) override {
    }

    int getNumDlogResources(int subchannelIndex) override {
		if (subchannelIndex == DIN_SUBCHANNEL_INDEX) {
            return 8;
        }
        if (subchannelIndex >= AIN_1_SUBCHANNEL_INDEX && subchannelIndex <= AIN_4_SUBCHANNEL_INDEX) {
            return 1;
        }
        return 0;
	}
    
	DlogResourceType getDlogResourceType(int subchannelIndex, int resourceIndex) override {
        if (subchannelIndex == DIN_SUBCHANNEL_INDEX) {
            return DlogResourceType(DLOG_RESOURCE_TYPE_DIN0 + resourceIndex);
        }
        if (subchannelIndex >= AIN_1_SUBCHANNEL_INDEX && subchannelIndex <= AIN_4_SUBCHANNEL_INDEX) {
            return DLOG_RESOURCE_TYPE_U;
        }
        return DLOG_RESOURCE_TYPE_NONE;
	}
    
	const char *getDlogResourceLabel(int subchannelIndex, int resourceIndex) override {
        if (subchannelIndex == DIN_SUBCHANNEL_INDEX) {
            return getPinLabelOrDefault(subchannelIndex, resourceIndex);
        }
        
        if (subchannelIndex >= AIN_1_SUBCHANNEL_INDEX && subchannelIndex <= AIN_4_SUBCHANNEL_INDEX) {
            const char *label = getChannelLabel(subchannelIndex);
            if (*label) {
                return label;
            }
            return getDefaultChannelLabel(subchannelIndex);
        }

        return "";
    }

    float getDlogResourceMinPeriod(int subchannelIndex, int resourceIndex) override {
        if (subchannelIndex == DIN_SUBCHANNEL_INDEX) {
            return 0.00001f; // 10 us
        }
        return Module::getDlogResourceMinPeriod(subchannelIndex, resourceIndex);
    }

    void onStartDlog() override {
        if (dlog_record::isModuleLocalRecording()) {
           uint32_t resources = 0;

           for (int i = 0; i < dlog_record::g_recording.parameters.numDlogItems; i++) {
               auto &dlogItem = dlog_record::g_recording.parameters.dlogItems[i];
               if (dlogItem.slotIndex == slotIndex) {
                   if (dlogItem.subchannelIndex == DIN_SUBCHANNEL_INDEX) {
                       resources |= 1 << dlogItem.resourceIndex;
                   } else if (dlogItem.subchannelIndex == DOUT_SUBCHANNEL_INDEX) {
                       resources |= 1 << (8 + dlogItem.resourceIndex);
                   } else if (
                       dlogItem.subchannelIndex >= AIN_1_SUBCHANNEL_INDEX &&
                       dlogItem.subchannelIndex <= AIN_4_SUBCHANNEL_INDEX
                   ) {
                       resources |= 1 << (16 + (dlogItem.subchannelIndex - AIN_1_SUBCHANNEL_INDEX));
                   }
               }
           }

            if (resources != 0) {
                nextDlogRecordingStart.duration = dlog_record::g_parameters.duration;
                nextDlogRecordingStart.period = dlog_record::g_parameters.period;
                nextDlogRecordingStart.resources = resources;

                strcpy(nextDlogRecordingStart.filePath, dlog_record::g_parameters.filePath + 2 /* skip drive part */ );

                for (int i = 0; i < 8; i++) {
                    strcpy(nextDlogRecordingStart.dinLabels + i * (CHANNEL_LABEL_MAX_LENGTH + 1), getDlogResourceLabel(DIN_SUBCHANNEL_INDEX, i));
                }

                for (int i = 0; i < 8; i++) {
                    strcpy(nextDlogRecordingStart.doutLabels + i * (CHANNEL_LABEL_MAX_LENGTH + 1), getDlogResourceLabel(DOUT_SUBCHANNEL_INDEX, i));
                }

                for (int i = 0; i < 4; i++) {
                    strcpy(nextDlogRecordingStart.ainLabels + i * (CHANNEL_LABEL_MAX_LENGTH + 1), getDlogResourceLabel(AIN_1_SUBCHANNEL_INDEX + i, 0));
                }

                nextDlogCommand = &dlogRecordingStart_command;
            }
        }
    }

    void onStopDlog() override {
        nextDlogCommand = &dlogRecordingStop_command;
    }

    // These are executed from the low priority thread which is solely in charge of disk operations.
    void executeDiskDriveOperation() {
        for (int nretry = 0; nretry < 10; nretry++) {
            diskOperationStatus = Mio168Module::DISK_OPERATION_NOT_FINISHED;

            sendMessageToPsu((HighPriorityThreadMessage)PSU_MESSAGE_DISK_DRIVE_OPERATION, slotIndex);

            while (diskOperationStatus == Mio168Module::DISK_OPERATION_NOT_FINISHED) {
                osDelay(0);
            }

            if (diskOperationStatus == Mio168Module::DISK_OPERATION_SUCCESSFULLY_FINISHED) {
                break;
            }
        }

        diskOperationStatus = Mio168Module::DISK_OPERATION_IDLE;
    }

    int diskDriveInitialize() override {
        diskOperationParams.command = &diskDriveInitialize_command;
        executeDiskDriveOperation();
        return diskOperationParams.result;
    }

    int diskDriveStatus() override {
        diskOperationParams.command = &diskDriveStatus_command;
        executeDiskDriveOperation();
        return diskOperationParams.result;
    }

    int diskDriveRead(uint8_t *buff, uint32_t sector) override {
        diskOperationParams.command = &diskDriveRead_command;
        diskOperationParams.buff = buff;
        diskOperationParams.sector = sector;
        executeDiskDriveOperation();
        return diskOperationParams.result;
    }
    
    int diskDriveWrite(uint8_t *buff, uint32_t sector) override {
        diskOperationParams.command = &diskDriveWrite_command;
        diskOperationParams.buff = buff;
        diskOperationParams.sector = sector;
        executeDiskDriveOperation();
        return diskOperationParams.result;
    }
    
    int diskDriveIoctl(uint8_t cmd, void *buff) override {
        diskOperationParams.command = &diskDriveIoctl_command;
        diskOperationParams.cmd = cmd;
        diskOperationParams.buff = (uint8_t *)buff;
        executeDiskDriveOperation();
        return diskOperationParams.result;
    }
};

////////////////////////////////////////////////////////////////////////////////

const Mio168Module::CommandDef Mio168Module::getInfo_command = {
	COMMAND_GET_INFO,
	nullptr,
	&Mio168Module::Command_GetInfo_Done
};

const Mio168Module::CommandDef Mio168Module::getState_command = {
	COMMAND_GET_STATE,
	&Mio168Module::Command_GetState_FillRequest,
	&Mio168Module::Command_GetState_Done
};

const Mio168Module::CommandDef Mio168Module::setParams_command = {
	COMMAND_SET_PARAMS,
	&Mio168Module::Command_SetParams_FillRequest,
	&Mio168Module::Command_SetParams_Done
};

const Mio168Module::CommandDef Mio168Module::dlogRecordingStart_command = {
	COMMAND_DLOG_RECORDING_START,
	&Mio168Module::Command_DlogRecordingStart_FillRequest,
	&Mio168Module::Command_DlogRecordingStart_Done
};

const Mio168Module::CommandDef Mio168Module::dlogRecordingStop_command = {
	COMMAND_DLOG_RECORDING_STOP,
	nullptr,
	&Mio168Module::Command_DlogRecordingStop_Done
};

const Mio168Module::CommandDef Mio168Module::diskDriveInitialize_command = {
	COMMAND_DISK_DRIVE_INITIALIZE,
	nullptr,
	&Mio168Module::Command_DiskDriveInitialize_Done
};

const Mio168Module::CommandDef Mio168Module::diskDriveStatus_command = {
	COMMAND_DISK_DRIVE_STATUS,
	nullptr,
	&Mio168Module::Command_DiskDriveStatus_Done
};

const Mio168Module::CommandDef Mio168Module::diskDriveRead_command = {
	COMMAND_DISK_DRIVE_READ,
	&Mio168Module::Command_DiskDriveRead_FillRequest,
	&Mio168Module::Command_DiskDriveRead_Done
};

const Mio168Module::CommandDef Mio168Module::diskDriveWrite_command = {
	COMMAND_DISK_DRIVE_WRITE,
	&Mio168Module::Command_DiskDriveWrite_FillRequest,
	&Mio168Module::Command_DiskDriveWrite_Done
};

const Mio168Module::CommandDef Mio168Module::diskDriveIoctl_command = {
	COMMAND_DISK_DRIVE_IOCTL,
	&Mio168Module::Command_DiskDriveIoctl_FillRequest,
	&Mio168Module::Command_DiskDriveIoctl_Done
};

////////////////////////////////////////////////////////////////////////////////

static Mio168Module g_mio168Module;
Module *g_module = &g_mio168Module;

////////////////////////////////////////////////////////////////////////////////

class DinConfigurationPage : public SetPage {
public:
    static int g_selectedChannelIndex;

    void pageAlloc() {
        Mio168Module *module = (Mio168Module *)g_slots[hmi::g_selectedSlotIndex];

        m_pinRanges = m_pinRangesOrig = module->dinChannel.m_pinRanges;
        m_pinSpeeds = m_pinSpeedsOrig = module->dinChannel.m_pinSpeeds;
    }

    int getDirty() { 
        return m_pinRanges != m_pinRangesOrig || m_pinSpeeds != m_pinSpeedsOrig;
    }

    void set() {
        if (getDirty()) {
            sendMessageToPsu((HighPriorityThreadMessage)PSU_MESSAGE_DIN_CONFIGURE, hmi::g_selectedSlotIndex);
        }

        popPage();
    }

    int getPinRange(int pin) {
        return m_pinRanges & (1 << pin) ? 1 : 0;
    }
    
    void setPinRange(int pin, int value) {
        if (value) {
            m_pinRanges |= 1 << pin;
        } else {
            m_pinRanges &= ~(1 << pin);
        }
    }

    int getPinSpeed(int pin) {
        return m_pinSpeeds & (1 << pin) ? 1 : 0;
    }

    void setPinSpeed(int pin, int value) {
        if (value) {
            m_pinSpeeds |= 1 << pin;
        } else {
            m_pinSpeeds &= ~(1 << pin);
        }
    }

    uint8_t m_pinRanges;
    uint8_t m_pinSpeeds;

private:
    uint8_t m_pinRangesOrig;
    uint8_t m_pinSpeedsOrig;
};

static DinConfigurationPage g_dinConfigurationPage;

////////////////////////////////////////////////////////////////////////////////

class AinConfigurationPage : public SetPage {
public:
    static int g_selectedChannelIndex;

    void pageAlloc() {
        Mio168Module *module = (Mio168Module *)g_slots[hmi::g_selectedSlotIndex];
        AinChannel &channel = module->ainChannels[g_selectedChannelIndex - AIN_1_SUBCHANNEL_INDEX];

        m_mode = m_modeOrig = channel.m_mode;
        m_range = m_rangeOrig = channel.m_range;
    }

    int getDirty() { 
        return m_mode != m_modeOrig || m_range != m_rangeOrig;
    }

    void set() {
        if (getDirty()) {
            sendMessageToPsu((HighPriorityThreadMessage)PSU_MESSAGE_AIN_CONFIGURE, hmi::g_selectedSlotIndex);
        }

        popPage();
    }

    EnumItem *getRangeEnumDefinition() {
		static EnumItem g_ain12VoltageRangeEnumDefinition[] = {
			{ 0, "\xbd""2.4 V" },
			{ 1, "\xbd""48 V" },
			{ 2, "\xbd""240 V" },
			{ 0, 0 }
		};

		static EnumItem g_ain34VoltageRangeEnumDefinition[] = {
			{ 0, "\xbd""2.4 V" },
			{ 1, "\xbd""12 V" },
			{ 0, 0 }
		};

		static EnumItem g_ain34CurrentRangeEnumDefinition[] = {
			{ 0, "\xbd""24 mA" },
			{ 1, "\xbd""1.2 A" },
			{ 2, "\xbd""10 A" },
			{ 0, 0 }
		};

		if (g_selectedChannelIndex == AIN_1_SUBCHANNEL_INDEX || g_selectedChannelIndex == AIN_2_SUBCHANNEL_INDEX) {
            return m_mode == MEASURE_MODE_VOLTAGE ? g_ain12VoltageRangeEnumDefinition : nullptr;
        } else {
			return m_mode == MEASURE_MODE_VOLTAGE ? g_ain34VoltageRangeEnumDefinition : 
				m_mode == MEASURE_MODE_CURRENT ? g_ain34CurrentRangeEnumDefinition : nullptr;
        }
    }

    uint8_t m_mode;
    uint8_t m_range;

private:
    uint8_t m_modeOrig;
    uint8_t m_rangeOrig;
};

int AinConfigurationPage::g_selectedChannelIndex;
static AinConfigurationPage g_ainConfigurationPage;

////////////////////////////////////////////////////////////////////////////////

class AoutDac7760ConfigurationPage : public SetPage {
public:
    static int g_selectedChannelIndex;

    void pageAlloc() {
        Mio168Module *module = (Mio168Module *)g_slots[hmi::g_selectedSlotIndex];
        AoutDac7760Channel &channel = module->aoutDac7760Channels[g_selectedChannelIndex - AOUT_1_SUBCHANNEL_INDEX];

        m_outputEnabled = m_outputEnabledOrig = channel.m_outputEnabled;
        m_mode = m_modeOrig = channel.m_mode;
        m_currentRange = m_currentRangeOrig = channel.m_currentRange;
        m_voltageRange = m_voltageRangeOrig = channel.m_voltageRange;
    }

    int getDirty() { 
        return m_outputEnabled != m_outputEnabledOrig ||
            m_mode != m_modeOrig ||
            m_currentRange != m_currentRangeOrig ||
            m_voltageRange != m_voltageRangeOrig;
    }

    void set() {
        if (getDirty()) {
            sendMessageToPsu((HighPriorityThreadMessage)PSU_MESSAGE_AOUT_DAC7760_CONFIGURE, hmi::g_selectedSlotIndex);
        }

        popPage();
    }

    bool m_outputEnabled;
    uint8_t m_mode;
    uint8_t m_currentRange;
    uint8_t m_voltageRange;

private:
    bool m_outputEnabledOrig;
    uint8_t m_modeOrig;
    uint8_t m_currentRangeOrig;
    uint8_t m_voltageRangeOrig;
};

int AoutDac7760ConfigurationPage::g_selectedChannelIndex;
static AoutDac7760ConfigurationPage g_aoutDac7760ConfigurationPage;

////////////////////////////////////////////////////////////////////////////////

class AoutDac7563ConfigurationPage : public SetPage {
public:
    static int g_selectedChannelIndex;

    void pageAlloc() {
    }

    int getDirty() { 
        return false;
    }

    void set() {
        popPage();
    }

private:
};

int AoutDac7563ConfigurationPage::g_selectedChannelIndex;
static AoutDac7563ConfigurationPage g_aoutDac7563ConfigurationPage;

////////////////////////////////////////////////////////////////////////////////

Page *Mio168Module::getPageFromId(int pageId) {
    if (pageId == PAGE_ID_DIB_MIO168_DIN_CONFIGURATION) {
        return &g_dinConfigurationPage;
    } else if (pageId == PAGE_ID_DIB_MIO168_AIN_CONFIGURATION) {
        return &g_ainConfigurationPage;
    } else if (pageId == PAGE_ID_DIB_MIO168_AOUT_DAC7760_CONFIGURATION) {
        return &g_aoutDac7760ConfigurationPage;
    } else if (pageId == PAGE_ID_DIB_MIO168_AOUT_DAC7563_CONFIGURATION) {
        return &g_aoutDac7563ConfigurationPage;
    }
    return nullptr;
}

void Mio168Module::onHighPriorityThreadMessage(uint8_t type, uint32_t param) {
    if (type == PSU_MESSAGE_DIN_CONFIGURE) {
        dinChannel.m_pinRanges = g_dinConfigurationPage.m_pinRanges;
        dinChannel.m_pinSpeeds = g_dinConfigurationPage.m_pinSpeeds;
    } else if (type == PSU_MESSAGE_AIN_CONFIGURE) {
        AinChannel &channel = ainChannels[AinConfigurationPage::g_selectedChannelIndex - AIN_1_SUBCHANNEL_INDEX];
        channel.m_mode = g_ainConfigurationPage.m_mode;
        channel.m_range = g_ainConfigurationPage.m_range;
    } else if (type == PSU_MESSAGE_AOUT_DAC7760_CONFIGURE) {
        AoutDac7760Channel &channel = aoutDac7760Channels[AoutDac7760ConfigurationPage::g_selectedChannelIndex - AOUT_1_SUBCHANNEL_INDEX];
        channel.m_outputEnabled = g_aoutDac7760ConfigurationPage.m_outputEnabled;
        channel.m_mode = g_aoutDac7760ConfigurationPage.m_mode;
        channel.m_currentRange = g_aoutDac7760ConfigurationPage.m_currentRange;
        channel.m_voltageRange = g_aoutDac7760ConfigurationPage.m_voltageRange;

        if (channel.getValue() < channel.getMinValue()) {
            channel.setValue(channel.getMinValue());
        } else if (channel.getValue() > channel.getMaxValue()) {
            channel.setValue(channel.getMaxValue());
        }
    } 
	else if (type == PSU_MESSAGE_DISK_DRIVE_OPERATION) {
        tick();
    }
}

} // namespace dib_mio168

namespace gui {

using namespace dib_mio168;

////////////////////////////////////////////////////////////////////////////////

const char *getPinLabel(int subchannelIndex, Cursor cursor) {
    int slotIndex = cursor / 8;
    int pinIndex = cursor % 8;

    if (isPageOnStack(PAGE_ID_SYS_SETTINGS_LABELS_AND_COLORS)) {
        const char *label = LabelsAndColorsPage::getChannelLabel(slotIndex, subchannelIndex);
        const char *pinLabel = label + pinIndex * (CHANNEL_LABEL_MAX_LENGTH + 1);
        if (!*pinLabel) {
            pinLabel = g_slots[slotIndex]->getDefaultChannelLabel(subchannelIndex) + pinIndex * (CHANNEL_LABEL_MAX_LENGTH + 1);
        }
        return pinLabel;
    } else {
        return ((Mio168Module *)g_slots[slotIndex])->getPinLabelOrDefault(subchannelIndex, pinIndex);
    }
}

const char *getChannelLabel(int slotIndex, int subchannelIndex) {
    auto module = (Mio168Module *)g_slots[slotIndex];
    const char *label = module->getChannelLabel(subchannelIndex);
    if (*label) {
        return label;
    }
    return module->getDefaultChannelLabel(subchannelIndex);
}

////////////////////////////////////////////////////////////////////////////////

void data_dib_mio168_din_pins(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_COUNT) {
        value = 8;
    } else if (operation == DATA_OPERATION_GET_CURSOR_VALUE) {
        value = hmi::g_selectedSlotIndex * 8 + value.getInt();
    }
}

void data_dib_mio168_din_pins_1_4(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_COUNT) {
        value = 4;
    } else if (operation == DATA_OPERATION_GET_CURSOR_VALUE) {
        value = hmi::g_selectedSlotIndex * 8 + value.getInt();
    }
}

void data_dib_mio168_din_pins_5_8(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_COUNT) {
        value = 4;
    } else if (operation == DATA_OPERATION_GET_CURSOR_VALUE) {
        value = hmi::g_selectedSlotIndex * 8 + 4 + value.getInt();
    }
}

void data_dib_mio168_din_no(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = cursor % 8 + 1;
    } else if (operation == DATA_OPERATION_GET_COLOR) {
        if (!dlog_record::isIdle() && dlog_record::g_recording.parameters.isDlogItemEnabled(cursor / 8, DIN_SUBCHANNEL_INDEX, (DlogResourceType)(DLOG_RESOURCE_TYPE_DIN0 + cursor % 8))) {
            value = Value(COLOR_ID_DATA_LOGGING, VALUE_TYPE_UINT16);
        }
    }
}

void data_dib_mio168_din_state(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        auto mio168Module = (Mio168Module *)g_slots[cursor / 8];
        value = mio168Module->dinChannel.getPinState(cursor % 8) ? 1 : 0;
    }
}

void data_dib_mio168_din_range(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = g_dinConfigurationPage.getPinRange(cursor % 8);
    }
}

void action_dib_mio168_din_select_range() {
    uint8_t pin = getFoundWidgetAtDown().cursor % 8;
    g_dinConfigurationPage.setPinRange(pin, g_dinConfigurationPage.getPinRange(pin) ? 0 : 1);
}

void data_dib_mio168_din_has_speed(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = cursor % 8 < 2;
    }
}

void data_dib_mio168_din_speed(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = g_dinConfigurationPage.getPinSpeed(cursor % 8);
    }
}

void data_dib_mio168_din_label(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = getPinLabel(DIN_SUBCHANNEL_INDEX, cursor);
    }
}

void data_dib_mio168_din_label_label(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        static const char *g_dinLabelLabels[8] = {
            "DIN1 label:", "DIN2 label:", "DIN3 label:", "DIN4 label:",
            "DIN5 label:", "DIN6 label:", "DIN7 label:", "DIN8 label:",
        };
        value = g_dinLabelLabels[cursor % 8];

    }
}

void action_dib_mio168_din_select_speed() {
    uint8_t pin = getFoundWidgetAtDown().cursor % 8;
    g_dinConfigurationPage.setPinSpeed(pin, g_dinConfigurationPage.getPinSpeed(pin) ? 0 : 1);
}

void action_dib_mio168_din_show_configuration() {
    if (getActivePageId() == PAGE_ID_MAIN) {
        hmi::selectSlot(getFoundWidgetAtDown().cursor);
    }
    pushPage(PAGE_ID_DIB_MIO168_DIN_CONFIGURATION);
}

void action_dib_mio168_show_din_channel_labels() {
    pushPage(PAGE_ID_DIB_MIO168_DIN_CHANNEL_LABELS);
}

static int g_editLabelSlotIndex;
static int g_editSubchannelIndex;
static int g_editLabelPinIndex;

void onSetPinLabel(char *value) {
    const char *label = LabelsAndColorsPage::getChannelLabel(g_editLabelSlotIndex, g_editSubchannelIndex);
    char pinLabels[8 * (CHANNEL_LABEL_MAX_LENGTH + 1)];
    memcpy(pinLabels, label, 8 * (CHANNEL_LABEL_MAX_LENGTH + 1));

    strcpy(pinLabels + g_editLabelPinIndex * (CHANNEL_LABEL_MAX_LENGTH + 1), value);
    
    LabelsAndColorsPage::setChannelLabel(g_editLabelSlotIndex, g_editSubchannelIndex, pinLabels);
    
    popPage();
}

void onSetPinDefaultLabel() {
    const char *label = LabelsAndColorsPage::getChannelLabel(g_editLabelSlotIndex, g_editSubchannelIndex);
    char pinLabels[8 * (CHANNEL_LABEL_MAX_LENGTH + 1)];
    memcpy(pinLabels, label, 8 * (CHANNEL_LABEL_MAX_LENGTH + 1));

    *(pinLabels + g_editLabelPinIndex * (CHANNEL_LABEL_MAX_LENGTH + 1)) = 0;

    LabelsAndColorsPage::setChannelLabel(g_editLabelSlotIndex, g_editSubchannelIndex, pinLabels);
    popPage();
}

void editPinLabel(int subchannelIndex) {
    int cursor = getFoundWidgetAtDown().cursor;
    g_editLabelSlotIndex = cursor / 8;
    g_editSubchannelIndex = subchannelIndex;
    g_editLabelPinIndex = cursor % 8;

    const char *label = LabelsAndColorsPage::getChannelLabel(g_editLabelSlotIndex, g_editSubchannelIndex);
    const char *pinLabel = label + g_editLabelPinIndex * (CHANNEL_LABEL_MAX_LENGTH + 1);
    const char *pinLabelOrDefault = pinLabel;
    if (!*pinLabelOrDefault) {
        pinLabelOrDefault = g_slots[g_editLabelSlotIndex]->getDefaultChannelLabel(g_editSubchannelIndex) + g_editLabelPinIndex * (CHANNEL_LABEL_MAX_LENGTH + 1);
    }

    Keypad::startPush(0, 
        pinLabelOrDefault,
        1, CHANNEL_LABEL_MAX_LENGTH,
        false, 
        onSetPinLabel, popPage, 
        *pinLabel ? onSetPinDefaultLabel : nullptr);
}

void action_dib_mio168_change_din_label() {
    editPinLabel(DIN_SUBCHANNEL_INDEX);
}

////////////////////////////////////////////////////////////////////////////////

void data_dib_mio168_dout_pins(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_COUNT) {
        value = 8;
    } else if (operation == DATA_OPERATION_GET_CURSOR_VALUE) {
        value = hmi::g_selectedSlotIndex * 8 + value.getInt();
    }
}

void data_dib_mio168_dout_pins_1_4(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_COUNT) {
        value = 4;
    } else if (operation == DATA_OPERATION_GET_CURSOR_VALUE) {
        value = hmi::g_selectedSlotIndex * 8 + value.getInt();
    }
}

void data_dib_mio168_dout_pins_5_8(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_COUNT) {
        value = 4;
    } else if (operation == DATA_OPERATION_GET_CURSOR_VALUE) {
        value = hmi::g_selectedSlotIndex * 8 + 4 + value.getInt();
    }
}

void data_dib_mio168_dout_no(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = cursor % 8 + 1;
    }
}

void data_dib_mio168_dout_state(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        auto mio168Module = (Mio168Module *)g_slots[cursor / 8];
        value = mio168Module->doutChannel.getPinState(cursor % 8);
    }
}

void data_dib_mio168_dout_label(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = getPinLabel(DOUT_SUBCHANNEL_INDEX, cursor);
    }
}

void data_dib_mio168_dout_label_label(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        static const char *g_doutLabelLabels[8] = {
            "DOUT1 label:", "DOUT2 label:", "DOUT3 label:", "DOUT4 label:",
            "DOUT5 label:", "DOUT6 label:", "DOUT7 label:", "DOUT8 label:",
        };
        value = g_doutLabelLabels[cursor % 8];
    }
}

void action_dib_mio168_dout_toggle_state() {
    int cursor = getFoundWidgetAtDown().cursor;
    auto mio168Module = (Mio168Module *)g_slots[cursor / 8];
    int pin = cursor % 8;
    mio168Module->doutChannel.setPinState(pin, !mio168Module->doutChannel.getPinState(pin));
}

void action_dib_mio168_show_dout_channel_labels() {
    pushPage(PAGE_ID_DIB_MIO168_DOUT_CHANNEL_LABELS);
}

void action_dib_mio168_change_dout_label() {
    editPinLabel(DOUT_SUBCHANNEL_INDEX);
}

////////////////////////////////////////////////////////////////////////////////

void data_dib_mio168_ain_channels(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_COUNT) {
        value = 4;
    } else if (operation == DATA_OPERATION_GET_CURSOR_VALUE) {
        value = hmi::g_selectedSlotIndex * 4 + value.getInt();
    }
}

void data_dib_mio168_ain_label(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        int slotIndex = cursor / 4;
        int subchannelIndex = cursor % 4;

        if (isPageOnStack(PAGE_ID_SYS_SETTINGS_LABELS_AND_COLORS)) {
            const char *label = LabelsAndColorsPage::getChannelLabel(slotIndex, AIN_1_SUBCHANNEL_INDEX + subchannelIndex);
            if (*label) {
                value = label;
            } else {
                value = g_slots[slotIndex]->getDefaultChannelLabel(AIN_1_SUBCHANNEL_INDEX + subchannelIndex);
            }            
        } else {
            int ainChannelIndex;
            AinConfigurationPage *page = (AinConfigurationPage *)getPage(PAGE_ID_DIB_MIO168_AIN_CONFIGURATION);
            if (page) {
				slotIndex = hmi::g_selectedSlotIndex;
                ainChannelIndex = page->g_selectedChannelIndex - AIN_1_SUBCHANNEL_INDEX;
            } else {
                ainChannelIndex = subchannelIndex;
            }

            value = getChannelLabel(slotIndex, AIN_1_SUBCHANNEL_INDEX + ainChannelIndex);
        }
    }
}

void data_dib_mio168_ain_label_label(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        static const char *g_inLabelLabels[4] = { "AIN1 label:", "AIN2 label:", "AIN3 label:", "AIN4 label:" };
        value = g_inLabelLabels[cursor % 4];

    }
}

void data_dib_mio168_ain_value(DataOperationEnum operation, Cursor cursor, Value &value) {
    int slotIndex = cursor / 4;
    int ainChannelIndex = cursor % 4;
    if (operation == DATA_OPERATION_GET) {
        auto mio168Module = (Mio168Module *)g_slots[slotIndex];
        auto &channel = mio168Module->ainChannels[ainChannelIndex];
        if (channel.m_mode == MEASURE_MODE_OPEN) {
            value = "-";
        } else {
            value = MakeValue(roundPrec(channel.m_value, channel.getResolution()), channel.m_mode == MEASURE_MODE_VOLTAGE ? UNIT_VOLT : UNIT_AMPER);
        }
    } else if (operation == DATA_OPERATION_GET_BACKGROUND_COLOR) {
        if (!dlog_record::isIdle() && dlog_record::g_recording.parameters.isDlogItemEnabled(slotIndex, AIN_1_SUBCHANNEL_INDEX + ainChannelIndex, DLOG_RESOURCE_TYPE_U)) {
            value = Value(COLOR_ID_DATA_LOGGING, VALUE_TYPE_UINT16);
        }
    } 
}

static EnumItem g_ainModeEnumDefinition[] = {
    { MEASURE_MODE_CURRENT, "Current" },
    { MEASURE_MODE_VOLTAGE, "Voltage" },
    { MEASURE_MODE_OPEN, "Open" },
    { 0, 0 }
};

void data_dib_mio168_ain_mode(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = g_ainConfigurationPage.m_mode;
    }
}

void onSetAinMode(uint16_t value) {
    popPage();
    g_ainConfigurationPage.m_mode = (uint8_t)value;
    if (g_ainConfigurationPage.m_range > AinChannel::getRangeMaxValue(g_ainConfigurationPage.g_selectedChannelIndex, g_ainConfigurationPage.m_mode)) {
        g_ainConfigurationPage.m_range = 0;
    }
}

void action_dib_mio168_ain_select_mode() {
    pushSelectFromEnumPage(g_ainModeEnumDefinition, g_ainConfigurationPage.m_mode, nullptr, onSetAinMode);
}

void data_dib_mio168_ain_range_is_available(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = g_ainConfigurationPage.getRangeEnumDefinition() ? 1 : 0;
    }
}

void data_dib_mio168_ain_range(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = getWidgetLabel(g_ainConfigurationPage.getRangeEnumDefinition(), g_ainConfigurationPage.m_range);
    }
}

void onSetAinRange(uint16_t value) {
    popPage();
    g_ainConfigurationPage.m_range = (uint8_t)value;
}

void action_dib_mio168_ain_select_range() {
    pushSelectFromEnumPage(g_ainConfigurationPage.getRangeEnumDefinition(), g_ainConfigurationPage.m_range, nullptr, onSetAinRange);
}

void action_dib_mio168_ain_show_configuration() {
    int cursor = getFoundWidgetAtDown().cursor;
    
    int slotIndex = cursor / 4;
    hmi::selectSlot(slotIndex);
    
    int ainChannelIndex = cursor % 4;
    AinConfigurationPage::g_selectedChannelIndex = AIN_1_SUBCHANNEL_INDEX + ainChannelIndex;
    pushPage(PAGE_ID_DIB_MIO168_AIN_CONFIGURATION);
}

void action_dib_mio168_ain_change_label() {
    int cursor = getFoundWidgetAtDown().cursor;
    int slotIndex = cursor / 4;
    int subchannelIndex = cursor % 4;
    LabelsAndColorsPage::editChannelLabel(slotIndex, AIN_1_SUBCHANNEL_INDEX + subchannelIndex);
}

////////////////////////////////////////////////////////////////////////////////

void data_dib_mio168_aout_channels(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_COUNT) {
        value = 4;
    } else if (operation == DATA_OPERATION_GET_CURSOR_VALUE) {
        value = hmi::g_selectedSlotIndex * 4 + value.getInt();
    }
}

void data_dib_mio168_aout_label(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        int slotIndex = cursor / 4;
        int subchannelIndex = cursor % 4;

        if (isPageOnStack(PAGE_ID_SYS_SETTINGS_LABELS_AND_COLORS)) {
            const char *label = LabelsAndColorsPage::getChannelLabel(slotIndex, AOUT_1_SUBCHANNEL_INDEX + subchannelIndex);
            if (*label) {
                value = label;
            } else {
                value = g_slots[slotIndex]->getDefaultChannelLabel(AOUT_1_SUBCHANNEL_INDEX + subchannelIndex);
            }            
        } else {
            int aoutChannelIndex;

            AoutDac7760ConfigurationPage *page = (AoutDac7760ConfigurationPage *)getPage(PAGE_ID_DIB_MIO168_AOUT_DAC7760_CONFIGURATION);
            if (page) {
				slotIndex = hmi::g_selectedSlotIndex;
                aoutChannelIndex = AoutDac7760ConfigurationPage::g_selectedChannelIndex - AOUT_1_SUBCHANNEL_INDEX;
            } else {
                AoutDac7563ConfigurationPage *page = (AoutDac7563ConfigurationPage *)getPage(PAGE_ID_DIB_MIO168_AOUT_DAC7563_CONFIGURATION);
                if (page) {
					slotIndex = hmi::g_selectedSlotIndex;
                    aoutChannelIndex = AoutDac7563ConfigurationPage::g_selectedChannelIndex - AOUT_1_SUBCHANNEL_INDEX;
                } else {
                    aoutChannelIndex = subchannelIndex;
                }
            }

            value = getChannelLabel(slotIndex, AOUT_1_SUBCHANNEL_INDEX + aoutChannelIndex);
        }
    }
}

void data_dib_mio168_aout_label_label(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        static const char *g_aoutLabelLabels[4] = { "AOUT1 label:", "AOUT2 label:", "AOUT3 label:", "AOUT4 label:" };
        value = g_aoutLabelLabels[cursor % 4];
    }
}

void data_dib_mio168_aout_value(DataOperationEnum operation, Cursor cursor, Value &value) {
    int slotIndex = cursor / 4;
    int aoutChannelIndex = cursor % 4;

    if (operation == DATA_OPERATION_GET) {
        bool focused = g_focusCursor == cursor && g_focusDataId == DATA_ID_DIB_MIO168_AOUT_VALUE;
        if (focused && g_focusEditValue.getType() != VALUE_TYPE_NONE) {
            value = g_focusEditValue;
        } else if (focused && getActivePageId() == PAGE_ID_EDIT_MODE_KEYPAD && edit_mode_keypad::g_keypad->isEditing()) {
            data_keypad_text(operation, cursor, value);
        } else {
            if (aoutChannelIndex < 2) {
                auto &channel = ((Mio168Module *)g_slots[slotIndex])->aoutDac7760Channels[aoutChannelIndex];
                value = MakeValue(channel.getValue(), channel.getUnit());
            } else {
                value = MakeValue(((Mio168Module *)g_slots[slotIndex])->aoutDac7563Channels[aoutChannelIndex - 2].m_value, UNIT_VOLT);
            }
        }
    } else if (operation == DATA_OPERATION_GET_MIN) {
        if (aoutChannelIndex < 2) {
            auto &channel = ((Mio168Module *)g_slots[slotIndex])->aoutDac7760Channels[aoutChannelIndex];
            value = MakeValue(channel.getMinValue(), channel.getUnit());
        } else {
            value = MakeValue(AOUT_DAC7563_MIN, UNIT_VOLT);
        }
    } else if (operation == DATA_OPERATION_GET_MAX) {
        if (aoutChannelIndex < 2) {
            auto &channel = ((Mio168Module *)g_slots[slotIndex])->aoutDac7760Channels[aoutChannelIndex];
            value = MakeValue(channel.getMaxValue(), channel.getUnit());
        } else {
            value = MakeValue(AOUT_DAC7563_MAX, UNIT_VOLT);
        }
    } else if (operation == DATA_OPERATION_GET_NAME) {
        value = getChannelLabel(slotIndex, AOUT_1_SUBCHANNEL_INDEX + aoutChannelIndex);
    } else if (operation == DATA_OPERATION_GET_UNIT) {
        if (aoutChannelIndex < 2) {
            auto &channel = ((Mio168Module *)g_slots[slotIndex])->aoutDac7760Channels[aoutChannelIndex];
            value = channel.getUnit();
        } else {
            value = UNIT_VOLT;
        }
    } else if (operation == DATA_OPERATION_GET_ENCODER_STEP_VALUES) {
        StepValues *stepValues = value.getStepValues();

        if (aoutChannelIndex < 2) {
            auto &channel = ((Mio168Module *)g_slots[slotIndex])->aoutDac7760Channels[aoutChannelIndex];
            channel.getStepValues(stepValues);
        } else {
            auto &channel = ((Mio168Module *)g_slots[slotIndex])->aoutDac7563Channels[aoutChannelIndex - 2];
            channel.getStepValues(stepValues);
        }

        value = 1;
    } else if (operation == DATA_OPERATION_SET) {
        if (aoutChannelIndex < 2) {
            auto &channel = ((Mio168Module *)g_slots[slotIndex])->aoutDac7760Channels[aoutChannelIndex];
            channel.setValue(roundPrec(value.getFloat(), channel.getResolution()));
        } else {
            auto &channel = ((Mio168Module *)g_slots[slotIndex])->aoutDac7563Channels[aoutChannelIndex - 2];
            channel.m_value = roundPrec(value.getFloat(), channel.getResolution());
        }
    }
}

void data_dib_mio168_aout_output_enabled(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        AoutDac7760ConfigurationPage *page = (AoutDac7760ConfigurationPage *)getPage(PAGE_ID_DIB_MIO168_AOUT_DAC7760_CONFIGURATION);
        if (page) {
            value = g_aoutDac7760ConfigurationPage.m_outputEnabled;
        } else {
            int slotIndex = cursor / 4;
            int aoutChannelIndex = cursor % 4;
            if (aoutChannelIndex < 2) {
                auto &channel = ((Mio168Module *)g_slots[slotIndex])->aoutDac7760Channels[aoutChannelIndex];
                value = channel.m_outputEnabled;
            } else {
                value = 0;
            }
        }
    }
}

void data_dib_mio168_aout_output_mode(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = g_aoutDac7760ConfigurationPage.m_mode;
    }
}

void onSetOutputMode(uint16_t value) {
    popPage();
    g_aoutDac7760ConfigurationPage.m_mode = (SourceMode)value;
}

void action_dib_mio168_aout_select_output_mode() {
    g_aoutDac7760ConfigurationPage.m_mode = g_aoutDac7760ConfigurationPage.m_mode == SOURCE_MODE_VOLTAGE ? SOURCE_MODE_CURRENT : SOURCE_MODE_VOLTAGE;
}

static EnumItem g_aoutVoltageRangeEnumDefinition[] = {
    { 0, "0 V to +5 V" },
    { 1, "0 V to +10 V" },
    { 2, "\xbd""5 V" },
    { 3, "\xbd""10 V" },
    { 0, 0 }
};

void data_dib_mio168_aout_voltage_range(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = getWidgetLabel(g_aoutVoltageRangeEnumDefinition, g_aoutDac7760ConfigurationPage.m_voltageRange);
    }
}

void onSetVoltageRange(uint16_t value) {
    popPage();
    g_aoutDac7760ConfigurationPage.m_voltageRange = (uint8_t)value;
}

void action_dib_mio168_aout_select_voltage_range() {
    pushSelectFromEnumPage(g_aoutVoltageRangeEnumDefinition, g_aoutDac7760ConfigurationPage.m_voltageRange, nullptr, onSetVoltageRange);
}

static EnumItem g_aoutCurrentRangeEnumDefinition[] = {
    { 5, "4 mA to 20 mA" },
    { 6, "0 mA to 20 mA" },
    { 7, "0 mA to 24 mA" },
    { 0, 0 }
};

void data_dib_mio168_aout_current_range(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = getWidgetLabel(g_aoutCurrentRangeEnumDefinition, g_aoutDac7760ConfigurationPage.m_currentRange);
    }
}

void onSetCurrentRange(uint16_t value) {
    popPage();
    g_aoutDac7760ConfigurationPage.m_currentRange = (uint8_t)value;
}

void action_dib_mio168_aout_select_current_range() {
    pushSelectFromEnumPage(g_aoutCurrentRangeEnumDefinition, g_aoutDac7760ConfigurationPage.m_currentRange, nullptr, onSetCurrentRange);
}

void action_dib_mio168_aout_toggle_output_enabled() {
    g_aoutDac7760ConfigurationPage.m_outputEnabled = !g_aoutDac7760ConfigurationPage.m_outputEnabled;
}

void action_dib_mio168_aout_show_configuration() {
    int cursor = getFoundWidgetAtDown().cursor;
    
    int slotIndex = cursor / 4;
    hmi::selectSlot(slotIndex);
    
    int aoutChannelIndex = cursor % 4;
    if (aoutChannelIndex < 2) {
        AoutDac7760ConfigurationPage::g_selectedChannelIndex = AOUT_1_SUBCHANNEL_INDEX + aoutChannelIndex;
        pushPage(PAGE_ID_DIB_MIO168_AOUT_DAC7760_CONFIGURATION);
    } else {
        AoutDac7563ConfigurationPage::g_selectedChannelIndex = AOUT_1_SUBCHANNEL_INDEX + aoutChannelIndex;
        pushPage(PAGE_ID_DIB_MIO168_AOUT_DAC7563_CONFIGURATION);
    }
}

void data_dib_mio168_aout_channel_has_settings(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = cursor % 4 < 2 ? 1 : 0;
    }
}

void action_dib_mio168_aout_change_label() {
    int cursor = getFoundWidgetAtDown().cursor;
    int slotIndex = cursor / 4;
    int subchannelIndex = cursor % 4;
    LabelsAndColorsPage::editChannelLabel(slotIndex, AOUT_1_SUBCHANNEL_INDEX + subchannelIndex);
}

////////////////////////////////////////////////////////////////////////////////

void data_dib_mio168_pwm_channels(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_COUNT) {
        value = 2;
    } else if (operation == DATA_OPERATION_GET_CURSOR_VALUE) {
        value = hmi::g_selectedSlotIndex * 2 + value.getInt();
    }
}

void data_dib_mio168_pwm_label(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        int slotIndex = cursor / 2;
        int subchannelIndex = cursor % 2;

        if (isPageOnStack(PAGE_ID_SYS_SETTINGS_LABELS_AND_COLORS)) {
            const char *label = LabelsAndColorsPage::getChannelLabel(slotIndex, PWM_1_SUBCHANNEL_INDEX + subchannelIndex);
            if (*label) {
                value = label;
            } else {
                value = g_slots[slotIndex]->getDefaultChannelLabel(PWM_1_SUBCHANNEL_INDEX + subchannelIndex);
            }            
        } else {
            value = getChannelLabel(slotIndex, PWM_1_SUBCHANNEL_INDEX + subchannelIndex);
        }        
    }
}

void data_dib_mio168_pwm_label_label(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        static const char *g_pwmLabelLabels[2] = { "PWM1 label:", "PWM2 label:"};
        value = g_pwmLabelLabels[cursor % 2];
    }
}

void data_dib_mio168_pwm_freq(DataOperationEnum operation, Cursor cursor, Value &value) {
    int slotIndex = cursor / 2;
    int pwmChannelIndex = cursor % 2;

    if (operation == DATA_OPERATION_GET) {
        bool focused = g_focusCursor == cursor && g_focusDataId == DATA_ID_DIB_MIO168_PWM_FREQ;
        if (focused && g_focusEditValue.getType() != VALUE_TYPE_NONE) {
            value = g_focusEditValue;
        } else if (focused && getActivePageId() == PAGE_ID_EDIT_MODE_KEYPAD && edit_mode_keypad::g_keypad->isEditing()) {
            data_keypad_text(operation, cursor, value);
        } else {
            value = MakeValue(((Mio168Module *)g_slots[slotIndex])->pwmChannels[pwmChannelIndex].m_freq, UNIT_HERTZ);
        }
    } else if (operation == DATA_OPERATION_GET_ALLOW_ZERO) {
        value = 1;
    } else if (operation == DATA_OPERATION_GET_MIN) {
        value = MakeValue(PWM_MIN_FREQUENCY, UNIT_HERTZ);
    } else if (operation == DATA_OPERATION_GET_MAX) {
        value = MakeValue(PWM_MAX_FREQUENCY, UNIT_HERTZ);
    } else if (operation == DATA_OPERATION_GET_NAME) {
        value = "PWM frequency";
    } else if (operation == DATA_OPERATION_GET_UNIT) {
        value = UNIT_HERTZ;
    } else if (operation == DATA_OPERATION_GET_ENCODER_STEP_VALUES) {
        static float values[] = { 1.0f, 100.0f, 1000.0f, 10000.0f };

        StepValues *stepValues = value.getStepValues();

        stepValues->values = values;
        stepValues->count = sizeof(values) / sizeof(float);
        stepValues->unit = UNIT_HERTZ;

        stepValues->encoderSettings.accelerationEnabled = true;

        float fvalue = ((Mio168Module *)g_slots[slotIndex])->pwmChannels[pwmChannelIndex].m_freq;
        float step = MAX(powf(10.0f, floorf(log10f(fabsf(fvalue))) - 1), 0.001f);

        stepValues->encoderSettings.range = step * 5.0f;
        stepValues->encoderSettings.step = step;

        value = 1;
    } else if (operation == DATA_OPERATION_SET) {
        ((Mio168Module *)g_slots[slotIndex])->pwmChannels[pwmChannelIndex].m_freq = value.getFloat();
    }
}

void data_dib_mio168_pwm_duty(DataOperationEnum operation, Cursor cursor, Value &value) {
    int slotIndex = cursor / 2;
    int pwmChannelIndex = cursor % 2;
    
    if (operation == DATA_OPERATION_GET) {
        bool focused = g_focusCursor == cursor && g_focusDataId == DATA_ID_DIB_MIO168_PWM_DUTY;
        if (focused && g_focusEditValue.getType() != VALUE_TYPE_NONE) {
            value = g_focusEditValue;
        } else if (focused && getActivePageId() == PAGE_ID_EDIT_MODE_KEYPAD && edit_mode_keypad::g_keypad->isEditing()) {
            data_keypad_text(operation, cursor, value);
        } else {
            value = MakeValue(((Mio168Module *)g_slots[slotIndex])->pwmChannels[pwmChannelIndex].m_duty, UNIT_PERCENT);
        }
    } else if (operation == DATA_OPERATION_GET_MIN) {
        value = MakeValue(0.0f, UNIT_PERCENT);
    } else if (operation == DATA_OPERATION_GET_MAX) {
        value = MakeValue(100.0f, UNIT_PERCENT);
    } else if (operation == DATA_OPERATION_GET_NAME) {
        value = "PWM duty cycle";
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

        value = 1;
    } else if (operation == DATA_OPERATION_SET) {
        ((Mio168Module *)g_slots[slotIndex])->pwmChannels[pwmChannelIndex].m_duty = value.getFloat();
    } 
}

void action_dib_mio168_pwm_change_label() {
    int cursor = getFoundWidgetAtDown().cursor;
    int slotIndex = cursor / 2;
    int subchannelIndex = cursor % 2;
    LabelsAndColorsPage::editChannelLabel(slotIndex, PWM_1_SUBCHANNEL_INDEX + subchannelIndex);
}

////////////////////////////////////////////////////////////////////////////////

void data_dib_mio168_afe_version(DataOperationEnum operation, Cursor cursor, Value &value) {
	if (operation == DATA_OPERATION_GET) {
		auto module = (Mio168Module *)g_slots[hmi::g_selectedSlotIndex];
		value = module->afeVersion;
	}
}

void action_dib_mio168_show_info() {
    pushPage(PAGE_ID_DIB_MIO168_INFO);
}

void action_dib_mio168_show_calibration() {
    Page *page = getActivePage();
    if (page && page->getDirty()) {
        int pageId = getActivePageId();
        ((SetPage *)getActivePage())->set();
        pushPage(pageId);
    }

    auto module = (Mio168Module *)g_slots[hmi::g_selectedSlotIndex];

    if (getPage(PAGE_ID_DIB_MIO168_AOUT_DAC7760_CONFIGURATION)) {
        hmi::g_selectedSubchannelIndex = g_aoutDac7760ConfigurationPage.g_selectedChannelIndex;
        auto &channel = module->aoutDac7760Channels[hmi::g_selectedSubchannelIndex - AOUT_1_SUBCHANNEL_INDEX];
        module->dac7760CalibrationChannelMode = channel.getMode();
        module->dac7760CalibrationChannelCurrentRange = channel.getCurrentRange();
        module->dac7760CalibrationChannelVoltageRange = channel.getVoltageRange();
    } else if (getPage(PAGE_ID_DIB_MIO168_AOUT_DAC7563_CONFIGURATION)) {
        hmi::g_selectedSubchannelIndex = g_aoutDac7563ConfigurationPage.g_selectedChannelIndex;
    } else {
        hmi::g_selectedSubchannelIndex = g_ainConfigurationPage.g_selectedChannelIndex;
    }

    calibration::g_viewer.start(hmi::g_selectedSlotIndex, hmi::g_selectedSubchannelIndex);
    
    pushPage(PAGE_ID_CH_SETTINGS_CALIBRATION);
}

void action_dib_mio168_show_channel_labels() {
    hmi::selectSlot(getFoundWidgetAtDown().cursor);
    pushPage(PAGE_ID_DIB_MIO168_CHANNEL_LABELS);
}

} // namespace gui

} // namespace eez
