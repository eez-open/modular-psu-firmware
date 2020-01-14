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

#include <eez/system.h>

#if defined(EEZ_PLATFORM_STM32)
#include <eez/platform/stm32/spi.h>
#endif

#include <eez/modules/psu/psu.h>

#include <eez/modules/dcpX05/dac.h>
#include <eez/modules/dcpX05/adc.h>
#include <eez/modules/dcpX05/ioexp.h>

namespace eez {
namespace psu {

////////////////////////////////////////////////////////////////////////////////

#if defined(EEZ_PLATFORM_STM32)
static const uint8_t DATA_BUFFER_A = 0B00010000;
static const uint8_t DATA_BUFFER_B = 0B00100100;
#endif

static const uint16_t DAC_MIN = 0;
static const uint16_t DAC_MAX = (1L << DAC_RES) - 1;

#if defined(EEZ_PLATFORM_SIMULATOR)
extern float g_uSet[CH_MAX];
extern float g_iSet[CH_MAX];
#endif

////////////////////////////////////////////////////////////////////////////////

void DigitalAnalogConverter::init() {
}

bool DigitalAnalogConverter::test(IOExpander &ioexp, AnalogDigitalConverter &adc) {
#if defined(EEZ_PLATFORM_STM32)
    Channel &channel = Channel::getBySlotIndex(slotIndex);

    if (ioexp.g_testResult != TEST_OK) {
        // DebugTrace("Ch%d DAC test skipped because of IO expander", channel.channelIndex + 1);
        g_testResult = TEST_SKIPPED;
        return true;
    }

    if (adc.g_testResult != TEST_OK) {
        // DebugTrace("Ch%d DAC test skipped because of ADC", channel.channelIndex + 1);
        g_testResult = TEST_SKIPPED;
        return true;
    }

    m_testing = true;

    channel.calibrationEnableNoEvent(false);

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
        g_testResult = TEST_FAILED;
        DebugTrace("Ch%d DAC test, U_set failure: expected=%g, got=%g, abs diff=%g\n", channel.channelIndex + 1, uSet, uMon, uDiff);
    }

    float iMon = channel.i.mon_dac_last;
    float iDiff = iMon - iSet;
    if (fabsf(iDiff) > iSet * DAC_TEST_TOLERANCE / 100) {
        g_testResult = TEST_FAILED;
        DebugTrace("Ch%d DAC test, I_set failure: expected=%g, got=%g, abs diff=%g\n", channel.channelIndex + 1, iSet, iMon, iDiff);
    }

    if (g_testResult == TEST_FAILED) {
        generateError(SCPI_ERROR_CH1_DAC_TEST_FAILED + channel.channelIndex);
    } else {
        g_testResult = TEST_OK;
    }

    m_testing = false;
#endif

#if defined(EEZ_PLATFORM_SIMULATOR)
    g_testResult = TEST_OK;
#endif

    return g_testResult != TEST_FAILED;
}

////////////////////////////////////////////////////////////////////////////////

void DigitalAnalogConverter::setVoltage(float value) {
    Channel &channel = Channel::getBySlotIndex(slotIndex);

#if defined(EEZ_PLATFORM_STM32)
    set(DATA_BUFFER_B, remap(value, channel.params.U_MIN, (float)DAC_MIN, channel.params.U_MAX, (float)DAC_MAX));
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
    Channel &channel = Channel::getBySlotIndex(slotIndex);
    g_uSet[channel.channelIndex] = remap(value, (float)DAC_MIN, channel.params.U_MIN, (float)DAC_MAX, channel.params.U_MAX);
#endif
}

void DigitalAnalogConverter::setCurrent(float value) {
    Channel &channel = Channel::getBySlotIndex(slotIndex);

#if defined(EEZ_PLATFORM_STM32)
    set(DATA_BUFFER_A, remap(value, channel.params.I_MIN, (float)DAC_MIN, channel.getDualRangeMax(), (float)DAC_MAX));
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
    Channel &channel = Channel::getBySlotIndex(slotIndex);
    g_iSet[channel.channelIndex] = remap(value, (float)DAC_MIN, channel.params.I_MIN, (float)DAC_MAX, channel.getDualRangeMax());
#endif
}

////////////////////////////////////////////////////////////////////////////////

#if defined(EEZ_PLATFORM_STM32)

void DigitalAnalogConverter::set(uint8_t buffer, uint16_t value) {
    Channel &channel = Channel::getBySlotIndex(slotIndex);

#ifdef DEBUG
    if (buffer == DATA_BUFFER_B) {
        debug::g_uDac[channel.channelIndex].set(value);
    } else {
        debug::g_iDac[channel.channelIndex].set(value);
    }
#endif

    uint8_t data[3];
    uint8_t result[3];

    data[0] = buffer;
    data[1] = value >> 8;
    data[2] = value & 0xFF;

    spi::select(channel.slotIndex, spi::CHIP_DAC);
    spi::transfer(channel.slotIndex, data, result, 3);
    spi::deselect(channel.slotIndex);
}

void DigitalAnalogConverter::set(uint8_t buffer, float value) {
    set(buffer, (uint16_t)clamp(round(value), DAC_MIN, DAC_MAX));
}

#endif

} // namespace psu
} // namespace eez
