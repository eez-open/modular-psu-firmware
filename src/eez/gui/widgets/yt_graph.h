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

#include <eez/dlog_file.h>

namespace eez {
namespace gui {

static const int MAX_NUM_OF_Y_VALUES = dlog_file::MAX_NUM_OF_Y_AXES;

enum {
	YT_GRAPH_UPDATE_METHOD_SCROLL,
	YT_GRAPH_UPDATE_METHOD_SCAN_LINE,
	YT_GRAPH_UPDATE_METHOD_STATIC
};

} // gui
} // eez
