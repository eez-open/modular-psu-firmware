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

#include <assert.h>

#if defined EEZ_PLATFORM_STM32
#include <main.h>
#include <gpio.h>
#include <tim.h>
#endif

#include <eez/firmware.h>
#include <eez/system.h>

#include <eez/modules/psu/psu.h>

#if OPTION_FAN
#include <eez/modules/aux_ps/fan.h>
#endif

#include <eez/modules/psu/io_pins.h>
#include <eez/modules/psu/persist_conf.h>
#include <eez/modules/psu/trigger.h>
#include <eez/modules/psu/dlog_record.h>

#include <eez/modules/bp3c/flash_slave.h>

namespace eez {
namespace psu {
namespace io_pins {

IOPin g_ioPins[4];

static struct {
    unsigned outputFault : 2;
    unsigned outputEnabled : 2;
    unsigned toutputPulse : 1;
    unsigned inhibited : 1;
} g_lastState = { 2, 2, 0, 0 };

static uint32_t g_toutputPulseStartTickCountMs;

float g_pwmFrequency[NUM_IO_PINS - DOUT1] = { PWM_DEFAULT_FREQUENCY, PWM_DEFAULT_FREQUENCY };
float g_pwmDuty[NUM_IO_PINS - DOUT1] = { PWM_DEFAULT_DUTY, PWM_DEFAULT_DUTY };
static uint32_t g_pwmPeriodInt[NUM_IO_PINS - DOUT1];
static bool g_pwmStarted;

uart::UartMode g_uartMode;
uint32_t g_uartBaudRate = 115200;
uint32_t g_uartDataBits = 8;
uint32_t g_uartStopBits = 1;
uint32_t g_uartParity = 1;

#if defined EEZ_PLATFORM_STM32
static float g_pwmStartedFrequency;
#endif

#if defined EEZ_PLATFORM_STM32

int ioPinRead(int pin) {
    if (pin == EXT_TRIG1) {
#if EEZ_MCU_REVISION_R1B5
        if (bp3c::flash_slave::g_bootloaderMode) {
            return 0;
        }
        return HAL_GPIO_ReadPin(MCU_REV_GPIO(UART_RX_DIN1_GPIO_Port), MCU_REV_GPIO(UART_RX_DIN1_Pin)) ? 1 : 0;
#else
        return HAL_GPIO_ReadPin(DIN1_GPIO_Port, DIN1_Pin) ? 1 : 0;
#endif
    } {
    	assert(pin == EXT_TRIG2);
    	return HAL_GPIO_ReadPin(DIN2_GPIO_Port, DIN2_Pin) ? 1 : 0;
    }
}

void ioPinWrite(int pin, int state) {
    if (pin == DOUT1) {
#if EEZ_MCU_REVISION_R1B5
        if (!bp3c::flash_slave::g_bootloaderMode) {
    	    HAL_GPIO_WritePin(MCU_REV_GPIO(UART_TX_DOUT1_GPIO_Port), MCU_REV_GPIO(UART_TX_DOUT1_Pin), state ? GPIO_PIN_SET : GPIO_PIN_RESET);
        }
#else
    	HAL_GPIO_WritePin(DOUT1_GPIO_Port, DOUT1_Pin, state ? GPIO_PIN_SET : GPIO_PIN_RESET);
#endif
    } else {
        assert(pin == DOUT2);
        HAL_GPIO_WritePin(DOUT2_GPIO_Port, DOUT2_Pin, state ? GPIO_PIN_SET : GPIO_PIN_RESET);
    }
}

#endif

#if defined EEZ_PLATFORM_SIMULATOR

static int g_pins[NUM_IO_PINS];

int ioPinRead(int pin) {
    return g_pins[pin];
}

void ioPinWrite(int pin, int state) {
	g_pins[pin] = state;
}

#endif

uint8_t isOutputFault() {
#if OPTION_FAN
    if (isPowerUp()) {
        if (aux_ps::fan::g_testResult == TEST_FAILED) {
            return 1;
        }
    }
#endif

    for (int i = 0; i < CH_NUM; ++i) {
        Channel &channel = Channel::get(i);
        if (channel.isTripped() || !channel.isTestOk()) {
            return 1;
        }
    }

    return 0;
}

uint8_t isOutputEnabled() {
    for (int i = 0; i < CH_NUM; ++i) {
        if (Channel::get(i).isOutputEnabled() && !isInhibited()) {
            return 1;
        }
    }
    return 0;
}

void initInputPin(int pin) {
#if defined EEZ_PLATFORM_STM32
    if (!bp3c::flash_slave::g_bootloaderMode || pin != DIN1) {
        const IOPin &ioPin = g_ioPins[pin];
        if (ioPin.function != FUNCTION_UART) {
            GPIO_InitTypeDef GPIO_InitStruct = { 0 };

            GPIO_InitStruct.Pin = pin == 0 ? MCU_REV_GPIO(UART_RX_DIN1_Pin) : DIN2_Pin;
            GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
            GPIO_InitStruct.Pull = ioPin.polarity == io_pins::POLARITY_POSITIVE ? GPIO_PULLDOWN : GPIO_PULLUP;
            GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
            HAL_GPIO_Init(pin == 0 ? MCU_REV_GPIO(UART_RX_DIN1_GPIO_Port) : DIN2_GPIO_Port, &GPIO_InitStruct);
        }
    }
#endif
}

uint32_t calcPwmDutyInt(float duty, uint32_t periodInt) {
    return MIN((uint32_t)round((duty / 100.0f) * (periodInt + 1)), 65535);
}

void updatePwmDuty(int pin) {
#if defined EEZ_PLATFORM_STM32
    float duty = g_pwmDuty[pin - DOUT1];
    uint32_t periodInt = g_pwmPeriodInt[pin - DOUT1];
    uint32_t dutyInt = calcPwmDutyInt(duty, periodInt);
    /* Set the Capture Compare Register value */
    TIM3->CCR2 = dutyInt;
#else
    (void)pin;
#endif
}

void calcPwmPrescalerAndPeriod(float frequency, uint32_t &prescalerInt, uint32_t &periodInt) {
    if (frequency > 0) {
        double clockFrequency = 216000000.0;
        double prescaler = frequency < 2000.0 ? 108 : 0;
        double period = (clockFrequency / 2) / ((prescaler + 1) * frequency) - 1;
        if (period > 65535.0) {
            period = 65535.0;
            prescaler = (clockFrequency / 2) / (frequency * (period + 1)) - 1;
            if (prescaler > 65535.0) {
                prescaler = 65535.0;
            }
        }

        prescalerInt = MIN((uint32_t)round(prescaler), 65535);
        periodInt = MIN((uint32_t)round(period), 65535);
    } else {
        prescalerInt = 0;
        periodInt = 0;
    }
}

void pwmStop(int pin) {
    if (g_pwmStarted) {    
#if defined EEZ_PLATFORM_STM32
        HAL_TIM_PWM_Stop(&htim3, TIM_CHANNEL_2);
        HAL_TIM_PWM_DeInit(&htim3);
#endif
        g_pwmStarted = false;
    }

#if defined EEZ_PLATFORM_STM32
    // Configure DOUT2 GPIO pin
    GPIO_InitTypeDef GPIO_InitStruct = { 0 };
    GPIO_InitStruct.Pin = DOUT2_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(DOUT2_GPIO_Port, &GPIO_InitStruct);
#endif

    ioPinWrite(pin, 0);
}

void updatePwmFrequency(int pin) {
	float frequency = g_pwmFrequency[pin - DOUT1];
    uint32_t prescalerInt;
    uint32_t periodInt;
    calcPwmPrescalerAndPeriod(frequency, prescalerInt, periodInt);

    g_pwmPeriodInt[pin - DOUT1] = periodInt; 

#if defined EEZ_PLATFORM_STM32
    if (!g_pwmStarted) {
        MX_TIM3_Init();
    }

    /* Set the Prescaler value */
    TIM3->PSC = prescalerInt;

    /* Set the Autoreload value */
    TIM3->ARR = periodInt;

    updatePwmDuty(pin);

    if (isPowerUp() && frequency > 0) {
        if (!g_pwmStarted) {
            HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_2);
            g_pwmStarted = true;
        } else {
    		if (g_pwmStartedFrequency < 1 && frequency > 2 * g_pwmStartedFrequency) {
    			// forces an update event
            	TIM3->EGR |= TIM_EGR_UG;
    		}
        }

        g_pwmStartedFrequency = frequency;
    } else {
        pwmStop(pin);
    }
#endif
}

void initOutputPin(int pin) {
    const IOPin &ioPin = g_ioPins[pin];

    if (pin == DOUT1) {
#if defined EEZ_PLATFORM_STM32
        if (!bp3c::flash_slave::g_bootloaderMode) {
            if (ioPin.function != FUNCTION_UART) {
                // Configure DOUT1 GPIO pin
                GPIO_InitTypeDef GPIO_InitStruct = { 0 };
                GPIO_InitStruct.Pin = MCU_REV_GPIO(UART_TX_DOUT1_Pin);
                GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
                GPIO_InitStruct.Pull = GPIO_NOPULL;
                GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
                HAL_GPIO_Init(MCU_REV_GPIO(UART_TX_DOUT1_GPIO_Port), &GPIO_InitStruct);
            }
        }
#endif
    } else if (pin == DOUT2) {
        if (ioPin.function == io_pins::FUNCTION_PWM) {
            updatePwmFrequency(pin);
        } else {
            pwmStop(pin);
        }
    }
}

void reset() {
    for (int i = 0; i < NUM_IO_PINS; i++) {
        g_ioPins[i].polarity = 0;
        g_ioPins[i].function = 0;
        if (i > DOUT1) {
            g_pwmFrequency[i - DOUT1] = PWM_DEFAULT_FREQUENCY;
            g_pwmDuty[i - DOUT1] = PWM_DEFAULT_DUTY;
        }
    }
}

void tick() {
    // execute input pins function
    const IOPin &inputPin1 = g_ioPins[0];
    int inputPin1Value = ioPinRead(EXT_TRIG1);
    bool inputPin1State = (inputPin1Value && inputPin1.polarity == io_pins::POLARITY_POSITIVE) || (!inputPin1Value && inputPin1.polarity == io_pins::POLARITY_NEGATIVE);

    const IOPin &inputPin2 = g_ioPins[1];
    int inputPin2Value = ioPinRead(EXT_TRIG2);
    bool inputPin2State = (inputPin2Value && inputPin2.polarity == io_pins::POLARITY_POSITIVE) || (!inputPin2Value && inputPin2.polarity == io_pins::POLARITY_NEGATIVE);

    unsigned inhibited = persist_conf::devConf.isInhibitedByUser;

    if (!inhibited) {
        if (inputPin1.function == io_pins::FUNCTION_INHIBIT) {
            inhibited = inputPin1State;
        }

        if (inputPin2.function == io_pins::FUNCTION_INHIBIT) {
            inhibited = inputPin2State;
        }
    }

    if (inhibited != g_lastState.inhibited) {
        g_lastState.inhibited = inhibited;
        Channel::onInhibitedChanged(inhibited);
    }

    if ((inputPin1.function == io_pins::FUNCTION_SYSTRIG || inputPin1.function == io_pins::FUNCTION_DLOGTRIG) && inputPin1State && !g_ioPins[0].state) {
        trigger::generateTrigger(trigger::SOURCE_PIN1);
    }

    if ((inputPin2.function == io_pins::FUNCTION_SYSTRIG || inputPin2.function == io_pins::FUNCTION_DLOGTRIG) && inputPin2State && !g_ioPins[1].state) {
        trigger::generateTrigger(trigger::SOURCE_PIN2);
    }

    g_ioPins[0].state = inputPin1State;
    g_ioPins[1].state = inputPin2State;

    // end trigger output pulse
    if (g_lastState.toutputPulse) {
        int32_t diff = millis() - g_toutputPulseStartTickCountMs;
        if (diff > CONF_TOUTPUT_PULSE_WIDTH_MS) {
            for (int pin = 2; pin < NUM_IO_PINS; ++pin) {
                const IOPin &outputPin = g_ioPins[pin];
                if (outputPin.function == io_pins::FUNCTION_TOUTPUT) {
                    setPinState(pin, false);
                }
            }

            g_lastState.toutputPulse = 0;
        }
    }

    enum { UNKNOWN, UNCHANGED, CHANGED } trippedState = UNKNOWN, outputEnabledState = UNKNOWN;

    // execute output pins function
    for (int pin = 2; pin < NUM_IO_PINS; ++pin) {
        const IOPin &outputPin = g_ioPins[pin];

        if (outputPin.function == io_pins::FUNCTION_FAULT) {
            if (trippedState == UNKNOWN) {
                uint8_t outputFault = isOutputFault();
                if (g_lastState.outputFault != outputFault) {
                    g_lastState.outputFault = outputFault;
                    trippedState = CHANGED;
                } else {
                    trippedState = UNCHANGED;
                }
            }

            if (trippedState == CHANGED) {
                setPinState(pin, g_lastState.outputFault);
            }
        } else if (outputPin.function == io_pins::FUNCTION_ON_COUPLE) {
            if (outputEnabledState == UNKNOWN) {
                uint8_t outputEnabled = isOutputEnabled();
                if (g_lastState.outputEnabled != outputEnabled) {
                    g_lastState.outputEnabled = outputEnabled;
                    outputEnabledState = CHANGED;
                } else {
                    outputEnabledState = UNCHANGED;
                }
            }

            if (outputEnabledState == CHANGED) {
                setPinState(pin, g_lastState.outputEnabled);
            }
        }
    }
}

void onTrigger() {
    // start trigger output pulse
    for (int pin = 2; pin < NUM_IO_PINS; ++pin) {
        const IOPin &outputPin = g_ioPins[pin];
        if (outputPin.function == io_pins::FUNCTION_TOUTPUT) {
            setPinState(pin, true);
            g_lastState.toutputPulse = 1;
            g_toutputPulseStartTickCountMs = millis();
        }
    }
}

void refresh() {
    if (g_ioPins[0].function != FUNCTION_UART) {
        uart::refresh();
    }

    for (int pin = 0; pin < NUM_IO_PINS; ++pin) {
        if (pin < 2) {
            initInputPin(pin);
        } else {
            initOutputPin(pin);

            const IOPin &ioPin = g_ioPins[pin];
            if (ioPin.function == io_pins::FUNCTION_NONE) {
                setPinState(pin, false);
            } else if (ioPin.function == io_pins::FUNCTION_OUTPUT) {
                setPinState(pin, g_ioPins[pin].state);
            } else if (ioPin.function == io_pins::FUNCTION_FAULT) {
                if (g_lastState.outputFault != 2) {
                    setPinState(pin, g_lastState.outputFault);
                }
            } else if (ioPin.function == io_pins::FUNCTION_ON_COUPLE) {
                if (g_lastState.outputEnabled != 2) {
                    setPinState(pin, g_lastState.outputEnabled);
                }
            }
        }
    }

    if (g_ioPins[0].function == FUNCTION_UART) {
        uart::refresh();
    }
}

bool getIsInhibitedByUser() {
    return persist_conf::devConf.isInhibitedByUser;
}

void setIsInhibitedByUser(bool isInhibitedByUser) {
    persist_conf::setIsInhibitedByUser(isInhibitedByUser);
}

bool isInhibited() {
    return g_lastState.inhibited;
}

void setPinPolarity(int pin, unsigned polarity) {
    g_ioPins[pin].polarity = polarity;
    refresh();
}

void setPinFunction(int pin, unsigned function) {
    if (g_ioPins[pin].function != function) {
        if (pin == DIN1 && g_ioPins[DIN1].function == FUNCTION_UART) {
            g_ioPins[DOUT1].function = FUNCTION_NONE;
        }

        if (pin == DOUT1 && g_ioPins[DOUT1].function == FUNCTION_UART) {
            g_ioPins[DIN1].function = FUNCTION_NONE;
        }

        g_ioPins[pin].function = function;

        if (pin == DIN1 && function == FUNCTION_UART) {
            g_ioPins[DOUT1].function = FUNCTION_UART;
        }

        if (pin == DOUT1 && function == FUNCTION_UART) {
            g_ioPins[DIN1].function = FUNCTION_UART;
        }

        if (pin == DIN1 || pin == DIN2) {
            int otherPin = pin == DIN1 ? DIN2 : DIN1;

            if (function == io_pins::FUNCTION_SYSTRIG) {
                if (g_ioPins[otherPin].function == io_pins::FUNCTION_SYSTRIG) {
                    g_ioPins[otherPin].function = io_pins::FUNCTION_NONE;
                }

                trigger::Source triggerSource = pin == 0 ? trigger::SOURCE_PIN1 : trigger::SOURCE_PIN2;

                if (dlog_record::g_recordingParameters.triggerSource == triggerSource) {
                    dlog_record::setTriggerSource(trigger::SOURCE_IMMEDIATE);
                }

                if (trigger::g_triggerSource != triggerSource) {
                    trigger::setSource(triggerSource);
                }
            } else if (function == io_pins::FUNCTION_DLOGTRIG) {
                if (g_ioPins[otherPin].function == io_pins::FUNCTION_DLOGTRIG) {
                    g_ioPins[otherPin].function = io_pins::FUNCTION_NONE;
                }

                trigger::Source triggerSource = pin == 0 ? trigger::SOURCE_PIN1 : trigger::SOURCE_PIN2;

                if (trigger::g_triggerSource == triggerSource) {
                    trigger::setSource(trigger::SOURCE_IMMEDIATE);
                }

                if (dlog_record::g_recordingParameters.triggerSource != triggerSource) {
                    dlog_record::setTriggerSource(triggerSource);
                }
            } else {
                if (
					(pin == 0 && dlog_record::g_recordingParameters.triggerSource == trigger::SOURCE_PIN1) ||
					(pin == 1 && dlog_record::g_recordingParameters.triggerSource == trigger::SOURCE_PIN2)
				) {
                    dlog_record::setTriggerSource(trigger::SOURCE_IMMEDIATE);
                }

                if (
					(pin == 0 && trigger::g_triggerSource == trigger::SOURCE_PIN1) || 
					(pin == 1 && trigger::g_triggerSource == trigger::SOURCE_PIN2)
				) {
                    trigger::setSource(trigger::SOURCE_IMMEDIATE);
                }
            }
        }
    }

    io_pins::refresh();
}

void setPinState(int pin, bool state) {
    if (pin >= 2) {
        g_ioPins[pin].state = state;

        if (g_ioPins[pin].polarity == io_pins::POLARITY_NEGATIVE) {
            state = !state;
        }

        ioPinWrite(pin == 2 ? DOUT1 : DOUT2, state);
    }
}

bool getPinState(int pin) {
    if (pin < 2) {
        bool state;
        if (pin == 0) {
            state = ioPinRead(EXT_TRIG1) ? true : false;
            if (g_ioPins[0].polarity == io_pins::POLARITY_NEGATIVE) {
                state = !state;
            }
        } else {
            state = ioPinRead(EXT_TRIG2) ? true : false;
            if (g_ioPins[1].polarity == io_pins::POLARITY_NEGATIVE) {
                state = !state;
            }
        }
        return state;
    }

    return g_ioPins[pin].state;
}

void setPwmFrequency(int pin, float frequency) {
    if (pin >= DOUT1) {
        if (g_pwmFrequency[pin - DOUT1] != frequency) {
            g_pwmFrequency[pin - DOUT1] = frequency;
            updatePwmFrequency(pin);
        }
    }
}

float getPwmFrequency(int pin) {
    if (pin >= DOUT1) {
        return g_pwmFrequency[pin - DOUT1];
    }
    return 0;
}

void setPwmDuty(int pin, float duty) {
    if (pin >= DOUT1) {
        if (g_pwmDuty[pin - DOUT1] != duty) {
            g_pwmDuty[pin - DOUT1] = duty;
            updatePwmDuty(pin);
        }
    }
}

float getPwmDuty(int pin) {
    if (pin >= DOUT1) {
        return g_pwmDuty[pin - DOUT1];
    }
    return 0;
}

} // namespace io_pins
} // namespace psu
} // namespace eez
