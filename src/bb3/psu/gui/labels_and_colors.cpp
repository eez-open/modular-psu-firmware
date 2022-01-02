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

#include <bb3/hmi.h>
#include <eez/core/memory.h>
#include <bb3/psu/psu.h>
#include <bb3/psu/gui/psu.h>
#include <bb3/psu/gui/labels_and_colors.h>

namespace eez {
namespace psu {
namespace gui {

struct ChannelLabelAndColor {
    int subchannelIndex;
    char *label;
    uint8_t color;
    ChannelLabelAndColor *next;
};

struct SlotLabelAndColor {
    char label[SLOT_LABEL_MAX_LENGTH + 1];
    uint8_t color;
    ChannelLabelAndColor* first;
};

SlotLabelAndColor *g_slotLabelAndColors = (SlotLabelAndColor *)FILE_VIEW_BUFFER;

SlotLabelAndColor *getSlotLabelAndColor(int slotIndex) {
    return g_slotLabelAndColors + slotIndex;
}

ChannelLabelAndColor *getChannelLabelAndColor(int slotIndex, int subchannelIndex) {
    SlotLabelAndColor *slotLabelAndColors = getSlotLabelAndColor(slotIndex);
    for (ChannelLabelAndColor *p = slotLabelAndColors->first; p; p = p->next) {
        if (p->subchannelIndex == subchannelIndex) {
            return p;
        }
    }
    return nullptr;
}

////////////////////////////////////////////////////////////////////////////////

int LabelsAndColorsPage::g_colorIndex;
uint16_t LabelsAndColorsPage::g_colorDataId;
int LabelsAndColorsPage::g_editSlotIndex;
int LabelsAndColorsPage::g_editSubchannelIndex;

const char *LabelsAndColorsPage::getSlotLabel(int slotIndex) {
    return getSlotLabelAndColor(slotIndex)->label;
}

const char *LabelsAndColorsPage::getSlotLabelOrDefault(int slotIndex) {
    const char *label = getSlotLabelAndColor(slotIndex)->label;
    return *label ? label : g_slots[slotIndex]->getDefaultLabel();
}

void LabelsAndColorsPage::setSlotLabel(int slotIndex, const char *label) {
    stringCopy(getSlotLabelAndColor(slotIndex)->label, SLOT_LABEL_MAX_LENGTH + 1, label);
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

bool LabelsAndColorsPage::isSlotColorModified(int slotIndex) {
    return getSlotLabelAndColor(slotIndex)->color != 0;
}

void LabelsAndColorsPage::editChannelLabel(int slotIndex, int subchannelIndex) {
    g_editSlotIndex = slotIndex;
    g_editSubchannelIndex = subchannelIndex;
    startTextKeypad(0, 
        LabelsAndColorsPage::getChannelLabelOrDefault(slotIndex, subchannelIndex), 
        1, g_slots[slotIndex]->getChannelLabelMaxLength(subchannelIndex),
        false, 
        onSetChannelLabel, popPage, 
        LabelsAndColorsPage::isChannelLabelModified(slotIndex, subchannelIndex) ? onSetChannelDefaultLabel : nullptr);
}

void LabelsAndColorsPage::onSetChannelLabel(char *value) {
    LabelsAndColorsPage::setChannelLabel(g_editSlotIndex, g_editSubchannelIndex, value);
    popPage();
}

void LabelsAndColorsPage::onSetChannelDefaultLabel() {
    LabelsAndColorsPage::setChannelLabel(g_editSlotIndex, g_editSubchannelIndex, "");
    popPage();
}

const char *LabelsAndColorsPage::getChannelLabel(int slotIndex, int subchannelIndex) {
    return getChannelLabelAndColor(slotIndex, subchannelIndex)->label;
}

const char *LabelsAndColorsPage::getChannelLabelOrDefault(int slotIndex, int subchannelIndex) {
    const char *label = getChannelLabelAndColor(slotIndex, subchannelIndex)->label;
    return *label ? label : g_slots[slotIndex]->getDefaultChannelLabel(subchannelIndex);
}

void LabelsAndColorsPage::setChannelLabel(int slotIndex, int subchannelIndex, const char *label) {
    auto labelMaxLength = g_slots[slotIndex]->getChannelLabelMaxLength(subchannelIndex);    
    memcpy(getChannelLabelAndColor(slotIndex, subchannelIndex)->label, label, labelMaxLength + 1);
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

bool LabelsAndColorsPage::isChannelColorModified(int slotIndex, int subchannelIndex) {
    return getChannelLabelAndColor(slotIndex, subchannelIndex)->color != 0;
}

void LabelsAndColorsPage::pageAlloc() {
    ChannelLabelAndColor *channelLabelAndColors = (ChannelLabelAndColor *)(FILE_VIEW_BUFFER + NUM_SLOTS * sizeof(SlotLabelAndColor));

    for (int slotIndex = 0; slotIndex < NUM_SLOTS; slotIndex++) {
        stringCopy(g_slotLabelAndColors[slotIndex].label, SLOT_LABEL_MAX_LENGTH + 1, g_slots[slotIndex]->getLabel());
        g_slotLabelAndColors[slotIndex].color = g_slots[slotIndex]->getColor();
        g_slotLabelAndColors[slotIndex].first = channelLabelAndColors;

        auto numChannels = g_slots[slotIndex]->getNumSubchannels();
        if (numChannels > 0) {
            g_slotLabelAndColors[slotIndex].first = channelLabelAndColors;

            for (int relativeChannelIndex = 0; relativeChannelIndex < numChannels; relativeChannelIndex++) {
                int subchannelIndex = g_slots[slotIndex]->getSubchannelIndexFromRelativeChannelIndex(relativeChannelIndex);
                channelLabelAndColors->subchannelIndex = subchannelIndex;
                
                auto labelMaxLength = g_slots[slotIndex]->getChannelLabelMaxLength(subchannelIndex);

                channelLabelAndColors->label = (char *)(channelLabelAndColors + 1);
                memcpy(channelLabelAndColors->label, g_slots[slotIndex]->getChannelLabel(subchannelIndex), labelMaxLength + 1);
                channelLabelAndColors->color = g_slots[slotIndex]->getChannelColor(subchannelIndex);

                ChannelLabelAndColor *next = (ChannelLabelAndColor *)((uint8_t *)(channelLabelAndColors + 1) + (((labelMaxLength + 1) + 3) / 4) * 4);

                if (relativeChannelIndex < numChannels - 1) {
                    channelLabelAndColors->next = next;
                } else {
                    channelLabelAndColors->next = 0;
                }

                channelLabelAndColors = next;
            }
        } else {
            g_slotLabelAndColors[slotIndex].first = 0;
        }
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

        for (ChannelLabelAndColor *p = g_slotLabelAndColors[slotIndex].first; p; p = p->next) {
            ChannelLabelAndColor &channelLabelAndColors = *p;

            auto labelMaxLength = g_slots[slotIndex]->getChannelLabelMaxLength(channelLabelAndColors.subchannelIndex);

            if (memcmp(channelLabelAndColors.label, g_slots[slotIndex]->getChannelLabel(channelLabelAndColors.subchannelIndex), labelMaxLength) != 0) {
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

        for (ChannelLabelAndColor *p = g_slotLabelAndColors[slotIndex].first; p; p = p->next) {
            ChannelLabelAndColor &channelLabelAndColors = *p;

            auto labelMaxLength = g_slots[slotIndex]->getChannelLabelMaxLength(channelLabelAndColors.subchannelIndex);

            if (memcmp(channelLabelAndColors.label, g_slots[slotIndex]->getChannelLabel(channelLabelAndColors.subchannelIndex), labelMaxLength) != 0) {
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

void data_slot_labels_and_colors_view(int slotIndex, DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = g_slots[slotIndex]->getLabelsAndColorsPageId();
    }
}

void data_slot1_labels_and_colors_view(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    data_slot_labels_and_colors_view(0, operation, widgetCursor, value);
}

void data_slot2_labels_and_colors_view(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    data_slot_labels_and_colors_view(1, operation, widgetCursor, value);
}

void data_slot3_labels_and_colors_view(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    data_slot_labels_and_colors_view(2, operation, widgetCursor, value);
}

void data_colors(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    auto cursor = widgetCursor.cursor;
    if (operation == DATA_OPERATION_COUNT) {
        value = 24;
    } else if (operation == DATA_OPERATION_SELECT) {
        value = LabelsAndColorsPage::g_colorIndex;
        LabelsAndColorsPage::g_colorIndex = cursor;
    } else if (operation == DATA_OPERATION_DESELECT) {
        LabelsAndColorsPage::g_colorIndex = value.getInt();
    } 
}

void data_color_is_selected(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    auto cursor = widgetCursor.cursor;
    if (operation == DATA_OPERATION_GET) {
        value = (eez::gui::transformColorHook(LabelsAndColorsPage::g_colorDataId) - COLOR_ID_CHANNEL1) == cursor;
    } 
}

void data_labels_and_colors_page_slot_title(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = LabelsAndColorsPage::getSlotLabelOrDefault(hmi::g_selectedSlotIndex);
    }
}

void data_labels_and_colors_page_channel_title(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    auto cursor = widgetCursor.cursor;
    if (operation == DATA_OPERATION_GET) {
        auto &channel = psu::Channel::get(cursor);
        value = LabelsAndColorsPage::getChannelLabelOrDefault(channel.slotIndex, channel.subchannelIndex);
    }
}

void data_labels_and_colors_is_color_modified(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        if (g_channel) {
            value = LabelsAndColorsPage::isChannelColorModified(g_channel->slotIndex, g_channel->subchannelIndex);
        } else {
            value = LabelsAndColorsPage::isSlotColorModified(hmi::g_selectedSlotIndex);
        }
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
    startTextKeypad(0,
        LabelsAndColorsPage::getSlotLabelOrDefault(hmi::g_selectedSlotIndex), 1, SLOT_LABEL_MAX_LENGTH, false,
        onSetSlotLabel, popPage,
        LabelsAndColorsPage::isSlotLabelModified(hmi::g_selectedSlotIndex) ? onSetSlotDefaultLabel : nullptr);
}

void action_change_channel_label() {
    selectChannelByCursor();
    LabelsAndColorsPage::editChannelLabel(g_channel->slotIndex, g_channel->subchannelIndex);
}

void action_show_channel_color_picker() {
    selectChannelByCursor();
	LabelsAndColorsPage::g_colorDataId = COLOR_ID_LABELS_AND_COLORS_PAGE_CHANNEL_COLOR;
	pushPage(PAGE_ID_COLOR_PICKER);
}

void action_show_slot_color_picker() {
    hmi::selectSlot(getFoundWidgetAtDown().cursor);
	LabelsAndColorsPage::g_colorDataId = COLOR_ID_LABELS_AND_COLORS_PAGE_SLOT_COLOR;
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
