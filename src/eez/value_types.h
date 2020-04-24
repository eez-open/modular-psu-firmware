/*
 * EEZ Modular Firmware
 * Copyright (C) 2020-present, Envox d.o.o.
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

#define VALUE_TYPES \
    VALUE_TYPE(NONE) \
    VALUE_TYPE(INT) \
    VALUE_TYPE(UINT8) \
    VALUE_TYPE(UINT16) \
    VALUE_TYPE(UINT32) \
    VALUE_TYPE(FLOAT) \
    VALUE_TYPE(RANGE) \
    VALUE_TYPE(STR) \
    VALUE_TYPE(PASSWORD) \
    VALUE_TYPE(ENUM) \
    VALUE_TYPE(PERCENTAGE) \
    VALUE_TYPE(SIZE) \
    VALUE_TYPE(POINTER) \
    VALUE_TYPE(TIME_SECONDS) \
    VALUE_TYPE(YT_DATA_GET_VALUE_FUNCTION_POINTER) \
    VALUE_TYPE(LESS_THEN_MIN_FLOAT) \
    VALUE_TYPE(GREATER_THEN_MAX_FLOAT) \
    VALUE_TYPE(CHANNEL_LABEL) \
    VALUE_TYPE(CHANNEL_SHORT_LABEL) \
    VALUE_TYPE(CHANNEL_SHORT_LABEL_WITHOUT_COLUMN) \
    VALUE_TYPE(CHANNEL_BOARD_INFO_LABEL) \
    VALUE_TYPE(LESS_THEN_MIN_INT) \
    VALUE_TYPE(LESS_THEN_MIN_TIME_ZONE) \
    VALUE_TYPE(GREATER_THEN_MAX_INT) \
    VALUE_TYPE(GREATER_THEN_MAX_TIME_ZONE) \
    VALUE_TYPE(EVENT) \
    VALUE_TYPE(EVENT_MESSAGE) \
    VALUE_TYPE(ON_TIME_COUNTER) \
    VALUE_TYPE(COUNTDOWN) \
    VALUE_TYPE(TIME_ZONE) \
    VALUE_TYPE(DATE_DMY) \
    VALUE_TYPE(DATE_MDY) \
    VALUE_TYPE(YEAR) \
    VALUE_TYPE(MONTH) \
    VALUE_TYPE(DAY) \
    VALUE_TYPE(TIME) \
    VALUE_TYPE(TIME12) \
    VALUE_TYPE(HOUR) \
    VALUE_TYPE(MINUTE) \
    VALUE_TYPE(SECOND) \
    VALUE_TYPE(USER_PROFILE_LABEL) \
    VALUE_TYPE(USER_PROFILE_REMARK) \
    VALUE_TYPE(EDIT_INFO) \
    VALUE_TYPE(MAC_ADDRESS) \
    VALUE_TYPE(IP_ADDRESS) \
    VALUE_TYPE(PORT) \
    VALUE_TYPE(TEXT_MESSAGE) \
    VALUE_TYPE(STEP_VALUES) \
    VALUE_TYPE(FLOAT_LIST) \
    VALUE_TYPE(CHANNEL_TITLE) \
    VALUE_TYPE(CHANNEL_SHORT_TITLE) \
    VALUE_TYPE(CHANNEL_SHORT_TITLE_WITHOUT_TRACKING_ICON) \
    VALUE_TYPE(CHANNEL_SHORT_TITLE_WITH_COLON) \
    VALUE_TYPE(CHANNEL_LONG_TITLE) \
    VALUE_TYPE(DLOG_VALUE_LABEL) \
    VALUE_TYPE(DLOG_CURRENT_TIME) \
    VALUE_TYPE(FILE_LENGTH) \
    VALUE_TYPE(FILE_DATE_TIME) \
    VALUE_TYPE(FIRMWARE_VERSION) \
    VALUE_TYPE(SLOT_INFO) \
    VALUE_TYPE(SLOT_INFO2) \
    VALUE_TYPE(MASTER_INFO) \
    VALUE_TYPE(TEST_RESULT) \
    VALUE_TYPE(SCPI_ERROR) \
    VALUE_TYPE(STORAGE_INFO) \
    VALUE_TYPE(FOLDER_INFO) \
    VALUE_TYPE(CHANNEL_INFO_SERIAL) \
    VALUE_TYPE(DEBUG_VARIABLE) \
    VALUE_TYPE(CALIBRATION_POINT_INFO)

namespace eez {

#define VALUE_TYPE(NAME) VALUE_TYPE_##NAME,
enum ValueTypes {
	VALUE_TYPES
};
#undef VALUE_TYPE

}