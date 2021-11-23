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

#include <math.h>

#include <scpi/scpi.h>

#include <bb3/system.h>

#if defined(EEZ_PLATFORM_STM32)
#include <bb3/platform/stm32/spi.h>
#endif

#include <bb3/psu/psu.h>
#include <bb3/psu/ramp.h>

#include <bb3/dib-dcp405/dib-dcp405.h>
#include <bb3/dib-dcp405/dac.h>
#include <bb3/dib-dcp405/adc.h>
#include <bb3/dib-dcp405/ioexp.h>

namespace eez {
namespace psu {

////////////////////////////////////////////////////////////////////////////////

#if defined(EEZ_PLATFORM_STM32)

static const uint8_t DATA_BUFFER_A = 0B00010000;
static const uint8_t DATA_BUFFER_B = 0B00100100;

#if CONF_SURVIVE_MODE
static const uint32_t CONF_U_RAMP_DURATION_USEC = 4000; // 4 ms
static const uint32_t CONF_I_RAMP_DURATION_USEC = 300000; // 300 ms
#else
static const uint32_t CONF_U_RAMP_DURATION_USEC = 2000; // 2 ms
static const uint32_t CONF_I_RAMP_DURATION_USEC = 2000; // 2 ms
#endif

#endif // EEZ_PLATFORM_STM32

#if defined(EEZ_PLATFORM_SIMULATOR)
extern float g_uSet[CH_MAX];
extern float g_iSet[CH_MAX];
#endif

////////////////////////////////////////////////////////////////////////////////

void DigitalAnalogConverter::init() {
}

bool DigitalAnalogConverter::test(IOExpander &ioexp, AnalogDigitalConverter &adc) {
    testResult = TEST_OK;

#if defined(EEZ_PLATFORM_STM32)
    if (ioexp.testResult != TEST_OK) {
        testResult = TEST_SKIPPED;
        return true;
    }

    if (adc.testResult != TEST_OK) {
        testResult = TEST_SKIPPED;
        return true;
    }

    m_testing = true;

    auto &channel = Channel::get(channelIndex);

    channel.calibrationEnableNoEvent(false);
    channel.setCurrentRangeSelectionMode(CURRENT_RANGE_SELECTION_USE_BOTH);
    channel.setCurrentRange(CURRENT_RANGE_HIGH);
    channel.setCurrentLimit(5.0f);

    // set U on DAC and check it on ADC
    float uSet = channel.u.max / 2;
    float iSet = channel.i.max / 2;

    channel.setVoltage(uSet);
    channel.setCurrent(iSet);

    channel.adcMeasureMonDac();

    channel.setVoltage(0);
    channel.setCurrent(0);

    float uMon = channel.u.mon_dac_last;
    float uDiff = uMon - uSet;
    if (fabsf(uDiff) > uSet * DAC_TEST_TOLERANCE / 100) {
        testResult = TEST_FAILED;
        DebugTrace("Ch%d DAC test, U_set failure: expected=%g, got=%g, abs diff=%g\n", channel.channelIndex + 1, uSet, uMon, uDiff);
    }

    float iMon = channel.i.mon_dac_last;
    float iDiff = iMon - iSet;
    if (fabsf(iDiff) > iSet * DAC_TEST_TOLERANCE / 100) {
        testResult = TEST_FAILED;
        DebugTrace("Ch%d DAC test, I_set failure: expected=%g, got=%g, abs diff=%g\n", channel.channelIndex + 1, iSet, iMon, iDiff);
    }

    if (testResult == TEST_FAILED) {
        generateChannelError(SCPI_ERROR_CH1_DAC_TEST_FAILED, channel.channelIndex);
    } else {
        testResult = TEST_OK;
    }

    m_testing = false;
#endif

#if defined(EEZ_PLATFORM_SIMULATOR)
    testResult = TEST_OK;
#endif

    return testResult != TEST_FAILED;
}

void DigitalAnalogConverter::tick() {
#if defined(EEZ_PLATFORM_STM32)
    if (m_uIsRampActive) {
        uint16_t value;

        uint32_t tickCountUsec = micros();

        uint32_t diff = tickCountUsec - m_uRampStartTimeUsec;
        if (diff >= CONF_U_RAMP_DURATION_USEC) {
            value = m_uRampTargetValue;
            m_uIsRampActive = false;
        } else {
            value = m_uRampTargetValue * diff / CONF_U_RAMP_DURATION_USEC;
        }
        
        set(DATA_BUFFER_B, value, FROM_RAMP);
    }

    if (m_iIsRampActive) {
        uint16_t value;

        uint32_t tickCountUsec = micros();

        uint32_t diff = tickCountUsec - m_iRampStartTimeUsec;
        if (diff >= CONF_I_RAMP_DURATION_USEC) {
            value = m_iRampTargetValue;
            m_iIsRampActive = false;
        } else {
            value = m_iRampTargetValue * diff / CONF_I_RAMP_DURATION_USEC;
        }
        
        set(DATA_BUFFER_A, value, FROM_RAMP);
    }
#endif // EEZ_PLATFORM_STM32
}

bool DigitalAnalogConverter::isOverHwOvpThreshold() {
    static const float CONF_OVP_HW_VOLTAGE_THRESHOLD = 1.0f;
    Channel &channel = Channel::get(channelIndex);
    float u_set = remap(m_uRampLastValue, (float)DAC_MIN, channel.params.U_MIN, (float)DAC_MAX, channel.params.U_MAX);
    return u_set > channel.getCalibratedVoltage(CONF_OVP_HW_VOLTAGE_THRESHOLD);
}

////////////////////////////////////////////////////////////////////////////////

void DigitalAnalogConverter::setVoltage(float value, RampOption rampOption) {
    Channel &channel = Channel::get(channelIndex);

#if defined(EEZ_PLATFORM_STM32)
    set(DATA_BUFFER_B, clamp(remap(value, channel.params.U_MIN, (float)DAC_MIN, channel.params.U_MAX, (float)DAC_MAX), (float)DAC_MIN, (float)DAC_MAX), rampOption);
#endif

#if defined(EEZ_PLATFORM_SIMULATOR)
    g_uSet[channel.channelIndex] = value;
#endif
}

void DigitalAnalogConverter::setDacVoltage(uint16_t value) {
#if defined(EEZ_PLATFORM_STM32)
    set(DATA_BUFFER_B, value);
#endif

#if defined(EEZ_PLATFORM_SIMULATOR)
    Channel &channel = Channel::get(channelIndex);
    g_uSet[channel.channelIndex] = remap(value, (float)DAC_MIN, channel.params.U_MIN, (float)DAC_MAX, channel.params.U_MAX);
#endif
}

void DigitalAnalogConverter::setCurrent(float value, RampOption rampOption) {
    Channel &channel = Channel::get(channelIndex);

#if defined(EEZ_PLATFORM_STM32)
    set(DATA_BUFFER_A, clamp(remap(value, channel.params.I_MIN, (float)DAC_MIN, channel.getDualRangeMax(), (float)DAC_MAX), (float)DAC_MIN, (float)DAC_MAX), rampOption);
#endif

#if defined(EEZ_PLATFORM_SIMULATOR)
    g_iSet[channel.channelIndex] = value;
#endif
}

void DigitalAnalogConverter::setDacCurrent(uint16_t value) {
#if defined(EEZ_PLATFORM_STM32)
    set(DATA_BUFFER_A, value);
#endif

#if defined(EEZ_PLATFORM_SIMULATOR)
    Channel &channel = Channel::get(channelIndex);
    g_iSet[channel.channelIndex] = remap(value, (float)DAC_MIN, channel.params.I_MIN, (float)DAC_MAX, channel.getDualRangeMax());
#endif
}

////////////////////////////////////////////////////////////////////////////////

#if defined(EEZ_PLATFORM_STM32)

void DigitalAnalogConverter::set(uint8_t buffer, uint16_t value, RampOption rampOption) {
    Channel &channel = Channel::get(channelIndex);

    if (buffer == DATA_BUFFER_B) {
        if (
            rampOption == WITH_RAMP && 
            !ramp::isActive(channel)
        ) {
            m_uIsRampActive = true;
            m_uRampTargetValue = value;
            value = 0;
            rampOption = FROM_RAMP;
            m_uRampStartTimeUsec = micros();
        }

        m_uRampLastValue = value;
        
        if (rampOption != FROM_RAMP) {
            m_uIsRampActive = false;
        }

        m_uLastValue = value;
    } 
    else {
        if (
            rampOption == WITH_RAMP && 
            !ramp::isActive(channel)
        ) {
            m_iIsRampActive = true;
            m_iRampTargetValue = value;
            value = 0;
            rampOption = FROM_RAMP;
            m_iRampStartTimeUsec = micros();
        }

        m_iRampLastValue = value;
        
        if (rampOption != FROM_RAMP) {
            m_iIsRampActive = false;
        }

        m_iLastValue = value;
    }

    uint8_t data[3];
    uint8_t result[3];

    data[0] = buffer;
    data[1] = value >> 8;
    data[2] = value & 0xFF;

	if (g_isBooted && !isPsuThread()) {
        DebugTrace("wrong thread 6\n");
    }

	spi::select(slotIndex, spi::CHIP_DAC);
    spi::transfer3(slotIndex, data, result);
    spi::deselect(slotIndex);
}

void DigitalAnalogConverter::set(uint8_t buffer, float value) {
    set(buffer, (uint16_t)clamp(round(value), DAC_MIN, DAC_MAX));
}

#endif

} // namespace psu
} // namespace eez
