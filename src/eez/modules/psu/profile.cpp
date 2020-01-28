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
#if OPTION_SD_CARD
#include <eez/modules/psu/sd_card.h>
#include <eez/libs/sd_fat/sd_fat.h>
#endif
#include <eez/scpi/scpi.h>

namespace eez {

using namespace scpi;

namespace psu {
namespace profile {

#define AUTO_NAME_PREFIX "Saved at "

static bool g_saveEnabled = true;
bool g_profileDirty;
static bool g_freeze;

////////////////////////////////////////////////////////////////////////////////

#if OPTION_SD_CARD

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

static void getChannelProfileListFilePath(int channelIndex, int location, char *filePath) {
    strcpy(filePath, LISTS_DIR);
    strcat(filePath, PATH_SEPARATOR);
    strcat(filePath, "PROFILE_");
    strcatInt(filePath, channelIndex + 1);
    strcat(filePath, "_");
    strcatInt(filePath, location);
    strcat(filePath, LIST_EXT);
}

void loadProfileList(Parameters &profile, Channel &channel, int profileChannelIndex, int location) {
    int err;
    if (!sd_card::isMounted(&err)) {
        if (err != SCPI_ERROR_MISSING_MASS_MEDIA && err != SCPI_ERROR_MASS_MEDIA_NO_FILESYSTEM) {
    	    generateError(err);
        }
        return;
    }

    char filePath[MAX_PATH_LENGTH];
    getChannelProfileListFilePath(profileChannelIndex, location, filePath);

    if (!sd_card::exists(filePath, &err)) {
        return;
    }
    
    if (list::loadList(channel.channelIndex, filePath, &err)) {
        if (location == 0) {
            list::setListsChanged(channel, false);
        }
    }
    else {
        generateError(err);
    }
}

void saveProfileList(Parameters &profile, Channel &channel, int profileChannelIndex, int location) {
    if (location == 0 && !list::getListsChanged(channel)) {
        return;
    }

    char filePath[MAX_PATH_LENGTH];
    getChannelProfileListFilePath(profileChannelIndex, location, filePath);
    
    int err;
    if (list::saveList(channel.channelIndex, filePath, &err)) {
        if (location == 0) {
            list::setListsChanged(channel, false);
        }
        uint8_t hour, minute, second;
        datetime::getTime(hour, minute, second);
        DebugTrace("[%02d:%02d:%02d] List for profile at location %d and channel %d saved!\n", hour, minute, second, location, channel.channelIndex + 1);
    } else {
        generateError(err);
    }
}

void saveProfileListForAllChannels(Parameters &profile, int location) {
    int channelsMap[CH_MAX];
    mapCurrentChannelsToProfileChannels(profile, channelsMap);

    for (int i = 0; i < CH_NUM; ++i) {
        Channel &channel = Channel::get(i);
        saveProfileList(profile, channel, channelsMap[i], location);
    }
}

void deleteProfileList(int channelIndex, int location) {
    char filePath[MAX_PATH_LENGTH];
    getChannelProfileListFilePath(channelIndex, location, filePath);

    int err;

    if (!sd_card::exists(filePath, &err)) {
        return;
    }

    if (!sd_card::deleteFile(filePath, &err)) {
        generateError(err);
    }
}

void deleteProfileLists(int location) {
#if OPTION_SD_CARD
    int err;
    if (!sd_card::isMounted(&err)) {
        if (err != SCPI_ERROR_MISSING_MASS_MEDIA && err != SCPI_ERROR_MASS_MEDIA_NO_FILESYSTEM) {
    	    generateError(err);
        }
        return;
    }

    for (int i = 0; i < CH_MAX; ++i) {
        deleteProfileList(i, location);
    }
#endif
}

#endif

////////////////////////////////////////////////////////////////////////////////

void recallChannelsFromProfile(Parameters &profile, int location) {
    bool wasSaveProfileEnabled = enableSave(false);

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

            channel.flags.outputEnabled = channel.isTripped() || mismatch ? 0 : profile.channels[j].flags.output_enabled;
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

#if OPTION_SD_CARD
            loadProfileList(profile, channel, j, location);
#endif
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

    enableSave(wasSaveProfileEnabled);
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

            if (j != channelsMap[i]) {
                list::setListsChanged(channel, true);
            }
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

bool isAutoSaveAllowed() {
    return persist_conf::devConf.profileAutoRecallEnabled && persist_conf::devConf.profile_auto_recall_location == 0;
}

bool enableSave(bool enable) {
    bool wasEnabled = g_saveEnabled;
    g_saveEnabled = enable;
    return wasEnabled;
}

void doSave() {
    Parameters profile;

    Parameters *existingProfile = load(0);
    if (existingProfile) {
        memcpy(&profile, existingProfile, sizeof(Parameters));
    } else {
        memset(&profile, 0, sizeof(Parameters));
    }

    fillProfile(profile);

    persist_conf::saveProfile(0, &profile);

#if OPTION_SD_CARD
    saveProfileListForAllChannels(profile, 0);
#endif
}

void save(bool immediately) {
    if (g_saveEnabled && !g_freeze && isAutoSaveAllowed()) {
        if (immediately) {
            doSave();
            g_profileDirty = false;
        } else {
            g_profileDirty = true;
        }
    }
}

void saveIfDirty() {
    if (g_profileDirty) {
        doSave();
        g_profileDirty = false;
    }
}

void init() {
    for (int profileIndex = 0; profileIndex < NUM_PROFILE_LOCATIONS; profileIndex++) {
        persist_conf::loadProfile(profileIndex);
    }
}

void tick() {
    if (g_profileDirty && !list::isActive() && !calibration::isEnabled() && idle::isIdle()) {
        doSave();
        g_profileDirty = false;
    }
}

////////////////////////////////////////////////////////////////////////////////

bool checkProfileModuleMatch(Parameters &profile) {
    return true;
}

bool recallFromProfile(Parameters &profile, int location) {
    bool wasSaveProfileEnabled = enableSave(false);

    bool result = true;

    if (profile.flags.powerIsUp) {
        result &= powerUp();
    } else {
        powerDown();
    }

    for (int i = 0; i < temp_sensor::NUM_TEMP_SENSORS; ++i) {
        memcpy(&temperature::sensors[i].prot_conf, profile.temp_prot + i, sizeof(temperature::ProtectionConfiguration));
    }

    recallChannelsFromProfile(profile, location);

    enableSave(wasSaveProfileEnabled);

    return result;
}

bool recall(int location, int *err) {
    if (location > 0 && location < NUM_PROFILE_LOCATIONS) {
        Parameters *profile = persist_conf::loadProfile(location);
        if (profile && profile->flags.isValid) {
            if (recallFromProfile(*profile, location)) {
                save();
                if (location >= 0 && location <= 9) {
                    event_queue::pushEvent(event_queue::EVENT_INFO_RECALL_FROM_PROFILE_0 + location);
                }
                return true;
            } else {
                return false;
            }
        }
    }

    *err = SCPI_ERROR_CANNOT_LOAD_EMPTY_PROFILE;
    return false;
}

bool recallFromFile(const char *filePath, int *err) {
#if OPTION_SD_CARD
    if (!sd_card::isMounted(err)) {
        return false;
    }

    if (!sd_card::exists(filePath, NULL)) {
        if (err)
            *err = SCPI_ERROR_LIST_NOT_FOUND;
        return false;
    }

    File file;

    if (!file.open(filePath, FILE_OPEN_EXISTING | FILE_READ)) {
        if (err)
            *err = SCPI_ERROR_MASS_STORAGE_ERROR;
        return false;
    }

    Parameters profile;
    int size = file.read(&profile, sizeof(profile));
    file.close();

    if (size != sizeof(profile) || !persist_conf::checkBlock((const persist_conf::BlockHeader *)&profile, sizeof(profile), PROFILE_VERSION)) {
        if (err)
            *err = SCPI_ERROR_MASS_STORAGE_ERROR;
        return false;
    }

    persist_conf::saveProfile(0, &profile);

    if (!recallFromProfile(profile, 0)) {
        // TODO replace with more specific error
        if (err)
            *err = SCPI_ERROR_EXECUTION_ERROR;
        return false;
    }

    event_queue::pushEvent(event_queue::EVENT_INFO_RECALL_FROM_FILE);
    return true;
#else
    if (err)
        *err = SCPI_ERROR_HARDWARE_MISSING;
    return false;
#endif
}

Parameters *load(int location) {
    if (location >= 0 && location < NUM_PROFILE_LOCATIONS) {
        Parameters *profile = persist_conf::loadProfile(location);
        if (profile && profile->flags.isValid) {
            return profile;
        }
    }
    return nullptr;
}

////////////////////////////////////////////////////////////////////////////////

void getSaveName(int location, char *name) {
    Parameters *profile = persist_conf::loadProfile(location);

    if (!profile || !profile->flags.isValid || strncmp(profile->name, AUTO_NAME_PREFIX, strlen(AUTO_NAME_PREFIX)) == 0) {
        strcpy(name, AUTO_NAME_PREFIX);
        datetime::getDateTimeAsString(name + strlen(AUTO_NAME_PREFIX));
    } else {
        strcpy(name, profile->name);
    }
}

bool saveAtLocation(int location, const char *name) {
    if (location >= 0 && location < NUM_PROFILE_LOCATIONS) {
        Parameters profile;
        memset(&profile, 0, sizeof(Parameters));
        fillProfile(profile);

        if (name) {
            strcpy(profile.name, name);
        } else {
            getSaveName(location, profile.name);
        }

        persist_conf::saveProfile(location, &profile);
        saveProfileListForAllChannels(profile, location);

        return true;
    }

    return false;
}

bool saveToFile(const char *filePath, int *err) {
#if OPTION_SD_CARD
    if (!sd_card::isMounted(err)) {
        return false;
    }

    sd_card::makeParentDir(filePath);

    sd_card::deleteFile(filePath, NULL);

    File file;

    if (!file.open(filePath, FILE_CREATE_ALWAYS | FILE_WRITE)) {
        if (err)
            *err = SCPI_ERROR_MASS_STORAGE_ERROR;
        return false;
    }

    Parameters profile;
    memset(&profile, 0, sizeof(Parameters));
    fillProfile(profile);

    profile.header.version = PROFILE_VERSION;
    profile.header.checksum = persist_conf::calcChecksum((const persist_conf::BlockHeader *)&profile, sizeof(profile));

    size_t size = file.write((const uint8_t *)&profile, sizeof(profile));
    file.close();

    if (size != sizeof(profile)) {
        *err = SCPI_ERROR_MASS_STORAGE_ERROR;
        return false;
    }

    return true;
#else
    if (err) {
        *err = SCPI_ERROR_HARDWARE_MISSING;
    }
    return false;
#endif
}

////////////////////////////////////////////////////////////////////////////////

bool deleteLocation(int location) {
    if (location > 0 && location < NUM_PROFILE_LOCATIONS) {
        Parameters profile;
        profile.flags.isValid = false;
        if (location == persist_conf::getProfileAutoRecallLocation()) {
            persist_conf::setProfileAutoRecallLocation(0);
        }
        persist_conf::saveProfile(location, &profile);

        if (osThreadGetId() != g_scpiTaskHandle) {
            osMessagePut(g_scpiMessageQueueId, SCPI_QUEUE_MESSAGE(SCPI_QUEUE_MESSAGE_TARGET_NONE, SCPI_QUEUE_MESSAGE_TYPE_DELETE_PROFILE_LISTS, location), osWaitForever);
            return true;
        }
    }

    return true;
}

bool deleteAll() {
    for (int i = 1; i < NUM_PROFILE_LOCATIONS; ++i) {
        if (!deleteLocation(i)) {
            return false;
        }
    }
    return true;
}

bool isValid(int location) {
    if (location >= 0 && location < NUM_PROFILE_LOCATIONS) {
        Parameters *profile = persist_conf::loadProfile(location);
        if (profile) {
            return profile->flags.isValid;
        }
    }
    return false;
}

bool setName(int location, const char *name, size_t name_len) {
    if (location > 0 && location < NUM_PROFILE_LOCATIONS) {
        Parameters *profile = persist_conf::loadProfile(location);
        if (profile && profile->flags.isValid) {
            memset(profile->name, 0, sizeof(profile->name));
            strncpy(profile->name, name, name_len);
            persist_conf::saveProfile(location, profile);
            return true;
        }
    }
    return false;
}

void getName(int location, char *name, int count) {
    if (location >= 0 && location < NUM_PROFILE_LOCATIONS) {
        Parameters *profile = persist_conf::loadProfile(location);
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

bool getFreezeState() {
    return g_freeze;
}

void setFreezeState(bool value) {
    g_freeze = value;
}

} // namespace profile
} // namespace psu
} // namespace eez
