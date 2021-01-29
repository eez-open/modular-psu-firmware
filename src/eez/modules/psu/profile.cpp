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
#include <eez/hmi.h>

#include <eez/modules/psu/psu.h>
#include <eez/modules/psu/channel_dispatcher.h>
#include <eez/modules/psu/datetime.h>
#include <eez/modules/psu/event_queue.h>
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
static Parameters g_profilesCache[NUM_PROFILE_LOCATIONS];
static List g_listsProfile0[CH_MAX];
static List g_listsProfile10[CH_MAX];

////////////////////////////////////////////////////////////////////////////////

static void loadProfileName(int location);
static Parameters *getProfileParametersFromCache(int location);

static void getProfileFilePath(int location, char *filePath, size_t filePathStrLength);

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
#if !CONF_SURVIVE_MODE
    auto tick = millis();
    if (tick - g_lastAutoSaveTime > CONF_AUTO_SAVE_TIMEOUT_MS) {
        g_lastAutoSaveTime = tick;
#endif
        if (isTickSaveAllowed() && isAutoSaveAllowed() && isProfile0Dirty() && sd_card::isMounted(nullptr, nullptr)) {
            saveStateToProfile0(true);
        }
#if !CONF_SURVIVE_MODE        
    }
#endif
}

void onAfterSdCardMounted() {
    for (int profileIndex = 1; profileIndex < NUM_PROFILE_LOCATIONS; profileIndex++) {
		loadProfileName(profileIndex);
    }
}

void saveIfDirty() {
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
    getProfileFilePath(location, filePath, sizeof(filePath));

    Parameters profile;
    resetProfileToDefaults(profile);
    if (!loadProfileFromFile(filePath, profile, g_listsProfile0, 0, showProgress, err)) {
        return false;
    }

    if (location != 0) {
        auto recallProfile = &profile;

        loadProfileParametersToCache(0);
        auto defaultProfile = getProfileParameters(0);

        bool forceDisableOutput = false;

        if (recallProfile->flags.couplingType != defaultProfile->flags.couplingType) {
            forceDisableOutput = true;
        } else {
            for (int i = 0; i < CH_NUM; ++i) {
                Channel &channel = Channel::get(i);
                if (
                    !(
                        recallProfile->channels[i].parametersAreValid && defaultProfile->channels[i].parametersAreValid &&
                        recallProfile->channels[i].moduleType == defaultProfile->channels[i].moduleType &&
                        g_slots[channel.slotIndex]->testAutoRecallValuesMatch(
                            (uint8_t *)recallProfile->channels[i].parameters, 
                            (uint8_t *)defaultProfile->channels[i].parameters
                        )
                    )
                ) {
                    forceDisableOutput = true;
                    break;
                }
            }
        }                

        if (forceDisableOutput) {
            recallOptions |= profile::RECALL_OPTION_FORCE_DISABLE_OUTPUT;
            event_queue::pushEvent(event_queue::EVENT_WARNING_AUTO_RECALL_VALUES_MISMATCH);
        }

    }

    if (!recallState(profile, g_listsProfile0, recallOptions, err)) {
        return false;
    }

    if (location == 0) {
        if (!(recallOptions & RECALL_OPTION_IGNORE_POWER)) {
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
    getProfileFilePath(location, filePath, sizeof(filePath));

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
    getProfileFilePath(location, profileFilePath, sizeof(profileFilePath));
    if (sd_card::copyFile(filePath, profileFilePath, true, err)) {
        loadProfileParametersToCache(location);
        return true;
    }
    return false;
}

bool exportLocationToFile(int location, const char *filePath, bool showProgress, int *err) {
    char profileFilePath[MAX_PATH_LENGTH];
    getProfileFilePath(location, profileFilePath, sizeof(profileFilePath));
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
        getProfileFilePath(location, filePath, sizeof(filePath));
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
    Parameters *profile = getProfileParametersFromCache(location);

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
            getProfileFilePath(location, filePath, sizeof(filePath));

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

    if (g_isBooted && !isLowPriorityThread()) {
        if (g_profilesCache[location].loadStatus == LOAD_STATUS_LOADING) {
            return;
        }
       
        g_profilesCache[location].loadStatus = LOAD_STATUS_LOADING;
        
        sendMessageToLowPriorityThread(THREAD_MESSAGE_LOAD_PROFILE, location);
    } else {
        char filePath[MAX_PATH_LENGTH];
        getProfileFilePath(location, filePath, sizeof(filePath));
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
    getProfileFilePath(location, filePath, sizeof(filePath));
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

static void getProfileFilePath(int location, char *filePath, size_t filePathStrLength) {
    snprintf(filePath, filePathStrLength, "%s%s%d%s",
        PROFILES_DIR, PATH_SEPARATOR, location, getExtensionFromFileType(FILE_TYPE_PROFILE));
}

static void resetProfileToDefaults(Parameters &profile) {
    memset(&profile, 0, sizeof(Parameters));

    for (int i = 0; i < CH_NUM; ++i) {
        Channel &channel = Channel::get(i);
        g_slots[channel.slotIndex]->resetPowerChannelProfileToDefaults(channel.subchannelIndex, (uint8_t *)profile.channels[i].parameters);
    }

    for (int i = 0; i < NUM_SLOTS; i++) {
        g_slots[i]->resetProfileToDefaults((uint8_t *)profile.slots[i].parameters);
    }

    for (int i = 0; i < temp_sensor::MAX_NUM_TEMP_SENSORS; ++i) {
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

static bool repositionSlotsInProfileToMatchCurrentSlotsConfiguration(Parameters &profile) {
    bool profileSlotAlreadyUsed[NUM_SLOTS];
    int slotsMap[NUM_SLOTS];
    for (int i = 0; i < NUM_SLOTS; i++) {
        profileSlotAlreadyUsed[i] = false;
        slotsMap[i] = -1;
    }

    for (int i = 0; i < NUM_SLOTS; i++) {
        Module *module = g_slots[i];
        if (
            profile.slots[i].parametersAreValid && 
            profile.slots[i].moduleType == module->moduleType
        ) {
            profileSlotAlreadyUsed[i] = true;
            slotsMap[i] = i;
        } else {
            break;
        }
    }

    for (int i = 0; i < NUM_SLOTS; i++) {
        if (slotsMap[i] == -1) {
            for (int j = 0; j < NUM_SLOTS; j++) {
                if (
                    !profileSlotAlreadyUsed[j] &&
                    profile.slots[j].parametersAreValid &&
                    profile.slots[j].moduleType == g_slots[i]->moduleType
                ) {
                    profileSlotAlreadyUsed[j] = true;
                    slotsMap[i] = j;
                    break;
                }
            }
        }
    }

    for (int i = 0; i < NUM_SLOTS; i++) {
        if (slotsMap[i] == -1) {
            for (int j = 0; j < NUM_SLOTS; j++) {
                if (!profileSlotAlreadyUsed[j] && !profile.slots[j].parametersAreValid) {
                    profileSlotAlreadyUsed[j] = true;
                    slotsMap[i] = j;
                    break;
                }
            }
        }
    }

    for (int i = 0; i < NUM_SLOTS; i++) {
        if (slotsMap[i] == -1) {
            for (int j = 0; j < NUM_SLOTS; j++) {
                if (!profileSlotAlreadyUsed[j]) {
                    profileSlotAlreadyUsed[j] = true;
                    slotsMap[i] = j;
                    break;
                }
            }
        }
    }

    bool mismatch = false;

    for (int i = 0; i < NUM_SLOTS - 1; i++) {
        if (slotsMap[i] != i) {
            for (int j = i + 1; j < NUM_SLOTS; j++) {
                if (slotsMap[j] == i) {
                    mismatch = true;

                    int a = slotsMap[i];
                    int b = slotsMap[j];

                    SlotParameters slotParameters;
                    memcpy(&slotParameters, &profile.slots[a], sizeof(ChannelParameters));
                    memcpy(&profile.slots[a], &profile.slots[b], sizeof(ChannelParameters));
                    memcpy(&profile.slots[b], &slotParameters, sizeof(ChannelParameters));

                    slotsMap[j] = slotsMap[i];
                    slotsMap[i] = i;
                    break;
                }
            }
        }
    }

    return mismatch;
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
            profile.channels[i].parametersAreValid &&
            profile.channels[i].moduleType == g_slots[channel.slotIndex]->moduleType
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
                    profile.channels[j].parametersAreValid &&
                    profile.channels[j].moduleType == g_slots[channel.slotIndex]->moduleType
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
                if (!profileChannelAlreadyUsed[j] && !profile.channels[j].parametersAreValid) {
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

    for (int i = 0; i < NUM_SLOTS; ++i) {
        profile.slots[i].moduleType = g_slots[i]->moduleType;
        profile.slots[i].moduleRevision = g_slots[i]->moduleRevision;
        profile.slots[i].parametersAreValid = 1;
        g_slots[i]->getProfileParameters((uint8_t *)profile.slots[i].parameters);
    }

    for (int i = 0; i < CH_NUM; ++i) {
        Channel &channel = Channel::get(i);

        profile.channels[i].moduleType = g_slots[channel.slotIndex]->moduleType;
        profile.channels[i].moduleRevision = g_slots[channel.slotIndex]->moduleRevision;
        
        profile.channels[i].parametersAreValid = 1;
        
        g_slots[channel.slotIndex]->getPowerChannelProfileParameters(i, (uint8_t *)profile.channels[i].parameters);

        if (lists) {
            auto &list = lists[i];
            memcpy(list.dwellList, list::getDwellList(channel, &list.dwellListLength), sizeof(list.dwellList));
            memcpy(list.voltageList, list::getVoltageList(channel, &list.voltageListLength), sizeof(list.voltageList));
            memcpy(list.currentList, list::getCurrentList(channel, &list.currentListLength), sizeof(list.currentList));
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

    profile.flags.triggerContinuousInitializationEnabled = trigger::g_triggerContinuousInitializationEnabled;
    profile.triggerSource = trigger::g_triggerSource;
    profile.triggerDelay = trigger::g_triggerDelay;

    memcpy(profile.ioPins, io_pins::g_ioPins, sizeof(profile.ioPins));
    memcpy(profile.ioPinsPwmFrequency, io_pins::g_pwmFrequency, sizeof(profile.ioPinsPwmFrequency));
    memcpy(profile.ioPinsPwmDuty, io_pins::g_pwmDuty, sizeof(profile.ioPinsPwmDuty));

    profile.flags.isValid = true;
}

static struct {
    Parameters *profile;
    List *lists;
    int recallOptions;
    int *err;
    bool result;
} g_recallStateParams;

void recallStateFromPsuThread() {
    g_recallStateParams.result = recallState(*g_recallStateParams.profile, g_recallStateParams.lists, g_recallStateParams.recallOptions, g_recallStateParams.err);
}

static bool recallState(Parameters &profile, List *lists, int recallOptions, int *err) {
    if (g_isBooted && !isPsuThread()) {
        g_recallStateParams.profile = &profile;
        g_recallStateParams.lists = lists;
        g_recallStateParams.recallOptions = recallOptions;
        g_recallStateParams.err= err;
        sendMessageToPsu(PSU_MESSAGE_RECALL_STATE);
        return g_recallStateParams.result;
    }

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

    bool mismatch = false;
    mismatch = repositionSlotsInProfileToMatchCurrentSlotsConfiguration(profile);

    for (int i = 0; i < NUM_SLOTS; ++i) {
        if (profile.slots[i].parametersAreValid) {
            g_slots[i]->setProfileParameters((uint8_t *)profile.slots[i].parameters, mismatch, recallOptions);
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

    mismatch = repositionChannelsInProfileToMatchCurrentChannelConfiguration(profile, lists);

    int numTrackingChannels = 0;

    for (int i = 0; i < CH_NUM; ++i) {
        Channel &channel = Channel::get(i);

        if (profile.channels[i].parametersAreValid) {
            g_slots[channel.slotIndex]->setPowerChannelProfileParameters(i, (uint8_t *)profile.channels[i].parameters, mismatch, recallOptions, numTrackingChannels);

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

    trigger::g_triggerContinuousInitializationEnabled = profile.flags.triggerContinuousInitializationEnabled;
    trigger::g_triggerSource = (trigger::Source)profile.triggerSource;
    trigger::g_triggerDelay = profile.triggerDelay;

    Channel::updateAllChannels();

    memcpy(io_pins::g_ioPins, profile.ioPins, sizeof(profile.ioPins));
    memcpy(io_pins::g_pwmFrequency, profile.ioPinsPwmFrequency, sizeof(profile.ioPinsPwmFrequency));
    memcpy(io_pins::g_pwmDuty, profile.ioPinsPwmDuty, sizeof(profile.ioPinsPwmDuty));
    io_pins::refresh();

    return true;
}

////////////////////////////////////////////////////////////////////////////////

WriteContext::WriteContext(File &file_)
    : file(file_)
{
}

bool WriteContext::group(const char *groupName) {
    char line[256 + 1];
    snprintf(line, sizeof(line), "[%s]\n", groupName);
    return file.write((uint8_t *)line, strlen(line));
}

bool WriteContext::group(const char *groupNamePrefix, unsigned int index) {
    char line[256 + 1];
    snprintf(line, sizeof(line), "[%s%d]\n", groupNamePrefix, index);
    return file.write((uint8_t *)line, strlen(line));
}

bool WriteContext::property(const char *propertyName, int value) {
    char line[256 + 1];
    snprintf(line, sizeof(line), "\t%s=%d\n", propertyName, (int)value);
    return file.write((uint8_t *)line, strlen(line));
}

bool WriteContext::property(const char *propertyName, unsigned int value) {
    char line[256 + 1];
    snprintf(line, sizeof(line), "\t%s=%u\n", propertyName, (unsigned int)value);
    return file.write((uint8_t *)line, strlen(line));
}

bool WriteContext::property(const char *propertyName, float value) {
    char line[256 + 1];
    snprintf(line, sizeof(line), "\t%s=%g\n", propertyName, value);
    return file.write((uint8_t *)line, strlen(line));
}

bool WriteContext::property(const char *propertyName, const char *str) {
    char line[256 + 1];
    snprintf(line, sizeof(line), "\t%s=\"", propertyName);
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
    snprintf(line, sizeof(line), "\t%s=```\n", propertyName);
    if (!file.write((uint8_t *)line, strlen(line))) {
        return false;
    }

    int err;
    if (!list::saveList(file, dwellList, dwellListLength, voltageList, voltageListLength, currentList, currentListLength, false, &err)) {
        return false;
    }

    snprintf(line, sizeof(line), "```\n");
    if (!file.write((uint8_t *)line, strlen(line))) {
        return false;
    }

    return true;
}

bool WriteContext::flush() {
    return file.flush();
}

////////////////////////////////////////////////////////////////////////////////

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

    for (int slotIndex = 0; slotIndex < NUM_SLOTS; slotIndex++) {
        auto &slot = parameters.slots[slotIndex];
        if (slot.parametersAreValid) {
            ctx.group("slot", slotIndex + 1);

            WRITE_PROPERTY("moduleType", slot.moduleType);
            WRITE_PROPERTY("moduleRevision", slot.moduleRevision);

            if (!getModule(slot.moduleType)->writeProfileProperties(ctx, (uint8_t *)slot.parameters)) {
                return false;
            }
        }
    }

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
        if (channel.parametersAreValid) {
            ctx.group("ch", channelIndex + 1);

            WRITE_PROPERTY("moduleType", channel.moduleType);
            WRITE_PROPERTY("moduleRevision", channel.moduleRevision);

            if (!getModule(channel.moduleType)->writePowerChannelProfileProperties(ctx, (uint8_t *)channel.parameters)) {
                return false;
            }

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

    ctx.group("trigger");
    WRITE_PROPERTY("continuousInitializationEnabled", parameters.flags.triggerContinuousInitializationEnabled);
    WRITE_PROPERTY("source", parameters.triggerSource);
    WRITE_PROPERTY("delay", parameters.triggerDelay);

    for (int i = 0; i < NUM_IO_PINS; ++i) {
        ctx.group("io_pin", i + 1);
        WRITE_PROPERTY("function", parameters.ioPins[i].function);
        WRITE_PROPERTY("polarity", parameters.ioPins[i].polarity);
        WRITE_PROPERTY("state", parameters.ioPins[i].state);
        if (i >= DOUT1) {
            WRITE_PROPERTY("pwmFrequency", parameters.ioPinsPwmFrequency[i - DOUT1]);
            WRITE_PROPERTY("pwmDuty", parameters.ioPinsPwmDuty[i - DOUT1]);
        }
    }

    return true;
}

static bool saveProfileToFile(const char *filePath, Parameters &profile, List *lists, bool showProgress, int *err) {
    if (!sd_card::isMounted(filePath, err)) {
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
    getProfileFilePath(0, filePath, sizeof(filePath));

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

bool ReadContext::property(const char *name, uint8_t &value) {
    if (strcmp(propertyName, name) != 0) {
        return false;
    }

    unsigned int temp;
    if (sd_card::match(file, temp)) {
        value = (uint8_t)temp;
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

////////////////////////////////////////////////////////////////////////////////

static bool profileReadCallback(ReadContext &ctx, Parameters &parameters, List *lists) {
    if (ctx.matchGroup("system")) {
        READ_FLAG("powerIsUp", parameters.flags.powerIsUp);
        READ_STRING_PROPERTY("profileName", parameters.name, PROFILE_NAME_MAX_LENGTH);
    }

    int slotIndex;
    if (ctx.matchGroup("slot", slotIndex)) {
        --slotIndex;

        auto &slot = parameters.slots[slotIndex];
        
        slot.parametersAreValid = 1;

        READ_PROPERTY("moduleType", slot.moduleType);
        READ_PROPERTY("moduleRevision", slot.moduleRevision);

        if (getModule(slot.moduleType)->readProfileProperties(ctx, (uint8_t *)slot.parameters)) {
            return true;
        }
    }

    if (ctx.matchGroup("dcpsupply")) {
        READ_FLAG("couplingType", parameters.flags.couplingType);
    }

    int channelIndex;
    if (ctx.matchGroup("dcpsupply.ch", channelIndex) || ctx.matchGroup("ch", channelIndex)) {
        --channelIndex;

        auto &channel = parameters.channels[channelIndex];
        
        channel.parametersAreValid = 1;

        READ_PROPERTY("moduleType", channel.moduleType);
        READ_PROPERTY("moduleRevision", channel.moduleRevision);

        if (getModule(channel.moduleType)->readPowerChannelProfileProperties(ctx, (uint8_t *)channel.parameters)) {
            return true;
        }

        if (lists) {
            READ_LIST_PROPERTY("list", channelIndex, lists);
        }
    }

    int tempSensorIndex;
    if (ctx.matchGroup("tempsensor", tempSensorIndex)) {
        --tempSensorIndex;

        auto &tempSensorProt = parameters.tempProt[tempSensorIndex];

        READ_PROPERTY("delay", tempSensorProt.delay);
        READ_PROPERTY("level", tempSensorProt.level);
        READ_PROPERTY("state", tempSensorProt.state);
    }

    if (ctx.matchGroup("trigger")) {
        READ_FLAG("continuousInitializationEnabled", parameters.flags.triggerContinuousInitializationEnabled);
        READ_PROPERTY("source", parameters.triggerSource);
        READ_PROPERTY("delay", parameters.triggerDelay);
    }

    int ioPinIndex;
    if (ctx.matchGroup("io_pin", ioPinIndex)) {
        --ioPinIndex;

        auto &ioPin = parameters.ioPins[ioPinIndex];

        READ_FLAG("function", ioPin.function);
        READ_FLAG("polarity", ioPin.polarity);
        READ_FLAG("state", ioPin.state);
        if (ioPinIndex >= DOUT1) {
            READ_FLAG("pwmFrequency", parameters.ioPinsPwmFrequency[ioPinIndex - DOUT1]);
            READ_FLAG("pwmDuty", parameters.ioPinsPwmDuty[ioPinIndex - DOUT1]);
        }
    }

    return false;
}

static bool profileRead(ReadContext &ctx, Parameters &parameters, List *lists, int options, bool showProgress) {
    return ctx.doRead(profileReadCallback, parameters, lists, options, showProgress);
}

static bool loadProfileFromFile(const char *filePath, Parameters &profile, List *lists, int options, bool showProgress, int *err) {
    if (!sd_card::isMounted(filePath, err)) {
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
    return !trigger::isActive();
}

static bool isAutoSaveAllowed() {
    return !g_freeze && persist_conf::devConf.profileAutoRecallEnabled;
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
