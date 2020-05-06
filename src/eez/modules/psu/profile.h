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

#include <eez/modules/psu/temperature.h>
#include <eez/modules/psu/io_pins.h>

#define PROFILE_EXT ".profile"

namespace eez {
namespace psu {
/// PSU configuration profiles (save, recall, ...).
namespace profile {

/// Channel binary flags stored in profile.
struct ChannelFlags {
    unsigned output_enabled : 1;
    unsigned sense_enabled : 1;
    unsigned u_state : 1;
    unsigned i_state : 1;
    unsigned p_state : 1;
    unsigned rprog_enabled : 1;
    unsigned parameters_are_valid : 1;
    unsigned displayValue1 : 2;
    unsigned displayValue2 : 2;
    unsigned u_triggerMode : 2;
    unsigned i_triggerMode : 2;
    unsigned currentRangeSelectionMode: 2;
    unsigned autoSelectCurrentRange: 1;
    unsigned triggerOutputState: 1;
    unsigned triggerOnListStop: 3;
    unsigned u_type : 1;
    unsigned dprogState : 2;
    unsigned trackingEnabled : 1;
};

/// Channel parameters stored in profile.
struct ChannelParameters {
    uint16_t moduleType;
    uint16_t moduleRevision;
    ChannelFlags flags;
    float u_set;
    float u_step;
	float u_limit;
    float u_delay;
    float u_level;
    float i_set;
    float i_step;
	float i_limit;
    float i_delay;
	float p_limit;
    float p_delay;
    float p_level;
    float ytViewRate;
    float u_triggerValue;
    float i_triggerValue;
    uint16_t listCount;
    float u_rampDuration;
    float i_rampDuration;
    float outputDelayDuration;
#ifdef EEZ_PLATFORM_SIMULATOR
    bool load_enabled;
    float load;
	float voltProgExt;
#endif
};

/// Channel binary flags stored in profile.
struct ProfileFlags {
    unsigned isValid: 1;
    unsigned powerIsUp: 1;
    unsigned couplingType : 3;
    unsigned triggerContinuousInitializationEnabled: 1;
    unsigned reserverd : 10;
};

enum LoadStatus {
    LOAD_STATUS_ONLY_NAME,
    LOAD_STATUS_LOADING,
    LOAD_STATUS_LOADED
};

/// Profile parameters.
struct Parameters {
    LoadStatus loadStatus;
    ProfileFlags flags;
    char name[PROFILE_NAME_MAX_LENGTH + 1];
    ChannelParameters channels[CH_MAX];
    temperature::ProtectionConfiguration tempProt[temp_sensor::MAX_NUM_TEMP_SENSORS];
    uint16_t triggerSource;
    float triggerDelay;
    io_pins::IOPin ioPins[4];
};

void init();
void tick();

void onAfterSdCardMounted();

void shutdownSave();

Parameters *getProfileParameters(int location);

enum {
    RECALL_OPTION_FORCE_DISABLE_OUTPUT = 0x01,
    RECALL_OPTION_IGNORE_POWER = 0x02
};

bool recallFromLocation(int location);
bool recallFromLocation(int location, int recallOptions, bool showProgress, int *err);
bool recallFromFile(const char *filePath, int recallOptions, bool showProgress, int *err);

bool saveToLocation(int location);
bool saveToLocation(int location, const char *name, bool showProgress, int *err);
bool saveToFile(const char *filePath, bool showProgress, int *err);

bool importFileToLocation(const char *filePath, int location, bool showProgress, int *err);
bool exportLocationToFile(int location, const char *filePath, bool showProgress, int *err);

bool deleteLocation(int location, bool showProgress, int *err);
bool deleteAllLocations(int *err);

bool isLoaded(int location);
bool isValid(int location);

void getSaveName(int location, char *name);

bool setName(int location, const char *name, bool showProgress, int *err);
void getName(int location, char *name, int count);

bool getFreezeState();
void setFreezeState(bool value);

void loadProfileParametersToCache(int location);

}
}
} // namespace eez::psu::profile
