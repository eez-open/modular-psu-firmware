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

/// Digital to analog converter HW used by the channel.
class DigitalAnalogConverter {
  public:
    uint8_t channelIndex;
    TestResult g_testResult;

    void init();
    bool test();

    void set_voltage(float voltage);
    void set_current(float voltage);

    void set_voltage(uint16_t voltage);
    void set_current(uint16_t current);

    bool isTesting() {
        return m_testing;
    }

  private:
    bool m_testing;

    void set(uint8_t buffer, uint16_t value);
    void set(uint8_t buffer, float value);
};

} // namespace psu
} // namespace eez
