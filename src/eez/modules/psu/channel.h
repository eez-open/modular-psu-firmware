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

/// PSU channel.
class Channel {
    friend class DigitalAnalogConverter;
    friend struct calibration::Value;

  public:
    /// Binary flags for the channel calibration configuration.
    struct CalibrationConfigurationFlags {
        /// Is voltage calibrated?
        unsigned u_cal_params_exists : 1;
        /// Is current in range 0 (5A) calibrated?
        unsigned i_cal_params_exists_range_high : 1;
        /// Is current in range 1 (50mA) calibrated?
        unsigned i_cal_params_exists_range_low : 1;
    };

    /// Calibration parameters for the single point.
    struct CalibrationValuePointConfiguration {
        /// Value set on DAC by the calibration module.
        float dac;
        /// Real value, in volts, set by the user who reads it on the instrument (voltmeter and
        /// ampermeter).
        float val;
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
        /// Min point.
        CalibrationValuePointConfiguration min;
        /// Mid point.
        CalibrationValuePointConfiguration mid;
        /// Max point.
        CalibrationValuePointConfiguration max;

        /// Real min after calibration
        float minPossible;

        /// Real max after calibration
        float maxPossible;
    };

    /// A structure where calibration parameters for the channel are stored.
    struct CalibrationConfiguration {
        /// Used by the persist_conf.
        persist_conf::BlockHeader header;

        /// Flags
        CalibrationConfigurationFlags flags;

        /// Calibration parameters for the voltage.
        CalibrationValueConfiguration u;

        /// Calibration parameters for the currents in both ranges.
        CalibrationValueConfiguration i[2];

        /// Date when calibration is saved.
        /// Automatically set if RTC is present.
        /// Format is YYYYMMDD.
        char calibration_date[8 + 1];

        /// Remark about calibration set by user.
        char calibration_remark[CALIBRATION_REMARK_MAX_LENGTH + 1];
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
        unsigned historyStarted : 1;
        unsigned senseEnabled : 1;
        unsigned cvMode : 1;
        unsigned ccMode : 1;
        unsigned powerOk : 1;
        unsigned _calEnabled : 1;
        unsigned rprogEnabled : 1;
        unsigned reserved2 : 2;
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

    static Channel g_channels[CH_MAX];

    /// Get channel instance
    /// \param channel_index Zero based channel index, greater then or equal to 0 and less then
    /// CH_MAX. \returns Reference to channel.
    static inline Channel &get(int channelIndex) {
        return g_channels[channelIndex];
    } 

    static inline Channel &getBySlotIndex(uint8_t slotIndex, uint8_t subchannelIndex = 0) {
        return g_channels[g_slots[slotIndex].channelIndex + subchannelIndex];
    }

    /// Save and disable OE for all the channels.
    static void saveAndDisableOE();

    /// Restore previously saved OE state for all the channels.
    static void restoreOE();

    typedef float(*YtDataGetValueFunctionPointer)(uint32_t rowIndex, uint8_t columnIndex, float *max);

    static YtDataGetValueFunctionPointer getChannelHistoryValueFuncs(int channelIndex);

    // Slot index. Starts from 0.
    uint8_t slotIndex;

    /// Channel index. Starts from 0.
    uint8_t channelIndex;

    /// In case when module has multiple channels. Starts from 0
    uint8_t subchannelIndex;

    ChannelInterface *channelInterface;

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

#ifdef EEZ_PLATFORM_SIMULATOR
    Simulator simulator;
#endif // EEZ_PLATFORM_SIMULATOR

    void set(uint8_t slotIndex, uint8_t subchannelIndex);

    void setChannelIndex(uint8_t channelIndex);

    /// Initialize channel and underlying hardware.
    /// Makes a required tests, for example ADC, DAC and IO Expander tests.
    void init();

    /// Reset the channel to default values.
    void reset();

    /// Clear channel calibration configuration.
    void clearCalibrationConf();

    /// Test the channel.
    bool test();

    /// Is channel installed?
    bool isInstalled();

    /// Is channel power ok (state of PWRGOOD bit in IO Expander)?
    bool isPowerOk();

    TestResult getTestResult();

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

    /// Called when device power is turned off, so channel
    /// can do its own housekeeping.
    void onPowerDown();

    /// Force ADC to measure u.mon_dac and i.mon_dac.
    void adcMeasureMonDac();

    /// Force ADC to measure all values: u.mon, u.mon_dac, i.mon and i.mon_dac.
    void adcMeasureAll();

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

    /// Is channel calibrated, both voltage and current?
    bool isCalibrationExists();

    /// Is OVP, OCP or OPP tripped?
    bool isTripped();

    inline bool isHwOvpEnabled() {
        return prot_conf.flags.u_state && prot_conf.flags.u_type;
    }

    bool checkSwOvpCondition(float uProtectionLevel);

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

    bool isVoltageBalanced();
    bool isCurrentBalanced();
    float getUSetUnbalanced();
    float getISetUnbalanced();

    uint32_t getCurrentHistoryValuePosition();

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

    void enterOvpProtection();

    void setDprogState(DprogState dprogState);

    void getFirmwareVersion(uint8_t &majorVersion, uint8_t &minorVersion);

  private:
    bool delayLowRippleCheck;
    uint32_t outputEnableStartTime;
    
    MaxCurrentLimitCause maxCurrentLimitCause;

    float uHistory[CHANNEL_HISTORY_SIZE];
    float iHistory[CHANNEL_HISTORY_SIZE];
    uint32_t historyPosition;
    uint32_t historyLastTick;

    int reg_get_ques_isum_bit_mask_for_channel_protection_value(ProtectionValue &cpv);

    static float getChannel0HistoryValue(uint32_t rowIndex, uint8_t columnIndex, float *max);
    static float getChannel1HistoryValue(uint32_t rowIndex, uint8_t columnIndex, float *max);
    static float getChannel2HistoryValue(uint32_t rowIndex, uint8_t columnIndex, float *max);
    static float getChannel3HistoryValue(uint32_t rowIndex, uint8_t columnIndex, float *max);
    static float getChannel4HistoryValue(uint32_t rowIndex, uint8_t columnIndex, float *max);
    static float getChannel5HistoryValue(uint32_t rowIndex, uint8_t columnIndex, float *max);

    void clearProtectionConf();
    void protectionEnter(ProtectionValue &cpv);
    void protectionCheck(ProtectionValue &cpv);
    void protectionCheck();

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

    void doRemoteSensingEnable(bool enable);
    void doRemoteProgrammingEnable(bool enable);

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

} // namespace psu
} // namespace eez
