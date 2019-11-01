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

#if OPTION_DISPLAY

#include <math.h>

#include <eez/apps/psu/gui/page_recordings_view.h>
#include <eez/gui/document.h>
#include <eez/gui/data.h>

#include <eez/debug.h>

#if OPTION_ENCODER
#include <eez/modules/mcu/encoder.h>
#endif


namespace eez {
namespace psu {
namespace gui {

#if OPTION_ENCODER

void RecordingsViewPage::pageAlloc() {
    mcu::encoder::setUseSameSpeed(true);
}

void RecordingsViewPage::pageFree() {
    mcu::encoder::setUseSameSpeed(false);
}

bool RecordingsViewPage::onEncoder(int counter) {
    int size = data::ytDataGetSize(0, DATA_ID_RECORDING);
    int pageSize = data::ytDataGetPageSize(0, DATA_ID_RECORDING);

    if (size > pageSize) {
        int currentPosition = data::ytDataGetPosition(0, DATA_ID_RECORDING);
        auto currentPositionValue = Value(1.0f *currentPosition, UNIT_SECOND);
        int newPosition = (int)floorf(mcu::encoder::increment(currentPositionValue, counter, 0, 1.0f * (size - pageSize), -1, 1.0f));
        data::ytDataSetPosition(0, DATA_ID_RECORDING, newPosition);
    }

    return true;
}

#endif

} // namespace gui
} // namespace psu
} // namespace eez

#endif