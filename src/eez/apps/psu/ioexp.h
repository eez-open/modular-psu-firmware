/*
 * EEZ PSU Firmware
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

class Channel;

/// IO Expander HW used by the channel.
class IOExpander {
  public:
    static const uint8_t IO_BIT_IN_RPOL = 0; // remote sense reverse polarity detection
    static const uint8_t IO_BIT_IN_CC_ACTIVE = 1;
    static const uint8_t IO_BIT_IN_CV_ACTIVE = 2;
    static const uint8_t IO_BIT_IN_PWRGOOD = 3;

    static const uint8_t IO_BIT_OUT_DP_ENABLE = 8;
    static const uint8_t IO_BIT_OUT_OUTPUT_ENABLE = 9;
    static const uint8_t IO_BIT_OUT_REMOTE_SENSE = 10;
    static const uint8_t IO_BIT_OUT_REMOTE_PROGRAMMING = 11;

    // DPC505
    static const uint8_t DCP505_IO_BIT_IN_OVP_FAULT = 4;

    static const uint8_t DCP505_IO_BIT_OUT_OVP_ENABLE = 5;
    static const uint8_t DCP505_IO_BIT_OUT_OE_UNCOUPLED_LED = 6;
    static const uint8_t DCP505_IO_BIT_OUT_OE_COUPLED_LED = 7;

    static const uint8_t DCP505_IO_BIT_IN_CURRENT_RANGE_5A = 12;
    static const uint8_t DCP505_IO_BIT_IN_CURRENT_RANGE_500MA = 13;
    static const uint8_t DCP505_IO_BIT_IN_CURRENT_RANGE_50MA = 14;

    // DPC405
    static const uint8_t DCP405_IO_BIT_IN_ADC_DRDY = 4;

    static const uint8_t DCP405_IO_BIT_OUT_CURRENT_RANGE_50MA = 5;
    static const uint8_t DCP405_IO_BIT_OUT_CURRENT_RANGE_500MA = 6;
    static const uint8_t DCP405_IO_BIT_OUT_CURRENT_RANGE_5A = 7;

    static const uint8_t DCP405_IO_BIT_OUT_OVP_ENABLE = 12;
    static const uint8_t DCP405_IO_BIT_OUT_OVP_LE = 13;
    static const uint8_t DCP405_IO_BIT_OUT_OE_UNCOUPLED_LED = 14;
    static const uint8_t DCP405_IO_BIT_OUT_OE_COUPLED_LED = 15;
    
    TestResult g_testResult;

    IOExpander(Channel &channel);

    void init();
    bool test();

    void tick(uint32_t tick_usec);

    uint8_t readGpio();
    uint8_t readGpioB();

    int getBitDirection(int io_bit); // 0: output, 1: input
    bool testBit(int io_bit);
    void changeBit(int io_bit, bool set);

  private:
    Channel &channel;
    uint8_t gpioa;
    uint8_t gpiob;

    uint8_t read(uint8_t reg);
    void write(uint8_t reg, uint8_t val);
};

} // namespace psu
} // namespace eez
