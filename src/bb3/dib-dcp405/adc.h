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

#include <bb3/psu/psu.h>

namespace eez {
namespace psu {

struct Channel;

/// Analog to digital converter HW used by the channel.
class AnalogDigitalConverter {
public:
    static const uint16_t ADC_MIN = 0;
    static const uint16_t ADC_MAX = (1L << ADC_RES) - 1;

    uint8_t channelIndex;
    uint8_t slotIndex;
    TestResult testResult;
    
    AdcDataType adcDataType;

    int16_t m_uLastMonDac;
    int16_t m_iLastMonDac;

    void init();
    bool test();

    void start(AdcDataType adcDataType);
    float read();

    void readAllRegisters(uint8_t registers[]);

private:
    uint32_t start_time;

#if defined(EEZ_PLATFORM_STM32)
    uint8_t getReg1Val();
#endif
};

} // namespace psu
} // namespace eez
