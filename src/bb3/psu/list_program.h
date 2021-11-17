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

#define LIST_EXT ".list"

namespace eez {

// forward declaration
class File;
    
namespace psu {

// forward declaration
namespace sd_card {
class BufferedFileRead;
class BufferedFileWrite;
}

namespace list {

void init();

void resetChannelList(Channel &channel);
void reset();

void setDwellList(Channel &channel, float *list, uint16_t listLength);
float *getDwellList(Channel &channel, uint16_t *listLength);

void setVoltageList(Channel &channel, float *list, uint16_t listLength);
float *getVoltageList(Channel &channel, uint16_t *listLength);

void setCurrentList(Channel &channel, float *list, uint16_t listLength);
float *getCurrentList(Channel &channel, uint16_t *listLength);

uint16_t getListCount(Channel &channel);
void setListCount(Channel &channel, uint16_t value);

bool isListEmpty(Channel &channel);

bool areListLengthsEquivalent(uint16_t size1, uint16_t size2);
bool areListLengthsEquivalent(uint16_t size1, uint16_t size2, uint16_t size3);
bool areListLengthsEquivalent(Channel &channel);
bool areVoltageAndDwellListLengthsEquivalent(Channel &channel);
bool areCurrentAndDwellListLengthsEquivalent(Channel &channel);
bool areVoltageAndCurrentListLengthsEquivalent(Channel &channel);

int checkLimits(int iChannel);

bool loadList(
    sd_card::BufferedFileRead &file,
    float *dwellList, uint16_t &dwellListLength,
    float *voltageList, uint16_t &voltageListLength,
    float *currentList, uint16_t &currentListLength,
    bool showProgress,
    int *err
);
bool loadList(
    const char *filePath,
    float *dwellList, uint16_t &dwellListLength,
    float *voltageList, uint16_t &voltageListLength,
    float *currentList, uint16_t &currentListLength,
    bool showProgress,
    int *err
);
bool loadList(int iChannel, const char *filePath, int *err);

bool saveList(
    sd_card::BufferedFileWrite &file,
    float *dwellList, uint16_t &dwellListLength,
    float *voltageList, uint16_t &voltageListLength,
    float *currentList, uint16_t &currentListLength,
    bool showProgress,
    int *err
);
bool saveList(
    const char *filePath,
    float *dwellList, uint16_t &dwellListLength,
    float *voltageList, uint16_t &voltageListLength,
    float *currentList, uint16_t &currentListLength,
    bool showProgress,
    int *err
);
bool saveList(int iChannel, const char *filePath, int *err);

void executionStart(Channel &channel);

int maxListsSize(Channel &channel);

bool setListValue(Channel &channel, int16_t it, int *err);

void tick();

bool isActive();
bool isActive(Channel &channel);

extern int g_numChannelsWithVisibleCounters;
extern int g_channelsWithVisibleCounters[CH_MAX];
bool getCurrentDwellTime(Channel &channel, int32_t &remaining, uint32_t &total);

void abort();

}
}
} // namespace eez::psu::list