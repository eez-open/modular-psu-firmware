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

#include <eez/gui/document.h>

namespace eez {
namespace gui {

void decompressAssets();

extern bool g_assetsLoaded;

const Style *getStyle(int styleID);
const Widget *getPageWidget(int pageId);
const uint8_t *getFontData(int fontID);
const Bitmap *getBitmap(int bitmapID);
int getThemesCount();
const char *getThemeName(int i);
const uint16_t *getThemeColors(int themeIndex);
const uint32_t getThemeColorsCount(int themeIndex);
const uint16_t *getColors();

const char *getActionName(int16_t actionId);
int16_t getDataIdFromName(const char *name);

int getExternalAssetsFirstPageId();

#define GET_WIDGET_PROPERTY(widget, propertyName, type) ((type)widget->propertyName)
#define GET_WIDGET_LIST_ELEMENT(list, index) ((list).first + (index))

bool loadExternalAssets(const char *filePath, int *err);

////////////////////////////////////////////////////////////////////////////////

} // namespace gui
} // namespace eez
