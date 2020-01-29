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

namespace eez {
namespace gui {

#define LIST_TYPE_VERTICAL 1
#define LIST_TYPE_HORIZONTAL 2

struct ListWidget {
    uint8_t listType; // LIST_TYPE_VERTICAL or LIST_TYPE_HORIZONTAL
#if OPTION_SDRAM    
    const Widget *itemWidget;
#else
    uint32_t itemWidgetOffset;
#endif
    uint8_t gap;
};

void ListWidget_fixPointers(Widget *widget);
void ListWidget_enum(WidgetCursor &widgetCursor, EnumWidgetsCallback callback);

} // namespace gui
} // namespace eez