/* / mcu / sound.h
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

#include <assert.h>
#include <stdio.h>

#if defined(EEZ_PLATFORM_STM32)
#include <tim.h>
#endif

#include <eez/firmware.h>
#include <eez/system.h>
#include <eez/sound.h>
#include <eez/index.h>
#include <eez/util.h>
#include <eez/scripting/scripting.h>

#include <eez/scpi/scpi.h>

#include <eez/modules/psu/psu.h>
#include <eez/modules/psu/serial_psu.h>
#if OPTION_ETHERNET
#include <eez/modules/psu/ethernet.h>
#include <eez/modules/psu/ntp.h>
#endif
#include <eez/modules/psu/board.h>
#include <eez/modules/psu/datetime.h>
#include <eez/modules/psu/persist_conf.h>
#include <eez/modules/psu/rtc.h>
#include <eez/modules/psu/temperature.h>
#include <eez/modules/psu/calibration.h>
#include <eez/modules/psu/profile.h>
#include <eez/modules/psu/dlog_record.h>
#include <eez/modules/psu/sd_card.h>
#include <eez/modules/psu/channel_dispatcher.h>
#include <eez/modules/psu/event_queue.h>
#include <eez/modules/psu/io_pins.h>
#include <eez/modules/psu/list_program.h>
#include <eez/modules/psu/ramp.h>
#include <eez/function_generator.h>
#include <eez/modules/psu/trigger.h>
#include <eez/modules/psu/ontime.h>

#if OPTION_DISPLAY
#include <eez/modules/psu/gui/psu.h>
#endif

#if OPTION_FAN
#include <eez/modules/aux_ps/fan.h>
#endif

#include <eez/modules/mcu/battery.h>
#include <eez/modules/mcu/eeprom.h>

#include <eez/modules/dib-dcp405/dib-dcp405.h>
#include <eez/modules/dib-dcp405/ioexp.h>
#include <eez/modules/dib-dcp405/dac.h>
#include <eez/modules/dib-dcp405/adc.h>

#include <eez/modules/bp3c/io_exp.h>
#include <eez/modules/bp3c/eeprom.h>
#include <eez/modules/bp3c/flash_slave.h>

#if defined(EEZ_PLATFORM_SIMULATOR)

// for home directory (see getConfFilePath)
#ifdef _WIN32
#undef INPUT
#undef OUTPUT
#include <Shlobj.h>
#include <Windows.h>
#include <direct.h>
#else
#include <string.h>
#include <pwd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif

#endif

namespace eez {

using namespace scpi;

TestResult g_masterTestResult;
uint8_t g_numMasterErrors;
char g_masterErrorMessage[MASTER_ERROR_MESSAGE_SIZE];

#if defined(EEZ_PLATFORM_SIMULATOR)

char *getConfFilePath(const char *file_name) {
    static char file_path[1024];

    *file_path = 0;

#ifdef _WIN32
    if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_PROFILE, NULL, 0, file_path))) {
        stringAppendString(file_path, sizeof(file_path), "\\.eez_psu_sim");
        _mkdir(file_path);
        stringAppendString(file_path, sizeof(file_path), "\\");
    }
#elif defined(__EMSCRIPTEN__)
    stringAppendString(file_path, sizeof(file_path), "/eez_modular_firmware/");
#else
    const char *home_dir = 0;
    if ((home_dir = getenv("HOME")) == NULL) {
        home_dir = getpwuid(getuid())->pw_dir;
    }
    if (home_dir) {
        stringAppendString(file_path, sizeof(file_path), home_dir);
        stringAppendString(file_path, sizeof(file_path), "/.eez_psu_sim");
        mkdir(file_path, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
        stringAppendString(file_path, sizeof(file_path), "/");
    }
#endif

    char *q = file_path + strlen(file_path);
    const char *p = file_name;
    while (*p) {
        char ch = *p++;
#ifdef _WIN32
        if (ch == '/')
            *q++ = '\\';
#else
        if (ch == '\\')
            *q++ = '/';
#endif
        else
            *q++ = ch;
    }
    *q = 0;

    return file_path;
}

#endif

void generateError(int16_t error) {
    eez::scpi::generateError(error);
}

void generateChannelError(int16_t error, int channelIndex) {
    if (channelIndex < 6) {
        eez::scpi::generateError(error > 0 ? error + channelIndex : error - channelIndex);
    } else {
        // TODO !!??
        eez::scpi::generateError(error);
    }
}

namespace psu {

using namespace scpi;

}
} // namespacee eez::psu

#if defined(EEZ_PLATFORM_STM32)

extern "C" void PSU_IncTick() {
    g_tickCount++;

    using namespace eez;
    using namespace eez::psu;
    if (ramp::isActive() || eez::dcp405::isDacRampActive() || function_generator::isActive()) {
        sendMessageToPsu(PSU_MESSAGE_TICK, 0, 0);
    }
}

#endif

namespace eez {
namespace psu {

////////////////////////////////////////////////////////////////////////////////

static bool g_powerIsUp;
static bool g_testPowerUpDelay;
static uint32_t g_powerDownTime;

static MaxCurrentLimitCause g_maxCurrentLimitCause;

RLState g_rlState = RL_STATE_LOCAL;

bool g_rprogAlarm = false;

void (*g_diagCallback)();

bool g_adcMeasureAllFinished = false;

////////////////////////////////////////////////////////////////////////////////

void PsuModule::setEnabled(bool value) {
    if (enabled != value) {
        int channelIndex = Channel::getBySlotIndex(slotIndex)->channelIndex;

        if (enabled) {
            for (int i = channelIndex; i < channelIndex + numPowerChannels; i++) {
                Channel &channel = Channel::get(i);

                if (channel.isOk()) {
                    if (channel.channelIndex < 2 && (channel_dispatcher::getCouplingType() == channel_dispatcher::COUPLING_TYPE_SERIES || channel_dispatcher::getCouplingType() == channel_dispatcher::COUPLING_TYPE_PARALLEL)) {
                        channel_dispatcher::setCouplingType(channel_dispatcher::COUPLING_TYPE_NONE, nullptr);
                    }

                    channel.setVoltage(channel.params.U_MIN);
                    channel.setCurrent(channel.params.I_MIN);
                    channel_dispatcher::outputEnable(channel, false);
                    channel.onPowerDown();
                }
            }
        }

        Module::setEnabled(value);

        if (enabled) {
            initChannels();

            for (int i = channelIndex; i < channelIndex + numPowerChannels; i++) {
                Channel &channel = Channel::get(i);
                channel.test();

                if (channel.isOk()) {
                    channel.setVoltage(channel.params.U_MIN);
                    channel.setCurrent(channel.params.I_MIN);
                    channel_dispatcher::outputEnable(channel, false);
                }
            }
        }
    }
}

int PsuModule::getSlotSettingsPageId() {
    return eez::gui::PAGE_ID_CH_SETTINGS;
}

void PsuModule::resetPowerChannelProfileToDefaults(int channelIndex, uint8_t *buffer) {
    auto &channel = Channel::get(channelIndex);
    auto parameters = (PowerChannelProfileParameters *)buffer;

    parameters->flags.output_enabled = 0;
    parameters->flags.sense_enabled = 0;

    parameters->flags.rprog_enabled = 0;

    parameters->flags.u_state = channel.params.OVP_DEFAULT_STATE;
    if (channel.params.features & CH_FEATURE_HW_OVP) {
        parameters->flags.u_type = 1; // HW
    } else {
        parameters->flags.u_type = 0; // SW
    }    
    parameters->flags.i_state = channel.params.OCP_DEFAULT_STATE;
    parameters->flags.p_state = channel.params.OPP_DEFAULT_STATE;

    parameters->u_set = channel.params.U_MIN;
    parameters->u_step = channel.params.U_DEF_STEP;
    parameters->u_limit = channel.u.max;

    parameters->i_set = channel.params.I_MIN;
    parameters->i_step = channel.params.I_DEF_STEP;
    parameters->i_limit = channel.i.max;

    parameters->p_limit = channel.roundChannelValue(UNIT_WATT, channel.params.PTOT);

    parameters->u_delay = channel.params.OVP_DEFAULT_DELAY;
    parameters->u_level = channel.u.max;
    parameters->i_delay = channel.params.OCP_DEFAULT_DELAY;
    parameters->p_delay = channel.params.OPP_DEFAULT_DELAY;
    parameters->p_level = channel.params.OPP_DEFAULT_LEVEL;

    parameters->displayValues[0].type = DISPLAY_VALUE_VOLTAGE;
	parameters->displayValues[0].scale = DISPLAY_VALUE_SCALE_LIMIT;

	parameters->displayValues[1].type = DISPLAY_VALUE_CURRENT;
	parameters->displayValues[1].scale = DISPLAY_VALUE_SCALE_LIMIT;
	
	parameters->ytViewRate = GUI_YT_VIEW_RATE_DEFAULT;

#ifdef EEZ_PLATFORM_SIMULATOR
    parameters->load_enabled = 0;
    parameters->load = 0;
    parameters->voltProgExt = 0;
#endif

    parameters->flags.u_triggerMode = TRIGGER_MODE_FIXED;
    parameters->flags.i_triggerMode = TRIGGER_MODE_FIXED;
    parameters->flags.triggerOutputState = 1;
    parameters->flags.triggerOnListStop = TRIGGER_ON_LIST_STOP_OUTPUT_OFF;
    parameters->u_triggerValue = channel.params.U_MIN;
    parameters->i_triggerValue = channel.params.I_MIN;
    parameters->listCount = 0;

    parameters->flags.currentRangeSelectionMode = CURRENT_RANGE_SELECTION_USE_BOTH;
    parameters->flags.autoSelectCurrentRange = 0;

    parameters->flags.dprogState = 1;
    parameters->flags.trackingEnabled = 0;

    parameters->u_rampDuration = RAMP_DURATION_DEF_VALUE_U;
    parameters->i_rampDuration = RAMP_DURATION_DEF_VALUE_I;

    parameters->outputDelayDuration = 0;
    
    *parameters->label = 0;
    parameters->color = 0;
}

void PsuModule::getPowerChannelProfileParameters(int channelIndex, uint8_t *buffer) {
    assert(sizeof(PowerChannelProfileParameters) < MAX_CHANNEL_PARAMETERS_SIZE);

    auto &channel = Channel::get(channelIndex);
    auto parameters = (PowerChannelProfileParameters *)buffer;

    parameters->flags.output_enabled = channel.flags.outputEnabled;
    parameters->flags.sense_enabled = channel.flags.senseEnabled;

    if (channel.params.features & CH_FEATURE_RPROG) {
        parameters->flags.rprog_enabled = channel.flags.rprogEnabled;
    } else {
        parameters->flags.rprog_enabled = 0;
    }

    parameters->flags.u_state = channel.prot_conf.flags.u_state;
    parameters->flags.u_type = channel.prot_conf.flags.u_type;
    parameters->flags.i_state = channel.prot_conf.flags.i_state;
    parameters->flags.p_state = channel.prot_conf.flags.p_state;

    parameters->u_set = channel.getUSet();
    parameters->u_step = channel.u.step;
    parameters->u_limit = channel.u.limit;

    parameters->i_set = channel.getISet();
    parameters->i_step = channel.i.step;
    parameters->i_limit = channel.i.limit;

    parameters->p_limit = channel.p_limit;

    parameters->u_delay = channel.prot_conf.u_delay;
    parameters->u_level = channel.prot_conf.u_level;
    parameters->i_delay = channel.prot_conf.i_delay;
    parameters->p_delay = channel.prot_conf.p_delay;
    parameters->p_level = channel.prot_conf.p_level;

    parameters->displayValues[0] = channel.displayValues[0];
    parameters->displayValues[1] = channel.displayValues[1];
    parameters->ytViewRate = channel.ytViewRate;

#ifdef EEZ_PLATFORM_SIMULATOR
    parameters->load_enabled = channel.simulator.load_enabled;
    parameters->load = channel.simulator.load;
    parameters->voltProgExt = channel.simulator.voltProgExt;
#endif

    parameters->flags.u_triggerMode = channel.flags.voltageTriggerMode;
    parameters->flags.i_triggerMode = channel.flags.currentTriggerMode;
    parameters->flags.triggerOutputState = channel.flags.triggerOutputState;
    parameters->flags.triggerOnListStop = channel.flags.triggerOnListStop;
    parameters->u_triggerValue = channel.u.triggerLevel;
    parameters->i_triggerValue = channel.i.triggerLevel;
    parameters->listCount = list::getListCount(channel);

    parameters->flags.currentRangeSelectionMode = channel.flags.currentRangeSelectionMode;
    parameters->flags.autoSelectCurrentRange = channel.flags.autoSelectCurrentRange;

    parameters->flags.dprogState = channel.flags.dprogState;
    parameters->flags.trackingEnabled = channel.flags.trackingEnabled;

    parameters->u_rampDuration = channel.u.rampDuration;
    parameters->i_rampDuration = channel.i.rampDuration;

    parameters->outputDelayDuration = channel.outputDelayDuration;

    stringCopy(parameters->label, sizeof(parameters->label), channel.label);
    parameters->color = channel.color;
}

void PsuModule::setPowerChannelProfileParameters(int channelIndex, uint8_t *buffer, bool mismatch, int recallOptions, int &numTrackingChannels) {
    auto &channel = Channel::get(channelIndex);
    auto parameters = (PowerChannelProfileParameters *)buffer;

    channel.flags.currentRangeSelectionMode = parameters->flags.currentRangeSelectionMode;
    channel.flags.autoSelectCurrentRange = parameters->flags.autoSelectCurrentRange;

    channel.u.set = channel.roundChannelValue(UNIT_VOLT, MIN(parameters->u_set, channel.u.max));
    channel.u.step = channel.roundChannelValue(UNIT_VOLT, parameters->u_step);
    channel.u.limit = channel.roundChannelValue(UNIT_VOLT, MIN(parameters->u_limit, channel.u.max));

    channel.i.set = channel.roundChannelValue(UNIT_AMPER, MIN(parameters->i_set, channel.i.max));
    channel.i.step = channel.roundChannelValue(UNIT_AMPER, parameters->i_step);

    channel.p_limit = channel.roundChannelValue(UNIT_WATT, MIN(parameters->p_limit, channel.u.max * channel.i.max));

    channel.prot_conf.u_delay = parameters->u_delay;
    channel.prot_conf.u_level = parameters->u_level;
    channel.prot_conf.i_delay = parameters->i_delay;
    channel.prot_conf.p_delay = parameters->p_delay;
    channel.prot_conf.p_level = parameters->p_level;

    channel.prot_conf.flags.u_state = parameters->flags.u_state;
    channel.prot_conf.flags.u_type = parameters->flags.u_type;
    channel.prot_conf.flags.i_state = parameters->flags.i_state;
    channel.prot_conf.flags.p_state = parameters->flags.p_state;

    channel.setCurrentLimit(parameters->i_limit);

#ifdef EEZ_PLATFORM_SIMULATOR
    channel.simulator.load_enabled = parameters->load_enabled;
    channel.simulator.load = parameters->load;
    channel.simulator.voltProgExt = parameters->voltProgExt;
#endif

    channel.flags.outputEnabled = channel.isTripped() || mismatch || (recallOptions & profile::RECALL_OPTION_FORCE_DISABLE_OUTPUT) ? 0 : parameters->flags.output_enabled;
    channel.flags.senseEnabled = parameters->flags.sense_enabled;

    if (channel.params.features & CH_FEATURE_RPROG) {
        channel.flags.rprogEnabled = parameters->flags.rprog_enabled;
    } else {
        channel.flags.rprogEnabled = 0;
    }

    channel.displayValues[0] = parameters->displayValues[0];
    channel.displayValues[1] = parameters->displayValues[1];
    channel.ytViewRate = parameters->ytViewRate;
    if (channel.displayValues[0].type == 0 && channel.displayValues[1].type == 0) {
		channel.displayValues[0].type = DISPLAY_VALUE_VOLTAGE;
		channel.displayValues[0].scale = DISPLAY_VALUE_SCALE_LIMIT;

		channel.displayValues[1].type = DISPLAY_VALUE_CURRENT;
		channel.displayValues[1].scale = DISPLAY_VALUE_SCALE_LIMIT;
	}
    if (channel.ytViewRate == 0) {
        channel.ytViewRate = GUI_YT_VIEW_RATE_DEFAULT;
    }

    channel.flags.voltageTriggerMode = (TriggerMode)parameters->flags.u_triggerMode;
    channel.flags.currentTriggerMode = (TriggerMode)parameters->flags.i_triggerMode;
    channel.flags.triggerOutputState = parameters->flags.triggerOutputState;
    channel.flags.triggerOnListStop = parameters->flags.triggerOnListStop;
    channel.u.triggerLevel = channel.roundChannelValue(UNIT_VOLT, parameters->u_triggerValue);
    channel.i.triggerLevel = channel.roundChannelValue(UNIT_AMPER, parameters->i_triggerValue);
    list::setListCount(channel, parameters->listCount);

    channel.flags.dprogState = parameters->flags.dprogState;

    channel.flags.trackingEnabled = parameters->flags.trackingEnabled;
    if (channel.flags.trackingEnabled) {
        ++numTrackingChannels;
    }

    channel.u.rampDuration = parameters->u_rampDuration;
    if (channel.u.rampDuration > 0 && channel.u.rampDuration < channel.params.U_RAMP_DURATION_MIN_VALUE) {
        channel.u.rampDuration = channel.params.U_RAMP_DURATION_MIN_VALUE;
    }
    channel.i.rampDuration = parameters->i_rampDuration;

    channel.outputDelayDuration = parameters->outputDelayDuration;

    stringCopy(channel.label, sizeof(channel.label), parameters->label);
    channel.color = parameters->color;
}

bool PsuModule::writePowerChannelProfileProperties(profile::WriteContext &ctx, const uint8_t *buffer) {
    auto parameters = (const PowerChannelProfileParameters *)buffer;

    WRITE_PROPERTY("output_enabled", parameters->flags.output_enabled);
    WRITE_PROPERTY("sense_enabled", parameters->flags.sense_enabled);
    WRITE_PROPERTY("u_state", parameters->flags.u_state);
    WRITE_PROPERTY("i_state", parameters->flags.i_state);
    WRITE_PROPERTY("p_state", parameters->flags.p_state);
    WRITE_PROPERTY("rprog_enabled", parameters->flags.rprog_enabled);
	WRITE_PROPERTY("u_triggerMode", parameters->flags.u_triggerMode);
    WRITE_PROPERTY("i_triggerMode", parameters->flags.i_triggerMode);
    WRITE_PROPERTY("currentRangeSelectionMode", parameters->flags.currentRangeSelectionMode);
    WRITE_PROPERTY("autoSelectCurrentRange", parameters->flags.autoSelectCurrentRange);
    WRITE_PROPERTY("triggerOutputState", parameters->flags.triggerOutputState);
    WRITE_PROPERTY("triggerOnListStop", parameters->flags.triggerOnListStop);
    WRITE_PROPERTY("u_type", parameters->flags.u_type);
    WRITE_PROPERTY("dprogState", parameters->flags.dprogState);
    WRITE_PROPERTY("trackingEnabled", parameters->flags.trackingEnabled);

    WRITE_PROPERTY("u_set", parameters->u_set);
    WRITE_PROPERTY("u_step", parameters->u_step);
    WRITE_PROPERTY("u_limit", parameters->u_limit);
    WRITE_PROPERTY("u_delay", parameters->u_delay);
    WRITE_PROPERTY("u_level", parameters->u_level);
    WRITE_PROPERTY("i_set", parameters->i_set);
    WRITE_PROPERTY("i_step", parameters->i_step);
    WRITE_PROPERTY("i_limit", parameters->i_limit);
    WRITE_PROPERTY("i_delay", parameters->i_delay);
    WRITE_PROPERTY("p_limit", parameters->p_limit);
    WRITE_PROPERTY("p_delay", parameters->p_delay);
    WRITE_PROPERTY("p_level", parameters->p_level);
	
	WRITE_PROPERTY("displayValue1.type", parameters->displayValues[0].type);
	WRITE_PROPERTY("displayValue1.scale", parameters->displayValues[0].scale);
	WRITE_PROPERTY("displayValue1.range", parameters->displayValues[0].range);
	
	WRITE_PROPERTY("displayValue2.type", parameters->displayValues[1].type);
	WRITE_PROPERTY("displayValue2.scale", parameters->displayValues[1].scale);
	WRITE_PROPERTY("displayValue2.range", parameters->displayValues[1].range);

	WRITE_PROPERTY("ytViewRate", parameters->ytViewRate);

	WRITE_PROPERTY("u_triggerValue", parameters->u_triggerValue);
    WRITE_PROPERTY("i_triggerValue", parameters->i_triggerValue);
    WRITE_PROPERTY("listCount", parameters->listCount);

    WRITE_PROPERTY("u_rampDuration", parameters->u_rampDuration);
    WRITE_PROPERTY("i_rampDuration", parameters->i_rampDuration);

    WRITE_PROPERTY("outputDelayDuration", parameters->outputDelayDuration);

    WRITE_PROPERTY("label", parameters->label);
    WRITE_PROPERTY("color", parameters->color);

#ifdef EEZ_PLATFORM_SIMULATOR
    WRITE_PROPERTY("load_enabled", parameters->load_enabled);
    WRITE_PROPERTY("load", parameters->load);
    WRITE_PROPERTY("voltProgExt", parameters->voltProgExt);
#endif

    return true;
}

bool PsuModule::readPowerChannelProfileProperties(profile::ReadContext &ctx, uint8_t *buffer) {
    auto parameters = (PowerChannelProfileParameters *)buffer;

    READ_FLAG("output_enabled", parameters->flags.output_enabled);
    READ_FLAG("sense_enabled", parameters->flags.sense_enabled);
    READ_FLAG("u_state", parameters->flags.u_state);
    READ_FLAG("i_state", parameters->flags.i_state);
    READ_FLAG("p_state", parameters->flags.p_state);
    READ_FLAG("rprog_enabled", parameters->flags.rprog_enabled);
    READ_FLAG("u_triggerMode", parameters->flags.u_triggerMode);
    READ_FLAG("i_triggerMode", parameters->flags.i_triggerMode);
    READ_FLAG("currentRangeSelectionMode", parameters->flags.currentRangeSelectionMode);
    READ_FLAG("autoSelectCurrentRange", parameters->flags.autoSelectCurrentRange);
    READ_FLAG("triggerOutputState", parameters->flags.triggerOutputState);
    READ_FLAG("triggerOnListStop", parameters->flags.triggerOnListStop);
    READ_FLAG("u_type", parameters->flags.u_type);
    READ_FLAG("dprogState", parameters->flags.dprogState);
    READ_FLAG("trackingEnabled", parameters->flags.trackingEnabled);

    READ_PROPERTY("u_set", parameters->u_set);
    READ_PROPERTY("u_step", parameters->u_step);
    READ_PROPERTY("u_limit", parameters->u_limit);
    READ_PROPERTY("u_delay", parameters->u_delay);
    READ_PROPERTY("u_level", parameters->u_level);
    READ_PROPERTY("i_set", parameters->i_set);
    READ_PROPERTY("i_step", parameters->i_step);
    READ_PROPERTY("i_limit", parameters->i_limit);
    READ_PROPERTY("i_delay", parameters->i_delay);
    READ_PROPERTY("p_limit", parameters->p_limit);
    READ_PROPERTY("p_delay", parameters->p_delay);
    READ_PROPERTY("p_level", parameters->p_level);

	READ_PROPERTY("displayValue1.type", parameters->displayValues[0].type);
	READ_PROPERTY("displayValue1.scale", parameters->displayValues[0].scale);
	READ_PROPERTY("displayValue1.range", parameters->displayValues[0].range);
	
	READ_PROPERTY("displayValue2.type", parameters->displayValues[1].type);
	READ_PROPERTY("displayValue2.scale", parameters->displayValues[1].scale);
	READ_PROPERTY("displayValue2.range", parameters->displayValues[1].range);

	READ_PROPERTY("ytViewRate", parameters->ytViewRate);
	
	READ_PROPERTY("u_triggerValue", parameters->u_triggerValue);
    READ_PROPERTY("i_triggerValue", parameters->i_triggerValue);
    READ_PROPERTY("listCount", parameters->listCount);

    READ_PROPERTY("u_rampDuration", parameters->u_rampDuration);
    READ_PROPERTY("i_rampDuration", parameters->i_rampDuration);

    READ_PROPERTY("outputDelayDuration", parameters->outputDelayDuration);

    READ_STRING_PROPERTY("label", parameters->label, Channel::CHANNEL_LABEL_MAX_LENGTH);
    READ_PROPERTY("color", parameters->color);

#ifdef EEZ_PLATFORM_SIMULATOR
    READ_PROPERTY("load_enabled", parameters->load_enabled);
    READ_PROPERTY("load", parameters->load);
    READ_PROPERTY("voltProgExt", parameters->voltProgExt);
#endif

    return false;
}

bool PsuModule::getProfileOutputEnable(uint8_t *buffer) {
    auto parameters = (PowerChannelProfileParameters *)buffer;
    return parameters->flags.output_enabled;
}

float PsuModule::getProfileUSet(uint8_t *buffer) {
    auto parameters = (PowerChannelProfileParameters *)buffer;
    return parameters->u_set;
}

float PsuModule::getProfileISet(uint8_t *buffer) {
    auto parameters = (PowerChannelProfileParameters *)buffer;
    return parameters->i_set;
}

bool PsuModule::testAutoRecallValuesMatch(uint8_t *bufferRecall, uint8_t *bufferDefault) {
    auto recallParameters = (PowerChannelProfileParameters *)bufferRecall;
    auto defaultParameters = (PowerChannelProfileParameters *)bufferDefault;

    return 
        recallParameters->u_set == defaultParameters->u_set &&
        recallParameters->i_set == defaultParameters->i_set;
}

eez_err_t PsuModule::getLabel(const char *&label) {
    return SCPI_ERROR_HARDWARE_MISSING;
}

eez_err_t PsuModule::setLabel(const char *label, int length) {
    return SCPI_ERROR_HARDWARE_MISSING;
}

eez_err_t PsuModule::getColor(uint8_t &color) {
    return SCPI_ERROR_HARDWARE_MISSING;
}

eez_err_t PsuModule::setColor(uint8_t color) {
    return SCPI_ERROR_HARDWARE_MISSING;
}

size_t PsuModule::getChannelLabelMaxLength(int subchannelIndex) {
    return Channel::CHANNEL_LABEL_MAX_LENGTH;
}

const char *PsuModule::getChannelLabel(int subchannelIndex) {
    Channel *channel = Channel::getBySlotIndex(slotIndex, subchannelIndex);
    return channel->getLabel();
}

const char *PsuModule::getDefaultChannelLabel(int subchannelIndex) {
    Channel *channel = Channel::getBySlotIndex(slotIndex, subchannelIndex);
    return channel->getDefaultLabel();
}

eez_err_t PsuModule::getChannelLabel(int subchannelIndex, const char *&label) {
    label = getChannelLabel(subchannelIndex);
    return SCPI_RES_OK;
}

eez_err_t PsuModule::setChannelLabel(int subchannelIndex, const char *label, int length) {
    if (length == -1) {
        length = strlen(label);
    }
    if (length > (int)Channel::CHANNEL_LABEL_MAX_LENGTH) {
        length = Channel::CHANNEL_LABEL_MAX_LENGTH;
    }

    Channel *channel = Channel::getBySlotIndex(slotIndex, subchannelIndex);
    memcpy(channel->label, label, length);
    channel->label[length] = 0;
    return SCPI_RES_OK;
}

uint8_t PsuModule::getChannelColor(int subchannelIndex) {
    Channel *channel = Channel::getBySlotIndex(slotIndex, subchannelIndex);
    return channel->color;
}

eez_err_t PsuModule::getChannelColor(int subchannelIndex, uint8_t &color) {
    color = getChannelColor(subchannelIndex);
    return SCPI_RES_OK;
}

eez_err_t PsuModule::setChannelColor(int subchannelIndex, uint8_t color) {
    Channel *channel = Channel::getBySlotIndex(slotIndex, subchannelIndex);
    channel->color = color;
    return SCPI_RES_OK;
}

bool PsuModule::getCalibrationConfiguration(int subchannelIndex, CalibrationConfiguration &calConf, int *err) {
    Channel *channel = Channel::getBySlotIndex(slotIndex, subchannelIndex);
    memcpy(&calConf, &channel->cal_conf, sizeof(CalibrationConfiguration));
    return true;
}

bool PsuModule::setCalibrationConfiguration(int subchannelIndex, const CalibrationConfiguration &calConf, int *err) {
    Channel *channel = Channel::getBySlotIndex(slotIndex, subchannelIndex);
    memcpy(&channel->cal_conf, &calConf, sizeof(CalibrationConfiguration));
    return true;
}

bool PsuModule::getCalibrationRemark(int subchannelIndex, const char *&calibrationRemark, int *err) {
    Channel *channel = Channel::getBySlotIndex(slotIndex, subchannelIndex);
    calibrationRemark = channel->cal_conf.calibrationRemark;
    return true;
}

bool PsuModule::getCalibrationDate(int subchannelIndex, uint32_t &calibrationDate, int *err) {
    Channel *channel = Channel::getBySlotIndex(slotIndex, subchannelIndex);
    calibrationDate = channel->cal_conf.calibrationDate;
    return true;
}

int PsuModule::getNumDlogResources(int subchannelIndex) {
    if (
        slotIndex == 1 && (
            channel_dispatcher::getCouplingType() == channel_dispatcher::COUPLING_TYPE_PARALLEL || 
            channel_dispatcher::getCouplingType() == channel_dispatcher::COUPLING_TYPE_SERIES
        )
    ) {
        return 0;
    }

    return 3;
}

DlogResourceType PsuModule::getDlogResourceType(int subchannelIndex, int resourceIndex) {
    static const DlogResourceType g_resourceTypes[] = { DLOG_RESOURCE_TYPE_U, DLOG_RESOURCE_TYPE_I, DLOG_RESOURCE_TYPE_P };
    return g_resourceTypes[resourceIndex];
}

static char g_resourceLabel[20];

const char *PsuModule::getDlogResourceLabel(int subchannelIndex, int resourceIndex) {
    static const char *g_resourceTypeLabels[] = { "U", "I", "P" };
    auto channel = Channel::getBySlotIndex(slotIndex, subchannelIndex);
    snprintf(g_resourceLabel, sizeof(g_resourceLabel), "%s %s", channel->getLabelOrDefault(), g_resourceTypeLabels[resourceIndex]);
    return g_resourceLabel;
}

int PsuModule::getNumFunctionGeneratorResources(int subchannelIndex) {
	auto channel = Channel::getBySlotIndex(slotIndex, subchannelIndex);

	if (channel->channelIndex == 1 && (channel_dispatcher::getCouplingType() == channel_dispatcher::COUPLING_TYPE_PARALLEL || channel_dispatcher::getCouplingType() == channel_dispatcher::COUPLING_TYPE_SERIES)) {
		return 0;
	}

	if (channel->flags.trackingEnabled) {
        for (int i = 0; i < CH_NUM; i++) {
            if (Channel::get(i).flags.trackingEnabled) {
				if (channel->channelIndex > Channel::get(i).channelIndex) {
					return 0;
				}
				break;
            }
        }
    }
	
	return 2;
}

FunctionGeneratorResourceType PsuModule::getFunctionGeneratorResourceType(int subchannelIndex, int resourceIndex) {
	return resourceIndex == 0 ? FUNCTION_GENERATOR_RESOURCE_TYPE_U : FUNCTION_GENERATOR_RESOURCE_TYPE_I;
}

bool PsuModule::getFunctionGeneratorResourceTriggerMode(int subchannelIndex, int resourceIndex, TriggerMode &triggerMode, int *err) {
    auto channel = Channel::getBySlotIndex(slotIndex, subchannelIndex);

    if (resourceIndex == 0) {
        triggerMode = channel_dispatcher::getVoltageTriggerMode(*channel);
        return true;
    }

    if (resourceIndex == 1) {
        triggerMode = channel_dispatcher::getCurrentTriggerMode(*channel);
        return true;
    }

    if (err) {
        *err = SCPI_ERROR_HARDWARE_MISSING;
    }

    return false;
}

bool PsuModule::setFunctionGeneratorResourceTriggerMode(int subchannelIndex, int resourceIndex, TriggerMode triggerMode, int *err) {
    auto channel = Channel::getBySlotIndex(slotIndex, subchannelIndex);

    if (resourceIndex == 0) {
        channel_dispatcher::setVoltageTriggerMode(*channel, triggerMode);
        return true;
    }

    if (resourceIndex == 1) {
        channel_dispatcher::setCurrentTriggerMode(*channel, triggerMode);
        return true;
    }

    if (err) {
        *err = SCPI_ERROR_HARDWARE_MISSING;
    }

    return false;
}

const char *PsuModule::getFunctionGeneratorResourceLabel(int subchannelIndex, int resourceIndex) {
    static const char *g_resourceTypeLabels[] = { "U", "I" };
    auto channel = Channel::getBySlotIndex(slotIndex, subchannelIndex);
    snprintf(g_resourceLabel, sizeof(g_resourceLabel), "%s %s", channel->getLabelOrDefault(), g_resourceTypeLabels[resourceIndex]);
    return g_resourceLabel;
}

void PsuModule::getFunctionGeneratorAmplitudeInfo(int subchannelIndex, int resourceIndex, FunctionGeneratorResourceType resourceType, float &min, float &max, StepValues *stepValues) {
    auto channel = Channel::getBySlotIndex(slotIndex, subchannelIndex);
    if (resourceIndex == 0) {
        min = channel_dispatcher::getUMin(*channel);
        max = channel_dispatcher::getUMax(*channel);
        if (stepValues) {
            channel->getVoltageStepValues(stepValues, false);
        }
    } else {
		min = channel_dispatcher::getIMin(*channel);
		max = channel_dispatcher::getIMax(*channel);
		if (stepValues) {
            channel->getCurrentStepValues(stepValues, false, true);
        }
    }
}

bool PsuModule::isMicroAmperAllowed(int subchannelIndex) {
	return Channel::getBySlotIndex(slotIndex, subchannelIndex)->isMicroAmperAllowed();
}

bool PsuModule::isAmperAllowed(int subchannelIndex) {
	return Channel::getBySlotIndex(slotIndex, subchannelIndex)->isAmperAllowed();
}

////////////////////////////////////////////////////////////////////////////////

static void tick0();
static void tick1();
static void tick2();
static void tick3();
static void tick4();

////////////////////////////////////////////////////////////////////////////////

void init() {
}

void onThreadMessage(uint8_t type, uint32_t param) {
    if (type == PSU_MESSAGE_TICK) {
#if defined(EEZ_PLATFORM_STM32)
        ramp::tick();
        dcp405::tickDacRamp();
        function_generator::tick();

        if (g_tickCount % 5 == 0) {
            tick0();
        } else if (g_tickCount % 5 == 1) {
            tick1();
        } else if (g_tickCount % 5 == 2) {
            tick2();
        } else if (g_tickCount % 5 == 3) {
            tick3();
        } else if (g_tickCount % 5 == 4) {
            tick4();
        }
#endif
    } else if (type == PSU_MESSAGE_CHANGE_POWER_STATE) {
        changePowerState(param ? true : false);
    } else if (type == PSU_MESSAGE_RESET) {
        reset();
    } else if (type == PSU_MESSAGE_TEST) {
        test();
    } 
#if defined(EEZ_PLATFORM_STM32)
    else if (type == PSU_MESSAGE_SPI_IRQ) {
        g_slots[param]->onSpiIrq();
    } else if (type == PSU_MESSAGE_SPI_DMA_TRANSFER_COMPLETED) {
        int slotIndex = param & 0xff;
        int status = param >> 8;
        g_slots[slotIndex]->onSpiDmaTransferCompleted(status);
    }
#endif
    else if (type == PSU_MESSAGE_ADC_MEASURE_ALL) {
        Channel::get(param).adcMeasureAll();
        g_adcMeasureAllFinished = true;
    } else if (type == PSU_MESSAGE_TRIGGER_START_IMMEDIATELY) {
        trigger::startImmediatelyInPsuThread();
    } else if (type == PSU_MESSAGE_TRIGGER_ABORT) {
        trigger::abort();
    } else if (type == PSU_MESSAGE_TRIGGER_CHANNEL_SAVE_AND_DISABLE_OE) {
        Channel::saveAndDisableOE();
    } else if (type == PSU_MESSAGE_TRIGGER_CHANNEL_RESTORE_OE) {
        Channel::restoreOE();
    } else if (type == PSU_MESSAGE_SET_COUPLING_TYPE) {
        channel_dispatcher::setCouplingTypeInPsuThread((channel_dispatcher::CouplingType)param);
    } else if (type == PSU_MESSAGE_SET_TRACKING_CHANNELS) {
        channel_dispatcher::setTrackingChannels((uint16_t)param);
    } else if (type == PSU_MESSAGE_SYNC_OUTPUT_ENABLE) {
        channel_dispatcher::syncOutputEnable();
    } else if (type == PSU_MESSAGE_HARD_RESET) {
        restart();
    } else if (type == PSU_MESSAGE_SHUTDOWN) {
        shutdown();
    } else if (type == PSU_MESSAGE_SET_VOLTAGE) {
        channel_dispatcher::setVoltageInPsuThread((int)param);
    } else if (type == PSU_MESSAGE_SET_CURRENT) {
        channel_dispatcher::setCurrentInPsuThread((int)param);
    } else if (type == PSU_MESSAGE_SET_VOLTAGE_LIMIT) {
        channel_dispatcher::setVoltageLimitInPsuThread((int)param);
    } else if (type == PSU_MESSAGE_SET_CURRENT_LIMIT) {
        channel_dispatcher::setCurrentLimitInPsuThread((int)param);
    } else if (type == PSU_MESSAGE_FLASH_SLAVE_START) {
        bp3c::flash_slave::doStart();
    } else if (type == PSU_MESSAGE_FLASH_SLAVE_LEAVE_BOOTLOADER_MODE) {
        bp3c::flash_slave::leaveBootloaderMode();
    } else if (type == PSU_MESSAGE_RECALL_STATE) {
        profile::recallStateFromPsuThread();
    } else if (type == PSU_MESSAGE_SLOT_SET_ENABLED) {
        g_slots[(param >> 8) & 0xFF]->setEnabled(param & 0xFF ? true : false);
    } else if (type == PSU_MESSAGE_SET_CURRENT_RANGE_SELECTION_MODE) {
    	Channel &channel = Channel::get((param >> 8) & 0xFF);
        channel_dispatcher::setCurrentRangeSelectionMode(channel, (CurrentRangeSelectionMode)(param & 0xFF));
    } else if (type == PSU_MESSAGE_CALIBRATION_ENABLE) {
    	Channel &channel = Channel::get((param >> 8) & 0xFF);
        channel_dispatcher::calibrationEnable(channel, param & 0xFF);
    } else if (type == PSU_MESSAGE_REMOTE_SENSING_EANBLE) {
    	Channel &channel = Channel::get((param >> 8) & 0xFF);
        channel_dispatcher::remoteSensingEnable(channel, param & 0xFF);
    } else if (type == PSU_MESSAGE_READ_CHANNEL_TEMPERATURE) {
        Channel &channel = Channel::get(param);
        channel.readTemperature();
    } else if (type == PSU_MESSAGE_REMOTE_PROGRAMMING_ENABLE) {
        Channel &channel = Channel::get((param >> 8) & 0xFF);
        channel.remoteProgrammingEnable(param & 0xFF);
    } else if (type == PSU_MESSAGE_SET_DPROG_STATE) {
        Channel &channel = Channel::get((param >> 8) & 0xFF);
        channel.setDprogState((DprogState)(param & 0xFF));
    } else if (type == PSU_MESSAGE_SAVE_SERIAL_NO) {
        persist_conf::saveSerialNo(param);
    } else if (type == PSU_MESSAGE_MODULE_RESYNC) {
        g_slots[param]->resync();
    } else if (type == PSU_MESSAGE_COPY_CHANNEL_TO_CHANNEL) {
        channel_dispatcher::copyChannelToChannel(param >> 8, param & 0xFF);
    } else if (type == PSU_MESSAGE_RESET_CHANNELS_HISTORY) {
        Channel::resetHistoryForAllChannels();
    } else if (calibration::onHighPriorityThreadMessage(type, param)) {
        // handled
    } else if (type >= PSU_MESSAGE_MODULE_SPECIFIC) {
        int slotIndex = param & 0xff;
        g_slots[slotIndex]->onHighPriorityThreadMessage(type, param);
    }
}

bool measureAllAdcValuesOnChannel(int channelIndex) {
	if (g_slots[Channel::get(channelIndex).slotIndex]->moduleType == MODULE_TYPE_NONE) {
		return true;
	}

    g_adcMeasureAllFinished = false;
    sendMessageToPsu(PSU_MESSAGE_ADC_MEASURE_ALL, channelIndex, 0);

    int i;
    for (i = 0; i < 100 && !g_adcMeasureAllFinished; ++i) {
        osDelay(10);
    }

    return g_adcMeasureAllFinished;
}

////////////////////////////////////////////////////////////////////////////////

void initChannels() {
    for (int i = 0; i < NUM_SLOTS; ++i) {
        WATCHDOG_RESET(WATCHDOG_LONG_OPERATION);
    	g_slots[i]->initChannels();
    }    
}

bool testChannels() {
    if (!g_powerIsUp) {
        // test is skipped
        return true;
    }

    bool result = true;

    channel_dispatcher::disableOutputForAllChannels();

    for (int i = 0; i < CH_NUM; ++i) {
        WATCHDOG_RESET(WATCHDOG_LONG_OPERATION);
        result &= Channel::get(i).test();
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////

bool psuReset() {
    channel_dispatcher::disableOutputForAllChannels();

    // *ESE 0
    scpi_reg_set(SCPI_REG_ESE, 0);

    // *SRE 0
    scpi_reg_set(SCPI_REG_SRE, 0);

    // *STB 0
    scpi_reg_set(SCPI_REG_STB, 0);

    // *ESR 0
    scpi_reg_set(SCPI_REG_ESR, 0);

    // STAT:OPER[:EVEN] 0
    scpi_reg_set(SCPI_REG_OPER, 0);

    // STAT:OPER:COND 0
    reg_set(SCPI_PSU_REG_OPER_COND, 0);

    // STAT:OPER:ENAB 0
    scpi_reg_set(SCPI_REG_OPERE, 0);

    // STAT:OPER:INST[:EVEN] 0
    reg_set(SCPI_PSU_REG_OPER_INST_EVENT, 0);

    // STAT:OPER:INST:COND 0
    reg_set(SCPI_PSU_REG_OPER_INST_COND, 0);

    // STAT:OPER:INST:ENAB 0
    reg_set(SCPI_PSU_REG_OPER_INST_ENABLE, 0);

    // STAT:QUES[:EVEN] 0
    scpi_reg_set(SCPI_REG_QUES, 0);

    // STAT:QUES:COND 0
    reg_set(SCPI_PSU_REG_QUES_COND, 0);

    // STAT:QUES:ENAB 0
    scpi_reg_set(SCPI_REG_QUESE, 0);

    // STAT:QUES:INST[:EVEN] 0
    reg_set(SCPI_PSU_REG_QUES_INST_EVENT, 0);

    // STAT:QUES:INST:COND 0
    reg_set(SCPI_PSU_REG_QUES_INST_COND, 0);

    // STAT:QUES:INST:ENAB 0
    reg_set(SCPI_PSU_REG_QUES_INST_ENABLE, 0);

    for (int i = 0; i < NUM_REG_INSTRUMENTS; i++) {
        // STAT:QUES:INST:ISUM[:EVEN] 0
        reg_set((scpi_psu_reg_name_t)(SCPI_PSU_CH_REG_QUES_INST_ISUM_EVENT1 + i), 0);

        // STAT:QUES:INST:ISUM:COND 0
        reg_set((scpi_psu_reg_name_t)(SCPI_PSU_CH_REG_QUES_INST_ISUM_COND1 + i), 0);

        // STAT:OPER:INST:ISUM:ENAB 0
        reg_set((scpi_psu_reg_name_t)(SCPI_PSU_CH_REG_QUES_INST_ISUM_ENABLE1 + i), 0);

        // STAT:OPER:INST:ISUM:COND 0
        reg_set((scpi_psu_reg_name_t)(SCPI_PSU_CH_REG_OPER_INST_ISUM_COND1 + i), 0);

        // STAT:OPER:INST:ISUM[:EVEN] 0
        reg_set((scpi_psu_reg_name_t)(SCPI_PSU_CH_REG_OPER_INST_ISUM_EVENT1 + i), 0);

        // STAT:OPER:INST:ISUM:ENAB 0
        reg_set((scpi_psu_reg_name_t)(SCPI_PSU_CH_REG_OPER_INST_ISUM_ENABLE1 + i), 0);
    }

    eez::scpi::resetContext();

#if OPTION_ETHERNET
    ntp::reset();
#endif

    for (int slotIndex = 0; slotIndex < NUM_SLOTS; slotIndex++) {
        g_slots[slotIndex]->resetConfiguration();
    }

    // TEMP:PROT[AUX]
    // TEMP:PROT:DEL
    // TEMP:PROT:STAT[AUX]
    for (int i = 0; i < temp_sensor::NUM_TEMP_SENSORS; ++i) {
        temperature::ProtectionConfiguration &temp_prot = temperature::sensors[i].prot_conf;
        if (i == temp_sensor::AUX) {
            temp_prot.delay = OTP_AUX_DEFAULT_DELAY;
            temp_prot.level = OTP_AUX_DEFAULT_LEVEL;
            temp_prot.state = OTP_AUX_DEFAULT_STATE;
        } else {
            temp_prot.delay = OTP_CH_DEFAULT_DELAY;
            temp_prot.level = OTP_CH_DEFAULT_LEVEL;
            temp_prot.state = OTP_CH_DEFAULT_STATE;
        }
    }

    // CAL[:MODE] OFF
    calibration::g_editor.stop();

    // reset channels
    int err;
    if (!channel_dispatcher::setCouplingType(channel_dispatcher::COUPLING_TYPE_NONE, &err)) {
        event_queue::pushEvent(err);
    }
    for (int i = 0; i < CH_NUM; ++i) {
        Channel::get(i).reset();
    }

    //
    channel_dispatcher::setTrackingChannels(0);

    //
    trigger::reset();

    //
    list::reset();

    //
    dlog_record::reset();

    //
    io_pins::reset();

	//
	function_generator::reset();

	//
	scripting::resetSettings();

    // SYST:POW ON
    if (powerUp()) {
        Channel::updateAllChannels();

    	return true;
    }

    return false;
}

////////////////////////////////////////////////////////////////////////////////

bool autoRecall(int recallOptions) {
	if (sd_card::isMounted(nullptr, nullptr)) {
		if (persist_conf::isProfileAutoRecallEnabled()) {
			int location = persist_conf::getProfileAutoRecallLocation();
			int err;
			auto forceDisableOutput = persist_conf::isForceDisablingAllOutputsOnPowerUpEnabled() || !g_bootTestSuccess;
			if (profile::recallFromLocation(location, recallOptions | (forceDisableOutput ? profile::RECALL_OPTION_FORCE_DISABLE_OUTPUT : 0), false, &err)) {
				return true;
			}
			if (err != SCPI_ERROR_FILE_NOT_FOUND) {
				generateError(SCPI_ERROR_RECALL_FROM_PROFILE);
			}
		}
	}

    return false;
}

////////////////////////////////////////////////////////////////////////////////

bool powerUp() {
    if (g_powerIsUp) {
        return true;
    }

    sound::playPowerUp(sound::PLAY_POWER_UP_CONDITION_NONE);

    g_rlState = persist_conf::devConf.isFrontPanelLocked ? RL_STATE_REMOTE : RL_STATE_LOCAL;

#if OPTION_DISPLAY
    gui::showWelcomePage();
#endif

    WATCHDOG_RESET(WATCHDOG_LONG_OPERATION);

    // turn power on
    board::powerUp();
    g_powerIsUp = true;

    WATCHDOG_RESET(WATCHDOG_LONG_OPERATION);

    bp3c::io_exp::hardResetModules();

    WATCHDOG_RESET(WATCHDOG_LONG_OPERATION);

    psuReset();

#if !CONF_SURVIVE_MODE
    ontime::g_mcuCounter.start();
    for (int slotIndex = 0; slotIndex < NUM_SLOTS; slotIndex++) {
        if (g_slots[slotIndex]->moduleType != MODULE_TYPE_NONE) {
            WATCHDOG_RESET(WATCHDOG_LONG_OPERATION);
            ontime::g_moduleCounters[slotIndex].start();
        }
    }
#endif

    // init channels
    initChannels();

    bool testSuccess = true;

    if (g_isBooted) {
    	testSuccess &= testMaster();
    }

    // test channels
    testSuccess &= testChannels();

    // turn on Power On (PON) bit of ESE register
    reg_set_esr_bits(ESR_PON);

    InfoTrace("Power up (v%s)\n", MCU_FIRMWARE);

    // play power up tune on success
    if (testSuccess) {
        sound::playPowerUp(sound::PLAY_POWER_UP_CONDITION_TEST_SUCCESSFUL);
    }

    g_bootTestSuccess &= testSuccess;

    return true;
}

void powerDownOnlyPowerChannels() {
    trigger::abort();
    dlog_record::abort();

    channel_dispatcher::setCouplingType(channel_dispatcher::COUPLING_TYPE_NONE, nullptr);

    powerDownChannels(true);

    board::powerDown();

#if !CONF_SURVIVE_MODE
    for (int slotIndex = 0; slotIndex < NUM_SLOTS; slotIndex++) {
        if (g_slots[slotIndex]->moduleType != MODULE_TYPE_NONE && g_slots[slotIndex]->numPowerChannels > 0) {
            ontime::g_moduleCounters[slotIndex].stop();
        }
    }
#endif
}

void powerDown() {
#if OPTION_DISPLAY
    if (!g_shutdownInProgress) {
        if (g_isBooted) {
            gui::showEnteringStandbyPage();
        } else {
            gui::showStandbyPage();
        }
    }
#endif

    if (!g_powerIsUp)
        return;

    trigger::abort();
    dlog_record::abort();

    int err;
    if (!channel_dispatcher::setCouplingType(channel_dispatcher::COUPLING_TYPE_NONE, &err)) {
        event_queue::pushEvent(err);
    }

    powerDownChannels();

#if defined(EEZ_PLATFORM_STM32)
    HAL_NVIC_DisableIRQ(EXTI9_5_IRQn);
    HAL_NVIC_DisableIRQ(EXTI15_10_IRQn);
#endif

    board::powerDown();

    g_powerIsUp = false;

#if !CONF_SURVIVE_MODE
    ontime::g_mcuCounter.stop();
    for (int slotIndex = 0; slotIndex < NUM_SLOTS; slotIndex++) {
        if (g_slots[slotIndex]->moduleType != MODULE_TYPE_NONE) {
            ontime::g_moduleCounters[slotIndex].stop();
        }
    }
#endif

    event_queue::pushEvent(event_queue::EVENT_INFO_POWER_DOWN);

    io_pins::tick();
    io_pins::refresh();

    sound::playPowerDown();
}

void powerDownChannels(bool onlyPowerChannels) {
    channel_dispatcher::disableOutputForAllChannels();

    for (int i = 0; i < CH_NUM; ++i) {
        Channel::get(i).onPowerDown();
    }

    for (int i = 0; i < NUM_SLOTS; ++i) {
        if (!onlyPowerChannels || g_slots[i]->numPowerChannels > 0) {
            g_slots[i]->onPowerDown();
        }
    }
}

bool isPowerUp() {
    return g_powerIsUp;
}

void changePowerState(bool up) {
    if (up == g_powerIsUp)
        return;

    // at least MIN_POWER_UP_DELAY seconds shall pass after last power down
    if (g_testPowerUpDelay) {
        if (millis() - g_powerDownTime < MIN_POWER_UP_DELAY * 1000)
            return;
        g_testPowerUpDelay = false;
    }

    if (!isPsuThread()) {
        sendMessageToPsu(PSU_MESSAGE_CHANGE_POWER_STATE, up ? 1 : 0);
        return;
    }

    if (up) {
        g_bootTestSuccess = true;

        if (!powerUp()) {
            return;
        }

        if (!autoRecall(profile::RECALL_OPTION_IGNORE_POWER)) {
            io_pins::refresh();
        }
    } else {
        powerDown();

        g_testPowerUpDelay = true;
        g_powerDownTime = millis();
    }
}

void powerDownBySensor() {
    if (g_powerIsUp) {
        channel_dispatcher::disableOutputForAllChannels();
        powerDown();
    }
}

////////////////////////////////////////////////////////////////////////////////

void onProtectionTripped() {
    if (isPowerUp()) {
        if (persist_conf::isShutdownWhenProtectionTrippedEnabled()) {
            powerDownBySensor();
        } else {
            if (persist_conf::isOutputProtectionCoupleEnabled()) {
                channel_dispatcher::disableOutputForAllChannels();
            }
        }
    }
}

////////////////////////////////////////////////////////////////////////////////

void tick0() {
    WATCHDOG_RESET(WATCHDOG_HIGH_PRIORITY_THREAD);

    for (int i = 0; i < NUM_SLOTS; i++) {
        g_slots[i]->tick();
    }

#if defined(EEZ_PLATFORM_SIMULATOR)
	ramp::tick();
	dcp405::tickDacRamp();
	function_generator::tick();
#endif
}

void tick1() {
    trigger::tick();
    list::tick();
}

void tick2() {
    for (int i = 0; i < CH_NUM; ++i) {
        Channel::get(i).tick();
    }
}

void tick3() {
    dlog_record::tick();
    io_pins::tick();
    temperature::tick();
    aux_ps::fan::tick();
}

void tick4() {
    datetime::tick();

#if defined(EEZ_PLATFORM_STM32)
    // call every 10 ms
    static int counter = 0;
    if (++counter == 10) {
        touch::tickHighPriority();
        counter = 0;
    }
#endif

    if (g_diagCallback) {
        g_diagCallback();
        g_diagCallback = NULL;
    }
}

void tick() {
    tick0();
    tick1();
    tick2();
    tick3();
    tick4();
}

////////////////////////////////////////////////////////////////////////////////

void setQuesBits(int bit_mask, bool on) {
    reg_set_ques_bit(bit_mask, on);
}

void setOperBits(int bit_mask, bool on) {
    reg_set_oper_bit(bit_mask, on);
}

////////////////////////////////////////////////////////////////////////////////

bool isMaxCurrentLimited() {
    return g_maxCurrentLimitCause != MAX_CURRENT_LIMIT_CAUSE_NONE;
}

void limitMaxCurrent(MaxCurrentLimitCause cause) {
    if (g_maxCurrentLimitCause != cause) {
        g_maxCurrentLimitCause = cause;

        if (isMaxCurrentLimited()) {
            for (int i = 0; i < CH_NUM; ++i) {
                if (Channel::get(i).getCurrentLimit() > ERR_MAX_CURRENT) {
                    channel_dispatcher::setCurrentLimit(Channel::get(i), ERR_MAX_CURRENT);
                }
            }
        }
    }
}

void unlimitMaxCurrent() {
    limitMaxCurrent(MAX_CURRENT_LIMIT_CAUSE_NONE);
}

MaxCurrentLimitCause getMaxCurrentLimitCause() {
    return g_maxCurrentLimitCause;
}

#if defined(EEZ_PLATFORM_SIMULATOR)

namespace simulator {

static float g_temperature[temp_sensor::NUM_TEMP_SENSORS];
static bool g_pwrgood[CH_MAX];
static bool g_rpol[CH_MAX];
static bool g_cv[CH_MAX];
static bool g_cc[CH_MAX];
float g_uSet[CH_MAX];
float g_iSet[CH_MAX];

void init() {
    for (int i = 0; i < temp_sensor::NUM_TEMP_SENSORS; ++i) {
        g_temperature[i] = 25.0f;
    }

    for (int i = 0; i < CH_MAX; ++i) {
        g_pwrgood[i] = true;
        g_rpol[i] = false;
    }
}

void tick() {
    psu::tick();
}

void setTemperature(int sensor, float value) {
    g_temperature[sensor] = value;
}

float getTemperature(int sensor) {
    return g_temperature[sensor];
}

bool getPwrgood(int pin) {
    return g_pwrgood[pin];
}

void setPwrgood(int pin, bool on) {
    g_pwrgood[pin] = on;
}

bool getRPol(int pin) {
    return g_rpol[pin];
}

void setRPol(int pin, bool on) {
    g_rpol[pin] = on;
}

bool getCV(int pin) {
    return g_cv[pin];
}

void setCV(int pin, bool on) {
    g_cv[pin] = on;
}

bool getCC(int pin) {
    return g_cc[pin];
}

void setCC(int pin, bool on) {
    g_cc[pin] = on;
}

////////////////////////////////////////////////////////////////////////////////

void exit() {
}

} // namespace simulator

#endif

} // namespace psu
} // namespace eez
