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
    unsigned reserverd : 11;
};

/// Profile parameters.
struct Parameters {
    persist_conf::BlockHeader header;
    ProfileFlags flags;
    char name[PROFILE_NAME_MAX_LENGTH + 1];
    ChannelParameters channels[CH_MAX];
    temperature::ProtectionConfiguration temp_prot[temp_sensor::MAX_NUM_TEMP_SENSORS];
};

static const uint16_t PROFILE_VERSION = 11;

// auto save support
extern bool g_profileDirty;
bool enableSave(bool enable);
void save(bool immediately = false);
void saveIfDirty();

void init();
void tick();

bool checkProfileModuleMatch(Parameters &profile);

void recallChannelsFromProfile(Parameters &profile, int location);
bool recallFromProfile(Parameters &profile, int location);
bool recall(int location, int *err);
bool recallFromFile(const char *filePath, int *err);

Parameters *load(int location);

void getSaveName(int location, char *name);
bool saveAtLocation(int location, const char *name = nullptr);

bool saveToFile(const char *filePath, int *err);

void deleteProfileLists(int location);

bool deleteLocation(int location);
bool deleteAll();

bool isValid(int location);

bool setName(int location, const char *name, size_t nameLength);
void getName(int location, char *name, int count);

bool getFreezeState();
void setFreezeState(bool value);

}
}
} // namespace eez::psu::profile
