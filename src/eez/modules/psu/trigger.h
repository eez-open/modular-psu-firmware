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
namespace trigger {

static const float DELAY_DEFAULT = 0;
static const float DELAY_MIN = 0;
static const float DELAY_MAX = 3600.0f;

enum Source {
    SOURCE_BUS,
    SOURCE_IMMEDIATE,
    SOURCE_MANUAL,
    SOURCE_PIN1,
    SOURCE_PIN2
};

extern Source g_triggerSource;
extern float g_triggerDelay;
extern bool g_triggerContinuousInitializationEnabled;

void init();
void reset();

void setDelay(float delay);

void setSource(Source source);

void setVoltage(Channel &channel, float value);
float getVoltage(Channel &channel);

void setCurrent(Channel &channel, float value);
float getCurrent(Channel &channel);

enum State { STATE_IDLE, STATE_INITIATED, STATE_TRIGGERED, STATE_EXECUTING };
State getState();
bool isInitiated(Source source);
int generateTrigger(Source source, bool checkImmediatelly = true);
int startImmediately();
void startImmediatelyInPsuThread();
int initiate();
int enableInitiateContinuous(bool enable);
void setTriggerFinished(Channel &channel);
bool isIdle();
bool isInitiated();
bool isTriggered();
bool isActive();
void abort();

void tick();

}
}
} // namespace eez::psu::trigger