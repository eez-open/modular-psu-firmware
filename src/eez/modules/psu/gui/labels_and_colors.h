/*
 * EEZ Modular Firmware
 * Copyright (C) 2020-present, Envox d.o.o.
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

#include <eez/gui/gui.h>
using namespace eez::gui;

namespace eez {
namespace psu {
namespace gui {

class LabelsAndColorsPage : public SetPage {
public:
    static int g_colorIndex;

    static Value getSlotLabel(int slotIndex);
    static Value getSlotLabelOrDefault(int slotIndex);
    static void setSlotLabel(int slotIndex, const char *label);
    static bool isSlotLabelModified(int slotIndex);
    
    static uint8_t getSlotColor(int slotIndex);
    static void setSlotColor(int slotIndex, uint8_t color);
    static bool isSlotColorModified(int slotIndex);

    static Value getChannelLabel(int slotIndex, int subchannelIndex);
    static Value getChannelLabelOrDefault(int slotIndex, int subchannelIndex);
    static void setChannelLabel(int slotIndex, int subchannelIndex, const char *label);
    static bool isChannelLabelModified(int slotIndex, int subchannelIndex);
    
    static uint8_t getChannelColor(int slotIndex, int subchannelIndex);
    static void setChannelColor(int slotIndex, int subchannelIndex, uint8_t color);
    static bool isChannelColorModified(int slotIndex, int subchannelIndex);

    void pageAlloc();

    int getDirty();
    void set();
};

} // namespace gui
} // namespace psu
} // namespace eez
