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

#include <eez/modules/psu/psu.h>

#include <eez/modules/psu/channel_dispatcher.h>
#include <eez/modules/psu/io_pins.h>
#include <eez/modules/psu/list_program.h>
#include <eez/modules/psu/persist_conf.h>
#include <eez/modules/psu/profile.h>
#include <eez/modules/psu/ramp.h>
#include <eez/function_generator.h>
#include <eez/modules/psu/trigger.h>
#include <eez/scpi/regs.h>
#include <eez/system.h>

#include <eez/modules/psu/dlog_record.h>

namespace eez {
namespace psu {
namespace trigger {

Source g_triggerSource;
float g_triggerDelay;
bool g_triggerContinuousInitializationEnabled;

static State g_state;
static uint32_t g_triggeredTime;

bool g_triggerInProgress[CH_MAX];

void setState(State newState) {
    if (g_state != newState) {
        if (newState == STATE_INITIATED) {
            for (int i = 0; i < CH_NUM; ++i) {
                Channel &channel = Channel::get(i);
                channel.setOperBits(OPER_ISUM_TRIG, true);
            }
        }

        if (g_state == STATE_INITIATED) {
            for (int i = 0; i < CH_NUM; ++i) {
                Channel &channel = Channel::get(i);
                channel.setOperBits(OPER_ISUM_TRIG, false);
            }
        }

        g_state = newState;
    }
}

void reset() {
	abort();

    g_triggerDelay = trigger::DELAY_DEFAULT;
    setSource(trigger::SOURCE_IMMEDIATE);
    g_triggerContinuousInitializationEnabled = 0;

    setState(STATE_IDLE);
}

void init() {
    setState(STATE_IDLE);

    if (g_triggerContinuousInitializationEnabled) {
        initiate();
    }
}

void setDelay(float delay) {
    g_triggerDelay = delay;
}

void setSource(Source triggerSource) {
    g_triggerSource = triggerSource;

    if (triggerSource == trigger::SOURCE_PIN1) {
        if (io_pins::g_ioPins[0].function != io_pins::FUNCTION_SYSTRIG) {
            io_pins::setPinFunction(0, io_pins::FUNCTION_SYSTRIG);
        }
    } else if (triggerSource == trigger::SOURCE_PIN2) {
        if (io_pins::g_ioPins[1].function != io_pins::FUNCTION_SYSTRIG) {
            io_pins::setPinFunction(1, io_pins::FUNCTION_SYSTRIG);
        }
    } else {
        if (io_pins::g_ioPins[0].function == io_pins::FUNCTION_SYSTRIG) {
            io_pins::setPinFunction(0, io_pins::FUNCTION_NONE);
        } else if (io_pins::g_ioPins[1].function == io_pins::FUNCTION_SYSTRIG) {
            io_pins::setPinFunction(1, io_pins::FUNCTION_NONE);
        }
    }
}

void check(uint32_t currentTime) {
    if (currentTime - g_triggeredTime > g_triggerDelay * 1000L) {
        startImmediately();
    }
}

bool isSeqInitiated(Source source) {
    return g_triggerSource == source && g_state == STATE_INITIATED;

}

bool isDlogInitiated(Source source) {
    return dlog_record::g_recordingParameters.triggerSource == source && dlog_record::isInitiated();

}

bool isInitiated(Source source) {
    return isSeqInitiated(source) || isDlogInitiated(source);
}

int generateTrigger(Source source, bool checkImmediatelly) {
    bool seqInitiated = isSeqInitiated(source);
    bool dlogInitiated = isDlogInitiated(source);

    if (!seqInitiated && !dlogInitiated) {
        return SCPI_ERROR_TRIGGER_IGNORED;
    }

    if (dlogInitiated) {
        dlog_record::triggerGenerated();
    }

    if (seqInitiated) {
        setState(STATE_TRIGGERED);

        g_triggeredTime = millis();

        if (checkImmediatelly) {
            check(g_triggeredTime);
        }
    }

    return SCPI_RES_OK;
}

bool isTriggerFinishedOnAllChannels() {
    for (int i = 0; i < CH_NUM; ++i) {
        if (g_triggerInProgress[i]) {
            return false;
        }
    }
    return true;
}

void triggerFinished() {
    if (g_triggerContinuousInitializationEnabled) {
        setState(STATE_INITIATED);
    } else {
        setState(STATE_IDLE);
    }
}

void onTriggerFinished(Channel &channel) {
    if (channel.getVoltageTriggerMode() == TRIGGER_MODE_LIST) {
        int err;

        switch (channel.getTriggerOnListStop()) {
        case TRIGGER_ON_LIST_STOP_OUTPUT_OFF:
            channel_dispatcher::setVoltage(channel, 0);
            channel_dispatcher::setCurrent(channel, 0);
            channel_dispatcher::outputEnable(channel, false);
            break;
        case TRIGGER_ON_LIST_STOP_SET_TO_FIRST_STEP:
            if (!list::setListValue(channel, 0, &err)) {
                generateError(err);
            }
            break;
        case TRIGGER_ON_LIST_STOP_SET_TO_LAST_STEP:
            if (!list::setListValue(channel, list::maxListsSize(channel) - 1, &err)) {
                generateError(err);
            }
            break;
        case TRIGGER_ON_LIST_STOP_STANDBY:
            channel_dispatcher::setVoltage(channel, 0);
            channel_dispatcher::setCurrent(channel, 0);
            channel_dispatcher::outputEnable(channel, false);
            changePowerState(false);
            break;
        }
    }
}

void setTriggerFinished(Channel &channel) {
    if (channel.channelIndex < 2 && (channel_dispatcher::getCouplingType() == channel_dispatcher::COUPLING_TYPE_SERIES || channel_dispatcher::getCouplingType() == channel_dispatcher::COUPLING_TYPE_PARALLEL)) {
        g_triggerInProgress[0] = false;
        g_triggerInProgress[1] = false;
    } else if (channel.flags.trackingEnabled) {
        for (int i = 0; i < CH_NUM; ++i) {
            if (Channel::get(i).flags.trackingEnabled) {
                g_triggerInProgress[i] = false;
            }
        }
    } else {
        g_triggerInProgress[channel.channelIndex] = false;
    }

    onTriggerFinished(channel);

    if (isTriggerFinishedOnAllChannels() && !function_generator::isActive()) {
        triggerFinished();
    }
}

int checkTrigger() {
    bool onlyFixed = true;
    
    bool trackingChannelsChecked = false;

    for (int i = 0; i < CH_NUM; ++i) {
        Channel &channel = Channel::get(i);

        if (!channel.isOk()) {
            continue;
        }

        if (i == 1 && (channel_dispatcher::getCouplingType() == channel_dispatcher::COUPLING_TYPE_PARALLEL || channel_dispatcher::getCouplingType() == channel_dispatcher::COUPLING_TYPE_SERIES)) {
            continue;
        }

        if (channel.flags.trackingEnabled) {
            if (trackingChannelsChecked) {
                continue;
            }
            trackingChannelsChecked = true;
        }

        if (channel.getVoltageTriggerMode() != channel.getCurrentTriggerMode()) {
            return SCPI_ERROR_INCOMPATIBLE_TRANSIENT_MODES;
        }

        if (channel.getVoltageTriggerMode() != TRIGGER_MODE_FIXED) {
            if (channel.isRemoteProgrammingEnabled()) {
                return SCPI_ERROR_CANNOT_INIT_TRIGGER_WHILE_RPROG_IS_ENABLED;
            }

            if (channel.getVoltageTriggerMode() == TRIGGER_MODE_LIST) {
                if (list::isListEmpty(channel)) {
                    g_errorChannelIndex = channel.channelIndex;
                    return SCPI_ERROR_LIST_IS_EMPTY;
                }

                if (!list::areListLengthsEquivalent(channel)) {
                    return SCPI_ERROR_LIST_LENGTHS_NOT_EQUIVALENT;
                }

                int err = list::checkLimits(i);
                if (err) {
                    return err;
                }
            } else if (channel.getVoltageTriggerMode() == TRIGGER_MODE_STEP) {
                if (channel.isVoltageLimitExceeded(channel.u.triggerLevel)) {
                    g_errorChannelIndex = channel.channelIndex;
                    return SCPI_ERROR_VOLTAGE_LIMIT_EXCEEDED;
                }

                if (channel.isCurrentLimitExceeded(channel.i.triggerLevel)) {
                    g_errorChannelIndex = channel.channelIndex;
                    return SCPI_ERROR_CURRENT_LIMIT_EXCEEDED;
                }

                int err;
                if (channel.isPowerLimitExceeded(channel.u.triggerLevel, channel.i.triggerLevel, &err)) {
                    g_errorChannelIndex = channel.channelIndex;
                    return err;
                }
            } else if (channel.getVoltageTriggerMode() == TRIGGER_MODE_FUNCTION_GENERATOR) {
                int err = function_generator::checkLimits(i);
                if (err) {
                    return err;
                }
            }

            onlyFixed = false;
        }
    }

    if (function_generator::getNumChannelsInFunctionGeneratorTriggerMode() > 0) {
        onlyFixed = false;
    }

    if (onlyFixed) {
        return SCPI_ERROR_CANNOT_INITIATE_WHILE_IN_FIXED_MODE;
    }

    return 0;
}

int startImmediately() {
    int err = checkTrigger();
    if (err) {
        return err;
    }

    setState(STATE_EXECUTING);
    for (int i = 0; i < CH_NUM; ++i) {
        Channel &channel = Channel::get(i);
        if (channel.isOk()) {
            g_triggerInProgress[i] = true;
        }
    }

    io_pins::onTrigger();

    if (!isPsuThread()) {
    	sendMessageToPsu(PSU_MESSAGE_TRIGGER_START_IMMEDIATELY, 0, 0);
    } else {
        startImmediatelyInPsuThread();
    }

    return SCPI_RES_OK;
}

void startImmediatelyInPsuThread() {
    bool trackingChannelsStarted = false;

	function_generator::executionStart();

    for (int i = 0; i < CH_NUM; ++i) {
        Channel &channel = Channel::get(i);

        if (!channel.isOk()) {
            continue;
        }

        if (i == 1 && (channel_dispatcher::getCouplingType() == channel_dispatcher::COUPLING_TYPE_PARALLEL || channel_dispatcher::getCouplingType() == channel_dispatcher::COUPLING_TYPE_SERIES)) {
            continue;
        }

        if (channel.flags.trackingEnabled) {
            if (trackingChannelsStarted) {
                continue;
            }
            trackingChannelsStarted = true;
        }

        if (channel.getVoltageTriggerMode() == TRIGGER_MODE_LIST) {
            list::executionStart(channel);
            
            if (list::isActive()) {
                channel_dispatcher::outputEnableOnNextSync(channel, channel_dispatcher::getTriggerOutputState(channel));
            }
        } else if (channel.getVoltageTriggerMode() == TRIGGER_MODE_STEP) {
            ramp::executionStart(channel);
            channel_dispatcher::outputEnableOnNextSync(channel, channel_dispatcher::getTriggerOutputState(channel));
        } else if (channel.getVoltageTriggerMode() == TRIGGER_MODE_FUNCTION_GENERATOR) {
            channel_dispatcher::outputEnableOnNextSync(channel, channel_dispatcher::getTriggerOutputState(channel));
        } else {
            setTriggerFinished(channel);
        }
    }

    channel_dispatcher::syncOutputEnable();
}

int initiate() {
    int err = checkTrigger();
    if (err) {
        return err;
    }

    setState(STATE_INITIATED);

    if (g_triggerSource == SOURCE_IMMEDIATE) {
        return trigger::generateTrigger(trigger::SOURCE_IMMEDIATE);
    }

    return SCPI_RES_OK;
}

int enableInitiateContinuous(bool enable) {
    g_triggerContinuousInitializationEnabled = enable;
    if (enable) {
        return initiate();
    } else {
        return SCPI_RES_OK;
    }
}

State getState() {
	return g_state;
}

bool isIdle() {
    return g_state == STATE_IDLE;
}

bool isInitiated() {
    return g_state == STATE_INITIATED;
}

bool isTriggered() {
    return g_state == STATE_TRIGGERED;
}

bool isActive() {
    return list::isActive() || ramp::isActive() || function_generator::isActive();
}

void abort() {
    if (!isPsuThread()) {
        sendMessageToPsu(PSU_MESSAGE_TRIGGER_ABORT, 0, 0);
    } else {
        list::abort();
        ramp::abort();
        function_generator::abort();

        bool sync = false;
        for (int i = 0; i < CH_NUM; ++i) {
            auto &channel = Channel::get(i);
            if ((channel_dispatcher::getVoltageTriggerMode(channel) != TRIGGER_MODE_FIXED || channel_dispatcher::getCurrentTriggerMode(channel) != TRIGGER_MODE_FIXED) && channel.isOutputEnabled()) {
                channel_dispatcher::outputEnableOnNextSync(channel, false);
                sync = true;
            }
        }
        if (sync) {
            channel_dispatcher::syncOutputEnable();
        }
        
        setState(STATE_IDLE);
    }
}

void tick() {
    if (g_state == STATE_TRIGGERED) {
        check(millis());
    }
}

} // namespace trigger
} // namespace psu
} // namespace eez
