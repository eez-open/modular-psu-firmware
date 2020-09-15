/*
* EEZ Generic Firmware
* Copyright (C) 2018-present, Envox d.o.o.
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

#include <stdint.h>

#include <eez/firmware.h>

namespace eez {

namespace psu {
    struct Channel;
    namespace profile {
        class WriteContext;
        class ReadContext;
        struct List;
    }
}

namespace gui {
    class Page;
}

static const uint16_t MODULE_TYPE_NONE = 0;
static const uint16_t MODULE_TYPE_DCP405 = 405;
static const uint16_t MODULE_TYPE_DCM220 = 220;
static const uint16_t MODULE_TYPE_DCM224 = 224;
static const uint16_t MODULE_TYPE_DIB_MIO168 = 168;
static const uint16_t MODULE_TYPE_DIB_PREL6 = 6;
static const uint16_t MODULE_TYPE_DIB_SMX46 = 46;

enum SlotViewType {
    SLOT_VIEW_TYPE_DEFAULT,
    SLOT_VIEW_TYPE_DEFAULT_2COL,
    SLOT_VIEW_TYPE_MAX,
    SLOT_VIEW_TYPE_MIN,
    SLOT_VIEW_TYPE_MICRO,
};

enum FlashMethod {
    FLASH_METHOD_NONE,
    FLASH_METHOD_STM32_BOOTLOADER_UART,
    FLASH_METHOD_STM32_BOOTLOADER_SPI
};

enum SourceMode {
    SOURCE_MODE_CURRENT,
    SOURCE_MODE_VOLTAGE
};

static const int MAX_NUM_CH_IN_CH_LIST = 32;

struct SlotAndSubchannelIndex {
    uint8_t slotIndex;
    uint8_t subchannelIndex;
};

struct ChannelList {
    int numChannels;
    SlotAndSubchannelIndex channels[MAX_NUM_CH_IN_CH_LIST];
};

struct Module {
    uint16_t moduleType;
    const char *moduleName;
    const char *moduleBrand;
    uint16_t latestModuleRevision;
    FlashMethod flashMethod;
    uint32_t flashDuration;
    uint32_t spiBaudRatePrescaler;
    bool spiCrcCalculationEnable;
    uint8_t numPowerChannels;
    uint8_t numOtherChannels;

    uint8_t slotIndex = -1;
    bool enabled;
    uint16_t moduleRevision = 0;
    bool firmwareInstalled = false;
	uint8_t firmwareMajorVersion = 0;
	uint8_t firmwareMinorVersion = 0;
	uint32_t idw0 = 0;
	uint32_t idw1 = 0;
	uint32_t idw2 = 0;

    virtual void setEnabled(bool value);

    virtual Module *createModule() = 0;

    virtual TestResult getTestResult();

    virtual void boot();
    virtual psu::Channel *createPowerChannel(int slotIndex, int channelIndex, int subchannelIndex);
    virtual void initChannels();
    virtual void tick();
    virtual void onPowerDown();
    virtual void onSpiIrq();
    virtual void onSpiDmaTransferCompleted(int status);
    virtual gui::Page *getPageFromId(int pageId);
    virtual void animatePageAppearance(int previousPageId, int activePageId);
    virtual int getSlotView(SlotViewType slotViewType, int slotIndex, int cursor);
    virtual int getChannelSettingsPageId();
    virtual int getSlotSettingsPageId();

    virtual void getProfileParameters(int channelIndex, uint8_t *buffer);
    virtual void setProfileParameters(int channelIndex, uint8_t *buffer, bool mismatch, int recallOptions, int &numTrackingChannels);
    virtual bool writeProfileProperties(psu::profile::WriteContext &ctx, const uint8_t *buffer);
    virtual bool readProfileProperties(psu::profile::ReadContext &ctx, uint8_t *buffer);
    virtual bool getProfileOutputEnable(uint8_t *buffer);
    virtual float getProfileUSet(uint8_t *buffer);
    virtual float getProfileISet(uint8_t *buffer);

    virtual int getNumSubchannels();
    virtual bool isValidSubchannelIndex(int subchannelIndex);
    virtual int getSubchannelIndexFromRelativeChannelIndex(int relativeChannelIndex);

    virtual bool getDigitalInputData(int subchannelIndex, uint8_t &data, int *err);

    virtual bool getDigitalOutputData(int subchannelIndex, uint8_t &data, int *err);
    virtual bool setDigitalOutputData(int subchannelIndex, uint8_t data, int *err);

    virtual bool getMode(int subchannelIndex, SourceMode &mode, int *err);
    virtual bool setMode(int subchannelIndex, SourceMode mode, int *err);

    virtual bool getCurrentRange(int subchannelIndex, int8_t &range, int *err);
    virtual bool setCurrentRange(int subchannelIndex, int8_t range, int *err);

    virtual bool getVoltageRange(int subchannelIndex, int8_t &range, int *err);
    virtual bool setVoltageRange(int subchannelIndex, int8_t range, int *err);

    virtual bool routeOpen(ChannelList channelList, int *err);
    virtual bool routeClose(ChannelList channelList, int *err);
    
    virtual bool getVoltage(int subchannelIndex, float &value, int *err);
    virtual bool setVoltage(int subchannelIndex, float value, int *err);
};

static const int NUM_SLOTS = 3;
extern Module *g_slots[NUM_SLOTS];

Module *getModule(uint16_t moduleType);

void getModuleSerialInfo(uint8_t slotIndex, char *text);

extern bool g_isCol2Mode;
extern int g_slotIndexes[NUM_SLOTS];

} // namespace eez
