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

#include <main.h>

namespace eez {
namespace mcu {

class Button {
public:
    Button(GPIO_TypeDef* port, uint16_t pin, bool activeLow, bool detectLongPress);

    bool isClicked();
    bool isLongPress();

private:
    GPIO_TypeDef* m_port;
    uint16_t m_pin;
    GPIO_PinState m_activePinState;
    GPIO_PinState m_buttonPinState;
    bool m_detectLongPress;
    uint32_t m_buttonPinStateChangedTick;
    bool m_btnIsDown;
    bool m_longPressDetected;
};

} // namespace mcu
} // namespace eez
