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
#include <stdlib.h>

#include <eez/firmware.h>
#include <eez/unit.h>

namespace eez {

namespace psu {
    struct Channel;
    namespace profile {
        class WriteContext;
        class ReadContext;
        struct List;
    }
}

namespace gui {
    class Page;
}

static const uint16_t MODULE_TYPE_NONE = 0;
static const uint16_t MODULE_TYPE_DCP405 = 405;
static const uint16_t MODULE_TYPE_DCM220 = 220;
static const uint16_t MODULE_TYPE_DCM224 = 224;
static const uint16_t MODULE_TYPE_DIB_MIO168 = 168;
static const uint16_t MODULE_TYPE_DIB_PREL6 = 6;
static const uint16_t MODULE_TYPE_DIB_SMX46 = 46;

enum SlotViewType {
    SLOT_VIEW_TYPE_DEFAULT,
    SLOT_VIEW_TYPE_DEFAULT_2COL,
    SLOT_VIEW_TYPE_MAX,
    SLOT_VIEW_TYPE_MIN,
    SLOT_VIEW_TYPE_MICRO,
};

enum FlashMethod {
    FLASH_METHOD_NONE,
    FLASH_METHOD_STM32_BOOTLOADER_UART,
    FLASH_METHOD_STM32_BOOTLOADER_SPI
};

enum SourceMode {
    SOURCE_MODE_CURRENT,
    SOURCE_MODE_VOLTAGE
};

enum MeasureMode {
    MEASURE_MODE_CURRENT,
    MEASURE_MODE_VOLTAGE,
    MEASURE_MODE_OPEN
};

static const int MAX_NUM_CH_IN_CH_LIST = 32;

struct SlotAndSubchannelIndex {
    uint8_t slotIndex;
    uint8_t subchannelIndex;
};

struct ChannelList {
    int numChannels;
    SlotAndSubchannelIndex channels[MAX_NUM_CH_IN_CH_LIST];
};

enum CalibrationValueType {
    CALIBRATION_VALUE_U,
    CALIBRATION_VALUE_I_HI_RANGE,
    CALIBRATION_VALUE_I_LOW_RANGE
};

/// Header of the every block stored in EEPROM. It contains checksum and version.
struct BlockHeader {
    uint32_t checksum;
    uint16_t version;
};

/// Calibration parameters for the single point.
struct CalibrationValuePointConfiguration {
    /// Value set on DAC by the calibration module.
    float dac;
    /// Real value, in volts, set by the user who reads it on the instrument (voltmeter and ampermeter).
    float value;
    /// Value read from ADC.
    float adc;
};

#define MAX_CALIBRATION_POINTS 20
/// Calibration remark maximum length.
#define CALIBRATION_REMARK_MAX_LENGTH 32

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
    BlockHeader header;

    /// Calibration parameters for the voltage.
    CalibrationValueConfiguration u;

    /// Calibration parameters for the currents in both ranges.
    CalibrationValueConfiguration i[2];

    /// Date when calibration is saved.
    uint32_t calibrationDate;

    /// Remark about calibration set by user.
    char calibrationRemark[CALIBRATION_REMARK_MAX_LENGTH + 1];
};

struct StepValues {
    int count;
    const float *values;
    Unit unit;
    struct {
        bool accelerationEnabled;
        float range;
        float step;
    } encoderSettings;
};

struct Module {
    uint16_t moduleType;
    const char *moduleName;
    const char *moduleBrand;
    uint16_t latestModuleRevision;
    FlashMethod flashMethod;
    uint32_t spiBaudRatePrescaler;
    bool spiCrcCalculationEnable;
    uint8_t numPowerChannels;
    uint8_t numOtherChannels;

    uint8_t slotIndex = -1;
    bool enabled;
    uint16_t moduleRevision = 0;
    bool firmwareInstalled = false;
	uint8_t firmwareMajorVersion = 0;
	uint8_t firmwareMinorVersion = 0;
	uint32_t idw0 = 0;
	uint32_t idw1 = 0;
	uint32_t idw2 = 0;

    virtual void setEnabled(bool value);

    virtual Module *createModule() = 0;

    virtual TestResult getTestResult();

    virtual void boot();
    virtual psu::Channel *createPowerChannel(int slotIndex, int channelIndex, int subchannelIndex);
    virtual void initChannels();
    virtual void tick();
    virtual void writeUnsavedData();
    virtual void onPowerDown();
    virtual void onSpiIrq();
    virtual void onSpiDmaTransferCompleted(int status);
    virtual gui::Page *getPageFromId(int pageId);
    virtual void animatePageAppearance(int previousPageId, int activePageId);
    virtual int getSlotView(SlotViewType slotViewType, int slotIndex, int cursor);
    virtual int getChannelSettingsPageId();
    virtual int getSlotSettingsPageId();

    virtual void onLowPriorityThreadMessage(uint8_t type, uint32_t param);
    virtual void onHighPriorityThreadMessage(uint8_t type, uint32_t param);

    virtual void resetPowerChannelProfileToDefaults(int channelIndex, uint8_t *buffer);
    virtual void getPowerChannelProfileParameters(int channelIndex, uint8_t *buffer);
    virtual void setPowerChannelProfileParameters(int channelIndex, uint8_t *buffer, bool mismatch, int recallOptions, int &numTrackingChannels);
    virtual bool writePowerChannelProfileProperties(psu::profile::WriteContext &ctx, const uint8_t *buffer);
    virtual bool readPowerChannelProfileProperties(psu::profile::ReadContext &ctx, uint8_t *buffer);
    virtual bool getProfileOutputEnable(uint8_t *buffer);
    virtual float getProfileUSet(uint8_t *buffer);
    virtual float getProfileISet(uint8_t *buffer);

    virtual void resetProfileToDefaults(uint8_t *buffer);
    virtual void getProfileParameters(uint8_t *buffer);
    virtual void setProfileParameters(uint8_t *buffer, bool mismatch, int recallOptions);
    virtual bool writeProfileProperties(psu::profile::WriteContext &ctx, const uint8_t *buffer);
    virtual bool readProfileProperties(psu::profile::ReadContext &ctx, uint8_t *buffer);

    virtual void resetConfiguration();

    int getNumSubchannels();
    virtual bool isValidSubchannelIndex(int subchannelIndex);
    virtual int getSubchannelIndexFromRelativeChannelIndex(int relativeChannelIndex);

    virtual bool getDigitalInputData(int subchannelIndex, uint8_t &data, int *err);

    virtual bool getDigitalInputRange(int subchannelIndex, uint8_t pin, uint8_t &range, int *err);
    virtual bool setDigitalInputRange(int subchannelIndex, uint8_t pin, uint8_t range, int *err);

    virtual bool getDigitalInputSpeed(int subchannelIndex, uint8_t pin, uint8_t &speed, int *err);
    virtual bool setDigitalInputSpeed(int subchannelIndex, uint8_t pin, uint8_t speed, int *err);

    virtual bool getDigitalOutputData(int subchannelIndex, uint8_t &data, int *err);
    virtual bool setDigitalOutputData(int subchannelIndex, uint8_t data, int *err);

    virtual bool getMode(int subchannelIndex, SourceMode &mode, int *err);
    virtual bool setMode(int subchannelIndex, SourceMode mode, int *err);

    virtual bool getCurrentRange(int subchannelIndex, uint8_t &range, int *err);
    virtual bool setCurrentRange(int subchannelIndex, uint8_t range, int *err);

    virtual bool getVoltageRange(int subchannelIndex, uint8_t &range, int *err);
    virtual bool setVoltageRange(int subchannelIndex, uint8_t range, int *err);

    virtual bool getMeasureMode(int subchannelIndex, MeasureMode &mode, int *err);
    virtual bool setMeasureMode(int subchannelIndex, MeasureMode mode, int *err);

    virtual bool getMeasureRange(int subchannelIndex, uint8_t &range, int *err);
    virtual bool setMeasureRange(int subchannelIndex, uint8_t range, int *err);

    virtual bool getMeasureTempSensorBias(int subchannelIndex, bool &enabled, int *err);
    virtual bool setMeasureTempSensorBias(int subchannelIndex, bool enabled, int *err);

    virtual bool isRouteOpen(int subchannelIndex, bool &isRouteOpen, int *err);
    virtual bool routeOpen(ChannelList channelList, int *err);
    virtual bool routeClose(ChannelList channelList, int *err);

    virtual bool getSwitchMatrixNumRows(int &numRows, int *err);
    virtual bool getSwitchMatrixNumColumns(int &numColumns, int *err);
    static const int MAX_SWITCH_MATRIX_LABEL_LENGTH = 5;
    virtual bool setSwitchMatrixRowLabel(int rowIndex, const char *label, int *err);
    virtual bool getSwitchMatrixRowLabel(int columnIndex, char *label, int *err);
    virtual bool setSwitchMatrixColumnLabel(int rowIndex, const char *label, int *err);
    virtual bool getSwitchMatrixColumnLabel(int columnIndex, char *label, int *err);

    virtual bool getRelayCycles(int subchannelIndex, uint32_t &relayCycles, int *err);
    
    virtual bool outputEnable(int subchannelIndex, bool enable, int *err);
    virtual bool isOutputEnabled(int subchannelIndex, bool &enabled, int *err);

    virtual bool getVoltage(int subchannelIndex, float &value, int *err);
    virtual bool setVoltage(int subchannelIndex, float value, int *err);
    virtual bool getMeasuredVoltage(int subchannelIndex, float &value, int *err);
    virtual void getVoltageStepValues(int subchannelIndex, StepValues *stepValues, bool calibrationMode);
    virtual float getVoltageResolution(int subchannelIndex);
    virtual float getVoltageMinValue(int subchannelIndex);
    virtual float getVoltageMaxValue(int subchannelIndex);
    virtual bool isConstantVoltageMode(int subchannelIndex);
    virtual bool isVoltageCalibrationExists(int subchannelIndex);
    virtual bool isVoltageCalibrationEnabled(int subchannelIndex);
    virtual void enableVoltageCalibration(int subchannelIndex, bool enable);

    virtual bool getCurrent(int subchannelIndex, float &value, int *err);
    virtual bool setCurrent(int subchannelIndex, float value, int *err);
    virtual bool getMeasuredCurrent(int subchannelIndex, float &value, int *err);
    virtual void getCurrentStepValues(int subchannelIndex, StepValues *stepValues, bool calibrationMode);
    virtual float getCurrentResolution(int subchannelIndex);
    virtual float getCurrentMinValue(int subchannelIndex);
    virtual float getCurrentMaxValue(int subchannelIndex);
    virtual bool isCurrentCalibrationExists(int subchannelIndex);
    virtual bool isCurrentCalibrationEnabled(int subchannelIndex);
    virtual void enableCurrentCalibration(int subchannelIndex, bool enable);

    virtual bool loadChannelCalibration(int subchannelIndex, int *err);
    virtual bool saveChannelCalibration(int subchannelIndex, int *err);
    virtual void startChannelCalibration(int subchannelIndex);
    virtual void stopChannelCalibration(int subchannelIndex);
    virtual unsigned int getMaxCalibrationPoints(int m_subchannelIndex);
    virtual CalibrationValueType getCalibrationValueType(int subchannelIndex);
    virtual void getDefaultCalibrationPoints(int subchannelIndex, CalibrationValueType type, unsigned int &numPoints, float *&points);
    virtual bool getCalibrationConfiguration(int subchannelIndex, CalibrationConfiguration &calConf, int *err);
    virtual bool setCalibrationConfiguration(int subchannelIndex, const CalibrationConfiguration &calConf, int *err);
    virtual bool getCalibrationRemark(int subchannelIndex, const char *&calibrationRemark, int *err);
    virtual bool getCalibrationDate(int subchannelIndex, uint32_t &calibrationDate, int *err);
};

static const int NUM_SLOTS = 3;
extern Module *g_slots[NUM_SLOTS];

Module *getModule(uint16_t moduleType);

void getModuleSerialInfo(uint8_t slotIndex, char *serialStr);
bool setModuleSerialInfo(uint8_t slotIndex, const char *serialStr, size_t serialStrLen, int *err);

extern bool g_isCol2Mode;
extern int g_slotIndexes[NUM_SLOTS];

} // namespace eez
