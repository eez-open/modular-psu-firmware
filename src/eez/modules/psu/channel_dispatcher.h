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

#include <eez/modules/dib-dcp405/dac.h>
#include <eez/modules/dib-dcp405/ioexp.h>
#include <eez/modules/psu/persist_conf.h>
#include <eez/modules/psu/temp_sensor.h>
#include <eez/modules/dib-dcp405/adc.h>

namespace eez {

namespace gui {
struct WidgetCursor;
}

namespace psu {
namespace channel_dispatcher {

enum CouplingType { COUPLING_TYPE_NONE, COUPLING_TYPE_PARALLEL, COUPLING_TYPE_SERIES, COUPLING_TYPE_COMMON_GND, COUPLING_TYPE_SPLIT_RAILS };

bool isTrackingAllowed(Channel &channel, int *err);
bool isCouplingTypeAllowed(CouplingType couplingType, int *err);

bool setCouplingType(CouplingType couplingType, int *err);
void setCouplingTypeInPsuThread(CouplingType couplingType);
CouplingType getCouplingType();

void setTrackingChannels(uint16_t trackingEnabled);

float getValuePrecision(const Channel &channel, Unit unit, float value);
float roundChannelValue(const Channel &channel, Unit unit, float value);

float getUSet(const Channel &channel);
float getUSet(int slotIndex, int subchannelIndex);
float getUMon(const Channel &channel);
float getUMonLast(const Channel &channel);
float getUMonLast(int slotIndex, int subchannelIndex);
float getUMonDac(const Channel &channel);
float getUMonDacLast(const Channel &channel);
float getULimit(const Channel &channel);
float getUMaxLimit(const Channel &channel);
float getUMin(const Channel &channel);
float getUMin(int slotIndex, int subchannelIndex);
float getUDef(const Channel &channel);
float getUDef(int slotIndex, int subchannelIndex);
float getUMax(const Channel &channel);
float getUMax(int slotIndex, int subchannelIndex);
float getUMaxOvpLimit(const Channel &channel);
float getUMaxOvpLevel(const Channel &channel);
float getUProtectionLevel(const Channel &channel);
void setVoltage(Channel &channel, float voltage);
void setVoltageStep(Channel &channel, float voltageStep);
void setVoltageLimit(Channel &channel, float limit);
void setOvpParameters(Channel &channel, int type, int state, float level, float delay);
void setOvpState(Channel &channel, int state);
void setOvpType(Channel &channel, int type);
void setOvpLevel(Channel &channel, float level);
void setOvpDelay(Channel &channel, float delay);

float getISet(const Channel &channel);
float getISet(int slotIndex, int subchannelIndex);
float getIMon(const Channel &channel);
float getIMonLast(const Channel &channel);
float getIMonLast(int slotIndex, int subchannelIndex);
float getIMonDac(const Channel &channel);
float getILimit(const Channel &channel);
float getIMaxLimit(const Channel &channel);
float getIMaxLimit(int slotIndex, int subchannelIndex);
float getIMin(const Channel &channel);
float getIMin(int slotIndex, int subchannelIndex);
float getIDef(const Channel &channel);
float getIDef(int slotIndex, int subchannelIndex);
float getIMax(const Channel &channel);
float getIMax(int slotIndex, int subchannelIndex);
void setCurrent(Channel &channel, float current);
void setCurrentStep(Channel &channel, float currentStep);
void setCurrentLimit(Channel &channel, float limit);
void setOcpParameters(Channel &channel, int state, float delay);
void setOcpState(Channel &channel, int state);
void setOcpDelay(Channel &channel, float delay);

float getPowerLimit(const Channel &channel);
float getPowerMinLimit(const Channel &channel);
float getPowerMaxLimit(const Channel &channel);
float getPowerDefaultLimit(const Channel &channel);
float getPowerProtectionLevel(const Channel &channel);
void setPowerLimit(Channel &channel, float limit);

float getOppLevel(Channel &channel);
float getOppMinLevel(Channel &channel);
float getOppMaxLevel(Channel &channel);
float getOppDefaultLevel(Channel &channel);
void setOppParameters(Channel &channel, int state, float level, float delay);
void setOppState(Channel &channel, int state);
void setOppLevel(Channel &channel, float level);
void setOppDelay(Channel &channel, float delay);

void setVoltageRampDuration(Channel &channel, float duration);
void setCurrentRampDuration(Channel &channel, float duration);

void setOutputDelayDuration(Channel &channel, float duration);

void outputEnable(Channel &channel, bool enable);
void outputEnableOnNextSync(Channel &channel, bool enable);
void syncOutputEnable();
bool outputEnable(int numChannels, uint8_t *channels, bool enable, int *err);
void disableOutputForAllChannels();
void disableOutputForAllTrackingChannels();

void remoteSensingEnable(Channel &channel, bool enable);

bool isTripped(Channel &channel, int &channelIndex);
void clearProtection(Channel &channel);
void disableProtection(Channel &channel);

bool isOvpTripped(Channel &channel);
bool isOcpTripped(Channel &channel);
bool isOppTripped(Channel &channel);
bool isOtpTripped(Channel &channel);

void clearOtpProtection(int sensor);
void setOtpParameters(Channel &channel, int state, float level, float delay);
void setOtpState(int sensor, int state);
void setOtpLevel(int sensor, float level);
void setOtpDelay(int sensor, float delay);

void setDisplayViewSettings(Channel &channel, int displayValue1, int displayValue2, float ytViewRate);

TriggerMode getVoltageTriggerMode(Channel &channel);
void setVoltageTriggerMode(Channel &channel, TriggerMode mode);

TriggerMode getCurrentTriggerMode(Channel &channel);
void setCurrentTriggerMode(Channel &channel, TriggerMode mode);

bool getTriggerOutputState(Channel &channel);
void setTriggerOutputState(Channel &channel, bool enable);

TriggerOnListStop getTriggerOnListStop(Channel &channel);
void setTriggerOnListStop(Channel &channel, TriggerOnListStop value);

float getTriggerVoltage(Channel &channel);
void setTriggerVoltage(Channel &channel, float value);

float getTriggerCurrent(Channel &channel);
void setTriggerCurrent(Channel &channel, float value);

void setDwellList(Channel &channel, float *list, uint16_t listLength);
void setVoltageList(Channel &channel, float *list, uint16_t listLength);
void setCurrentList(Channel &channel, float *list, uint16_t listLength);
void setListCount(Channel &channel, uint16_t value);

void setCurrentRangeSelectionMode(Channel &channel, CurrentRangeSelectionMode mode);
void enableAutoSelectCurrentRange(Channel &channel, bool enable);

#ifdef EEZ_PLATFORM_SIMULATOR
void setLoadEnabled(Channel &channel, bool state);
void setLoad(Channel &channel, float load);
#endif

void setVoltageInPsuThread(int channelIndex);
void setCurrentInPsuThread(int channelIndex);

const char *copyChannelToChannel(int srcChannelIndex, int dstChannelIndex);

bool isEditEnabled(const eez::gui::WidgetCursor &widgetCursor);

bool getDigitalInputData(int slotIndex, int subchannelIndex, uint8_t &data, int *err);
#ifdef EEZ_PLATFORM_SIMULATOR
bool setDigitalInputData(int slotIndex, int subchannelIndex, uint8_t data, int *err);
#endif

bool getDigitalInputRange(int slotIndex, int subchannelIndex, uint8_t pin, uint8_t &range, int *err);
bool setDigitalInputRange(int slotIndex, int subchannelIndex, uint8_t pin, uint8_t range, int *err);

bool getDigitalInputSpeed(int slotIndex, int subchannelIndex, uint8_t pin, uint8_t &speed, int *err);
bool setDigitalInputSpeed(int slotIndex, int subchannelIndex, uint8_t pin, uint8_t speed, int *err);

bool getDigitalOutputData(int slotIndex, int subchannelIndex, uint8_t &data, int *err);
bool setDigitalOutputData(int slotIndex, int subchannelIndex, uint8_t data, int *err);

bool getSourceMode(int slotIndex, int subchannelIndex, SourceMode &mode, int *err);
bool setSourceMode(int slotIndex, int subchannelIndex, SourceMode mode, int *err);

bool getSourceCurrentRange(int slotIndex, int subchannelIndex, uint8_t &range, int *err);
bool setSourceCurrentRange(int slotIndex, int subchannelIndex, uint8_t range, int *err);

bool getSourceVoltageRange(int slotIndex, int subchannelIndex, uint8_t &range, int *err);
bool setSourceVoltageRange(int slotIndex, int subchannelIndex, uint8_t range, int *err);

bool getMeasureMode(int slotIndex, int subchannelIndex, MeasureMode &mode, int *err);
bool setMeasureMode(int slotIndex, int subchannelIndex, MeasureMode mode, int *err);

bool getMeasureCurrentRange(int slotIndex, int subchannelIndex, uint8_t &range, int *err);
bool setMeasureCurrentRange(int slotIndex, int subchannelIndex, uint8_t range, int *err);

bool getMeasureVoltageRange(int slotIndex, int subchannelIndex, uint8_t &range, int *err);
bool setMeasureVoltageRange(int slotIndex, int subchannelIndex, uint8_t range, int *err);

bool getMeasureCurrentNPLC(int slotIndex, int subchannelIndex, float &nplc, int *err);
bool setMeasureCurrentNPLC(int slotIndex, int subchannelIndex, float nplc, int *err);

bool getMeasureVoltageNPLC(int slotIndex, int subchannelIndex, float &nplc, int *err);
bool setMeasureVoltageNPLC(int slotIndex, int subchannelIndex, float nplc, int *err);

bool isRouteOpen(int slotIndex, int subchannelIndex, int *err);
bool routeOpen(ChannelList channelList, int *err);
bool routeClose(ChannelList channelList, int *err);

bool getVoltage(int slotIndex, int subchannelIndex, float &value, int *err);
bool setVoltage(int slotIndex, int subchannelIndex, float value, int *err);
void getVoltageStepValues(int slotIndex, int subchannelIndex, StepValues *stepValues, bool calibrationMode);
float getVoltageResolution(int slotIndex, int subchannelIndex);
float getVoltageMinValue(int slotIndex, int subchannelIndex);
float getVoltageMaxValue(int slotIndex, int subchannelIndex);

bool getCurrent(int slotIndex, int subchannelIndex, float &value, int *err);
bool setCurrent(int slotIndex, int subchannelIndex, float value, int *err);
void getCurrentStepValues(int slotIndex, int subchannelIndex, StepValues *stepValues, bool calibrationMode);
float getCurrentResolution(int slotIndex, int subchannelIndex);
float getCurrentMinValue(int slotIndex, int subchannelIndex);
float getCurrentMaxValue(int slotIndex, int subchannelIndex);

bool getMeasuredVoltage(int slotIndex, int subchannelIndex, float &value, int *err);
bool getMeasuredCurrent(int slotIndex, int subchannelIndex, float &value, int *err);

} // namespace channel_dispatcher
} // namespace psu
} // namespace eez
