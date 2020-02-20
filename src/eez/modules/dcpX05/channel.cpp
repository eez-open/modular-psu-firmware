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

#include <eez/modules/dcpX05/channel.h>
#include <eez/modules/dcpX05/adc.h>
#include <eez/modules/dcpX05/dac.h>
#include <eez/modules/dcpX05/ioexp.h>

#include <eez/modules/psu/psu.h>
#include <eez/modules/psu/debug.h>
#include <eez/modules/psu/profile.h>
#include <eez/modules/psu/channel_dispatcher.h>

#include <eez/scpi/regs.h>

#include <eez/system.h>
#include <eez/index.h>

/// ADC conversion should be finished after ADC_CONVERSION_MAX_TIME_MS milliseconds.
#define CONF_ADC_CONVERSION_MAX_TIME_MS 10

#define CONF_FALLING_EDGE_OVP_PERCENTAGE 2.0f
#define CONF_FALLING_EDGE_OVP_DELAY_MS 1
#define CONF_OVP_SW_OVP_AT_START_DURATION_MS 5
#define CONF_OVP_SW_OVP_AT_START_U_SET_THRESHOLD 1.2f
#define CONF_OVP_SW_OVP_AT_START_U_PROTECTION_LEVEL 1.55f

namespace eez {

using namespace psu;

namespace dcpX05 {

struct Channel : ChannelInterface {
	AnalogDigitalConverter adc;
	DigitalAnalogConverter dac;
	IOExpander ioexp;

    bool delayed_dp_off;
	uint32_t delayed_dp_off_start;
	bool dpOn;
	uint32_t dpNegMonitoringTime;

    float uSet;

	float uBeforeBalancing = NAN;
	float iBeforeBalancing = NAN;

	bool fallingEdge;
	uint32_t fallingEdgeStart;
	float fallingEdgePreviousUMonAdc;

	Channel(int slotIndex_) : ChannelInterface(slotIndex_), uSet(0) {}

	void getParams(int subchannelIndex, ChannelParams &params) {
		auto slot = g_slots[slotIndex];

		params.U_MIN = 0.0f;
		params.U_DEF = 0.0f;

		if (slot.moduleInfo->moduleType == MODULE_TYPE_DCP505) {
			params.U_MAX = 50.0f;

			params.U_CAL_VAL_MIN = 0.15f;
			params.U_CAL_VAL_MID = 24.1f;
			params.U_CAL_VAL_MAX = 48.0f;
			params.U_CURR_CAL = 25.0f;
		} else {
			params.U_MAX = 40.0f;

			params.U_CAL_VAL_MIN = 0.15f;
			params.U_CAL_VAL_MID = 20.0f;
			params.U_CAL_VAL_MAX = 38.0f;
			params.U_CURR_CAL = 20.0f;
		}

		params.U_MIN_STEP = 0.01f;
		params.U_DEF_STEP = 0.1f;
		params.U_MAX_STEP = 5.0f;

		if (slot.moduleInfo->moduleType == MODULE_TYPE_DCP505) {
			params.U_CAL_VAL_MIN = 0.15f;
			params.U_CAL_VAL_MID = 24.1f;
			params.U_CAL_VAL_MAX = 48.0f;
		} else {
			params.U_CAL_VAL_MIN = 0.15f;
			params.U_CAL_VAL_MID = 20.0f;
			params.U_CAL_VAL_MAX = 38.0f;
		}
		params.I_VOLT_CAL = 0.1f;

		params.I_MIN = 0.0f;
		params.I_DEF = 0.0f;
		params.I_MAX = 5.0f;
		
		params.I_MIN_STEP = 0.01f;
		params.I_DEF_STEP = 0.01f;
		params.I_MAX_STEP = 1.0f; 
		
		params.I_CAL_VAL_MIN = 0.05f;
		params.I_CAL_VAL_MID = 2.425f;
		params.I_CAL_VAL_MAX = 4.8f;
		if (slot.moduleInfo->moduleType == MODULE_TYPE_DCP505) {
			params.U_CURR_CAL = 25.0f;
		} else {
			params.U_CURR_CAL = 20.0f;
		}

		params.OVP_DEFAULT_STATE = false;
		params.OVP_MIN_DELAY = 0.0f;
		params.OVP_DEFAULT_DELAY = 0.0f;
		params.OVP_MAX_DELAY = 10.0f;

		params.OCP_DEFAULT_STATE = false;
		params.OCP_MIN_DELAY = 0.0f;
		params.OCP_DEFAULT_DELAY = 0.02f;
		params.OCP_MAX_DELAY = 10.0f;

		params.OPP_DEFAULT_STATE = true;
		params.OPP_MIN_DELAY = 1.0f;
		params.OPP_DEFAULT_DELAY = 10.0f;
		params.OPP_MAX_DELAY = 300.0f;
		params.OPP_MIN_LEVEL = 0.0f;
		params.OPP_DEFAULT_LEVEL = 155.0f;

		params.PTOT = 155.0f;

		params.U_RESOLUTION = 0.005f;
		params.I_RESOLUTION = 0.0005f;
		params.I_LOW_RESOLUTION = 0.000005f;
		params.P_RESOLUTION = 0.001f;

		if (slot.moduleInfo->moduleType == MODULE_TYPE_DCP505) {
			params.VOLTAGE_GND_OFFSET = 1.05f;
		} else {
			params.VOLTAGE_GND_OFFSET = 0.86f;
		}
		params.CURRENT_GND_OFFSET = 0.11f;

		params.CALIBRATION_DATA_TOLERANCE_PERCENT = 10.0f;

		params.CALIBRATION_MID_TOLERANCE_PERCENT = 1.0f;

		if (slot.moduleInfo->moduleType == MODULE_TYPE_DCP505) {
			params.features = CH_FEATURE_VOLT | CH_FEATURE_CURRENT | CH_FEATURE_POWER | CH_FEATURE_OE | 
				CH_FEATURE_DPROG | CH_FEATURE_RPROG | CH_FEATURE_RPOL;
		} else if (slot.moduleInfo->moduleType == MODULE_TYPE_DCP405) {
			params.features = CH_FEATURE_VOLT | CH_FEATURE_CURRENT | CH_FEATURE_POWER | CH_FEATURE_OE | 
				CH_FEATURE_DPROG | CH_FEATURE_RPROG | CH_FEATURE_RPOL | 
				CH_FEATURE_CURRENT_DUAL_RANGE | CH_FEATURE_HW_OVP | CH_FEATURE_COUPLING;
		} else {
			// DCP405B
			params.features = CH_FEATURE_VOLT | CH_FEATURE_CURRENT | CH_FEATURE_POWER | CH_FEATURE_OE | 
				CH_FEATURE_CURRENT_DUAL_RANGE | CH_FEATURE_COUPLING;
		}

		params.MON_REFRESH_RATE_MS = 250;
	}

	void init(int subchannelIndex) {
		ioexp.slotIndex = slotIndex;
		adc.slotIndex = slotIndex;
		dac.slotIndex = slotIndex;

		ioexp.init();
		adc.init();
		dac.init();
	}

	void onPowerDown(int subchannelIndex) {
	}

	void reset(int subchannelIndex) {
		uSet = 0;
        dpOn = false;
		uBeforeBalancing = NAN;
		iBeforeBalancing = NAN;
	}

	void test(int subchannelIndex) {
		ioexp.test();
		adc.test();
		dac.test(ioexp, adc);
	}

	TestResult getTestResult(int subchannelIndex) {
		if (ioexp.g_testResult == TEST_NONE || adc.g_testResult == TEST_NONE || dac.g_testResult == TEST_NONE) {
			return TEST_NONE;
		}

		if (ioexp.g_testResult == TEST_OK && adc.g_testResult == TEST_OK && dac.g_testResult == TEST_OK) {
			return TEST_OK;
		}

		return TEST_FAILED;
	}

	AdcDataType getNextAdcDataType(AdcDataType adcDataType) {
		if (adcDataType == ADC_DATA_TYPE_U_MON) {
			return ADC_DATA_TYPE_I_MON;
		}

		psu::Channel &channel = psu::Channel::getBySlotIndex(slotIndex);
		if (adcDataType == ADC_DATA_TYPE_I_MON && channel.isRemoteProgrammingEnabled()) {
			return ADC_DATA_TYPE_U_MON_DAC;
		}

		return ADC_DATA_TYPE_U_MON;
	}

	void tick(int subchannelIndex, uint32_t tickCount) {
		if (isDacTesting(subchannelIndex)) {
			return;
		}

        psu::Channel &channel = psu::Channel::getBySlotIndex(slotIndex);

		ioexp.tick(tickCount);

#if !CONF_SKIP_PWRGOOD_TEST
		if (!ioexp.testBit(IOExpander::IO_BIT_IN_PWRGOOD)) {
			// DebugTrace("Ch%d PWRGOOD bit changed to 0, gpio=%d", channel.channelIndex + 1, (int)ioexp.gpio);
			channel.flags.powerOk = 0;
			generateError(SCPI_ERROR_CH1_FAULT_DETECTED - channel.channelIndex);
			powerDownBySensor();
			return;
		}
#endif

		if (channel.isOutputEnabled() && ioexp.isAdcReady()) {
			auto adcDataType = adc.adcDataType;
			float value = adc.read(channel);
			adc.start(getNextAdcDataType(adcDataType));
			channel.onAdcData(adcDataType, value);
#ifdef DEBUG
			psu::debug::g_adcCounter.inc();
#endif
		}

		if (channel.params.features & CH_FEATURE_DPROG) {
			if (channel.flags.dprogState == DPROG_STATE_ON) {
				// turn off DP after delay
				if (delayed_dp_off && (millis() - delayed_dp_off_start) >= DP_OFF_DELAY_PERIOD) {
					delayed_dp_off = false;
					setDpEnable(false);
				}
			}
			
			/// Output power is monitored and if its go below DP_NEG_LEV
			/// that is negative value in Watts (default -1 W),
			/// and that condition lasts more then DP_NEG_DELAY seconds (default 5 s),
			/// down-programmer circuit has to be switched off.
			if (channel.isOutputEnabled()) {
				if (channel.u.mon_last * channel.i.mon_last >= DP_NEG_LEV || tickCount < dpNegMonitoringTime) {
					dpNegMonitoringTime = tickCount;
				} else {
					if (tickCount - dpNegMonitoringTime > DP_NEG_DELAY * 1000000UL) {
						if (dpOn) {
							// DebugTrace("CH%d, neg. P, DP off: %f", channel.channelIndex + 1, channel.u.mon_last * channel.i.mon_last);
							dpNegMonitoringTime = tickCount;
							generateError(SCPI_ERROR_CH1_DOWN_PROGRAMMER_SWITCHED_OFF + channel.channelIndex);
							setDpEnable(false);
						} else {
							// DebugTrace("CH%d, neg. P, output off: %f", channel.channelIndex + 1, channel.u.mon_last * channel.i.mon_last);
							generateError(SCPI_ERROR_CH1_OUTPUT_FAULT_DETECTED - channel.channelIndex);
							channel_dispatcher::outputEnable(channel, false);
						}
					} else if (tickCount - dpNegMonitoringTime > 500 * 1000UL) {
						if (dpOn && channel.channelIndex < 2) {
							if (channel_dispatcher::getCouplingType() == channel_dispatcher::COUPLING_TYPE_SERIES) {
								psu::Channel &channel2 = psu::Channel::get(channel.channelIndex == 0 ? 1 : 0);
								voltageBalancing(channel2);
								dpNegMonitoringTime = tickCount;
							} else if (channel_dispatcher::getCouplingType() == channel_dispatcher::COUPLING_TYPE_PARALLEL) {
								psu::Channel &channel2 = psu::Channel::get(channel.channelIndex == 0 ? 1 : 0);
								currentBalancing(channel2);
								dpNegMonitoringTime = tickCount;
							}
						}
					}
				}
			}
		}

        if (channel.channelIndex < 2 && channel_dispatcher::getCouplingType() == channel_dispatcher::COUPLING_TYPE_SERIES) {
            unsigned cvMode = isCvMode(subchannelIndex) ? 1 : 0;
            if (cvMode != channel.flags.cvMode) {
                restoreVoltageToValueBeforeBalancing(psu::Channel::get(channel.channelIndex == 0 ? 1 : 0));
            }
        }

        if (channel.channelIndex < 2 && channel_dispatcher::getCouplingType() == channel_dispatcher::COUPLING_TYPE_PARALLEL) {
            unsigned ccMode = isCcMode(subchannelIndex) ? 1 : 0;
		    if (ccMode != channel.flags.ccMode) {
				restoreCurrentToValueBeforeBalancing(psu::Channel::get(channel.channelIndex == 0 ? 1 : 0));
			}
		}

		if (channel.params.features & CH_FEATURE_HW_OVP) {
			bool fallingEdgeChange = false;

			if (fallingEdge) {
				fallingEdge =
					((millis() - fallingEdgeStart) < CONF_FALLING_EDGE_OVP_DELAY_MS) ||
					(fallingEdgePreviousUMonAdc > channel.u.mon_adc) ||
					(channel.u.mon_adc > uSet * (1.0f + CONF_FALLING_EDGE_OVP_PERCENTAGE / 100.0f));

				if (fallingEdge) {
					fallingEdgePreviousUMonAdc = channel.u.mon_adc;
				} else {
					DebugTrace("DAC=%g, ADC=%g\n", uSet, channel.u.mon_adc);
					fallingEdgeChange = true;
				}
			}

			// HW OVP handling
			if (ioexp.testBit(IOExpander::IO_BIT_OUT_OUTPUT_ENABLE)) {
				if (!fallingEdge && channel.isHwOvpEnabled() && !ioexp.testBit(IOExpander::DCP405_IO_BIT_OUT_OVP_ENABLE)) {
					// activate HW OVP
					ioexp.changeBit(IOExpander::DCP405_IO_BIT_OUT_OVP_ENABLE, true);
					if (fallingEdgeChange) {
						DebugTrace("HW OVP ON falling edge\n");
					} else {
						DebugTrace("HW OVP ON\n");
					}
				} else if ((fallingEdge || !channel.isHwOvpEnabled()) && ioexp.testBit(IOExpander::DCP405_IO_BIT_OUT_OVP_ENABLE)) {
					// deactivate HW OVP
					ioexp.changeBit(IOExpander::DCP405_IO_BIT_OUT_OVP_ENABLE, false);
					DebugTrace("HW OVP OFF\n");
				}
			}
		}
	}

	unsigned getRPol(int subchannelIndex) {
		return !ioexp.testBit(IOExpander::IO_BIT_IN_RPOL);
	}

	bool isCcMode(int subchannelIndex) {
		return ioexp.testBit(IOExpander::IO_BIT_IN_CV_ACTIVE);
	}

	bool isCvMode(int subchannelIndex) {
		return ioexp.testBit(IOExpander::IO_BIT_IN_CC_ACTIVE);
	}

	void waitConversionEnd() {
#if defined(EEZ_PLATFORM_STM32)
        for (int i = 0; i < CONF_ADC_CONVERSION_MAX_TIME_MS; i++) {
            ioexp.tick(micros());
            if (ioexp.isAdcReady()) {
				break;
			}
			delay(1);
		}
#endif
    }

	void adcMeasureUMon(int subchannelIndex) {
		psu::Channel &channel = psu::Channel::getBySlotIndex(slotIndex);

		adc.start(ADC_DATA_TYPE_U_MON);
		waitConversionEnd();
		channel.onAdcData(ADC_DATA_TYPE_U_MON, adc.read(channel));

		if (channel.isOutputEnabled()) {
			adc.start(ADC_DATA_TYPE_U_MON);
		}
	}

	void adcMeasureIMon(int subchannelIndex) {
        psu::Channel &channel = psu::Channel::getBySlotIndex(slotIndex);

		adc.start(ADC_DATA_TYPE_U_MON);
		waitConversionEnd();
		channel.onAdcData(ADC_DATA_TYPE_U_MON, adc.read(channel));

		if (channel.isOutputEnabled()) {
			adc.start(ADC_DATA_TYPE_U_MON);
		}
	}

	void adcMeasureMonDac(int subchannelIndex) {
        psu::Channel &channel = psu::Channel::getBySlotIndex(slotIndex);

		if (channel.isOk() || isDacTesting(subchannelIndex)) {
			adc.start(ADC_DATA_TYPE_U_MON_DAC);
			waitConversionEnd();
			channel.onAdcData(ADC_DATA_TYPE_U_MON_DAC, adc.read(channel));

			adc.start(ADC_DATA_TYPE_I_MON_DAC);
			waitConversionEnd();
			channel.onAdcData(ADC_DATA_TYPE_I_MON_DAC, adc.read(channel));

			if (channel.isOutputEnabled()) {
				adc.start(ADC_DATA_TYPE_U_MON);
			}
		}
	}

	void adcMeasureAll(int subchannelIndex) {
        psu::Channel &channel = psu::Channel::getBySlotIndex(slotIndex);

		adc.start(ADC_DATA_TYPE_U_MON);
		waitConversionEnd();
		channel.onAdcData(ADC_DATA_TYPE_U_MON, adc.read(channel));

		adc.start(ADC_DATA_TYPE_I_MON);
		waitConversionEnd();
		channel.onAdcData(ADC_DATA_TYPE_I_MON, adc.read(channel));

		adc.start(ADC_DATA_TYPE_U_MON_DAC);
		waitConversionEnd();
		channel.onAdcData(ADC_DATA_TYPE_U_MON_DAC, adc.read(channel));

		adc.start(ADC_DATA_TYPE_I_MON_DAC);
		waitConversionEnd();
		channel.onAdcData(ADC_DATA_TYPE_I_MON_DAC, adc.read(channel));

		if (channel.isOutputEnabled()) {
			adc.start(ADC_DATA_TYPE_U_MON);
		}
	}

	void setDpEnable(bool enable) {
		psu::Channel &channel = psu::Channel::getBySlotIndex(slotIndex);
		if (channel.params.features & CH_FEATURE_DPROG) {			
			// DP bit is active low
			ioexp.changeBit(IOExpander::IO_BIT_OUT_DP_ENABLE, !enable);

			setOperBits(OPER_ISUM_DP_OFF, !enable);
			dpOn = enable;

			if (enable) {
				dpNegMonitoringTime = micros();
			}
		}
	}

	void setOutputEnable(int subchannelIndex, bool enable, uint16_t tasks) {
		psu::Channel &channel = psu::Channel::getBySlotIndex(slotIndex);

		if (enable) {
			// OE
			if (tasks & OUTPUT_ENABLE_TASK_OE) {
				ioexp.changeBit(IOExpander::IO_BIT_OUT_OUTPUT_ENABLE, true);
			}

			// DAC
			if (tasks & OUTPUT_ENABLE_TASK_DAC) {
				dac.setVoltage(uSet);
			}

			// Current range
			if (tasks & OUTPUT_ENABLE_TASK_CURRENT_RANGE) {
				setCurrentRange(subchannelIndex);
			}

			// OVP
			if (tasks & OUTPUT_ENABLE_TASK_OVP) {
				if (channel.params.features & CH_FEATURE_HW_OVP) {
					if (channel.isHwOvpEnabled()) {
						// OVP has to be enabled after OE activation
						ioexp.changeBit(IOExpander::DCP405_IO_BIT_OUT_OVP_ENABLE, true);
						DebugTrace("HW OVP ON output enable\n");
					}
				}
			}

			// DP
			if (tasks & OUTPUT_ENABLE_TASK_DP) {
				if (channel.params.features & CH_FEATURE_DPROG) {
					if (channel.flags.dprogState == DPROG_STATE_ON) {
						// enable DP
						dpNegMonitoringTime = micros();
						delayed_dp_off = false;
						setDpEnable(true);
					}
				}
			}

			if (tasks & OUTPUT_ENABLE_TASK_ADC_START) {
				adc.start(ADC_DATA_TYPE_U_MON);
			}
		} else {
			// OVP
			if (tasks & OUTPUT_ENABLE_TASK_OVP) {
				if (channel.params.features & CH_FEATURE_HW_OVP) {
					if (channel.isHwOvpEnabled()) {
						// OVP has to be disabled before OE deactivation
						ioexp.changeBit(IOExpander::DCP405_IO_BIT_OUT_OVP_ENABLE, false);
						DebugTrace("HW OVP OFF output disable\n");
					}
				}
			}

			// DAC
			if (tasks & OUTPUT_ENABLE_TASK_DAC) {
				dac.setDacVoltage(0);
			}

			// OE
			if (tasks & OUTPUT_ENABLE_TASK_OE) {
				ioexp.changeBit(IOExpander::IO_BIT_OUT_OUTPUT_ENABLE, false);

				channel.u.resetMonValues();
				channel.i.resetMonValues();
			}

			// Current range
			if (tasks & OUTPUT_ENABLE_TASK_CURRENT_RANGE) {
				setCurrentRange(subchannelIndex);
			}

			// DP
			if (tasks & OUTPUT_ENABLE_TASK_DP) {
				if (channel.params.features & CH_FEATURE_DPROG) {			
					if (channel.flags.dprogState == DPROG_STATE_ON) {
						// turn off DP after some delay
						delayed_dp_off = true;
						delayed_dp_off_start = millis();
					}
				}
			}
		}

		if (tasks & OUTPUT_ENABLE_TASK_FINALIZE) {
			if (channel.params.features & CH_FEATURE_COUPLING) {
				if (channel.channelIndex == 0 && channel_dispatcher::getCouplingType() == channel_dispatcher::COUPLING_TYPE_PARALLEL) {
					ioexp.changeBit(IOExpander::DCP405_IO_BIT_OUT_OE_UNCOUPLED_LED, false);
					ioexp.changeBit(IOExpander::DCP405_IO_BIT_OUT_OE_COUPLED_LED, enable);
				} else if (channel.channelIndex == 1 && channel_dispatcher::getCouplingType() == channel_dispatcher::COUPLING_TYPE_PARALLEL) {
					ioexp.changeBit(IOExpander::DCP405_IO_BIT_OUT_OE_UNCOUPLED_LED, false);
					ioexp.changeBit(IOExpander::DCP405_IO_BIT_OUT_OE_COUPLED_LED, false);
				} else if (channel.channelIndex == 0 && channel_dispatcher::getCouplingType() == channel_dispatcher::COUPLING_TYPE_SERIES) {
					ioexp.changeBit(IOExpander::DCP405_IO_BIT_OUT_OE_UNCOUPLED_LED, false);
					ioexp.changeBit(IOExpander::DCP405_IO_BIT_OUT_OE_COUPLED_LED, enable);
				} else if (channel.channelIndex == 1 && channel_dispatcher::getCouplingType() == channel_dispatcher::COUPLING_TYPE_SERIES) {
					ioexp.changeBit(IOExpander::DCP405_IO_BIT_OUT_OE_UNCOUPLED_LED, false);
					ioexp.changeBit(IOExpander::DCP405_IO_BIT_OUT_OE_COUPLED_LED, false);
				} else if (channel.channelIndex < 2 && channel_dispatcher::getCouplingType() == channel_dispatcher::COUPLING_TYPE_SPLIT_RAILS) {
					ioexp.changeBit(IOExpander::DCP405_IO_BIT_OUT_OE_UNCOUPLED_LED, false);
					ioexp.changeBit(IOExpander::DCP405_IO_BIT_OUT_OE_COUPLED_LED, enable);
				} else if (channel_dispatcher::getCouplingType() == channel_dispatcher::COUPLING_TYPE_COMMON_GND) {
					ioexp.changeBit(IOExpander::DCP405_IO_BIT_OUT_OE_UNCOUPLED_LED, false);
					ioexp.changeBit(IOExpander::DCP405_IO_BIT_OUT_OE_COUPLED_LED, enable);
				} else {
					ioexp.changeBit(IOExpander::DCP405_IO_BIT_OUT_OE_UNCOUPLED_LED, enable);
					ioexp.changeBit(IOExpander::DCP405_IO_BIT_OUT_OE_COUPLED_LED, false);
				}
			} else {
				ioexp.changeBit(IOExpander::DCP505_IO_BIT_OUT_OE_UNCOUPLED_LED, enable);

			}

			restoreVoltageToValueBeforeBalancing(channel);
			restoreCurrentToValueBeforeBalancing(channel);
		}
	}

    void setDprogState(DprogState dprogState) {
		psu::Channel &channel = psu::Channel::getBySlotIndex(slotIndex);
		if (channel.params.features & CH_FEATURE_DPROG) {			
			if (dprogState == DPROG_STATE_OFF) {
				setDpEnable(false);
			} else {
				setDpEnable(channel.isOk() && ioexp.testBit(IOExpander::IO_BIT_OUT_OUTPUT_ENABLE));
			}
			delayed_dp_off = false;
		}
    }
    
    void setRemoteSense(int subchannelIndex, bool enable) {
		ioexp.changeBit(IOExpander::IO_BIT_OUT_REMOTE_SENSE, enable);
	}

	void setRemoteProgramming(int subchannelIndex, bool enable) {
		ioexp.changeBit(IOExpander::IO_BIT_OUT_REMOTE_PROGRAMMING, enable);
	}

	void setDacVoltage(int subchannelIndex, uint16_t value) {
		dac.setDacVoltage(value);

        uBeforeBalancing = NAN;
        restoreCurrentToValueBeforeBalancing(psu::Channel::getBySlotIndex(slotIndex));
    }

	void setDacVoltageFloat(int subchannelIndex, float value) {
		psu::Channel &channel = psu::Channel::getBySlotIndex(slotIndex);

		float previousUSet = uSet;

		uSet = value;

		if ((channel.params.features & CH_FEATURE_HW_OVP) && channel.isOutputEnabled() && value < previousUSet) {
			fallingEdge = true;
			fallingEdgePreviousUMonAdc = channel.u.mon_adc;
			fallingEdgeStart = millis();
			if (channel.isHwOvpEnabled()) {
				// deactivate HW OVP
				ioexp.changeBit(IOExpander::DCP405_IO_BIT_OUT_OVP_ENABLE, false);
				DebugTrace("HW OVP OFF falling edge\n");
			}
		}

        if (ioexp.testBit(IOExpander::IO_BIT_OUT_OUTPUT_ENABLE) || isDacTesting(subchannelIndex)) {
			dac.setVoltage(value);
		} else {
			dac.setDacVoltage(0);
		}

		uBeforeBalancing = NAN;
		restoreCurrentToValueBeforeBalancing(psu::Channel::getBySlotIndex(slotIndex));
	}

	void setDacCurrent(int subchannelIndex, uint16_t value) {
		dac.setDacCurrent(value);

		iBeforeBalancing = NAN;
		restoreVoltageToValueBeforeBalancing(psu::Channel::getBySlotIndex(slotIndex));
	}

	void setDacCurrentFloat(int subchannelIndex, float value) {
		dac.setCurrent(value);
		iBeforeBalancing = NAN;
		restoreVoltageToValueBeforeBalancing(psu::Channel::getBySlotIndex(slotIndex));
	}

	bool isDacTesting(int subchannelIndex) {
		return dac.isTesting();
	}

	void setCurrentRange(int subchannelIndex) {
        auto &channel = psu::Channel::getBySlotIndex(slotIndex);
		auto &slot = g_slots[slotIndex];

		if (!channel.hasSupportForCurrentDualRange()) {
			return;
		}

		if (slot.moduleRevision == MODULE_REVISION_DCP405_R1B1) {
			ioexp.changeBit(IOExpander::DCP405_IO_BIT_OUT_CURRENT_RANGE_500MA, false);
		}

		if (channel.isOutputEnabled()) {
			if (channel.flags.currentCurrentRange == 0 || dac.isTesting()) {
				// 5A
				// DebugTrace("CH%d: Switched to 5A range", channel.channelIndex + 1);
				ioexp.changeBit(IOExpander::DCP405_IO_BIT_OUT_CURRENT_RANGE_5A, true);
				ioexp.changeBit(slot.moduleRevision == MODULE_REVISION_DCP405_R1B1 ?
					IOExpander::DCP405_IO_BIT_OUT_CURRENT_RANGE_50MA :
					IOExpander::DCP405_R2B5_IO_BIT_OUT_CURRENT_RANGE_50MA, false);
				// calculateNegligibleAdcDiffForCurrent();
			} else {
				// 50mA
				// DebugTrace("CH%d: Switched to 50mA range", channel.channelIndex + 1);
				ioexp.changeBit(slot.moduleRevision == MODULE_REVISION_DCP405_R1B1 ?
					IOExpander::DCP405_IO_BIT_OUT_CURRENT_RANGE_50MA :
					IOExpander::DCP405_R2B5_IO_BIT_OUT_CURRENT_RANGE_50MA, true);
				ioexp.changeBit(IOExpander::DCP405_IO_BIT_OUT_CURRENT_RANGE_5A, false);
				// calculateNegligibleAdcDiffForCurrent();
			}
		} else {
			ioexp.changeBit(IOExpander::DCP405_IO_BIT_OUT_CURRENT_RANGE_5A, true);
			ioexp.changeBit(slot.moduleRevision == MODULE_REVISION_DCP405_R1B1 ?
				IOExpander::DCP405_IO_BIT_OUT_CURRENT_RANGE_50MA :
				IOExpander::DCP405_R2B5_IO_BIT_OUT_CURRENT_RANGE_50MA, false);
		}
	}

    bool isVoltageBalanced(int subchannelIndex) {
        return !isNaN(uBeforeBalancing);
    }

    bool isCurrentBalanced(int subchannelIndex) {
        return !isNaN(iBeforeBalancing);
    }

    float getUSetUnbalanced(int subchannelIndex) {
        psu::Channel &channel = psu::Channel::getBySlotIndex(slotIndex);
        return isVoltageBalanced(subchannelIndex) ? uBeforeBalancing : channel.u.set;
    }

    float getISetUnbalanced(int subchannelIndex) {
        psu::Channel &channel = psu::Channel::getBySlotIndex(slotIndex);
        return isCurrentBalanced(subchannelIndex) ? iBeforeBalancing : channel.i.set;
    }

	void readAllRegisters(int subchannelIndex, uint8_t ioexpRegisters[], uint8_t adcRegisters[]) {
		ioexp.readAllRegisters(ioexpRegisters);
		adc.readAllRegisters(adcRegisters);
	}

	#if defined(DEBUG) && defined(EEZ_PLATFORM_STM32)
	int getIoExpBitDirection(int subchannelIndex, int io_bit) {
		return ioexp.getBitDirection(io_bit);
	}

	bool testIoExpBit(int subchannelIndex, int io_bit) {
		return ioexp.testBit(io_bit);
	}

	void changeIoExpBit(int subchannelIndex, int io_bit, bool set) {
		ioexp.changeBit(io_bit, set);
	}
	#endif

    void voltageBalancing(psu::Channel &channel) {
        // DebugTrace("Channel voltage balancing: CH1_Umon=%f, CH2_Umon=%f",
        // Channel::get(0).u.mon_last, Channel::get(1).u.mon_last);
        if (isNaN(uBeforeBalancing)) {
            uBeforeBalancing = channel.u.set;
        }
        channel.doSetVoltage((psu::Channel::get(0).u.mon_last + psu::Channel::get(1).u.mon_last) / 2);
    }

    void currentBalancing(psu::Channel &channel) {
        // DebugTrace("CH%d channel current balancing: CH1_Imon=%f, CH2_Imon=%f", index,
        // Channel::get(0).i.mon_last, Channel::get(1).i.mon_last);
        if (isNaN(iBeforeBalancing)) {
            iBeforeBalancing = channel.i.set;
        }
        channel.doSetCurrent((psu::Channel::get(0).i.mon_last + psu::Channel::get(1).i.mon_last) / 2);
    }

	void restoreVoltageToValueBeforeBalancing(psu::Channel &channel) {
		if (!isNaN(uBeforeBalancing)) {
			// DebugTrace("Restore voltage to value before balancing: %f", uBeforeBalancing);
			channel.setVoltage(uBeforeBalancing);
			uBeforeBalancing = NAN;
		}
	}

	void restoreCurrentToValueBeforeBalancing(psu::Channel &channel) {
		if (!isNaN(iBeforeBalancing)) {
			// DebugTrace("Restore current to value before balancing: %f", index, iBeforeBalancing);
            channel.setCurrent(iBeforeBalancing);
			iBeforeBalancing = NAN;
		}
	}

#if defined(EEZ_PLATFORM_STM32)
	void onSpiIrq() {
		uint8_t intcap = ioexp.readIntcapRegister();
		// DebugTrace("CH%d INTCAP 0x%02X\n", (int)(channel.channelIndex + 1), (int)intcap);
		psu::Channel &channel = psu::Channel::getBySlotIndex(slotIndex);
		if (channel.params.features & CH_FEATURE_HW_OVP) {
			if (!(intcap & (1 << IOExpander::DCP405_R2B5_IO_BIT_IN_OVP_FAULT))) {
				if (channel.isOutputEnabled()) {
					DebugTrace("HW OVP trip MON=%g ADC=%g\n", channel.u.mon_last, channel.u.mon_adc);
					channel.enterOvpProtection();
				}
			}
		}
	}
#endif

	void getFirmwareVersion(uint8_t &majorVersion, uint8_t &minorVersion) {
		majorVersion = 0;
		minorVersion = 0;
	}
};

static Channel g_channel0(0);
static Channel g_channel1(1);
static Channel g_channel2(2);
ChannelInterface *g_channelInterfaces[NUM_SLOTS] = { &g_channel0, &g_channel1, &g_channel2 };

} // namespace dcpX05
} // namespace eez
