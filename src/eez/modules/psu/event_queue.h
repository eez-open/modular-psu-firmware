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

namespace eez {
namespace psu {
namespace event_queue {

static const int EVENT_TYPE_NONE = 0;
static const int EVENT_TYPE_DEBUG = 1;
static const int EVENT_TYPE_INFO = 2;
static const int EVENT_TYPE_WARNING = 3;
static const int EVENT_TYPE_ERROR = 4;

////////////////////////////////////////////////////////////////////////////////

#define LIST_OF_EVENTS                                                                             \
    EVENT_SCPI_ERROR(SCPI_ERROR_AUX_TEMP_SENSOR_TEST_FAILED, "AUX temp failed")                    \
    EVENT_SCPI_ERROR(SCPI_ERROR_CH1_TEMP_SENSOR_TEST_FAILED, "CH1 temp failed")                    \
    EVENT_SCPI_ERROR(SCPI_ERROR_CH2_TEMP_SENSOR_TEST_FAILED, "CH2 temp failed")                    \
    EVENT_SCPI_ERROR(SCPI_ERROR_CH3_TEMP_SENSOR_TEST_FAILED, "CH3 temp failed")                    \
    EVENT_SCPI_ERROR(SCPI_ERROR_CH4_TEMP_SENSOR_TEST_FAILED, "CH4 temp failed")                    \
    EVENT_SCPI_ERROR(SCPI_ERROR_CH5_TEMP_SENSOR_TEST_FAILED, "CH5 temp failed")                    \
    EVENT_SCPI_ERROR(SCPI_ERROR_CH6_TEMP_SENSOR_TEST_FAILED, "CH6 temp failed")                    \
    EVENT_SCPI_ERROR(SCPI_ERROR_CH1_DOWN_PROGRAMMER_SWITCHED_OFF, "DProg CH1 disabled")            \
    EVENT_SCPI_ERROR(SCPI_ERROR_CH2_DOWN_PROGRAMMER_SWITCHED_OFF, "DProg CH2 disabled")            \
    EVENT_SCPI_ERROR(SCPI_ERROR_CH3_DOWN_PROGRAMMER_SWITCHED_OFF, "DProg CH3 disabled")            \
    EVENT_SCPI_ERROR(SCPI_ERROR_CH4_DOWN_PROGRAMMER_SWITCHED_OFF, "DProg CH4 disabled")            \
    EVENT_SCPI_ERROR(SCPI_ERROR_CH5_DOWN_PROGRAMMER_SWITCHED_OFF, "DProg CH5 disabled")            \
    EVENT_SCPI_ERROR(SCPI_ERROR_CH6_DOWN_PROGRAMMER_SWITCHED_OFF, "DProg CH6 disabled")            \
    EVENT_SCPI_ERROR(SCPI_ERROR_CH1_OUTPUT_FAULT_DETECTED, "CH1 output fault")                     \
    EVENT_SCPI_ERROR(SCPI_ERROR_CH2_OUTPUT_FAULT_DETECTED, "CH2 output fault")                     \
    EVENT_SCPI_ERROR(SCPI_ERROR_CH3_OUTPUT_FAULT_DETECTED, "CH3 output fault")                     \
    EVENT_SCPI_ERROR(SCPI_ERROR_CH4_OUTPUT_FAULT_DETECTED, "CH4 output fault")                     \
    EVENT_SCPI_ERROR(SCPI_ERROR_CH5_OUTPUT_FAULT_DETECTED, "CH5 output fault")                     \
    EVENT_SCPI_ERROR(SCPI_ERROR_CH6_OUTPUT_FAULT_DETECTED, "CH6 output fault")                     \
    EVENT_ERROR(CH_OVP_TRIPPED, 0, "Ch%d OVP tripped")                                             \
    EVENT_ERROR(CH_OCP_TRIPPED, 10, "Ch%d OCP tripped")                                            \
    EVENT_ERROR(CH_OPP_TRIPPED, 20, "Ch%d OPP tripped")                                            \
    EVENT_ERROR(FILE_UPLOAD_FAILED, 30, "File upload failed")                                      \
    EVENT_ERROR(FILE_DOWNLOAD_FAILED, 31, "File download failed")                                  \
    EVENT_ERROR(CH_IOEXP_RESET_DETECTED, 40, "Ch%d IOEXP reset detected")                          \
    EVENT_ERROR(CH_IOEXP_FAULT_MATCH_DETECTED, 50, "Ch%d IOEXP fault match detected")              \
    EVENT_ERROR(AUX_OTP_TRIPPED, 60, "AUX OTP tripped")                                            \
    EVENT_ERROR(CH_OTP_TRIPPED, 70, "CH%d OTP tripped")                                            \
    EVENT_ERROR(CH_REMOTE_SENSE_REVERSE_POLARITY_DETECTED, 80, "CH%d rsense reverse polarity detected") \
	EVENT_ERROR(EEPROM_MCU_READ_ERROR, 86, "EEPROM read error on MCU")                             \
	EVENT_ERROR(EEPROM_SLOT1_READ_ERROR, 87, "EEPROM read error on module 1")                      \
	EVENT_ERROR(EEPROM_SLOT2_READ_ERROR, 88, "EEPROM read error on module 2")                      \
	EVENT_ERROR(EEPROM_SLOT3_READ_ERROR, 89, "EEPROM read error on module 3")                      \
	EVENT_ERROR(EEPROM_MCU_CRC_CHECK_ERROR, 90, "EEPROM CRC check error on MCU")                   \
	EVENT_ERROR(EEPROM_SLOT1_CRC_CHECK_ERROR, 91, "EEPROM CRC check error on module 1")            \
	EVENT_ERROR(EEPROM_SLOT2_CRC_CHECK_ERROR, 92, "EEPROM CRC check error on module 2")            \
	EVENT_ERROR(EEPROM_SLOT3_CRC_CHECK_ERROR, 93, "EEPROM CRC check error on module 3")            \
	EVENT_ERROR(EEPROM_MCU_WRITE_ERROR, 94, "EEPROM write error on MCU")                           \
	EVENT_ERROR(EEPROM_SLOT1_WRITE_ERROR, 95, "EEPROM write error on module 1")                    \
	EVENT_ERROR(EEPROM_SLOT2_WRITE_ERROR, 96, "EEPROM write error on module 2")                    \
	EVENT_ERROR(EEPROM_SLOT3_WRITE_ERROR, 97, "EEPROM write error on module 3")                    \
	EVENT_ERROR(EEPROM_MCU_WRITE_VERIFY_ERROR, 98, "EEPROM write verify error on MCU")             \
	EVENT_ERROR(EEPROM_SLOT1_WRITE_VERIFY_ERROR, 99, "EEPROM write verify error on module 1")      \
	EVENT_ERROR(EEPROM_SLOT2_WRITE_VERIFY_ERROR, 100, "EEPROM write verify error on module 2")     \
	EVENT_ERROR(EEPROM_SLOT3_WRITE_VERIFY_ERROR, 101, "EEPROM write verify error on module 3")     \
	EVENT_ERROR(SLOT1_CRC_CHECK_ERROR, 102, "CRC error on module 1")                               \
	EVENT_ERROR(SLOT2_CRC_CHECK_ERROR, 103, "CRC error on module 2")                               \
	EVENT_ERROR(SLOT3_CRC_CHECK_ERROR, 104, "CRC error on module 3")                               \
	EVENT_ERROR(DLOG_FILE_OPEN_ERROR, 110, "DLOG file open error")                                 \
	EVENT_ERROR(DLOG_TRUNCATE_ERROR, 111, "DLOG truncate error")                                   \
	EVENT_ERROR(DLOG_FILE_REOPEN_ERROR, 112, "DLOG file reopen error")                             \
	EVENT_ERROR(DLOG_WRITE_ERROR, 113, "DLOG write error")                                         \
    EVENT_ERROR(DLOG_SEEK_ERROR, 114, "DLOG seek error")                                           \
    EVENT_ERROR(SAVE_DEV_CONF_BLOCK_0, 120, "Failed to save configuration block 0")                \
    EVENT_ERROR(SAVE_DEV_CONF_BLOCK_1, 121, "Failed to save configuration block 1")                \
    EVENT_ERROR(SAVE_DEV_CONF_BLOCK_2, 122, "Failed to save configuration block 2")                \
    EVENT_ERROR(SAVE_DEV_CONF_BLOCK_3, 123, "Failed to save configuration block 3")                \
    EVENT_ERROR(SAVE_DEV_CONF_BLOCK_4, 124, "Failed to save configuration block 4")                \
    EVENT_ERROR(SAVE_DEV_CONF_BLOCK_5, 125, "Failed to save configuration block 5")                \
    EVENT_ERROR(SAVE_DEV_CONF_BLOCK_6, 126, "Failed to save configuration block 6")                \
    EVENT_ERROR(SAVE_DEV_CONF_BLOCK_7, 127, "Failed to save configuration block 7")                \
    EVENT_ERROR(SAVE_DEV_CONF_BLOCK_8, 128, "Failed to save configuration block 8")                \
    EVENT_ERROR(SAVE_DEV_CONF_BLOCK_9, 129, "Failed to save configuration block 9")                \
    EVENT_ERROR(SAVE_PROFILE_0, 130, "Failed to save profile 0")                                   \
    EVENT_ERROR(SAVE_PROFILE_1, 131, "Failed to save profile 1")                                   \
    EVENT_ERROR(SAVE_PROFILE_2, 132, "Failed to save profile 2")                                   \
    EVENT_ERROR(SAVE_PROFILE_3, 133, "Failed to save profile 3")                                   \
    EVENT_ERROR(SAVE_PROFILE_4, 134, "Failed to save profile 4")                                   \
    EVENT_ERROR(SAVE_PROFILE_5, 135, "Failed to save profile 5")                                   \
    EVENT_ERROR(SAVE_PROFILE_6, 136, "Failed to save profile 6")                                   \
    EVENT_ERROR(SAVE_PROFILE_7, 137, "Failed to save profile 7")                                   \
    EVENT_ERROR(SAVE_PROFILE_8, 138, "Failed to save profile 8")                                   \
    EVENT_ERROR(SAVE_PROFILE_9, 139, "Failed to save profile 9")                                   \
	EVENT_ERROR(SLOT1_SYNC_ERROR, 140, "Sync error on module 1")                                   \
	EVENT_ERROR(SLOT2_SYNC_ERROR, 141, "Sync error on module 2")                                   \
	EVENT_ERROR(SLOT3_SYNC_ERROR, 142, "Sync error on module 3")                                   \
    EVENT_ERROR(TOO_MANY_LOG_EVENTS, 150, "Too many log events")                                   \
	EVENT_ERROR(WATCHDOG_RESET, 151, "Watchdog reset")                                             \
    EVENT_ERROR(HIGH_TEMPERATURE, 152, "High temperature")                                         \
    EVENT_ERROR(SLOT1_FIRMWARE_MISMATCH, 160, "Firmware mismatch on module 1")                     \
	EVENT_ERROR(SLOT2_FIRMWARE_MISMATCH, 161, "Firmware mismatch on module 2")                     \
	EVENT_ERROR(SLOT3_FIRMWARE_MISMATCH, 162, "Firmware mismatch on module 3")                     \
    EVENT_WARNING(CH_CALIBRATION_DISABLED, 0, "Ch%d calibration disabled")                         \
    EVENT_WARNING(ETHERNET_NOT_CONNECTED, 20, "Ethernet not connected")                            \
    EVENT_WARNING(AUTO_RECALL_VALUES_MISMATCH, 21, "Auto-recall values mismatch")                  \
    EVENT_WARNING(NTP_REFRESH_FAILED, 22, "NTP refresh failed")                                    \
    EVENT_WARNING(FILE_UPLOAD_ABORTED, 23, "File upload aborted")                                  \
    EVENT_WARNING(FILE_DOWNLOAD_ABORTED, 24, "File download aborted")                              \
    EVENT_WARNING(AUTO_RECALL_MODULE_MISMATCH, 25, "Auto-recall module mismatch")                  \
    EVENT_INFO(WELCOME, 0, "Welcome!")                                                             \
    EVENT_INFO(POWER_UP, 1, "Power up")                                                            \
    EVENT_INFO(POWER_DOWN, 2, "Power down")                                                        \
    EVENT_INFO(CALIBRATION_PASSWORD_CHANGED, 3, "Calibration password changed")                    \
    EVENT_INFO(SOUND_ENABLED, 4, "Sound enabled")                                                  \
    EVENT_INFO(SOUND_DISABLED, 5, "Sound disabled")                                                \
    EVENT_INFO(SYSTEM_DATE_TIME_CHANGED, 6, "Date/time changed")                                   \
    EVENT_INFO(ETHERNET_ENABLED, 7, "Ethernet enabled")                                            \
    EVENT_INFO(ETHERNET_DISABLED, 8, "Ethernet disabled")                                          \
    EVENT_INFO(SYSTEM_PASSWORD_CHANGED, 9, "System password changed")                              \
    EVENT_INFO(CH_OUTPUT_ENABLED, 10, "Ch%d output on")                                            \
    EVENT_INFO(SYSTEM_DATE_TIME_CHANGED_DST, 20, "Date/time changed (DST)")                        \
    EVENT_INFO(CH_OUTPUT_DISABLED, 30, "Ch%d output off")                                          \
    EVENT_INFO(CH_REMOTE_SENSE_ENABLED, 40, "Ch%d remote sense enabled")                           \
    EVENT_INFO(CH_REMOTE_SENSE_DISABLED, 50, "Ch%d remote sense disabled")                         \
    EVENT_INFO(CH_REMOTE_PROG_ENABLED, 60, "Ch%d remote prog enabled")                             \
    EVENT_INFO(CH_REMOTE_PROG_DISABLED, 70, "Ch%d remote prog disabled")                           \
    EVENT_INFO(RECALL_FROM_PROFILE_0, 80, "Recall from profile 0")                                 \
    EVENT_INFO(RECALL_FROM_PROFILE_1, 81, "Recall from profile 1")                                 \
    EVENT_INFO(RECALL_FROM_PROFILE_2, 82, "Recall from profile 2")                                 \
    EVENT_INFO(RECALL_FROM_PROFILE_3, 83, "Recall from profile 3")                                 \
    EVENT_INFO(RECALL_FROM_PROFILE_4, 84, "Recall from profile 4")                                 \
    EVENT_INFO(RECALL_FROM_PROFILE_5, 85, "Recall from profile 5")                                 \
    EVENT_INFO(RECALL_FROM_PROFILE_6, 86, "Recall from profile 6")                                 \
    EVENT_INFO(RECALL_FROM_PROFILE_7, 87, "Recall from profile 7")                                 \
    EVENT_INFO(RECALL_FROM_PROFILE_8, 88, "Recall from profile 8")                                 \
    EVENT_INFO(RECALL_FROM_PROFILE_9, 89, "Recall from profile 9")                                 \
    EVENT_INFO(DEFAULE_PROFILE_CHANGED_TO_0, 90, "Default profile changed to 0")                   \
    EVENT_INFO(DEFAULE_PROFILE_CHANGED_TO_1, 91, "Default profile changed to 1")                   \
    EVENT_INFO(DEFAULE_PROFILE_CHANGED_TO_2, 92, "Default profile changed to 2")                   \
    EVENT_INFO(DEFAULE_PROFILE_CHANGED_TO_3, 93, "Default profile changed to 3")                   \
    EVENT_INFO(DEFAULE_PROFILE_CHANGED_TO_4, 94, "Default profile changed to 4")                   \
    EVENT_INFO(DEFAULE_PROFILE_CHANGED_TO_5, 95, "Default profile changed to 5")                   \
    EVENT_INFO(DEFAULE_PROFILE_CHANGED_TO_6, 96, "Default profile changed to 6")                   \
    EVENT_INFO(DEFAULE_PROFILE_CHANGED_TO_7, 97, "Default profile changed to 7")                   \
    EVENT_INFO(DEFAULE_PROFILE_CHANGED_TO_8, 98, "Default profile changed to 8")                   \
    EVENT_INFO(DEFAULE_PROFILE_CHANGED_TO_9, 99, "Default profile changed to 9")                   \
    EVENT_INFO(CH_CALIBRATION_ENABLED, 100, "Ch%d calibration enabled")                            \
    EVENT_INFO(COUPLED_IN_PARALLEL, 110, "Coupled in parallel")                                    \
    EVENT_INFO(COUPLED_IN_SERIES, 111, "Coupled in series")                                        \
    EVENT_INFO(CHANNELS_UNCOUPLED, 112, "Channels uncoupled")                                      \
    EVENT_INFO(CHANNELS_TRACKED, 113, "Channels operates in track mode")                           \
    EVENT_INFO(OUTPUT_PROTECTION_COUPLED, 114, "Output protection coupled")                        \
    EVENT_INFO(OUTPUT_PROTECTION_DECOUPLED, 115, "Output protection decoupled")                    \
    EVENT_INFO(SHUTDOWN_WHEN_PROTECTION_TRIPPED_ENABLED, 116, "Shutdown when tripped enabled")     \
    EVENT_INFO(SHUTDOWN_WHEN_PROTECTION_TRIPPED_DISABLED, 117, "Shutdown when tripped disabled")   \
    EVENT_INFO(FORCE_DISABLING_ALL_OUTPUTS_ON_POWERUP_ENABLED, 118, "Force disabling outputs enabled") \
    EVENT_INFO(FORCE_DISABLING_ALL_OUTPUTS_ON_POWERUP_DISABLED, 119, "Force disabling outputs disabled") \
    EVENT_INFO(FRONT_PANEL_LOCKED, 120, "Front panel locked")                                      \
    EVENT_INFO(FRONT_PANEL_UNLOCKED, 121, "Front panel unlocked")                                  \
    EVENT_INFO(RECALL_FROM_FILE, 122, "Recall from file")                                          \
    EVENT_INFO(FILE_UPLOAD_SUCCEEDED, 123, "File upload succeeded")                                \
    EVENT_INFO(FILE_DOWNLOAD_SUCCEEDED, 124, "File download succeeded")                            \
    EVENT_INFO(COUPLED_IN_COMMON_GND, 125, "Coupled in common GND")                                \
    EVENT_INFO(COUPLED_IN_SPLIT_RAILS, 126, "Coupled in split rails")                              \
	EVENT_INFO(SCREENSHOT_SAVED, 127, "A screenshot was saved")                                    \
    EVENT_INFO(SYSTEM_RESTART, 128, "System restart")                                              \
    EVENT_INFO(SYSTEM_SHUTDOWN, 129, "System shutdown")                                            \
    EVENT_INFO(DLOG_START, 130, "DLOG recording started")                                          \
    EVENT_INFO(DLOG_FINISH, 131, "DLOG recording finished")                                        \
    EVENT_INFO(ETHERNET_CONNECTED, 132, "Ethernet connected")                                      \
    EVENT_INFO(NTP_REFRESH_SUCCEEDED, 133, "NTP refresh succeeded")                                \

#define EVENT_ERROR_START_ID 10000
#define EVENT_WARNING_START_ID 12000
#define EVENT_INFO_START_ID 14000

#define EVENT_DEBUG_TRACE 32000
#define EVENT_INFO_TRACE 32001
#define EVENT_ERROR_TRACE 32002

#define EVENT_SCPI_ERROR(ID, TEXT)
#define EVENT_ERROR(NAME, ID, TEXT) EVENT_ERROR_##NAME = EVENT_ERROR_START_ID + ID,
#define EVENT_WARNING(NAME, ID, TEXT) EVENT_WARNING_##NAME = EVENT_WARNING_START_ID + ID,
#define EVENT_INFO(NAME, ID, TEXT) EVENT_INFO_##NAME = EVENT_INFO_START_ID + ID,
enum Events { LIST_OF_EVENTS };
#undef EVENT_SCPI_ERROR
#undef EVENT_INFO
#undef EVENT_WARNING
#undef EVENT_ERROR

////////////////////////////////////////////////////////////////////////////////

struct Event;

void init();
void tick();
void shutdownSave();

int16_t getLastErrorEventId();
int16_t getLastErrorEventChannelIndex();

const char *getEventTypeName(int16_t eventId);
const char *getEventMessage(int16_t eventId);

void setFilter(int filter);

void pushEvent(int16_t eventId, int channelIndex = -1);
void pushChannelEvent(int16_t eventId, int channelIndex);

void pushDebugTrace(const char *message, size_t messageLength);
void pushInfoTrace(const char *message, size_t messageLength);
void pushErrorTrace(const char *message, size_t messageLength);

void markAsRead();
void moveToTop();

void onEncoder(int couter);

} // namespace event_queue
} // namespace psu
} // namespace eez
