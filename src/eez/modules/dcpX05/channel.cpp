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

#include <eez/apps/psu/psu.h>
#include <eez/apps/psu/profile.h>
#include <eez/apps/psu/channel_dispatcher.h>

#include <eez/scpi/regs.h>

#include <eez/system.h>
#include <eez/index.h>

#define CONF_OVP_PERCENTAGE 2.0f
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

    DprogState dprogState;
    bool delayed_dp_off;
	uint32_t delayed_dp_off_start;
	bool dpOn;
	uint32_t dpNegMonitoringTime;

    float uSet;

	float uBeforeBalancing = NAN;
	float iBeforeBalancing = NAN;

	bool fallingEdge = true;

	bool isSwOvpAtStart;
	uint32_t oeTickCount;

	Channel(int slotIndex_) : ChannelInterface(slotIndex_), uSet(0) {}

	void init(int subchannelIndex) {
		ioexp.slotIndex = slotIndex;
		adc.slotIndex = slotIndex;
		dac.slotIndex = slotIndex;

		ioexp.init();
		adc.init();
		dac.init();

        dprogState = DPROG_STATE_AUTO;
	}

	void reset(int subchannelIndex) {
		uSet = 0;
        dprogState = DPROG_STATE_AUTO;
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

	void tick(int subchannelIndex, uint32_t tickCount) {
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

		AdcDataType adcDataType = adc.adcDataType;

		if (adcDataType) {

#if defined(EEZ_PLATFORM_STM32)
			if (ioexp.isAdcReady()) {
#endif

#ifdef DEBUG
				psu::debug::g_adcCounter.inc();
#endif

				float value = adc.read(channel);
				AdcDataType nextAdcDataType = channel.onAdcData(adcDataType, value);
				adc.start(nextAdcDataType);

#if defined(EEZ_PLATFORM_STM32)
			}
#endif
		}

        if (dprogState == DPROG_STATE_AUTO) {
            // turn off DP after delay
            if (delayed_dp_off && (micros() - delayed_dp_off_start) >= DP_OFF_DELAY_PERIOD * 1000000L) {
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

		if (fallingEdge) {
			fallingEdge = channel.u.mon_last > channel.u.set * (1.0f + CONF_OVP_PERCENTAGE / 100.0f);
		}

		if (g_slots[slotIndex].moduleType == MODULE_TYPE_DCP405) {
			if (channel.isOutputEnabled()) {
				if (!fallingEdge && channel.isHwOvpEnabled() && !ioexp.testBit(IOExpander::DCP405_IO_BIT_OUT_OVP_ENABLE)) {
					if (isSwOvpAtStart) {
						int32_t diff = tickCount - oeTickCount;
						if (diff >= CONF_OVP_SW_OVP_AT_START_DURATION_MS * 1000 || channel_dispatcher::getUSet(channel) >= CONF_OVP_SW_OVP_AT_START_U_SET_THRESHOLD) {
							// SW OVP at start lasts 5ms
							isSwOvpAtStart = false;
						} else {
							if (channel.checkSwOvpCondition(CONF_OVP_SW_OVP_AT_START_U_PROTECTION_LEVEL)) {
								channel.enterOvpProtection();
							}
						}
					}
					if (!isSwOvpAtStart) {
						// activate HW OVP
						ioexp.changeBit(IOExpander::DCP405_IO_BIT_OUT_OVP_ENABLE, true);
					}
				} else if ((fallingEdge || !channel.isHwOvpEnabled()) && ioexp.testBit(IOExpander::DCP405_IO_BIT_OUT_OVP_ENABLE)) {
					// deactivate HW OVP
					ioexp.changeBit(IOExpander::DCP405_IO_BIT_OUT_OVP_ENABLE, false);
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
        for (int i = 0; i < ADC_CONVERSION_MAX_TIME_MS; i++) {
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
	}

	void adcMeasureIMon(int subchannelIndex) {
        psu::Channel &channel = psu::Channel::getBySlotIndex(slotIndex);

		adc.start(ADC_DATA_TYPE_U_MON);
		waitConversionEnd();
		channel.onAdcData(ADC_DATA_TYPE_U_MON, adc.read(channel));
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
	}

	void setDpEnable(bool enable) {
		// DP bit is active low
		ioexp.changeBit(IOExpander::IO_BIT_OUT_DP_ENABLE, !enable);

		setOperBits(OPER_ISUM_DP_OFF, !enable);
		dpOn = enable;

		if (enable) {
			dpNegMonitoringTime = micros();
		}
	}

	void setOutputEnable(int subchannelIndex, bool enable) {
		psu::Channel &channel = psu::Channel::getBySlotIndex(slotIndex);

		if (enable) {
            if (dprogState == DPROG_STATE_AUTO) {
                // enable DP
                dpNegMonitoringTime = micros();
                delayed_dp_off = false;
                setDpEnable(true);
            }

			dac.setVoltage(uSet);

			setCurrentRange(subchannelIndex);

			ioexp.changeBit(IOExpander::IO_BIT_OUT_OUTPUT_ENABLE, true);

			if (channel.isHwOvpEnabled()) {
				isSwOvpAtStart = channel_dispatcher::getUSet(channel) < CONF_OVP_SW_OVP_AT_START_U_SET_THRESHOLD;
				if (isSwOvpAtStart) {
					oeTickCount = micros();
				} else {
					// OVP has to be enabled after OE activation
					ioexp.changeBit(IOExpander::DCP405_IO_BIT_OUT_OVP_ENABLE, true);
				}
			}

			adc.start(ADC_DATA_TYPE_U_MON);
		} else {
			if (channel.isHwOvpEnabled()) {
				// OVP has to be disabled before OE deactivation
				ioexp.changeBit(IOExpander::DCP405_IO_BIT_OUT_OVP_ENABLE, false);
			}

			dac.setVoltage(channel.getCalibratedVoltage(0));

			ioexp.changeBit(IOExpander::IO_BIT_OUT_OUTPUT_ENABLE, false);

			setCurrentRange(subchannelIndex);

            if (dprogState == DPROG_STATE_AUTO) {
                // turn off DP after some delay
                delayed_dp_off = true;
                delayed_dp_off_start = micros();
            }
		}

		ioexp.changeBit(g_slots[slotIndex].moduleType == MODULE_TYPE_DCP405 ?IOExpander::DCP405_IO_BIT_OUT_OE_UNCOUPLED_LED : IOExpander::DCP505_IO_BIT_OUT_OE_UNCOUPLED_LED, enable);

		restoreVoltageToValueBeforeBalancing(channel);
		restoreCurrentToValueBeforeBalancing(channel);
	}

    DprogState getDprogState() {
        return dprogState;
    }

    void setDprogState(DprogState dprogState_) {
        dprogState = dprogState_;

        if (dprogState == DPROG_STATE_OFF) {
            setDpEnable(false);
        } else if (dprogState == DPROG_STATE_ON) {
            setDpEnable(true);
        } else {
            psu::Channel &channel = psu::Channel::getBySlotIndex(slotIndex);
            setDpEnable(channel.isOutputEnabled());
        }

        delayed_dp_off = false;
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

        if (channel.isOutputEnabled()) {
            if (value < uSet) {
                fallingEdge = true;
				if (channel.isHwOvpEnabled()) {
					// deactivate HW OVP
					ioexp.changeBit(IOExpander::DCP405_IO_BIT_OUT_OVP_ENABLE, false);
				}
            }
        }

        if (channel.isOutputEnabled() || isDacTesting(subchannelIndex)) {
			dac.setVoltage(value);
		}

		uSet = value;

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
        psu::Channel &channel = psu::Channel::getBySlotIndex(slotIndex);

		if (!channel.hasSupportForCurrentDualRange()) {
			return;
		}

		if (channel.boardRevision == CH_BOARD_REVISION_DCP405_R1B1) {
			ioexp.changeBit(IOExpander::DCP405_IO_BIT_OUT_CURRENT_RANGE_500MA, false);
		}

		if (channel.isOutputEnabled()) {
			if (channel.flags.currentCurrentRange == 0 || dac.isTesting()) {
				// 5A
				// DebugTrace("CH%d: Switched to 5A range", channel.channelIndex + 1);
				ioexp.changeBit(IOExpander::DCP405_IO_BIT_OUT_CURRENT_RANGE_5A, true);
				ioexp.changeBit(channel.boardRevision == CH_BOARD_REVISION_DCP405_R1B1 ?
					IOExpander::DCP405_IO_BIT_OUT_CURRENT_RANGE_50MA :
					IOExpander::DCP405_R2B5_IO_BIT_OUT_CURRENT_RANGE_50MA, false);
				// calculateNegligibleAdcDiffForCurrent();
			} else {
				// 50mA
				// DebugTrace("CH%d: Switched to 50mA range", channel.channelIndex + 1);
				ioexp.changeBit(channel.boardRevision == CH_BOARD_REVISION_DCP405_R1B1 ?
					IOExpander::DCP405_IO_BIT_OUT_CURRENT_RANGE_50MA :
					IOExpander::DCP405_R2B5_IO_BIT_OUT_CURRENT_RANGE_50MA, true);
				ioexp.changeBit(IOExpander::DCP405_IO_BIT_OUT_CURRENT_RANGE_5A, false);
				// calculateNegligibleAdcDiffForCurrent();
			}

			adc.start(ADC_DATA_TYPE_U_MON);
		} else {
			ioexp.changeBit(IOExpander::DCP405_IO_BIT_OUT_CURRENT_RANGE_5A, true);
			ioexp.changeBit(channel.boardRevision == CH_BOARD_REVISION_DCP405_R1B1 ?
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
			profile::enableSave(false);
			channel.setVoltage(uBeforeBalancing);
			profile::enableSave(true);
			uBeforeBalancing = NAN;
		}
	}

	void restoreCurrentToValueBeforeBalancing(psu::Channel &channel) {
		if (!isNaN(iBeforeBalancing)) {
			// DebugTrace("Restore current to value before balancing: %f", index, iBeforeBalancing);
			profile::enableSave(false);
            channel.setCurrent(iBeforeBalancing);
			profile::enableSave(true);
			iBeforeBalancing = NAN;
		}
	}

#if defined(EEZ_PLATFORM_STM32)
	void onSpiIrq() {
		uint8_t intcap = ioexp.readIntcapRegister();
		// DebugTrace("CH%d INTCAP 0x%02X\n", (int)(channel.channelIndex + 1), (int)intcap);
		if (!(intcap & (1 << IOExpander::DCP405_R2B5_IO_BIT_IN_OVP_FAULT))) {
			psu::Channel &channel = psu::Channel::getBySlotIndex(slotIndex);
			if (channel.isOutputEnabled()) {
				channel.enterOvpProtection();
			}
		}
	}
#endif
};

static Channel g_channel0(0);
static Channel g_channel1(1);
static Channel g_channel2(2);
ChannelInterface *g_channelInterfaces[NUM_SLOTS] = { &g_channel0, &g_channel1, &g_channel2 };

} // namespace dcpX05
} // namespace eez
