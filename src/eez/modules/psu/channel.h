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

#include <math.h>

#include <eez/modules/psu/persist_conf.h>
#include <eez/modules/psu/temp_sensor.h>

#define IS_OVP_VALUE(channel, cpv) (&cpv == &channel->ovp)
#define IS_OCP_VALUE(channel, cpv) (&cpv == &channel->ocp)
#define IS_OPP_VALUE(channel, cpv) (&cpv == &channel->opp)

namespace eez {

struct StepValues;
   
namespace psu {

enum MaxCurrentLimitCause {
    MAX_CURRENT_LIMIT_CAUSE_NONE,
    MAX_CURRENT_LIMIT_CAUSE_FAN,
    MAX_CURRENT_LIMIT_CAUSE_TEMPERATURE
};

extern int CH_NUM;

namespace calibration {
struct Value;
}

enum DisplayValue { DISPLAY_VALUE_VOLTAGE, DISPLAY_VALUE_CURRENT, DISPLAY_VALUE_POWER };

enum TriggerMode { TRIGGER_MODE_FIXED, TRIGGER_MODE_LIST, TRIGGER_MODE_STEP };

enum TriggerOnListStop {
    TRIGGER_ON_LIST_STOP_OUTPUT_OFF,
    TRIGGER_ON_LIST_STOP_SET_TO_FIRST_STEP,
    TRIGGER_ON_LIST_STOP_SET_TO_LAST_STEP,
    TRIGGER_ON_LIST_STOP_STANDBY
};

enum CurrentRangeSelectionMode {
    CURRENT_RANGE_SELECTION_USE_BOTH,
    CURRENT_RANGE_SELECTION_ALWAYS_HIGH,
    CURRENT_RANGE_SELECTION_ALWAYS_LOW
};

enum CurrentRange { CURRENT_RANGE_HIGH, CURRENT_RANGE_LOW };

enum VoltageProtectionType {
    VOLTAGE_PROTECTION_TYPE_HW = 1,
    VOLTAGE_PROTECTION_TYPE_SW = 0
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
    CH_FEATURE_COUPLING = (1 << 9),
    CH_FEATURE_DINPUT = (1 << 10),
    CH_FEATURE_DOUTPUT = (1 << 11)
};

struct ChannelParams {
    float U_MIN;
    float U_DEF;
    float U_MAX;
    
    float U_MIN_STEP;
    float U_DEF_STEP;
    float U_MAX_STEP;

    unsigned int U_CAL_NUM_POINTS;
    float *U_CAL_POINTS;
    float U_CAL_I_SET;

    float I_MIN;
    float I_DEF;
    float I_MAX;
    
    float I_MON_MIN;

    float I_MIN_STEP;
    float I_DEF_STEP;
    float I_MAX_STEP; 
    
    unsigned int I_CAL_NUM_POINTS;
    float *I_CAL_POINTS;
    float I_CAL_U_SET;

    unsigned int I_LOW_RANGE_CAL_NUM_POINTS;
    float *I_LOW_RANGE_CAL_POINTS;
    float I_LOW_RANGE_CAL_U_SET;

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
    float U_RESOLUTION_DURING_CALIBRATION;
    float I_RESOLUTION;
    float I_RESOLUTION_DURING_CALIBRATION;
    float I_LOW_RESOLUTION;
    float I_LOW_RESOLUTION_DURING_CALIBRATION;
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

    uint32_t DAC_MAX;
    uint32_t ADC_MAX;

    float U_RAMP_DURATION_MIN_VALUE;
};

/// Runtime protection binary flags (alarmed, tripped)
struct ProtectionFlags {
    unsigned alarmed : 1;
    unsigned tripped : 1;
};

/// Runtime protection values
struct ProtectionValue {
    ProtectionFlags flags;
    uint32_t alarm_started;
};

enum ChannelMode {
    CHANNEL_MODE_UR,
    CHANNEL_MODE_CC,
    CHANNEL_MODE_CV
};

static const float OUTPUT_DELAY_DURATION_MIN_VALUE = 0.001f;
static const float OUTPUT_DELAY_DURATION_MAX_VALUE = 10.0f;
static const float OUTPUT_DELAY_DURATION_DEF_VALUE = 0.01f;
static const float OUTPUT_DELAY_DURATION_PREC = 0.001f;

static const float RAMP_DURATION_MIN_VALUE = 0.002f;
static const float RAMP_DURATION_MAX_VALUE = 10.0f;
static const float RAMP_DURATION_DEF_VALUE_U = 0.01f;
static const float RAMP_DURATION_DEF_VALUE_I = 0;
static const float RAMP_DURATION_PREC = 0.001f;

typedef float(*YtDataGetValueFunctionPointer)(uint32_t rowIndex, uint8_t columnIndex, float *max);

struct ChannelHistory {
    friend struct Channel;

    ChannelHistory(Channel& channel_) : channel(channel_) {}

    void reset();
    void update(uint32_t tickCount);

    static YtDataGetValueFunctionPointer getChannelHistoryValueFuncs(int channelIndex);

protected:
    bool historyStarted;
    float uHistory[CHANNEL_HISTORY_SIZE];
    float iHistory[CHANNEL_HISTORY_SIZE];
    uint32_t historyPosition;
    uint32_t historyLastTick;

private: 
    Channel& channel;

    static float getChannel0HistoryValue(uint32_t rowIndex, uint8_t columnIndex, float *max);
    static float getChannel1HistoryValue(uint32_t rowIndex, uint8_t columnIndex, float *max);
    static float getChannel2HistoryValue(uint32_t rowIndex, uint8_t columnIndex, float *max);
    static float getChannel3HistoryValue(uint32_t rowIndex, uint8_t columnIndex, float *max);
    static float getChannel4HistoryValue(uint32_t rowIndex, uint8_t columnIndex, float *max);
    static float getChannel5HistoryValue(uint32_t rowIndex, uint8_t columnIndex, float *max);
};

/// PSU channel.
struct Channel {
    friend class DigitalAnalogConverter;
    friend struct calibration::Value;
    friend struct ChannelHistory;

public:
    /// Calibration parameters for the single point.
    struct CalibrationValuePointConfiguration {
        /// Value set on DAC by the calibration module.
        float dac;
        /// Real value, in volts, set by the user who reads it on the instrument (voltmeter and ampermeter).
        float value;
        /// Value read from ADC.
        float adc;
    };

    /// Calibration parameters for the voltage and current.
    /// There are three points defined: `min`, `mid` and `max`.
    /// Only `min` and `max` are used in actual calculations -
    /// `mid` is only used for the validity checking.
    /// Here is how `DAC` value is calculated from the `real_value` set by user:
    /// `DAC = min.dac + (real_value - min.val) * (max.dac - min.dac) / (max.val - min.val);`
    /// And here is how `real_value` is calculated from the `ADC` value:
    /// `real_value = min.val + (ADC - min.adc) * (max.val - min.val) / (max.adc - min.adc);`
    struct CalibrationValueConfiguration {
        unsigned int numPoints;
        CalibrationValuePointConfiguration points[MAX_CALIBRATION_POINTS];
    };

    /// A structure where calibration parameters for the channel are stored.
    struct CalibrationConfiguration {
        /// Used by the persist_conf.
        persist_conf::BlockHeader header;

        /// Calibration parameters for the voltage.
        CalibrationValueConfiguration u;

        /// Calibration parameters for the currents in both ranges.
        CalibrationValueConfiguration i[2];

        /// Date when calibration is saved.
        uint32_t calibrationDate;

        /// Remark about calibration set by user.
        char calibrationRemark[CALIBRATION_REMARK_MAX_LENGTH + 1];
    };

    /// Binary flags for the channel protection configuration
    struct ProtectionConfigurationFlags {
        /// Is OVP enabled?
        unsigned u_state : 1;
        /// Is OCP enabled?
        unsigned i_state : 1;
        /// Is OPP enabled?
        unsigned p_state : 1;
        /// HW(1) or SW(0)?
        unsigned u_type : 1;
        unsigned u_hwOvpDeactivated : 1;
    };

    /// Channel OVP, OVP and OPP configuration parameters like level and delay.
    struct ChannelProtectionConfiguration {
        persist_conf::BlockHeader header;

        ProtectionConfigurationFlags flags;

        float u_delay;
        float u_level;
        float i_delay;
        float p_delay;
        float p_level;
    };

    /// Channel binary flags like output enabled, sense enabled, ...
    struct Flags {
        unsigned outputEnabled : 1;
        unsigned doOutputEnableOnNextSync: 1;
        unsigned outputEnabledValueOnNextSync: 1;
        unsigned reserved1 : 1;
        unsigned senseEnabled : 1;
        unsigned cvMode : 1;
        unsigned ccMode : 1;
        unsigned powerOk : 1;
        unsigned calEnabled : 1;
        unsigned rprogEnabled : 1;
        unsigned reserved2 : 1;
        unsigned rpol : 1; // remote sense reverse polarity is detected
        unsigned displayValue1 : 2;
        unsigned displayValue2 : 2;
        unsigned voltageTriggerMode : 2;
        unsigned currentTriggerMode : 2;
        unsigned triggerOutputState : 1;
        unsigned triggerOnListStop : 3;
        unsigned currentRangeSelectionMode : 2; // see enum CurrentRangeSelectionMode
        unsigned autoSelectCurrentRange : 1;    // switch between 5A/50mA depending on Imon
        unsigned currentCurrentRange : 1;       // 0: 5A, 1:50mA
        unsigned trackingEnabled : 1;
        unsigned dprogState: 2;
    };

    /// Voltage and current data set and measured during runtime.
    struct Value {
        float set;

        bool mon_measured;

        float mon_adc; // uncalibrated

        float mon_last; // calibrated, latest measurement

        float mon; // calibrated, average value

        // used for calculating average value
        float mon_prev;
        int8_t mon_index;
        float mon_arr[NUM_ADC_AVERAGING_VALUES];
        float mon_total;

        float mon_dac;
        float mon_dac_prev;
        float mon_dac_last;
        int8_t mon_dac_index;
        float mon_dac_arr[NUM_ADC_AVERAGING_VALUES];
        float mon_dac_total;

        float step;
        float limit;

        float min;
        float def;
        float max;

        float triggerLevel;
        float rampDuration;

        void init(float set_, float step_, float limit_);
        void resetMonValues();
        void addMonDacValue(float value, float precision);
        void addMonValue(float value, float precision);
    };

#ifdef EEZ_PLATFORM_SIMULATOR
    /// Per channel simulator data
    struct Simulator {
        bool oe;
        bool load_enabled;
        float load;
        float u_set;
        float u_dac;
        float i_set;
        float i_dac;
        float temperature[temp_sensor::NUM_TEMP_SENSORS];
        float voltProgExt;

        void setLoadEnabled(bool value);
        bool getLoadEnabled();

        void setLoad(float value);
        float getLoad();

        void setVoltProgExt(float value);
        float getVoltProgExt();
    };
#endif // EEZ_PLATFORM_SIMULATOR

    static Channel *g_channels[CH_MAX];
    static int8_t g_slotIndexToChannelIndex[NUM_SLOTS];

    /// Get channel instance
    /// \param channel_index Zero based channel index, greater then or equal to 0 and less then
    /// CH_MAX. \returns Reference to channel.
    static inline Channel &get(int channelIndex) {
        return *g_channels[channelIndex];
    } 

    static inline Channel *getBySlotIndex(uint8_t slotIndex, uint8_t subchannelIndex = 0) {
        int channelIndex = g_slotIndexToChannelIndex[slotIndex];
        return channelIndex != -1 ? g_channels[channelIndex + subchannelIndex] : nullptr;
    }

    /// Save and disable OE for all the channels.
    static void saveAndDisableOE();

    /// Restore previously saved OE state for all the channels.
    static void restoreOE();

    // Slot index. Starts from 0.
    uint8_t slotIndex;

    /// Channel index. Starts from 0.
    uint8_t channelIndex;

    /// In case when module has multiple channels. Starts from 0
    uint8_t subchannelIndex;

    ChannelParams params;

    Flags flags;

    Value u;
    Value i;

    float p_limit;

    CalibrationConfiguration cal_conf;
    ChannelProtectionConfiguration prot_conf;

    ProtectionValue ovp;
    ProtectionValue ocp;
    ProtectionValue opp;

    float ytViewRate;

    float outputDelayDuration;

#ifdef EEZ_PLATFORM_SIMULATOR
    Simulator simulator;
#endif // EEZ_PLATFORM_SIMULATOR

    Channel(uint8_t slotIndex, uint8_t channelIndex, uint8_t subchannelIndex);

    void initParams(uint16_t moduleRevision);

    /// Clear channel calibration configuration.
    void clearCalibrationConf();

    /// Is channel power ok (state of PWRGOOD bit in IO Expander)?
    bool isPowerOk();

    /// Is channel test failed?
    bool isTestFailed();

    /// Is channel test ok?
    bool isTestOk();

    /// Is channel ready to work with?
    bool isOk();

    /// Called by main loop, used for channel maintenance.
    void tick(uint32_t tick_usec);

    /// Called from channel driver when ADC data is ready.
    void onAdcData(AdcDataType adcDataType, float value);

    static void updateAllChannels();

    /// Is channel output enabled?
    bool isOutputEnabled();

    static void syncOutputEnable();

    /// Enable/disable channel output on all channels depending on inhibited state
    static void onInhibitedChanged(bool inhibited);

    /// Enable/disable channel calibration.
    void calibrationEnable(bool enabled);
    void calibrationEnableNoEvent(bool enabled);

    /// Is channel calibration enabled?
    bool isCalibrationEnabled();

    /// Enable/disable remote sensing.
    void remoteSensingEnable(bool enable);

    /// Is remote sensing enabled?
    bool isRemoteSensingEnabled();

    /// Enable/disable remote programming.
    void remoteProgrammingEnable(bool enable);

    /// Is remote programming enabled?
    bool isRemoteProgrammingEnabled();

    float getCalibratedVoltage(float value);

    /// Set channel voltage level.
    void setVoltage(float voltage);

    /// Set channel current level
    void setCurrent(float current);

    /// Is channel calibrated, voltage or current?
    bool isCalibrationExists();

    bool isVoltageCalibrationExists();
    bool isCurrentCalibrationExists(uint8_t currentRange);

    /// Is OVP, OCP or OPP tripped?
    bool isTripped();

    /// Clear channel protection tripp state.
    void clearProtection(bool clearOTP = true);

    /// Disable protection for this channel
    void disableProtection();

    /// Turn on/off bit in SCPI Questinable Instrument Isummary register for this channel.
    void setQuesBits(int bit_mask, bool on);

    /// Turn on/off bit in SCPI Operational Instrument Isummary register for this channel.
    void setOperBits(int bit_mask, bool on);

    /// Is channel in CV (constant voltage) mode?
    bool isCvMode() const {
        return flags.cvMode && !flags.ccMode;
    }

    /// Is channel in CC (constant current) mode?
    bool isCcMode() const {
        return flags.ccMode && !flags.cvMode;
    }

    /// Returns "CC", "CV" or "UR"
    ChannelMode getMode() const;
    const char *getModeStr() const;

    /// Returns currently set voltage limit
    float getVoltageLimit() const;

    /// Returns max. voltage limit
    float getVoltageMaxLimit() const;

    /// Change voltage limit, it will adjust U_SET if necessary.
    void setVoltageLimit(float limit);

    /// Returns currently set current limit
    float getCurrentLimit() const;

    /// Change current limit, it will adjust I_SET if necessary.
    void setCurrentLimit(float limit);

    /// Returns ERR_MAX_CURRENT if max. current is limited or i.max if not
    float getMaxCurrentLimit() const;

    /// Returns true if max current is limited to ERR_MAX_CURRENT.
    bool isMaxCurrentLimited() const;

    /// Returns max. current limit cause
    MaxCurrentLimitCause getMaxCurrentLimitCause() const;

    /// Set current max. limit to ERR_MAX_CURRENT
    void limitMaxCurrent(MaxCurrentLimitCause cause);

    /// Unset current max. limit
    void unlimitMaxCurrent();

    /// Returns currently set power limit
    float getPowerLimit() const;

    /// Returns max. power limit
    float getPowerMaxLimit() const;

    /// Change power limit, it will adjust U_SET or I_SET if necessary.
    void setPowerLimit(float limit);

    bool isVoltageWithinRange(float u);
    bool isVoltageLimitExceeded(float u);

    bool isCurrentWithinRange(float i);
    bool isCurrentLimitExceeded(float i);

    uint32_t getCurrentHistoryValuePosition();

    static void resetHistoryForAllChannels();
    void resetHistory();

    TriggerMode getVoltageTriggerMode();
    void setVoltageTriggerMode(TriggerMode mode);

    TriggerMode getCurrentTriggerMode();
    void setCurrentTriggerMode(TriggerMode mode);

    bool getTriggerOutputState();
    void setTriggerOutputState(bool enabled);

    TriggerOnListStop getTriggerOnListStop();
    void setTriggerOnListStop(TriggerOnListStop value);

    bool hasSupportForCurrentDualRange() const;
    void setCurrentRangeSelectionMode(CurrentRangeSelectionMode mode);
    CurrentRangeSelectionMode getCurrentRangeSelectionMode() {
        return (CurrentRangeSelectionMode)flags.currentRangeSelectionMode;
    }
    void enableAutoSelectCurrentRange(bool enable);
    bool isAutoSelectCurrentRangeEnabled() {
        return flags.autoSelectCurrentRange ? true : false;
    }
    float getDualRangeMax();
    void setCurrentRange(uint8_t currentRange);

    float getVoltageResolution() const;
    float getCurrentResolution(float value = NAN) const;
    float getPowerResolution() const;

    bool isMicroAmperAllowed() const;
    bool isAmperAllowed() const;

    float getValuePrecision(Unit unit, float value) const;

    float roundChannelValue(Unit unit, float value) const;

    void doSetVoltage(float value);
    void doSetCurrent(float value);

    float getDualRangeGndOffset();

    //
    // VIRTUAL METHODS
    //

    virtual void getParams(uint16_t moduleRevision) = 0;

    /// Initialize channel and underlying hardware.
    /// Makes a required tests, for example ADC, DAC and IO Expander tests.
    virtual void init() = 0;

    /// Called when device power is turned off, so channel
    /// can do its own housekeeping.
    virtual void onPowerDown();

    /// Reset the channel to default values.
    virtual void reset();

    /// Test the channel.
    virtual bool test() = 0;

    virtual void tickSpecific(uint32_t tickCount) = 0;

    virtual TestResult getTestResult() = 0;

    virtual unsigned getRPol();

    virtual bool isInCcMode() = 0;
    virtual bool isInCvMode() = 0;

    virtual void adcMeasureUMon() = 0;
    virtual void adcMeasureIMon() = 0;
    
    /// Force ADC to measure u.mon_dac and i.mon_dac.
    virtual void adcMeasureMonDac() = 0;
    
    /// Force ADC to measure all values: u.mon, u.mon_dac, i.mon and i.mon_dac.
    virtual void adcMeasureAll() = 0;

    virtual void setOutputEnable(bool enable, uint16_t tasks) = 0;

    virtual void setDprogState(DprogState dprogState);

    virtual void setDacVoltage(uint16_t value) = 0;
    virtual void setDacVoltageFloat(float value) = 0;
    virtual void setDacCurrent(uint16_t value) = 0;
    virtual void setDacCurrentFloat(float value) = 0;

    virtual bool isDacTesting() = 0;

    virtual void setRemoteSense(bool enable);
    virtual void setRemoteProgramming(bool enable);

    virtual void doSetCurrentRange();

    virtual bool isVoltageBalanced();
    virtual bool isCurrentBalanced();
    virtual float getUSetUnbalanced();
    virtual float getISetUnbalanced();

    virtual void readAllRegisters(uint8_t ioexpRegisters[], uint8_t adcRegisters[]);

    virtual void getVoltageStepValues(StepValues *stepValues, bool calibrationMode) = 0;
    virtual void getCurrentStepValues(StepValues *stepValues, bool calibrationMode) = 0;
    virtual void getPowerStepValues(StepValues *stepValues) = 0;

    virtual bool isPowerLimitExceeded(float u, float i, int *err) = 0;

#if defined(DEBUG) && defined(EEZ_PLATFORM_STM32)
    virtual int getIoExpBitDirection(int io_bit);
    virtual bool testIoExpBit(int io_bit);
    virtual void changeIoExpBit(int io_bit, bool set);
#endif

    virtual float readTemperature() = 0;

    virtual uint8_t getDigitalInputData();
    
    virtual uint8_t getDigitalOutputData();
    virtual void setDigitalOutputData(uint8_t data);

    //
    //
    //

protected:
    void doRemoteSensingEnable(bool enable);
    void doRemoteProgrammingEnable(bool enable);
    void protectionEnter(ProtectionValue &cpv, bool hwOvp);

    ChannelHistory *channelHistory;

private:
    bool delayLowRippleCheck;
    uint32_t outputEnableStartTime;
    
    MaxCurrentLimitCause maxCurrentLimitCause;

    int reg_get_ques_isum_bit_mask_for_channel_protection_value(ProtectionValue &cpv);

    void clearProtectionConf();
    void protectionCheck(ProtectionValue &cpv);
    void protectionCheck();

    bool checkSwOvpCondition();
    float getSwOvpProtectionLevel();

    void doCalibrationEnable(bool enable);
    bool isVoltageCalibrationEnabled();
    bool isCurrentCalibrationEnabled();

    void addUMonAdcValue(float value);
    void addIMonAdcValue(float value);
    void addUMonDacAdcValue(float value);
    void addIMonDacAdcValue(float value);

    void setCcMode(bool cc_mode);
    void setCvMode(bool cv_mode);

    void executeOutputEnable(bool enable, uint16_t tasks);
    static void executeOutputEnable(bool inhibited);

    uint32_t autoRangeCheckLastTickCount;
    void doAutoSelectCurrentRange(uint32_t tickCount);
};

#define OUTPUT_ENABLE_TASK_OE            (1 << 0)
#define OUTPUT_ENABLE_TASK_DAC           (1 << 1)
#define OUTPUT_ENABLE_TASK_CURRENT_RANGE (1 << 2)
#define OUTPUT_ENABLE_TASK_OVP           (1 << 3)
#define OUTPUT_ENABLE_TASK_DP            (1 << 4)
#define OUTPUT_ENABLE_TASK_ADC_START     (1 << 5)
#define OUTPUT_ENABLE_TASK_FINALIZE      (1 << 6)

extern int g_errorChannelIndex;

} // namespace psu
} // namespace eez
