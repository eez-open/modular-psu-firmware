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

#include <eez/scpi/scpi.h>

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

#define CONF_AUTO_SAVE_TIMEOUT_MS 60 * 1000
#define CONF_AUTO_NAME_PREFIX "Saved at "

#define CONF_PROFILE_SAVE_TIMEOUT_MS 2000

namespace eez {
namespace psu {
namespace profile {

struct List {
    float dwellList[MAX_LIST_LENGTH];
    uint16_t dwellListLength;

    float voltageList[MAX_LIST_LENGTH];
    uint16_t voltageListLength;

    float currentList[MAX_LIST_LENGTH];
    uint16_t currentListLength;
};

   
static uint32_t g_lastAutoSaveTime;
static bool g_freeze;
static profile::Parameters g_profilesCache[NUM_PROFILE_LOCATIONS];
static List g_listsProfile0[CH_MAX];
static List g_listsProfile10[CH_MAX];

////////////////////////////////////////////////////////////////////////////////

static void loadProfileName(int location);
static Parameters *getProfileParametersFromCache(int location);

static void getProfileFilePath(int location, char *filePath);

static void resetProfileToDefaults(Parameters &profile);

static void saveState(Parameters &profile, List *lists);
static bool recallState(Parameters &profile, List *lists, int recallOptions, int *err);

static bool saveProfileToFile(const char *filePath, Parameters &profile, List *lists, bool showProgress, int *err);
static void saveStateToProfile0(bool merge);

enum {
    LOAD_PROFILE_FROM_FILE_OPTION_ONLY_NAME = 0x01
};
static bool loadProfileFromFile(const char *filePath, Parameters &profile, List *lists, int options, bool showProgress, int *err);

static bool doSaveToLastLocation(int *err);
static bool doRecallFromLastLocation(int *err);

static bool isTickSaveAllowed();
static bool isAutoSaveAllowed();
static bool isProfile0Dirty();

////////////////////////////////////////////////////////////////////////////////

void init() {
	onAfterSdCardMounted();
}

void tick() {
    auto tick = millis();
    if (tick - g_lastAutoSaveTime > CONF_AUTO_SAVE_TIMEOUT_MS) {
        g_lastAutoSaveTime = tick;
        if (isTickSaveAllowed() && isAutoSaveAllowed() && isProfile0Dirty()) {
            saveStateToProfile0(true);
        }
    }
}

void onAfterSdCardMounted() {
    for (int profileIndex = 1; profileIndex < NUM_PROFILE_LOCATIONS; profileIndex++) {
		loadProfileName(profileIndex);
    }
}

void shutdownSave() {
    if (isAutoSaveAllowed() && isProfile0Dirty()) {
        saveStateToProfile0(true);
    }
}

Parameters *getProfileParameters(int location) {
    return getProfileParametersFromCache(location);
}

////////////////////////////////////////////////////////////////////////////////

bool recallFromLocation(int location) {
    if (location == NUM_PROFILE_LOCATIONS - 1) {
        int err;
        if (!doRecallFromLastLocation(&err)) {
            generateError(err);
            return false;
        }
        return true;
    }

    return recallFromLocation(location, 0, false, nullptr);
}

bool recallFromLocation(int location, int recallOptions, bool showProgress, int *err) {
    if (location == NUM_PROFILE_LOCATIONS - 1) {
        return doRecallFromLastLocation(err);
    }

    char filePath[MAX_PATH_LENGTH];
    getProfileFilePath(location, filePath);

    Parameters profile;
    resetProfileToDefaults(profile);
    if (!loadProfileFromFile(filePath, profile, g_listsProfile0, 0, showProgress, err)) {
        return false;
    }

    if (!recallState(profile, g_listsProfile0, recallOptions, err)) {
        return false;
    }

    if (location == 0) {
        if (!(recallOptions & profile::RECALL_OPTION_IGNORE_POWER)) {
            // save to cache
            memcpy(&g_profilesCache[0], &profile, sizeof(profile));
            saveState(g_profilesCache[0], g_listsProfile0);
            g_profilesCache[0].loadStatus = LOAD_STATUS_LOADED;
        }
    } else {
        event_queue::pushEvent(event_queue::EVENT_INFO_RECALL_FROM_PROFILE_0 + location);

        if (isAutoSaveAllowed()) {
            saveStateToProfile0(false);
        }        
    }

    return true;
}

bool recallFromFile(const char *filePath, int recallOptions, bool showProgress, int *err) {
    Parameters profile;
    resetProfileToDefaults(profile);
    if (!loadProfileFromFile(filePath, profile, g_listsProfile0, 0, showProgress, err)) {
        return false;
    }

    if (!recallState(profile, g_listsProfile0, recallOptions, err)) {
        return false;
    }

    event_queue::pushEvent(event_queue::EVENT_INFO_RECALL_FROM_FILE);

    if (isAutoSaveAllowed()) {
        saveStateToProfile0(false);
    }

    return true;
}

////////////////////////////////////////////////////////////////////////////////

bool saveToLocation(int location) {
    if (location == NUM_PROFILE_LOCATIONS - 1) {
        int err;
        if (!doSaveToLastLocation(&err)) {
            generateError(err);
            return false;
        }
        return true;
    }

    return saveToLocation(location, nullptr, false, nullptr);
}

bool saveToLocation(int location, const char *name, bool showProgress, int *err) {
    if (location == NUM_PROFILE_LOCATIONS - 1) {
        return doSaveToLastLocation(err);
    }

    char filePath[MAX_PATH_LENGTH];
    getProfileFilePath(location, filePath);

    Parameters profile;
    memset(&profile, 0, sizeof(Parameters));
    saveState(profile, nullptr);
    if (name) {
        strcpy(profile.name, name);
    }

    if (!saveProfileToFile(filePath, profile, nullptr, showProgress, err)) {
        return false;
    }

    // save to cache
    memcpy(&g_profilesCache[location], &profile, sizeof(profile));

    return true;
}

bool saveToFile(const char *filePath, bool showProgress, int *err) {
    Parameters profile;
    memset(&profile, 0, sizeof(Parameters));
    saveState(profile, nullptr);
    return saveProfileToFile(filePath, profile, nullptr, showProgress, err);
}

////////////////////////////////////////////////////////////////////////////////

bool importFileToLocation(const char *filePath, int location, bool showProgress, int *err) {
    char profileFilePath[MAX_PATH_LENGTH];
    getProfileFilePath(location, profileFilePath);
    if (sd_card::copyFile(filePath, profileFilePath, true, err)) {
        loadProfileParametersToCache(location);
        return true;
    }
    return false;
}

bool exportLocationToFile(int location, const char *filePath, bool showProgress, int *err) {
    char profileFilePath[MAX_PATH_LENGTH];
    getProfileFilePath(location, profileFilePath);
    return sd_card::copyFile(profileFilePath, filePath, true, err);
}

////////////////////////////////////////////////////////////////////////////////

bool deleteLocation(int location, bool showProgress, int *err) {
    if (location > 0 && location < NUM_PROFILE_LOCATIONS) {
        if (location == persist_conf::getProfileAutoRecallLocation()) {
            persist_conf::setProfileAutoRecallLocation(0);
        }

        g_profilesCache[location].flags.isValid = false;

        char filePath[MAX_PATH_LENGTH];
        getProfileFilePath(location, filePath);
        if (!sd_card::exists(filePath, err)) {
            return true;
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

bool isLoaded(int location) {
    if (location >= 0 && location < NUM_PROFILE_LOCATIONS) {
        if (g_profilesCache[location].loadStatus == LOAD_STATUS_LOADED) {
            return true;
        }
        loadProfileParametersToCache(location);
    }
    return false;
}

bool isValid(int location) {
    if (location >= 0 && location < NUM_PROFILE_LOCATIONS) {
        Parameters *profile = getProfileParametersFromCache(location);
        if (profile) {
            return profile->flags.isValid;
        }
    }
    return false;
}

////////////////////////////////////////////////////////////////////////////////

void getSaveName(int location, char *name) {
    Parameters *profile = profile::getProfileParametersFromCache(location);

    if (!profile || !profile->flags.isValid || strncmp(profile->name, CONF_AUTO_NAME_PREFIX, strlen(CONF_AUTO_NAME_PREFIX)) == 0) {
        strcpy(name, CONF_AUTO_NAME_PREFIX);
        datetime::getDateTimeAsString(name + strlen(CONF_AUTO_NAME_PREFIX));
    } else {
        strcpy(name, profile->name);
    }
}


////////////////////////////////////////////////////////////////////////////////

bool setName(int location, const char *name, bool showProgress, int *err) {
    if (location > 0 && location < NUM_PROFILE_LOCATIONS) {
        Parameters *profileFromCache = getProfileParametersFromCache(location);
        if (profileFromCache && profileFromCache->flags.isValid) {

            char filePath[MAX_PATH_LENGTH];
            getProfileFilePath(location, filePath);

            Parameters profile;
            resetProfileToDefaults(profile);
            if (!loadProfileFromFile(filePath, profile, g_listsProfile10, 0, false, err)) {
                return false;
            }

            memcpy(&profile, profileFromCache, sizeof(profile));
            if (name) {
                strcpy(profile.name, name);
            }

            if (!saveProfileToFile(filePath, profile, g_listsProfile10, showProgress, err)) {
                return false;
            }

            memcpy(profileFromCache, &profile, sizeof(profile));

            return true;
        }
    }

    *err = SCPI_ERROR_EXECUTION_ERROR;
    return false;
}

void getName(int location, char *name, int count) {
    if (location >= 0 && location < NUM_PROFILE_LOCATIONS) {
        if (g_profilesCache[location].flags.isValid) {
            strncpy(name, g_profilesCache[location].name, count - 1);
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

////////////////////////////////////////////////////////////////////////////////

void loadProfileParametersToCache(int location) {
    using namespace eez::scpi;

    if (osThreadGetId() != g_scpiTaskHandle) {
        if (g_profilesCache[location].loadStatus == LOAD_STATUS_LOADING) {
            return;
        }
       
        g_profilesCache[location].loadStatus = LOAD_STATUS_LOADING;
        
        osMessagePut(g_scpiMessageQueueId, SCPI_QUEUE_MESSAGE(SCPI_QUEUE_MESSAGE_TARGET_NONE, SCPI_QUEUE_MESSAGE_TYPE_LOAD_PROFILE, location), osWaitForever);
    } else {
        char filePath[MAX_PATH_LENGTH];
        getProfileFilePath(location, filePath);
        int err;
        if (!loadProfileFromFile(filePath, g_profilesCache[location], nullptr, 0, false, &err)) {
            if (err != SCPI_ERROR_FILE_NOT_FOUND && err != SCPI_ERROR_MISSING_MASS_MEDIA) {
                generateError(err);
            }
        }

        g_profilesCache[location].loadStatus = LOAD_STATUS_LOADED;
    }
}

////////////////////////////////////////////////////////////////////////////////

static void loadProfileName(int location) {
    char filePath[MAX_PATH_LENGTH];
    getProfileFilePath(location, filePath);
    int err;
    if (!loadProfileFromFile(filePath, g_profilesCache[location], nullptr, LOAD_PROFILE_FROM_FILE_OPTION_ONLY_NAME, false, &err)) {
        if (err != SCPI_ERROR_FILE_NOT_FOUND && err != SCPI_ERROR_MISSING_MASS_MEDIA) {
            generateError(err);
        }
    }
    g_profilesCache[location].loadStatus = LOAD_STATUS_ONLY_NAME;
}

static Parameters *getProfileParametersFromCache(int location) {
    if (location >= 0 && location < NUM_PROFILE_LOCATIONS - 1) {
        if (g_profilesCache[location].loadStatus == LOAD_STATUS_LOADED) {
            Parameters *profile = &g_profilesCache[location];
            if (profile->flags.isValid) {
                return profile;
            }
        } else {
            loadProfileParametersToCache(location);
        }
    }
    return nullptr;
}

////////////////////////////////////////////////////////////////////////////////

static void getProfileFilePath(int location, char *filePath) {
    strcpy(filePath, PROFILES_DIR);
    strcat(filePath, PATH_SEPARATOR);
    strcatInt(filePath, location);
    strcat(filePath, getExtensionFromFileType(FILE_TYPE_PROFILE));
}

static void resetProfileToDefaults(Parameters &profile) {
    memset(&profile, 0, sizeof(Parameters));

    for (int i = 0; i < CH_MAX; i++) {
        profile.channels[i].u_rampState = false;
        profile.channels[i].u_rampDuration = RAMP_DURATION_DEF_VALUE;
        profile.channels[i].i_rampState = false;
        profile.channels[i].i_rampDuration = RAMP_DURATION_DEF_VALUE;

        profile.channels[i].flags.outputDelayState = 0;
        profile.channels[i].outputDelayDuration = 0;
    }
}

static bool repositionChannelsInProfileToMatchCurrentChannelConfiguration(Parameters &profile, List *lists) {
    bool profileChannelAlreadyUsed[CH_MAX];
    int channelsMap[CH_MAX];
    for (int i = 0; i < CH_MAX; i++) {
        profileChannelAlreadyUsed[i] = false;
        channelsMap[i] = -1;
    }

    for (int i = 0; i < CH_NUM; i++) {
        Channel &channel = Channel::get(i);
        if (
            profile.channels[i].flags.parameters_are_valid &&
            profile.channels[i].moduleType == g_slots[channel.slotIndex].moduleInfo->moduleType
        ) {
            profileChannelAlreadyUsed[i] = true;
            channelsMap[i] = i;
        } else {
            break;
        }
    }

    for (int i = 0; i < CH_NUM; i++) {
        if (channelsMap[i] == -1) {
            Channel &channel = Channel::get(i);
            for (int j = 0; j < CH_MAX; j++) {
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

    for (int i = 0; i < CH_NUM; i++) {
        if (channelsMap[i] == -1) {
            for (int j = 0; j < CH_MAX; j++) {
                if (!profileChannelAlreadyUsed[j] && !profile.channels[j].flags.parameters_are_valid) {
                    profileChannelAlreadyUsed[j] = true;
                    channelsMap[i] = j;
                    break;
                }
            }
        }
    }

    for (int i = 0; i < CH_MAX; i++) {
        if (channelsMap[i] == -1) {
            for (int j = 0; j < CH_MAX; j++) {
                if (!profileChannelAlreadyUsed[j]) {
                    profileChannelAlreadyUsed[j] = true;
                    channelsMap[i] = j;
                    break;
                }
            }
        }
    }

    bool mismatch = false;

    for (int i = 0; i < CH_MAX - 1; i++) {
        if (channelsMap[i] != i) {
            for (int j = i + 1; j < CH_MAX; j++) {
                if (channelsMap[j] == i) {
                    mismatch = true;

                    int a = channelsMap[i];
                    int b = channelsMap[j];

                    ChannelParameters channelParameters;
                    memcpy(&channelParameters, &profile.channels[a], sizeof(ChannelParameters));
                    memcpy(&profile.channels[a], &profile.channels[b], sizeof(ChannelParameters));
                    memcpy(&profile.channels[b], &channelParameters, sizeof(ChannelParameters));

                    temperature::ProtectionConfiguration tempProt;
                    memcpy(&tempProt, &profile.tempProt[temp_sensor::CH1 + a], sizeof(temperature::ProtectionConfiguration));
                    memcpy(&profile.tempProt[temp_sensor::CH1 + a], &profile.tempProt[temp_sensor::CH1 + b], sizeof(temperature::ProtectionConfiguration));
                    memcpy(&profile.tempProt[temp_sensor::CH1 + b], &tempProt, sizeof(temperature::ProtectionConfiguration));

                    List list;
                    memcpy(&list, &lists[a], sizeof(List));
                    memcpy(&lists[a], &lists[b], sizeof(List));
                    memcpy(&lists[b], &list, sizeof(List));

                    channelsMap[j] = channelsMap[i];
                    channelsMap[i] = i;
                    break;
                }
            }
        }
    }

    return mismatch;
}

////////////////////////////////////////////////////////////////////////////////

static void saveState(Parameters &profile, List *lists) {
    profile.flags.couplingType = channel_dispatcher::getCouplingType();
    profile.flags.powerIsUp = isPowerUp();

    for (int i = 0; i < CH_MAX; ++i) {
        if (i < CH_NUM) {
            Channel &channel = Channel::get(i);

            profile.channels[i].moduleType = g_slots[channel.slotIndex].moduleInfo->moduleType;
            profile.channels[i].moduleRevision = g_slots[channel.slotIndex].moduleRevision;

            profile.channels[i].flags.parameters_are_valid = 1;

            profile.channels[i].flags.output_enabled = channel.flags.outputEnabled;
            profile.channels[i].flags.sense_enabled = channel.flags.senseEnabled;

            if (channel.params.features & CH_FEATURE_RPROG) {
                profile.channels[i].flags.rprog_enabled = channel.flags.rprogEnabled;
            } else {
                profile.channels[i].flags.rprog_enabled = 0;
            }

            profile.channels[i].flags.u_state = channel.prot_conf.flags.u_state;
            profile.channels[i].flags.u_type = channel.prot_conf.flags.u_type;
            profile.channels[i].flags.i_state = channel.prot_conf.flags.i_state;
            profile.channels[i].flags.p_state = channel.prot_conf.flags.p_state;

            profile.channels[i].u_set = channel.getUSetUnbalanced();
            profile.channels[i].u_step = channel.u.step;
            profile.channels[i].u_limit = channel.u.limit;

            profile.channels[i].i_set = channel.getISetUnbalanced();
            profile.channels[i].i_step = channel.i.step;
            profile.channels[i].i_limit = channel.i.limit;

            profile.channels[i].p_limit = channel.p_limit;

            profile.channels[i].u_delay = channel.prot_conf.u_delay;
            profile.channels[i].u_level = channel.prot_conf.u_level;
            profile.channels[i].i_delay = channel.prot_conf.i_delay;
            profile.channels[i].p_delay = channel.prot_conf.p_delay;
            profile.channels[i].p_level = channel.prot_conf.p_level;

            profile.channels[i].flags.displayValue1 = channel.flags.displayValue1;
            profile.channels[i].flags.displayValue2 = channel.flags.displayValue2;
            profile.channels[i].ytViewRate = channel.ytViewRate;

#ifdef EEZ_PLATFORM_SIMULATOR
            profile.channels[i].load_enabled = channel.simulator.load_enabled;
            profile.channels[i].load = channel.simulator.load;
            profile.channels[i].voltProgExt = channel.simulator.voltProgExt;
#endif

            profile.channels[i].flags.u_triggerMode = channel.flags.voltageTriggerMode;
            profile.channels[i].flags.i_triggerMode = channel.flags.currentTriggerMode;
            profile.channels[i].flags.triggerOutputState = channel.flags.triggerOutputState;
            profile.channels[i].flags.triggerOnListStop = channel.flags.triggerOnListStop;
            profile.channels[i].u_triggerValue = trigger::getVoltage(channel);
            profile.channels[i].i_triggerValue = trigger::getCurrent(channel);
            profile.channels[i].listCount = list::getListCount(channel);

            profile.channels[i].flags.currentRangeSelectionMode = channel.flags.currentRangeSelectionMode;
            profile.channels[i].flags.autoSelectCurrentRange = channel.flags.autoSelectCurrentRange;

            profile.channels[i].flags.dprogState = channel.flags.dprogState;
            profile.channels[i].flags.trackingEnabled = channel.flags.trackingEnabled;

            profile.channels[i].u_rampState = channel.u.rampState;
            profile.channels[i].u_rampDuration = channel.u.rampDuration;
            profile.channels[i].i_rampState = channel.i.rampState;
            profile.channels[i].i_rampDuration = channel.i.rampDuration;

            profile.channels[i].flags.outputDelayState = channel.flags.outputDelayState;
            profile.channels[i].outputDelayDuration = channel.outputDelayDuration;

            if (lists) {
                auto &list = lists[i];
                memcpy(list.dwellList, list::getDwellList(channel, &list.dwellListLength), sizeof(list.dwellList));
                memcpy(list.voltageList, list::getVoltageList(channel, &list.voltageListLength), sizeof(list.voltageList));
                memcpy(list.currentList, list::getCurrentList(channel, &list.currentListLength), sizeof(list.currentList));
            }
        }
    }

    for (int i = 0; i < temp_sensor::MAX_NUM_TEMP_SENSORS; ++i) {
        if (i < temp_sensor::NUM_TEMP_SENSORS) {
            memcpy(profile.tempProt + i, &temperature::sensors[i].prot_conf, sizeof(temperature::ProtectionConfiguration));
        } else {
            if (i == temp_sensor::AUX) {
                profile.tempProt[i].delay = OTP_AUX_DEFAULT_DELAY;
                profile.tempProt[i].level = OTP_AUX_DEFAULT_LEVEL;
                profile.tempProt[i].state = OTP_AUX_DEFAULT_STATE;
            } else {
                profile.tempProt[i].delay = OTP_CH_DEFAULT_DELAY;
                profile.tempProt[i].level = OTP_CH_DEFAULT_LEVEL;
                profile.tempProt[i].state = OTP_CH_DEFAULT_STATE;
            }
        }
    }

    profile.flags.isValid = true;
}

static bool recallState(Parameters &profile, List *lists, int recallOptions, int *err) {
    if (!(recallOptions & RECALL_OPTION_IGNORE_POWER)) {
        if (profile.flags.powerIsUp) {
            if (!powerUp()) {
                if (err) {
                    *err = SCPI_ERROR_EXECUTION_ERROR;
                }
                return false;
            }
        } else {
            powerDown();
        }
    }

    for (int i = 0; i < temp_sensor::NUM_TEMP_SENSORS; ++i) {
        memcpy(&temperature::sensors[i].prot_conf, profile.tempProt + i, sizeof(temperature::ProtectionConfiguration));
    }

    trigger::abort();

    int setCouplingTypeErr;
    if (!channel_dispatcher::setCouplingType((channel_dispatcher::CouplingType)profile.flags.couplingType, &setCouplingTypeErr)) {
        event_queue::pushEvent(setCouplingTypeErr);
    }

    bool mismatch = repositionChannelsInProfileToMatchCurrentChannelConfiguration(profile, lists);

    int numTrackingChannels = 0;

    for (int i = 0; i < CH_NUM; ++i) {
        Channel &channel = Channel::get(i);

        if (profile.channels[i].flags.parameters_are_valid) {
            channel.u.set = MIN(profile.channels[i].u_set, channel.u.max);
            channel.u.step = profile.channels[i].u_step;
            channel.u.limit = MIN(profile.channels[i].u_limit, channel.u.max);

            channel.i.set = MIN(profile.channels[i].i_set, channel.i.max);
            channel.i.step = profile.channels[i].i_step;

            channel.p_limit = MIN(profile.channels[i].p_limit, channel.u.max * channel.i.max);

            channel.prot_conf.u_delay = profile.channels[i].u_delay;
            channel.prot_conf.u_level = profile.channels[i].u_level;
            channel.prot_conf.i_delay = profile.channels[i].i_delay;
            channel.prot_conf.p_delay = profile.channels[i].p_delay;
            channel.prot_conf.p_level = profile.channels[i].p_level;

            channel.prot_conf.flags.u_state = profile.channels[i].flags.u_state;
            channel.prot_conf.flags.u_type = profile.channels[i].flags.u_type;
            channel.prot_conf.flags.i_state = profile.channels[i].flags.i_state;
            channel.prot_conf.flags.p_state = profile.channels[i].flags.p_state;

            channel.setCurrentLimit(profile.channels[i].i_limit);

#ifdef EEZ_PLATFORM_SIMULATOR
            channel.simulator.load_enabled = profile.channels[i].load_enabled;
            channel.simulator.load = profile.channels[i].load;
            channel.simulator.voltProgExt = profile.channels[i].voltProgExt;
#endif

            channel.flags.outputEnabled = channel.isTripped() || mismatch || (recallOptions & RECALL_OPTION_FORCE_DISABLE_OUTPUT) ? 0 : profile.channels[i].flags.output_enabled;
            channel.flags.senseEnabled = profile.channels[i].flags.sense_enabled;

            if (channel.params.features & CH_FEATURE_RPROG) {
                channel.flags.rprogEnabled = profile.channels[i].flags.rprog_enabled;
            } else {
                channel.flags.rprogEnabled = 0;
            }

            channel.flags.displayValue1 = profile.channels[i].flags.displayValue1;
            channel.flags.displayValue2 = profile.channels[i].flags.displayValue2;
            channel.ytViewRate = profile.channels[i].ytViewRate;
            if (channel.flags.displayValue1 == 0 && channel.flags.displayValue2 == 0) {
                channel.flags.displayValue1 = DISPLAY_VALUE_VOLTAGE;
                channel.flags.displayValue2 = DISPLAY_VALUE_CURRENT;
            }
            if (channel.ytViewRate == 0) {
                channel.ytViewRate = GUI_YT_VIEW_RATE_DEFAULT;
            }

            channel.flags.voltageTriggerMode = (TriggerMode)profile.channels[i].flags.u_triggerMode;
            channel.flags.currentTriggerMode = (TriggerMode)profile.channels[i].flags.i_triggerMode;
            channel.flags.triggerOutputState = profile.channels[i].flags.triggerOutputState;
            channel.flags.triggerOnListStop = profile.channels[i].flags.triggerOnListStop;
            trigger::setVoltage(channel, profile.channels[i].u_triggerValue);
            trigger::setCurrent(channel, profile.channels[i].i_triggerValue);
            list::setListCount(channel, profile.channels[i].listCount);

            channel.flags.currentRangeSelectionMode = profile.channels[i].flags.currentRangeSelectionMode;
            channel.flags.autoSelectCurrentRange = profile.channels[i].flags.autoSelectCurrentRange;

            channel.flags.dprogState = profile.channels[i].flags.dprogState;

            channel.flags.trackingEnabled = profile.channels[i].flags.trackingEnabled;
            if (channel.flags.trackingEnabled) {
                ++numTrackingChannels;
            }

            channel.u.rampState = profile.channels[i].u_rampState;
            channel.u.rampDuration = profile.channels[i].u_rampDuration;
            channel.i.rampState = profile.channels[i].i_rampState;
            channel.i.rampDuration = profile.channels[i].i_rampDuration;

            channel.flags.outputDelayState = profile.channels[i].flags.outputDelayState;
            channel.outputDelayDuration = profile.channels[i].outputDelayDuration;

            auto &list = lists[i];
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

    return true;
}

////////////////////////////////////////////////////////////////////////////////

class WriteContext {
public:
    WriteContext(File &file_);

    bool group(const char *groupName);
    bool group(const char *groupNamePrefix, unsigned int index);

    bool property(const char *propertyName, int value);
    bool property(const char *propertyName, unsigned int value);
    bool property(const char *propertyName, float value);
    bool property(const char *propertyName, const char *str);
    bool property(
        const char *propertyName,
        float *dwellList, uint16_t &dwellListLength,
        float *voltageList, uint16_t &voltageListLength,
        float *currentList, uint16_t &currentListLength
    );

    bool flush();

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
    return file.write((uint8_t *)line, strlen(line));
}

bool WriteContext::group(const char *groupNamePrefix, unsigned int index) {
    char line[256 + 1];
    sprintf(line, "[%s%d]\n", groupNamePrefix, index);
    return file.write((uint8_t *)line, strlen(line));
}

bool WriteContext::property(const char *propertyName, int value) {
    char line[256 + 1];
    sprintf(line, "\t%s=%d\n", propertyName, (int)value);
    return file.write((uint8_t *)line, strlen(line));
}

bool WriteContext::property(const char *propertyName, unsigned int value) {
    char line[256 + 1];
    sprintf(line, "\t%s=%u\n", propertyName, (int)value);
    return file.write((uint8_t *)line, strlen(line));
}

bool WriteContext::property(const char *propertyName, float value) {
    char line[256 + 1];
    sprintf(line, "\t%s=%g\n", propertyName, value);
    return file.write((uint8_t *)line, strlen(line));
}

bool WriteContext::property(const char *propertyName, const char *str) {
    char line[256 + 1];
    sprintf(line, "\t%s=\"", propertyName);
    if (!file.write((uint8_t *)line, strlen(line))) {
        return false;
    }

    for (; *str; str++) {
        if (*str == '"') {
            if (!file.write((uint8_t *)"\\\"", 2)) {
                return false;
            }
        } else if (*str == '\\') {
            if (!file.write((uint8_t *)"\\\\", 2)) {
                return false;
            }
        } else {
            if (!file.write((uint8_t *)str, 1)) {
                return false;
            }
        }
    }

    if (!file.write((uint8_t *)"\"\n", 2)) {
        return false;
    }

    return true;
}

bool WriteContext::property(
    const char *propertyName,
    float *dwellList, uint16_t &dwellListLength,
    float *voltageList, uint16_t &voltageListLength,
    float *currentList, uint16_t &currentListLength
) {
    char line[256 + 1];
    sprintf(line, "\t%s=```\n", propertyName);
    if (!file.write((uint8_t *)line, strlen(line))) {
        return false;
    }

    int err;
    if (!list::saveList(file, dwellList, dwellListLength, voltageList, voltageListLength, currentList, currentListLength, false, &err)) {
        return false;
    }

    sprintf(line, "```\n");
    if (!file.write((uint8_t *)line, strlen(line))) {
        return false;
    }

    return true;
}

bool WriteContext::flush() {
    return file.flush();
}

////////////////////////////////////////////////////////////////////////////////

#define WRITE_GROUP(p) if (!ctx.group(name)) return false
#define WRITE_PROPERTY(p1, p2) if (!ctx.property(p1, p2)) return false
#define WRITE_LIST_PROPERTY(p1, p2, p3, p4, p5, p6, p7) if (!ctx.property(p1, p2, p3, p4, p5, p6, p7)) return false

static bool profileWrite(WriteContext &ctx, const Parameters &parameters, List *lists, bool showProgress) {
#if OPTION_DISPLAY
    size_t processedSoFar = 0;
    static const int CH_PROGRESS_WEIGHT = 20;
    static const int TEMP_SENSOR_PROGRESS_WEIGHT = 5;
    size_t totalSize = 2 + CH_MAX * CH_PROGRESS_WEIGHT + temp_sensor::NUM_TEMP_SENSORS * TEMP_SENSOR_PROGRESS_WEIGHT;
    if (showProgress) {
        psu::gui::updateProgressPage(processedSoFar, totalSize);
    }
#endif

    ctx.group("system");
    WRITE_PROPERTY("powerIsUp", parameters.flags.powerIsUp);
    WRITE_PROPERTY("profileName", parameters.name);

#if OPTION_DISPLAY
    if (showProgress) {
        processedSoFar++;
        psu::gui::updateProgressPage(1, totalSize);
    }
#endif

    ctx.group("dcpsupply");
    WRITE_PROPERTY("couplingType", parameters.flags.couplingType);

#if OPTION_DISPLAY
    if (showProgress) {
        processedSoFar++;
        psu::gui::updateProgressPage(2, totalSize);
    }
#endif

    for (int channelIndex = 0; channelIndex < CH_MAX; channelIndex++) {
        auto &channel = parameters.channels[channelIndex];
        if (channel.flags.parameters_are_valid) {
            ctx.group("dcpsupply.ch", channelIndex + 1);

            WRITE_PROPERTY("moduleType", channel.moduleType);
            WRITE_PROPERTY("moduleRevision", channel.moduleRevision);

            WRITE_PROPERTY("output_enabled", channel.flags.output_enabled);
            WRITE_PROPERTY("sense_enabled", channel.flags.sense_enabled);
            WRITE_PROPERTY("u_state", channel.flags.u_state);
            WRITE_PROPERTY("i_state", channel.flags.i_state);
            WRITE_PROPERTY("p_state", channel.flags.p_state);
            WRITE_PROPERTY("rprog_enabled", channel.flags.rprog_enabled);
            WRITE_PROPERTY("displayValue1", channel.flags.displayValue1);
            WRITE_PROPERTY("displayValue2", channel.flags.displayValue2);
            WRITE_PROPERTY("u_triggerMode", channel.flags.u_triggerMode);
            WRITE_PROPERTY("i_triggerMode", channel.flags.i_triggerMode);
            WRITE_PROPERTY("currentRangeSelectionMode", channel.flags.currentRangeSelectionMode);
            WRITE_PROPERTY("autoSelectCurrentRange", channel.flags.autoSelectCurrentRange);
            WRITE_PROPERTY("triggerOutputState", channel.flags.triggerOutputState);
            WRITE_PROPERTY("triggerOnListStop", channel.flags.triggerOnListStop);
            WRITE_PROPERTY("u_type", channel.flags.u_type);
            WRITE_PROPERTY("dprogState", channel.flags.dprogState);
            WRITE_PROPERTY("trackingEnabled", channel.flags.trackingEnabled);

            WRITE_PROPERTY("u_set", channel.u_set);
            WRITE_PROPERTY("u_step", channel.u_step);
            WRITE_PROPERTY("u_limit", channel.u_limit);
            WRITE_PROPERTY("u_delay", channel.u_delay);
            WRITE_PROPERTY("u_level", channel.u_level);
            WRITE_PROPERTY("i_set", channel.i_set);
            WRITE_PROPERTY("i_step", channel.i_step);
            WRITE_PROPERTY("i_limit", channel.i_limit);
            WRITE_PROPERTY("i_delay", channel.i_delay);
            WRITE_PROPERTY("p_limit", channel.p_limit);
            WRITE_PROPERTY("p_delay", channel.p_delay);
            WRITE_PROPERTY("p_level", channel.p_level);
            WRITE_PROPERTY("ytViewRate", channel.ytViewRate);
            WRITE_PROPERTY("u_triggerValue", channel.u_triggerValue);
            WRITE_PROPERTY("i_triggerValue", channel.i_triggerValue);
            WRITE_PROPERTY("listCount", channel.listCount);

            WRITE_PROPERTY("u_rampState", channel.u_rampState);
            WRITE_PROPERTY("u_rampDuration", channel.u_rampDuration);
            WRITE_PROPERTY("i_rampState", channel.i_rampState);
            WRITE_PROPERTY("i_rampDuration", channel.i_rampDuration);

            WRITE_PROPERTY("outputDelayState", channel.flags.outputDelayState);
            WRITE_PROPERTY("outputDelayDuration", channel.outputDelayDuration);

            if (lists) {
                WRITE_LIST_PROPERTY(
                    "list",
                    lists[channelIndex].dwellList,
                    lists[channelIndex].dwellListLength,
                    lists[channelIndex].voltageList,
                    lists[channelIndex].voltageListLength,
                    lists[channelIndex].currentList,
                    lists[channelIndex].currentListLength
                );

            } else {
                auto &channel = Channel::get(channelIndex);

                uint16_t dwellListLength;
                float *dwellList = list::getDwellList(channel, &dwellListLength);

                uint16_t voltageListLength;
                float *voltageList = list::getVoltageList(channel, &voltageListLength);

                uint16_t currentListLength;
                float *currentList = list::getCurrentList(channel, &currentListLength);

                WRITE_LIST_PROPERTY(
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
            WRITE_PROPERTY("load_enabled", channel.load_enabled);
            WRITE_PROPERTY("load", channel.load);
            WRITE_PROPERTY("voltProgExt", channel.voltProgExt);
#endif
        }

#if OPTION_DISPLAY
        if (showProgress) {
            processedSoFar += CH_PROGRESS_WEIGHT;
            psu::gui::updateProgressPage(processedSoFar, totalSize);
        }
#endif
    }

    for (int i = 0; i < temp_sensor::NUM_TEMP_SENSORS; ++i) {
        auto &sensor = temperature::sensors[i];
        if (sensor.isInstalled()) {
            auto &tempSensorProt = parameters.tempProt[i];

            ctx.group("tempsensor", i + 1);

            WRITE_PROPERTY("name", sensor.getName());
            WRITE_PROPERTY("delay", tempSensorProt.delay);
            WRITE_PROPERTY("level", tempSensorProt.level);
            WRITE_PROPERTY("state", tempSensorProt.state);
        }

#if OPTION_DISPLAY
        if (showProgress) {
            processedSoFar += TEMP_SENSOR_PROGRESS_WEIGHT;
            psu::gui::updateProgressPage(processedSoFar, totalSize);
        }
#endif
    }

    return true;
}

static bool saveProfileToFile(const char *filePath, Parameters &profile, List *lists, bool showProgress, int *err) {
    if (!sd_card::isMounted(err)) {
        if (err) {
            *err = SCPI_ERROR_MISSING_MASS_MEDIA;
        }
        return false;
    }

    if (!sd_card::makeParentDir(filePath, err)) {
        if (err) {
            *err = SCPI_ERROR_MASS_STORAGE_ERROR;
        }
        return false;
    }

    uint32_t timeout = millis() + CONF_PROFILE_SAVE_TIMEOUT_MS;
    while (millis() < timeout) {
        File file;

        if (file.open(filePath, FILE_CREATE_ALWAYS | FILE_WRITE)) {
            WriteContext ctx(file);

            if (profileWrite(ctx, profile, lists, showProgress)) {
                if (ctx.flush()) {
                    file.close();
                    onSdCardFileChangeHook(filePath);
                    return true;
                }
            }
        }

        sd_card::reinitialize();
    }

    if (err) {
        *err = SCPI_ERROR_MASS_STORAGE_ERROR;
    }
    return false;
}

static void saveStateToProfile0(bool merge) {
    char filePath[MAX_PATH_LENGTH];
    getProfileFilePath(0, filePath);

    if (!merge) {
        memset(&g_profilesCache[0], 0, sizeof(Parameters));
        memset(g_listsProfile0, 0, CH_MAX * sizeof(List));
    }

    saveState(g_profilesCache[0], g_listsProfile0);

    int err;
    if (!saveProfileToFile(filePath, g_profilesCache[0], g_listsProfile0, false, &err)) {
        generateError(err);
        return;
    }
}

////////////////////////////////////////////////////////////////////////////////

class ReadContext {
public:
    ReadContext(File &file_);

    bool doRead(bool (*callback)(ReadContext &ctx, Parameters &parameters, List *lists), Parameters &parameters, List *lists, int options, bool showProgress);

    bool matchGroup(const char *groupName);
    bool matchGroup(const char *groupNamePrefix, int &index);

    bool property(const char *name, unsigned int &value);
    bool property(const char *name, uint16_t &value);
    bool property(const char *name, bool &value);

    bool property(const char *name, float &value);
    bool property(const char *name, char *str, unsigned int strLength);

    bool listProperty(const char *name, int channelIndex, List *lists);

    void skipPropertyValue();

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

bool ReadContext::doRead(bool (*callback)(ReadContext &ctx, Parameters &parameters, List *lists), Parameters &parameters, List *lists, int options, bool showProgress) {
#if OPTION_DISPLAY
    size_t totalSize = file.size();
#endif

    while (true) {
#if OPTION_DISPLAY
        if (showProgress) {
            psu::gui::updateProgressPage(file.tell(), totalSize);
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

            if (callback(*this, parameters, lists)) {
                if (!result) {
                    return false;
                }
            } else {
                skipPropertyValue();
            }

            if (options & LOAD_PROFILE_FROM_FILE_OPTION_ONLY_NAME) {
                if (strcmp(propertyName, "profileName") == 0) {
                    break;
                }
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

bool ReadContext::listProperty(const char *name, int channelIndex, List *lists) {
    if (strcmp(propertyName, name) != 0) {
        return false;
    }

    if (!sd_card::match(file, "```")) {
        result = false;
    }

    auto &list = lists[channelIndex];
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

void ReadContext::skipPropertyValue() {
    if (sd_card::match(file, "```")) {
        sd_card::skipUntil(file, "```");
    } else {
        sd_card::skipUntilEOL(file);
    }
}

#define READ_FLAG(name, value) \
    auto name = value; \
    if (ctx.property(#name, name)) { \
        value = name; \
        return true; \
    }

#define READ_PROPERTY(name, value) \
    if (ctx.property(#name, value)) { \
        return true; \
    }

#define READ_STRING_PROPERTY(name, str, strLength) \
    if (ctx.property(#name, str, strLength)) { \
        return true; \
    }

#define READ_LIST_PROPERTY(name, channelIndex, lists) \
    if (ctx.listProperty(#name, channelIndex, lists)) { \
        return true; \
    }

#define SKIP_PROPERTY(name) \
    if (ctx.skipProperty(#name)) { \
        return true; \
    }

////////////////////////////////////////////////////////////////////////////////

static bool profileReadCallback(ReadContext &ctx, Parameters &parameters, List *lists) {
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

        READ_PROPERTY(u_rampState, channel.u_rampState);
        READ_PROPERTY(u_rampDuration, channel.u_rampDuration);
        READ_PROPERTY(i_rampState, channel.i_rampState);
        READ_PROPERTY(i_rampDuration, channel.i_rampDuration);

        READ_FLAG(outputDelayState, channel.flags.outputDelayState);
        READ_PROPERTY(outputDelayDuration, channel.outputDelayDuration);

        if (lists) {
            READ_LIST_PROPERTY(list, channelIndex, lists);
        }

#ifdef EEZ_PLATFORM_SIMULATOR
        READ_PROPERTY(load_enabled, channel.load_enabled);
        READ_PROPERTY(load, channel.load);
        READ_PROPERTY(voltProgExt, channel.voltProgExt);
#endif
    }

    int tempSensorIndex;
    if (ctx.matchGroup("tempsensor", tempSensorIndex)) {
        --tempSensorIndex;

        auto &tempSensorProt = parameters.tempProt[tempSensorIndex];

        READ_PROPERTY(delay, tempSensorProt.delay);
        READ_PROPERTY(level, tempSensorProt.level);
        READ_PROPERTY(state, tempSensorProt.state);
    }

    return false;
}

static bool profileRead(ReadContext &ctx, Parameters &parameters, List *lists, int options, bool showProgress) {
    return ctx.doRead(profileReadCallback, parameters, lists, options, showProgress);
}

static bool loadProfileFromFile(const char *filePath, Parameters &profile, List *lists, int options, bool showProgress, int *err) {
    if (!sd_card::isMounted(err)) {
        if (err) {
            *err = SCPI_ERROR_MISSING_MASS_MEDIA;
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

    ReadContext ctx(file);

    bool result = profileRead(ctx, profile, lists, options, showProgress);

    file.close();

    if (!result) {
        if (err) {
            *err = SCPI_ERROR_MASS_STORAGE_ERROR;
        }
        return false;
    }

    return true;
}

////////////////////////////////////////////////////////////////////////////////

static bool doSaveToLastLocation(int *err) {
    memset(&g_profilesCache[NUM_PROFILE_LOCATIONS - 1], 0, sizeof(Parameters));
    saveState(g_profilesCache[NUM_PROFILE_LOCATIONS - 1], g_listsProfile10);
    return true;
}

static bool doRecallFromLastLocation(int *err) {
    return recallState(g_profilesCache[NUM_PROFILE_LOCATIONS - 1], g_listsProfile10, 0, err);
}

////////////////////////////////////////////////////////////////////////////////

static bool isTickSaveAllowed() {
    return !list::isActive() && !calibration::isEnabled();
}

static bool isAutoSaveAllowed() {
    return !g_freeze && persist_conf::devConf.profileAutoRecallEnabled && persist_conf::devConf.profileAutoRecallLocation == 0;
}

static bool isProfile0Dirty() {
    Parameters profile;
    memcpy(&profile, &g_profilesCache[0], sizeof(profile));
    saveState(profile, nullptr);

    if (memcmp(&profile, &g_profilesCache[0], sizeof(profile)) != 0) {
        return true;
    }

    for (int channelIndex = 0; channelIndex < CH_NUM; channelIndex++) {
        auto &channel = Channel::get(channelIndex);

        auto &list = g_listsProfile0[channelIndex];

        uint16_t dwellListLength;
        float *dwellList = list::getDwellList(channel, &dwellListLength);
        if (dwellListLength != list.dwellListLength || memcmp(dwellList, list.dwellList, sizeof(list.dwellList)) != 0) {
            return true;
        }

        uint16_t voltageListLength;
        float *voltageList = list::getVoltageList(channel, &voltageListLength);
        if (voltageListLength != list.voltageListLength || memcmp(voltageList, list.voltageList, sizeof(list.voltageList)) != 0) {
            return true;
        }

        uint16_t currentListLength;
        float *currentList = list::getCurrentList(channel, &currentListLength);
        if (currentListLength != list.currentListLength || memcmp(currentList, list.currentList, sizeof(list.currentList)) != 0) {
            return true;
        }
    }

    return false;
}

} // namespace profile
} // namespace psu
} // namespace eez
