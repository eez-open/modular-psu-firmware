/*
 * EEZ Modular Firmware
 * Copyright (C) 2020-present, Envox d.o.o.
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

#include <eez/util.h>

#include <eez/modules/psu/psu.h>
#include <eez/modules/psu/channel_dispatcher.h>
#include <eez/modules/psu/ramp.h>
#include <eez/modules/psu/trigger.h>

namespace eez {
namespace psu {
namespace ramp {

static struct {
    int state;
    uint32_t startTime;
    bool voltageRampDone;
    bool currentRampDone;
} g_execution[CH_MAX];

int g_numChannelsWithVisibleCounters;
int g_channelsWithVisibleCounters[CH_MAX];

static bool g_active;

static void setActive(bool active, bool forceUpdate = false);

void executionStart(Channel &channel) {
    g_execution[channel.channelIndex].state = 1;
    channel_dispatcher::setVoltage(channel, 0);
    channel_dispatcher::setCurrent(channel, 0);
    setActive(true, true);
}

void tick(uint32_t tickUsec) {
    bool active = false;

    auto tickMs = tickUsec / 1000;

    for (int i = 0; i < CH_NUM; i++) {
        if (g_execution[i].state) {
            auto &channel = Channel::get(i);
            if (channel.isOutputEnabled()) {
                if (g_execution[i].state == 1) {
                    g_execution[i].startTime = tickMs;
                    g_execution[i].state = 2;
                }

                if (g_execution[i].state == 2) {
                    if (!channel.flags.outputDelayState || tickMs >= g_execution[i].startTime + channel.outputDelayDuration * 1000) {
                        g_execution[i].state = 3;
                        g_execution[i].voltageRampDone = false;
                        g_execution[i].currentRampDone = false;
                    }
                }

                if (g_execution[i].state == 3) {
                    if (channel.u.rampState && tickMs < g_execution[i].startTime + (channel.outputDelayDuration + channel.u.rampDuration) * 1000) {
                        channel_dispatcher::setVoltage(channel, channel.u.triggerLevel * ((tickMs - g_execution[i].startTime) / 1000.0f - channel.outputDelayDuration) / channel.u.rampDuration);
                    } else if (!g_execution[i].voltageRampDone) {
                        channel_dispatcher::setVoltage(channel, channel.u.triggerLevel);
                        g_execution[i].voltageRampDone = true;
                    }

                    if (channel.i.rampState && tickMs < g_execution[i].startTime + (channel.outputDelayDuration + channel.i.rampDuration) * 1000) {
                        channel_dispatcher::setCurrent(channel, channel.i.triggerLevel * ((tickMs - g_execution[i].startTime) / 1000.0f - channel.outputDelayDuration) / channel.i.rampDuration);
                    } else if (!g_execution[i].currentRampDone) {
                        channel_dispatcher::setCurrent(channel, channel.i.triggerLevel);
                        g_execution[i].currentRampDone = true;
                    }
                    
                    if (g_execution[i].voltageRampDone && g_execution[i].currentRampDone) {
                        trigger::setTriggerFinished(channel);
                        g_execution[i].state = 0;
                    }
                }
            }

            if (g_execution[i].state) {
                active = true;
            }
        }
    }

    if (active != g_active) {
        setActive(active);
    }
}

bool isActive() {
    return g_active;
}

bool isActive(Channel &channel) {
    return g_execution[channel.channelIndex].state != 0;
}

void abort() {
    bool sync = false;
    for (int i = 0; i < CH_NUM; ++i) {
        if (g_execution[i].state != 0) {
            g_execution[i].state = 0;
            channel_dispatcher::outputEnableOnNextSync(Channel::get(i), false);
            sync = true;
        }
    }
    if (sync) {
        channel_dispatcher::syncOutputEnable();
    }

    setActive(false, true);
}

void getCountdownTime(int channelIndex, uint32_t &remaining, uint32_t &total) {
    if (g_execution[channelIndex].state) {
        auto &channel = Channel::get(channelIndex);
        float duration = channel.outputDelayDuration + MAX(channel.u.rampDuration, channel.i.rampDuration);
        total = (uint32_t)roundf(duration);
        if (g_execution[channelIndex].state == 1) {
            remaining = total;
        } else {
            remaining = (uint32_t)roundf((g_execution[channelIndex].startTime + duration * 1000.0f - millis()) / 1000.0f);
        }
    } else {
        remaining = 0;
        total = 0;
    }
}

static void updateChannelsWithVisibleCountersList() {
    g_numChannelsWithVisibleCounters = 0;
    for (int channelIndex = 0; channelIndex < CH_NUM; channelIndex++) {
        uint32_t remaining;
        uint32_t total;
        getCountdownTime(channelIndex, remaining, total);
        if (total >= CONF_RAMP_COUNDOWN_DISPLAY_THRESHOLD) {
            g_channelsWithVisibleCounters[g_numChannelsWithVisibleCounters++] = channelIndex;
        }
    }
}

static void setActive(bool active, bool forceUpdate) {
    if (g_active != active) {
        g_active = active;
        updateChannelsWithVisibleCountersList();
    } else {
        if (forceUpdate) {
            updateChannelsWithVisibleCountersList();
        }
    }
}

}
}
} // namespace eez::psu::ramp
