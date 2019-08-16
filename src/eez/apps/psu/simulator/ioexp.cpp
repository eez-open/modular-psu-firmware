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

#include <eez/apps/psu/psu.h>

#include <eez/apps/psu/ioexp.h>

namespace eez {
namespace psu {

////////////////////////////////////////////////////////////////////////////////

IOExpander::IOExpander(Channel &channel_) : channel(channel_) {
    g_testResult = TEST_SKIPPED;

    gpio = 0B0000000100000000; // 5A
}

void IOExpander::init() {
}

bool IOExpander::test() {
    g_testResult = TEST_OK;
    channel.flags.powerOk = 1;
    return g_testResult != TEST_FAILED;
}

void IOExpander::tick(uint32_t tick_usec) {
    if (simulator::getPwrgood(channel.index - 1)) {
        gpio |= 1 << IOExpander::IO_BIT_IN_PWRGOOD;
    } else {
        gpio &= ~(1 << IOExpander::IO_BIT_IN_PWRGOOD);
    }

    if (channel.getFeatures() & CH_FEATURE_RPOL) {
        if (!simulator::getRPol(channel.index - 1)) {
            gpio |= 1 << IOExpander::IO_BIT_IN_RPOL;
        } else {
            gpio &= ~(1 << IOExpander::IO_BIT_IN_RPOL);
        }
    }

    if (simulator::getCV(channel.index - 1)) {
        gpio |= 1 << IOExpander::IO_BIT_IN_CV_ACTIVE;
    } else {
        gpio &= ~(1 << IOExpander::IO_BIT_IN_CV_ACTIVE);
    }

    if (simulator::getCC(channel.index - 1)) {
        gpio |= 1 << IOExpander::IO_BIT_IN_CC_ACTIVE;
    } else {
        gpio &= ~(1 << IOExpander::IO_BIT_IN_CC_ACTIVE);
    }
}

bool IOExpander::testBit(int io_bit) {
    return gpio & (1 << io_bit) ? true : false;
}

void IOExpander::changeBit(int io_bit, bool set) {
    gpio = set ? (gpio | (1 << io_bit)) : (gpio & ~(1 << io_bit));
}

void IOExpander::readAllRegisters(uint8_t registers[]) {
}

} // namespace psu
} // namespace eez
