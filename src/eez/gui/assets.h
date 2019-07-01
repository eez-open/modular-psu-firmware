/*
 * EEZ Generic Firmware
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

#include <eez/gui/document.h>

namespace eez {
namespace gui {

extern Document *g_document;
extern Styles *g_styles;
extern uint8_t *g_fontsData;
extern uint8_t *g_bitmapsData;
extern Colors *g_colorsData;

void decompressAssets();

inline OBJ_OFFSET getListItemOffset(const List &list, int index, int listItemSize) {
    return list.first + index * listItemSize;
}

inline OBJ_OFFSET getStyleOffset(int styleID) {
    return getListItemOffset(*g_styles, styleID - 1, sizeof(Style));
}

inline OBJ_OFFSET getPageOffset(int pageID) {
    return getListItemOffset(g_document->pages, pageID, sizeof(Widget));
}

#define DECL_WIDGET_STYLE(var, widget) DECL_STYLE(var, transformStyle(widget))
#define DECL_STYLE(var, styleID)                                                                   \
    const Style *var =                                                                             \
        (styleID) ? (const Style *)((uint8_t *)g_styles + getStyleOffset(styleID)) : nullptr

#define DECL_WIDGET(var, widgetOffset)                                                             \
    const Widget *var = (const Widget *)((uint8_t *)g_document + (widgetOffset))
#define DECL_WIDGET_SPECIFIC(type, var, widget)                                                    \
    const type *var = (const type *)((uint8_t *)g_document + (widget)->specific)
#define DECL_STRING(var, offset) const char *var = (const char *)((uint8_t *)g_document + (offset))
#define DECL_BITMAP(var, offset)                                                                   \
    const Bitmap *var = (const Bitmap *)((uint8_t *)g_document + (offset))
#define DECL_STRUCT_WITH_OFFSET(Struct, var, offset)                                               \
    const Struct *var = (const Struct *)((uint8_t *)g_document + (offset))

////////////////////////////////////////////////////////////////////////////////

} // namespace gui
} // namespace eez
