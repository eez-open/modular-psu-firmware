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

#include <stdio.h>

#include <eez/file_type.h>

#include <eez/modules/psu/psu.h>

#include <eez/modules/psu/calibration.h>
#include <eez/modules/psu/channel_dispatcher.h>
#include <eez/modules/psu/datetime.h>
#include <eez/modules/psu/event_queue.h>
#include <eez/modules/psu/idle.h>
#include <eez/modules/psu/list_program.h>
#include <eez/modules/psu/persist_conf.h>
#include <eez/modules/psu/profile.h>
#include <eez/modules/psu/scpi/psu.h>
#include <eez/modules/psu/trigger.h>
#include <eez/modules/psu/sd_card.h>
#include <eez/modules/psu/gui/psu.h>
#include <eez/libs/sd_fat/sd_fat.h>
#include <eez/scpi/scpi.h>

namespace eez {

using namespace scpi;

namespace psu {
namespace profile {

#define AUTO_NAME_PREFIX "Saved at "

static bool g_freeze;

static struct {
    bool loaded;
    profile::Parameters profile;
    bool dirty;
    unsigned numSaveErrors;
} g_profilesCache[NUM_PROFILE_LOCATIONS];

static struct {
    float dwellList[MAX_LIST_LENGTH];
    uint16_t dwellListLength;

    float voltageList[MAX_LIST_LENGTH];
    uint16_t voltageListLength;

    float currentList[MAX_LIST_LENGTH];
    uint16_t currentListLength;
} g_channelsLists[CH_MAX];

////////////////////////////////////////////////////////////////////////////////

void mapCurrentChannelsToProfileChannels(Parameters &profile, int channelsMap[]) {
    bool profileChannelAlreadyUsed[CH_MAX];
    for (int j = 0; j < CH_MAX; ++j) {
        profileChannelAlreadyUsed[j] = false;
    }

    for (int i = 0; i < CH_MAX; ++i) {
        channelsMap[i] = -1;
    }

    for (int i = 0; i < CH_NUM; ++i) {
        Channel &channel = Channel::get(i);
        if (
            profile.channels[i].flags.parameters_are_valid &&
            profile.channels[i].moduleType == g_slots[channel.slotIndex].moduleInfo->moduleType
        ) {
            profileChannelAlreadyUsed[i] = true;
            channelsMap[i] = i;
            break;
        }
    }

    for (int i = 0; i < CH_NUM; ++i) {
        if (channelsMap[i] == -1) {
            Channel &channel = Channel::get(i);
            for (int j = 0; j < CH_MAX; ++j) {
                if (
                    !profileChannelAlreadyUsed[j] &&
                    profile.channels[j].flags.parameters_are_valid &&
                    profile.channels[j].moduleType == g_slots[channel.slotIndex].moduleInfo->moduleType
                    ) {
                    profileChannelAlreadyUsed[j] = true;
                    channelsMap[i] = j;
                    break;
                }
            }
        }
    }

    for (int i = 0; i < CH_NUM; ++i) {
        if (channelsMap[i] == -1) {
            for (int j = 0; j < CH_MAX; ++j) {
                if (!profileChannelAlreadyUsed[j] && !profile.channels[j].flags.parameters_are_valid) {
                    profileChannelAlreadyUsed[j] = true;
                    channelsMap[i] = j;
                    break;
                }
            }
        }
    }

    for (int i = 0; i < CH_MAX; ++i) {
        if (channelsMap[i] == -1) {
            for (int j = 0; j < CH_MAX; ++j) {
                if (!profileChannelAlreadyUsed[j]) {
                    profileChannelAlreadyUsed[j] = true;
                    channelsMap[i] = j;
                    break;
                }
            }
        }
    }
}

////////////////////////////////////////////////////////////////////////////////

static void getProfileFilePath(int location, char *filePath) {
    strcpy(filePath, PROFILES_DIR);
    strcat(filePath, PATH_SEPARATOR);
    strcatInt(filePath, location);
    strcat(filePath, getExtensionFromFileType(FILE_TYPE_PROFILE));
}

////////////////////////////////////////////////////////////////////////////////

bool saveProfile(int location, bool showProgres, int *err) {
    if (location == 0) {
        return saveToLocation(0, nullptr, false, err);
    }

    if (err) {
        *err = SCPI_ERROR_MASS_STORAGE_ERROR;
    }

    return false;
}

void doSave() {
    int err;
    if (!saveProfile(0, false, &err)) {
        generateError(err);
    }
}

////////////////////////////////////////////////////////////////////////////////

void recallChannelsFromProfile(Parameters &profile, int recallOptions) {
    trigger::abort();

    int err;
    if (!channel_dispatcher::setCouplingType((channel_dispatcher::CouplingType)profile.flags.couplingType, &err)) {
        event_queue::pushEvent(err);
    }

    int channelsMap[CH_MAX];
    mapCurrentChannelsToProfileChannels(profile, channelsMap);

    bool mismatch = false;
    for (int i = 0; i < CH_NUM; ++i) {
        if (channelsMap[i] != i) {
            mismatch = true;
            break;
        }
    }

    int numTrackingChannels = 0;

    for (int i = 0; i < CH_NUM; ++i) {
        Channel &channel = Channel::get(i);

        int j = channelsMap[i];

        if (profile.channels[j].flags.parameters_are_valid) {
            channel.u.set = MIN(profile.channels[j].u_set, channel.u.max);
            channel.u.step = profile.channels[j].u_step;
            channel.u.limit = MIN(profile.channels[j].u_limit, channel.u.max);

            channel.i.set = MIN(profile.channels[j].i_set, channel.i.max);
            channel.i.step = profile.channels[j].i_step;

            channel.p_limit = MIN(profile.channels[j].p_limit, channel.u.max * channel.i.max);

            channel.prot_conf.u_delay = profile.channels[j].u_delay;
            channel.prot_conf.u_level = profile.channels[j].u_level;
            channel.prot_conf.i_delay = profile.channels[j].i_delay;
            channel.prot_conf.p_delay = profile.channels[j].p_delay;
            channel.prot_conf.p_level = profile.channels[j].p_level;

            channel.prot_conf.flags.u_state = profile.channels[j].flags.u_state;
            channel.prot_conf.flags.u_type = profile.channels[j].flags.u_type;
            channel.prot_conf.flags.i_state = profile.channels[j].flags.i_state;
            channel.prot_conf.flags.p_state = profile.channels[j].flags.p_state;

            channel.setCurrentLimit(profile.channels[j].i_limit);

#ifdef EEZ_PLATFORM_SIMULATOR
            channel.simulator.load_enabled = profile.channels[j].load_enabled;
            channel.simulator.load = profile.channels[j].load;
            channel.simulator.voltProgExt = profile.channels[j].voltProgExt;
#endif

            channel.flags.outputEnabled = channel.isTripped() || mismatch || (recallOptions & RECALL_OPTION_FORCE_DISABLE_OUTPUT) ? 0 : profile.channels[j].flags.output_enabled;
            channel.flags.senseEnabled = profile.channels[j].flags.sense_enabled;

            if (channel.params.features & CH_FEATURE_RPROG) {
                channel.flags.rprogEnabled = profile.channels[j].flags.rprog_enabled;
            } else {
                channel.flags.rprogEnabled = 0;
            }

            channel.flags.displayValue1 = profile.channels[j].flags.displayValue1;
            channel.flags.displayValue2 = profile.channels[j].flags.displayValue2;
            channel.ytViewRate = profile.channels[j].ytViewRate;
            if (channel.flags.displayValue1 == 0 && channel.flags.displayValue2 == 0) {
                channel.flags.displayValue1 = DISPLAY_VALUE_VOLTAGE;
                channel.flags.displayValue2 = DISPLAY_VALUE_CURRENT;
            }
            if (channel.ytViewRate == 0) {
                channel.ytViewRate = GUI_YT_VIEW_RATE_DEFAULT;
            }

            channel.flags.voltageTriggerMode = (TriggerMode)profile.channels[j].flags.u_triggerMode;
            channel.flags.currentTriggerMode = (TriggerMode)profile.channels[j].flags.i_triggerMode;
            channel.flags.triggerOutputState = profile.channels[j].flags.triggerOutputState;
            channel.flags.triggerOnListStop = profile.channels[j].flags.triggerOnListStop;
            trigger::setVoltage(channel, profile.channels[j].u_triggerValue);
            trigger::setCurrent(channel, profile.channels[j].i_triggerValue);
            list::setListCount(channel, profile.channels[j].listCount);

            channel.flags.currentRangeSelectionMode = profile.channels[j].flags.currentRangeSelectionMode;
            channel.flags.autoSelectCurrentRange = profile.channels[j].flags.autoSelectCurrentRange;

            channel.flags.dprogState = profile.channels[j].flags.dprogState;
            
            channel.flags.trackingEnabled = profile.channels[j].flags.trackingEnabled;
            if (channel.flags.trackingEnabled) {
                ++numTrackingChannels;
            }

            auto &list = g_channelsLists[j];
            channel_dispatcher::setDwellList(channel, list.dwellList, list.dwellListLength);
            channel_dispatcher::setVoltageList(channel, list.voltageList, list.voltageListLength);
            channel_dispatcher::setCurrentList(channel, list.currentList, list.currentListLength);
        }
    }

    if (numTrackingChannels == 1) {
        for (int i = 0; i < CH_NUM; ++i) {
            Channel &channel = Channel::get(i);
            if (channel.flags.trackingEnabled) {
                channel.flags.trackingEnabled = 0;
                break;
            }
        }
    }

    Channel::updateAllChannels();
}

void fillProfile(Parameters &profile) {
    int channelsMap[CH_MAX];
    mapCurrentChannelsToProfileChannels(profile, channelsMap);

    Parameters savedProfile;
    memcpy(&savedProfile, &profile, sizeof(Parameters));

    profile.flags.couplingType = channel_dispatcher::getCouplingType();
    profile.flags.powerIsUp = isPowerUp();

    for (int i = 0; i < CH_MAX; ++i) {
        int j = i;

        if (i < CH_NUM) {
            Channel &channel = Channel::get(i);

            profile.channels[j].moduleType = g_slots[channel.slotIndex].moduleInfo->moduleType;
            profile.channels[j].moduleRevision = g_slots[channel.slotIndex].moduleRevision;

            profile.channels[j].flags.parameters_are_valid = 1;

            profile.channels[j].flags.output_enabled = channel.flags.outputEnabled;
            profile.channels[j].flags.sense_enabled = channel.flags.senseEnabled;

            if (channel.params.features & CH_FEATURE_RPROG) {
                profile.channels[j].flags.rprog_enabled = channel.flags.rprogEnabled;
            } else {
                profile.channels[j].flags.rprog_enabled = 0;
            }

            profile.channels[j].flags.u_state = channel.prot_conf.flags.u_state;
            profile.channels[j].flags.u_type = channel.prot_conf.flags.u_type;
            profile.channels[j].flags.i_state = channel.prot_conf.flags.i_state;
            profile.channels[j].flags.p_state = channel.prot_conf.flags.p_state;

            profile.channels[j].u_set = channel.getUSetUnbalanced();
            profile.channels[j].u_step = channel.u.step;
            profile.channels[j].u_limit = channel.u.limit;

            profile.channels[j].i_set = channel.getISetUnbalanced();
            profile.channels[j].i_step = channel.i.step;
            profile.channels[j].i_limit = channel.i.limit;

            profile.channels[j].p_limit = channel.p_limit;

            profile.channels[j].u_delay = channel.prot_conf.u_delay;
            profile.channels[j].u_level = channel.prot_conf.u_level;
            profile.channels[j].i_delay = channel.prot_conf.i_delay;
            profile.channels[j].p_delay = channel.prot_conf.p_delay;
            profile.channels[j].p_level = channel.prot_conf.p_level;

            profile.channels[j].flags.displayValue1 = channel.flags.displayValue1;
            profile.channels[j].flags.displayValue2 = channel.flags.displayValue2;
            profile.channels[j].ytViewRate = channel.ytViewRate;

#ifdef EEZ_PLATFORM_SIMULATOR
            profile.channels[j].load_enabled = channel.simulator.load_enabled;
            profile.channels[j].load = channel.simulator.load;
            profile.channels[j].voltProgExt = channel.simulator.voltProgExt;
#endif

            profile.channels[j].flags.u_triggerMode = channel.flags.voltageTriggerMode;
            profile.channels[j].flags.i_triggerMode = channel.flags.currentTriggerMode;
            profile.channels[j].flags.triggerOutputState = channel.flags.triggerOutputState;
            profile.channels[j].flags.triggerOnListStop = channel.flags.triggerOnListStop;
            profile.channels[j].u_triggerValue = trigger::getVoltage(channel);
            profile.channels[j].i_triggerValue = trigger::getCurrent(channel);
            profile.channels[j].listCount = list::getListCount(channel);

            profile.channels[j].flags.currentRangeSelectionMode = channel.flags.currentRangeSelectionMode;
            profile.channels[j].flags.autoSelectCurrentRange = channel.flags.autoSelectCurrentRange;

            profile.channels[j].flags.dprogState = channel.flags.dprogState;
            profile.channels[j].flags.trackingEnabled = channel.flags.trackingEnabled;
        } else {
            memcpy(&profile.channels[j], &savedProfile.channels[channelsMap[i]], sizeof(ChannelParameters));
        }
    }

    for (int i = 0; i < temp_sensor::MAX_NUM_TEMP_SENSORS; ++i) {
        if (i < temp_sensor::NUM_TEMP_SENSORS) {
            memcpy(profile.temp_prot + i, &temperature::sensors[i].prot_conf, sizeof(temperature::ProtectionConfiguration));
        } else {
            profile.temp_prot[i].sensor = i;
            if (profile.temp_prot[i].sensor == temp_sensor::AUX) {
                profile.temp_prot[i].delay = OTP_AUX_DEFAULT_DELAY;
                profile.temp_prot[i].level = OTP_AUX_DEFAULT_LEVEL;
                profile.temp_prot[i].state = OTP_AUX_DEFAULT_STATE;
            } else {
                profile.temp_prot[i].delay = OTP_CH_DEFAULT_DELAY;
                profile.temp_prot[i].level = OTP_CH_DEFAULT_LEVEL;
                profile.temp_prot[i].state = OTP_CH_DEFAULT_STATE;
            }
        }
    }

    profile.flags.isValid = true;
}

////////////////////////////////////////////////////////////////////////////////

void loadProfileParameters(int location) {
}

Parameters *getProfileParameters(int location) {
    if (location >= 0 && location < NUM_PROFILE_LOCATIONS) {
        if (g_profilesCache[location].loaded) {
            Parameters *profile = &g_profilesCache[location].profile;
            if (profile->flags.isValid) {
                return profile;
            }
        }
    }
    return nullptr;
}

////////////////////////////////////////////////////////////////////////////////

bool isTickSaveAllowed() {
    return !list::isActive() && !calibration::isEnabled();
}

bool isAutoSaveAllowed() {
    return !g_freeze && persist_conf::devConf.profileAutoRecallEnabled && persist_conf::devConf.profileAutoRecallLocation == 0;
}

bool isProfileDirty() {
    // TODO
    return true;
}

////////////////////////////////////////////////////////////////////////////////

void init() {
    for (int profileIndex = 0; profileIndex < NUM_PROFILE_LOCATIONS; profileIndex++) {
        loadProfileParameters(profileIndex);
    }
}

void tick() {
    // TODO autoSave if dirty every 60 seconds
    if (isTickSaveAllowed() && isAutoSaveAllowed() && isProfileDirty()) {

    }
}

////////////////////////////////////////////////////////////////////////////////

void shutdownSave() {
    if (isAutoSaveAllowed() && isProfileDirty()) {
        doSave();
    }
}

////////////////////////////////////////////////////////////////////////////////

bool doRecall(Parameters &profile, int recallOptions, int *err) {
    bool result = true;

    if (!(recallOptions & RECALL_OPTION_IGNORE_POWER)) {
        if (profile.flags.powerIsUp) {
            result &= powerUp();
            if (!result) {
                if (err) {
                    *err = SCPI_ERROR_EXECUTION_ERROR;
                }
            }
        } else {
            powerDown();
        }
    }

    if (result) {
        for (int i = 0; i < temp_sensor::NUM_TEMP_SENSORS; ++i) {
            memcpy(&temperature::sensors[i].prot_conf, profile.temp_prot + i, sizeof(temperature::ProtectionConfiguration));
        }

        recallChannelsFromProfile(profile, recallOptions);
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////

class ReadContext {
public:
    ReadContext(File &file_);

    bool doRead(void (*callback)(ReadContext &ctx, Parameters &parameters), Parameters &parameters, bool showProgress);

    bool matchGroup(const char *groupName);
    bool matchGroup(const char *groupNamePrefix, int &index);

    bool property(const char *name, unsigned int &value);
    bool property(const char *name, uint16_t &value);
    bool property(const char *name, bool &value);

    bool property(const char *name, float &value);
    bool property(const char *name, char *str, unsigned int strLength);

    bool listProperty(const char *name, int channelIndex);

    bool skipProperty(const char *name);

    bool result;

private:
    sd_card::BufferedFileRead file;
    char groupName[100];
    char propertyName[100];
};

ReadContext::ReadContext(File &file_)
    : result(true)
    , file(file_)
{
}

bool ReadContext::doRead(void (*callback)(ReadContext &ctx, Parameters &parameters), Parameters &parameters, bool showProgress) {
#if OPTION_DISPLAY
    size_t totalSize = file.size();
#endif

    while (true) {
#if OPTION_DISPLAY
        if (showProgress) {
            eez::psu::gui::PsuAppContext::updateProgressPage(file.tell(), totalSize);
        }
#endif

        sd_card::matchZeroOrMoreSpaces(file);
        if (!file.available()) {
            break;
        }

        if (sd_card::match(file, '[')) {
            if (!sd_card::matchUntil(file, ']', groupName)) {
                return false;
            }
        } else {
            if (!sd_card::matchUntil(file, '=', propertyName)) {
                return false;
            }
            callback(*this, parameters);
            if (!result) {
                return false;
            }
        }
    }
    
    parameters.flags.isValid = 1;

    return true;
}

bool ReadContext::matchGroup(const char *groupName_) {
    return strcmp(groupName, groupName_) == 0;
}

bool ReadContext::matchGroup(const char *groupNamePrefix, int &index) {
    auto prefixLength = strlen(groupNamePrefix);
    if (strncmp(groupName, groupNamePrefix, prefixLength) != 0) {
        return false;
    }

    const char *startptr = groupName + prefixLength;
    char *endptr;
    index = strtol(startptr, &endptr, 10);
    return endptr > startptr;
}

bool ReadContext::property(const char *name, unsigned int &value) {
    if (strcmp(propertyName, name) != 0) {
        return false;
    }

    unsigned int temp;
    if (sd_card::match(file, temp)) {
        value = temp;
    } else {
        result = false;
    }
       
    return true;
}

bool ReadContext::property(const char *name, uint16_t &value) {
    if (strcmp(propertyName, name) != 0) {
        return false;
    }

    unsigned int temp;
    if (sd_card::match(file, temp)) {
        value = (uint16_t)temp;
    } else {
        result = false;
    }

    return true;
}

bool ReadContext::property(const char *name, bool &value) {
    if (strcmp(propertyName, name) != 0) {
        return false;
    }

    unsigned int temp;
    if (sd_card::match(file, temp)) {
        value = temp;
    } else {
        result = false;
    }

    return true;
}

bool ReadContext::property(const char *name, float &value) {
    if (strcmp(propertyName, name) != 0) {
        return false;
    }

    float temp;
    if (sd_card::match(file, temp)) {
        value = temp;
    } else {
        result = false;
    }

    return true;
}

bool ReadContext::property(const char *name, char *str, unsigned int strLength) {
    if (strcmp(propertyName, name) != 0) {
        return false;
    }

    if (!sd_card::matchQuotedString(file, str, strLength)) {
        result = false;
    }

    return true;
}

bool ReadContext::listProperty(const char *name, int channelIndex) {
    if (strcmp(propertyName, name) != 0) {
        return false;
    }

    if (!sd_card::match(file, "```")) {
        result = false;
    }

    auto &list = g_channelsLists[channelIndex];
    int err;
    if (!list::loadList(file, list.dwellList, list.dwellListLength, list.voltageList, list.voltageListLength, list.currentList, list.currentListLength, false, &err)) {
        result = false;
        return true;
    }

    if (!sd_card::match(file, "```")) {
        result = false;
        return true;
    }


    return true;
}

bool ReadContext::skipProperty(const char *name) {
    if (strcmp(propertyName, name) != 0) {
        return false;
    }

    sd_card::skipUntilEOL(file);

    return true;
}

#define READ_FLAG(name, value) \
    auto name = value; \
    if (ctx.property(#name, name)) { \
        value = name; \
        return; \
    }

#define READ_PROPERTY(name, value) \
    if (ctx.property(#name, value)) { \
        return; \
    }

#define READ_STRING_PROPERTY(name, str, strLength) \
    if (ctx.property(#name, str, strLength)) { \
        return; \
    }

#define READ_LIST_PROPERTY(name, channelIndex) \
    if (ctx.listProperty(#name, channelIndex)) { \
        return; \
    }

#define SKIP_PROPERTY(name) \
    if (ctx.skipProperty(#name)) { \
        return; \
    }

////////////////////////////////////////////////////////////////////////////////

void profileReadCallback(ReadContext &ctx, Parameters &parameters) {
    if (ctx.matchGroup("system")) {
        READ_FLAG(powerIsUp, parameters.flags.powerIsUp);
        READ_STRING_PROPERTY(profileName, parameters.name, PROFILE_NAME_MAX_LENGTH);
    }

    if (ctx.matchGroup("dcpsupply")) {
        READ_FLAG(couplingType, parameters.flags.couplingType);
    }

    int channelIndex;
    if (ctx.matchGroup("dcpsupply.ch", channelIndex)) {
        --channelIndex;

        auto &channel = parameters.channels[channelIndex];
        
        channel.flags.parameters_are_valid = 1;

        READ_PROPERTY(moduleType, channel.moduleType);
        READ_PROPERTY(moduleRevision, channel.moduleRevision);

        READ_FLAG(output_enabled, channel.flags.output_enabled);
        READ_FLAG(sense_enabled, channel.flags.sense_enabled);
        READ_FLAG(u_state, channel.flags.u_state);
        READ_FLAG(i_state, channel.flags.i_state);
        READ_FLAG(p_state, channel.flags.p_state);
        READ_FLAG(rprog_enabled, channel.flags.rprog_enabled);
        READ_FLAG(displayValue1, channel.flags.displayValue1);
        READ_FLAG(displayValue2, channel.flags.displayValue2);
        READ_FLAG(u_triggerMode, channel.flags.u_triggerMode);
        READ_FLAG(i_triggerMode, channel.flags.i_triggerMode);
        READ_FLAG(currentRangeSelectionMode, channel.flags.currentRangeSelectionMode);
        READ_FLAG(autoSelectCurrentRange, channel.flags.autoSelectCurrentRange);
        READ_FLAG(triggerOutputState, channel.flags.triggerOutputState);
        READ_FLAG(triggerOnListStop, channel.flags.triggerOnListStop);
        READ_FLAG(u_type, channel.flags.u_type);
        READ_FLAG(dprogState, channel.flags.dprogState);
        READ_FLAG(trackingEnabled, channel.flags.trackingEnabled);

        READ_PROPERTY(u_set, channel.u_set);
        READ_PROPERTY(u_step, channel.u_step);
        READ_PROPERTY(u_limit, channel.u_limit);
        READ_PROPERTY(u_delay, channel.u_delay);
        READ_PROPERTY(u_level, channel.u_level);
        READ_PROPERTY(i_set, channel.i_set);
        READ_PROPERTY(i_step, channel.i_step);
        READ_PROPERTY(i_limit, channel.i_limit);
        READ_PROPERTY(i_delay, channel.i_delay);
        READ_PROPERTY(p_limit, channel.p_limit);
        READ_PROPERTY(p_delay, channel.p_delay);
        READ_PROPERTY(p_level, channel.p_level);
        READ_PROPERTY(ytViewRate, channel.ytViewRate);
        READ_PROPERTY(u_triggerValue, channel.u_triggerValue);
        READ_PROPERTY(i_triggerValue, channel.i_triggerValue);
        READ_PROPERTY(listCount, channel.listCount);

        READ_LIST_PROPERTY(list, channelIndex);

#ifdef EEZ_PLATFORM_SIMULATOR
        READ_PROPERTY(load_enabled, channel.load_enabled);
        READ_PROPERTY(load, channel.load);
        READ_PROPERTY(voltProgExt, channel.voltProgExt);
#endif
    }

    int tempSensorIndex;
    if (ctx.matchGroup("tempsensor", tempSensorIndex)) {
        --tempSensorIndex;

        auto &tempSensorProt = parameters.temp_prot[tempSensorIndex];

        tempSensorProt.sensor = tempSensorIndex;

        SKIP_PROPERTY(name)
        READ_PROPERTY(delay, tempSensorProt.delay);
        READ_PROPERTY(level, tempSensorProt.level);
        READ_PROPERTY(state, tempSensorProt.state);
    }
}

bool profileRead(ReadContext &ctx, Parameters &parameters, bool showProgress) {
    return ctx.doRead(profileReadCallback, parameters, showProgress);
}

bool doRecallFromFile(const char *filePath, int recallOptions, bool showProgress, int *err) {
    if (!sd_card::isMounted(err)) {
        if (err) {
            *err = SCPI_ERROR_FILE_NOT_FOUND;
        }
        return false;
    }

    if (!sd_card::exists(filePath, NULL)) {
        if (err) {
            *err = SCPI_ERROR_FILE_NOT_FOUND;
        }
        return false;
    }

    File file;

    if (!file.open(filePath, FILE_OPEN_EXISTING | FILE_READ)) {
        if (err) {
            *err = SCPI_ERROR_MASS_STORAGE_ERROR;
        }
        return false;
    }

    Parameters profile;
    memset(&profile, 0, sizeof(Parameters));

    ReadContext ctx(file);

    bool result = profileRead(ctx, profile, showProgress);

    file.close();

    if (!result) {
        if (err) {
            *err = SCPI_ERROR_MASS_STORAGE_ERROR;
        }
        return false;
    }

    if (!doRecall(profile, recallOptions, err)) {
        return false;
    }

    return true;
}

bool recallFromLocation(int location) {
    if (location == 10) {
        // TODO
        return true;
    }
    return recallFromLocation(location, 0, false, nullptr);
}

bool recallFromLocation(int location, int recallOptions, bool showProgress, int *err) {
    char filePath[MAX_PATH_LENGTH];
    getProfileFilePath(location, filePath);
    if (doRecallFromFile(filePath, recallOptions, showProgress, err)) {
        event_queue::pushEvent(event_queue::EVENT_INFO_RECALL_FROM_PROFILE_0 + location);
        return true;
    }
    return false;
}

bool recallFromFile(const char *filePath, int recallOptions, bool showProgress, int *err) {
    if (doRecallFromFile(filePath, recallOptions, showProgress, err)) {
        event_queue::pushEvent(event_queue::EVENT_INFO_RECALL_FROM_FILE);
        return true;
    }
    return false;
}

////////////////////////////////////////////////////////////////////////////////

void getSaveName(int location, char *name) {
    Parameters *profile = profile::getProfileParameters(location);

    if (!profile || !profile->flags.isValid || strncmp(profile->name, AUTO_NAME_PREFIX, strlen(AUTO_NAME_PREFIX)) == 0) {
        strcpy(name, AUTO_NAME_PREFIX);
        datetime::getDateTimeAsString(name + strlen(AUTO_NAME_PREFIX));
    } else {
        strcpy(name, profile->name);
    }
}

////////////////////////////////////////////////////////////////////////////////

class WriteContext {
public:
    WriteContext(File &file_);

    bool group(const char *groupName);
    bool group(const char *groupNamePrefix, unsigned int index);

    void property(const char *propertyName, int value);
    void property(const char *propertyName, unsigned int value);
    void property(const char *propertyName, float value);
    void property(const char *propertyName, const char *str);
    void property(
        const char *propertyName,
        float *dwellList, uint16_t &dwellListLength,
        float *voltageList, uint16_t &voltageListLength,
        float *currentList, uint16_t &currentListLength
    );

    void flush();

private:
    sd_card::BufferedFileWrite file;
};

WriteContext::WriteContext(File &file_)
    : file(file_)
{
}

bool WriteContext::group(const char *groupName) {
    char line[256 + 1];
    sprintf(line, "[%s]\n", groupName);
    file.write((uint8_t *)line, strlen(line));
    return true;
}

bool WriteContext::group(const char *groupNamePrefix, unsigned int index) {
    char line[256 + 1];
    sprintf(line, "[%s%d]\n", groupNamePrefix, index);
    file.write((uint8_t *)line, strlen(line));
    return true;
}

void WriteContext::property(const char *propertyName, int value) {
    char line[256 + 1];
    sprintf(line, "\t%s=%d\n", propertyName, (int)value);
    file.write((uint8_t *)line, strlen(line));
}

void WriteContext::property(const char *propertyName, unsigned int value) {
    char line[256 + 1];
    sprintf(line, "\t%s=%u\n", propertyName, (int)value);
    file.write((uint8_t *)line, strlen(line));
}

void WriteContext::property(const char *propertyName, float value) {
    char line[256 + 1];
    sprintf(line, "\t%s=%g\n", propertyName, value);
    file.write((uint8_t *)line, strlen(line));
}

void WriteContext::property(const char *propertyName, const char *str) {
    char line[256 + 1];
    sprintf(line, "\t%s=\"%s\"\n", propertyName, str);
    file.write((uint8_t *)line, strlen(line));
}

void WriteContext::property(
    const char *propertyName,
    float *dwellList, uint16_t &dwellListLength,
    float *voltageList, uint16_t &voltageListLength,
    float *currentList, uint16_t &currentListLength
) {
    char line[256 + 1];
    sprintf(line, "\t%s=```\n", propertyName);
    file.write((uint8_t *)line, strlen(line));

    int err;
    list::saveList(file, dwellList, dwellListLength, voltageList, voltageListLength, currentList, currentListLength, false, &err);

    sprintf(line, "```\n");
    file.write((uint8_t *)line, strlen(line));
}

void WriteContext::flush() {
    file.flush();
}

////////////////////////////////////////////////////////////////////////////////

bool profileWrite(WriteContext &ctx, const Parameters &parameters, bool showProgress) {
#if OPTION_DISPLAY
    size_t processedSoFar = 0;
    static const int CH_PROGRESS_WEIGHT = 20;
    static const int TEMP_SENSOR_PROGRESS_WEIGHT = 5;
    size_t totalSize = 2 + CH_MAX * CH_PROGRESS_WEIGHT + temp_sensor::NUM_TEMP_SENSORS * TEMP_SENSOR_PROGRESS_WEIGHT;
    if (showProgress) {
        eez::psu::gui::PsuAppContext::updateProgressPage(processedSoFar, totalSize);
    }
#endif

    ctx.group("system");
    ctx.property("powerIsUp", parameters.flags.powerIsUp);
    ctx.property("profileName", parameters.name);

#if OPTION_DISPLAY
    if (showProgress) {
        processedSoFar++;
        eez::psu::gui::PsuAppContext::updateProgressPage(1, totalSize);
    }
#endif

    ctx.group("dcpsupply");
    ctx.property("couplingType", parameters.flags.couplingType);

#if OPTION_DISPLAY
    if (showProgress) {
        processedSoFar++;
        eez::psu::gui::PsuAppContext::updateProgressPage(2, totalSize);
    }
#endif

    for (int channelIndex = 0; channelIndex < CH_MAX; channelIndex++) {
        auto &channel = parameters.channels[channelIndex];
        if (channel.flags.parameters_are_valid) {
            ctx.group("dcpsupply.ch", channelIndex + 1);

            ctx.property("moduleType", channel.moduleType);
            ctx.property("moduleRevision", channel.moduleRevision);

            ctx.property("output_enabled", channel.flags.output_enabled);
            ctx.property("sense_enabled", channel.flags.sense_enabled);
            ctx.property("u_state", channel.flags.u_state);
            ctx.property("i_state", channel.flags.i_state);
            ctx.property("p_state", channel.flags.p_state);
            ctx.property("rprog_enabled", channel.flags.rprog_enabled);
            ctx.property("displayValue1", channel.flags.displayValue1);
            ctx.property("displayValue2", channel.flags.displayValue2);
            ctx.property("u_triggerMode", channel.flags.u_triggerMode);
            ctx.property("i_triggerMode", channel.flags.i_triggerMode);
            ctx.property("currentRangeSelectionMode", channel.flags.currentRangeSelectionMode);
            ctx.property("autoSelectCurrentRange", channel.flags.autoSelectCurrentRange);
            ctx.property("triggerOutputState", channel.flags.triggerOutputState);
            ctx.property("triggerOnListStop", channel.flags.triggerOnListStop);
            ctx.property("u_type", channel.flags.u_type);
            ctx.property("dprogState", channel.flags.dprogState);
            ctx.property("trackingEnabled", channel.flags.trackingEnabled);

            ctx.property("u_set", channel.u_set);
            ctx.property("u_step", channel.u_step);
            ctx.property("u_limit", channel.u_limit);
            ctx.property("u_delay", channel.u_delay);
            ctx.property("u_level", channel.u_level);
            ctx.property("i_set", channel.i_set);
            ctx.property("i_step", channel.i_step);
            ctx.property("i_limit", channel.i_limit);
            ctx.property("i_delay", channel.i_delay);
            ctx.property("p_limit", channel.p_limit);
            ctx.property("p_delay", channel.p_delay);
            ctx.property("p_level", channel.p_level);
            ctx.property("ytViewRate", channel.ytViewRate);
            ctx.property("u_triggerValue", channel.u_triggerValue);
            ctx.property("i_triggerValue", channel.i_triggerValue);
            ctx.property("listCount", channel.listCount);

            {
                auto &channel = Channel::get(channelIndex);

                uint16_t dwellListLength;
                float *dwellList = list::getDwellList(channel, &dwellListLength);

                uint16_t voltageListLength;
                float *voltageList = list::getVoltageList(channel, &voltageListLength);

                uint16_t currentListLength;
                float *currentList = list::getCurrentList(channel, &currentListLength);

                ctx.property(
                    "list",
                    dwellList,
                    dwellListLength,
                    voltageList,
                    voltageListLength,
                    currentList,
                    currentListLength
                );
            }

#ifdef EEZ_PLATFORM_SIMULATOR
            ctx.property("load_enabled", channel.load_enabled);
            ctx.property("load", channel.load);
            ctx.property("voltProgExt", channel.voltProgExt);
#endif
        }

#if OPTION_DISPLAY
        if (showProgress) {
            processedSoFar += CH_PROGRESS_WEIGHT;
            eez::psu::gui::PsuAppContext::updateProgressPage(processedSoFar, totalSize);
        }
#endif
    }

    for (int i = 0; i < temp_sensor::NUM_TEMP_SENSORS; ++i) {
        auto &tempSensorProt = parameters.temp_prot[i];
        auto &sensor = temperature::sensors[tempSensorProt.sensor];
        if (sensor.isInstalled()) {
            ctx.group("tempsensor", tempSensorProt.sensor + 1);

            ctx.property("name", sensor.getName());
            ctx.property("delay", tempSensorProt.delay);
            ctx.property("level", tempSensorProt.level);
            ctx.property("state", tempSensorProt.state);
        }

#if OPTION_DISPLAY
        if (showProgress) {
            processedSoFar += TEMP_SENSOR_PROGRESS_WEIGHT;
            eez::psu::gui::PsuAppContext::updateProgressPage(processedSoFar, totalSize);
        }
#endif
    }

    return true;
}

bool saveToLocation(int location) {
    if (location == 10) {
        // TODO
        return true;
    }
    return saveToLocation(location, nullptr, false, nullptr);
}

bool saveToLocation(int location, const char *name, bool showProgress, int *err) {
    char filePath[MAX_PATH_LENGTH];
    getProfileFilePath(location, filePath);
    return saveToFile(filePath, showProgress, err);
}

bool saveToFile(const char *filePath, bool showProgress, int *err) {
    if (!sd_card::isMounted(err)) {
        return false;
    }

    if (!sd_card::makeParentDir(filePath, err)) {
        return false;
    }

    File file;

    if (!file.open(filePath, FILE_CREATE_ALWAYS | FILE_WRITE)) {
        if (err)
            *err = SCPI_ERROR_MASS_STORAGE_ERROR;
        return false;
    }

    WriteContext ctx(file);

    bool result;

    Parameters profile;
    memset(&profile, 0, sizeof(Parameters));
    fillProfile(profile);
    result = profileWrite(ctx, profile, showProgress);

    ctx.flush();

    file.close();

    if (!result) {
        *err = SCPI_ERROR_MASS_STORAGE_ERROR;
        return false;
    }

    onSdCardFileChangeHook(filePath);

    return true;
}

////////////////////////////////////////////////////////////////////////////////

bool importFileToLocation(const char *filePath, int location, bool showProgress, int *err) {
    if (err) {
        *err = SCPI_ERROR_MASS_STORAGE_ERROR;
    }
    return false;
}

bool exportLocationToFile(int location, const char *filePath, bool showProgress, int *err) {
    if (err) {
        *err = SCPI_ERROR_MASS_STORAGE_ERROR;
    }
    return false;
}

////////////////////////////////////////////////////////////////////////////////

bool deleteLocation(int location, bool showProgress, int *err) {
    if (location > 0 && location < NUM_PROFILE_LOCATIONS) {
        if (g_profilesCache[location].loaded) {
            g_profilesCache[location].profile.flags.isValid = false;
        }

        if (location == persist_conf::getProfileAutoRecallLocation()) {
            persist_conf::setProfileAutoRecallLocation(0);
        }

        char filePath[MAX_PATH_LENGTH];
        getProfileFilePath(location, filePath);
        if (!sd_card::exists(filePath, err)) {
            return false;
        }

        if (!sd_card::deleteFile(filePath, err)) {
            return false;
        }

        return true;
    } else {
        *err = SCPI_ERROR_EXECUTION_ERROR;
        return false;
    }
}

bool deleteAllLocations(int *err) {
    for (int i = 1; i < NUM_PROFILE_LOCATIONS; ++i) {
        if (!deleteLocation(i, false, err)) {
            return false;
        }
    }

    return true;
}

////////////////////////////////////////////////////////////////////////////////

bool isValid(int location) {
    if (location >= 0 && location < NUM_PROFILE_LOCATIONS) {
        Parameters *profile = getProfileParameters(location);
        if (profile) {
            return profile->flags.isValid;
        }
    }
    return false;
}

////////////////////////////////////////////////////////////////////////////////

bool setName(int location, const char *name, size_t name_len, bool showProgress, int *err) {
    if (location > 0 && location < NUM_PROFILE_LOCATIONS) {
        Parameters *profile = getProfileParameters(location);
        if (profile && profile->flags.isValid) {
            char savedName[PROFILE_NAME_MAX_LENGTH + 1];

            strcpy(savedName, profile->name);

            strncpy(profile->name, name, name_len);
            profile->name[name_len] = 0;

            if (!saveProfile(location, showProgress, err)) {
                strcpy(profile->name, savedName);
                return false;
            }

            return true;
        }
    }

    *err = SCPI_ERROR_EXECUTION_ERROR;
    return false;
}

void getName(int location, char *name, int count) {
    if (location >= 0 && location < NUM_PROFILE_LOCATIONS) {
        Parameters *profile = getProfileParameters(location);
        if (profile && profile->flags.isValid) {
            strncpy(name, profile->name, count - 1);
            name[count - 1] = 0;
            return;
        }
    }
    if (location > 0) {
        strncpy(name, "--Empty--", count - 1);
    } else {
        strncpy(name, "--Never used--", count - 1);
    }
    name[count - 1] = 0;
}

////////////////////////////////////////////////////////////////////////////////

bool getFreezeState() {
    return g_freeze;
}

void setFreezeState(bool value) {
    g_freeze = value;
}

} // namespace profile
} // namespace psu
} // namespace eez
