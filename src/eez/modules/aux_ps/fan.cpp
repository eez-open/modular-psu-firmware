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

#if OPTION_FAN

#include <eez/modules/aux_ps/fan.h>
#include <eez/modules/aux_ps/pid.h>

#include <eez/modules/psu/psu.h>
#include <eez/modules/psu/channel.h>
#include <eez/modules/psu/scpi/psu.h>
#include <eez/modules/psu/temperature.h>
#include <eez/modules/psu/persist_conf.h>

#include <eez/modules/dib-dcp405/channel.h>

#include <eez/system.h>

#if defined(EEZ_PLATFORM_STM32)
#include <i2c.h>
#endif

#include <eez/modules/psu/psu.h>
#include <eez/modules/psu/gui/psu.h>
#include <eez/modules/psu/gui/page_sys_settings.h>

namespace eez {
namespace aux_ps {
namespace fan {

////////////////////////////////////////////////////////////////////////////////

TestResult g_testResult = TEST_FAILED;

static int g_fanSpeedPWM;

double g_Kp = FAN_PID_KP;
double g_Ki = FAN_PID_KI_MIN;
double g_Kd = FAN_PID_KD;
int g_POn = FAN_PID_POn;

static double g_pidTemp;
static double g_pidDuty;
static double g_pidTarget = FAN_MIN_TEMP;
static PID g_fanPID(&g_pidTemp, &g_pidDuty, &g_pidTarget, FAN_PID_KP, FAN_PID_KI_MIN, FAN_PID_KD, FAN_PID_POn, REVERSE);

int g_rpm = 0;

////////////////////////////////////////////////////////////////////////////////

#if defined(EEZ_PLATFORM_STM32)

// MAX31760
// Precision Fan-Speed Controller with Nonvolatile Lookup Table
// https://datasheets.maximintegrated.com/en/ds/MAX31760.pdf

#define MAX31760_DEVICE_ADDRESS 0xAE

#define CONF_FAN_TEST_DURATION 10000000 // 10 sec
#define CONF_FAN_TEST_PWM FAN_MIN_PWM

#define CONF_NUM_TACH_PULSES_PER_REVOLUTION 2

#define REG_CR1  0x00 // Control register 1
#define REG_CR2  0x01 // Control register 2
#define REG_CR3  0x02 // Control register 3

#define REG_FFDC 0x03 // Fan Fault Duty Cycle
#define REG_MASK 0x04 // Alert Mask Register

#define REG_PWMR 0x50 // Direct Duty-Cycle Control Register

#define REG_TC1H 0x52 // TACH1 Count Register, MSB
#define REG_TC1L 0x53 // TACH1 Count Register, LSB
#define REG_TC2H 0x54 // TACH2 Count Register, MSB
#define REG_TC2L 0x55 // TACH2 Count Register, LSB

#define REG_RTH 0x56 // Remote Temperature Reading Register, MSB
#define REG_RTL 0x57 // Remote Temperature Reading Register, LSB
#define REG_LTH 0x58 // Locale Temperature Reading Register, MSB
#define REG_LTL 0x59 // Locale Temperature Reading Register, LSB

#define REG_STATUS 0x5A // Status Register (SR)

#define SR_TACH1A 0x01

#define FREQ_33HZ   (0x00 << 3)
#define FREQ_150HZ  (0x01 << 3)
#define FREQ_1500HZ (0x02 << 3)
#define FREQ_25KHZ  (0x03 << 3)

#define RAMP_RATE_SLOW        (0x00 << 4)
#define RAMP_RATE_SLOW_MEDIUM (0x01 << 4)
#define RAMP_RATE_MEDIUM_FAST (0x02 << 4)
#define RAMP_RATE_FAST        (0x03 << 4)

uint8_t g_cr1 = 0;
uint8_t g_cr2 = 0;
uint8_t g_cr3 = 0;

uint32_t g_fanSpeedLastMeasuredTick;

////////////////////////////////////////////////////////////////////////////////

class PWMMeasurement {
public:
	void addMeasurement(int pwm, int rpm) {
		// DebugTrace("%d pwm = %d rpm\n", pwm, rpm);
		measurements[pwm] = rpm;
	}

	int measurePWM(int speedPercentage, int &rpm) {
    	rpm = 0;

    	int a = FAN_MIN_PWM;
        int b = FAN_MAX_PWM;

        if (measurements[a] == 0) {
        	return a;
		}

        if (measurements[b] == 0) {
        	return b;
		}

        int target = measurements[a] + (speedPercentage - 1) * (measurements[b] - measurements[a]) / 99;

        for (int i = a; i < b; i++) {
			if (measurements[i] != 0) {
				if (measurements[i] >= target)  {
					break;
				}
				a = i;
			}
        }

        for (int i = b; i > a; i--) {
			if (measurements[i] != 0) {
				if (measurements[i] <= target)  {
					break;
				}
				b = i;
			}
        }

		int pwm = (a + b) / 2;

        rpm = measurements[pwm];

        return pwm;
	}

private:
	int measurements[256];
};

PWMMeasurement g_pwmMeasurement;

////////////////////////////////////////////////////////////////////////////////

HAL_StatusTypeDef readReg(uint8_t reg, uint8_t *value) {
	taskENTER_CRITICAL();
    HAL_StatusTypeDef returnValue = HAL_I2C_Master_Transmit(&hi2c1, MAX31760_DEVICE_ADDRESS, &reg, 1, 5);
	taskEXIT_CRITICAL();
    if (returnValue != HAL_OK) {
    	return returnValue;
    }

	taskENTER_CRITICAL();
    returnValue = HAL_I2C_Master_Receive(&hi2c1, MAX31760_DEVICE_ADDRESS, value, 1, 5);
	taskEXIT_CRITICAL();
    if (returnValue != HAL_OK) {
    	return returnValue;
    }

    return HAL_OK;
}

HAL_StatusTypeDef writeReg(uint8_t reg, uint8_t value) {
	uint8_t data[2];
	data[0] = reg;
	data[1] = value;
	taskENTER_CRITICAL();
    HAL_StatusTypeDef returnValue = HAL_I2C_Master_Transmit(&hi2c1, MAX31760_DEVICE_ADDRESS, data, 2, 5);
	taskEXIT_CRITICAL();
	return returnValue;
}

HAL_StatusTypeDef readTemp(uint8_t reg, float *temp) {
	static const float RESOLUTION = 0.125f;
	HAL_StatusTypeDef returnValue;

	int16_t regVal;

	returnValue = readReg(reg + 0, ((uint8_t *)&regVal) + 1); // read MSB
    if (returnValue != HAL_OK) {
    	return returnValue;
    }

	returnValue = readReg(reg + 1, ((uint8_t *)&regVal) + 0); // read LSB
    if (returnValue != HAL_OK) {
    	return returnValue;
    }

	*temp = (regVal >> 5) * RESOLUTION;
	return HAL_OK;
}

HAL_StatusTypeDef readLocalTemp(float *temp) {
	return readTemp(REG_LTH, temp);
}

HAL_StatusTypeDef readRemoteTemp(float *temp) {
	return readTemp(REG_RTH, temp);
}

HAL_StatusTypeDef readRpm(uint8_t reg, int n, float *rpm) {
	HAL_StatusTypeDef returnValue;

	uint8_t tch;
	returnValue = readReg(reg + 0, &tch); // read MSB
    if (returnValue != HAL_OK) {
    	return returnValue;
    }

    uint8_t tcl;
	returnValue = readReg(reg + 1, &tcl); // read LSB
    if (returnValue != HAL_OK) {
    	return returnValue;
    }

	*rpm = 60 * 100000 / (tch * 256 + tcl) / n;
	return HAL_OK;
}

HAL_StatusTypeDef setPwmFrequency(uint8_t freq) {
	g_cr1 = (g_cr1 & 0b11100111) | freq;
	return writeReg(REG_CR1, (uint8_t)g_cr1);
}

enum PwmPolarity {
	PWM_POLARITY_POSITIVE,
	PWM_POLARITY_NEGATIVE
};

HAL_StatusTypeDef setPwmPolarity(PwmPolarity pwmPolarity) {
	if (pwmPolarity) {
		g_cr1 |= 0b00000100;
	} else {
		g_cr1 &= 0b00000100;
	}

	return writeReg(REG_CR1, g_cr1);
}

HAL_StatusTypeDef setStandbyMode(bool enable) {
	if (enable) {
		g_cr2 |= 0x80;
	} else {
		g_cr2 &= ~0x80;
	}
	return writeReg(REG_CR2, g_cr2);
}

HAL_StatusTypeDef setFanSpinUp(bool enable) {
	if (enable) {
		g_cr2 |= 0x20;
	} else {
		g_cr2 &= ~0x20;
	}
	return writeReg(REG_CR2, g_cr2);
}

HAL_StatusTypeDef setDirectFanControl(bool enable) {
	if (enable) {
		g_cr2 |= 0x01;
	} else {
		g_cr2 &= ~0x01;
	}
	return writeReg(REG_CR2, g_cr2);
}

HAL_StatusTypeDef clearFanFail() {
	return writeReg(REG_CR3, g_cr3 | 0x80);
}

HAL_StatusTypeDef setPwmDutyCycleRampRate(uint8_t rampRate) {
	g_cr3 = (g_cr3 & 0b11001111) | rampRate;
	return writeReg(REG_CR3, g_cr3);
}

HAL_StatusTypeDef setTachFull(bool enable) {
	if (enable) {
		g_cr3 |= 0x08;
	} else {
		g_cr3 &= ~0x08;
	}
	return writeReg(REG_CR3, g_cr3);
}

HAL_StatusTypeDef setPulseStretchEnable(bool enable) {
	if (enable) {
		g_cr3 |= 0x04;
	} else {
		g_cr3 &= ~0x04;
	}
	return writeReg(REG_CR3, g_cr3);
}

HAL_StatusTypeDef setTach2Enable(bool enable) {
	if (enable) {
		g_cr3 |= 0x02;
	} else {
		g_cr3 &= ~0x02;
	}
	return writeReg(REG_CR3, g_cr3);
}

HAL_StatusTypeDef setTach1Enable(bool enable) {
	if (enable) {
		g_cr3 |= 0x01;
	} else {
		g_cr3 &= ~0x01;
	}
	return writeReg(REG_CR3, g_cr3);
}

HAL_StatusTypeDef setPwmDutyCycle(uint8_t dutyCycle) {
	return writeReg(REG_PWMR, dutyCycle);
}

#endif

////////////////////////////////////////////////////////////////////////////////

void init() {
	g_fanPID.SetSampleTime(FAN_SPEED_ADJUSTMENT_INTERVAL);
	g_fanPID.SetOutputLimits(0, 255);
	g_fanPID.SetMode(AUTOMATIC);

#if defined(EEZ_PLATFORM_STM32)
	setStandbyMode(false);
	clearFanFail();
	setFanSpinUp(false);
	setDirectFanControl(true);
	setPwmFrequency(FREQ_1500HZ);
	setPwmPolarity(PWM_POLARITY_NEGATIVE);
	setPwmDutyCycleRampRate(RAMP_RATE_FAST);
	setPwmDutyCycle(0);
	setTachFull(false);
	setPulseStretchEnable(false);
	setTach2Enable(false);
	setTach1Enable(true);
#endif
}

bool test() {
#if defined(EEZ_PLATFORM_STM32)
	// start testing
	g_testResult = TEST_NONE;
	setPwmDutyCycle(CONF_FAN_TEST_PWM);
	g_fanSpeedLastMeasuredTick = micros();
#endif

#if defined(EEZ_PLATFORM_SIMULATOR)
	g_testResult = TEST_OK;
#endif

    return g_testResult == TEST_OK || g_testResult == TEST_NONE;
}

void getIMonMax(float &iMonMax, float &iMax) {
	using namespace psu;
    iMonMax = 0;
    iMax = 0;
	for (int i = 0; i < CH_NUM; ++i) {
		Channel& channel = Channel::get(i);
        if (channel.isOutputEnabled()) {
            if (channel.i.mon_last > iMonMax) {
                iMonMax = channel.i.mon_last;
            }
        }
        if (channel.params.I_MAX > iMax) {
            iMax = channel.params.I_MAX;
        }
	}
}

#if defined(EEZ_PLATFORM_STM32)
void genFanError() {
	g_testResult = TEST_FAILED;
	generateError(SCPI_ERROR_FAN_TEST_FAILED);
	psu::setQuesBits(QUES_FAN, true);
	psu::limitMaxCurrent(psu::MAX_CURRENT_LIMIT_CAUSE_FAN);
}

bool checkStatus() {
	uint8_t status;
	readReg(REG_STATUS, &status);
	if (status & SR_TACH1A) {
		// TACH1A alarm
		genFanError();
		return false;
	}
	return true;
}

void checkTest() {
	int32_t diff = micros() - g_fanSpeedLastMeasuredTick;

	if (checkStatus()) {
    	if (diff >= CONF_FAN_TEST_DURATION) {
    		g_testResult = TEST_OK;
    	}
	} 
	// else {
	// 	DebugTrace("FAN test duration: %d", diff);
	// }

	if (g_testResult != TEST_NONE) {
		// test is finished
		setPwmDutyCycle(g_fanSpeedPWM);
	}
}

#endif

int updateFanSpeed() {
    int newFanSpeedPWM = g_fanSpeedPWM;

    uint8_t fanMode;
    uint8_t fanSpeed;
    auto page = (psu::gui::SysSettingsTemperaturePage *)psu::gui::getPage(PAGE_ID_SYS_SETTINGS_TEMPERATURE);
    if (page) {
        fanMode = page->fanMode;
        fanSpeed = page->fanSpeedPWM;
    } else {
        fanMode = psu::persist_conf::devConf.fanMode;
        fanSpeed = psu::persist_conf::devConf.fanSpeedPWM;
    }

    if (psu::isPowerUp() && fanMode == FAN_MODE_MANUAL) {
        if (fanSpeed == 0) {
            newFanSpeedPWM = 0;
        } else {
        	if (page && page->fanPWMMeasuringInProgress) {
#if defined(EEZ_PLATFORM_STM32)
        		int rpm;
				newFanSpeedPWM = g_pwmMeasurement.measurePWM((uint8_t)page->fanSpeedPercentage.getFloat(), rpm);
				if (rpm != 0) {
					page->fanSpeedPWM = newFanSpeedPWM;
					page->fanPWMMeasuringInProgress = false;
					g_rpm = rpm;
				}
#else
                newFanSpeedPWM = FAN_MAX_PWM;
				page->fanSpeedPWM = newFanSpeedPWM;
				page->fanPWMMeasuringInProgress = false;
#endif
        		page->fanSpeedPWM = newFanSpeedPWM;
        	} else {
        		newFanSpeedPWM = fanSpeed;
        	}
        }
    } else {
		if (psu::CH_NUM > 0) {
			// adjust fan speed depending on max. channel temperature
			float maxChannelTemperature = psu::temperature::getMaxChannelTemperature();

			float iMonMax, iMax;
			getIMonMax(iMonMax, iMax);
			float Ki = roundPrec(remap(iMonMax * iMonMax, 0, FAN_PID_KI_MIN, iMax * iMax, FAN_PID_KI_MAX), 0.05f);
			if (Ki != g_Ki) {
				g_Ki = Ki;
				g_fanPID.SetTunings(g_Kp, g_Ki, g_Kd, g_POn);
				g_pidTarget = FAN_MIN_TEMP - 2.0 * iMonMax;
			}

			bool setMaxPwm = false;

			g_pidTemp = setMaxPwm ? TEMP_SENSOR_MAX_VALID_TEMPERATURE : maxChannelTemperature;
			if (g_fanPID.Compute()) {
				newFanSpeedPWM = (int)round(g_pidDuty);

				if (newFanSpeedPWM <= 0) {
					newFanSpeedPWM = 0;
				} else {
					newFanSpeedPWM += FAN_MIN_PWM;
				}

				if (newFanSpeedPWM > FAN_MAX_PWM) {
					newFanSpeedPWM = FAN_MAX_PWM;
				}
			}

			if (setMaxPwm) {
				newFanSpeedPWM = FAN_MAX_PWM;
			}
		} else {
			newFanSpeedPWM = 0;
		}
    }

	return newFanSpeedPWM;
}

void tick(uint32_t tickCount) {
#if defined(EEZ_PLATFORM_STM32)
    if (g_testResult == TEST_NONE) {
    	// still testing
    	checkTest();
    }
#endif

    if (g_testResult != TEST_OK) {
        return;
    }

#if defined(EEZ_PLATFORM_STM32)
	int32_t diff = tickCount - g_fanSpeedLastMeasuredTick;
	if (diff >= FAN_SPEED_MEASURMENT_INTERVAL * 1000L) {
	    g_fanSpeedLastMeasuredTick = tickCount;

	    if (g_fanSpeedPWM != 0) {
			if (checkStatus()) {
				float rpm;
				if (readRpm(REG_TC1H, CONF_NUM_TACH_PULSES_PER_REVOLUTION, &rpm) == HAL_OK) {
					g_rpm = (int)roundf(rpm);
					g_pwmMeasurement.addMeasurement(g_fanSpeedPWM, g_rpm);
				}
			}
		}

	    int newFanSpeedPWM = updateFanSpeed();
	    if (newFanSpeedPWM != g_fanSpeedPWM) {
	    	g_fanSpeedPWM = newFanSpeedPWM;
	    	setPwmDutyCycle(g_fanSpeedPWM);
	    	if (g_fanSpeedPWM == 0) {
	    		g_rpm = 0;
	    	}
	    }
	}
#endif

#if defined(EEZ_PLATFORM_SIMULATOR)
	g_fanSpeedPWM = updateFanSpeed();
    if (g_fanSpeedPWM != 0) {
        g_rpm = (int)roundf(remap(1.0f * g_fanSpeedPWM, FAN_MIN_PWM, 500, FAN_MAX_PWM, 3200));
    } else {
        g_rpm = 0;
    }
#endif
}

void setPidTunings(double Kp, double Ki, double Kd, int POn) {
	g_Kp = Kp;
	g_Ki = Ki;
	g_Kd = Kd;
	g_POn = POn;

	g_fanPID.SetTunings(g_Kp, g_Ki, g_Kd, g_POn);
}

float readTemperature() {
#if defined(EEZ_PLATFORM_STM32)
	float temperature;
	if (readLocalTemp(&temperature) == HAL_OK) {
		return roundPrec(temperature, 1.0f);
	}
#endif
	return NAN;
}

} // namespace fan
} // namespace aux_ps
} // namespace eez

#endif

namespace eez {
namespace aux_ps {
namespace fan {

FanStatus getStatus() {
#if OPTION_FAN
	if (aux_ps::fan::g_testResult == TEST_FAILED || aux_ps::fan::g_testResult == TEST_WARNING) {
		return FAN_STATUS_INVALID;
	} else if (aux_ps::fan::g_testResult == TEST_OK) {
		return FAN_STATUS_VALID;
	} else if (aux_ps::fan::g_testResult == TEST_NONE) {
		return FAN_STATUS_TESTING;
	} else {
		return FAN_STATUS_NOT_INSTALLED;
	}
#else
	return FAN_STATUS_NOT_INSTALLED;
#endif
}

} // namespace fan
} // namespace aux_ps
} // namespace eez
