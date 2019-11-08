/*
 * EEZ Modular Firmware
 * Copyright (C) 2015-present, Envox d.o.o.
 *
 * This program is free software: you can redistribute it and/or modify
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <eez/modules/psu/psu.h>

#include <math.h>

#if OPTION_DISPLAY

#include <eez/modules/psu/persist_conf.h>

#include <eez/modules/mcu/display.h>

namespace eez {
namespace gui {
namespace lcd {

uint8_t getDisplayBackgroundLuminosityStep() {
    return eez::psu::persist_conf::devConf.displayBackgroundLuminosityStep;
}

uint8_t getDisplayBrightness() {
    return eez::psu::persist_conf::devConf.displayBrightness;
}

uint8_t getBrightness() {
    return (uint8_t)round(remapQuad(getDisplayBrightness(), 1, 196, 20, 106));
}

} // namespace lcd
} // namespace gui
} // namespace eez

#endif
