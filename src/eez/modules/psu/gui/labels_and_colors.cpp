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

#if OPTION_DISPLAY

#include <string.h>

#include <eez/hmi.h>
#include <eez/memory.h>
#include <eez/modules/psu/psu.h>
#include <eez/modules/psu/gui/psu.h>
#include <eez/modules/psu/gui/labels_and_colors.h>

namespace eez {
namespace psu {
namespace gui {

struct ChannelLabelAndColor {
    int subchannelIndex;
    char label[SLOT_LABEL_MAX_CHARS + 1];
    uint8_t color;
};

struct SlotLabelAndColor {
    char label[SLOT_LABEL_MAX_CHARS + 1];
    uint8_t color;
    int numChannels;
    ChannelLabelAndColor* channelLabelAndColors;
};

SlotLabelAndColor *g_slotLabelAndColors = (SlotLabelAndColor *)FILE_VIEW_BUFFER;

SlotLabelAndColor *getSlotLabelAndColor(int slotIndex) {
    return g_slotLabelAndColors + slotIndex;
}

ChannelLabelAndColor *getChannelLabelAndColor(int slotIndex, int subchannelIndex) {
    SlotLabelAndColor *slotLabelAndColors = getSlotLabelAndColor(slotIndex);
    for (int i = 0; i < slotLabelAndColors->numChannels; i++) {
        if (slotLabelAndColors->channelLabelAndColors[i].subchannelIndex == subchannelIndex) {
            return slotLabelAndColors->channelLabelAndColors + i;
        }
    }
    return nullptr;
}

////////////////////////////////////////////////////////////////////////////////

int LabelsAndColorsPage::g_colorIndex;

Value LabelsAndColorsPage::getSlotLabel(int slotIndex) {
    return getSlotLabelAndColor(slotIndex)->label;
}

Value LabelsAndColorsPage::getSlotLabelOrDefault(int slotIndex) {
    const char *label = getSlotLabelAndColor(slotIndex)->label;
    return *label ? label : g_slots[slotIndex]->getDefaultLabel();
}

void LabelsAndColorsPage::setSlotLabel(int slotIndex, const char *label) {
    strcpy(getSlotLabelAndColor(slotIndex)->label, label);
}

bool LabelsAndColorsPage::isSlotLabelModified(int slotIndex) {
    return *getSlotLabelAndColor(slotIndex)->label;
}

uint8_t LabelsAndColorsPage::getSlotColor(int slotIndex) {
    return getSlotLabelAndColor(slotIndex)->color;
}

void LabelsAndColorsPage::setSlotColor(int slotIndex, uint8_t color) {
    getSlotLabelAndColor(slotIndex)->color = color;
}

Value LabelsAndColorsPage::getChannelLabel(int slotIndex, int subchannelIndex) {
    return getChannelLabelAndColor(slotIndex, subchannelIndex)->label;
}

Value LabelsAndColorsPage::getChannelLabelOrDefault(int slotIndex, int subchannelIndex) {
    const char *label = getChannelLabelAndColor(slotIndex, subchannelIndex)->label;
    return *label ? label : g_slots[slotIndex]->getDefaultChannelLabel(subchannelIndex);
}

void LabelsAndColorsPage::setChannelLabel(int slotIndex, int subchannelIndex, const char *label) {
    strcpy(getChannelLabelAndColor(slotIndex, subchannelIndex)->label, label);
}

bool LabelsAndColorsPage::isChannelLabelModified(int slotIndex, int subchannelIndex) {
    return *getChannelLabelAndColor(slotIndex, subchannelIndex)->label;
}

uint8_t LabelsAndColorsPage::getChannelColor(int slotIndex, int subchannelIndex) {
    return getChannelLabelAndColor(slotIndex, subchannelIndex)->color;
}

void LabelsAndColorsPage::setChannelColor(int slotIndex, int subchannelIndex, uint8_t color) {
    getChannelLabelAndColor(slotIndex, subchannelIndex)->color = color;
}

void LabelsAndColorsPage::pageAlloc() {
    ChannelLabelAndColor *channelLabelAndColors = (ChannelLabelAndColor *)(FILE_VIEW_BUFFER + NUM_SLOTS * sizeof(SlotLabelAndColor));

    for (int slotIndex = 0; slotIndex < NUM_SLOTS; slotIndex++) {
        strcpy(g_slotLabelAndColors[slotIndex].label, g_slots[slotIndex]->getLabel());
        g_slotLabelAndColors[slotIndex].color = g_slots[slotIndex]->getColor();
        g_slotLabelAndColors[slotIndex].channelLabelAndColors = channelLabelAndColors;
        g_slotLabelAndColors[slotIndex].numChannels = g_slots[slotIndex]->getNumSubchannels();

        for (int relativeChannelIndex = 0; relativeChannelIndex < g_slotLabelAndColors[slotIndex].numChannels; relativeChannelIndex++) {
            int subchannelIndex = g_slots[slotIndex]->getSubchannelIndexFromRelativeChannelIndex(relativeChannelIndex);
            channelLabelAndColors[relativeChannelIndex].subchannelIndex = subchannelIndex;
            strcpy(channelLabelAndColors[relativeChannelIndex].label, g_slots[slotIndex]->getChannelLabel(subchannelIndex));
            channelLabelAndColors[relativeChannelIndex].color = g_slots[slotIndex]->getChannelColor(subchannelIndex);
        }

        channelLabelAndColors += g_slotLabelAndColors[slotIndex].numChannels;
    }
}

int LabelsAndColorsPage::getDirty() {
    for (int slotIndex = 0; slotIndex < NUM_SLOTS; slotIndex++) {
        if (strcmp(g_slotLabelAndColors[slotIndex].label, g_slots[slotIndex]->getLabel()) != 0) {
            return 1;
        }

        if (g_slotLabelAndColors[slotIndex].color != g_slots[slotIndex]->getColor()) {
            return 1;
        }

        for (int relativeChannelIndex = 0; relativeChannelIndex < g_slotLabelAndColors[slotIndex].numChannels; relativeChannelIndex++) {
            ChannelLabelAndColor &channelLabelAndColors = g_slotLabelAndColors[slotIndex].channelLabelAndColors[relativeChannelIndex];

            if (strcmp(channelLabelAndColors.label, g_slots[slotIndex]->getChannelLabel(channelLabelAndColors.subchannelIndex)) != 0) {
                return 1;
            }

            if (channelLabelAndColors.color != g_slots[slotIndex]->getChannelColor(channelLabelAndColors.subchannelIndex)) {
                return 1;
            }
         }
    }

    return 0;
}

void LabelsAndColorsPage::set() {
    for (int slotIndex = 0; slotIndex < NUM_SLOTS; slotIndex++) {
        if (strcmp(g_slotLabelAndColors[slotIndex].label, g_slots[slotIndex]->getLabel()) != 0) {
            g_slots[slotIndex]->setLabel(g_slotLabelAndColors[slotIndex].label);
        }
        
        if (g_slotLabelAndColors[slotIndex].color != g_slots[slotIndex]->getColor()) {
            g_slots[slotIndex]->setColor(g_slotLabelAndColors[slotIndex].color);
        }

        for (int relativeChannelIndex = 0; relativeChannelIndex < g_slotLabelAndColors[slotIndex].numChannels; relativeChannelIndex++) {
            ChannelLabelAndColor &channelLabelAndColors = g_slotLabelAndColors[slotIndex].channelLabelAndColors[relativeChannelIndex];
            
            if (strcmp(channelLabelAndColors.label, g_slots[slotIndex]->getChannelLabel(channelLabelAndColors.subchannelIndex)) != 0) {
                g_slots[slotIndex]->setChannelLabel(channelLabelAndColors.subchannelIndex, channelLabelAndColors.label);
            }
            
            if (channelLabelAndColors.color != g_slots[slotIndex]->getChannelColor(channelLabelAndColors.subchannelIndex)) {
                g_slots[slotIndex]->setChannelColor(channelLabelAndColors.subchannelIndex, channelLabelAndColors.color);
            }
        }
    }

    popPage();
}

} // namespace gui
} // namespace psu

namespace gui {

using namespace eez::psu::gui;

void data_slot_labels_and_colors_view(int slotIndex, DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = g_slots[slotIndex]->getLabelsAndColorsPageId();
    }
}

void data_slot1_labels_and_colors_view(DataOperationEnum operation, Cursor cursor, Value &value) {
    data_slot_labels_and_colors_view(0, operation, cursor, value);
}

void data_slot2_labels_and_colors_view(DataOperationEnum operation, Cursor cursor, Value &value) {
    data_slot_labels_and_colors_view(1, operation, cursor, value);
}

void data_slot3_labels_and_colors_view(DataOperationEnum operation, Cursor cursor, Value &value) {
    data_slot_labels_and_colors_view(2, operation, cursor, value);
}

void data_colors(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_COUNT) {
        value = 24;
    } else if (operation == DATA_OPERATION_SELECT) {
        value = LabelsAndColorsPage::g_colorIndex;
        LabelsAndColorsPage::g_colorIndex = cursor;
    } else if (operation == DATA_OPERATION_DESELECT) {
        LabelsAndColorsPage::g_colorIndex = value.getInt();
    } 
}

void data_labels_and_colors_page_slot_title(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = LabelsAndColorsPage::getSlotLabelOrDefault(hmi::g_selectedSlotIndex);
    }
}

void data_labels_and_colors_page_channel_title(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        auto &channel = psu::Channel::get(cursor);
        value = LabelsAndColorsPage::getChannelLabelOrDefault(channel.slotIndex, channel.subchannelIndex);
    }
}

void action_show_labels_and_colors() {
    pushPage(PAGE_ID_SYS_SETTINGS_LABELS_AND_COLORS);
}

void onSetSlotLabel(char *value) {
    LabelsAndColorsPage::setSlotLabel(hmi::g_selectedSlotIndex, value);
    popPage();
}

void onSetSlotDefaultLabel() {
    LabelsAndColorsPage::setSlotLabel(hmi::g_selectedSlotIndex, "");
    popPage();
}

void action_change_slot_label() {
    hmi::selectSlot(getFoundWidgetAtDown().cursor);
    Keypad::startPush(0,
        LabelsAndColorsPage::getSlotLabelOrDefault(hmi::g_selectedSlotIndex).getString(), 1, SLOT_LABEL_MAX_CHARS, false,
        onSetSlotLabel, popPage,
        LabelsAndColorsPage::isSlotLabelModified(hmi::g_selectedSlotIndex) ? onSetSlotDefaultLabel : nullptr);
}

void onSetChannelLabel(char *value) {
    LabelsAndColorsPage::setChannelLabel(g_channel->slotIndex, g_channel->subchannelIndex, value);
    popPage();
}

void onSetChannelDefaultLabel() {
    LabelsAndColorsPage::setChannelLabel(g_channel->slotIndex, g_channel->subchannelIndex, "");
    popPage();
}

void action_change_channel_label() {
    selectChannelByCursor();
    Keypad::startPush(0, 
        LabelsAndColorsPage::getChannelLabelOrDefault(g_channel->slotIndex, g_channel->subchannelIndex).getString(), 1, CHANNEL_LABEL_MAX_CHARS, false, 
        onSetChannelLabel, popPage, 
        LabelsAndColorsPage::isChannelLabelModified(g_channel->slotIndex, g_channel->subchannelIndex) ? onSetChannelDefaultLabel : nullptr);
}

void action_show_channel_color_picker() {
    selectChannelByCursor();
    pushPage(PAGE_ID_COLOR_PICKER);
}

void action_show_slot_color_picker() {
    hmi::selectSlot(getFoundWidgetAtDown().cursor);
    pushPage(PAGE_ID_COLOR_PICKER);
}

void action_pick_color() {
    if (g_channel) {
        LabelsAndColorsPage::setChannelColor(g_channel->slotIndex, g_channel->subchannelIndex, 1 + getFoundWidgetAtDown().cursor);
    } else {
        LabelsAndColorsPage::setSlotColor(hmi::g_selectedSlotIndex, 1 + getFoundWidgetAtDown().cursor);
    }
    popPage();
}

void action_pick_default_color() {
    if (g_channel) {
        LabelsAndColorsPage::setChannelColor(g_channel->slotIndex, g_channel->subchannelIndex, 0);
    } else {
        LabelsAndColorsPage::setSlotColor(hmi::g_selectedSlotIndex, 0);
    }
    popPage();
}

} // namespace gui
} // namespace eez

#endif
