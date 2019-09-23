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

/** @file conf_advanced.h
@brief Advanced compile time configuration.
*/

#pragma once

/// PSU serial number.
#define PSU_SERIAL "0000000"

/// Firmware version.
#define FIRMWARE "M1 0.1"

/// Manufacturer description text used for *IDN?
#define IDN_MANUFACTURER "Envox"

/// Model description text used for *IDN?
#if defined(EEZ_PLATFORM_STM32)
#define IDN_MODEL "EEZ BB3 (STM32)"
#else
#define IDN_MODEL "EEZ BB3 (Simulator)"
#endif

#define MCU_NAME "STM32F7"
#define MCU_REVISION "R1B5"

/// SCPI TCP server port.
#define TCP_PORT 5025

/// Name of the DAC chip.
#define DAC_NAME "DAC8552"

/// DAC chip resolution in number of bits.
#define DAC_RES 16

/// Allowed difference, in percentage, between DAC and ADC value during testing.
#define DAC_TEST_TOLERANCE 4.0f

/// Max. number of tries during DAC testing before giving up.
#define DAC_TEST_MAX_TRIES 3

/// Name of the ADC chip.
#define ADC_NAME "ADS1120"

/// ADC chip resolution in number of bits.
#define ADC_RES 15

#define ADC_READ_TIME_US 1800

/// How many times per second will ADC take snapshot value?
/// 0: 20 SPS, 1: 45 SPS, 2: 90 SPS, 3: 175 SPS, 4: 330 SPS, 5: 600 SPS, 6: 1000 SPS
#define ADC_SPS 5

/// ADC conversion should be finished after ADC_CONVERSION_MAX_TIME_MS milliseconds.
#define ADC_CONVERSION_MAX_TIME_MS 3

/// Maximum number of attempts to recover from ADC timeout before giving up.
#define MAX_ADC_TIMEOUT_RECOVERY_ATTEMPTS 3

/// Password minimum length in number characters.
#define PASSWORD_MIN_LENGTH 4

/// Password maximum length in number of characters.
#define PASSWORD_MAX_LENGTH 16

/// Calibration remark maximum length.
#define CALIBRATION_REMARK_MAX_LENGTH 32

/// Default calibration remark text.
#define CALIBRATION_REMARK_INIT "Not calibrated"

/// Temperature reading interval.
#define TEMP_SENSOR_READ_EVERY_MS 1000

/// Minimum OTP delay
#define OTP_AUX_MIN_DELAY 0.0f

/// Maximum OTP delay
#define OTP_AUX_MAX_DELAY 300.0f

/// Minimum OTP level
#define OTP_AUX_MIN_LEVEL 0.0f

/// Maximum OTP level
#define OTP_AUX_MAX_LEVEL 100.0f

/// Number of profile storage locations
#define NUM_PROFILE_LOCATIONS 10

/// Profile name maximum length in number of characters.
#define PROFILE_NAME_MAX_LENGTH 32

/// Size in number characters of SCPI parser input buffer.
#define SCPI_PARSER_INPUT_BUFFER_LENGTH 2048

/// Size of SCPI parser error queue.
#define SCPI_PARSER_ERROR_QUEUE_SIZE 20

/// Since we are not using timer, but ADC interrupt for the OVP and
/// OCP delay measuring there will be some error (size of which
/// depends on ADC_SPS value). You can use the following value, which
/// will be subtracted from the OVP and OCP delay, to correct this error.
/// Value is given in seconds.
#define PROT_DELAY_CORRECTION 0.002f

/// This is the delay period, after the channel output went OFF,
/// after which we shall turn DP off.
/// Value is given in seconds.
#define DP_OFF_DELAY_PERIOD 1

/// Text returned by the SYStem:CAPability command
#define STR_SYST_CAP "DCSUPPLY WITH (MEASURE|MULTIPLE|TRIGGER)"

/// Set to 1 to skip the test of PWRGOOD signal
#define CONF_SKIP_PWRGOOD_TEST 0

/// Minimal temperature (in oC) for sensor to be declared as valid.
#define TEMP_SENSOR_MIN_VALID_TEMPERATURE -55
#define TEMP_SENSOR_MAX_VALID_TEMPERATURE 125

/// Interval at which fan speed should be adjusted
#define FAN_SPEED_ADJUSTMENT_INTERVAL 500

/// Interval at which fan speed should be measured
#define FAN_SPEED_MEASURMENT_INTERVAL 5000

/// Fan switch-on temperature (in oC)
#define FAN_MIN_TEMP 50

/// Max. allowed temperature (in oC), if it stays more then FAN_MAX_TEMP_DELAY seconds then main
/// power will be turned off.
#define FAN_MAX_TEMP 75

/// PWM value for min. fan speed
#define FAN_MIN_PWM 30

/// PWM value for max. fan speed
#define FAN_MAX_PWM 255

/// FAN PID controller parameters
#define FAN_PID_KP 0.1f
#define FAN_PID_KI_MIN 0.05f
#define FAN_PID_KI_MAX 0.8f
#define FAN_PID_KD 0
#define FAN_PID_POn                                                                                \
    1 // PoM: 0, PoE: 1, see
      // http://brettbeauregard.com/blog/2017/06/introducing-proportional-on-measurement/

/// Min. PWM after which fan failed will be asserted if RPM is not measured
#define FAN_FAILED_THRESHOLD 15

/// Max. allowed output current (in ampers) if fan or temp. sensor is invalid.
#define ERR_MAX_CURRENT 2.0f

/// Nominal fan RPM (for PWM=255).
#define FAN_NOMINAL_RPM 4500

/// Number of seconds after which main power will be turned off.
#define FAN_MAX_TEMP_DELAY 30

/// Temperature drop (in oC) below FAN_MAX_TEMP to turn again main power on. Premature attempt to
/// turn power on will report error -200.
#define FAN_MAX_TEMP_DROP 15

/// Enable/disable RPM measurement during work - it will still be enabled at the boot during fan
/// test.
#define FAN_OPTION_RPM_MEASUREMENT 1

/// Interval (in milliseconds) at which watchdog impulse will be sent
#define WATCHDOG_INTERVAL 250

/// Interval (in minutes) at which "on time" will be written to EEPROM
#define WRITE_ONTIME_INTERVAL 10

/// Maximum allowed length (including label) of the keypad text.
#define MAX_KEYPAD_TEXT_LENGTH 128

/// Enable transition to the Main page after period of inactivity.
#define GUI_BACK_TO_MAIN_ENABLED 0

/// Inactivity period duration in seconds before transition to the Main page.
#define GUI_BACK_TO_MAIN_DELAY 10

/// How much to wait (in seconds) for a lease for an IP address from a DHCP server
/// until we declare ethernet initialization failure.
#define ETHERNET_DHCP_TIMEOUT 15

/// Output power is monitored and if its go below DP_NEG_LEV
/// that is negative value in Watts (default -1 W),
/// and that condition lasts more then DP_NEG_DELAY seconds (default 5 s),
/// down-programmer circuit has to be switched off.
#define DP_NEG_LEV -5 // -5 W

/// See DP_NEG_LEV.
#define DP_NEG_DELAY 5 // 5 s

/// Replace standard SPI transactions implementation with in-house implementation,
/// It is more simple version where all interrupts are disabled during SPI transactions.
/// We had some problems (WATCHDOG, ADC timeout and EEPROM errros) with SPI in the
/// past and when we used our SPI transactions implementation, problems disappeared.
/// But, unfortunately, now ethernet doesn't work.
#define REPLACE_SPI_TRANSACTIONS_IMPLEMENTATION 0

/// Number of history values shown in YT diagram. This value must be the same as
/// the width of YT widget.
#define CHANNEL_HISTORY_SIZE 352

#define GUI_YT_VIEW_RATE_DEFAULT 0.1f
#define GUI_YT_VIEW_RATE_MIN 0.01f
#define GUI_YT_VIEW_RATE_MAX 300.0f

#define MAX_LIST_LENGTH 256

#define LIST_DWELL_MIN 0.0001f
#define LIST_DWELL_MAX 65535.0f
#define LIST_DWELL_DEF 0.01f

#define MAX_LIST_COUNT 65535

#define LISTS_DIR PATH_SEPARATOR "LISTS"
#define PROFILES_DIR PATH_SEPARATOR "PROFILES"
#define LIST_FILE_EXTENSION ".CSV"
#define MAX_PATH_LENGTH 255
#define CSV_SEPARATOR ','
#define LIST_CSV_FILE_NO_VALUE_CHAR '='

/// Time in seconds of SCPI inactivity to declare SCPI to be idle.
#define SCPI_IDLE_TIMEOUT 60

/// Time in seconds of GUI inactivity to declare GUI to be idle.
#define GUI_IDLE_TIMEOUT 1

/// Time in seconds of Encoder inactivity to declare Encoder to be idle.
#define ENCODER_IDLE_TIMEOUT 60

/// Changed but not confirmed value will be reset to current one
/// after this timeout in seconds.
/// See https://github.com/eez-open/psu-firmware/issues/84
#define ENCODER_CHANGE_TIMEOUT 15

#define DISPLAY_BRIGHTNESS_MIN 1
#define DISPLAY_BRIGHTNESS_MAX 20
#define DISPLAY_BRIGHTNESS_DEFAULT 20

#define DISPLAY_BACKGROUND_LUMINOSITY_STEP_MIN 0
#define DISPLAY_BACKGROUND_LUMINOSITY_STEP_MAX 20
#define DISPLAY_BACKGROUND_LUMINOSITY_STEP_DEFAULT 10
#define DISPLAY_BACKGROUND_COLOR_R 80
#define DISPLAY_BACKGROUND_COLOR_G 128
#define DISPLAY_BACKGROUND_COLOR_B 255

/// Number of values used for ADC averaging
#define NUM_ADC_AVERAGING_VALUES 30

/// Width of the trigger output pulse, in milliseconds.
#define CONF_TOUTPUT_PULSE_WIDTH_MS 100

/// Duration of BP LED's flash during boot and test
#define CONF_BP_TEST_FLASH_DURATION_MS 500

#define CONF_DEFAULT_NTP_SERVER "europe.pool.ntp.org"

/// Query NTP server every 24 hours
#define CONF_NTP_PERIOD_SEC 24L * 60 * 60

/// Query NTP server every 10 minutes if error happened last time
#define CONF_NTP_PERIOD_AFTER_ERROR_SEC 10L * 60

/// To prevent too fast switching betweeen current ranges
#define CURRENT_AUTO_RANGE_SWITCHING_DELAY_MS 5

/// Change to 1 if you want to add jitter column in DLOG file
#define CONF_DLOG_JITTER 0

/// During data logging call file.sync every N seconds
#define CONF_DLOG_SYNC_FILE_TIME 10 // 10 seconds

/// Size of serial port output buffer
#define CONF_SERIAL_BUFFER_SIZE 1024

// Default duration of all animations in seconds
#define CONF_DEFAULT_ANIMATIONS_DURATION 0.25f
