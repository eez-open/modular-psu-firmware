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

#include <math.h>

#include <scpi/scpi.h>

#include <eez/system.h>
#include <eez/firmware.h>

#include <eez/modules/psu/psu.h>
#include <eez/modules/psu/channel_dispatcher.h>
#include <eez/modules/psu/list_program.h>
#include <eez/modules/psu/trigger.h>
#include <eez/modules/psu/sd_card.h>
#include <eez/modules/psu/io_pins.h>

#include <eez/modules/psu/gui/psu.h>

#include <eez/libs/sd_fat/sd_fat.h>

#define CONF_COUNTER_THRESHOLD_IN_SECONDS 5
#define CONF_SAVE_LIST_TIMEOUT_MS 2000

namespace eez {

extern char g_listFilePath[CH_MAX][MAX_PATH_LENGTH];

namespace psu {
namespace list {

static struct {
    float dwellList[MAX_LIST_LENGTH];
    uint16_t dwellListLength;

    float voltageList[MAX_LIST_LENGTH];
    uint16_t voltageListLength;

    float currentList[MAX_LIST_LENGTH];
    uint16_t currentListLength;

    uint16_t count;
} g_channelsLists[CH_MAX];

static struct {
    int32_t counter;
    int16_t it;
    uint32_t nextPointTime;
    int32_t currentRemainingDwellTime;
    float currentTotalDwellTime;
    uint32_t lastTickCount;
} g_execution[CH_MAX];

static bool g_active;

////////////////////////////////////////////////////////////////////////////////

void init() {
    reset();
}

void resetChannelList(Channel &channel) {
    int i = channel.channelIndex;

    g_channelsLists[i].voltageListLength = 0;
    g_channelsLists[i].currentListLength = 0;
    g_channelsLists[i].dwellListLength = 0;

    g_channelsLists[i].count = 1;

    g_execution[i].counter = -1;
}

void reset() {
    for (int i = 0; i < CH_NUM; ++i) {
        resetChannelList(Channel::get(i));
    }
}

void setDwellList(Channel &channel, float *list, uint16_t listLength) {
    memcpy(g_channelsLists[channel.channelIndex].dwellList, list, listLength * sizeof(float));
    g_channelsLists[channel.channelIndex].dwellListLength = listLength;
}

float *getDwellList(Channel &channel, uint16_t *listLength) {
    *listLength = g_channelsLists[channel.channelIndex].dwellListLength;
    return g_channelsLists[channel.channelIndex].dwellList;
}

void setVoltageList(Channel &channel, float *list, uint16_t listLength) {
    memcpy(g_channelsLists[channel.channelIndex].voltageList, list, listLength * sizeof(float));
    g_channelsLists[channel.channelIndex].voltageListLength = listLength;
}

float *getVoltageList(Channel &channel, uint16_t *listLength) {
    *listLength = g_channelsLists[channel.channelIndex].voltageListLength;
    return g_channelsLists[channel.channelIndex].voltageList;
}

void setCurrentList(Channel &channel, float *list, uint16_t listLength) {
    memcpy(g_channelsLists[channel.channelIndex].currentList, list, listLength * sizeof(float));
    g_channelsLists[channel.channelIndex].currentListLength = listLength;
}

float *getCurrentList(Channel &channel, uint16_t *listLength) {
    *listLength = g_channelsLists[channel.channelIndex].currentListLength;
    return g_channelsLists[channel.channelIndex].currentList;
}

uint16_t getListCount(Channel &channel) {
    return g_channelsLists[channel.channelIndex].count;
}

void setListCount(Channel &channel, uint16_t value) {
    g_channelsLists[channel.channelIndex].count = value;
}

bool isListEmpty(Channel &channel) {
    return g_channelsLists[channel.channelIndex].dwellListLength == 0 &&
           g_channelsLists[channel.channelIndex].voltageListLength == 0 &&
           g_channelsLists[channel.channelIndex].currentListLength == 0;
}

bool areListLengthsEquivalent(uint16_t size1, uint16_t size2) {
    return size1 != 0 && size2 != 0 && (size1 == 1 || size2 == 1 || size1 == size2);
}

bool areListLengthsEquivalent(uint16_t size1, uint16_t size2, uint16_t size3) {
    return list::areListLengthsEquivalent(size1, size2) &&
           list::areListLengthsEquivalent(size1, size3) &&
           list::areListLengthsEquivalent(size2, size3);
}

bool areListLengthsEquivalent(Channel &channel) {
    return list::areListLengthsEquivalent(g_channelsLists[channel.channelIndex].dwellListLength,
                                          g_channelsLists[channel.channelIndex].voltageListLength,
                                          g_channelsLists[channel.channelIndex].currentListLength);
}

bool areVoltageAndDwellListLengthsEquivalent(Channel &channel) {
    return areListLengthsEquivalent(g_channelsLists[channel.channelIndex].voltageListLength,
                                    g_channelsLists[channel.channelIndex].dwellListLength);
}

bool areCurrentAndDwellListLengthsEquivalent(Channel &channel) {
    return areListLengthsEquivalent(g_channelsLists[channel.channelIndex].currentListLength,
                                    g_channelsLists[channel.channelIndex].dwellListLength);
}

bool areVoltageAndCurrentListLengthsEquivalent(Channel &channel) {
    return areListLengthsEquivalent(g_channelsLists[channel.channelIndex].voltageListLength,
                                    g_channelsLists[channel.channelIndex].currentListLength);
}

int checkLimits(int iChannel) {
    Channel &channel = Channel::get(iChannel);

    uint16_t voltageListLength = g_channelsLists[iChannel].voltageListLength;
    uint16_t currentListLength = g_channelsLists[iChannel].currentListLength;

    for (int j = 0; j < voltageListLength || j < currentListLength; ++j) {
        float voltage = g_channelsLists[iChannel].voltageList[j % voltageListLength];

        if (j < voltageListLength) {
            float roundedValue = channel_dispatcher::roundChannelValue(channel, UNIT_VOLT, voltage);
            float diff = fabsf(roundedValue - voltage);
            if (diff > 5E-6f) {
                return SCPI_ERROR_CANNOT_SET_LIST_VALUE;
            }
            voltage = roundedValue;
        }

        float current = g_channelsLists[iChannel].currentList[j % currentListLength];

        if (j < currentListLength) {
            float roundedValue = channel_dispatcher::roundChannelValue(channel, UNIT_AMPER, current);
            float diff = fabsf(roundedValue - current);
            if (diff > 5E-6f) {
                return SCPI_ERROR_CANNOT_SET_LIST_VALUE;
            }
            current = roundedValue;
        }

        
        if (channel.isVoltageLimitExceeded(voltage)) {
            g_errorChannelIndex = channel.channelIndex;
            return SCPI_ERROR_VOLTAGE_LIMIT_EXCEEDED;
        }

        if (channel.isCurrentLimitExceeded(current)) {
            g_errorChannelIndex = channel.channelIndex;
            return SCPI_ERROR_CURRENT_LIMIT_EXCEEDED;
        }

        int err;
        if (channel.isPowerLimitExceeded(voltage, current, &err)) {
            g_errorChannelIndex = channel.channelIndex;
            return err;
        }
    }

    return 0;
}

bool loadList(
    sd_card::BufferedFileRead &file,
    float *dwellList, uint16_t &dwellListLength,
    float *voltageList, uint16_t &voltageListLength,
    float *currentList, uint16_t &currentListLength,
    bool showProgress,
    int *err
) {
    dwellListLength = 0;
    voltageListLength = 0;
    currentListLength = 0;

    bool success = true;

#if OPTION_DISPLAY
    size_t totalSize = file.size();
#endif

    for (int i = 0; i < MAX_LIST_LENGTH; ++i) {
        sd_card::matchZeroOrMoreSpaces(file);
        if (!file.available() || file.peek() == '`') {
            break;
        }

        float value;

        if (sd_card::match(file, LIST_CSV_FILE_NO_VALUE_CHAR)) {
            if (i < dwellListLength) {
                success = false;
                break;
            }
        } else if (sd_card::match(file, value)) {
            if (i == dwellListLength) {
                dwellList[i] = value;
                dwellListLength = i + 1;
            } else {
                success = false;
                break;
            }
        } else {
            success = false;
            break;
        }

        sd_card::match(file, CSV_SEPARATOR);

        if (sd_card::match(file, LIST_CSV_FILE_NO_VALUE_CHAR)) {
            if (i < voltageListLength) {
                success = false;
                break;
            }
        } else if (sd_card::match(file, value)) {
            if (i == voltageListLength) {
                voltageList[i] = value;
                ++voltageListLength;
            } else {
                success = false;
                break;
            }
        } else {
            success = false;
            break;
        }

        sd_card::match(file, CSV_SEPARATOR);

        if (sd_card::match(file, LIST_CSV_FILE_NO_VALUE_CHAR)) {
            if (i < currentListLength) {
                success = false;
                break;
            }
        } else if (sd_card::match(file, value)) {
            if (i == currentListLength) {
                currentList[i] = value;
                ++currentListLength;
            } else {
                success = false;
                break;
            }
        } else {
            success = false;
            break;
        }

#if OPTION_DISPLAY
        if (showProgress) {
            psu::gui::updateProgressPage(file.tell(), totalSize);
        }
#endif
    }

    return success;
}


bool loadList(
    const char *filePath,
    float *dwellList, uint16_t &dwellListLength,
    float *voltageList, uint16_t &voltageListLength,
    float *currentList, uint16_t &currentListLength,
    bool showProgress,
    int *err
) {
    if (!sd_card::isMounted(filePath, err)) {
        return false;
    }

    if (!sd_card::exists(filePath, err)) {
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

    sd_card::BufferedFileRead bufferedFile(file);

    bool success = loadList(bufferedFile, dwellList, dwellListLength, voltageList, voltageListLength, currentList, currentListLength, showProgress, err);

    file.close();

    if (err) {
        if (success) {
            *err = SCPI_RES_OK;
        } else {
            // TODO replace with more specific error
            *err = SCPI_ERROR_EXECUTION_ERROR;
        }
    }

    return success;
}

bool loadList(int iChannel, const char *filePath, int *err) {
    float dwellList[MAX_LIST_LENGTH];
    uint16_t dwellListLength = 0;

    float voltageList[MAX_LIST_LENGTH];
    uint16_t voltageListLength = 0;

    float currentList[MAX_LIST_LENGTH];
    uint16_t currentListLength = 0;
    
    if (loadList(filePath, dwellList, dwellListLength, voltageList, voltageListLength, currentList, currentListLength, false, err)) {
        Channel &channel = Channel::get(iChannel);
        channel_dispatcher::setDwellList(channel, dwellList, dwellListLength);
        channel_dispatcher::setVoltageList(channel, voltageList, voltageListLength);
        channel_dispatcher::setCurrentList(channel, currentList, currentListLength);
        return true;
    }

    return false;
}

bool saveList(
    sd_card::BufferedFileWrite &file,
    float *dwellList, uint16_t &dwellListLength,
    float *voltageList, uint16_t &voltageListLength,
    float *currentList, uint16_t &currentListLength,
    bool showProgress,
    int *err
) {
#if OPTION_DISPLAY
    uint16_t maxListLength = MAX(MAX(dwellListLength, voltageListLength), currentListLength);
#endif

    for (int i = 0; i < dwellListLength || i < voltageListLength || i < currentListLength; i++) {
        if (i < dwellListLength) {
            if (!file.print(dwellList[i], 4)) {
                return false;
            }
        } else {
            if (!file.print(LIST_CSV_FILE_NO_VALUE_CHAR)) {
                return false;
            }
        }

        if (!file.print(CSV_SEPARATOR)) {
            return false;
        }

        if (i < voltageListLength) {
            if (!file.print(voltageList[i], 4)) {
                return false;
            }
        } else {
            if (!file.print(LIST_CSV_FILE_NO_VALUE_CHAR)) {
                return false;
            }
        }

        if (!file.print(CSV_SEPARATOR)) {
            return false;
        }

        if (i < currentListLength) {
            if (!file.print(currentList[i], 4)) {
                return false;
            }
        } else {
            if (!file.print(LIST_CSV_FILE_NO_VALUE_CHAR)) {
                return false;
            }
        }

        if (!file.print('\n')) {
            return false;
        }

#if OPTION_DISPLAY
        if (showProgress) {
            psu::gui::updateProgressPage(i, maxListLength);
        }
#endif
    }

    return true;
}

bool saveList(
    const char *filePath,
    float *dwellList, uint16_t &dwellListLength,
    float *voltageList, uint16_t &voltageListLength,
    float *currentList, uint16_t &currentListLength,
    bool showProgress,
    int *err
) {
    if (!sd_card::isMounted(filePath, err)) {
        return false;
    }

    if (!sd_card::makeParentDir(filePath, err)) {
        return false;
    }

    uint32_t timeout = millis() + CONF_SAVE_LIST_TIMEOUT_MS;
    while (millis() < timeout) {
        File file;
        if (file.open(filePath, FILE_CREATE_ALWAYS | FILE_WRITE)) {
            sd_card::BufferedFileWrite bufferedFile(file);

            if (saveList(bufferedFile, dwellList, dwellListLength, voltageList, voltageListLength, currentList, currentListLength, showProgress, err)) {
                if (bufferedFile.flush()) {
                    if (file.close()) {
                        onSdCardFileChangeHook(filePath);
                        if (err) {
                            *err = SCPI_RES_OK;
                        }
                        return true;
                    }
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

bool saveList(int iChannel, const char *filePath, int *err) {
    if (!g_shutdownInProgress && !isLowPriorityThread()) {
        stringCopy(&g_listFilePath[iChannel][0], sizeof(g_listFilePath[iChannel]), filePath);
        sendMessageToLowPriorityThread(THREAD_MESSAGE_SAVE_LIST, iChannel);
        return true;
    }

    auto &channelList = g_channelsLists[iChannel];
    return saveList(
        filePath,
        channelList.dwellList, channelList.dwellListLength,
        channelList.voltageList, channelList.voltageListLength,
        channelList.currentList, channelList.currentListLength,
        false,
        err
    );
}

void updateChannelsWithVisibleCountersList();

void setActive(bool active, bool forceUpdate = false) {
    if (g_active != active) {
        g_active = active;
        updateChannelsWithVisibleCountersList();
    } else {
        if (forceUpdate) {
            updateChannelsWithVisibleCountersList();
        }
    }
}

void executionStart(Channel &channel) {
    g_execution[channel.channelIndex].it = -1;
    g_execution[channel.channelIndex].counter = g_channelsLists[channel.channelIndex].count;
    channel_dispatcher::setVoltage(channel, 0);
    channel_dispatcher::setCurrent(channel, 0);
    setActive(true, true);
}

int maxListsSize(Channel &channel) {
    uint16_t maxSize = 0;

    if (g_channelsLists[channel.channelIndex].voltageListLength > maxSize) {
        maxSize = g_channelsLists[channel.channelIndex].voltageListLength;
    }

    if (g_channelsLists[channel.channelIndex].currentListLength > maxSize) {
        maxSize = g_channelsLists[channel.channelIndex].currentListLength;
    }

    if (g_channelsLists[channel.channelIndex].dwellListLength > maxSize) {
        maxSize = g_channelsLists[channel.channelIndex].dwellListLength;
    }

    return maxSize;
}

bool setListValue(Channel &channel, int16_t it, int *err) {
    float voltage = channel_dispatcher::roundChannelValue(channel, UNIT_VOLT, g_channelsLists[channel.channelIndex].voltageList[it % g_channelsLists[channel.channelIndex].voltageListLength]);
    if (channel.isVoltageLimitExceeded(voltage)) {
        g_errorChannelIndex = channel.channelIndex;
        *err = SCPI_ERROR_VOLTAGE_LIMIT_EXCEEDED;
        return false;
    }

    float current = channel_dispatcher::roundChannelValue(channel, UNIT_AMPER, g_channelsLists[channel.channelIndex].currentList[it % g_channelsLists[channel.channelIndex].currentListLength]);
    if (channel.isCurrentLimitExceeded(current)) {
        g_errorChannelIndex = channel.channelIndex;
        *err = SCPI_ERROR_CURRENT_LIMIT_EXCEEDED;
        return false;
    }

    if (channel.isPowerLimitExceeded(voltage, current, err)) {
        g_errorChannelIndex = channel.channelIndex;
        return false;
    }

    if (channel_dispatcher::getUSet(channel) != voltage) {
        channel_dispatcher::setVoltage(channel, voltage);
    }

    if (channel_dispatcher::getISet(channel) != current) {
        channel_dispatcher::setCurrent(channel, current);
    }

    return true;
}

void tick() {
    bool active = false;

    for (int i = 0; i < CH_NUM; ++i) {
        Channel &channel = Channel::get(i);
        if (g_execution[i].counter >= 0) {
            int channelIndex;
            if (channel_dispatcher::isTripped(channel, channelIndex)) {
                setActive(false);
                trigger::abort();
                return;
            }

            active = true;

            uint32_t tickCount;
            bool tickCountInMillis = g_execution[i].currentTotalDwellTime > CONF_COUNTER_THRESHOLD_IN_SECONDS;
            if (tickCountInMillis) {
                tickCount = millis();
            } else {
                tickCount = millis() * 1000;
            }

            if (io_pins::isInhibited()) {
                if (g_execution[i].it != -1) {
                    g_execution[i].nextPointTime += tickCount - g_execution[i].lastTickCount;
                }
            } else {
                bool set = false;

                if (g_execution[i].it == -1) {
                    set = true;
                } else {
                    g_execution[i].currentRemainingDwellTime = g_execution[i].nextPointTime - tickCount;
                    if (g_execution[i].currentRemainingDwellTime <= 0) {
                        set = true;
                    }
                }

                if (set) {
                    if (++g_execution[i].it == maxListsSize(channel)) {
                        if (g_execution[i].counter > 0) {
                            if (--g_execution[i].counter == 0) {
                                g_execution[i].counter = -1;
                                trigger::setTriggerFinished(channel);
                                return;
                            }
                        }

                        g_execution[i].it = 0;
                    }

                    int err;
                    if (!setListValue(channel, g_execution[i].it, &err)) {
                        generateError(err);
                        setActive(false);
                        trigger::abort();
                        return;
                    }

                    g_execution[i].currentTotalDwellTime = g_channelsLists[i] .dwellList[g_execution[i].it % g_channelsLists[i].dwellListLength];
                    // if dwell time is greater then CONF_COUNTER_THRESHOLD_IN_SECONDS ...
                    if (g_execution[i].currentTotalDwellTime > CONF_COUNTER_THRESHOLD_IN_SECONDS) {
                        // ... then count in milliseconds
                        g_execution[i].currentRemainingDwellTime = (uint32_t)round(g_execution[i].currentTotalDwellTime * 1000L);
                        if (!tickCountInMillis) {
                            tickCount /= 1000;
                        }
                    } else {
                        // ... else count in microseconds
                        g_execution[i].currentRemainingDwellTime = (uint32_t)round(g_execution[i].currentTotalDwellTime * 1000000L);
                        if (tickCountInMillis) {
                            tickCount *= 1000;
                        }
                    }
                    g_execution[i].nextPointTime = tickCount + g_execution[i].currentRemainingDwellTime;
                }
            }

            g_execution[i].lastTickCount = tickCount;
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
    return g_execution[channel.channelIndex].counter >= 0;
}

int g_numChannelsWithVisibleCounters;
int g_channelsWithVisibleCounters[CH_MAX];

int getFirstTrackingChannel() {
    for (int i = 0; i < CH_NUM; i++) {
        if (Channel::get(i).flags.trackingEnabled) {
            return i;
        }
    }
    return -1;
}

int32_t getCounter(int channelIndex) {
    if (Channel::get(channelIndex).flags.trackingEnabled) {
        channelIndex = getFirstTrackingChannel();
    }
    return g_execution[channelIndex].counter;
}

void updateChannelsWithVisibleCountersList() {
    g_numChannelsWithVisibleCounters = 0;
    for (int channelIndex = 0; channelIndex < CH_NUM; channelIndex++) {
        if (getCounter(channelIndex) >= 0) {
            auto &channelLists = g_channelsLists[channelIndex];
            for (int j = 0; j < channelLists.dwellListLength; j++) {
                if (channelLists.dwellList[j] >= CONF_LIST_COUNDOWN_DISPLAY_THRESHOLD) {
                    g_channelsWithVisibleCounters[g_numChannelsWithVisibleCounters++] = channelIndex;
                    break;
                }
            }
        }
    }
}

bool getCurrentDwellTime(Channel &channel, int32_t &remaining, uint32_t &total) {
    int i = channel.flags.trackingEnabled ? getFirstTrackingChannel() : channel.channelIndex;
    if (g_execution[i].counter >= 0) {
        total = (uint32_t)ceilf(g_execution[i].currentTotalDwellTime);
        if (g_execution[i].currentTotalDwellTime > CONF_COUNTER_THRESHOLD_IN_SECONDS) {
            remaining = (uint32_t)ceil(g_execution[i].currentRemainingDwellTime / 1000L);
        } else {
            remaining = (uint32_t)ceil(g_execution[i].currentRemainingDwellTime / 1000000L);
        }
        return true;
    }
    return false;
}

void abort() {
    for (int i = 0; i < CH_NUM; ++i) {
        if (g_execution[i].counter >= 0) {
            g_execution[i].counter = -1;
        }
    }
    
    setActive(false, true);
}

} // namespace list
} // namespace psu
} // namespace eez
