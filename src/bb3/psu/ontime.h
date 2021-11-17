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

#include <bb3/psu/timer.h>

namespace eez {
namespace psu {
namespace ontime {

void counterToString(char *str, size_t count, uint32_t counterTime);

enum { 
    ON_TIME_COUNTER_MCU, 
    ON_TIME_COUNTER_SLOT1,
    ON_TIME_COUNTER_SLOT2,
    ON_TIME_COUNTER_SLOT3,
    OTHER_COUNTER
};

class Counter {
  public:
    Counter(int type);

    int getType();
    void setType(int type);

    bool isActive();

    void start();
    void stop();

    void init();
    void tick();

    uint32_t getTotalTime();
    uint32_t getLastTime();

  private:
    uint8_t typeAndIsActive;
    uint32_t totalTime;
    uint32_t lastTime;
    uint32_t lastTick;
    uint32_t fractionTime;
    Interval writeInterval;
};

extern ontime::Counter g_mcuCounter;
extern ontime::Counter g_moduleCounters[];

} // namespace ontime
} // namespace psu
} // namespace eez
