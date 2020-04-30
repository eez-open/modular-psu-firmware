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

#include <eez/index.h>

#if OPTION_ETHERNET
#include <eez/modules/mcu/ethernet.h>
#endif

#include <eez/modules/dcp405/channel.h>
#include <eez/modules/dcm220/channel.h>

#include <eez/modules/psu/psu.h>

namespace eez {

////////////////////////////////////////////////////////////////////////////////

ChannelInterface::ChannelInterface(int slotIndex_) 
    : slotIndex(slotIndex_) 
{
}

unsigned ChannelInterface::getRPol(int subchannelIndex) {
    return 0;
}

void ChannelInterface::setRemoteSense(int subchannelIndex, bool enable) {
}

void ChannelInterface::setRemoteProgramming(int subchannelIndex, bool enable) {
}

void ChannelInterface::setCurrentRange(int subchannelIndex) {
}

bool ChannelInterface::isVoltageBalanced(int subchannelIndex) {
	return false;
}

bool ChannelInterface::isCurrentBalanced(int subchannelIndex) {
	return false;
}

float ChannelInterface::getUSetUnbalanced(int subchannelIndex) {
    psu::Channel &channel = psu::Channel::getBySlotIndex(slotIndex, subchannelIndex);
    return channel.u.set;
}

float ChannelInterface::getISetUnbalanced(int subchannelIndex) {
    psu::Channel &channel = psu::Channel::getBySlotIndex(slotIndex, subchannelIndex);
    return channel.i.set;
}

void ChannelInterface::readAllRegisters(int subchannelIndex, uint8_t ioexpRegisters[], uint8_t adcRegisters[]) {
}

void ChannelInterface::onSpiIrq() {
}

#if defined(DEBUG) && defined(EEZ_PLATFORM_STM32)
int ChannelInterface::getIoExpBitDirection(int subchannelIndex, int io_bit) {
	return 0;
}

bool ChannelInterface::testIoExpBit(int subchannelIndex, int io_bit) {
	return false;
}

void ChannelInterface::changeIoExpBit(int subchannelIndex, int io_bit, bool set) {
}
#endif

////////////////////////////////////////////////////////////////////////////////

static ModuleInfo g_modules[] = {
    { 
        MODULE_TYPE_NONE,
        "None",
        0,
        1,
        nullptr
    },
    {
        MODULE_TYPE_DCP405,
        "DCP405",
        MODULE_REVISION_DCP405_R2B7,
        1,
        dcp405::g_channelInterfaces
    },
    {
        MODULE_TYPE_DCM220, 
        "DCM220",
        MODULE_REVISION_DCM220_R2B4,
        2,
        dcm220::g_channelInterfaces
    },
    {
        MODULE_TYPE_DCM224, 
        "DCM224",
        MODULE_REVISION_DCM224_R1B1,
        2,
        dcm220::g_channelInterfaces
    }
};

ModuleInfo *getModuleInfo(uint16_t moduleType) {
    for (size_t i = 0; i < sizeof(g_modules) / sizeof(ModuleInfo); i++) {
        if (g_modules[i].moduleType == moduleType) {
            return &g_modules[i];
        }
    }
    return &g_modules[0];
}

SlotInfo g_slots[NUM_SLOTS + 1] = {
    {
        &g_modules[0]
    },
    {
        &g_modules[0]
    },
    {
        &g_modules[0]
    },
    // invalid slot
    {
        &g_modules[0]
    }
};

} // namespace eez
