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

namespace eez {
namespace psu {

class IOExpander;
class AnalogDigitalConverter;

/// Digital to analog converter HW used by the channel.
class DigitalAnalogConverter {
public:
    static const uint16_t DAC_MIN = 0;
    static const uint16_t DAC_MAX = (1L << DAC_RES) - 1;

    uint8_t slotIndex;
    uint8_t channelIndex;
    TestResult testResult;

    void init();
    bool test(IOExpander &ioexp, AnalogDigitalConverter &adc);

    void tick();

    enum RampOption {
        NO_RAMP,
        WITH_RAMP,
        FROM_RAMP
    };

    void setVoltage(float voltage, RampOption rampOption = NO_RAMP);
    void setDacVoltage(uint16_t voltage);

    void setCurrent(float voltage, RampOption rampOption = NO_RAMP);
    void setDacCurrent(uint16_t current);

    bool isTesting() {
        return m_testing;
    }

    bool isOverHwOvpThreshold();

    bool isRampActive() {
        return m_uIsRampActive|| m_iIsRampActive;
    }

private:
    bool m_testing;

    // U ramp
    bool m_uIsRampActive = false;
    uint16_t m_uRampLastValue;
    uint16_t m_uRampTargetValue;
    uint32_t m_uRampStartTimeUsec;

    // I ramp
    bool m_iIsRampActive = false;
    uint16_t m_iRampLastValue;
    uint16_t m_iRampTargetValue;
    uint32_t m_iRampStartTimeUsec;

#if defined(EEZ_PLATFORM_STM32)
    void set(uint8_t buffer, uint16_t value, RampOption rampOption = NO_RAMP);
    void set(uint8_t buffer, float value);
#endif
};

} // namespace psu
} // namespace eez
