/*
* EEZ Middleware
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

#if defined(EEZ_PLATFORM_STM32)

#include <bb3/mcu/button.h>

#include <gpio.h>

#include <bb3/system.h>

#define CONF_DEBOUNCE_THRESHOLD_TIME 10 // 10 ms
#define CONF_GUI_LONG_TOUCH_TIMEOUT 1000 // 1 sec

namespace eez {
namespace mcu {

Button::Button(GPIO_TypeDef* port, uint16_t pin, bool activeLow, bool detectLongPress) 
    : m_port(port)
    , m_pin(pin)
    , m_activePinState(activeLow ? GPIO_PIN_RESET : GPIO_PIN_SET)
    , m_buttonPinState(activeLow ? GPIO_PIN_RESET : GPIO_PIN_SET)
    , m_detectLongPress(detectLongPress)
    , m_buttonPinStateChangedTick(0)
    , m_btnIsDown(false)
	, m_longPressDetected(false)
{
}

bool Button::isClicked() {
    if (HAL_GPIO_ReadPin(m_port, m_pin) == m_activePinState) {
        // button is DOWN
        if (!m_btnIsDown) {
            if (m_buttonPinState != m_activePinState) {
                m_buttonPinState = m_activePinState;
                m_buttonPinStateChangedTick = millis();
            } else {
                int32_t diff = millis() - m_buttonPinStateChangedTick;
                if (diff > CONF_DEBOUNCE_THRESHOLD_TIME) {
                	m_buttonPinStateChangedTick = millis();
                    m_btnIsDown = true;
                    m_longPressDetected = false;
                    return m_detectLongPress ? false : true;
                }
            }
        }
    } else {
        // button is UP
        if (m_btnIsDown) {
            if (m_buttonPinState == m_activePinState) {
                m_buttonPinState = m_activePinState == GPIO_PIN_RESET ? GPIO_PIN_SET : GPIO_PIN_RESET;
                m_buttonPinStateChangedTick = millis();
            } else {
                int32_t diff = millis() - m_buttonPinStateChangedTick;
                if (diff > CONF_DEBOUNCE_THRESHOLD_TIME) {
                    m_btnIsDown = false;
                    return m_detectLongPress && !m_longPressDetected ? true : false;
                }
            }
        }
    }
    return false;
}

bool Button::isLongPress() {
	if (m_btnIsDown && !m_longPressDetected) {
		int32_t diff = millis() - m_buttonPinStateChangedTick;
		if (diff > CONF_GUI_LONG_TOUCH_TIMEOUT) {
			m_longPressDetected = true;
			return true;
		}
	}
    return false;
}

bool Button::isPressed() {
    return m_btnIsDown;
}

} // namespace mcu
} // namespace eez

#endif // EEZ_PLATFORM_STM32