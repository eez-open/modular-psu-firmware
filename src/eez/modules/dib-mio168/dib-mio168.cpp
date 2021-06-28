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
#include <float.h>

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

#include <eez/function_generator.h>

#if defined(EEZ_PLATFORM_SIMULATOR)
#include <eez/platform/simulator/front_panel.h>
#endif

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
    PSU_MESSAGE_AOUT_DAC7563_CONFIGURE,
    PSU_MESSAGE_DISK_DRIVE_OPERATION,
};

static const uint16_t MODULE_REVISION_R1B2  = 0x0102;

enum {
	EFFICIENCY_FORMULA_NONE,
	EFFICIENCY_FORMULA_P1_OVER_P2,
	EFFICIENCY_FORMULA_P2_OVER_P1
};

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

static float AOUT_DAC7760_ENCODER_STEP_VALUES[] = { 0.01f, 0.1f, 0.2f, 0.5f };
static float AOUT_DAC7760_ENCODER_STEP_VALUES_20V[] = { 0.05f, 0.1f, 0.2f, 0.5f };
static float AOUT_DAC7760_AMPER_ENCODER_STEP_VALUES[] = { 1E-5f, 1E-4f, 1E-3f, 2E-3f };

static float AOUT_DAC7760_ENCODER_STEP_VALUES_CAL[] = { 0.001f, 0.01f, 0.02f, 0.05f };
static float AOUT_DAC7760_AMPER_ENCODER_STEP_VALUES_CAL[] = { 1E-5f, 1E-4f, 1E-3f, 2E-3f };

static float AOUT_DAC7563_MIN = -10.0f;
static float AOUT_DAC7563_MAX = 10.0f;
static float AOUT_DAC7563_ENCODER_STEP_VALUES[] = { 0.05f, 0.1f, 0.2f, 0.5f };
static float AOUT_DAC7563_ENCODER_STEP_VALUES_CAL[] = { 0.005f, 0.01f, 0.02f, 0.05f };

static float PWM_MIN_FREQUENCY = 0.1f;
static float PWM_MAX_FREQUENCY = 1000000.0f;

static const size_t CHANNEL_LABEL_MAX_LENGTH = 5;

static const int NUM_REQUEST_RETRIES = 10;
static const int MAX_DMA_TRANSFER_ERRORS = 10;
static const uint32_t REFRESH_TIME_MS = 249;
static const uint32_t TIMEOUT_TIME_MS = 350;
static const uint32_t TIMEOUT_UNTIL_OUT_OF_SYNC_MS = 10000;

////////////////////////////////////////////////////////////////////////////////

enum Command {
	COMMAND_NONE = 0x69365f39,

    COMMAND_GET_INFO = 0x0200ccb7,
    COMMAND_GET_STATE = 0x0093f106,
    COMMAND_SET_PARAMS = 0x1ff1c78c,

    COMMAND_DLOG_RECORDING_START = 0x7d3f9f28,
    COMMAND_DLOG_RECORDING_STOP = 0x7de480a1,
    COMMAND_DLOG_RECORDING_DATA = 0x1ddc8028,

    COMMAND_DISK_DRIVE_INITIALIZE = 0x0f8066f8,
    COMMAND_DISK_DRIVE_STATUS = 0x457f0700,
    COMMAND_DISK_DRIVE_READ = 0x4478491d,
    COMMAND_DISK_DRIVE_WRITE = 0x7d652b00,
    COMMAND_DISK_DRIVE_IOCTL = 0x22cc23c8
};

#define GET_STATE_COMMAND_FLAG_SD_CARD_PRESENT (1 << 0)

#define DLOG_STATE_IDLE 0
#define DLOG_STATE_EXECUTING 1
#define DLOG_STATE_FINISH_RESULT_OK 2
#define DLOG_STATE_FINISH_RESULT_BUFFER_OVERFLOW 3
#define DLOG_STATE_FINISH_RESULT_MASS_STORAGE_ERROR 4

using function_generator::Waveform;

struct DlogState {
    uint8_t state; // DLOG_STATE_...
    uint32_t fileLength;
    uint32_t numSamples;
};

struct WaveformParameters {
	Waveform waveform;
	float frequency;
	float phaseShift;
	float amplitude;
	float offset;
	float dutyCycle;
};

struct SetParams {
	uint8_t dinRanges;
	uint8_t dinSpeeds;

	uint8_t doutStates;
    WaveformParameters doutWaveformParameters[8];

	struct {
		uint8_t mode; // enum SourceMode
		uint8_t range;
        float nplc; // from 0 to 25
        float p1CalX[3];
        float p1CalY[3];
        float p2CalX[3];
        float p2CalY[3];
	} ain[4];

	uint8_t acAnalysisEnabled;

	uint8_t powerLineFrequency; // 50 or 60

	struct {
		uint8_t outputEnabled;
		uint8_t outputRange;
		float outputValue;
	} aout_dac7760[2];

	struct {
		float voltage;
	} aout_dac7563[2];

	WaveformParameters aoutWaveformParameters[4];

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
    uint32_t command;

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
            uint16_t reserved;
            uint8_t buffer[1024];
        } diskDriveWrite;

        struct {
            uint8_t cmd;
            uint8_t buffer[DISK_DRIVER_IOCTL_BUFFER_MAX_SIZE];
        } diskDriveIoctl;
    };
};

struct Response {
	uint32_t command;

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
			uint8_t ainRange[4];
            uint16_t ainFaultStatus;
            uint8_t ainDiagStatus;
            float activePower;
            float reactivePower;
            float voltRMS;
            float currRMS;
            DlogState dlogState;
        } getState;

        struct {
            uint8_t result; // 1 - success, 0 - failure
        } setParams;

        struct {
            uint32_t recordIndex;
            uint16_t numRecords;
            uint8_t buffer[1024];
        } dlogRecordingData;

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

inline double getAinConversionFactor(uint8_t afeVersion, uint8_t channelIndex, uint8_t mode, uint8_t range) {
    if (afeVersion == 1) {
        if (channelIndex < 2) {
            if (mode == MEASURE_MODE_VOLTAGE) {
                if (range == 0) {
                    return 2.4; // +/- 2.4 V
                }
                if (range == 1) {
                    return 48.0; // +/- 48 V
                }
                return 240.0; // +/- 240 V
            }
            if (mode == MEASURE_MODE_CURRENT) {
                return 2.4 / 33 / 1; // +/- 48 mV (22 ohm, PGA is 1)
            }
        } else {
            if (mode == MEASURE_MODE_VOLTAGE) {
                if (range == 0) {
                    return 2.4; // +/- 2.4 V
                }
                return 12.0; // +/- 12 V
            }
            if (mode == MEASURE_MODE_CURRENT) {
                if (range == 0) {
                    return 2.4 / 22 / 1; // +/- 48 mA (22 ohm, PGA is 1)
                }
                if (range == 1) {
                    return 2.4 / 0.33 / 4; // +/- 1.2 A (0.33 ohm, PGA is 4)
                }
                return 2.4 / 0.01 / 12; // +/- 10 A (0.01 ohm, PGA is 12)
            }
        }
    }  else if (afeVersion == 2) {
        if (channelIndex == 0 || channelIndex == 1) {
            if (range == 0) {
                return -3.0 * 2.4 / 2.24; // +/- 3 V (3 V is 2.24 V)
            }
            return -12.0 * 2.4 / 2.0; // +/- 12 V (12 V is 2.0 V)
        } else {
            if (range == 0) {
                return -2.4 / 833.3; // +/- 2.4 mA (833.3 ohm, PGA is 1)
            }
            return 2.4 / 2.11; // +/- 2 A (2.11 ohm, PGA is 1)
        }
    } else if (afeVersion == 3) {
        if (channelIndex == 0) {
            if (range == 0) {
                return 50.0 * 2.4 / 1.8; // +/- 50 V (50 V is 1.8 V)
            }
            return 450.0 * 2.4 / 1.86; // +/- 450 V (450 V is 1.86 V)
        } else if (channelIndex == 1) {
            if (range == 0) {
                return 2.4 / 1.886; // +/- 1 A (1A is 1.886 V, PGA is 1)
            }
            return 10.0 * 2.4 / 0.82 / 2; // +/- 10 A (10 A is 0.82 V, PGA is 2)
        } else if (channelIndex == 2) {
            if (range == 0) {
                return 2.4; // +/- 2.4 V
            }
            if (range == 1) {
                return 48.0; // +/- 48.0 V
            }
            return 240.0; // +/- 240 V
        } else {
            if (range == 0) {
                return 2.4 / 33 / 1; // +/- 24 mA (22 ohm, PGA is 1)
            }
            if (range == 1) {
                return 2.4 / 0.33 / 4; // +/- 1.2 A (0.33 ohm, PGA is 4)
            }
            return 2.4 / 0.01 / 12; // +/- 10 A (0.01 ohm, PGA is 12)
        }
    }
    return 0;
}

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

    float offsetAndScale(float value) {
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
            snprintf(propName, sizeof(propName), "din_pin%dLabel", i);
            WRITE_PROPERTY(propName, parameters.pinLabels + i * (CHANNEL_LABEL_MAX_LENGTH + 1));
        }
        return true;
    }

    bool readProfileProperties(psu::profile::ReadContext &ctx, ProfileParameters &parameters) {
        READ_PROPERTY("din_pinRanges", parameters.pinRanges);
        READ_PROPERTY("din_pinSpeeds", parameters.pinSpeeds);
        for (int i = 0; i < 8; i++) {
            char propName[32];
            snprintf(propName, sizeof(propName), "din_pin%dLabel", i);
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

#if defined(EEZ_PLATFORM_SIMULATOR)
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

    TriggerMode m_triggerMode[8];
    
    struct ProfileParameters {
        uint8_t pinStates;
        char pinLabels[8 * (CHANNEL_LABEL_MAX_LENGTH + 1)];
        uint8_t triggerMode[8];
    };

    void resetProfileToDefaults(ProfileParameters &parameters) {
        parameters.pinStates = 0;
        
        memset(parameters.pinLabels, 0, sizeof(m_pinLabels));
        
        for (int i = 0; i < 8; i++) {
            parameters.triggerMode[i] = TRIGGER_MODE_FIXED;
        }
    }

    void getProfileParameters(ProfileParameters &parameters) {
        parameters.pinStates = m_pinStates;
        
        memcpy(parameters.pinLabels, m_pinLabels, sizeof(m_pinLabels));
        
        for (int i = 0; i < 8; i++) {
            parameters.triggerMode[i] = m_triggerMode[i];
        }
    }

    void setProfileParameters(ProfileParameters &parameters) {
        m_pinStates = parameters.pinStates;
        
        memcpy(m_pinLabels, parameters.pinLabels, sizeof(m_pinLabels));
        
        for (int i = 0; i < 8; i++) {
            m_triggerMode[i] = (TriggerMode)parameters.triggerMode[i];
        }
    }

    bool writeProfileProperties(psu::profile::WriteContext &ctx, ProfileParameters &parameters) {
        WRITE_PROPERTY("dout_pin_states", parameters.pinStates);

        for (int i = 0; i < 8; i++) {
            char propName[32];

            snprintf(propName, sizeof(propName), "dout_pin%d_label", i);
            WRITE_PROPERTY(propName, parameters.pinLabels + i * (CHANNEL_LABEL_MAX_LENGTH + 1));

            snprintf(propName, sizeof(propName), "dout_pin%d_triggerMode", i);
            WRITE_PROPERTY(propName, parameters.triggerMode[i]);
        }

        return true;
    }

    bool readProfileProperties(psu::profile::ReadContext &ctx, ProfileParameters &parameters) {
        READ_PROPERTY("dout_pin_states", parameters.pinStates);

        for (int i = 0; i < 8; i++) {
            char propName[32];

            snprintf(propName, sizeof(propName), "dout_pin%d_label", i);
            READ_STRING_PROPERTY(propName, parameters.pinLabels + i * (CHANNEL_LABEL_MAX_LENGTH + 1), CHANNEL_LABEL_MAX_LENGTH);

            snprintf(propName, sizeof(propName), "dout_pin%d_triggerMode", i);
            READ_PROPERTY(propName, parameters.triggerMode[i]);
        }

        return false;
    }

   void resetConfiguration() {
        m_pinStates = 0;
        
        memset(&m_pinLabels, 0, sizeof(m_pinLabels));
        
        for (int i = 0; i < 8; i++) {
            m_triggerMode[i] = TRIGGER_MODE_FIXED;
        }
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
	int m_subchannelIndex;
    float m_value = 0;
    uint8_t m_autoRange;

	uint8_t m_afeVersion = 0;

private:
    uint8_t m_mode = MEASURE_MODE_VOLTAGE;

	// AFE1:
	//		AIN1 and AIN2:
	//			0: +/- 48 mA
	//		AIN3 and AIN4:
	//			0: +/- 48 mA
	//			1: +/- 1.2 A
	//			2: +/- 10 A
    //          3: Auto range
	//
	// AFE3
	//		AIN1
	//			no current
	//		AIN2
	//			0: +/- 1 A
	//			1: +/- 10 A
    //          2: Auto range
	//		AIN3
	//			no current
	//		AIN4
	//			0: +/- 48 mA
	//			1: +/- 1.2 A
	//			2: +/- 10 A
    //			3: Auto range
	uint8_t m_currentRange;

	// AFE1:
	//		AIN1 and AIN2:
	//			0: +/- 2.4 V
	//			1: +/- 24 V
	//			2: +/- 240 V
    //			3: Auto range
	//		AIN3 and AIN4:
	//			0: +/- 2.4 V
	//			1: +/- 12 V
    //			2: Auto range
	//
	// AFE3
	//		AIN1
	//			0: +/- 50 V
	//			1: +/- 450 V
    //			2: Auto range
	//		AIN2
	//			no voltage
	//		AIN3
	//			0: +/- 2.4 V
	//			1: +/- 48 V
	//			2: +/- 240 V
    //			3: Auto range
	//		AIN4
	//			no voltage
	uint8_t m_voltageRange;

public:
    float m_currentNPLC = 1.0f;
    float m_voltageNPLC = 1.0f;
    
	char m_label[CHANNEL_LABEL_MAX_LENGTH + 1];

    CalConf calConf[5];
    bool ongoingCal = false;

	AinChannel(int subchannelIndex) : m_subchannelIndex(subchannelIndex) {
        memset(calConf, 0, sizeof(CalConf));
    }

	void setAfeVersion(uint8_t afeVersion) {
		m_afeVersion = afeVersion;

		if (m_afeVersion == 1) {
			if (m_subchannelIndex == AIN_1_SUBCHANNEL_INDEX || m_subchannelIndex == AIN_2_SUBCHANNEL_INDEX) {
				m_currentRange = 0;
				if (m_voltageRange > 3) {
					m_voltageRange = 3;
				}
			} else {
				if (m_currentRange > 3) {
					m_currentRange = 3;
				}
				if (m_voltageRange > 2) {
					m_voltageRange = 2;
				}
			}
		} else if (m_afeVersion == 2) {
			if (m_subchannelIndex == AIN_1_SUBCHANNEL_INDEX || m_subchannelIndex == AIN_2_SUBCHANNEL_INDEX) {
				if (m_mode == MEASURE_MODE_CURRENT) {
					m_mode = MEASURE_MODE_VOLTAGE;
				}
				if (m_voltageRange > 1) {
					m_voltageRange = 1;
				}
			} else {
				if (m_mode == MEASURE_MODE_VOLTAGE) {
					m_mode = MEASURE_MODE_CURRENT;
				}
				if (m_currentRange > 1) {
					m_currentRange = 1;
				}
			}
		} else if (m_afeVersion == 3) {
			if (m_subchannelIndex == AIN_1_SUBCHANNEL_INDEX) {
				if (m_mode == MEASURE_MODE_CURRENT) {
					m_mode = MEASURE_MODE_VOLTAGE;
				}
				if (m_voltageRange > 2) {
					m_voltageRange = 2;
				}
			} else if (m_subchannelIndex == AIN_2_SUBCHANNEL_INDEX) {
				if (m_mode == MEASURE_MODE_VOLTAGE) {
					m_mode = MEASURE_MODE_CURRENT;
				}
				if (m_currentRange > 2) {
					m_currentRange = 2;
				}
			} else if (m_subchannelIndex == AIN_3_SUBCHANNEL_INDEX) {
				if (m_mode == MEASURE_MODE_CURRENT) {
					m_mode = MEASURE_MODE_VOLTAGE;
				}
				if (m_voltageRange > 3) {
					m_voltageRange = 3;
				}
			} else {
				if (m_mode == MEASURE_MODE_VOLTAGE) {
					m_mode = MEASURE_MODE_CURRENT;
				}
				if (m_currentRange > 3) {
					m_currentRange = 3;
				}
			}
		}
	}

	MeasureMode getMode() {
		return (MeasureMode)m_mode;
	}

	void setMode(MeasureMode mode) {
		m_mode = mode;
	}
	
	struct ProfileParameters {
        uint8_t mode;
		uint8_t currentRange;
        uint8_t voltageRange;
        float currentNPLC;
        float voltageNPLC;
        char label[CHANNEL_LABEL_MAX_LENGTH + 1];
    };

    void resetProfileToDefaults(ProfileParameters &parameters) {
		if (m_afeVersion == 1) {
			parameters.mode = MEASURE_MODE_VOLTAGE;
			if (m_subchannelIndex == AIN_1_SUBCHANNEL_INDEX || m_subchannelIndex == AIN_2_SUBCHANNEL_INDEX) {
				parameters.currentRange = 0;
				parameters.voltageRange = 3;
			} else {
				parameters.currentRange = 3;
				parameters.voltageRange = 2;
			}
		} else if (m_afeVersion == 2) {
			if (m_subchannelIndex == AIN_1_SUBCHANNEL_INDEX || m_subchannelIndex == AIN_2_SUBCHANNEL_INDEX) {
				parameters.mode = MEASURE_MODE_VOLTAGE;
				parameters.currentRange = 0;
				parameters.voltageRange = 1;
			} else {
				parameters.mode = MEASURE_MODE_CURRENT;
				parameters.currentRange = 1;
				parameters.voltageRange = 0;
			}
		} else if (m_afeVersion == 3) {
			if (m_subchannelIndex == AIN_1_SUBCHANNEL_INDEX) {
				parameters.mode = MEASURE_MODE_VOLTAGE;
				parameters.currentRange = 0;
				parameters.voltageRange = 2;
			} else if (m_subchannelIndex == AIN_2_SUBCHANNEL_INDEX) {
				parameters.mode = MEASURE_MODE_CURRENT;
				parameters.currentRange = 2;
				parameters.voltageRange = 0;
			} else if (m_subchannelIndex == AIN_3_SUBCHANNEL_INDEX) {
				parameters.mode = MEASURE_MODE_VOLTAGE;
				parameters.currentRange = 0;
				parameters.voltageRange = 3;
			} else {
				parameters.mode = MEASURE_MODE_CURRENT;
				parameters.currentRange = 3;
				parameters.voltageRange = 0;
			}
		} else {
			parameters.mode = MEASURE_MODE_VOLTAGE;
			parameters.currentRange = 0;
			parameters.voltageRange = 0;
		}

        parameters.currentNPLC = 5.0f;
        parameters.voltageNPLC = 5.0f;
        *parameters.label = 0;
    }

    void getProfileParameters(ProfileParameters &parameters) {
        parameters.mode = m_mode;
		parameters.currentRange = m_currentRange;
        parameters.voltageRange = m_voltageRange;
        parameters.currentNPLC = m_currentNPLC;
        parameters.voltageNPLC = m_voltageNPLC;
        memcpy(parameters.label, m_label, sizeof(m_label));
    }

    void setProfileParameters(ProfileParameters &parameters) {
        m_mode = parameters.mode;
		m_currentRange = parameters.currentRange;
		m_voltageRange = parameters.voltageRange;
        m_currentNPLC = parameters.currentNPLC;
        m_voltageNPLC = parameters.voltageNPLC;
        memcpy(m_label, parameters.label, sizeof(m_label));

		if (m_mode == MEASURE_MODE_VOLTAGE) {
			if (getVoltageRangeMaxValue() == -1) {
				m_mode = MEASURE_MODE_CURRENT;
			}
		} else {
			if (getCurrentRangeMaxValue() == -1) {
				m_mode = MEASURE_MODE_VOLTAGE;
			}
		}

		if (m_currentRange > getCurrentRangeMaxValue()) {
			m_currentRange = getCurrentRangeMaxValue();
		}

		if (m_voltageRange > getVoltageRangeMaxValue()) {
			m_voltageRange = getVoltageRangeMaxValue();
		}
	}

    bool writeProfileProperties(psu::profile::WriteContext &ctx, int i, ProfileParameters &parameters) {
        char propName[32];

        snprintf(propName, sizeof(propName), "ain_%d_mode", i+1);
        WRITE_PROPERTY(propName, parameters.mode);

        snprintf(propName, sizeof(propName), "ain_%d_currentRange", i+1);
        WRITE_PROPERTY(propName, parameters.currentRange);

		snprintf(propName, sizeof(propName), "ain_%d_voltageRange", i + 1);
		WRITE_PROPERTY(propName, parameters.voltageRange);
		
		snprintf(propName, sizeof(propName), "ain_%d_currentNPLC", i + 1);
		WRITE_PROPERTY(propName, parameters.currentNPLC);

		snprintf(propName, sizeof(propName), "ain_%d_voltageNPLC", i + 1);
		WRITE_PROPERTY(propName, parameters.voltageNPLC);

		snprintf(propName, sizeof(propName), "ain_%d_label", i+1);
        WRITE_PROPERTY(propName, parameters.label);

        return true;
    }

    bool readProfileProperties(psu::profile::ReadContext &ctx, int i, ProfileParameters &parameters) {
        char propName[32];

        snprintf(propName, sizeof(propName), "ain_%d_mode", i+1);
        READ_PROPERTY(propName, parameters.mode);

        snprintf(propName, sizeof(propName), "ain_%d_currentRange", i+1);
        READ_PROPERTY(propName, parameters.currentRange);

		snprintf(propName, sizeof(propName), "ain_%d_voltageRange", i + 1);
		READ_PROPERTY(propName, parameters.voltageRange);
		
		snprintf(propName, sizeof(propName), "ain_%d_currentNPLC", i + 1);
		READ_PROPERTY(propName, parameters.currentNPLC);

		snprintf(propName, sizeof(propName), "ain_%d_voltageNPLC", i + 1);
		READ_PROPERTY(propName, parameters.voltageNPLC);

		snprintf(propName, sizeof(propName), "ain_%d_label", i+1);
        READ_STRING_PROPERTY(propName, parameters.label, CHANNEL_LABEL_MAX_LENGTH);

        return false;
    }

    void resetConfiguration() {
		if (m_afeVersion == 1) {
			m_mode = MEASURE_MODE_VOLTAGE;
			if (m_subchannelIndex == AIN_1_SUBCHANNEL_INDEX || m_subchannelIndex == AIN_2_SUBCHANNEL_INDEX) {
				m_currentRange = 0;
				m_voltageRange = 3;
			} else {
				m_currentRange = 3;
				m_voltageRange = 2;
			}
		} else if (m_afeVersion == 2) {
			if (m_subchannelIndex == AIN_1_SUBCHANNEL_INDEX || m_subchannelIndex == AIN_2_SUBCHANNEL_INDEX) {
				m_mode = MEASURE_MODE_VOLTAGE;
				m_currentRange = 0;
				m_voltageRange = 1;
			} else {
				m_mode = MEASURE_MODE_CURRENT;
				m_currentRange = 1;
				m_voltageRange = 0;
			}
		} else if (m_afeVersion == 3) {
			if (m_subchannelIndex == AIN_1_SUBCHANNEL_INDEX) {
				m_mode = MEASURE_MODE_VOLTAGE;
				m_currentRange = 0;
				m_voltageRange = 2;
			} else if (m_subchannelIndex == AIN_2_SUBCHANNEL_INDEX) {
				m_mode = MEASURE_MODE_CURRENT;
				m_currentRange = 2;
				m_voltageRange = 0;
			} else if (m_subchannelIndex == AIN_3_SUBCHANNEL_INDEX) {
				m_mode = MEASURE_MODE_VOLTAGE;
				m_currentRange = 0;
				m_voltageRange = 3;
			} else {
				m_mode = MEASURE_MODE_CURRENT;
				m_currentRange = 3;
				m_voltageRange = 0;
			}
		} else {
			m_mode = MEASURE_MODE_VOLTAGE;
			m_currentRange = 0;
			m_voltageRange = 0;
		}
        m_currentNPLC = 5.0f;
        m_voltageNPLC = 5.0f;
        *m_label = 0;
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

    static bool isAutoRange(uint8_t afeVersion, int subchannelIndex, uint8_t mode, uint8_t currentRange, uint8_t voltageRange) {
        if (afeVersion == 1) {
            if (subchannelIndex == AIN_1_SUBCHANNEL_INDEX || subchannelIndex == AIN_2_SUBCHANNEL_INDEX) {
                return mode == MEASURE_MODE_VOLTAGE ? voltageRange == 3 : false;
            } else {
                return mode == MEASURE_MODE_VOLTAGE ? voltageRange == 2 : currentRange == 3;
            }
        } else if (afeVersion == 3) {
            if (subchannelIndex == AIN_1_SUBCHANNEL_INDEX) {
                return voltageRange == 2;
            } else if (subchannelIndex == AIN_2_SUBCHANNEL_INDEX) {
				return currentRange == 2;
			} else if (subchannelIndex == AIN_3_SUBCHANNEL_INDEX) {
				return voltageRange == 3;
			} else {
				return currentRange == 3;
			}
        }
        return false;
    }

    bool isAutoRange() {
        return AinChannel::isAutoRange(m_afeVersion, m_subchannelIndex, m_mode, m_currentRange, m_voltageRange);
    }

	Unit getUnit() {
        if (m_afeVersion == 1) {
            if (m_subchannelIndex == AIN_1_SUBCHANNEL_INDEX || m_subchannelIndex == AIN_2_SUBCHANNEL_INDEX) {
                return m_mode == MEASURE_MODE_VOLTAGE ? UNIT_VOLT : UNIT_MILLI_AMPER;
            } else {
                return m_mode == MEASURE_MODE_VOLTAGE ? UNIT_VOLT : m_currentRange == 0 || (m_currentRange == 3 && m_autoRange == 0) ? UNIT_MILLI_AMPER : UNIT_AMPER;
            }
        } else if (m_afeVersion == 2) {
            if (m_subchannelIndex == AIN_1_SUBCHANNEL_INDEX || m_subchannelIndex == AIN_2_SUBCHANNEL_INDEX) {
                return UNIT_VOLT;
            } else {
                return m_currentRange == 0 ? UNIT_MICRO_AMPER : UNIT_AMPER;
            }
        } else if (m_afeVersion == 3) {
            if (m_subchannelIndex == AIN_1_SUBCHANNEL_INDEX) {
                return UNIT_VOLT;
            } else if (m_subchannelIndex == AIN_2_SUBCHANNEL_INDEX) {
                return UNIT_AMPER;
            } else if (m_subchannelIndex == AIN_3_SUBCHANNEL_INDEX) {
                return UNIT_VOLT;
            } else {
                return m_currentRange == 0 || (m_currentRange == 3 && m_autoRange == 0) ? UNIT_MILLI_AMPER : UNIT_AMPER;
            }
        }
        return m_mode == MEASURE_MODE_VOLTAGE ? UNIT_VOLT : UNIT_AMPER;
	}

	int getNumFixedDecimals() {
        if (m_afeVersion == 1) {
            if (m_subchannelIndex == AIN_1_SUBCHANNEL_INDEX || m_subchannelIndex == AIN_2_SUBCHANNEL_INDEX) {
                if (m_mode == MEASURE_MODE_VOLTAGE) {
                    if (m_voltageRange == 0 || (m_voltageRange == 3 && m_autoRange == 0)) {
                        return 4; // +/- 2.4 V
                    } else if (m_voltageRange == 1 || (m_voltageRange == 3 && m_autoRange == 1)) {
                        return 3; // +/- 48 V
                    } else {
                        return 2; // +/- 240 V
                    }
                } else {
                    return 6; // +/- 48 mA
                }
            } else {
                if (m_mode == MEASURE_MODE_VOLTAGE) {
                    if (m_voltageRange == 0 || (m_voltageRange == 2 && m_autoRange == 0)) {
                        return 4; // +/- 2.4 V
                    } else {
                        return 3; // +/- 12 V
                    }
                } else {
                    if (m_currentRange == 0 || (m_currentRange == 3 && m_autoRange == 0)) {
                        return 6; // +/- 48 mA
                    } else if (m_currentRange == 1 || (m_currentRange == 3 && m_autoRange == 1)) {
                        return 4; // +/- 1.2 A
                    } else {
                        return 3; // +/- 10 A
                    }
                }
            }
        } else if (m_afeVersion == 2) {
            if (m_subchannelIndex == AIN_1_SUBCHANNEL_INDEX || m_subchannelIndex == AIN_2_SUBCHANNEL_INDEX) {
                if (m_voltageRange == 0) {
                    return 4; // +/- 3 V
                } else {
                    return 3; // +/- 12 V
                }
            } else {
                if (m_currentRange == 0) {
                    return 6; // +/- 2.4 mA
                } else {
                    return 4; // +/- 2 A
                }
            }
        } else if (m_afeVersion == 3) {
            if (m_subchannelIndex == AIN_1_SUBCHANNEL_INDEX) {
                if (m_voltageRange == 0 || (m_voltageRange == 2 && m_autoRange == 0)) {
                    return 3; // +/- 50 V
                } else {
                    return 2; // +/- 450 V
                }
            } else if (m_subchannelIndex == AIN_2_SUBCHANNEL_INDEX) {
                if (m_currentRange == 0 || (m_currentRange == 2 && m_autoRange == 0)) {
                    return 4; // +/- 1 A
                } else {
                    return 3; // +/- 10 A
                }
            } else if (m_subchannelIndex == AIN_3_SUBCHANNEL_INDEX) {
                if (m_voltageRange == 0 || (m_voltageRange == 3 && m_autoRange == 0)) {
                    return 4; // +/- 2.4 V
                } else if (m_voltageRange == 1 || (m_voltageRange == 3 && m_autoRange == 1)) {
                    return 3; // +/- 48 V
                } else {
                    return 2; // +/- 240 V
                }
            } else {
                if (m_currentRange == 0 || (m_currentRange == 3 && m_autoRange == 0)) {
                    return 6; // +/- 24 mA
                } else if (m_currentRange == 1 || (m_currentRange == 3 && m_autoRange == 1)) {
                    return 4; // +/- 1.2 A
                } else {
                    return 3; // +/- 10 A
                }
            }
        }

        return m_mode == MEASURE_MODE_VOLTAGE ? 3 : 4;
	}

    float getResolution() {
        return 1.0f / powf(10.0f, getNumFixedDecimals());
    }

	float getMinValue() {
		return -getMaxValue();
	}

	float getMaxValue() {
        if (m_afeVersion == 1) {
            if (m_subchannelIndex == AIN_1_SUBCHANNEL_INDEX || m_subchannelIndex == AIN_2_SUBCHANNEL_INDEX) {
                if (m_mode == MEASURE_MODE_VOLTAGE) {
                    if (m_voltageRange == 0) {
                        return 2.4f;
                    } else if (m_voltageRange == 1) {
                        return 48.0f;
                    } else {
                        return 240.0f;
                    }
                } else {
                    return 48.0E-3f;
                }
            } else {
                if (m_mode == MEASURE_MODE_VOLTAGE) {
                    if (m_voltageRange == 0) {
                        return 2.4f;
                    } else {
                        return 12.0f;
                    }
                } else {
                    if (m_currentRange == 0) {
                        return 48.0E-3f;
                    } else if (m_currentRange == 1) {
                        return 1.2f;
                    } else {
                        return 10.0f;
                    }
                }
            }
        } else if (m_afeVersion == 2) { 
            if (m_subchannelIndex == AIN_1_SUBCHANNEL_INDEX || m_subchannelIndex == AIN_2_SUBCHANNEL_INDEX) {
                if (m_voltageRange == 0) {
                    return 3.0f;
                } else {
                    return 12.0f;
                }
            } else {
                if (m_currentRange == 0) {
                    return 2.4E-3f;
                } else {
                    return 2.0f;
                }
            }
        } else if (m_afeVersion == 3) { 
            if (m_subchannelIndex == AIN_1_SUBCHANNEL_INDEX) {
                if (m_voltageRange == 0) {
                    return 50.0f;
                } else {
                    return 450.0f;
                }
            } else if (m_subchannelIndex == AIN_2_SUBCHANNEL_INDEX) {
                if (m_currentRange == 0) {
                    return 1.0f;
                } else {
                    return 10.0f;
                }
            } else if (m_subchannelIndex == AIN_3_SUBCHANNEL_INDEX) {
                if (m_voltageRange == 0) {
                    return 2.4f;
                } else if (m_voltageRange == 1) {
                    return 48.0f;
                } else {
                    return 240.0f;
                }
            } else {
                if (m_currentRange == 0) {
                    return 24.0E-3f;
                } else if (m_currentRange == 1) {
                    return 1.2f;
                } else {
                    return 10.0f;
                }
            }
        }

        return 0;
	}
	
	void addValue(float value) {
        m_value = value;
    }

	float getValue() {
		return roundPrec(ongoingCal ? m_value : calConf[getCalConfIndex()].offsetAndScale(m_value), getResolution());
	}

	int8_t getCurrentRangeMaxValue() {
        if (m_afeVersion == 1) {
            if (m_subchannelIndex == AIN_1_SUBCHANNEL_INDEX || m_subchannelIndex == AIN_2_SUBCHANNEL_INDEX) {
                return 0;
            } else {
                return 3;
            }
        } else if (m_afeVersion == 2) {
            if (m_subchannelIndex == AIN_1_SUBCHANNEL_INDEX || m_subchannelIndex == AIN_2_SUBCHANNEL_INDEX) {
                return -1;
            } else {
                return 1;
            }
        } else if (m_afeVersion == 3) {
            if (m_subchannelIndex == AIN_1_SUBCHANNEL_INDEX) {
                return -1;
            } else if (m_subchannelIndex == AIN_2_SUBCHANNEL_INDEX) {
                return 2;
            } else if (m_subchannelIndex == AIN_3_SUBCHANNEL_INDEX) {
                return -1;
            } else {
                return 3;
            }
        }

        return -1;
	}

	int8_t getVoltageRangeMaxValue() {
        if (m_afeVersion == 1) {
            if (m_subchannelIndex == AIN_1_SUBCHANNEL_INDEX || m_subchannelIndex == AIN_2_SUBCHANNEL_INDEX) {
                return 3;
            }
            else {
                return 2;
            }
        } else if (m_afeVersion == 2) {
            if (m_subchannelIndex == AIN_1_SUBCHANNEL_INDEX || m_subchannelIndex == AIN_2_SUBCHANNEL_INDEX) {
                return 1;
            } else {
                return -1;
            }
        } else if (m_afeVersion == 3) {
            if (m_subchannelIndex == AIN_1_SUBCHANNEL_INDEX) {
                return 2;
            } else if (m_subchannelIndex == AIN_2_SUBCHANNEL_INDEX) {
                return -1;
            } else if (m_subchannelIndex == AIN_3_SUBCHANNEL_INDEX) {
                return 3;
            } else {
                return -1;
            }
        }

        return 0;
	}

    EnumItem *getModeRangeEnumDefinition() {
		static EnumItem g_afe1Ain12ModeRangeEnumDefinition[] = {
			{ 0, "\xbd""2.4 V" },
			{ 1, "\xbd""48 V" },
			{ 2, "\xbd""240 V" },
            { 3, "Voltage auto range" },
            { 4, "\xbd""48 mA" },
			{ 0, 0 }
		};

		static EnumItem g_afe1Ain34ModeRangeEnumDefinition[] = {
			{ 0, "\xbd""2.4 V" },
			{ 1, "\xbd""12 V" },
            { 2, "Voltage auto range" },
			{ 3, "\xbd""48 mA" },
            { 4, "\xbd""1.2 A" },
            { 5, "\xbd""10 A" },
            { 6, "Current auto range" },
			{ 0, 0 }
		};

		static EnumItem g_afe2Ain12ModeRangeEnumDefinition[] = {
			{ 0, "\xbd""3 V" },
			{ 1, "\xbd""12 V" },
			{ 0, 0 }
		};

		static EnumItem g_afe2Ain34ModeRangeEnumDefinition[] = {
			{ 0, "\xbd""2.4 mA / Auto" },
			{ 1, "\xbd""2 A" },
			{ 0, 0 }
		};

		static EnumItem g_afe3Ain1ModeRangeEnumDefinition[] = {
			{ 0, "\xbd""50 V" },
			{ 1, "\xbd""450 V" },
            { 2, "Auto range" },
			{ 0, 0 }
		};

		static EnumItem g_afe3Ain2ModeRangeEnumDefinition[] = {
            { 0, "\xbd""1 A" },
            { 1, "\xbd""10 A" },
            { 2, "Auto range" },
			{ 0, 0 }
		};

		static EnumItem g_afe3Ain3ModeRangeEnumDefinition[] = {
			{ 0, "\xbd""2.4 V" },
			{ 1, "\xbd""48 V" },
			{ 2, "\xbd""240 V" },
            { 3, "Auto range" },
			{ 0, 0 }
		};

		static EnumItem g_afe3Ain4ModeRangeEnumDefinition[] = {
            { 0, "\xbd""24 mA" },
            { 1, "\xbd""1.2 A" },
            { 2, "\xbd""10 A" },
            { 3, "Auto range" },
			{ 0, 0 }
		};

        if (m_afeVersion == 1) {
            if (m_subchannelIndex == AIN_1_SUBCHANNEL_INDEX || m_subchannelIndex == AIN_2_SUBCHANNEL_INDEX) {
                return g_afe1Ain12ModeRangeEnumDefinition;
            } else {
                return g_afe1Ain34ModeRangeEnumDefinition;
            }
        } else if (m_afeVersion == 2) {
            if (m_subchannelIndex == AIN_1_SUBCHANNEL_INDEX|| m_subchannelIndex == AIN_2_SUBCHANNEL_INDEX) {
                return g_afe2Ain12ModeRangeEnumDefinition;
            } else {
                return g_afe2Ain34ModeRangeEnumDefinition;
            }
        } else if (m_afeVersion == 3) {
            if (m_subchannelIndex == AIN_1_SUBCHANNEL_INDEX) {
                return g_afe3Ain1ModeRangeEnumDefinition;
            } else if (m_subchannelIndex == AIN_2_SUBCHANNEL_INDEX) {
                return g_afe3Ain2ModeRangeEnumDefinition;
            } else if (m_subchannelIndex == AIN_3_SUBCHANNEL_INDEX) {
                return g_afe3Ain3ModeRangeEnumDefinition;
            } else {
                return g_afe3Ain4ModeRangeEnumDefinition;
            }
        }

        return nullptr;
    }

    EnumItem *getModeRangeEnumDefinitionForAutoRange() {
		static EnumItem g_afe1Ain12ModeRangeEnumDefinition[] = {
			{ 0, "\xbd""2.4 V*" },
			{ 1, "\xbd""48 V*" },
			{ 2, "\xbd""240 V*" },
			{ 0, 0 }
		};

		static EnumItem g_afe1Ain34ModeRangeEnumDefinition[] = {
			{ 0, "\xbd""2.4 V*" },
			{ 1, "\xbd""12 V*" },
			{ 2, "\xbd""48 mA*" },
            { 3, "\xbd""1.2 A*" },
            { 4, "\xbd""10 A*" },
		};

		static EnumItem g_afe3Ain1ModeRangeEnumDefinition[] = {
			{ 0, "\xbd""50 V*" },
			{ 1, "\xbd""450 V*" },
			{ 0, 0 }
		};

		static EnumItem g_afe3Ain2ModeRangeEnumDefinition[] = {
            { 0, "\xbd""1 A*" },
            { 1, "\xbd""10 A*" },
			{ 0, 0 }
		};

		static EnumItem g_afe3Ain3ModeRangeEnumDefinition[] = {
			{ 0, "\xbd""2.4 V*" },
			{ 1, "\xbd""48 V*" },
			{ 2, "\xbd""240 V*" },
			{ 0, 0 }
		};

		static EnumItem g_afe3Ain4ModeRangeEnumDefinition[] = {
            { 0, "\xbd""24 mA*" },
            { 1, "\xbd""1.2 A*" },
            { 2, "\xbd""10 A*" },
			{ 0, 0 }
		};

        if (m_afeVersion == 1) {
            if (m_subchannelIndex == AIN_1_SUBCHANNEL_INDEX || m_subchannelIndex == AIN_2_SUBCHANNEL_INDEX) {
                return g_afe1Ain12ModeRangeEnumDefinition;
            } else {
                return g_afe1Ain34ModeRangeEnumDefinition;
            }
        } else if (m_afeVersion == 3) {
            if (m_subchannelIndex == AIN_1_SUBCHANNEL_INDEX) {
                return g_afe3Ain1ModeRangeEnumDefinition;
            } else if (m_subchannelIndex == AIN_2_SUBCHANNEL_INDEX) {
                return g_afe3Ain2ModeRangeEnumDefinition;
            } else if (m_subchannelIndex == AIN_3_SUBCHANNEL_INDEX) {
                return g_afe3Ain3ModeRangeEnumDefinition;
            } else {
                return g_afe3Ain4ModeRangeEnumDefinition;
            }
        }

        return nullptr;
    }

    int getCalConfIndex(uint8_t range) {
        if (m_afeVersion == 1) {
            if (m_subchannelIndex == AIN_1_SUBCHANNEL_INDEX || m_subchannelIndex == AIN_2_SUBCHANNEL_INDEX) {
                if (m_mode == SOURCE_MODE_VOLTAGE) {
                    return range;
                } else {
                    return 3;
                }
            } else {
                if (m_mode == SOURCE_MODE_VOLTAGE) {
                    return range;
                } else {
                    return 2 + range;
                }
            }
        } else if (m_afeVersion == 2) {
            return range;
        } else if (m_afeVersion == 3) {
            if (m_subchannelIndex == AIN_1_SUBCHANNEL_INDEX) {
                return range;
            } else if (m_subchannelIndex == AIN_2_SUBCHANNEL_INDEX) {
                return range;
            } else if (m_subchannelIndex == AIN_3_SUBCHANNEL_INDEX) {
                return range;
            } else {
                return 2 + range;
            }
        }

        return -1;
    }

    int getCalConfIndex() {
        if (m_afeVersion == 1) {
            if (m_subchannelIndex == AIN_1_SUBCHANNEL_INDEX || m_subchannelIndex == AIN_2_SUBCHANNEL_INDEX) {
                if (m_mode == SOURCE_MODE_VOLTAGE) {
                    return m_voltageRange == 3 ? m_autoRange : m_voltageRange;
                } else {
                    return 3;
                }
            } else {
                if (m_mode == SOURCE_MODE_VOLTAGE) {
                    return m_voltageRange == 2 ? m_autoRange : m_voltageRange;
                } else {
                    return 2 + (m_currentRange == 3 ? m_autoRange : m_currentRange);
                }
            }
        } else if (m_afeVersion == 2) {
            if (m_subchannelIndex == AIN_1_SUBCHANNEL_INDEX || m_subchannelIndex == AIN_2_SUBCHANNEL_INDEX) {
                return m_voltageRange;
            } else {
                return m_currentRange;
            }
        } else if (m_afeVersion == 3) {
            if (m_subchannelIndex == AIN_1_SUBCHANNEL_INDEX) {
                return m_voltageRange == 2 ? m_autoRange : m_voltageRange;
            } else if (m_subchannelIndex == AIN_2_SUBCHANNEL_INDEX) {
                return m_currentRange == 2 ? m_autoRange : m_currentRange;
            } else if (m_subchannelIndex == AIN_3_SUBCHANNEL_INDEX) {
                return m_voltageRange == 3 ? m_autoRange : m_voltageRange;
            } else {
                return 2 + (m_currentRange == 3 ? m_autoRange : m_currentRange);
            }
        }

        return -1;
    }

    CalConf &getCalConf() {
        return calConf[getCalConfIndex()];
    }

	void getStepValues(StepValues *stepValues) {
		static float ENCODER_STEP_VALUES_CAL[] = { 0.001f, 0.01f, 0.02f, 0.05f };
		static float AMPER_ENCODER_STEP_VALUES_CAL[] = { 0.0001f, 0.0005f, 0.001f, 0.005f };

        if (getMode() == MEASURE_MODE_VOLTAGE) {
            stepValues->values = ENCODER_STEP_VALUES_CAL;
            stepValues->count = sizeof(ENCODER_STEP_VALUES_CAL) / sizeof(float);
            stepValues->unit = UNIT_VOLT;
        } else {
            stepValues->values = AMPER_ENCODER_STEP_VALUES_CAL;
            stepValues->count = sizeof(AMPER_ENCODER_STEP_VALUES_CAL) / sizeof(float);
            stepValues->unit = UNIT_AMPER;
        }

		stepValues->encoderSettings.accelerationEnabled = true;
		stepValues->encoderSettings.range = getMaxValue() - getMinValue();
		stepValues->encoderSettings.step = getResolution();
	}

    EncoderMode getEncoderMode() {
        if (getMode() == MEASURE_MODE_VOLTAGE) {
            return edit_mode_step::g_mio168AinVoltageEncoderMode;
        } else {
            return edit_mode_step::g_mio168AinCurrentEncoderMode;
        }
    }

    void setEncoderMode(EncoderMode encoderMode) {
        if (getMode() == MEASURE_MODE_VOLTAGE) {
			edit_mode_step::g_mio168AinVoltageEncoderMode = encoderMode;
        } else {
			edit_mode_step::g_mio168AinCurrentEncoderMode = encoderMode;
        }
    }
};

static EnumItem g_aoutCurrentRangeEnumDefinition[] = {
	{ 5, "4 mA - 20 mA" },
	{ 6, "0 mA - 20 mA" },
	{ 7, "0 mA - 24 mA" },
	{ 0, 0 }
};

static EnumItem g_aoutVoltageRangeEnumDefinition[] = {
	{ 0, "0 V - 5 V" },
	{ 1, "0 V - 10 V" },
	{ 2, "\xbd""5 V" },
	{ 3, "\xbd""10 V" },
	{ 0, 0 }
};

struct AoutDac7760Channel {
    bool m_outputEnabled = false;
    uint8_t m_mode = SOURCE_MODE_VOLTAGE;

private:
    uint8_t m_currentRange = 5;
	uint8_t m_voltageRange = 0;

public:
	float m_currentValue = 0;
    float m_voltageValue = 0;

    CalConf calConf[7];
    bool ongoingCal = false;

    char m_label[CHANNEL_LABEL_MAX_LENGTH + 1];

    TriggerMode m_triggerMode;

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
        uint8_t triggerMode;
    };

    void resetProfileToDefaults(ProfileParameters &parameters) {
        parameters.outputEnabled = false;
        parameters.mode = SOURCE_MODE_VOLTAGE;
        parameters.currentRange = 5;
        parameters.voltageRange = 0;
        parameters.currentValue = 0;
        parameters.voltageValue = 0;
        *parameters.label = 0;
        parameters.triggerMode = TRIGGER_MODE_FIXED;
    }

    void getProfileParameters(ProfileParameters &parameters) {
        parameters.outputEnabled = m_outputEnabled;
        parameters.mode = m_mode;
        parameters.currentRange = m_currentRange;
        parameters.voltageRange = m_voltageRange;
        parameters.currentValue = m_currentValue;
        parameters.voltageValue = m_voltageValue;
        memcpy(parameters.label, m_label, sizeof(m_label));
        parameters.triggerMode = m_triggerMode;
    }

    void setProfileParameters(ProfileParameters &parameters) {
        m_outputEnabled = parameters.outputEnabled;
        m_mode = parameters.mode;
        m_currentRange = parameters.currentRange;
        m_voltageRange = parameters.voltageRange;
        m_currentValue = parameters.currentValue;
        m_voltageValue = parameters.voltageValue;
        memcpy(m_label, parameters.label, sizeof(m_label));
		m_triggerMode = (TriggerMode)parameters.triggerMode;
    }

    bool writeProfileProperties(psu::profile::WriteContext &ctx, int i, ProfileParameters &parameters) {
        char propName[32];

        snprintf(propName, sizeof(propName), "aout_dac7760_%d_outputEnabled", i+1);
        WRITE_PROPERTY(propName, parameters.outputEnabled);

        snprintf(propName, sizeof(propName), "aout_dac7760_%d_mode", i+1);
        WRITE_PROPERTY(propName, parameters.mode);

        snprintf(propName, sizeof(propName), "aout_dac7760_%d_currentRange", i+1);
        WRITE_PROPERTY(propName, parameters.currentRange);

        snprintf(propName, sizeof(propName), "aout_dac7760_%d_voltageRange", i+1);
        WRITE_PROPERTY(propName, parameters.voltageRange);

        snprintf(propName, sizeof(propName), "aout_dac7760_%d_currentValue", i+1);
        WRITE_PROPERTY(propName, parameters.currentValue);

        snprintf(propName, sizeof(propName), "aout_dac7760_%d_voltageValue", i+1);
        WRITE_PROPERTY(propName, parameters.voltageValue);

        snprintf(propName, sizeof(propName), "aout_dac7760_%d_label", i+1);
        WRITE_PROPERTY(propName, parameters.label);

        snprintf(propName, sizeof(propName), "aout_dac7760_%d_triggerMode", i+1);
        WRITE_PROPERTY(propName, parameters.triggerMode);

        return true;
    }

    bool readProfileProperties(psu::profile::ReadContext &ctx, int i, ProfileParameters &parameters) {
        char propName[32];

        snprintf(propName, sizeof(propName), "aout_dac7760_%d_outputEnabled", i+1);
        READ_PROPERTY(propName, parameters.outputEnabled);

        snprintf(propName, sizeof(propName), "aout_dac7760_%d_mode", i+1);
        READ_PROPERTY(propName, parameters.mode);

        snprintf(propName, sizeof(propName), "aout_dac7760_%d_currentRange", i+1);
        READ_PROPERTY(propName, parameters.currentRange);

        snprintf(propName, sizeof(propName), "aout_dac7760_%d_voltageRange", i+1);
        READ_PROPERTY(propName, parameters.voltageRange);

        snprintf(propName, sizeof(propName), "aout_dac7760_%d_currentValue", i+1);
        READ_PROPERTY(propName, parameters.currentValue);

        snprintf(propName, sizeof(propName), "aout_dac7760_%d_voltageValue", i+1);
        READ_PROPERTY(propName, parameters.voltageValue);

        snprintf(propName, sizeof(propName), "aout_dac7760_%d_label", i+1);
        READ_STRING_PROPERTY(propName, parameters.label, CHANNEL_LABEL_MAX_LENGTH);

        snprintf(propName, sizeof(propName), "aout_dac7760_%d_triggerMode", i+1);
        READ_PROPERTY(propName, parameters.triggerMode);

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
        m_triggerMode = TRIGGER_MODE_FIXED;
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
		if (m_currentValue < getCurrentMinValue()) {
			m_currentValue = getCurrentMinValue();
		} else if (m_currentValue > getCurrentMaxValue()) {
			m_currentValue = getCurrentMaxValue();
		}
    }

    int8_t getVoltageRange() {
        return m_voltageRange;
    }
    
    void setVoltageRange(int8_t range) {
        m_voltageRange = range;
		if (m_voltageValue < getVoltageMinValue()) {
			m_voltageValue = getVoltageMinValue();
		} else if (m_voltageValue > getVoltageMaxValue()) {
			m_voltageValue = getVoltageMaxValue();
		}
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

    CalConf &getCalConf() {
        return calConf[getCalConfIndex()];
    }

    float getCalibratedValue() {
        return ongoingCal ? getValue() : getCalConf().offsetAndScale(getValue());
    }

    float getCalibratedValue(float value) {
        return getCalConf().offsetAndScale(value);
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
		float resolution = m_voltageRange == 0 ? 0.002 : 0.005;
		return ongoingCal ? 0.1f * resolution : resolution;
    }

    float getCurrentResolution() {
		float resolution = 1E-5f; // 10 uA
        return ongoingCal ? 0.1f * resolution : resolution;
    }

    float getResolution() {
        if (getMode() == SOURCE_MODE_VOLTAGE) {
            return getVoltageResolution();
        } else {
            return getCurrentResolution();
        }
    }

    void getVoltageStepValues(StepValues *stepValues) {
        if (ongoingCal) {
            stepValues->values = AOUT_DAC7760_ENCODER_STEP_VALUES_CAL;
            stepValues->count = sizeof(AOUT_DAC7760_ENCODER_STEP_VALUES_CAL) / sizeof(float);
            stepValues->unit = UNIT_VOLT;
        } else {
            if (m_voltageRange == 3) {
                stepValues->values = AOUT_DAC7760_ENCODER_STEP_VALUES_20V;
                stepValues->count = sizeof(AOUT_DAC7760_ENCODER_STEP_VALUES_20V) / sizeof(float);
            } else {
                stepValues->values = AOUT_DAC7760_ENCODER_STEP_VALUES;
                stepValues->count = sizeof(AOUT_DAC7760_ENCODER_STEP_VALUES) / sizeof(float);
            }
            stepValues->unit = UNIT_VOLT;
        }
    }

    void getCurrentStepValues(StepValues *stepValues) {
        if (ongoingCal) {
            stepValues->values = AOUT_DAC7760_AMPER_ENCODER_STEP_VALUES_CAL;
            stepValues->count = sizeof(AOUT_DAC7760_AMPER_ENCODER_STEP_VALUES_CAL) / sizeof(float);
            stepValues->unit = UNIT_AMPER;
        } else {
            stepValues->values = AOUT_DAC7760_AMPER_ENCODER_STEP_VALUES;
            stepValues->count = sizeof(AOUT_DAC7760_AMPER_ENCODER_STEP_VALUES) / sizeof(float);
            stepValues->unit = UNIT_AMPER;
        }
    }

    void getStepValues(StepValues *stepValues) {
        if (getMode() == SOURCE_MODE_VOLTAGE) {
            getVoltageStepValues(stepValues);
        } else {
            getCurrentStepValues(stepValues);
        }

        stepValues->encoderSettings.accelerationEnabled = true;
        stepValues->encoderSettings.range = getMaxValue() - getMinValue();
        stepValues->encoderSettings.step = getResolution();
    }

    EncoderMode getEncoderMode() {
        if (getMode() == SOURCE_MODE_VOLTAGE) {
            return edit_mode_step::g_mio168AoutVoltageEncoderMode;
        } else {
            return edit_mode_step::g_mio168AoutCurrentEncoderMode;
        }
    }

    void setEncoderMode(EncoderMode encoderMode) {
        if (getMode() == SOURCE_MODE_VOLTAGE) {
			edit_mode_step::g_mio168AoutVoltageEncoderMode = encoderMode;
        } else {
			edit_mode_step::g_mio168AoutCurrentEncoderMode = encoderMode;
        }
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

    TriggerMode m_triggerMode;

    AoutDac7563Channel() {
        memset(&calConf, 0, sizeof(CalConf));
    }

    struct ProfileParameters {
        float value;
        char label[CHANNEL_LABEL_MAX_LENGTH + 1];
        uint8_t triggerMode;
    };

    float getCalibratedValue() {
        return ongoingCal ? m_value : calConf.offsetAndScale(m_value);
    }

    float getCalibratedValue(float value) {
        return calConf.offsetAndScale(value);
    }

    void resetProfileToDefaults(ProfileParameters &parameters) {
        parameters.value = 0;
        *parameters.label = 0;
        parameters.triggerMode = TRIGGER_MODE_FIXED;
    }

    void getProfileParameters(ProfileParameters &parameters) {
        parameters.value = m_value;
        memcpy(parameters.label, m_label, sizeof(m_label));
        parameters.triggerMode = m_triggerMode;
    }

    void setProfileParameters(ProfileParameters &parameters) {
        m_value = parameters.value;
        memcpy(m_label, parameters.label, sizeof(m_label));
		m_triggerMode = (TriggerMode)parameters.triggerMode;
    }

    bool writeProfileProperties(psu::profile::WriteContext &ctx, int i, ProfileParameters &parameters) {
        char propName[32];

        snprintf(propName, sizeof(propName), "aout_dac7563_%d_value", i+1);
        WRITE_PROPERTY(propName, parameters.value);

        snprintf(propName, sizeof(propName), "aout_dac7563_%d_label", i+1);
        WRITE_PROPERTY(propName, parameters.label);

        snprintf(propName, sizeof(propName), "aout_dac7563_%d_triggerMode", i+1);
        WRITE_PROPERTY(propName, parameters.triggerMode);

        return true;
    }

    bool readProfileProperties(psu::profile::ReadContext &ctx, int i, ProfileParameters &parameters) {
        char propName[32];

        snprintf(propName, sizeof(propName), "aout_dac7563_%d_value", i+1);
        READ_PROPERTY(propName, parameters.value);

        snprintf(propName, sizeof(propName), "aout_dac7563_%d_label", i+1);
        READ_STRING_PROPERTY(propName, parameters.label, CHANNEL_LABEL_MAX_LENGTH);

        snprintf(propName, sizeof(propName), "aout_dac7563_%d_triggerMode", i+1);
        READ_PROPERTY(propName, parameters.triggerMode);

        return false;
    }

    void resetConfiguration() {
        m_value = 0;
        *m_label = 0;
        m_triggerMode = TRIGGER_MODE_FIXED;
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

    EncoderMode getEncoderMode() {
        return edit_mode_step::g_mio168AoutVoltageEncoderMode;
    }

    void setEncoderMode(EncoderMode encoderMode) {
		edit_mode_step::g_mio168AoutVoltageEncoderMode = encoderMode;
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

        snprintf(propName, sizeof(propName), "pwm_%d_freq", i+1);
        WRITE_PROPERTY(propName, parameters.freq);

        snprintf(propName, sizeof(propName), "pwm_%d_duty", i+1);
        WRITE_PROPERTY(propName, parameters.duty);

        snprintf(propName, sizeof(propName), "pwm_%d_label", i+1);
        WRITE_PROPERTY(propName, parameters.label);

        return true;
    }

    bool readProfileProperties(psu::profile::ReadContext &ctx, int i, ProfileParameters &parameters) {
        char propName[32];

        snprintf(propName, sizeof(propName), "pwm_%d_freq", i+1);
        READ_PROPERTY(propName, parameters.freq);

        snprintf(propName, sizeof(propName), "pwm_%d_duty", i+1);
        READ_PROPERTY(propName, parameters.duty);

        snprintf(propName, sizeof(propName), "pwm_%d_label", i+1);
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
    uint8_t afeVersion;
#if defined(EEZ_PLATFORM_SIMULATOR)
	uint8_t simulatorAfeVersion = 1;
#endif

    bool powerDown = false;
    bool synchronized = false;

    uint32_t input[(sizeof(Request) + 3) / 4 + 1];
    uint32_t output[(sizeof(Request) + 3) / 4];

    bool spiReady = false;
    bool spiDmaTransferCompleted = false;
    int spiDmaTransferStatus;

    uint32_t lastTransferTime = 0;
	SetParams lastTransferredParams;
    bool forceTransferSetParams;

	struct CommandDef {
		uint32_t command;
		void (Mio168Module::*fillRequest)(Request &request);
		void (Mio168Module::*done)(Response &response, bool isSuccess);
	};

    static const CommandDef getInfo_command;
    static const CommandDef getState_command;
    static const CommandDef setParams_command;

    static const CommandDef dlogRecordingStart_command;
    static const CommandDef dlogRecordingStop_command;
    static const CommandDef dlogRecordingData_command;

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

    const CommandDef *currentCommand = nullptr;
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
	AinChannel ainChannels[4]{ 
		AinChannel(AIN_1_SUBCHANNEL_INDEX),
		AinChannel(AIN_2_SUBCHANNEL_INDEX),
		AinChannel(AIN_3_SUBCHANNEL_INDEX),
		AinChannel(AIN_4_SUBCHANNEL_INDEX)
	};
    AoutDac7760Channel aoutDac7760Channels[2];
    AoutDac7563Channel aoutDac7563Channels[2];
    PwmChannel pwmChannels[2];

    uint16_t ainFaultStatus;
    bool isFaultP(int subchannelIndex) {
        return (ainFaultStatus & (1 << (8 + subchannelIndex))) != 0;
    }
    bool isFaultN(int subchannelIndex) {
        return (ainFaultStatus & (1 << (subchannelIndex))) != 0;
    }

    uint8_t ainDiagStatus = 3;
    bool isError(int subchannelIndex) {
		if (afeVersion == 3 && (subchannelIndex == 0 || subchannelIndex == 1)) {
			return (ainDiagStatus & (1 << (subchannelIndex))) == 0;
		}
		return false;
    }

    float activePower = 0;
    float reactivePower = 0;
    float voltRMS = 0;
    float currRMS = 0;
    float apparentPower;
    float powerFactor;

    uint8_t calibrationChannelMode;
    uint8_t calibrationChannelCurrentRange;
    uint8_t calibrationChannelVoltageRange;

    const CommandDef *nextDlogCommand = nullptr;
    DlogRecordingStart nextDlogRecordingStart;
    DlogRecordingStart dlogRecordingStart;
    uint32_t dlogDataRecordIndex = 0;

    uint8_t selectedPageInMaxView = 0;
	uint8_t selectedPageInDefaultView = 0;

	uint8_t efficiencyFormula = EFFICIENCY_FORMULA_P1_OVER_P2;
	uint8_t acAnalysisEnabled;

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
    }

    Module *createModule() override {
        return new Mio168Module();
    }

    void initChannels() override {
        powerDown = false;

#if defined(EEZ_PLATFORM_SIMULATOR)
		simulatorAfeVersion = persist_conf::getAfeVersion(slotIndex);
		if (simulatorAfeVersion < 1 || simulatorAfeVersion > 4) {
			simulatorAfeVersion = 1;
		}
#endif

        if (!synchronized) {
            setTestResult(TEST_CONNECTING);

			executeCommand(&getInfo_command);

//			if (!g_isBooted) {
				while (state != STATE_IDLE) {
                    WATCHDOG_RESET(WATCHDOG_LONG_OPERATION);
#if defined(EEZ_PLATFORM_STM32)
					if (HAL_GPIO_ReadPin(spi::IRQ_GPIO_Port[slotIndex], spi::IRQ_Pin[slotIndex]) == GPIO_PIN_RESET) {
                        osDelay(1);
                        if (HAL_GPIO_ReadPin(spi::IRQ_GPIO_Port[slotIndex], spi::IRQ_Pin[slotIndex]) == GPIO_PIN_RESET) {
						    spiReady = true;
                        }
					}
#endif
					tick();
                    osDelay(1);
				}
//			}

            forceTransferSetParams = true;
        }

        if (synchronized) {
			for (int otherChannelIndex = 0; otherChannelIndex < numOtherChannels; otherChannelIndex++) {
				int subchannelIndex = numPowerChannels + otherChannelIndex;
				g_slots[slotIndex]->loadChannelCalibration(subchannelIndex, nullptr);
			}
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

			for (int i = 0; i < 4; i++) {
				ainChannels[i].setAfeVersion(afeVersion);
			}

			firmwareVersionAcquired = true;

            synchronized = true;
            setTestResult(TEST_OK);
        } else {
            synchronized = false;
            if (firmwareInstalled) {
                event_queue::pushEvent(event_queue::EVENT_ERROR_SLOT1_SYNC_ERROR + slotIndex);
            }
            setTestResult(TEST_FAILED);
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
                channel.m_autoRange = data.ainRange[i];
            }
            ainFaultStatus = data.ainFaultStatus;
            ainDiagStatus = data.ainDiagStatus;
	        activePower = data.activePower;
	        reactivePower = data.reactivePower;
	        voltRMS = data.voltRMS;
	        currRMS = data.currRMS;
	        apparentPower = voltRMS * currRMS;
	        powerFactor = activePower / apparentPower;

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
						dlog_record::abortAfterBufferOverflowError(1);
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

        for (int i = 0; i < 8; i++) {
			auto waveformParameters = function_generator::getWaveformParameters(slotIndex, DOUT_SUBCHANNEL_INDEX, i);
			if (!powerDown && waveformParameters && doutChannel.m_triggerMode[i] == TRIGGER_MODE_FUNCTION_GENERATOR && function_generator::isActive()) {
                params.doutStates &= ~(1 << i);

				params.doutWaveformParameters[i].waveform = waveformParameters->waveform;
				params.doutWaveformParameters[i].frequency = waveformParameters->frequency;

				params.doutWaveformParameters[i].phaseShift = waveformParameters->phaseShift;
				params.doutWaveformParameters[i].dutyCycle = waveformParameters->dutyCycle;
			} else {
				params.doutWaveformParameters[i].waveform = Waveform::WAVEFORM_NONE;
			}
        }

        for (int i = 0; i < 4; i++) {
            auto channel = &ainChannels[i];
            params.ain[i].mode = channel->getMode();
            params.ain[i].range = channel->getMode() == MEASURE_MODE_VOLTAGE ? channel->getVoltageRange() : channel->getCurrentRange();
            params.ain[i].nplc = channel->ongoingCal ? 25.0f : (channel->getMode() == MEASURE_MODE_VOLTAGE ? channel->m_voltageNPLC : channel->m_currentNPLC);
            
            int calConfIndex = channel->getCalConfIndex(0);
            for (int j = 0; j < 3; j++) {
                auto &calConf = channel->calConf[calConfIndex + j];
                if (calConf.state.calEnabled) {
                    params.ain[i].p1CalX[j] = calConf.points[0].calValue;
                    params.ain[i].p1CalY[j] = calConf.points[0].uncalValue;
                    params.ain[i].p2CalX[j] = calConf.points[1].calValue;
                    params.ain[i].p2CalY[j] = calConf.points[1].uncalValue;
                } else {
                    params.ain[i].p1CalX[j] = 0;
                    params.ain[i].p1CalY[j] = 0;
                    params.ain[i].p2CalX[j] = 1;
                    params.ain[i].p2CalY[j] = 1;
                }
            }
        }

		params.acAnalysisEnabled = acAnalysisEnabled && !(dlog_record::isExecuting() && dlog_record::isModuleAtSlotRecording(slotIndex));

        params.powerLineFrequency = persist_conf::getPowerLineFrequency();

        for (int i = 0; i < 2; i++) {
            auto channel = &aoutDac7760Channels[i];
            params.aout_dac7760[i].outputEnabled = !powerDown && channel->m_outputEnabled;
            params.aout_dac7760[i].outputRange = channel->getMode() == SOURCE_MODE_VOLTAGE ? channel->getVoltageRange() : channel->getCurrentRange();
            params.aout_dac7760[i].outputValue = channel->getCalibratedValue();

			auto waveformParameters = function_generator::getWaveformParameters(slotIndex, AOUT_1_SUBCHANNEL_INDEX + i, 0);
			if (!powerDown && !channel->ongoingCal && waveformParameters && channel->m_triggerMode == TRIGGER_MODE_FUNCTION_GENERATOR && function_generator::isActive()) {
                params.aout_dac7760[i].outputEnabled = 1;

				params.aoutWaveformParameters[i].waveform = waveformParameters->waveform;
				params.aoutWaveformParameters[i].frequency = waveformParameters->frequency;
				params.aoutWaveformParameters[i].phaseShift = waveformParameters->phaseShift;
				params.aoutWaveformParameters[i].amplitude = channel->getCalibratedValue(waveformParameters->amplitude);
				params.aoutWaveformParameters[i].offset = channel->getCalibratedValue(waveformParameters->offset);
				params.aoutWaveformParameters[i].dutyCycle = waveformParameters->dutyCycle;

                float min;
                float max;

                if (waveformParameters->waveform == function_generator::WAVEFORM_DC) {
                    min = waveformParameters->amplitude;
                    max = waveformParameters->amplitude;
                } else {
                    min = waveformParameters->offset - waveformParameters->amplitude;
                    max = waveformParameters->offset + waveformParameters->amplitude;
                }

                if (waveformParameters->resourceType == FUNCTION_GENERATOR_RESOURCE_TYPE_U) {
                    if (min >= 0 && max <= 5.0f) {
                        params.aout_dac7760[i].outputRange = 0;
                    } else if (min >= 0 && max <= 10.0f) {
                        params.aout_dac7760[i].outputRange = 1;
                    } else if (min >= -5.0f && max <= 5.0f) {
                        params.aout_dac7760[i].outputRange = 2;
                    } else {
                        params.aout_dac7760[i].outputRange = 3;
                    }
                } else {
                    if (min >= 0.004f && max <= 0.02f) {
                        params.aout_dac7760[i].outputRange = 5;
                    } else if (min >= 0 && max <= 0.02f) {
                        params.aout_dac7760[i].outputRange = 6;
                    } else {
                        params.aout_dac7760[i].outputRange = 7;
                    }
                }
			} else {
				params.aoutWaveformParameters[i].waveform = Waveform::WAVEFORM_NONE;
			}
        }

        for (int i = 0; i < 2; i++) {
            auto channel = &aoutDac7563Channels[i];
            params.aout_dac7563[i].voltage = powerDown ? 0 : channel->getCalibratedValue();

			auto waveformParameters = function_generator::getWaveformParameters(slotIndex, AOUT_3_SUBCHANNEL_INDEX + i, 0);
			if (!powerDown && !channel->ongoingCal && waveformParameters && channel->m_triggerMode == TRIGGER_MODE_FUNCTION_GENERATOR && function_generator::isActive()) {
				params.aoutWaveformParameters[2 + i].waveform = waveformParameters->waveform;
				params.aoutWaveformParameters[2 + i].frequency = waveformParameters->frequency;
				params.aoutWaveformParameters[2 + i].phaseShift = waveformParameters->phaseShift;
				params.aoutWaveformParameters[2 + i].amplitude = channel->getCalibratedValue(waveformParameters->amplitude);
				params.aoutWaveformParameters[2 + i].offset = channel->getCalibratedValue(waveformParameters->offset);
				params.aoutWaveformParameters[2 + i].dutyCycle = waveformParameters->dutyCycle;
			} else {
				params.aoutWaveformParameters[2 + i].waveform = Waveform::WAVEFORM_NONE;
			}
        }

        for (int i = 0; i < 2; i++) {
            auto channel = &pwmChannels[i];
            params.pwm[i].freq = powerDown ? 0 : channel->m_freq;
            params.pwm[i].duty = powerDown ? 0 : channel->m_duty;
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

    ////////////////////////////////////////

    void Command_DlogRecordingStop_Done(Response &response, bool isSuccess) {
        if (isSuccess) {
        }
    }

    ////////////////////////////////////////

    void Command_DlogRecordingData_OnResponse(Response &response) {
        if (isModuleControlledRecordingExecuting()) {
            auto &dlogRecordingData = response.dlogRecordingData;

            if (dlogRecordingData.numRecords == 0xFFFF) {
            	// buffer overflow
            	dlog_record::abortAfterBufferOverflowError(1);
            	return;
            }

            while (dlogDataRecordIndex < dlogRecordingData.recordIndex) {
                dlog_record::logInvalid();
                dlogDataRecordIndex++;
            }

            uint8_t *rx = dlogRecordingData.buffer;
            uint32_t end = dlogRecordingData.recordIndex + dlogRecordingData.numRecords;

            auto dinResources = dlogRecordingStart.resources;
            auto doutResources = dlogRecordingStart.resources >> 8;
            auto ainResources = dlogRecordingStart.resources >> 16;

            if (ainResources) {
                bool is24Bit = dlog_record::g_recordingParameters.period >= 1.0f / 16000;
                
                uint32_t logAin[4] = {
                    ainResources & (1 << 0),
                    ainResources & (1 << 1),
                    ainResources & (1 << 2),
                    ainResources & (1 << 3)
                };

                uint8_t values[4 * 3];

                if (is24Bit) {
                    while (dlogDataRecordIndex < end) {
                        uint8_t *p = values;

                        for (int j = 0; j < 4; j++) {
                            if (logAin[j]) {
                                *p++ = *rx++;
                                *p++ = *rx++;
                                *p++ = *rx++;
                            } else {
                                rx += 3;
                            }
                        }

                        uint32_t bits = rx[0] | (rx[1] << 8);
                        rx += 2;

                        dlog_record::logInt24(values, bits);
                        
                        dlogDataRecordIndex++;
                    }
                } else {
                    while (dlogDataRecordIndex < end) {
                        uint8_t *p = values;

                        for (int j = 0; j < 4; j++) {
                            if (logAin[j]) {
                                *p++ = *rx++;
                                *p++ = *rx++;
                            } else {
                                rx += 2;
                            }
                        }

                        uint32_t bits = rx[0] | (rx[1] << 8);
                        rx += 2;

                        dlog_record::logInt16(values, bits);
                        
                        dlogDataRecordIndex++;
                    }
                }
            } else {
                if (dinResources && doutResources) {
                    while (dlogDataRecordIndex < end) {
                        uint32_t bits = rx[0] | (rx[1] << 8);
                        rx += 2;
                        dlog_record::log(bits);
                        dlogDataRecordIndex++;
                    }
                } else if (dinResources) {
                    while (dlogDataRecordIndex < end) {
                        uint32_t bits = *rx++;
                        dlog_record::log(bits);
                        dlogDataRecordIndex++;
                    }
                } else {
                    while (dlogDataRecordIndex < end) {
                        uint32_t bits = *rx++ << 8;
                        dlog_record::log(bits);
                        dlogDataRecordIndex++;
                    }
                }
            }
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
			uint32_t dlogPeriodMs = (uint32_t)(dlog_record::g_activeRecording.parameters.period * 1000);
			if (dlogPeriodMs < REFRESH_TIME_MS) {
				if (dlog_record::isModuleAtSlotRecording(slotIndex)) {
                    if (!dlog_record::isModuleControlledRecording()) {
					    return dlogPeriodMs;
                    }
				}
			}
		}

		return REFRESH_TIME_MS;
	}

    bool isModuleControlledRecordingExecuting() {
		return dlog_record::isExecuting() &&
			dlog_record::isModuleAtSlotRecording(slotIndex) &&
			dlog_record::isModuleControlledRecording();
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
        spiReady = false;
        spiDmaTransferCompleted = false;
        auto status = bp3c::comm::transferDMA(slotIndex, (uint8_t *)output, (uint8_t *)input, sizeof(Request));
        return status == bp3c::comm::TRANSFER_STATUS_OK;
    }

    bool getCommandResult() {
        Request &request = *(Request *)output;
        request.command = COMMAND_NONE;
        spiReady = false;
        spiDmaTransferCompleted = false;
        auto status = bp3c::comm::transferDMA(slotIndex, (uint8_t *)output, (uint8_t *)input, sizeof(Request));
        return status == bp3c::comm::TRANSFER_STATUS_OK;
    }

    bool isCommandResponse() {
        Response &response = *(Response *)input;
        return response.command == (0x8000 | currentCommand->command);
    }

    void doRetry() {
    	bp3c::comm::abortTransfer(slotIndex);
        
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

        if (powerDown) {
            synchronized = false;
            setTestResult(TEST_FAILED);
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

            Response &response = *(Response *)input;
            if (response.command == (0x8000 | COMMAND_DLOG_RECORDING_DATA)) {
                Command_DlogRecordingData_OnResponse(response);
            }
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
            if (numCrcErrors >= MAX_DMA_TRANSFER_ERRORS) {
                event_queue::pushEvent(event_queue::EVENT_ERROR_SLOT1_CRC_CHECK_ERROR + slotIndex);
                synchronized = false;
                setTestResult(TEST_FAILED);
            }
            //else if (numCrcErrors > 5) {
            //    DebugTrace("Slot %d CRC error no. %d\n", slotIndex + 1, numCrcErrors);
            //}
        } else {
            numTransferErrors++;
            if (numTransferErrors >= MAX_DMA_TRANSFER_ERRORS) {
                event_queue::pushEvent(event_queue::EVENT_ERROR_SLOT1_SYNC_ERROR + slotIndex);
                synchronized = false;
                setTestResult(TEST_FAILED);
            }
            //else if (numTransferErrors > 5) {
            //    DebugTrace("Slot %d SPI transfer error %d no. %d\n", slotIndex + 1, status, numTransferErrors);
            //}
        }
    }

    void pumpCurrentCommand() {
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

                    response->command = 0x8000 | currentCommand->command;

                    if (currentCommand->command == COMMAND_GET_INFO) {
                        response->getInfo.firmwareMajorVersion = 1;
                        response->getInfo.firmwareMinorVersion = 0;
                        response->getInfo.idw0 = 0;
                        response->getInfo.idw1 = 0;
                        response->getInfo.idw2 = 0;
                        response->getInfo.afeVersion = simulatorAfeVersion;
                    } else if (currentCommand->command == COMMAND_GET_STATE) {
						memset(&response->getState, 0, sizeof(response->getState));
                        response->getState.flags |= GET_STATE_COMMAND_FLAG_SD_CARD_PRESENT;
						response->getState.ainDiagStatus = 3;
                    } else {
                        if (
                            currentCommand->command == COMMAND_DLOG_RECORDING_DATA ||
                            (state == STATE_WAIT_DMA_TRANSFER_COMPLETED_FOR_REQUEST && isModuleControlledRecordingExecuting())
                        ) {
                            response->command = 0x8000 | COMMAND_DLOG_RECORDING_DATA;

                            bool is24Bit = dlog_record::g_recordingParameters.period >= 1.0f / 16000;

                            response->dlogRecordingData.recordIndex = dlogDataRecordIndex;
                            response->dlogRecordingData.numRecords = (uint16_t)roundf(1.0f / dlog_record::g_recordingParameters.period / 1000);

							static int32_t ain[4] = {
								ain[0] = 0x7FFFFFFF / 5,
								ain[1] = 0x7FFFFFFF / 5 * 2,
								ain[2] = 0x7FFFFFFF / 5 * 3,
								ain[3] = 0x7FFFFFFF / 5 * 4
							};

                            uint8_t *p = response->dlogRecordingData.buffer;

                            for (int i = 0; i < response->dlogRecordingData.numRecords; i++) {
                                if (is24Bit) {
									for (int j = 0; j < 4; j++) {
										ain[j] -= 10000;
										*p++ = (ain[j] >> 24) & 0xFF;
										*p++ = (ain[j] >> 16) & 0xFF;
										*p++ = (ain[j] >>  8) & 0xFF;
									}
								} else {
									for (int j = 0; j < 4; j++) {
										ain[j] -= 10000;
										*p++ = (ain[j] >> 24) & 0xFF;
										*p++ = (ain[j] >> 16) & 0xFF;
									}
								}
                                *p++ = 0x55;
                                *p++ = 0xAA;
                            }
                        }
                    }

                    stateTransition(EVENT_DMA_TRANSFER_COMPLETED);
                    #endif
                } 
                
                uint32_t tickCountMs = millis();
                if (tickCountMs - lastStateTransitionTime >= TIMEOUT_TIME_MS) {
                    stateTransition(EVENT_TIMEOUT);
                }
            }
        } 
    }

    void pumpNextCommand() {
        if (!currentCommand) {
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
#ifdef EEZ_PLATFORM_STM32
                    event_queue::pushEvent(event_queue::EVENT_ERROR_SLOT1_SYNC_ERROR + slotIndex);
                    synchronized = false;
                    setTestResult(TEST_FAILED);
#endif
                } else {
                    if (forceTransferSetParams) {
                        forceTransferSetParams = false;
                        executeCommand(&setParams_command);
                    } else {
                        SetParams params;
                        fillSetParams(params);
                        if (memcmp(&params, &lastTransferredParams, sizeof(SetParams)) != 0) {
                            executeCommand(&setParams_command);
                        } else if (tickCountMs - lastRefreshTime >= getRefreshTimeMs()) {
                            refreshStartTime = tickCountMs;
                            executeCommand(&getState_command);
                        } else if (isModuleControlledRecordingExecuting()) {
                            executeCommand(&dlogRecordingData_command);
                        }
                    }
                }
            }
        }
    }

    void tick() override {
        pumpCurrentCommand();
        pumpNextCommand();
        pumpCurrentCommand();
    }

    void onSpiIrq() override {
        spiReady = true;
		// if (g_isBooted) {
		// 	stateTransition(EVENT_SLAVE_READY);
		// }
    }

    void onSpiDmaTransferCompleted(int status) override {
        //  if (g_isBooted) {
        //      if (status == bp3c::comm::TRANSFER_STATUS_OK) {
        //          stateTransition(EVENT_DMA_TRANSFER_COMPLETED);
        //      } else {
        //          reportDmaTransferFailed(status);
        //          stateTransition(EVENT_DMA_TRANSFER_FAILED);
        //      }
        //  } else {
             spiDmaTransferCompleted = true;
             spiDmaTransferStatus = status;
        //  }
    }

    void onPowerDown() override {
        powerDown = true;
        if (bp3c::flash_slave::g_bootloaderMode) {
            synchronized = false;
            setTestResult(TEST_FAILED);
        } else {
            executeCommand(&setParams_command);
        }

#if defined(EEZ_PLATFORM_SIMULATOR)
        sendMessageToLowPriorityThread(THREAD_MESSAGE_FS_DRIVER_UNLINK, slotIndex, 0);
#endif
    }

    void resync() override {
        if (!synchronized) {
            executeCommand(&getInfo_command);
        }
    }

    Page *getPageFromId(int pageId) override;

    void animatePageAppearance(int previousPageId, int activePageId) override {
        if (previousPageId == PAGE_ID_MAIN && activePageId == PAGE_ID_DIB_MIO168_SETTINGS) {
            psu::gui::animateSlideDown();
        } else if (previousPageId == PAGE_ID_DIB_MIO168_SETTINGS && activePageId == PAGE_ID_MAIN) {
            psu::gui::animateSlideUp();
        } else if (
            (previousPageId == PAGE_ID_SYS_SETTINGS_LABELS_AND_COLORS && activePageId == PAGE_ID_DIB_MIO168_CHANNEL_LABELS) ||
            (previousPageId == PAGE_ID_DIB_MIO168_CHANNEL_LABELS && activePageId == PAGE_ID_DIB_MIO168_DIN_CHANNEL_LABELS) ||
            (previousPageId == PAGE_ID_DIB_MIO168_CHANNEL_LABELS && activePageId == PAGE_ID_DIB_MIO168_DOUT_CHANNEL_LABELS) ||
			((previousPageId == PAGE_ID_DIB_MIO168_SETTINGS || previousPageId == PAGE_ID_MAIN) && activePageId == PAGE_ID_DIB_MIO168_DIN_CONFIGURATION) ||
			((previousPageId == PAGE_ID_DIB_MIO168_SETTINGS || previousPageId == PAGE_ID_MAIN) && activePageId == PAGE_ID_DIB_MIO168_AIN_CONFIGURATION) ||
			((previousPageId == PAGE_ID_DIB_MIO168_SETTINGS || previousPageId == PAGE_ID_MAIN) && activePageId == PAGE_ID_DIB_MIO168_AOUT_DAC7563_CONFIGURATION) ||
			((previousPageId == PAGE_ID_DIB_MIO168_SETTINGS || previousPageId == PAGE_ID_MAIN) && activePageId == PAGE_ID_DIB_MIO168_AOUT_DAC7760_CONFIGURATION) ||
			((previousPageId == PAGE_ID_DIB_MIO168_SETTINGS || previousPageId == PAGE_ID_MAIN) && activePageId == PAGE_ID_DIB_MIO168_INFO) ||
			(previousPageId == PAGE_ID_DIB_MIO168_AIN_CONFIGURATION && activePageId == PAGE_ID_CH_SETTINGS_CALIBRATION) ||
			(previousPageId == PAGE_ID_DIB_MIO168_AOUT_DAC7563_CONFIGURATION && activePageId == PAGE_ID_CH_SETTINGS_CALIBRATION) ||
			(previousPageId == PAGE_ID_DIB_MIO168_AOUT_DAC7760_CONFIGURATION && activePageId == PAGE_ID_CH_SETTINGS_CALIBRATION)
        ) {
            psu::gui::animateSlideLeft();
        } else if (
            (previousPageId == PAGE_ID_DIB_MIO168_CHANNEL_LABELS && activePageId == PAGE_ID_SYS_SETTINGS_LABELS_AND_COLORS) ||
            (previousPageId == PAGE_ID_DIB_MIO168_DIN_CHANNEL_LABELS && activePageId == PAGE_ID_DIB_MIO168_CHANNEL_LABELS) ||
            (previousPageId == PAGE_ID_DIB_MIO168_DOUT_CHANNEL_LABELS && activePageId == PAGE_ID_DIB_MIO168_CHANNEL_LABELS) ||
			(previousPageId == PAGE_ID_DIB_MIO168_DIN_CONFIGURATION && (activePageId == PAGE_ID_DIB_MIO168_SETTINGS || activePageId == PAGE_ID_MAIN)) ||
			(previousPageId == PAGE_ID_DIB_MIO168_AIN_CONFIGURATION && (activePageId == PAGE_ID_DIB_MIO168_SETTINGS || activePageId == PAGE_ID_MAIN)) ||
			(previousPageId == PAGE_ID_DIB_MIO168_AOUT_DAC7563_CONFIGURATION && (activePageId == PAGE_ID_DIB_MIO168_SETTINGS || activePageId == PAGE_ID_MAIN)) ||
			(previousPageId == PAGE_ID_DIB_MIO168_AOUT_DAC7760_CONFIGURATION && (activePageId == PAGE_ID_DIB_MIO168_SETTINGS || activePageId == PAGE_ID_MAIN)) ||
			(previousPageId == PAGE_ID_DIB_MIO168_INFO && (activePageId == PAGE_ID_DIB_MIO168_SETTINGS || activePageId == PAGE_ID_MAIN)) ||
			(previousPageId == PAGE_ID_CH_SETTINGS_CALIBRATION && activePageId == PAGE_ID_DIB_MIO168_AIN_CONFIGURATION) ||
			(previousPageId == PAGE_ID_CH_SETTINGS_CALIBRATION && activePageId == PAGE_ID_DIB_MIO168_AOUT_DAC7563_CONFIGURATION) ||
			(previousPageId == PAGE_ID_CH_SETTINGS_CALIBRATION && activePageId == PAGE_ID_DIB_MIO168_AOUT_DAC7760_CONFIGURATION)
		) {
            psu::gui::animateSlideRight();
        } else if (
			(previousPageId == PAGE_ID_DIB_MIO168_AOUT_DAC7760_CONFIGURATION && activePageId == PAGE_ID_SYS_SETTINGS_FUNCTION_GENERATOR) ||
			(previousPageId == PAGE_ID_DIB_MIO168_AOUT_DAC7760_CONFIGURATION && activePageId == PAGE_ID_SYS_SETTINGS_TRIGGER) ||
			(previousPageId == PAGE_ID_DIB_MIO168_AOUT_DAC7563_CONFIGURATION && activePageId == PAGE_ID_SYS_SETTINGS_FUNCTION_GENERATOR) ||
			(previousPageId == PAGE_ID_DIB_MIO168_AOUT_DAC7563_CONFIGURATION && activePageId == PAGE_ID_SYS_SETTINGS_TRIGGER)
		) {
			psu::gui::animateSlideLeft();
		} else if (
			(previousPageId == PAGE_ID_SYS_SETTINGS_FUNCTION_GENERATOR && activePageId == PAGE_ID_DIB_MIO168_AOUT_DAC7760_CONFIGURATION) ||
			(previousPageId == PAGE_ID_SYS_SETTINGS_TRIGGER && activePageId == PAGE_ID_DIB_MIO168_AOUT_DAC7760_CONFIGURATION) ||
			(previousPageId == PAGE_ID_SYS_SETTINGS_FUNCTION_GENERATOR && activePageId == PAGE_ID_DIB_MIO168_AOUT_DAC7563_CONFIGURATION) ||
			(previousPageId == PAGE_ID_SYS_SETTINGS_TRIGGER && activePageId == PAGE_ID_DIB_MIO168_AOUT_DAC7563_CONFIGURATION)
		) {
			psu::gui::animateSlideRight();
		}
    }

    int getSlotView(SlotViewType slotViewType, int slotIndex, int cursor) override {
        if (slotViewType == SLOT_VIEW_TYPE_DEFAULT) {
            return isDefaultViewVertical() ? PAGE_ID_DIB_MIO168_SLOT_VIEW_DEF_VERT : PAGE_ID_DIB_MIO168_SLOT_VIEW_DEF_HORZ;
        }
        if (slotViewType == SLOT_VIEW_TYPE_DEFAULT_2COL) {
            return isDefaultViewVertical() ? PAGE_ID_DIB_MIO168_SLOT_VIEW_DEF_2COL_VERT : PAGE_ID_DIB_MIO168_SLOT_VIEW_DEF_2COL_HORZ;
        }
        assert (slotViewType == SLOT_VIEW_TYPE_MAX);
        return PAGE_ID_DIB_MIO168_SLOT_VIEW_MAX;
    }

    int getSlotSettingsPageId() override {
        return PAGE_ID_DIB_MIO168_SETTINGS;
    }

    int getLabelsAndColorsPageId() override {
        return getTestResult() == TEST_OK ? PAGE_ID_DIB_MIO168_LABELS_AND_COLORS : PAGE_ID_NONE;
    }

    void onHighPriorityThreadMessage(uint8_t type, uint32_t param) override;

    struct ProfileParameters : public Module::ProfileParameters  {
        DinChannel::ProfileParameters dinChannel;
        DoutChannel::ProfileParameters doutChannel;
        AinChannel::ProfileParameters ainChannels[4];
        AoutDac7760Channel::ProfileParameters aoutDac7760Channels[2];
        AoutDac7563Channel::ProfileParameters aoutDac7563Channels[2];
        PwmChannel::ProfileParameters pwmChannels[2];
        uint8_t selectedPageInMaxView;
		uint8_t selectedPageInDefaultView;
		uint8_t efficiencyFormula;
        uint8_t acAnalysisEnabled;
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

        selectedPageInMaxView = 0;
		selectedPageInDefaultView = 0;
		
		efficiencyFormula = EFFICIENCY_FORMULA_P1_OVER_P2;
        acAnalysisEnabled = 0;
    }

    void getProfileParameters(uint8_t *buffer) override {
        Module::getProfileParameters(buffer);

        assert(sizeof(ProfileParameters) < MAX_SLOT_PARAMETERS_SIZE);

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

        parameters->selectedPageInMaxView = selectedPageInMaxView;
		parameters->selectedPageInDefaultView = selectedPageInDefaultView;

		parameters->efficiencyFormula = efficiencyFormula;
        parameters->acAnalysisEnabled = acAnalysisEnabled;
    }
    
    void setProfileParameters(uint8_t *buffer, bool mismatch, int recallOptions) override {
        Module::setProfileParameters(buffer, mismatch, recallOptions);

        auto parameters = (ProfileParameters *)buffer;

        dinChannel.setProfileParameters(parameters->dinChannel);
        doutChannel.setProfileParameters(parameters->doutChannel);
        for (int i = 0; i < 4; i++) {
            ainChannels[i].setProfileParameters(parameters->ainChannels[i]);
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

        selectedPageInMaxView = parameters->selectedPageInMaxView;
		selectedPageInDefaultView = parameters->selectedPageInDefaultView;

		efficiencyFormula = parameters->efficiencyFormula;
        acAnalysisEnabled = parameters->acAnalysisEnabled;
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

        WRITE_PROPERTY("selectedPageInMaxView", parameters->selectedPageInMaxView);
		WRITE_PROPERTY("selectedPageInDefaultView", parameters->selectedPageInDefaultView);

		WRITE_PROPERTY("efficiencyFormula", parameters->efficiencyFormula);
        WRITE_PROPERTY("acAnalysisEnabled", parameters->acAnalysisEnabled);

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

        READ_PROPERTY("selectedPageInMaxView", parameters->selectedPageInMaxView);
		READ_PROPERTY("selectedPageInDefaultView", parameters->selectedPageInDefaultView);

		READ_PROPERTY("efficiencyFormula", parameters->efficiencyFormula);
        READ_PROPERTY("acAnalysisEnabled", parameters->acAnalysisEnabled);

        return false;
    }

    void resetConfiguration() override {
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

        selectedPageInMaxView = 0;
		selectedPageInDefaultView = 0;

		efficiencyFormula = EFFICIENCY_FORMULA_P1_OVER_P2;
        acAnalysisEnabled = 0;
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

    const char *getShortChannelLabel(int subchannelIndex) {
        if (subchannelIndex == AOUT_1_SUBCHANNEL_INDEX) {
            return "AO1";
        }
        if (subchannelIndex == AOUT_2_SUBCHANNEL_INDEX) {
            return "AO2";
        }
        if (subchannelIndex == AOUT_3_SUBCHANNEL_INDEX) {
            return "AO3";
        }
        if (subchannelIndex == AOUT_4_SUBCHANNEL_INDEX) {
            return "AO4";
        }
        if (subchannelIndex == PWM_1_SUBCHANNEL_INDEX) {
            return "P1";
        }
        if (subchannelIndex == PWM_2_SUBCHANNEL_INDEX) {
            return "P2";
        }
        return getDefaultChannelLabel(subchannelIndex);
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
            stringCopy(ainChannels[subchannelIndex - AIN_1_SUBCHANNEL_INDEX].m_label, length + 1, label);
            return SCPI_RES_OK;
        }
        
        if (subchannelIndex >= AOUT_1_SUBCHANNEL_INDEX && subchannelIndex <= AOUT_2_SUBCHANNEL_INDEX) {
            stringCopy(aoutDac7760Channels[subchannelIndex - AOUT_1_SUBCHANNEL_INDEX].m_label, length + 1, label);
            return SCPI_RES_OK;
        }
        
        if (subchannelIndex >= AOUT_3_SUBCHANNEL_INDEX && subchannelIndex <= AOUT_4_SUBCHANNEL_INDEX) {
            stringCopy(aoutDac7563Channels[subchannelIndex - AOUT_3_SUBCHANNEL_INDEX].m_label, length + 1, label);
            return SCPI_RES_OK;
        }
        
        if (subchannelIndex >= PWM_1_SUBCHANNEL_INDEX && subchannelIndex <= PWM_2_SUBCHANNEL_INDEX) {
            stringCopy(pwmChannels[subchannelIndex - PWM_1_SUBCHANNEL_INDEX].m_label, length + 1, label);
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

                stringCopy(pinLabel, length + 1, label);

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

#if defined(EEZ_PLATFORM_SIMULATOR)
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

    bool getSourceMode(int subchannelIndex, SourceMode &mode, int *err) override {
		if (subchannelIndex < AOUT_1_SUBCHANNEL_INDEX || subchannelIndex > AOUT_2_SUBCHANNEL_INDEX) {
            if (err) {
                *err = SCPI_ERROR_HARDWARE_MISSING;
            }
            return false;
        }
        mode = aoutDac7760Channels[subchannelIndex - AOUT_1_SUBCHANNEL_INDEX].getMode();
        return true;
    }
    
    bool setSourceMode(int subchannelIndex, SourceMode mode, int *err) override {
		if (subchannelIndex < AOUT_1_SUBCHANNEL_INDEX || subchannelIndex > AOUT_2_SUBCHANNEL_INDEX) {
            if (err) {
                *err = SCPI_ERROR_HARDWARE_MISSING;
            }
            return false;
        }
        aoutDac7760Channels[subchannelIndex - AOUT_1_SUBCHANNEL_INDEX].setMode(mode);
        return true;
    }

    bool getSourceCurrentRange(int subchannelIndex, uint8_t &range, int *err) override {
		if (subchannelIndex < AOUT_1_SUBCHANNEL_INDEX || subchannelIndex > AOUT_2_SUBCHANNEL_INDEX) {
            if (err) {
                *err = SCPI_ERROR_HARDWARE_MISSING;
            }
            return false;
        }
        range = aoutDac7760Channels[subchannelIndex - AOUT_1_SUBCHANNEL_INDEX].getCurrentRange() - 4; // 5:7 -> 1:3
        return true;
    }
    
    bool setSourceCurrentRange(int subchannelIndex, uint8_t range, int *err) override {
		if (subchannelIndex < AOUT_1_SUBCHANNEL_INDEX || subchannelIndex > AOUT_2_SUBCHANNEL_INDEX) {
            if (err) {
                *err = SCPI_ERROR_HARDWARE_MISSING;
            }
            return false;
        }

        range += 4; // 1:3 -> 5:7

        if (range < 5 || range > 7) {
            if (err) {
                *err = SCPI_ERROR_ILLEGAL_PARAMETER_VALUE;
            }
            return false;
        }

        aoutDac7760Channels[subchannelIndex - AOUT_1_SUBCHANNEL_INDEX].setCurrentRange(range);
        return true;
    }

    bool getSourceVoltageRange(int subchannelIndex, uint8_t &range, int *err) override {
        if (subchannelIndex < AOUT_1_SUBCHANNEL_INDEX || subchannelIndex > AOUT_2_SUBCHANNEL_INDEX) {
            if (err) {
                *err = SCPI_ERROR_HARDWARE_MISSING;
            }
            return false;
        }
        range = aoutDac7760Channels[subchannelIndex - AOUT_1_SUBCHANNEL_INDEX].getVoltageRange() + 1; // 0:3 -> 1:4
        return true;
    }
    
    bool setSourceVoltageRange(int subchannelIndex, uint8_t range, int *err) override {
		if (subchannelIndex < AOUT_1_SUBCHANNEL_INDEX || subchannelIndex > AOUT_2_SUBCHANNEL_INDEX) {
            if (err) {
                *err = SCPI_ERROR_HARDWARE_MISSING;
            }
            return false;
        }

        range -= 1; // 1:4 -> 0:3

        if (range < 0 || range > 3) {
            if (err) {
                *err = SCPI_ERROR_ILLEGAL_PARAMETER_VALUE;
            }
            return false;
        }

        aoutDac7760Channels[subchannelIndex - AOUT_1_SUBCHANNEL_INDEX].setVoltageRange(range);
        return true;
    }

    bool getMeasureMode(int subchannelIndex, MeasureMode &mode, int *err) override {
		if (subchannelIndex < AIN_1_SUBCHANNEL_INDEX || subchannelIndex > AIN_4_SUBCHANNEL_INDEX || isError(subchannelIndex - AIN_1_SUBCHANNEL_INDEX)) {
            if (err) {
                *err = SCPI_ERROR_HARDWARE_MISSING;
            }
            return false;
        }
        mode = (MeasureMode)ainChannels[subchannelIndex - AIN_1_SUBCHANNEL_INDEX].getMode();
        return true;
    }
    
    bool setMeasureMode(int subchannelIndex, MeasureMode mode, int *err) override {
		if (subchannelIndex < AIN_1_SUBCHANNEL_INDEX || subchannelIndex > AIN_4_SUBCHANNEL_INDEX || isError(subchannelIndex - AIN_1_SUBCHANNEL_INDEX)) {
            if (err) {
                *err = SCPI_ERROR_HARDWARE_MISSING;
            }
            return false;
        }
        ainChannels[subchannelIndex - AIN_1_SUBCHANNEL_INDEX].setMode(mode);
        return true;
    }

    bool getMeasureCurrentRange(int subchannelIndex, uint8_t &range, int *err) override {
		if (subchannelIndex < AIN_1_SUBCHANNEL_INDEX || subchannelIndex > AIN_4_SUBCHANNEL_INDEX || isError(subchannelIndex - AIN_1_SUBCHANNEL_INDEX)) {
            if (err) {
                *err = SCPI_ERROR_HARDWARE_MISSING;
            }
            return false;
        }
        range = ainChannels[subchannelIndex - AIN_1_SUBCHANNEL_INDEX].getCurrentRange() + 1;
        return true;
    }
    
    bool setMeasureCurrentRange(int subchannelIndex, uint8_t range, int *err) override {
		if (subchannelIndex < AIN_1_SUBCHANNEL_INDEX || subchannelIndex > AIN_4_SUBCHANNEL_INDEX || isError(subchannelIndex - AIN_1_SUBCHANNEL_INDEX)) {
            if (err) {
                *err = SCPI_ERROR_HARDWARE_MISSING;
            }
            return false;
        }
        
        range -= 1;

        if ((int8_t)range > ainChannels[subchannelIndex - AIN_1_SUBCHANNEL_INDEX].getCurrentRangeMaxValue()) {
            if (err) {
                *err = SCPI_ERROR_ILLEGAL_PARAMETER_VALUE;
            }
            return false;
        }

        ainChannels[subchannelIndex - AIN_1_SUBCHANNEL_INDEX].setCurrentRange(range);

        return true;
    }

	bool getMeasureVoltageRange(int subchannelIndex, uint8_t &range, int *err) override {
		if (subchannelIndex < AIN_1_SUBCHANNEL_INDEX || subchannelIndex > AIN_4_SUBCHANNEL_INDEX || isError(subchannelIndex - AIN_1_SUBCHANNEL_INDEX)) {
			if (err) {
				*err = SCPI_ERROR_HARDWARE_MISSING;
			}
			return false;
		}
		range = ainChannels[subchannelIndex - AIN_1_SUBCHANNEL_INDEX].getVoltageRange() + 1;
		return true;
	}

	bool setMeasureVoltageRange(int subchannelIndex, uint8_t range, int *err) override {
		if (subchannelIndex < AIN_1_SUBCHANNEL_INDEX || subchannelIndex > AIN_4_SUBCHANNEL_INDEX || isError(subchannelIndex - AIN_1_SUBCHANNEL_INDEX)) {
			if (err) {
				*err = SCPI_ERROR_HARDWARE_MISSING;
			}
			return false;
		}

        range -= 1;

		if ((int8_t)range > ainChannels[subchannelIndex - AIN_1_SUBCHANNEL_INDEX].getVoltageRangeMaxValue()) {
			if (err) {
				*err = SCPI_ERROR_ILLEGAL_PARAMETER_VALUE;
			}
			return false;
		}

		ainChannels[subchannelIndex - AIN_1_SUBCHANNEL_INDEX].setVoltageRange(range);

		return true;
	}

    bool getMeasureCurrentNPLC(int subchannelIndex, float &nplc, int *err) override {
		if (subchannelIndex < AIN_1_SUBCHANNEL_INDEX || subchannelIndex > AIN_4_SUBCHANNEL_INDEX || isError(subchannelIndex - AIN_1_SUBCHANNEL_INDEX)) {
			if (err) {
				*err = SCPI_ERROR_HARDWARE_MISSING;
			}
			return false;
		}
        nplc = ainChannels[subchannelIndex - AIN_1_SUBCHANNEL_INDEX].m_currentNPLC;
        return true;
    }

    bool setMeasureCurrentNPLC(int subchannelIndex, float nplc, int *err) override {
		if (subchannelIndex < AIN_1_SUBCHANNEL_INDEX || subchannelIndex > AIN_4_SUBCHANNEL_INDEX || isError(subchannelIndex - AIN_1_SUBCHANNEL_INDEX)) {
			if (err) {
				*err = SCPI_ERROR_HARDWARE_MISSING;
			}
			return false;
		}
 
		if (nplc < 0.0f || nplc > 25.0f) {
			if (err) {
				*err = SCPI_ERROR_ILLEGAL_PARAMETER_VALUE;
			}
			return false;
		}

		ainChannels[subchannelIndex - AIN_1_SUBCHANNEL_INDEX].m_currentNPLC = nplc;

		return true;
    }

    bool getMeasureVoltageNPLC(int subchannelIndex, float &nplc, int *err) override {
		if (subchannelIndex < AIN_1_SUBCHANNEL_INDEX || subchannelIndex > AIN_4_SUBCHANNEL_INDEX || isError(subchannelIndex - AIN_1_SUBCHANNEL_INDEX)) {
			if (err) {
				*err = SCPI_ERROR_HARDWARE_MISSING;
			}
			return false;
		}
        nplc = ainChannels[subchannelIndex - AIN_1_SUBCHANNEL_INDEX].m_voltageNPLC;
        return true;
    }

    bool setMeasureVoltageNPLC(int subchannelIndex, float nplc, int *err) override {
		if (subchannelIndex < AIN_1_SUBCHANNEL_INDEX || subchannelIndex > AIN_4_SUBCHANNEL_INDEX || isError(subchannelIndex - AIN_1_SUBCHANNEL_INDEX)) {
			if (err) {
				*err = SCPI_ERROR_HARDWARE_MISSING;
			}
			return false;
		}
 
		if (nplc < 0.0f || nplc > 25.0f) {
			if (err) {
				*err = SCPI_ERROR_ILLEGAL_PARAMETER_VALUE;
			}
			return false;
		}

		ainChannels[subchannelIndex - AIN_1_SUBCHANNEL_INDEX].m_voltageNPLC = nplc;

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
        if (subchannelIndex < AIN_1_SUBCHANNEL_INDEX || subchannelIndex > AIN_4_SUBCHANNEL_INDEX || isError(subchannelIndex - AIN_1_SUBCHANNEL_INDEX)) {
            if (err) {
                *err = SCPI_ERROR_HARDWARE_MISSING;
            }
            return false;
        }

        auto &channel = ainChannels[subchannelIndex - AIN_1_SUBCHANNEL_INDEX];

        if (channel.getMode() != MEASURE_MODE_VOLTAGE) {
            if (err) {
                *err = SCPI_ERROR_EXECUTION_ERROR;
            }
            return false;
        }

        value = channel.getValue();
        return true;
    }

    void getVoltageStepValues(int subchannelIndex, StepValues *stepValues, bool calibrationMode) override {
        if (subchannelIndex >= AOUT_1_SUBCHANNEL_INDEX && subchannelIndex <= AOUT_2_SUBCHANNEL_INDEX) {
            aoutDac7760Channels[subchannelIndex - AOUT_1_SUBCHANNEL_INDEX].getStepValues(stepValues);
            stepValues->encoderSettings.mode = aoutDac7760Channels[subchannelIndex - AOUT_1_SUBCHANNEL_INDEX].getEncoderMode();
        } else if (subchannelIndex >= AOUT_3_SUBCHANNEL_INDEX && subchannelIndex <= AOUT_4_SUBCHANNEL_INDEX) {
        	aoutDac7563Channels[subchannelIndex - AOUT_3_SUBCHANNEL_INDEX].getStepValues(stepValues);
            stepValues->encoderSettings.mode = aoutDac7563Channels[subchannelIndex - AOUT_3_SUBCHANNEL_INDEX].getEncoderMode();
        } else if (subchannelIndex >= AIN_1_SUBCHANNEL_INDEX && subchannelIndex <= AIN_4_SUBCHANNEL_INDEX) {
        	ainChannels[subchannelIndex - AIN_1_SUBCHANNEL_INDEX].getStepValues(stepValues);
            stepValues->encoderSettings.mode = ainChannels[subchannelIndex - AIN_1_SUBCHANNEL_INDEX].getEncoderMode();
        }
	}

    void setVoltageEncoderMode(int subchannelIndex, EncoderMode encoderMode) override {
        if (subchannelIndex >= AOUT_1_SUBCHANNEL_INDEX && subchannelIndex <= AOUT_2_SUBCHANNEL_INDEX) {
            aoutDac7760Channels[subchannelIndex - AOUT_1_SUBCHANNEL_INDEX].setEncoderMode(encoderMode);
        } else if (subchannelIndex >= AOUT_3_SUBCHANNEL_INDEX && subchannelIndex <= AOUT_4_SUBCHANNEL_INDEX) {
            aoutDac7563Channels[subchannelIndex - AOUT_3_SUBCHANNEL_INDEX].setEncoderMode(encoderMode);
        }
    }
    
    float getVoltageResolution(int subchannelIndex) override {
        if (subchannelIndex >= AOUT_1_SUBCHANNEL_INDEX && subchannelIndex <= AOUT_2_SUBCHANNEL_INDEX) {
            return aoutDac7760Channels[subchannelIndex - AOUT_1_SUBCHANNEL_INDEX].getVoltageResolution();
		} else if (subchannelIndex >= AOUT_3_SUBCHANNEL_INDEX && subchannelIndex <= AOUT_4_SUBCHANNEL_INDEX) {
            return aoutDac7563Channels[subchannelIndex - AOUT_3_SUBCHANNEL_INDEX].getResolution();
		} else {
			return ainChannels[subchannelIndex - AIN_1_SUBCHANNEL_INDEX].getResolution();
		}
    }

    float getVoltageMinValue(int subchannelIndex) override {
        if (subchannelIndex >= AOUT_1_SUBCHANNEL_INDEX && subchannelIndex <= AOUT_2_SUBCHANNEL_INDEX) {
            return aoutDac7760Channels[subchannelIndex - AOUT_1_SUBCHANNEL_INDEX].getVoltageMinValue();
		} else if (subchannelIndex >= AOUT_3_SUBCHANNEL_INDEX && subchannelIndex <= AOUT_4_SUBCHANNEL_INDEX) {
			return AOUT_DAC7563_MIN;
        } else if (subchannelIndex >= AIN_1_SUBCHANNEL_INDEX && subchannelIndex <= AIN_4_SUBCHANNEL_INDEX) {
			return ainChannels[subchannelIndex - AIN_1_SUBCHANNEL_INDEX].getMinValue();
		}
		return NAN;
    }

    float getVoltageMaxValue(int subchannelIndex) override {
        if (subchannelIndex >= AOUT_1_SUBCHANNEL_INDEX && subchannelIndex <= AOUT_2_SUBCHANNEL_INDEX) {
            return aoutDac7760Channels[subchannelIndex - AOUT_1_SUBCHANNEL_INDEX].getVoltageMaxValue();
        } else if (subchannelIndex >= AOUT_3_SUBCHANNEL_INDEX && subchannelIndex <= AOUT_4_SUBCHANNEL_INDEX) {
            return AOUT_DAC7563_MAX;
		} else if (subchannelIndex >= AIN_1_SUBCHANNEL_INDEX && subchannelIndex <= AIN_4_SUBCHANNEL_INDEX) {
			return ainChannels[subchannelIndex - AIN_1_SUBCHANNEL_INDEX].getMaxValue();
		}
		return NAN;
    }

    bool isConstantVoltageMode(int subchannelIndex) override {
        return true;
    }

    bool isVoltageCalibrationExists(int subchannelIndex) override {
        if (getCalibrationValueType(subchannelIndex) == CALIBRATION_VALUE_U) {
            if (subchannelIndex >= AOUT_1_SUBCHANNEL_INDEX && subchannelIndex <= AOUT_2_SUBCHANNEL_INDEX) {
                return aoutDac7760Channels[subchannelIndex - AOUT_1_SUBCHANNEL_INDEX].getCalConf().state.calState;
            } else if (subchannelIndex >= AOUT_3_SUBCHANNEL_INDEX && subchannelIndex <= AOUT_4_SUBCHANNEL_INDEX) {
                return aoutDac7563Channels[subchannelIndex - AOUT_3_SUBCHANNEL_INDEX].calConf.state.calState;
            } else if (subchannelIndex >= AIN_1_SUBCHANNEL_INDEX && subchannelIndex <= AIN_4_SUBCHANNEL_INDEX) {
                return ainChannels[subchannelIndex - AIN_1_SUBCHANNEL_INDEX].getCalConf().state.calState;
            }
        }
        return false;
    }

    bool isVoltageCalibrationEnabled(int subchannelIndex) override {
        if (getCalibrationValueType(subchannelIndex) == CALIBRATION_VALUE_U) {
            if (subchannelIndex >= AOUT_1_SUBCHANNEL_INDEX && subchannelIndex <= AOUT_2_SUBCHANNEL_INDEX) {
                return aoutDac7760Channels[subchannelIndex - AOUT_1_SUBCHANNEL_INDEX].getCalConf().state.calEnabled;
            } else if (subchannelIndex >= AOUT_3_SUBCHANNEL_INDEX && subchannelIndex <= AOUT_4_SUBCHANNEL_INDEX) {
                return aoutDac7563Channels[subchannelIndex - AOUT_3_SUBCHANNEL_INDEX].calConf.state.calEnabled;
            } else if (subchannelIndex >= AIN_1_SUBCHANNEL_INDEX && subchannelIndex <= AIN_4_SUBCHANNEL_INDEX) {
                return ainChannels[subchannelIndex - AIN_1_SUBCHANNEL_INDEX].getCalConf().state.calEnabled;
            }
        }
        return false;
    }
    
    void enableVoltageCalibration(int subchannelIndex, bool enabled) override {
        if (getCalibrationValueType(subchannelIndex) == CALIBRATION_VALUE_U) {
            if (subchannelIndex >= AOUT_1_SUBCHANNEL_INDEX && subchannelIndex <= AOUT_2_SUBCHANNEL_INDEX) {
                aoutDac7760Channels[subchannelIndex - AOUT_1_SUBCHANNEL_INDEX].getCalConf().state.calEnabled = enabled;
            } else if (subchannelIndex >= AOUT_3_SUBCHANNEL_INDEX && subchannelIndex <= AOUT_4_SUBCHANNEL_INDEX) {
                aoutDac7563Channels[subchannelIndex - AOUT_3_SUBCHANNEL_INDEX].calConf.state.calEnabled = enabled;
            } else if (subchannelIndex >= AIN_1_SUBCHANNEL_INDEX && subchannelIndex <= AIN_4_SUBCHANNEL_INDEX) {
                ainChannels[subchannelIndex - AIN_1_SUBCHANNEL_INDEX].getCalConf().state.calEnabled = enabled;
            } else {
                return;
            }
            saveChannelCalibration(subchannelIndex, nullptr);
        }
    }

    bool loadChannelCalibration(int subchannelIndex, int *err) override {
        assert(sizeof(CalConf) <= 64);

        auto afeCheck = persist_conf::getAfeVersion(slotIndex) == afeVersion;

		if (subchannelIndex >= AOUT_1_SUBCHANNEL_INDEX && subchannelIndex <= AOUT_2_SUBCHANNEL_INDEX) {
			for (int i = 0; i < 7; i++) {
				CalConf *calConf = &aoutDac7760Channels[subchannelIndex - AOUT_1_SUBCHANNEL_INDEX].calConf[i];
				if (!afeCheck || !persist_conf::loadChannelCalibrationConfiguration(slotIndex, (subchannelIndex - AOUT_1_SUBCHANNEL_INDEX) * 7 + i, &calConf->header, sizeof(CalConf), CalConf::VERSION)) {
					calConf->clear();
				}
			}
		} else if (subchannelIndex >= AOUT_3_SUBCHANNEL_INDEX && subchannelIndex <= AOUT_4_SUBCHANNEL_INDEX) {
			CalConf *calConf = &aoutDac7563Channels[subchannelIndex - AOUT_3_SUBCHANNEL_INDEX].calConf;
			if (!afeCheck || !persist_conf::loadChannelCalibrationConfiguration(slotIndex, 2 * 7 + (subchannelIndex - AOUT_3_SUBCHANNEL_INDEX), &calConf->header, sizeof(CalConf), CalConf::VERSION)) {
				calConf->clear();
			}
		} else if (subchannelIndex >= AIN_1_SUBCHANNEL_INDEX && subchannelIndex <= AIN_4_SUBCHANNEL_INDEX) {
			if (isError(subchannelIndex - AIN_1_SUBCHANNEL_INDEX)) {
				if (err) {
					*err = SCPI_ERROR_HARDWARE_MISSING;
				}
				return false;
			}
			for (int i = 0; i < 5; i++) {
				CalConf *calConf = &ainChannels[subchannelIndex - AIN_1_SUBCHANNEL_INDEX].calConf[i];
				if (!afeCheck || !persist_conf::loadChannelCalibrationConfiguration(slotIndex, 2 * 7 + 2 + (subchannelIndex - AIN_1_SUBCHANNEL_INDEX) * 5 + i, &calConf->header, sizeof(CalConf), CalConf::VERSION)) {
					calConf->clear();
				}
			}
		}

        return true;
    }

    bool saveChannelCalibration(int subchannelIndex, int *err) override {
        CalConf *calConf = nullptr;
        int calConfIndex = 0;

        if (subchannelIndex >= AOUT_1_SUBCHANNEL_INDEX && subchannelIndex <= AOUT_2_SUBCHANNEL_INDEX) {
            int dac7760ChannelIndex = subchannelIndex - AOUT_1_SUBCHANNEL_INDEX;
            calConf = &aoutDac7760Channels[dac7760ChannelIndex].getCalConf();
            calConfIndex = dac7760ChannelIndex * 7 + aoutDac7760Channels[dac7760ChannelIndex].getCalConfIndex();
        } else if (subchannelIndex >= AOUT_3_SUBCHANNEL_INDEX && subchannelIndex <= AOUT_4_SUBCHANNEL_INDEX) {
            calConf = &aoutDac7563Channels[subchannelIndex - AOUT_3_SUBCHANNEL_INDEX].calConf;
            calConfIndex = 2 * 7 + (subchannelIndex - AOUT_3_SUBCHANNEL_INDEX);
        } else if (subchannelIndex >= AIN_1_SUBCHANNEL_INDEX && subchannelIndex <= AIN_4_SUBCHANNEL_INDEX) {
			if (isError(subchannelIndex - AIN_1_SUBCHANNEL_INDEX)) {
				if (err) {
					*err = SCPI_ERROR_HARDWARE_MISSING;
				}
				return false;
			}
            int ainChannelIndex = subchannelIndex - AIN_1_SUBCHANNEL_INDEX;
            calConf = &ainChannels[ainChannelIndex].getCalConf();
            calConfIndex = 2 * 7 + 2 + ainChannelIndex * 5 + ainChannels[ainChannelIndex].getCalConfIndex();
        }

        if (calConf) {
            auto result = persist_conf::saveChannelCalibrationConfiguration(slotIndex, calConfIndex, &calConf->header, sizeof(CalConf), CalConf::VERSION);
            if (result) {
                if (persist_conf::getAfeVersion(slotIndex) != afeVersion) {
                    persist_conf::setAfeVersion(slotIndex, afeVersion);
                }
            }
            return result;
        }

        if (err) {
            *err = SCPI_ERROR_HARDWARE_MISSING;
        }
        return false;
    }

    void initChannelCalibration(int subchannelIndex) override {
        if (subchannelIndex >= AOUT_1_SUBCHANNEL_INDEX && subchannelIndex <= AOUT_2_SUBCHANNEL_INDEX) {
            auto &channel = aoutDac7760Channels[subchannelIndex - AOUT_1_SUBCHANNEL_INDEX];
            calibrationChannelMode = channel.getMode();
            calibrationChannelCurrentRange = channel.getCurrentRange();
            calibrationChannelVoltageRange = channel.getVoltageRange();
        } else if (subchannelIndex >= AIN_1_SUBCHANNEL_INDEX && subchannelIndex <= AIN_4_SUBCHANNEL_INDEX) {
            auto &channel = ainChannels[subchannelIndex - AIN_1_SUBCHANNEL_INDEX];
            calibrationChannelMode = channel.getMode();
            calibrationChannelCurrentRange = channel.getCurrentRange();
            calibrationChannelVoltageRange = channel.getVoltageRange();
        }
    }

    void startChannelCalibration(int subchannelIndex) override {
        if (subchannelIndex >= AOUT_1_SUBCHANNEL_INDEX && subchannelIndex <= AOUT_2_SUBCHANNEL_INDEX) {
            auto &channel = aoutDac7760Channels[subchannelIndex - AOUT_1_SUBCHANNEL_INDEX];
            channel.ongoingCal = 1;
            channel.m_mode = calibrationChannelMode;
            channel.setVoltageRange(calibrationChannelVoltageRange);
            channel.setCurrentRange(calibrationChannelCurrentRange);
        } else if (subchannelIndex >= AOUT_3_SUBCHANNEL_INDEX && subchannelIndex <= AOUT_4_SUBCHANNEL_INDEX) {
            aoutDac7563Channels[subchannelIndex - AOUT_3_SUBCHANNEL_INDEX].ongoingCal = 1;
        } else if (subchannelIndex >= AIN_1_SUBCHANNEL_INDEX && subchannelIndex <= AIN_4_SUBCHANNEL_INDEX) {
            auto &channel = ainChannels[subchannelIndex - AIN_1_SUBCHANNEL_INDEX];
            channel.ongoingCal = 1;
            channel.setMode((MeasureMode)calibrationChannelMode);
            channel.setVoltageRange(calibrationChannelVoltageRange);
            channel.setCurrentRange(calibrationChannelCurrentRange);
        }
    }
    
    bool calibrationReadAdcValue(int subchannelIndex, float &adcValue, int *err) override {
        if (subchannelIndex >= AOUT_1_SUBCHANNEL_INDEX && subchannelIndex <= AOUT_4_SUBCHANNEL_INDEX) {
            adcValue = 0;
            return true;
        }

        if (err) {
            *err = SCPI_ERROR_BAD_SEQUENCE_OF_CALIBRATION_COMMANDS;
        }
        return false;
    }

    bool calibrationMeasure(int subchannelIndex, float &measuredValue, int *err) override {
        if (subchannelIndex >= AIN_1_SUBCHANNEL_INDEX && subchannelIndex <= AIN_4_SUBCHANNEL_INDEX) {
			if (isError(subchannelIndex - AIN_1_SUBCHANNEL_INDEX)) {
				if (err) {
					*err = SCPI_ERROR_HARDWARE_MISSING;
				}
				return false;
			}
			
			auto &channel = ainChannels[subchannelIndex - AIN_1_SUBCHANNEL_INDEX];
            measuredValue = channel.getValue();
            return true;
        }

        if (err) {
            *err = SCPI_ERROR_BAD_SEQUENCE_OF_CALIBRATION_COMMANDS;
        }
        return false;
    }

    void stopChannelCalibration(int subchannelIndex) override {
        if (subchannelIndex >= AOUT_1_SUBCHANNEL_INDEX && subchannelIndex <= AOUT_2_SUBCHANNEL_INDEX) {
            aoutDac7760Channels[subchannelIndex - AOUT_1_SUBCHANNEL_INDEX].ongoingCal = 0;
        } else if (subchannelIndex >= AOUT_3_SUBCHANNEL_INDEX && subchannelIndex <= AOUT_4_SUBCHANNEL_INDEX) {
            aoutDac7563Channels[subchannelIndex - AOUT_3_SUBCHANNEL_INDEX].ongoingCal = 0;
        } else if (subchannelIndex >= AIN_1_SUBCHANNEL_INDEX && subchannelIndex <= AIN_4_SUBCHANNEL_INDEX) {
            ainChannels[subchannelIndex - AIN_1_SUBCHANNEL_INDEX].ongoingCal = 0;
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
        } else if (subchannelIndex >= AIN_1_SUBCHANNEL_INDEX && subchannelIndex <= AIN_4_SUBCHANNEL_INDEX) {
            if (ainChannels[subchannelIndex - AIN_1_SUBCHANNEL_INDEX].getMode() == MEASURE_MODE_CURRENT) {
                return CALIBRATION_VALUE_I_HI_RANGE;
            }
        }
        return CALIBRATION_VALUE_U;
    }

    const char *getCalibrationValueRangeDescription(int subchannelIndex) override {
        if (subchannelIndex >= AOUT_1_SUBCHANNEL_INDEX && subchannelIndex <= AOUT_2_SUBCHANNEL_INDEX) {
            if (calibrationChannelMode == SOURCE_MODE_VOLTAGE) {
                return g_aoutVoltageRangeEnumDefinition[calibrationChannelVoltageRange].menuLabel;
            } else {
                return g_aoutCurrentRangeEnumDefinition[calibrationChannelCurrentRange - 5].menuLabel;
            }
        } else if (subchannelIndex >= AOUT_3_SUBCHANNEL_INDEX && subchannelIndex <= AOUT_4_SUBCHANNEL_INDEX) {
            return "\xbd""10 V";
        } else {
            if (afeVersion == 1) {
                if (subchannelIndex >= AIN_1_SUBCHANNEL_INDEX && subchannelIndex <= AIN_2_SUBCHANNEL_INDEX) {
                    if (calibrationChannelMode == SOURCE_MODE_VOLTAGE) {
                        return ainChannels[subchannelIndex - AIN_1_SUBCHANNEL_INDEX].getModeRangeEnumDefinition()[calibrationChannelVoltageRange].menuLabel;
                    } else {
                        return ainChannels[subchannelIndex - AIN_1_SUBCHANNEL_INDEX].getModeRangeEnumDefinition()[3 + calibrationChannelCurrentRange].menuLabel;
                    }
                } else if (subchannelIndex >= AIN_3_SUBCHANNEL_INDEX && subchannelIndex <= AIN_4_SUBCHANNEL_INDEX) {
                    if (calibrationChannelMode == SOURCE_MODE_VOLTAGE) {
                        return ainChannels[subchannelIndex - AIN_1_SUBCHANNEL_INDEX].getModeRangeEnumDefinition()[calibrationChannelVoltageRange].menuLabel;
                    } else {
                        return ainChannels[subchannelIndex - AIN_1_SUBCHANNEL_INDEX].getModeRangeEnumDefinition()[2 + calibrationChannelCurrentRange].menuLabel;
                    }
                }
            } else if (afeVersion == 2) {
                if (subchannelIndex == AIN_1_SUBCHANNEL_INDEX || subchannelIndex == AIN_2_SUBCHANNEL_INDEX) {
                    return ainChannels[subchannelIndex - AIN_1_SUBCHANNEL_INDEX].getModeRangeEnumDefinition()[calibrationChannelVoltageRange].menuLabel;
                } else {
                    return ainChannels[subchannelIndex - AIN_1_SUBCHANNEL_INDEX].getModeRangeEnumDefinition()[calibrationChannelCurrentRange].menuLabel;
                }
            } else if (afeVersion == 3) {
                if (subchannelIndex == AIN_1_SUBCHANNEL_INDEX) {
                    return ainChannels[subchannelIndex - AIN_1_SUBCHANNEL_INDEX].getModeRangeEnumDefinition()[calibrationChannelVoltageRange].menuLabel;
                } else if (subchannelIndex == AIN_2_SUBCHANNEL_INDEX) {
                    return ainChannels[subchannelIndex - AIN_1_SUBCHANNEL_INDEX].getModeRangeEnumDefinition()[calibrationChannelCurrentRange].menuLabel;
                } else if (subchannelIndex == AIN_3_SUBCHANNEL_INDEX) {
                    return ainChannels[subchannelIndex - AIN_1_SUBCHANNEL_INDEX].getModeRangeEnumDefinition()[calibrationChannelVoltageRange].menuLabel;
                } else {
                    return ainChannels[subchannelIndex - AIN_1_SUBCHANNEL_INDEX].getModeRangeEnumDefinition()[calibrationChannelCurrentRange].menuLabel;
                }
            }
        }
        return nullptr;
    }

    bool isCalibrationValueSource(int subchannelIndex) override {
        if (subchannelIndex >= AIN_1_SUBCHANNEL_INDEX && subchannelIndex <= AIN_4_SUBCHANNEL_INDEX) {
            return false;
        }
        return true;
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

    CalConf *getCalConf(int subchannelIndex) {
        if (subchannelIndex >= AOUT_1_SUBCHANNEL_INDEX && subchannelIndex <= AOUT_2_SUBCHANNEL_INDEX) {
            return &aoutDac7760Channels[subchannelIndex - AOUT_1_SUBCHANNEL_INDEX].getCalConf();
        } 
        
        if (subchannelIndex >= AOUT_3_SUBCHANNEL_INDEX && subchannelIndex <= AOUT_4_SUBCHANNEL_INDEX) {
			return &aoutDac7563Channels[subchannelIndex - AOUT_3_SUBCHANNEL_INDEX].calConf;
        }
        
        if (subchannelIndex >= AIN_1_SUBCHANNEL_INDEX && subchannelIndex <= AIN_4_SUBCHANNEL_INDEX) {
			return &ainChannels[subchannelIndex - AIN_1_SUBCHANNEL_INDEX].getCalConf();
		}

        return nullptr;
    }
    
    bool getCalibrationConfiguration(int subchannelIndex, CalibrationConfiguration &calConf, int *err) override {
		if (subchannelIndex >= AIN_1_SUBCHANNEL_INDEX && subchannelIndex <= AIN_4_SUBCHANNEL_INDEX) {
			if (isError(subchannelIndex - AIN_1_SUBCHANNEL_INDEX)) {
				if (err) {
					*err = SCPI_ERROR_HARDWARE_MISSING;
				}
				return false;
			}
		}
		
		CalConf *mioCalConf = getCalConf(subchannelIndex);

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
            stringCopy(calConf.calibrationRemark, MIO_CALIBRATION_REMARK_MAX_LENGTH + 1, mioCalConf->calibrationRemark);

            return true;
        }

        if (err) {
            *err = SCPI_ERROR_HARDWARE_MISSING;
        }
        return false;
    }

    bool setCalibrationConfiguration(int subchannelIndex, const CalibrationConfiguration &calConf, int *err) override {
		if (subchannelIndex >= AIN_1_SUBCHANNEL_INDEX && subchannelIndex <= AIN_4_SUBCHANNEL_INDEX) {
			if (isError(subchannelIndex - AIN_1_SUBCHANNEL_INDEX)) {
				if (err) {
					*err = SCPI_ERROR_HARDWARE_MISSING;
				}
				return false;
			}
		}
		
		CalConf *mioCalConf = getCalConf(subchannelIndex);

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

            mioCalConf->state.calState = isVoltage ? calConf.u.numPoints == 2 : calConf.i[0].numPoints == 2;
            mioCalConf->state.calEnabled = mioCalConf->state.calState;

            mioCalConf->calibrationDate = calConf.calibrationDate;
            stringCopy(mioCalConf->calibrationRemark, MIO_CALIBRATION_REMARK_MAX_LENGTH + 1, calConf.calibrationRemark);

            return true;
        }

        if (err) {
            *err = SCPI_ERROR_HARDWARE_MISSING;
        }
        return false;
    }

    bool getCalibrationRemark(int subchannelIndex, const char *&calibrationRemark, int *err) override {
		if (subchannelIndex >= AIN_1_SUBCHANNEL_INDEX && subchannelIndex <= AIN_4_SUBCHANNEL_INDEX) {
			if (isError(subchannelIndex - AIN_1_SUBCHANNEL_INDEX)) {
				if (err) {
					*err = SCPI_ERROR_HARDWARE_MISSING;
				}
				return false;
			}
		}
		
		CalConf *mioCalConf = getCalConf(subchannelIndex);

        if (mioCalConf) {
            calibrationRemark = mioCalConf->calibrationRemark;
        } else {
            calibrationRemark = "";
        }

        return true;
    }

    bool getCalibrationDate(int subchannelIndex, uint32_t &calibrationDate, int *err) override {
        CalConf *mioCalConf = getCalConf(subchannelIndex);

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
        if (subchannelIndex < AIN_1_SUBCHANNEL_INDEX || subchannelIndex > AIN_4_SUBCHANNEL_INDEX || isError(subchannelIndex - AIN_1_SUBCHANNEL_INDEX)) {
            if (err) {
                *err = SCPI_ERROR_HARDWARE_MISSING;
            }
            return false;
        }

        auto &channel = ainChannels[subchannelIndex - AIN_1_SUBCHANNEL_INDEX];

        if (channel.getMode() != MEASURE_MODE_CURRENT) {
            if (err) {
                *err = SCPI_ERROR_EXECUTION_ERROR;
            }
            return false;
        }

        value = channel.getValue();
        return true;
    }

    void getCurrentStepValues(int subchannelIndex, StepValues *stepValues, bool calibrationMode, bool highRange) override {
        if (subchannelIndex >= AOUT_1_SUBCHANNEL_INDEX && subchannelIndex <= AOUT_2_SUBCHANNEL_INDEX) {
            aoutDac7760Channels[subchannelIndex - AOUT_1_SUBCHANNEL_INDEX].getStepValues(stepValues);
            if (calibrationMode) {
                stepValues->encoderSettings.step /= 10.0f;
                stepValues->encoderSettings.range = stepValues->encoderSettings.step * 10.0f;
            }
            stepValues->encoderSettings.mode = aoutDac7760Channels[subchannelIndex - AOUT_1_SUBCHANNEL_INDEX].getEncoderMode();
        } else if (subchannelIndex >= AIN_1_SUBCHANNEL_INDEX && subchannelIndex <= AIN_4_SUBCHANNEL_INDEX) {
			ainChannels[subchannelIndex - AIN_1_SUBCHANNEL_INDEX].getStepValues(stepValues);
			if (calibrationMode) {
				stepValues->encoderSettings.step /= 10.0f;
				stepValues->encoderSettings.range = stepValues->encoderSettings.step * 10.0f;
			}
            stepValues->encoderSettings.mode = ainChannels[subchannelIndex - AIN_1_SUBCHANNEL_INDEX].getEncoderMode();
		}
    }
    
    void setCurrentEncoderMode(int subchannelIndex, EncoderMode encoderMode) override {
        if (subchannelIndex >= AOUT_1_SUBCHANNEL_INDEX && subchannelIndex <= AOUT_2_SUBCHANNEL_INDEX) {
            aoutDac7760Channels[subchannelIndex - AOUT_1_SUBCHANNEL_INDEX].setEncoderMode(encoderMode);
        } else if (subchannelIndex >= AIN_1_SUBCHANNEL_INDEX && subchannelIndex <= AIN_4_SUBCHANNEL_INDEX) {
            ainChannels[subchannelIndex - AIN_1_SUBCHANNEL_INDEX].setEncoderMode(encoderMode);
        }
    }
    
    float getCurrentResolution(int subchannelIndex) override {
        if (subchannelIndex >= AOUT_1_SUBCHANNEL_INDEX && subchannelIndex <= AOUT_2_SUBCHANNEL_INDEX) {
            return aoutDac7760Channels[subchannelIndex - AOUT_1_SUBCHANNEL_INDEX].getCurrentResolution();
        } else if (subchannelIndex >= AIN_1_SUBCHANNEL_INDEX && subchannelIndex <= AIN_4_SUBCHANNEL_INDEX) {
			return ainChannels[subchannelIndex - AIN_1_SUBCHANNEL_INDEX].getResolution();
		}
		return NAN;
    }

    float getCurrentMinValue(int subchannelIndex) override {
        if (subchannelIndex >= AOUT_1_SUBCHANNEL_INDEX && subchannelIndex <= AOUT_2_SUBCHANNEL_INDEX) {
            return aoutDac7760Channels[subchannelIndex - AOUT_1_SUBCHANNEL_INDEX].getCurrentMinValue();
        } else if (subchannelIndex >= AOUT_3_SUBCHANNEL_INDEX && subchannelIndex <= AOUT_4_SUBCHANNEL_INDEX) {
            return 0.0f;
        } else if (subchannelIndex >= AIN_1_SUBCHANNEL_INDEX && subchannelIndex <= AIN_4_SUBCHANNEL_INDEX) {
            return ainChannels[subchannelIndex - AIN_1_SUBCHANNEL_INDEX].getMinValue();
        }
		return NAN;
    }

    float getCurrentMaxValue(int subchannelIndex) override {
        if (subchannelIndex >= AOUT_1_SUBCHANNEL_INDEX && subchannelIndex <= AOUT_2_SUBCHANNEL_INDEX) {
            return aoutDac7760Channels[subchannelIndex - AOUT_1_SUBCHANNEL_INDEX].getCurrentMaxValue();
        } else if (subchannelIndex >= AOUT_3_SUBCHANNEL_INDEX && subchannelIndex <= AOUT_4_SUBCHANNEL_INDEX) {
            return 0.0f;
		} else if (subchannelIndex >= AIN_1_SUBCHANNEL_INDEX && subchannelIndex <= AIN_4_SUBCHANNEL_INDEX) {
			return ainChannels[subchannelIndex - AIN_1_SUBCHANNEL_INDEX].getMaxValue();
		}
		return NAN;
    }

    bool isCurrentCalibrationExists(int subchannelIndex) override {
        if (getCalibrationValueType(subchannelIndex) == CALIBRATION_VALUE_I_HI_RANGE) {
            if (subchannelIndex >= AIN_1_SUBCHANNEL_INDEX && subchannelIndex <= AIN_4_SUBCHANNEL_INDEX) {
                return ainChannels[subchannelIndex - AIN_1_SUBCHANNEL_INDEX].getCalConf().state.calState;
            }            
        }
        return false;
    }

    bool isCurrentCalibrationEnabled(int subchannelIndex) override {
        if (getCalibrationValueType(subchannelIndex) == CALIBRATION_VALUE_I_HI_RANGE) {
            if (subchannelIndex >= AIN_1_SUBCHANNEL_INDEX && subchannelIndex <= AIN_4_SUBCHANNEL_INDEX) {
                return ainChannels[subchannelIndex - AIN_1_SUBCHANNEL_INDEX].getCalConf().state.calEnabled;
            }
        }
        return false;
    }
    
    void enableCurrentCalibration(int subchannelIndex, bool enabled) override {
        if (getCalibrationValueType(subchannelIndex) == CALIBRATION_VALUE_I_HI_RANGE) {
            if (subchannelIndex >= AIN_1_SUBCHANNEL_INDEX && subchannelIndex <= AIN_4_SUBCHANNEL_INDEX) {
                ainChannels[subchannelIndex - AIN_1_SUBCHANNEL_INDEX].getCalConf().state.calEnabled = enabled;
            } else {
                return;
            }
            saveChannelCalibration(subchannelIndex, nullptr);          
        }
    }

    int getNumDlogResources(int subchannelIndex) override {
		if (subchannelIndex == DIN_SUBCHANNEL_INDEX) {
            return 8;
        }
        
        if (afeVersion != 4) {
            if (subchannelIndex >= AIN_1_SUBCHANNEL_INDEX && subchannelIndex <= AIN_4_SUBCHANNEL_INDEX && !isError(subchannelIndex - AIN_1_SUBCHANNEL_INDEX)) {
                return 1;
            }
        }
        
        return 0;
	}
    
	DlogResourceType getDlogResourceType(int subchannelIndex, int resourceIndex) override {
        if (subchannelIndex == DIN_SUBCHANNEL_INDEX) {
            return DlogResourceType(DLOG_RESOURCE_TYPE_DIN0 + resourceIndex);
        }

        if (afeVersion != 4) {
            if (subchannelIndex >= AIN_1_SUBCHANNEL_INDEX && subchannelIndex <= AIN_4_SUBCHANNEL_INDEX) {
                return 
                    ainChannels[subchannelIndex - AIN_1_SUBCHANNEL_INDEX].getMode() == MEASURE_MODE_VOLTAGE ?
                        DLOG_RESOURCE_TYPE_U : DLOG_RESOURCE_TYPE_I;
            }
        }

        return DLOG_RESOURCE_TYPE_NONE;
	}
    
    dlog_file::DataType getDlogResourceDataType(int subchannelIndex, int resourceIndex) override {
        if (subchannelIndex == DIN_SUBCHANNEL_INDEX || subchannelIndex == DOUT_SUBCHANNEL_INDEX) {
            return dlog_file::DATA_TYPE_BIT;
        }

        if (afeVersion != 4) {
            if (subchannelIndex >= AIN_1_SUBCHANNEL_INDEX && subchannelIndex <= AIN_4_SUBCHANNEL_INDEX) {
                if (dlog_record::g_recordingParameters.period < 1.0f / 16000) {
                    return dlog_file::DATA_TYPE_INT16_BE;
                }
                if (dlog_record::g_recordingParameters.period < 1.0f / 1000) {
                    return dlog_file::DATA_TYPE_INT24_BE;
                }
            }
        }

        return dlog_file::DATA_TYPE_FLOAT;
    }

    double getDlogResourceTransformOffset(int subchannelIndex, int resourceIndex) override {
        if (afeVersion != 4) {
            if (
                subchannelIndex >= AIN_1_SUBCHANNEL_INDEX &&
                subchannelIndex <= AIN_4_SUBCHANNEL_INDEX &&
                dlog_record::g_recordingParameters.period < 1.0f / 1000
            ) {
                auto ainChannelIndex = subchannelIndex - AIN_1_SUBCHANNEL_INDEX;
                auto &ainChannel = ainChannels[ainChannelIndex];
                auto &calConf = ainChannel.getCalConf();
                if (calConf.state.calEnabled) {
                    return calConf.points[0].uncalValue - calConf.points[0].calValue * 
                        (calConf.points[1].uncalValue - calConf.points[0].uncalValue) / (calConf.points[1].calValue - calConf.points[0].calValue);
                }
            }
        }

        return 0.0f;
    }

    double getDlogResourceTransformScale(int subchannelIndex, int resourceIndex) override {
        if (afeVersion != 4) {
            if (
                subchannelIndex >= AIN_1_SUBCHANNEL_INDEX &&
                subchannelIndex <= AIN_4_SUBCHANNEL_INDEX &&
                dlog_record::g_recordingParameters.period < 1.0f / 1000
            ) {
                auto ainChannelIndex = subchannelIndex - AIN_1_SUBCHANNEL_INDEX;
                auto &ainChannel = ainChannels[ainChannelIndex];
                auto mode = ainChannel.getMode();
                
                double f = getAinConversionFactor(
                    afeVersion,
                    ainChannelIndex,
                    mode,
                    mode == MEASURE_MODE_VOLTAGE ?
                        ainChannel.getVoltageRange() : ainChannel.getCurrentRange()
                );
                
                if (dlog_record::g_recordingParameters.period < 1.0f / 16000) {
                    f /= 1 << 15; // 16 bit
                } else {
                    f /= (1 << 23); // 24 bit
                }

                auto &calConf = ainChannel.getCalConf();
                if (calConf.state.calEnabled) {
					f *= (calConf.points[1].uncalValue - calConf.points[0].uncalValue) / (calConf.points[1].calValue - calConf.points[0].calValue);
                }

                return f;
            }
        }

        return 1.0f;
    }

	const char *getDlogResourceLabel(int subchannelIndex, int resourceIndex) override {
        if (subchannelIndex == DIN_SUBCHANNEL_INDEX) {
            return getPinLabelOrDefault(subchannelIndex, resourceIndex);
        }
        
        if (afeVersion != 4) {
            if (subchannelIndex >= AIN_1_SUBCHANNEL_INDEX && subchannelIndex <= AIN_4_SUBCHANNEL_INDEX) {
                const char *label = getChannelLabel(subchannelIndex);
                if (*label) {
                    return label;
                }
                return getDefaultChannelLabel(subchannelIndex);
            }
        }

        return "";
    }

    float getDlogResourceMinPeriod(int subchannelIndex, int resourceIndex) override {
        if (subchannelIndex == DIN_SUBCHANNEL_INDEX) {
            return 0.00001f; // 10 us
        }

        if (afeVersion != 4) {
            if (subchannelIndex >= AIN_1_SUBCHANNEL_INDEX && subchannelIndex <= AIN_4_SUBCHANNEL_INDEX) {
                return 1.0f / 32000; // 32 KSPS
            }
        }

        return Module::getDlogResourceMinPeriod(subchannelIndex, resourceIndex);
    }

    bool isDlogPeriodAllowed(int subchannelIndex, int resourceIndex, float period) override {
        if (afeVersion != 4) {
            if (subchannelIndex >= AIN_1_SUBCHANNEL_INDEX && subchannelIndex <= AIN_4_SUBCHANNEL_INDEX) {
                return period == 1.0f / 32000 || period == 1.0f / 16000 ||
                    period == 1.0f / 8000 || period == 1.0f / 4000 || period == 1.0f / 2000 || period >= 1.0f / 1000;
            }
        }

        return Module::isDlogPeriodAllowed(subchannelIndex, resourceIndex, period);
    }

    void onStartDlog() override {
        if (dlog_record::isModuleControlledRecording()) {
           uint32_t resources = 0;

           for (int i = 0; i < dlog_record::g_activeRecording.parameters.numDlogItems; i++) {
               auto &dlogItem = dlog_record::g_activeRecording.parameters.dlogItems[i];
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
                nextDlogRecordingStart.duration = dlog_record::g_recordingParameters.duration;
                nextDlogRecordingStart.period = dlog_record::g_recordingParameters.period;
                nextDlogRecordingStart.resources = resources;

                stringCopy(nextDlogRecordingStart.filePath, MAX_PATH_LENGTH, dlog_record::g_recordingParameters.filePath + 2 /* skip drive part */);

                for (int i = 0; i < 8; i++) {
                    stringCopy(nextDlogRecordingStart.dinLabels + i * (CHANNEL_LABEL_MAX_LENGTH + 1), CHANNEL_LABEL_MAX_LENGTH + 1, getDlogResourceLabel(DIN_SUBCHANNEL_INDEX, i));
                }

                for (int i = 0; i < 8; i++) {
                    stringCopy(nextDlogRecordingStart.doutLabels + i * (CHANNEL_LABEL_MAX_LENGTH + 1), CHANNEL_LABEL_MAX_LENGTH + 1, getDlogResourceLabel(DOUT_SUBCHANNEL_INDEX, i));
                }

                for (int i = 0; i < 4; i++) {
                    stringCopy(nextDlogRecordingStart.ainLabels + i * (CHANNEL_LABEL_MAX_LENGTH + 1), CHANNEL_LABEL_MAX_LENGTH + 1, getDlogResourceLabel(AIN_1_SUBCHANNEL_INDEX + i, 0));
                }

                nextDlogCommand = &dlogRecordingStart_command;
                
                dlogDataRecordIndex = 0;
            }
        }
    }

    void onStopDlog() override {
        nextDlogCommand = &dlogRecordingStop_command;
    }

    int getNumFunctionGeneratorResources(int subchannelIndex) override {
		if (subchannelIndex == DOUT_SUBCHANNEL_INDEX) {
			return 8;
		} else if (subchannelIndex >= AOUT_1_SUBCHANNEL_INDEX && subchannelIndex <= AOUT_4_SUBCHANNEL_INDEX && !isError(subchannelIndex - AOUT_1_SUBCHANNEL_INDEX)) {
            return 1;
        }
        return 0;
    }

    FunctionGeneratorResourceType getFunctionGeneratorResourceType(int subchannelIndex, int resourceIndex) override {
        if (subchannelIndex == DOUT_SUBCHANNEL_INDEX) {
            return FUNCTION_GENERATOR_RESOURCE_TYPE_DIGITAL;
        } else if (subchannelIndex >= AOUT_1_SUBCHANNEL_INDEX && subchannelIndex <= AOUT_2_SUBCHANNEL_INDEX) {
            return FUNCTION_GENERATOR_RESOURCE_TYPE_U_AND_I;
        }
        return FUNCTION_GENERATOR_RESOURCE_TYPE_U;
    }

    bool getFunctionGeneratorResourceTriggerMode(int subchannelIndex, int resourceIndex, TriggerMode &triggerMode, int *err) override {
        if (subchannelIndex == DOUT_SUBCHANNEL_INDEX) {
            triggerMode = doutChannel.m_triggerMode[resourceIndex];
            return true;
        }
        
        if (subchannelIndex >= AOUT_1_SUBCHANNEL_INDEX && subchannelIndex <= AOUT_2_SUBCHANNEL_INDEX) {
            auto &channel = aoutDac7760Channels[subchannelIndex - AOUT_1_SUBCHANNEL_INDEX];
            triggerMode = channel.m_triggerMode;
            return true;
        }
        
        if (subchannelIndex >= AOUT_3_SUBCHANNEL_INDEX && subchannelIndex <= AOUT_4_SUBCHANNEL_INDEX) {
            auto &channel = aoutDac7563Channels[subchannelIndex - AOUT_3_SUBCHANNEL_INDEX];
            triggerMode = channel.m_triggerMode;
            return true;
        }
        
        if (err) {
            *err = SCPI_ERROR_HARDWARE_MISSING;
        }

        return false;
    }

    bool setFunctionGeneratorResourceTriggerMode(int subchannelIndex, int resourceIndex, TriggerMode triggerMode, int *err) override {
        if (subchannelIndex == DOUT_SUBCHANNEL_INDEX) {
            if (!function_generator::onTriggerModeChanged(slotIndex, subchannelIndex, resourceIndex, triggerMode, err)) {
                return false;
            }
            doutChannel.m_triggerMode[resourceIndex] = triggerMode;
            return true;
        }

        if (subchannelIndex >= AOUT_1_SUBCHANNEL_INDEX && subchannelIndex <= AOUT_2_SUBCHANNEL_INDEX) {
            if (!function_generator::onTriggerModeChanged(slotIndex, subchannelIndex, resourceIndex, triggerMode, err)) {
                return false;
            }
            auto &channel = aoutDac7760Channels[subchannelIndex - AOUT_1_SUBCHANNEL_INDEX];
            channel.m_triggerMode = triggerMode;
            return true;
        }

        if (subchannelIndex >= AOUT_3_SUBCHANNEL_INDEX && subchannelIndex <= AOUT_4_SUBCHANNEL_INDEX) {
            if (!function_generator::onTriggerModeChanged(slotIndex, subchannelIndex, resourceIndex, triggerMode, err)) {
                return false;
            }
            auto &channel = aoutDac7563Channels[subchannelIndex - AOUT_3_SUBCHANNEL_INDEX];
            channel.m_triggerMode = triggerMode;
            return true;
        }

        if (err) {
            *err = SCPI_ERROR_HARDWARE_MISSING;
        }

        return false;    
    }

    const char *getFunctionGeneratorResourceLabel(int subchannelIndex, int resourceIndex) override {
        if (subchannelIndex == DOUT_SUBCHANNEL_INDEX) {
			const char *label;
			getChannelPinLabel(subchannelIndex, resourceIndex + 1, label);
			if (*label) {
				return label;
			}
			return getDefaultChannelPinLabel(subchannelIndex, resourceIndex + 1);
        }
        
        const char *label = getChannelLabel(subchannelIndex);
		if (*label) {
			return label;
		}
		return getDefaultChannelLabel(subchannelIndex);
	}

	void getFunctionGeneratorAmplitudeInfo(int subchannelIndex, int resourceIndex, FunctionGeneratorResourceType resourceType, float &min, float &max, StepValues *stepValues) override {
		if (subchannelIndex == DOUT_SUBCHANNEL_INDEX) {
			min = 0;
			max = 1.0f;

			if (stepValues) {
				static float values[] = { 0.001f, 0.05f, 0.1f, 0.2f };
				stepValues->values = values;
				stepValues->count = sizeof(values) / sizeof(float);
				stepValues->unit = UNIT_HERTZ;
			}
		} else if (subchannelIndex >= AOUT_1_SUBCHANNEL_INDEX && subchannelIndex <= AOUT_2_SUBCHANNEL_INDEX) {
            auto &channel = aoutDac7760Channels[subchannelIndex - AOUT_1_SUBCHANNEL_INDEX];
            if (resourceType == FUNCTION_GENERATOR_RESOURCE_TYPE_U) {
                min = -10.0f;
                max = 10.0f;
                if (stepValues) {
                    channel.getVoltageStepValues(stepValues);
                }
            } else {
                min = 0.0f;
                max = 0.024f;
                if (stepValues) {
                    channel.getCurrentStepValues(stepValues);
                }
            }
        } else if (subchannelIndex >= AOUT_3_SUBCHANNEL_INDEX && subchannelIndex <= AOUT_4_SUBCHANNEL_INDEX) {
            auto &channel = aoutDac7563Channels[subchannelIndex - AOUT_3_SUBCHANNEL_INDEX];
            min = AOUT_DAC7563_MIN;
            max = AOUT_DAC7563_MAX;
            if (stepValues) {
                channel.getStepValues(stepValues);
            }
        } else {
            return Module::getFunctionGeneratorAmplitudeInfo(subchannelIndex, resourceIndex, resourceType, min, max, stepValues);
        }
    }
	
    void getFunctionGeneratorFrequencyInfo(int subchannelIndex, int resourceIndex, float &min, float &max, StepValues *stepValues) override {
        if (subchannelIndex == DOUT_SUBCHANNEL_INDEX) {
            min = 0.1f;
            max = 8000.0f;

            if (stepValues) {
                static float values[] = { 1.0f, 10.0f, 25.0f, 100.0f };
                stepValues->values = values;
                stepValues->count = sizeof(values) / sizeof(float);
                stepValues->unit = UNIT_HERTZ;
            }
        } else if (subchannelIndex >= AOUT_1_SUBCHANNEL_INDEX && subchannelIndex <= AOUT_2_SUBCHANNEL_INDEX) {
            min = 0.1f;
            max = 120.0f;

            if (stepValues) {
                static float values[] = { 1.0f, 2.0f, 5.0f, 10.0f };
                stepValues->values = values;
                stepValues->count = sizeof(values) / sizeof(float);
                stepValues->unit = UNIT_HERTZ;
            }
        } else if (subchannelIndex >= AOUT_3_SUBCHANNEL_INDEX && subchannelIndex <= AOUT_4_SUBCHANNEL_INDEX) {
            min = 0.1f;
            max = 200.0f;

            if (stepValues) {
                static float values[] = { 1.0f, 2.0f, 5.0f, 10.0f };
                stepValues->values = values;
                stepValues->count = sizeof(values) / sizeof(float);
                stepValues->unit = UNIT_HERTZ;
            }
        } else {
            return Module::getFunctionGeneratorFrequencyInfo(subchannelIndex, resourceIndex, min, max, stepValues);
        }
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

	const char *getPinoutFile() override {
		if (afeVersion == 1) {
			return "mio168_afe1_pinout.jpg";
		} else if (afeVersion == 2) {
			return "mio168_afe2_pinout.jpg";
		} else if (afeVersion == 3) {
			return "mio168_afe3_pinout.jpg";
		} else if (afeVersion == 4) {
			return "mio168_no_afe_pinout.jpg";
		}
		return nullptr;
	}

	bool isMicroAmperAllowed(int subchannelIndex) override {
		if (subchannelIndex == AOUT_1_SUBCHANNEL_INDEX || subchannelIndex == AOUT_2_SUBCHANNEL_INDEX) {
			return true;
		}
		return Module::isMicroAmperAllowed(subchannelIndex);
	}

	bool isAmperAllowed(int subchannelIndex) override {
		if (subchannelIndex == AOUT_1_SUBCHANNEL_INDEX || subchannelIndex == AOUT_2_SUBCHANNEL_INDEX) {
			return false;
		}
		return Module::isMicroAmperAllowed(subchannelIndex);
	}

    float getMaxTemperature() {
        if (afeVersion != 4) {
            float maxCurrent = FLT_MIN;
            for (int i = 0; i < 4; i++) {
                if (ainChannels[i].getMode() == MEASURE_MODE_CURRENT) {
                    maxCurrent = MAX(maxCurrent, ainChannels[i].getValue());
                }
            }

            return clamp(remap(maxCurrent, 5.0f, FAN_MIN_TEMP, 8.0f, FAN_MAX_TEMP), 25.0f, FAN_MAX_TEMP);
        }

        return Module::getMaxTemperature();
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
	nullptr
};

const Mio168Module::CommandDef Mio168Module::dlogRecordingStop_command = {
	COMMAND_DLOG_RECORDING_STOP,
	nullptr,
	&Mio168Module::Command_DlogRecordingStop_Done
};

const Mio168Module::CommandDef Mio168Module::dlogRecordingData_command = {
	COMMAND_DLOG_RECORDING_DATA,
	nullptr,
    nullptr
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

class DoutConfigurationPage : public SetPage {
public:
    void pageAlloc() {
        auto module = (Mio168Module *)g_slots[g_slotIndex];

        for (int i = 0; i < 8; i++) {
            m_triggerMode[i] = m_triggerModeOrig[i] = module->doutChannel.m_triggerMode[i];
        }
    }

	void pageWillAppear() {
		hmi::g_selectedSlotIndex = g_slotIndex;

		if (!getDirty()) {
			pageAlloc();
		}
    }

    int getDirty() {
		return memcmp(m_triggerMode, m_triggerModeOrig, sizeof(m_triggerModeOrig)) != 0;
    }

    void apply() {
        for (int i = 0; i < 8; i++) {
            if (m_triggerMode[i] != m_triggerModeOrig[i]) {
				g_slots[g_slotIndex]->setFunctionGeneratorResourceTriggerMode(DOUT_SUBCHANNEL_INDEX, i, m_triggerMode[i], nullptr);
            }
        }
    }

    void set() {
		apply();
		popPage();
    }

    static int g_slotIndex;
    static int g_pin;

    TriggerMode m_triggerMode[8];

private:
    TriggerMode m_triggerModeOrig[8];
};

int DoutConfigurationPage::g_slotIndex;
int DoutConfigurationPage::g_pin;

static DoutConfigurationPage g_doutConfigurationPage;

////////////////////////////////////////////////////////////////////////////////

class AinConfigurationPage : public SetPage {
public:
    static int g_selectedChannelIndex;

    void pageAlloc() {
        Mio168Module *module = (Mio168Module *)g_slots[hmi::g_selectedSlotIndex];
        AinChannel &channel = module->ainChannels[g_selectedChannelIndex - AIN_1_SUBCHANNEL_INDEX];

        m_mode = m_modeOrig = channel.getMode();
        m_currentRange = m_currentRangeOrig = channel.getCurrentRange();
		m_voltageRange = m_voltageRangeOrig = channel.getVoltageRange();
        m_currentNPLC = m_currentNPLCOrig = channel.m_currentNPLC;
        m_voltageNPLC = m_voltageNPLCOrig = channel.m_voltageNPLC;
    }

    int getDirty() { 
        if (m_mode != m_modeOrig) {
            return true;
        }

        if (m_mode == MEASURE_MODE_VOLTAGE) {
            return m_voltageRange != m_voltageRangeOrig || m_voltageNPLC != m_voltageNPLCOrig;
        }

        return m_currentRange != m_currentRangeOrig || m_currentNPLC != m_currentNPLCOrig;
    }

    void set() {
		if (getDirty()) {
			sendMessageToPsu((HighPriorityThreadMessage)PSU_MESSAGE_AIN_CONFIGURE, hmi::g_selectedSlotIndex);
		}
		popPage();
    }

    bool hasMultipleModes() {
        Mio168Module *module = (Mio168Module *)g_slots[hmi::g_selectedSlotIndex];
        AinChannel &channel = module->ainChannels[g_selectedChannelIndex - AIN_1_SUBCHANNEL_INDEX];
        return channel.getCurrentRangeMaxValue() != -1 && channel.getVoltageRangeMaxValue() != -1;
    }

    EnumItem *getRangeEnumDefinition() {
		static EnumItem g_afe1Ain12VoltageRangeEnumDefinition[] = {
			{ 0, "\xbd""2.4 V" },
			{ 1, "\xbd""48 V" },
			{ 2, "\xbd""240 V" },
			{ 3, "Auto range" },
			{ 0, 0 }
		};

		static EnumItem g_afe1Ain12CurrentRangeEnumDefinition[] = {
			{ 0, "\xbd""48 mA" },
			{ 0, 0 }
		};

		static EnumItem g_afe1Ain34VoltageRangeEnumDefinition[] = {
			{ 0, "\xbd""2.4 V" },
			{ 1, "\xbd""12 V" },
			{ 2, "Auto range" },
			{ 0, 0 }
		};

		static EnumItem g_afe1Ain34CurrentRangeEnumDefinition[] = {
			{ 0, "\xbd""48 mA" },
			{ 1, "\xbd""1.2 A" },
			{ 2, "\xbd""10 A" },
			{ 3, "Auto range" },
			{ 0, 0 }
		};

		static EnumItem g_afe2Ain12VoltageRangeEnumDefinition[] = {
			{ 0, "\xbd""3 V" },
			{ 1, "\xbd""12 V" },
			{ 0, 0 }
		};

		static EnumItem g_afe2Ain34CurrentRangeEnumDefinition[] = {
			{ 0, "\xbd""2.4 mA / Auto" },
			{ 1, "\xbd""2 A" },
			{ 0, 0 }
		};

		static EnumItem g_afe3Ain1VoltageRangeEnumDefinition[] = {
			{ 0, "\xbd""50 V" },
			{ 1, "\xbd""450 V" },
			{ 2, "Auto range" },
			{ 0, 0 }
		};

		static EnumItem g_afe3Ain2CurrentRangeEnumDefinition[] = {
			{ 0, "\xbd""1 A" },
			{ 1, "\xbd""10 A" },
			{ 2, "Auto range" },
			{ 0, 0 }
		};

		static EnumItem g_afe3Ain3VoltageRangeEnumDefinition[] = {
			{ 0, "\xbd""2.4 V" },
			{ 1, "\xbd""48 V" },
			{ 2, "\xbd""240 V" },
			{ 3, "Auto range" },
			{ 0, 0 }
		};

		static EnumItem g_afe3Ain4CurrentRangeEnumDefinition[] = {
			{ 0, "\xbd""24 mA" },
			{ 1, "\xbd""1.2 A" },
			{ 2, "\xbd""10 A" },
			{ 3, "Auto range" },
			{ 0, 0 }
		};

		Mio168Module *module = (Mio168Module *)g_slots[hmi::g_selectedSlotIndex];
		AinChannel &channel = module->ainChannels[g_selectedChannelIndex - AIN_1_SUBCHANNEL_INDEX];

		if (channel.m_afeVersion == 1) {
			if (g_selectedChannelIndex == AIN_1_SUBCHANNEL_INDEX || g_selectedChannelIndex == AIN_2_SUBCHANNEL_INDEX) {
				return m_mode == MEASURE_MODE_VOLTAGE ? g_afe1Ain12VoltageRangeEnumDefinition : g_afe1Ain12CurrentRangeEnumDefinition;
			} else {
				return m_mode == MEASURE_MODE_VOLTAGE ? g_afe1Ain34VoltageRangeEnumDefinition : g_afe1Ain34CurrentRangeEnumDefinition;
			}
		} else if (channel.m_afeVersion == 2) {
			if (g_selectedChannelIndex == AIN_1_SUBCHANNEL_INDEX || g_selectedChannelIndex == AIN_2_SUBCHANNEL_INDEX) {
				return g_afe2Ain12VoltageRangeEnumDefinition;
			} else {
				return g_afe2Ain34CurrentRangeEnumDefinition;
			}
		} else if (channel.m_afeVersion == 3) {
			if (g_selectedChannelIndex == AIN_1_SUBCHANNEL_INDEX) {
				return g_afe3Ain1VoltageRangeEnumDefinition;
			} else if (g_selectedChannelIndex == AIN_2_SUBCHANNEL_INDEX) {
				return g_afe3Ain2CurrentRangeEnumDefinition;
			} else if (g_selectedChannelIndex == AIN_3_SUBCHANNEL_INDEX) {
				return g_afe3Ain3VoltageRangeEnumDefinition;
			} else {
				return g_afe3Ain4CurrentRangeEnumDefinition;
			}
		}

        return nullptr;
    }

    uint8_t m_mode;
	uint8_t m_currentRange;
    uint8_t m_voltageRange;
    float m_currentNPLC;
    float m_voltageNPLC;

    static uint8_t getModeRange(int slotIndex, int subchannelIndex) {
        Mio168Module *module = (Mio168Module *)g_slots[slotIndex];
        AinChannel &channel = module->ainChannels[subchannelIndex - AIN_1_SUBCHANNEL_INDEX];

		if (channel.m_afeVersion == 1) {
            if (subchannelIndex == AIN_1_SUBCHANNEL_INDEX || subchannelIndex == AIN_2_SUBCHANNEL_INDEX) {
                if (channel.getMode() == MEASURE_MODE_VOLTAGE) {
                    return channel.getVoltageRange();
                } else {
                    return 4;
                }
            } else {
                if (channel.getMode() == MEASURE_MODE_VOLTAGE) {
                    return channel.getVoltageRange();
                } {
                    return 3 + channel.getCurrentRange();
                }
            }
        } else if (channel.m_afeVersion == 2) {
			if (subchannelIndex == AIN_1_SUBCHANNEL_INDEX || subchannelIndex == AIN_2_SUBCHANNEL_INDEX) {
                return channel.getVoltageRange();
			} else {
                return channel.getCurrentRange();
			}
		} else if (channel.m_afeVersion == 3) {
			if (subchannelIndex == AIN_1_SUBCHANNEL_INDEX) {
				return channel.getVoltageRange();
			} else if (subchannelIndex == AIN_2_SUBCHANNEL_INDEX) {
				return channel.getCurrentRange();
			} else if (subchannelIndex == AIN_3_SUBCHANNEL_INDEX) {
                return channel.getVoltageRange();
			} else {
                return channel.getCurrentRange();
			}
		}

        return 0;
    }

    static uint8_t getModeRangeForAutoRange(int slotIndex, int subchannelIndex) {
        Mio168Module *module = (Mio168Module *)g_slots[slotIndex];
        AinChannel &channel = module->ainChannels[subchannelIndex - AIN_1_SUBCHANNEL_INDEX];

		if (channel.m_afeVersion == 1) {
            if (subchannelIndex == AIN_1_SUBCHANNEL_INDEX || subchannelIndex == AIN_2_SUBCHANNEL_INDEX) {
                return channel.m_autoRange;
            } else {
                if (channel.getMode() == MEASURE_MODE_VOLTAGE) {
                    return channel.m_autoRange;
                } {
                    return 2 + channel.m_autoRange;
                }
            }
        } else if (channel.m_afeVersion == 3) {
            return channel.m_autoRange;
		}

        return 0;
    }

    static void setModeRange(int slotIndex, int subchannelIndex, uint8_t modeRange) {
        Mio168Module *module = (Mio168Module *)g_slots[slotIndex];
        AinChannel &channel = module->ainChannels[subchannelIndex - AIN_1_SUBCHANNEL_INDEX];

		if (channel.m_afeVersion == 1) {
			if (subchannelIndex == AIN_1_SUBCHANNEL_INDEX || subchannelIndex == AIN_2_SUBCHANNEL_INDEX) {
				if (modeRange < 4) {
					channel.setMode(MEASURE_MODE_VOLTAGE);
					channel.setVoltageRange(modeRange);
				} else {
					channel.setMode(MEASURE_MODE_CURRENT);
					channel.setCurrentRange(0);
				}
			} else {
				if (modeRange < 3) {
					channel.setMode(MEASURE_MODE_VOLTAGE);
					channel.setVoltageRange(modeRange);
				} else {
					channel.setMode(MEASURE_MODE_CURRENT);
					channel.setCurrentRange(modeRange - 3);
				}
			}
		} else if (channel.m_afeVersion == 2) {
			if (subchannelIndex == AIN_1_SUBCHANNEL_INDEX || subchannelIndex == AIN_2_SUBCHANNEL_INDEX) {
                channel.setVoltageRange(modeRange);
			} else {
                channel.setCurrentRange(modeRange);
            }
		} else if (channel.m_afeVersion == 3) {
			if (subchannelIndex == AIN_1_SUBCHANNEL_INDEX) {
				channel.setVoltageRange(modeRange);
			} else if (subchannelIndex == AIN_2_SUBCHANNEL_INDEX) {
				channel.setCurrentRange(modeRange);
			} else if (subchannelIndex == AIN_3_SUBCHANNEL_INDEX) {
                channel.setVoltageRange(modeRange);
			} else {
                channel.setCurrentRange(modeRange);
			}
		}
    }

    bool isAutoRange() {
        Mio168Module *module = (Mio168Module *)g_slots[hmi::g_selectedSlotIndex];
        AinChannel &channel = module->ainChannels[g_selectedChannelIndex - AIN_1_SUBCHANNEL_INDEX];
        return AinChannel::isAutoRange(channel.m_afeVersion, channel.m_subchannelIndex, m_mode, m_currentRange, m_voltageRange);
    }

private:
    uint8_t m_modeOrig;
    uint8_t m_currentRangeOrig;
	uint8_t m_voltageRangeOrig;
    float m_currentNPLCOrig;
    float m_voltageNPLCOrig;
};

int AinConfigurationPage::g_selectedChannelIndex;
static AinConfigurationPage g_ainConfigurationPage;

////////////////////////////////////////////////////////////////////////////////

class AoutDac7760ConfigurationPage : public SetPage {
public:
    static int g_selectedChannelIndex;

    void pageAlloc() {
        auto module = (Mio168Module *)g_slots[hmi::g_selectedSlotIndex];
        auto &channel = module->aoutDac7760Channels[g_selectedChannelIndex - AOUT_1_SUBCHANNEL_INDEX];

        m_outputEnabled = m_outputEnabledOrig = channel.m_outputEnabled;
        m_mode = m_modeOrig = channel.m_mode;
        m_currentRange = m_currentRangeOrig = channel.getCurrentRange();
        m_voltageRange = m_voltageRangeOrig = channel.getVoltageRange();
        m_triggerMode = m_triggerModeOrig = channel.m_triggerMode;
    }

	void pageWillAppear() {
		if (!getDirty()) {
			pageAlloc();
		}
	}

    int getDirty() { 
        return m_outputEnabled != m_outputEnabledOrig ||
            m_mode != m_modeOrig ||
			(m_mode == SOURCE_MODE_VOLTAGE ? m_voltageRange != m_voltageRangeOrig : m_currentRange != m_currentRangeOrig) ||
			m_triggerMode != m_triggerModeOrig;
    }

	void apply() {
		if (getDirty()) {
			sendMessageToPsu((HighPriorityThreadMessage)PSU_MESSAGE_AOUT_DAC7760_CONFIGURE, hmi::g_selectedSlotIndex);
		}

		m_outputEnabled = m_outputEnabledOrig;
		m_mode = m_modeOrig;
		m_currentRange = m_currentRangeOrig;
		m_voltageRange = m_voltageRangeOrig;
		m_triggerMode = m_triggerModeOrig;
	}

    void set() {
		apply();
        popPage();
    }

    bool m_outputEnabled;
    uint8_t m_mode;
    uint8_t m_currentRange;
    uint8_t m_voltageRange;
    TriggerMode m_triggerMode;

    static EnumItem *getModeRangeEnumDefinition(int slotIndex, int subchannelIndex) {
		static EnumItem g_aoutMdeRangeEnumDefinition[] = {
			{ 0, "0 V - 5 V" },
			{ 1, "0 V - 10 V" },
            { 2, "\xbd""5 V" },
            { 3, "\xbd""10 V" },
            { 4, "4 mA - 20 mA" },
            { 5, "0 mA - 20 mA" },
            { 6, "0 mA - 24 mA" },
			{ 7, "OFF" },
			{ 0, 0 }
		};

        return g_aoutMdeRangeEnumDefinition;
    }

    static EnumItem *getModeRangeShortEnumDefinition(int slotIndex, int subchannelIndex) {
		static EnumItem g_aoutMdeRangeEnumDefinition[] = {
			{ 0, "0-5 V" },
			{ 1, "0-10 V" },
            { 2, "\xbd""5 V" },
            { 3, "\xbd""10 V" },
            { 4, "4-20 mA" },
            { 5, "0-20 mA" },
            { 6, "0-24 mA" },
			{ 7, "OFF" },
			{ 0, 0 }
		};

        return g_aoutMdeRangeEnumDefinition;
    }

    static uint8_t getModeRange(int slotIndex, int subchannelIndex) {
        Mio168Module *module = (Mio168Module *)g_slots[slotIndex];
        AoutDac7760Channel &channel = module->aoutDac7760Channels[subchannelIndex - AOUT_1_SUBCHANNEL_INDEX];

		if (channel.m_outputEnabled) {
			if (channel.m_mode == MEASURE_MODE_VOLTAGE) {
				return channel.getVoltageRange();
			} else {
				return channel.getCurrentRange() - 1;
			}
		}
		else {
			return 7;
		}
    }

   static  void setModeRange(int slotIndex, int subchannelIndex, uint8_t modeRange) {
        Mio168Module *module = (Mio168Module *)g_slots[slotIndex];
        AoutDac7760Channel &channel = module->aoutDac7760Channels[subchannelIndex - AOUT_1_SUBCHANNEL_INDEX];

        if (modeRange < 4) {
			channel.m_outputEnabled = true;
            channel.m_mode = MEASURE_MODE_VOLTAGE;
            channel.setVoltageRange(modeRange);
        } else if (modeRange < 7) {
			channel.m_outputEnabled = true;
            channel.m_mode = MEASURE_MODE_CURRENT;
            channel.setCurrentRange(modeRange + 1);
        } else {
			channel.m_outputEnabled = false;
        }
    }

private:
    bool m_outputEnabledOrig;
    uint8_t m_modeOrig;
    uint8_t m_currentRangeOrig;
    uint8_t m_voltageRangeOrig;
	TriggerMode m_triggerModeOrig;
};

int AoutDac7760ConfigurationPage::g_selectedChannelIndex;
static AoutDac7760ConfigurationPage g_aoutDac7760ConfigurationPage;

////////////////////////////////////////////////////////////////////////////////

class AoutDac7563ConfigurationPage : public SetPage {
public:
    static int g_selectedChannelIndex;

    void pageAlloc() {
        auto module = (Mio168Module *)g_slots[hmi::g_selectedSlotIndex];
        auto &channel = module->aoutDac7563Channels[g_selectedChannelIndex - AOUT_3_SUBCHANNEL_INDEX];

        m_triggerMode = m_triggerModeOrig = channel.m_triggerMode;
    }

	void pageWillAppear() {
		if (!getDirty()) {
			pageAlloc();
		}
	}

    int getDirty() { 
        return m_triggerMode != m_triggerModeOrig;
    }

	void apply() {
		if (getDirty()) {
			sendMessageToPsu((HighPriorityThreadMessage)PSU_MESSAGE_AOUT_DAC7563_CONFIGURE, hmi::g_selectedSlotIndex);
		}

		m_triggerMode = m_triggerModeOrig;
	}

	void set() {
		apply();
        popPage();
    }

	TriggerMode m_triggerMode;

private:
	TriggerMode m_triggerModeOrig;
};

int AoutDac7563ConfigurationPage::g_selectedChannelIndex;
static AoutDac7563ConfigurationPage g_aoutDac7563ConfigurationPage;

////////////////////////////////////////////////////////////////////////////////

Page *Mio168Module::getPageFromId(int pageId) {
    if (pageId == PAGE_ID_DIB_MIO168_DIN_CONFIGURATION) {
        return &g_dinConfigurationPage;
    } else if (pageId == PAGE_ID_DIB_MIO168_DOUT_CONFIGURATION) {
        return &g_doutConfigurationPage;
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
        channel.setMode((MeasureMode)g_ainConfigurationPage.m_mode);
		if (g_ainConfigurationPage.m_mode == MEASURE_MODE_VOLTAGE) {
			channel.setVoltageRange(g_ainConfigurationPage.m_voltageRange);
            channel.m_voltageNPLC = g_ainConfigurationPage.m_voltageNPLC;
		} else {
			channel.setCurrentRange(g_ainConfigurationPage.m_currentRange);
            channel.m_currentNPLC = g_ainConfigurationPage.m_currentNPLC;
		}
    } else if (type == PSU_MESSAGE_AOUT_DAC7760_CONFIGURE) {
        auto &channel = aoutDac7760Channels[AoutDac7760ConfigurationPage::g_selectedChannelIndex - AOUT_1_SUBCHANNEL_INDEX];
        channel.m_outputEnabled = g_aoutDac7760ConfigurationPage.m_outputEnabled;
        channel.m_mode = g_aoutDac7760ConfigurationPage.m_mode;
        channel.setCurrentRange(g_aoutDac7760ConfigurationPage.m_currentRange);
        channel.setVoltageRange(g_aoutDac7760ConfigurationPage.m_voltageRange);

		g_slots[slotIndex]->setFunctionGeneratorResourceTriggerMode(AoutDac7760ConfigurationPage::g_selectedChannelIndex, 0, g_aoutDac7760ConfigurationPage.m_triggerMode, nullptr);

        if (channel.getValue() < channel.getMinValue()) {
            channel.setValue(channel.getMinValue());
        } else if (channel.getValue() > channel.getMaxValue()) {
            channel.setValue(channel.getMaxValue());
        }

    } else if (type == PSU_MESSAGE_AOUT_DAC7563_CONFIGURE) {
        g_slots[slotIndex]->setFunctionGeneratorResourceTriggerMode(AoutDac7563ConfigurationPage::g_selectedChannelIndex, 0, g_aoutDac7563ConfigurationPage.m_triggerMode, nullptr);
	} else if (type == PSU_MESSAGE_DISK_DRIVE_OPERATION) {
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
        if (!dlog_record::isIdle() && dlog_record::g_activeRecording.parameters.isDlogItemEnabled(cursor / 8, DIN_SUBCHANNEL_INDEX, (DlogResourceType)(DLOG_RESOURCE_TYPE_DIN0 + cursor % 8))) {
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

    stringCopy(pinLabels + g_editLabelPinIndex * (CHANNEL_LABEL_MAX_LENGTH + 1), CHANNEL_LABEL_MAX_LENGTH + 1, value);
    
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

void action_dib_mio168_dout_show_configuration() {
    if (getActivePageId() == PAGE_ID_MAIN) {
        hmi::selectSlot(getFoundWidgetAtDown().cursor);
    }
    g_doutConfigurationPage.g_slotIndex = hmi::g_selectedSlotIndex;
    pushPage(PAGE_ID_DIB_MIO168_DOUT_CONFIGURATION);
}

void data_dib_mio168_dout_trigger_mode(DataOperationEnum operation, Cursor cursor, Value &value) {
	if (operation == DATA_OPERATION_GET) {
		int pin = cursor % 8;

        TriggerMode triggerMode;

        if (isPageOnStack(PAGE_ID_DIB_MIO168_DOUT_CONFIGURATION)) {
            triggerMode = g_doutConfigurationPage.m_triggerMode[pin];
        } else {
			triggerMode = ((Mio168Module *)g_slots[hmi::g_selectedSlotIndex])->doutChannel.m_triggerMode[pin];
        }

		value = MakeEnumDefinitionValue(triggerMode, ENUM_DEFINITION_CHANNEL_TRIGGER_MODE);
	}
}

void data_dib_mio168_dout_function_label(DataOperationEnum operation, Cursor cursor, Value &value) {
	if (operation == DATA_OPERATION_GET) {
        int pin = cursor % 8;
        auto waveformParameters = function_generator::getWaveformParameters(hmi::g_selectedSlotIndex, DOUT_SUBCHANNEL_INDEX, pin);
		value = function_generator::g_waveformShortLabel[waveformParameters ? waveformParameters->waveform : function_generator::WAVEFORM_NONE];
	}
}

void data_dib_mio168_dout_trigger_is_initiated(DataOperationEnum operation, Cursor cursor, Value &value) {
	if (operation == DATA_OPERATION_GET) {
		int pin = cursor % 8;

        TriggerMode triggerMode;

        if (isPageOnStack(PAGE_ID_DIB_MIO168_DOUT_CONFIGURATION)) {
            triggerMode = g_doutConfigurationPage.m_triggerMode[pin];
        } else {
			triggerMode = ((Mio168Module *)g_slots[hmi::g_selectedSlotIndex])->doutChannel.m_triggerMode[pin];
        }

		value = (trigger::isInitiated() || trigger::isTriggered()) && triggerMode == TRIGGER_MODE_FUNCTION_GENERATOR;
	} else if (operation == DATA_OPERATION_IS_BLINKING) {
		value = trigger::isInitiated();
	}
}

static void onSetDoutTriggerMode(uint16_t value) {
	popPage();

	g_doutConfigurationPage.m_triggerMode[g_doutConfigurationPage.g_pin] = (TriggerMode)value;
}

void action_dib_mio168_dout_select_trigger_mode() {
	static EnumItem g_enumDefinitionMio168AoutTriggerMode[] = {
		{ TRIGGER_MODE_FIXED, "Fixed" },
		{ TRIGGER_MODE_FUNCTION_GENERATOR, "Function generator" },
		{ 0, 0 }
	};

    g_doutConfigurationPage.g_pin = getFoundWidgetAtDown().cursor % 8;

	TriggerMode triggerMode = ((Mio168Module *)g_slots[g_doutConfigurationPage.g_slotIndex])->doutChannel.m_triggerMode[g_doutConfigurationPage.g_pin];
	pushSelectFromEnumPage(g_enumDefinitionMio168AoutTriggerMode, triggerMode, nullptr, onSetDoutTriggerMode);
}

void action_dib_mio168_dout_show_function() {
	int cursor = getFoundWidgetAtDown().cursor;
	
	int slotIndex;

	if (getActivePageId() == PAGE_ID_DIB_MIO168_DOUT_CONFIGURATION) {
		slotIndex = g_doutConfigurationPage.g_slotIndex;
		if (g_doutConfigurationPage.getDirty()) {
			g_doutConfigurationPage.apply();
		}
	} else {
		slotIndex = cursor / 8;
	}

	int pin = cursor % 8;

    g_slots[slotIndex]->setFunctionGeneratorResourceTriggerMode(DOUT_SUBCHANNEL_INDEX, pin, TRIGGER_MODE_FUNCTION_GENERATOR, nullptr);

	pushPage(PAGE_ID_SYS_SETTINGS_FUNCTION_GENERATOR);

	function_generator::selectWaveformParametersForChannel(slotIndex, DOUT_SUBCHANNEL_INDEX, pin);
}

////////////////////////////////////////////////////////////////////////////////

bool isDlogActiveOnAinChannel(int slotIndex, int ainChannelIndex) {
    if (dlog_record::isIdle()) {
        return false;
    }

    auto mio168Module = (Mio168Module *)g_slots[slotIndex];
    auto &channel = mio168Module->ainChannels[ainChannelIndex];
    Unit unit = channel.getUnit();

    eez::DlogResourceType resourceType = unit == UNIT_VOLT ?
		DLOG_RESOURCE_TYPE_U : DLOG_RESOURCE_TYPE_I;

    return dlog_record::g_activeRecording.parameters.isDlogItemEnabled(slotIndex,
        AIN_1_SUBCHANNEL_INDEX + ainChannelIndex, resourceType);
}

void data_dib_mio168_ain_channels(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_COUNT) {
        value = 4;
    } else if (operation == DATA_OPERATION_GET_CURSOR_VALUE) {
        value = hmi::g_selectedSlotIndex * 4 + value.getInt();
    }
}

void data_dib_mio168_ain_label(DataOperationEnum operation, Cursor cursor, Value &value) {
	int slotIndex = cursor / 4;
	int ainChannelIndex = cursor % 4;
	
	if (operation == DATA_OPERATION_GET) {
        if (isPageOnStack(PAGE_ID_SYS_SETTINGS_LABELS_AND_COLORS)) {
            const char *label = LabelsAndColorsPage::getChannelLabel(slotIndex, AIN_1_SUBCHANNEL_INDEX + ainChannelIndex);
            if (*label) {
                value = label;
            } else {
                value = g_slots[slotIndex]->getDefaultChannelLabel(AIN_1_SUBCHANNEL_INDEX + ainChannelIndex);
            }            
        } else {
            AinConfigurationPage *page = (AinConfigurationPage *)getPage(PAGE_ID_DIB_MIO168_AIN_CONFIGURATION);
            if (page) {
				slotIndex = hmi::g_selectedSlotIndex;
                ainChannelIndex = page->g_selectedChannelIndex - AIN_1_SUBCHANNEL_INDEX;
            }

            value = getChannelLabel(slotIndex, AIN_1_SUBCHANNEL_INDEX + ainChannelIndex);
        }
    } else if (operation == DATA_OPERATION_GET_BACKGROUND_COLOR) {
		if (isDlogActiveOnAinChannel(slotIndex, ainChannelIndex)) {
			value = Value(COLOR_ID_DATA_LOGGING, VALUE_TYPE_UINT16);
		}
	}
}

void data_dib_mio168_ain_label_short(DataOperationEnum operation, Cursor cursor, Value &value) {
	int slotIndex = cursor / 4;
	int ainChannelIndex = cursor % 4;
	
	if (operation == DATA_OPERATION_GET) {
        value = ((Mio168Module *)g_slots[slotIndex])->getShortChannelLabel(AIN_1_SUBCHANNEL_INDEX + ainChannelIndex);
    } else if (operation == DATA_OPERATION_GET_BACKGROUND_COLOR) {
		if (isDlogActiveOnAinChannel(slotIndex, ainChannelIndex)) {
			value = Value(COLOR_ID_DATA_LOGGING, VALUE_TYPE_UINT16);
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
        float fvalue = channel.getValue();
        Unit unit = channel.getUnit();
        auto numFixedDecimals = channel.getNumFixedDecimals();
        if (unit == UNIT_MICRO_AMPER) {
        	fvalue *= 10000000.0f;
            numFixedDecimals -= 6;
        } else if (unit == UNIT_MILLI_AMPER) {
        	fvalue *= 1000.0f;
            numFixedDecimals -= 3;
        }
 
        value = MakeValue(fvalue, unit, FLOAT_OPTIONS_SET_NUM_FIXED_DECIMALS(numFixedDecimals));
	} else if (operation == DATA_OPERATION_GET_BACKGROUND_COLOR) {
		if (isDlogActiveOnAinChannel(slotIndex, ainChannelIndex)) {
			value = Value(COLOR_ID_DATA_LOGGING, VALUE_TYPE_UINT16);
		}
	}
}

void data_dib_mio168_ain_is_overflow(DataOperationEnum operation, Cursor cursor, Value &value) {
	int slotIndex = cursor / 4;
	int ainChannelIndex = cursor % 4;
	if (operation == DATA_OPERATION_GET) {
		auto mio168Module = (Mio168Module *)g_slots[slotIndex];
        value = mio168Module->isFaultP(ainChannelIndex) || mio168Module->isFaultN(ainChannelIndex);
	}
}

void data_dib_mio168_ain_is_error(DataOperationEnum operation, Cursor cursor, Value &value) {
	int slotIndex = cursor / 4;
	int ainChannelIndex = cursor % 4;
	if (operation == DATA_OPERATION_GET) {
		auto mio168Module = (Mio168Module *)g_slots[slotIndex];
        value = mio168Module->isError(ainChannelIndex);
	}
}

void data_dib_mio168_ain_mode(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = g_ainConfigurationPage.m_mode;
    }
}

void action_dib_mio168_ain_select_mode() {
    g_ainConfigurationPage.m_mode = g_ainConfigurationPage.m_mode == MEASURE_MODE_VOLTAGE ? MEASURE_MODE_CURRENT : MEASURE_MODE_VOLTAGE;
}

void data_dib_mio168_ain_mode_is_multiple_selection_available(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = g_ainConfigurationPage.hasMultipleModes();
    }
}

void data_dib_mio168_ain_range_is_multiple_selection_available(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = g_ainConfigurationPage.getRangeEnumDefinition() && g_ainConfigurationPage.getRangeEnumDefinition()[1].menuLabel ? 1 : 0;
    }
}

void data_dib_mio168_ain_range(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = getWidgetLabel(g_ainConfigurationPage.getRangeEnumDefinition(),
			g_ainConfigurationPage.m_mode == MEASURE_MODE_VOLTAGE ? g_ainConfigurationPage.m_voltageRange : g_ainConfigurationPage.m_currentRange);
    }
}

void onSetAinRange(uint16_t value) {
    popPage();
	if (g_ainConfigurationPage.m_mode == MEASURE_MODE_VOLTAGE) {
		g_ainConfigurationPage.m_voltageRange = (uint8_t)value;
	} else {
		g_ainConfigurationPage.m_currentRange = (uint8_t)value;
	}
}

void action_dib_mio168_ain_select_range() {
    pushSelectFromEnumPage(g_ainConfigurationPage.getRangeEnumDefinition(),
		g_ainConfigurationPage.m_mode == MEASURE_MODE_VOLTAGE ? g_ainConfigurationPage.m_voltageRange : g_ainConfigurationPage.m_currentRange,
		nullptr, onSetAinRange);
}

void data_dib_mio168_ain_nplc(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        bool focused = g_focusCursor == cursor && g_focusDataId == DATA_ID_DIB_MIO168_AIN_NPLC;
        if (focused && g_focusEditValue.getType() != VALUE_TYPE_NONE) {
            value = g_focusEditValue;
        } else if (focused && getActivePageId() == PAGE_ID_EDIT_MODE_KEYPAD && edit_mode_keypad::g_keypad->isEditing()) {
            data_keypad_text(operation, cursor, value);
        } else {
            value = MakeValue(
                g_ainConfigurationPage.m_mode == MEASURE_MODE_VOLTAGE ?
                    g_ainConfigurationPage.m_voltageNPLC :
                    g_ainConfigurationPage.m_currentNPLC,
                UNIT_UNKNOWN
            );
        }
    } else if (operation == DATA_OPERATION_GET_MIN) {
        value = MakeValue(0.0f, UNIT_UNKNOWN);
    } else if (operation == DATA_OPERATION_GET_MAX) {
        value = MakeValue(25.0f, UNIT_UNKNOWN);
    } else if (operation == DATA_OPERATION_GET_NAME) {
        value = "NPLC";
    } else if (operation == DATA_OPERATION_GET_UNIT) {
        value = UNIT_UNKNOWN;
    } else if (operation == DATA_OPERATION_GET_ENCODER_STEP_VALUES) {
        static float values[] = { 0.1f, 0.2f, 0.5f, 1.0f };

        StepValues *stepValues = value.getStepValues();

        stepValues->values = values;
        stepValues->count = sizeof(values) / sizeof(float);
        stepValues->unit = UNIT_UNKNOWN;

        stepValues->encoderSettings.accelerationEnabled = false;

        stepValues->encoderSettings.mode = edit_mode_step::g_mio168NplcEncoderMode;

        value = 1;
    } else if (operation == DATA_OPERATION_SET_ENCODER_MODE) {
		edit_mode_step::g_mio168NplcEncoderMode = (EncoderMode)value.getInt();
    } else if (operation == DATA_OPERATION_SET) {
        if (g_ainConfigurationPage.m_mode == MEASURE_MODE_VOLTAGE) {
            g_ainConfigurationPage.m_voltageNPLC = value.getFloat();
        } else {
            g_ainConfigurationPage.m_currentNPLC = value.getFloat();
        }
    }
}

void data_dib_mio168_ain_aperture(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = Value(
            (
                g_ainConfigurationPage.m_mode == MEASURE_MODE_VOLTAGE ?
                    g_ainConfigurationPage.m_voltageNPLC :
                    g_ainConfigurationPage.m_currentNPLC
            ) / persist_conf::getPowerLineFrequency(), UNIT_SECOND);
    }
}

void data_dib_mio168_ain_has_power_column(DataOperationEnum operation, Cursor cursor, Value &value) {
	if (operation == DATA_OPERATION_GET) {
		int slotIndex;
		if (getActivePageId() == PAGE_ID_MAIN) {
			slotIndex = cursor;
		} else {
			slotIndex = hmi::g_selectedSlotIndex;
		}

		auto mio168Module = (Mio168Module *)g_slots[slotIndex];

		value = 
            mio168Module->afeVersion == 1 ?
                (mio168Module->ainChannels[0].getMode() != mio168Module->ainChannels[2].getMode() ||
                mio168Module->ainChannels[1].getMode() != mio168Module->ainChannels[3].getMode()) :
            mio168Module->afeVersion == 2 ?
                true :
            mio168Module->afeVersion == 3 ?
                true :
            false;
	}
}

void data_dib_mio168_ain_has_p1(DataOperationEnum operation, Cursor cursor, Value &value) {
	if (operation == DATA_OPERATION_GET) {
		int slotIndex;
		if (getActivePageId() == PAGE_ID_MAIN) {
			slotIndex = cursor;
		} else {
			slotIndex = hmi::g_selectedSlotIndex;
		}

		auto mio168Module = (Mio168Module *)g_slots[slotIndex];

		value = 
            mio168Module->afeVersion == 1 ?
                (mio168Module->ainChannels[0].getMode() != mio168Module->ainChannels[2].getMode()) :
            mio168Module->afeVersion == 2 ?
                true :
            mio168Module->afeVersion == 3 ?
                true :
            false;
	}
}

void data_dib_mio168_ain_p1(DataOperationEnum operation, Cursor cursor, Value &value) {
	if (operation == DATA_OPERATION_GET) {
		int slotIndex;
		if (getActivePageId() == PAGE_ID_MAIN) {
			slotIndex = cursor;
		} else {
			slotIndex = hmi::g_selectedSlotIndex;
		}

		auto mio168Module = (Mio168Module *)g_slots[slotIndex];

		value = MakeValue(
			roundPrec(
                mio168Module->afeVersion == 1 ?
                    (mio168Module->ainChannels[0].m_value * mio168Module->ainChannels[2].m_value) :
                mio168Module->afeVersion == 2 ?
                    (mio168Module->ainChannels[0].m_value * mio168Module->ainChannels[2].m_value) :
                mio168Module->afeVersion == 3 ?
                    (mio168Module->ainChannels[0].m_value * mio168Module->ainChannels[1].m_value) :
                NAN, 1E-4f),
			UNIT_WATT,
			FLOAT_OPTIONS_SET_NUM_FIXED_DECIMALS(4)
		);
	}
}

void data_dib_mio168_ain_has_p2(DataOperationEnum operation, Cursor cursor, Value &value) {
	if (operation == DATA_OPERATION_GET) {
		int slotIndex;
		if (getActivePageId() == PAGE_ID_MAIN) {
			slotIndex = cursor;
		} else {
			slotIndex = hmi::g_selectedSlotIndex;
		}

		auto mio168Module = (Mio168Module *)g_slots[slotIndex];

		value = 
            mio168Module->afeVersion == 1 ?
                (mio168Module->ainChannels[1].getMode() != mio168Module->ainChannels[3].getMode()) :
            mio168Module->afeVersion == 2 ?
                true :
            mio168Module->afeVersion == 3 ?
                true:
            false;
	}
}

void data_dib_mio168_ain_p2(DataOperationEnum operation, Cursor cursor, Value &value) {
	if (operation == DATA_OPERATION_GET) {
		int slotIndex;
		if (getActivePageId() == PAGE_ID_MAIN) {
			slotIndex = cursor;
		} else {
			slotIndex = hmi::g_selectedSlotIndex;
		}

		auto mio168Module = (Mio168Module *)g_slots[slotIndex];

		value = MakeValue(
			roundPrec(
                mio168Module->afeVersion == 1 ?
                    (mio168Module->ainChannels[1].m_value * mio168Module->ainChannels[3].m_value) :
                mio168Module->afeVersion == 2 ?
                    (mio168Module->ainChannels[1].m_value * mio168Module->ainChannels[3].m_value) :
                mio168Module->afeVersion == 3 ?
                    (mio168Module->ainChannels[2].m_value * mio168Module->ainChannels[3].m_value) :
                NAN, 1E-4f),
			UNIT_WATT,
			FLOAT_OPTIONS_SET_NUM_FIXED_DECIMALS(4)
		);
	}
}

void data_dib_mio168_ain_has_efficiency(DataOperationEnum operation, Cursor cursor, Value &value) {
	if (operation == DATA_OPERATION_GET) {
		int slotIndex;
		if (getActivePageId() == PAGE_ID_MAIN) {
			slotIndex = cursor;
		} else {
			slotIndex = hmi::g_selectedSlotIndex;
		}

		auto mio168Module = (Mio168Module *)g_slots[slotIndex];

		value =
            mio168Module->afeVersion == 1 ?
                (mio168Module->ainChannels[0].getMode() != mio168Module->ainChannels[2].getMode() &&
                mio168Module->ainChannels[1].getMode() != mio168Module->ainChannels[3].getMode()) :
            mio168Module->afeVersion == 2 ?
                true :
            mio168Module->afeVersion == 3 ?
                true :
            false;
	}
}

void data_dib_mio168_ain_efficiency_formula(DataOperationEnum operation, Cursor cursor, Value &value) {
	if (operation == DATA_OPERATION_GET) {
		int slotIndex;
		if (getActivePageId() == PAGE_ID_MAIN) {
			slotIndex = cursor;
		} else {
			slotIndex = hmi::g_selectedSlotIndex;
		}

		auto mio168Module = (Mio168Module *)g_slots[slotIndex];

		value = 
			mio168Module->efficiencyFormula == EFFICIENCY_FORMULA_P1_OVER_P2 ? "P1/P2" :
			mio168Module->efficiencyFormula == EFFICIENCY_FORMULA_P2_OVER_P1 ? "P2/P1" :
			"\xc1 OFF";
	}
}

void data_dib_mio168_ain_calc_efficiency(DataOperationEnum operation, Cursor cursor, Value &value) {
	if (operation == DATA_OPERATION_GET) {
		int slotIndex;
		if (getActivePageId() == PAGE_ID_MAIN) {
			slotIndex = cursor;
		} else {
			slotIndex = hmi::g_selectedSlotIndex;
		}

		auto mio168Module = (Mio168Module *)g_slots[slotIndex];

		value = mio168Module->efficiencyFormula != EFFICIENCY_FORMULA_NONE;
	}
}

void action_dib_mio168_ain_toggle_efficiency_formula() {
	int slotIndex;
	if (getActivePageId() == PAGE_ID_MAIN) {
		auto cursor = getFoundWidgetAtDown().cursor;
		slotIndex = cursor;
	} else {
		slotIndex = hmi::g_selectedSlotIndex;
	}

	auto mio168Module = (Mio168Module *)g_slots[slotIndex];

	mio168Module->efficiencyFormula = (mio168Module->efficiencyFormula + 1) % 3;
}

void data_dib_mio168_ain_efficiency(DataOperationEnum operation, Cursor cursor, Value &value) {
	if (operation == DATA_OPERATION_GET) {
		int slotIndex;
		if (getActivePageId() == PAGE_ID_MAIN) {
			slotIndex = cursor;
		} else {
			slotIndex = hmi::g_selectedSlotIndex;
		}

		auto mio168Module = (Mio168Module *)g_slots[slotIndex];

		auto p1 = roundPrec(
            mio168Module->afeVersion == 1 ?
                (mio168Module->ainChannels[0].m_value * mio168Module->ainChannels[2].m_value) :
            mio168Module->afeVersion == 2 ?
                (mio168Module->ainChannels[0].m_value * mio168Module->ainChannels[2].m_value) :
            mio168Module->afeVersion == 3 ?
                (mio168Module->ainChannels[0].m_value * mio168Module->ainChannels[1].m_value) :
            NAN, 1E-4f);

		auto p2 = roundPrec(
            mio168Module->afeVersion == 1 ?
                (mio168Module->ainChannels[1].m_value * mio168Module->ainChannels[3].m_value) :
            mio168Module->afeVersion == 2 ?
                (mio168Module->ainChannels[1].m_value * mio168Module->ainChannels[3].m_value) :
            mio168Module->afeVersion == 3 ?
                (mio168Module->ainChannels[2].m_value * mio168Module->ainChannels[3].m_value) :
            NAN, 1E-4f);

		auto efficiency = roundPrec((mio168Module->efficiencyFormula == EFFICIENCY_FORMULA_P1_OVER_P2 ? p1 / p2 : p2 / p1) * 100.0f, 0.01f);

		if (isNaN(efficiency)) {
			value = "   -";
		} else {
			value = MakeValue(efficiency, UNIT_NONE, FLOAT_OPTIONS_SET_NUM_FIXED_DECIMALS(2));
		}
	}
}

void data_dib_mio168_ain_is_dlog_active(DataOperationEnum operation, Cursor cursor, Value &value) {
	int slotIndex = cursor / 4;
	int ainChannelIndex = cursor % 4;
	if (operation == DATA_OPERATION_GET) {
        value = isDlogActiveOnAinChannel(slotIndex, ainChannelIndex);
	}
}

void data_dib_mio168_ain_is_auto_range(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = g_ainConfigurationPage.isAutoRange();
    }
}

void data_dib_mio168_ain_mode_and_range(DataOperationEnum operation, Cursor cursor, Value &value) {
    int slotIndex = cursor / 4;
    int ainChannelIndex = cursor % 4;
	if (operation == DATA_OPERATION_GET) {
        if (((Mio168Module *)g_slots[slotIndex])->ainChannels[ainChannelIndex].isAutoRange()) {
            value = getWidgetLabel(
                ((Mio168Module *)g_slots[slotIndex])->ainChannels[ainChannelIndex].getModeRangeEnumDefinitionForAutoRange(),
                AinConfigurationPage::getModeRangeForAutoRange(slotIndex, AIN_1_SUBCHANNEL_INDEX + ainChannelIndex)
            );
        } else {
            value = getWidgetLabel(
                ((Mio168Module *)g_slots[slotIndex])->ainChannels[ainChannelIndex].getModeRangeEnumDefinition(),
                AinConfigurationPage::getModeRange(slotIndex, AIN_1_SUBCHANNEL_INDEX + ainChannelIndex)
            );
        }
	} else if (operation == DATA_OPERATION_GET_BACKGROUND_COLOR) {
		if (isDlogActiveOnAinChannel(slotIndex, ainChannelIndex)) {
			value = Value(COLOR_ID_DATA_LOGGING, VALUE_TYPE_UINT16);
		}
	}
}

void onSetAinModeRange(uint16_t value) {
    popPage();
    AinConfigurationPage::setModeRange(
        hmi::g_selectedSlotIndex,
        AinConfigurationPage::g_selectedChannelIndex,
        (uint8_t)value\
    );
}

void action_dib_mio168_ain_select_mode_and_range() {
    int cursor = getFoundWidgetAtDown().cursor;
    
    int slotIndex = cursor / 4;
	hmi::selectSlot(slotIndex);
    int subchannelIndex = AIN_1_SUBCHANNEL_INDEX + cursor % 4;

    AinConfigurationPage::g_selectedChannelIndex = subchannelIndex;

    pushSelectFromEnumPage(
		((Mio168Module *)g_slots[slotIndex])->ainChannels[subchannelIndex - AIN_1_SUBCHANNEL_INDEX].getModeRangeEnumDefinition(),
        AinConfigurationPage::getModeRange(slotIndex, subchannelIndex),
        nullptr, 
        onSetAinModeRange
    );
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

void data_dib_mio168_ain_active_power(DataOperationEnum operation, Cursor cursor, Value &value) {
	if (operation == DATA_OPERATION_GET) {
		int slotIndex;
		if (getActivePageId() == PAGE_ID_MAIN) {
			slotIndex = cursor;
		} else {
			slotIndex = hmi::g_selectedSlotIndex;
		}

		auto mio168Module = (Mio168Module *)g_slots[slotIndex];

		value = MakeValue(
			roundPrec(mio168Module->activePower, 1E-4f),
			UNIT_WATT,
			FLOAT_OPTIONS_SET_NUM_FIXED_DECIMALS(4)
		);
	}
}

void data_dib_mio168_ain_reactive_power(DataOperationEnum operation, Cursor cursor, Value &value) {
	if (operation == DATA_OPERATION_GET) {
		int slotIndex;
		if (getActivePageId() == PAGE_ID_MAIN) {
			slotIndex = cursor;
		} else {
			slotIndex = hmi::g_selectedSlotIndex;
		}

		auto mio168Module = (Mio168Module *)g_slots[slotIndex];

		value = MakeValue(
			roundPrec(mio168Module->reactivePower, 1E-4f),
			UNIT_VOLT_AMPERE_REACTIVE,
			FLOAT_OPTIONS_SET_NUM_FIXED_DECIMALS(4)
		);
	}
}

void data_dib_mio168_ain_volt_rms(DataOperationEnum operation, Cursor cursor, Value &value) {
	if (operation == DATA_OPERATION_GET) {
		int slotIndex;
		if (getActivePageId() == PAGE_ID_MAIN) {
			slotIndex = cursor;
		} else {
			slotIndex = hmi::g_selectedSlotIndex;
		}

		auto mio168Module = (Mio168Module *)g_slots[slotIndex];

		value = MakeValue(
			roundPrec(mio168Module->voltRMS, 1E-4f),
			UNIT_VOLT,
			FLOAT_OPTIONS_SET_NUM_FIXED_DECIMALS(4)
		);
	}
}

void data_dib_mio168_ain_curr_rms(DataOperationEnum operation, Cursor cursor, Value &value) {
	if (operation == DATA_OPERATION_GET) {
		int slotIndex;
		if (getActivePageId() == PAGE_ID_MAIN) {
			slotIndex = cursor;
		} else {
			slotIndex = hmi::g_selectedSlotIndex;
		}

		auto mio168Module = (Mio168Module *)g_slots[slotIndex];

		value = MakeValue(
			roundPrec(mio168Module->currRMS, 1E-4f),
			UNIT_AMPER,
			FLOAT_OPTIONS_SET_NUM_FIXED_DECIMALS(4)
		);
	}
}

void data_dib_mio168_ain_apparent_power(DataOperationEnum operation, Cursor cursor, Value &value) {
	if (operation == DATA_OPERATION_GET) {
		int slotIndex;
		if (getActivePageId() == PAGE_ID_MAIN) {
			slotIndex = cursor;
		} else {
			slotIndex = hmi::g_selectedSlotIndex;
		}

		auto mio168Module = (Mio168Module *)g_slots[slotIndex];

		value = MakeValue(
			roundPrec(mio168Module->apparentPower, 1E-4f),
			UNIT_VOLT_AMPERE,
			FLOAT_OPTIONS_SET_NUM_FIXED_DECIMALS(4)
		);
	}
}

void data_dib_mio168_ain_power_factor(DataOperationEnum operation, Cursor cursor, Value &value) {
	if (operation == DATA_OPERATION_GET) {
		int slotIndex;
		if (getActivePageId() == PAGE_ID_MAIN) {
			slotIndex = cursor;
		} else {
			slotIndex = hmi::g_selectedSlotIndex;
		}

		auto mio168Module = (Mio168Module *)g_slots[slotIndex];

		value = MakeValue(
			roundPrec(mio168Module->powerFactor, 1E-3f),
			UNIT_NONE,
			FLOAT_OPTIONS_SET_NUM_FIXED_DECIMALS(3)
		);
	}
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

void data_dib_mio168_aout_label_short(DataOperationEnum operation, Cursor cursor, Value &value) {
	int slotIndex = cursor / 4;
	int ainChannelIndex = cursor % 4;
	
	if (operation == DATA_OPERATION_GET) {
        value = ((Mio168Module *)g_slots[slotIndex])->getShortChannelLabel(AOUT_1_SUBCHANNEL_INDEX + ainChannelIndex);
    } else if (operation == DATA_OPERATION_GET_BACKGROUND_COLOR) {
		if (isDlogActiveOnAinChannel(slotIndex, ainChannelIndex)) {
			value = Value(COLOR_ID_DATA_LOGGING, VALUE_TYPE_UINT16);
		}
	}
}

void data_dib_mio168_aout_label_label(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        static const char *g_aoutLabelLabels[4] = { "AOUT1 label:", "AOUT2 label:", "AOUT3 label:", "AOUT4 label:" };
        value = g_aoutLabelLabels[cursor % 4];
    }
}

void data_dib_mio168_aout_value_is_off(DataOperationEnum operation, Cursor cursor, Value &value) {
	int slotIndex = cursor / 4;
	int aoutChannelIndex = cursor % 4;

	if (operation == DATA_OPERATION_GET) {
		if (aoutChannelIndex < 2) {
			auto &channel = ((Mio168Module *)g_slots[slotIndex])->aoutDac7760Channels[aoutChannelIndex];
			value = !channel.m_outputEnabled;
		} else {
			value = 0;
		}
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
            stepValues->encoderSettings.mode = channel.getEncoderMode();
        } else {
            auto &channel = ((Mio168Module *)g_slots[slotIndex])->aoutDac7563Channels[aoutChannelIndex - 2];
            channel.getStepValues(stepValues);
            stepValues->encoderSettings.mode = channel.getEncoderMode();
        }

        value = 1;
    } else if (operation == DATA_OPERATION_SET_ENCODER_MODE) {
        if (aoutChannelIndex < 2) {
            auto &channel = ((Mio168Module *)g_slots[slotIndex])->aoutDac7760Channels[aoutChannelIndex];
            channel.setEncoderMode((EncoderMode)value.getInt());
        } else {
            auto &channel = ((Mio168Module *)g_slots[slotIndex])->aoutDac7563Channels[aoutChannelIndex - 2];
            channel.setEncoderMode((EncoderMode)value.getInt());
        }
    } else if (operation == DATA_OPERATION_SET) {
        if (aoutChannelIndex < 2) {
            auto &channel = ((Mio168Module *)g_slots[slotIndex])->aoutDac7760Channels[aoutChannelIndex];
            channel.setValue(roundPrec(value.getFloat(), channel.getResolution()));
        } else {
            auto &channel = ((Mio168Module *)g_slots[slotIndex])->aoutDac7563Channels[aoutChannelIndex - 2];
            channel.m_value = roundPrec(value.getFloat(), channel.getResolution());
        }
	} else if (operation == DATA_OPERATION_GET_SLOT_AND_SUBCHANNEL_INDEX) {
		value.pairOfInt16_.first = slotIndex;
		value.pairOfInt16_.second = AOUT_1_SUBCHANNEL_INDEX +  aoutChannelIndex;
		value.type_ = VALUE_TYPE_UINT32;
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

void data_dib_mio168_aout_voltage_range(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = getWidgetLabel(::g_aoutVoltageRangeEnumDefinition, g_aoutDac7760ConfigurationPage.m_voltageRange);
    }
}

void onSetVoltageRange(uint16_t value) {
    popPage();
    g_aoutDac7760ConfigurationPage.m_voltageRange = (uint8_t)value;
}

void action_dib_mio168_aout_select_voltage_range() {
    pushSelectFromEnumPage(g_aoutVoltageRangeEnumDefinition, g_aoutDac7760ConfigurationPage.m_voltageRange, nullptr, onSetVoltageRange);
}

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

void data_dib_mio168_aout_mode_and_range(DataOperationEnum operation, Cursor cursor, Value &value) {
	if (operation == DATA_OPERATION_GET) {
        int slotIndex = cursor / 4;
        int subchannelIndex = AOUT_1_SUBCHANNEL_INDEX + cursor % 4;
		value = getWidgetLabel(
            AoutDac7760ConfigurationPage::getModeRangeShortEnumDefinition(slotIndex, subchannelIndex), 
            AoutDac7760ConfigurationPage::getModeRange(slotIndex, subchannelIndex)
        );
	}
}

void onSetAoutModeRange(uint16_t value) {
    popPage();
    AoutDac7760ConfigurationPage::setModeRange(
        hmi::g_selectedSlotIndex,
        AoutDac7760ConfigurationPage::g_selectedChannelIndex,
        (uint8_t)value
    );
}

void action_dib_mio168_aout_select_mode_and_range() {
    int cursor = getFoundWidgetAtDown().cursor;
    
    int slotIndex = cursor / 4;
    int subchannelIndex = AOUT_1_SUBCHANNEL_INDEX + cursor % 4;

    hmi::selectSlot(slotIndex);
    AoutDac7760ConfigurationPage::g_selectedChannelIndex = subchannelIndex;

    pushSelectFromEnumPage(
        AoutDac7760ConfigurationPage::getModeRangeEnumDefinition(slotIndex, subchannelIndex),
        AoutDac7760ConfigurationPage::getModeRange(slotIndex, subchannelIndex),
        nullptr, 
		onSetAoutModeRange
    );
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

void data_dib_mio168_aout_trigger_mode(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
		TriggerMode triggerMode;
        if (getActivePageId() == PAGE_ID_DIB_MIO168_AOUT_DAC7760_CONFIGURATION) {
			triggerMode = g_aoutDac7760ConfigurationPage.m_triggerMode;
        } else if (getActivePageId() == PAGE_ID_DIB_MIO168_AOUT_DAC7563_CONFIGURATION) {
			triggerMode = g_aoutDac7563ConfigurationPage.m_triggerMode;
        } else {
            int slotIndex = cursor / 4;
            int aoutChannelIndex = cursor % 4;
            if (aoutChannelIndex < 2) {
                auto &channel = ((Mio168Module *)g_slots[slotIndex])->aoutDac7760Channels[aoutChannelIndex];
				triggerMode = channel.m_triggerMode;
            } else {
                auto &channel = ((Mio168Module *)g_slots[slotIndex])->aoutDac7563Channels[aoutChannelIndex - 2];
				triggerMode = channel.m_triggerMode;
            }
        }
		value = MakeEnumDefinitionValue(triggerMode, ENUM_DEFINITION_CHANNEL_TRIGGER_MODE);
    }
}

void data_dib_mio168_aout_function_label(DataOperationEnum operation, Cursor cursor, Value &value) {
	if (operation == DATA_OPERATION_GET) {
        int slotIndex = cursor / 4;
        int aoutChannelIndex = cursor % 4;
        auto waveformParameters = function_generator::getWaveformParameters(slotIndex, AOUT_1_SUBCHANNEL_INDEX + aoutChannelIndex, 0);
		value = function_generator::g_waveformShortLabel[waveformParameters ? waveformParameters->waveform : function_generator::WAVEFORM_NONE];
	}
}

void data_dib_mio168_aout_trigger_is_initiated(DataOperationEnum operation, Cursor cursor, Value &value) {
	if (operation == DATA_OPERATION_GET) {
		TriggerMode triggerMode;

		int slotIndex = cursor / 4;
		int aoutChannelIndex = cursor % 4;
		if (aoutChannelIndex < 2) {
			auto &channel = ((Mio168Module *)g_slots[slotIndex])->aoutDac7760Channels[aoutChannelIndex];
			triggerMode = channel.m_triggerMode;
		} else {
			auto &channel = ((Mio168Module *)g_slots[slotIndex])->aoutDac7563Channels[aoutChannelIndex - 2];
			triggerMode = channel.m_triggerMode;
		}

		value = (trigger::isInitiated() || trigger::isTriggered()) && triggerMode == TRIGGER_MODE_FUNCTION_GENERATOR;
	} else if (operation == DATA_OPERATION_IS_BLINKING) {
		value = trigger::isInitiated();
	}
}

static void onSetAoutTriggerMode(uint16_t value) {
	popPage();

    auto triggerMode = (TriggerMode)value;
    if (getActivePageId() == PAGE_ID_DIB_MIO168_AOUT_DAC7760_CONFIGURATION) {
        g_aoutDac7760ConfigurationPage.m_triggerMode = triggerMode;
    } else {
        g_aoutDac7563ConfigurationPage.m_triggerMode = triggerMode;
    }
}

void action_dib_mio168_aout_select_trigger_mode() {
    static EnumItem g_enumDefinitionMio168AoutTriggerMode[] = {
        { TRIGGER_MODE_FIXED, "Fixed" },
        { TRIGGER_MODE_FUNCTION_GENERATOR, "Function generator" },
        { 0, 0 } 
    };

    TriggerMode triggerMode;
    if (getActivePageId() == PAGE_ID_DIB_MIO168_AOUT_DAC7760_CONFIGURATION) {
        triggerMode = g_aoutDac7760ConfigurationPage.m_triggerMode;
    } else {
        triggerMode = g_aoutDac7563ConfigurationPage.m_triggerMode;
    }

	pushSelectFromEnumPage(g_enumDefinitionMio168AoutTriggerMode, triggerMode, nullptr, onSetAoutTriggerMode);
}

void action_dib_mio168_aout_show_function() {
	int slotIndex;
	int subchannelIndex;
	if (getActivePageId() == PAGE_ID_DIB_MIO168_AOUT_DAC7760_CONFIGURATION) {
		slotIndex = hmi::g_selectedSlotIndex;
		subchannelIndex = g_aoutDac7760ConfigurationPage.g_selectedChannelIndex;
		if (g_aoutDac7760ConfigurationPage.getDirty()) {
			g_aoutDac7760ConfigurationPage.apply();
		}
	} else if (getActivePageId() == PAGE_ID_DIB_MIO168_AOUT_DAC7563_CONFIGURATION) {
		slotIndex = hmi::g_selectedSlotIndex;
		subchannelIndex = g_aoutDac7563ConfigurationPage.g_selectedChannelIndex;
		if (g_aoutDac7563ConfigurationPage.getDirty()) {
			g_aoutDac7563ConfigurationPage.apply();
		}
	} else {
		int cursor = getFoundWidgetAtDown().cursor;
		slotIndex = cursor / 4;
		subchannelIndex = AOUT_1_SUBCHANNEL_INDEX + cursor % 4;
	}

	g_slots[slotIndex]->setFunctionGeneratorResourceTriggerMode(subchannelIndex, 0, TRIGGER_MODE_FUNCTION_GENERATOR, nullptr);

	pushPage(PAGE_ID_SYS_SETTINGS_FUNCTION_GENERATOR);

	function_generator::selectWaveformParametersForChannel(slotIndex, subchannelIndex, 0);
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

void data_dib_mio168_pwm_label_short(DataOperationEnum operation, Cursor cursor, Value &value) {
	if (operation == DATA_OPERATION_GET) {
        int slotIndex = cursor / 2;
        int pwmChannelIndex = cursor % 2;
        value = ((Mio168Module *)g_slots[slotIndex])->getShortChannelLabel(PWM_1_SUBCHANNEL_INDEX + pwmChannelIndex);
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

        stepValues->encoderSettings.mode = eez::psu::gui::edit_mode_step::g_frequencyEncoderMode;

        value = 1;
    } else if (operation == DATA_OPERATION_SET_ENCODER_MODE) {
        eez::psu::gui::edit_mode_step::g_frequencyEncoderMode = (EncoderMode)value.getInt();
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

        stepValues->encoderSettings.mode = eez::psu::gui::edit_mode_step::g_dutyEncoderMode;

        value = 1;
    } else if (operation == DATA_OPERATION_SET_ENCODER_MODE) {
        eez::psu::gui::edit_mode_step::g_dutyEncoderMode = (EncoderMode)value.getInt();
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
    if (getActivePageId() == PAGE_ID_MAIN) {
        hmi::selectSlot(getFoundWidgetAtDown().cursor);
    }
    pushPage(PAGE_ID_DIB_MIO168_INFO);
}

void action_dib_mio168_show_calibration() {
    Page *page = getActivePage();
    if (page && page->getDirty()) {
        int pageId = getActivePageId();
        ((SetPage *)getActivePage())->set();
        pushPage(pageId);
    }

    if (getPage(PAGE_ID_DIB_MIO168_AOUT_DAC7760_CONFIGURATION)) {
        hmi::g_selectedSubchannelIndex = g_aoutDac7760ConfigurationPage.g_selectedChannelIndex;
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

////////////////////////////////////////////////////////////////////////////////

static const int NUM_PAGES_IN_MAX_VIEW = 7;
static const int NUM_PAGES_IN_DEFAULT_VIEW = 7;

void data_dib_mio168_pager_list(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_COUNT) {
        value = NUM_PAGES_IN_MAX_VIEW;
    } else if (operation == DATA_OPERATION_GET_CURSOR_VALUE) {
        value = hmi::g_selectedSlotIndex * NUM_PAGES_IN_MAX_VIEW + value.getInt();
    }
}

void data_dib_mio168_pager_is_selected(DataOperationEnum operation, Cursor cursor, Value &value) {
	if (operation == DATA_OPERATION_GET) {
		auto module = (Mio168Module *)g_slots[cursor / NUM_PAGES_IN_MAX_VIEW];
		value = module->selectedPageInMaxView == cursor % NUM_PAGES_IN_MAX_VIEW ? 1 : 0;
	}
}

void data_dib_mio168_pager_selected_page(DataOperationEnum operation, Cursor cursor, Value &value) {
	if (operation == DATA_OPERATION_GET) {
		auto module = (Mio168Module *)g_slots[cursor];
		value = module->selectedPageInMaxView;
	}
}

void action_dib_mio168_pager_select_page() {
	auto cursor = getFoundWidgetAtDown().cursor;
	auto module = (Mio168Module *)g_slots[cursor / NUM_PAGES_IN_MAX_VIEW];
    if (cursor % NUM_PAGES_IN_MAX_VIEW != module->selectedPageInMaxView) {
        auto selectedPage = module->selectedPageInMaxView;
        module->selectedPageInMaxView = cursor % NUM_PAGES_IN_MAX_VIEW;
		module->selectedPageInDefaultView = module->selectedPageInMaxView;

        if (module->selectedPageInMaxView > selectedPage) {
            animateSlideLeftWithoutHeader();
        } else {
            animateSlideRightWithoutHeader();
        }
    }
}

void data_dib_mio168_is_settings_page_selected(DataOperationEnum operation, Cursor cursor, Value &value) {
	if (operation == DATA_OPERATION_GET) {
		auto module = (Mio168Module *)g_slots[cursor];
		value = module->selectedPageInMaxView == NUM_PAGES_IN_MAX_VIEW;
	}
}

void action_dib_mio168_show_slot_settings_in_max_view() {
    auto slotIndex = getFoundWidgetAtDown().cursor;
    hmi::selectSlot(slotIndex);
	auto module = (Mio168Module *)g_slots[slotIndex];
    module->selectedPageInMaxView = NUM_PAGES_IN_MAX_VIEW;
    animateSlideLeftWithoutHeader();
}

void data_dib_mio168_selected_page_in_default_view(DataOperationEnum operation, Cursor cursor, Value &value) {
	if (operation == DATA_OPERATION_GET) {
		auto module = (Mio168Module *)g_slots[cursor];
		value = module->selectedPageInDefaultView;
	}
}

void action_dib_mio168_select_page_in_default_view() {
	auto module = (Mio168Module *)g_slots[getFoundWidgetAtDown().cursor];
	module->selectedPageInDefaultView = (module->selectedPageInDefaultView + 1) % NUM_PAGES_IN_DEFAULT_VIEW;
	module->selectedPageInMaxView = module->selectedPageInDefaultView;
	if (isDefaultViewVertical()) {
		animateSlideLeftInDefaultViewVert(getFoundWidgetAtDown().cursor);
	} else {
		animateSlideLeftInDefaultViewHorz(getFoundWidgetAtDown().cursor);
	}
}

void data_dib_mio168_is_no_afe(DataOperationEnum operation, Cursor cursor, Value &value) {
	if (operation == DATA_OPERATION_GET) {
		int slotIndex;
		if (getActivePageId() == PAGE_ID_MAIN) {
			slotIndex = cursor;
		} else {
			slotIndex = hmi::g_selectedSlotIndex;
		}
		value = (int)((Mio168Module *)g_slots[slotIndex])->afeVersion == 4;
	}
}

void data_dib_mio168_ac_analysis_allowed(DataOperationEnum operation, Cursor cursor, Value &value) {
	if (operation == DATA_OPERATION_GET) {
		value = !(dlog_record::isExecuting() && dlog_record::isModuleAtSlotRecording(cursor));
	}
}

void data_dib_mio168_ac_analysis_enabled(DataOperationEnum operation, Cursor cursor, Value &value) {
	if (operation == DATA_OPERATION_GET) {
		auto module = (Mio168Module *)g_slots[cursor];
		value = module->acAnalysisEnabled && !(dlog_record::isExecuting() && dlog_record::isModuleAtSlotRecording(cursor));
	}
}

void action_dib_mio168_ain_toggle_ac_analysis() {
	auto slotIndex = getFoundWidgetAtDown().cursor;
    auto module = (Mio168Module *)g_slots[slotIndex];
    module->acAnalysisEnabled = !module->acAnalysisEnabled;
}

#if defined(EEZ_PLATFORM_SIMULATOR)

static int g_slotIndex;

void onSetSimulatorAfeVersion(uint16_t value) {
	g_frontPanelAppContext.popPage();

	auto afeVersion = (uint8_t)value;

	((Mio168Module *)g_slots[g_slotIndex])->simulatorAfeVersion = afeVersion;

	persist_conf::setAfeVersion(g_slotIndex, afeVersion);

#ifdef __EMSCRIPTEN__
	infoMessage("Reload page to apply change!");
#else
	infoMessage("Restart program to apply change!");
#endif
}

void data_dib_mio168_simulator_afe_version(DataOperationEnum operation, Cursor cursor, Value &value) {
	if (operation == DATA_OPERATION_GET) {
		value = (int)((Mio168Module *)g_slots[cursor])->simulatorAfeVersion - 1;
	}
}

void action_dib_mio168_select_simulator_afe_version() {
	g_slotIndex = getFoundWidgetAtDown().cursor;

	static EnumItem enumItems[] = {
		{ 1, "AFE1" },
		{ 2, "AFE2" },
		{ 3, "AFE3" },
		{ 4, "No AFE" },
		{ 0, 0 }
	};

	pushSelectFromEnumPage(&g_frontPanelAppContext, enumItems,
		((Mio168Module *)g_slots[g_slotIndex])->simulatorAfeVersion, NULL, onSetSimulatorAfeVersion);
}

#endif

} // namespace gui

} // namespace eez

