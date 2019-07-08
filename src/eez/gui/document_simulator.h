enum DataEnum {
    DATA_ID_NONE = 0,
    DATA_ID_EDIT_ENABLED = 1,
    DATA_ID_CHANNELS = 2,
    DATA_ID_CHANNEL_STATUS = 3,
    DATA_ID_CHANNEL_OUTPUT_STATE = 4,
    DATA_ID_CHANNEL_OUTPUT_MODE = 5,
    DATA_ID_CHANNEL_IS_CC = 6,
    DATA_ID_CHANNEL_IS_CV = 7,
    DATA_ID_CHANNEL_MON_VALUE = 8,
    DATA_ID_CHANNEL_U_SET = 9,
    DATA_ID_CHANNEL_U_MON = 10,
    DATA_ID_CHANNEL_U_MON_DAC = 11,
    DATA_ID_CHANNEL_U_LIMIT = 12,
    DATA_ID_CHANNEL_U_EDIT = 13,
    DATA_ID_CHANNEL_I_SET = 14,
    DATA_ID_CHANNEL_I_MON = 15,
    DATA_ID_CHANNEL_I_MON_DAC = 16,
    DATA_ID_CHANNEL_I_LIMIT = 17,
    DATA_ID_CHANNEL_I_EDIT = 18,
    DATA_ID_CHANNEL_P_MON = 19,
    DATA_ID_CHANNELS_VIEW_MODE = 20,
    DATA_ID_CHANNEL_DISPLAY_VALUE1 = 21,
    DATA_ID_CHANNEL_DISPLAY_VALUE2 = 22,
    DATA_ID_OVP = 23,
    DATA_ID_OCP = 24,
    DATA_ID_OPP = 25,
    DATA_ID_OTP_CH = 26,
    DATA_ID_OTP_AUX = 27,
    DATA_ID_ALERT_MESSAGE = 28,
    DATA_ID_ALERT_MESSAGE_2 = 29,
    DATA_ID_ALERT_MESSAGE_3 = 30,
    DATA_ID_EDIT_VALUE = 31,
    DATA_ID_EDIT_UNIT = 32,
    DATA_ID_EDIT_INFO = 33,
    DATA_ID_EDIT_INFO1 = 34,
    DATA_ID_EDIT_INFO2 = 35,
    DATA_ID_EDIT_MODE_INTERACTIVE_MODE_SELECTOR = 36,
    DATA_ID_EDIT_STEPS = 37,
    DATA_ID_MODEL_INFO = 38,
    DATA_ID_FIRMWARE_INFO = 39,
    DATA_ID_SELF_TEST_RESULT = 40,
    DATA_ID_KEYPAD_TEXT = 41,
    DATA_ID_KEYPAD_CAPS = 42,
    DATA_ID_KEYPAD_OPTION1_TEXT = 43,
    DATA_ID_KEYPAD_OPTION1_ENABLED = 44,
    DATA_ID_KEYPAD_OPTION2_TEXT = 45,
    DATA_ID_KEYPAD_OPTION2_ENABLED = 46,
    DATA_ID_KEYPAD_SIGN_ENABLED = 47,
    DATA_ID_KEYPAD_DOT_ENABLED = 48,
    DATA_ID_KEYPAD_UNIT_ENABLED = 49,
    DATA_ID_CHANNEL_LABEL = 50,
    DATA_ID_CHANNEL_SHORT_LABEL = 51,
    DATA_ID_CHANNEL_TEMP_STATUS = 52,
    DATA_ID_CHANNEL_TEMP = 53,
    DATA_ID_CHANNEL_ON_TIME_TOTAL = 54,
    DATA_ID_CHANNEL_ON_TIME_LAST = 55,
    DATA_ID_CHANNEL_CALIBRATION_STATUS = 56,
    DATA_ID_CHANNEL_CALIBRATION_STATE = 57,
    DATA_ID_CHANNEL_CALIBRATION_DATE = 58,
    DATA_ID_CHANNEL_CALIBRATION_REMARK = 59,
    DATA_ID_CHANNEL_CALIBRATION_STEP_IS_SET_REMARK_STEP = 60,
    DATA_ID_CHANNEL_CALIBRATION_STEP_NUM = 61,
    DATA_ID_CHANNEL_CALIBRATION_STEP_STATUS = 62,
    DATA_ID_CHANNEL_CALIBRATION_STEP_LEVEL_VALUE = 63,
    DATA_ID_CHANNEL_CALIBRATION_STEP_VALUE = 64,
    DATA_ID_CHANNEL_CALIBRATION_STEP_PREV_ENABLED = 65,
    DATA_ID_CHANNEL_CALIBRATION_STEP_NEXT_ENABLED = 66,
    DATA_ID_CAL_CH_U_MIN = 67,
    DATA_ID_CAL_CH_U_MID = 68,
    DATA_ID_CAL_CH_U_MAX = 69,
    DATA_ID_CAL_CH_I0_MIN = 70,
    DATA_ID_CAL_CH_I0_MID = 71,
    DATA_ID_CAL_CH_I0_MAX = 72,
    DATA_ID_CAL_CH_I1_MIN = 73,
    DATA_ID_CAL_CH_I1_MID = 74,
    DATA_ID_CAL_CH_I1_MAX = 75,
    DATA_ID_CHANNEL_PROTECTION_OVP_STATE = 76,
    DATA_ID_CHANNEL_PROTECTION_OVP_LEVEL = 77,
    DATA_ID_CHANNEL_PROTECTION_OVP_DELAY = 78,
    DATA_ID_CHANNEL_PROTECTION_OVP_LIMIT = 79,
    DATA_ID_CHANNEL_PROTECTION_OCP_STATE = 80,
    DATA_ID_CHANNEL_PROTECTION_OCP_DELAY = 81,
    DATA_ID_CHANNEL_PROTECTION_OCP_LIMIT = 82,
    DATA_ID_CHANNEL_PROTECTION_OCP_MAX_CURRENT_LIMIT_CAUSE = 83,
    DATA_ID_CHANNEL_PROTECTION_OPP_STATE = 84,
    DATA_ID_CHANNEL_PROTECTION_OPP_LEVEL = 85,
    DATA_ID_CHANNEL_PROTECTION_OPP_DELAY = 86,
    DATA_ID_CHANNEL_PROTECTION_OPP_LIMIT = 87,
    DATA_ID_CHANNEL_PROTECTION_OTP_INSTALLED = 88,
    DATA_ID_CHANNEL_PROTECTION_OTP_STATE = 89,
    DATA_ID_CHANNEL_PROTECTION_OTP_LEVEL = 90,
    DATA_ID_CHANNEL_PROTECTION_OTP_DELAY = 91,
    DATA_ID_EVENT_QUEUE_LAST_EVENT_TYPE = 92,
    DATA_ID_EVENT_QUEUE_LAST_EVENT_MESSAGE = 93,
    DATA_ID_EVENT_QUEUE_EVENTS = 94,
    DATA_ID_EVENT_QUEUE_EVENTS_TYPE = 95,
    DATA_ID_EVENT_QUEUE_EVENTS_MESSAGE = 96,
    DATA_ID_EVENT_QUEUE_MULTIPLE_PAGES = 97,
    DATA_ID_EVENT_QUEUE_PREVIOUS_PAGE_ENABLED = 98,
    DATA_ID_EVENT_QUEUE_NEXT_PAGE_ENABLED = 99,
    DATA_ID_EVENT_QUEUE_PAGE_INFO = 100,
    DATA_ID_CHANNEL_RSENSE_STATUS = 101,
    DATA_ID_CHANNEL_RPROG_INSTALLED = 102,
    DATA_ID_CHANNEL_RPROG_STATUS = 103,
    DATA_ID_CHANNEL_IS_COUPLED = 104,
    DATA_ID_CHANNEL_IS_TRACKED = 105,
    DATA_ID_CHANNEL_IS_COUPLED_OR_TRACKED = 106,
    DATA_ID_CHANNEL_COUPLING_IS_ALLOWED = 107,
    DATA_ID_CHANNEL_COUPLING_MODE = 108,
    DATA_ID_CHANNEL_COUPLING_SELECTED_MODE = 109,
    DATA_ID_CHANNEL_COUPLING_IS_SERIES = 110,
    DATA_ID_SYS_ON_TIME_TOTAL = 111,
    DATA_ID_SYS_ON_TIME_LAST = 112,
    DATA_ID_SYS_TEMP_AUX_STATUS = 113,
    DATA_ID_SYS_TEMP_AUX_OTP_STATE = 114,
    DATA_ID_SYS_TEMP_AUX_OTP_LEVEL = 115,
    DATA_ID_SYS_TEMP_AUX_OTP_DELAY = 116,
    DATA_ID_SYS_TEMP_AUX_OTP_IS_TRIPPED = 117,
    DATA_ID_SYS_TEMP_AUX = 118,
    DATA_ID_SYS_INFO_FIRMWARE_VER = 119,
    DATA_ID_SYS_INFO_SERIAL_NO = 120,
    DATA_ID_SYS_INFO_SCPI_VER = 121,
    DATA_ID_SYS_INFO_CPU = 122,
    DATA_ID_SYS_INFO_ETHERNET = 123,
    DATA_ID_SYS_INFO_FAN_STATUS = 124,
    DATA_ID_SYS_INFO_FAN_SPEED = 125,
    DATA_ID_CHANNEL_BOARD_INFO_LABEL = 126,
    DATA_ID_CHANNEL_BOARD_INFO_REVISION = 127,
    DATA_ID_DATE_TIME_DATE = 128,
    DATA_ID_DATE_TIME_YEAR = 129,
    DATA_ID_DATE_TIME_MONTH = 130,
    DATA_ID_DATE_TIME_DAY = 131,
    DATA_ID_DATE_TIME_TIME = 132,
    DATA_ID_DATE_TIME_HOUR = 133,
    DATA_ID_DATE_TIME_MINUTE = 134,
    DATA_ID_DATE_TIME_SECOND = 135,
    DATA_ID_DATE_TIME_TIME_ZONE = 136,
    DATA_ID_DATE_TIME_DST = 137,
    DATA_ID_SET_PAGE_DIRTY = 138,
    DATA_ID_PROFILES_LIST1 = 139,
    DATA_ID_PROFILES_LIST2 = 140,
    DATA_ID_PROFILES_AUTO_RECALL_STATUS = 141,
    DATA_ID_PROFILES_AUTO_RECALL_LOCATION = 142,
    DATA_ID_PROFILE_STATUS = 143,
    DATA_ID_PROFILE_LABEL = 144,
    DATA_ID_PROFILE_REMARK = 145,
    DATA_ID_PROFILE_IS_AUTO_RECALL_LOCATION = 146,
    DATA_ID_PROFILE_CHANNEL_U_SET = 147,
    DATA_ID_PROFILE_CHANNEL_I_SET = 148,
    DATA_ID_PROFILE_CHANNEL_OUTPUT_STATE = 149,
    DATA_ID_ETHERNET_INSTALLED = 150,
    DATA_ID_ETHERNET_ENABLED = 151,
    DATA_ID_ETHERNET_STATUS = 152,
    DATA_ID_ETHERNET_IP_ADDRESS = 153,
    DATA_ID_ETHERNET_DNS = 154,
    DATA_ID_ETHERNET_GATEWAY = 155,
    DATA_ID_ETHERNET_SUBNET_MASK = 156,
    DATA_ID_ETHERNET_SCPI_PORT = 157,
    DATA_ID_ETHERNET_IS_CONNECTED = 158,
    DATA_ID_ETHERNET_DHCP = 159,
    DATA_ID_ETHERNET_MAC = 160,
    DATA_ID_CHANNEL_IS_VOLTAGE_BALANCED = 161,
    DATA_ID_CHANNEL_IS_CURRENT_BALANCED = 162,
    DATA_ID_SYS_OUTPUT_PROTECTION_COUPLED = 163,
    DATA_ID_SYS_SHUTDOWN_WHEN_PROTECTION_TRIPPED = 164,
    DATA_ID_SYS_FORCE_DISABLING_ALL_OUTPUTS_ON_POWER_UP = 165,
    DATA_ID_SYS_PASSWORD_IS_SET = 166,
    DATA_ID_SYS_RL_STATE = 167,
    DATA_ID_SYS_SOUND_IS_ENABLED = 168,
    DATA_ID_SYS_SOUND_IS_CLICK_ENABLED = 169,
    DATA_ID_CHANNEL_DISPLAY_VIEW_SETTINGS_DISPLAY_VALUE1 = 170,
    DATA_ID_CHANNEL_DISPLAY_VIEW_SETTINGS_DISPLAY_VALUE2 = 171,
    DATA_ID_CHANNEL_DISPLAY_VIEW_SETTINGS_YT_VIEW_RATE = 172,
    DATA_ID_SYS_ENCODER_CONFIRMATION_MODE = 173,
    DATA_ID_SYS_ENCODER_MOVING_UP_SPEED = 174,
    DATA_ID_SYS_ENCODER_MOVING_DOWN_SPEED = 175,
    DATA_ID_SYS_ENCODER_INSTALLED = 176,
    DATA_ID_SYS_DISPLAY_STATE = 177,
    DATA_ID_SYS_DISPLAY_BRIGHTNESS = 178,
    DATA_ID_CHANNEL_TRIGGER_MODE = 179,
    DATA_ID_CHANNEL_TRIGGER_OUTPUT_STATE = 180,
    DATA_ID_CHANNEL_TRIGGER_ON_LIST_STOP = 181,
    DATA_ID_CHANNEL_U_TRIGGER_VALUE = 182,
    DATA_ID_CHANNEL_I_TRIGGER_VALUE = 183,
    DATA_ID_CHANNEL_LIST_COUNT = 184,
    DATA_ID_CHANNEL_LISTS = 185,
    DATA_ID_CHANNEL_LIST_INDEX = 186,
    DATA_ID_CHANNEL_LIST_DWELL = 187,
    DATA_ID_CHANNEL_LIST_DWELL_ENABLED = 188,
    DATA_ID_CHANNEL_LIST_VOLTAGE = 189,
    DATA_ID_CHANNEL_LIST_VOLTAGE_ENABLED = 190,
    DATA_ID_CHANNEL_LIST_CURRENT = 191,
    DATA_ID_CHANNEL_LIST_CURRENT_ENABLED = 192,
    DATA_ID_CHANNEL_LISTS_PREVIOUS_PAGE_ENABLED = 193,
    DATA_ID_CHANNEL_LISTS_NEXT_PAGE_ENABLED = 194,
    DATA_ID_CHANNEL_LISTS_CURSOR = 195,
    DATA_ID_CHANNEL_LISTS_INSERT_MENU_ENABLED = 196,
    DATA_ID_CHANNEL_LISTS_DELETE_MENU_ENABLED = 197,
    DATA_ID_CHANNEL_LISTS_DELETE_ROW_ENABLED = 198,
    DATA_ID_CHANNEL_LISTS_CLEAR_COLUMN_ENABLED = 199,
    DATA_ID_CHANNEL_LISTS_DELETE_ROWS_ENABLED = 200,
    DATA_ID_TRIGGER_SOURCE = 201,
    DATA_ID_TRIGGER_DELAY = 202,
    DATA_ID_TRIGGER_INITIATE_CONTINUOUSLY = 203,
    DATA_ID_TRIGGER_IS_INITIATED = 204,
    DATA_ID_TRIGGER_IS_MANUAL = 205,
    DATA_ID_CHANNEL_HAS_SUPPORT_FOR_CURRENT_DUAL_RANGE = 206,
    DATA_ID_CHANNEL_RANGES_SUPPORTED = 207,
    DATA_ID_CHANNEL_RANGES_MODE = 208,
    DATA_ID_CHANNEL_RANGES_AUTO_RANGING = 209,
    DATA_ID_CHANNEL_RANGES_CURRENTLY_SELECTED = 210,
    DATA_ID_TEXT_MESSAGE = 211,
    DATA_ID_SERIAL_STATUS = 212,
    DATA_ID_SERIAL_ENABLED = 213,
    DATA_ID_SERIAL_IS_CONNECTED = 214,
    DATA_ID_SERIAL_BAUD = 215,
    DATA_ID_SERIAL_PARITY = 216,
    DATA_ID_CHANNEL_LIST_COUNTDOWN = 217,
    DATA_ID_IO_PINS = 218,
    DATA_ID_IO_PINS_INHIBIT_STATE = 219,
    DATA_ID_IO_PIN_NUMBER = 220,
    DATA_ID_IO_PIN_POLARITY = 221,
    DATA_ID_IO_PIN_FUNCTION = 222,
    DATA_ID_NTP_ENABLED = 223,
    DATA_ID_NTP_SERVER = 224,
    DATA_ID_ASYNC_OPERATION_THROBBER = 225,
    DATA_ID_SYS_DISPLAY_BACKGROUND_LUMINOSITY_STEP = 226,
    DATA_ID_PROGRESS = 227,
    DATA_ID_VIEW_STATUS = 228,
    DATA_ID_DLOG_STATUS = 229,
    DATA_ID_MAIN_APP_VIEW = 230,
    DATA_ID_APPLICATIONS = 231,
    DATA_ID_APPLICATION_TITLE = 232,
    DATA_ID_APPLICATION_ICON = 233,
    DATA_ID_IS_HOME_APPLICATION_IN_FOREGROUND = 234,
    DATA_ID_FOREGROUND_APPLICATION = 235,
    DATA_ID_FOREGROUND_APPLICATION_TITLE = 236,
    DATA_ID_TOUCH_CALIBRATION_POINT = 237,
    DATA_ID_SLOTS = 238,
    DATA_ID_SLOT_MODULE_TYPE = 239,
    DATA_ID_SIMULATOR_LOAD_STATE = 240,
    DATA_ID_SIMULATOR_LOAD = 241,
    DATA_ID_SELECTED_THEME = 242
};

void data_edit_enabled(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_channels(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_channel_status(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_channel_output_state(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_channel_output_mode(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_channel_is_cc(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_channel_is_cv(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_channel_mon_value(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_channel_u_set(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_channel_u_mon(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_channel_u_mon_dac(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_channel_u_limit(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_channel_u_edit(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_channel_i_set(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_channel_i_mon(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_channel_i_mon_dac(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_channel_i_limit(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_channel_i_edit(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_channel_p_mon(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_channels_view_mode(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_channel_display_value1(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_channel_display_value2(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_ovp(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_ocp(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_opp(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_otp_ch(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_otp_aux(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_alert_message(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_alert_message_2(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_alert_message_3(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_edit_value(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_edit_unit(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_edit_info(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_edit_info1(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_edit_info2(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_edit_mode_interactive_mode_selector(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_edit_steps(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_model_info(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_firmware_info(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_self_test_result(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_keypad_text(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_keypad_caps(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_keypad_option1_text(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_keypad_option1_enabled(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_keypad_option2_text(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_keypad_option2_enabled(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_keypad_sign_enabled(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_keypad_dot_enabled(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_keypad_unit_enabled(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_channel_label(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_channel_short_label(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_channel_temp_status(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_channel_temp(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_channel_on_time_total(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_channel_on_time_last(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_channel_calibration_status(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_channel_calibration_state(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_channel_calibration_date(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_channel_calibration_remark(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_channel_calibration_step_is_set_remark_step(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_channel_calibration_step_num(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_channel_calibration_step_status(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_channel_calibration_step_level_value(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_channel_calibration_step_value(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_channel_calibration_step_prev_enabled(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_channel_calibration_step_next_enabled(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_cal_ch_u_min(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_cal_ch_u_mid(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_cal_ch_u_max(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_cal_ch_i0_min(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_cal_ch_i0_mid(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_cal_ch_i0_max(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_cal_ch_i1_min(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_cal_ch_i1_mid(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_cal_ch_i1_max(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_channel_protection_ovp_state(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_channel_protection_ovp_level(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_channel_protection_ovp_delay(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_channel_protection_ovp_limit(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_channel_protection_ocp_state(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_channel_protection_ocp_delay(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_channel_protection_ocp_limit(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_channel_protection_ocp_max_current_limit_cause(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_channel_protection_opp_state(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_channel_protection_opp_level(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_channel_protection_opp_delay(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_channel_protection_opp_limit(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_channel_protection_otp_installed(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_channel_protection_otp_state(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_channel_protection_otp_level(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_channel_protection_otp_delay(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_event_queue_last_event_type(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_event_queue_last_event_message(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_event_queue_events(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_event_queue_events_type(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_event_queue_events_message(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_event_queue_multiple_pages(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_event_queue_previous_page_enabled(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_event_queue_next_page_enabled(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_event_queue_page_info(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_channel_rsense_status(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_channel_rprog_installed(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_channel_rprog_status(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_channel_is_coupled(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_channel_is_tracked(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_channel_is_coupled_or_tracked(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_channel_coupling_is_allowed(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_channel_coupling_mode(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_channel_coupling_selected_mode(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_channel_coupling_is_series(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_sys_on_time_total(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_sys_on_time_last(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_sys_temp_aux_status(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_sys_temp_aux_otp_state(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_sys_temp_aux_otp_level(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_sys_temp_aux_otp_delay(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_sys_temp_aux_otp_is_tripped(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_sys_temp_aux(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_sys_info_firmware_ver(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_sys_info_serial_no(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_sys_info_scpi_ver(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_sys_info_cpu(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_sys_info_ethernet(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_sys_info_fan_status(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_sys_info_fan_speed(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_channel_board_info_label(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_channel_board_info_revision(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_date_time_date(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_date_time_year(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_date_time_month(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_date_time_day(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_date_time_time(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_date_time_hour(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_date_time_minute(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_date_time_second(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_date_time_time_zone(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_date_time_dst(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_set_page_dirty(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_profiles_list1(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_profiles_list2(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_profiles_auto_recall_status(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_profiles_auto_recall_location(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_profile_status(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_profile_label(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_profile_remark(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_profile_is_auto_recall_location(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_profile_channel_u_set(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_profile_channel_i_set(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_profile_channel_output_state(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_ethernet_installed(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_ethernet_enabled(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_ethernet_status(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_ethernet_ip_address(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_ethernet_dns(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_ethernet_gateway(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_ethernet_subnet_mask(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_ethernet_scpi_port(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_ethernet_is_connected(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_ethernet_dhcp(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_ethernet_mac(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_channel_is_voltage_balanced(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_channel_is_current_balanced(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_sys_output_protection_coupled(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_sys_shutdown_when_protection_tripped(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_sys_force_disabling_all_outputs_on_power_up(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_sys_password_is_set(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_sys_rl_state(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_sys_sound_is_enabled(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_sys_sound_is_click_enabled(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_channel_display_view_settings_display_value1(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_channel_display_view_settings_display_value2(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_channel_display_view_settings_yt_view_rate(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_sys_encoder_confirmation_mode(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_sys_encoder_moving_up_speed(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_sys_encoder_moving_down_speed(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_sys_encoder_installed(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_sys_display_state(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_sys_display_brightness(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_channel_trigger_mode(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_channel_trigger_output_state(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_channel_trigger_on_list_stop(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_channel_u_trigger_value(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_channel_i_trigger_value(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_channel_list_count(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_channel_lists(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_channel_list_index(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_channel_list_dwell(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_channel_list_dwell_enabled(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_channel_list_voltage(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_channel_list_voltage_enabled(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_channel_list_current(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_channel_list_current_enabled(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_channel_lists_previous_page_enabled(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_channel_lists_next_page_enabled(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_channel_lists_cursor(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_channel_lists_insert_menu_enabled(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_channel_lists_delete_menu_enabled(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_channel_lists_delete_row_enabled(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_channel_lists_clear_column_enabled(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_channel_lists_delete_rows_enabled(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_trigger_source(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_trigger_delay(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_trigger_initiate_continuously(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_trigger_is_initiated(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_trigger_is_manual(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_channel_has_support_for_current_dual_range(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_channel_ranges_supported(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_channel_ranges_mode(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_channel_ranges_auto_ranging(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_channel_ranges_currently_selected(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_text_message(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_serial_status(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_serial_enabled(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_serial_is_connected(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_serial_baud(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_serial_parity(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_channel_list_countdown(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_io_pins(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_io_pins_inhibit_state(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_io_pin_number(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_io_pin_polarity(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_io_pin_function(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_ntp_enabled(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_ntp_server(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_async_operation_throbber(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_sys_display_background_luminosity_step(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_progress(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_view_status(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_dlog_status(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_main_app_view(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_applications(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_application_title(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_application_icon(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_is_home_application_in_foreground(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_foreground_application(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_foreground_application_title(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_touch_calibration_point(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_slots(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_slot_module_type(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_simulator_load_state(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_simulator_load(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_selected_theme(DataOperationEnum operation, Cursor &cursor, Value &value);

typedef void (*DataOperationsFunction)(DataOperationEnum operation, Cursor &cursor, Value &value);

extern DataOperationsFunction g_dataOperationsFunctions[];

enum ActionsEnum {
    ACTION_ID_NONE = 0,
    ACTION_ID_CHANNEL_TOGGLE_OUTPUT = 1,
    ACTION_ID_EDIT = 2,
    ACTION_ID_EDIT_MODE_SLIDER = 3,
    ACTION_ID_EDIT_MODE_STEP = 4,
    ACTION_ID_EDIT_MODE_KEYPAD = 5,
    ACTION_ID_EXIT_EDIT_MODE = 6,
    ACTION_ID_TOGGLE_INTERACTIVE_MODE = 7,
    ACTION_ID_NON_INTERACTIVE_ENTER = 8,
    ACTION_ID_NON_INTERACTIVE_DISCARD = 9,
    ACTION_ID_KEYPAD_KEY = 10,
    ACTION_ID_KEYPAD_SPACE = 11,
    ACTION_ID_KEYPAD_BACK = 12,
    ACTION_ID_KEYPAD_CLEAR = 13,
    ACTION_ID_KEYPAD_CAPS = 14,
    ACTION_ID_KEYPAD_OK = 15,
    ACTION_ID_KEYPAD_CANCEL = 16,
    ACTION_ID_KEYPAD_SIGN = 17,
    ACTION_ID_KEYPAD_UNIT = 18,
    ACTION_ID_KEYPAD_OPTION1 = 19,
    ACTION_ID_KEYPAD_OPTION2 = 20,
    ACTION_ID_ENTER_TOUCH_CALIBRATION = 21,
    ACTION_ID_YES = 22,
    ACTION_ID_NO = 23,
    ACTION_ID_OK = 24,
    ACTION_ID_CANCEL = 25,
    ACTION_ID_LATER = 26,
    ACTION_ID_STAND_BY = 27,
    ACTION_ID_SHOW_PREVIOUS_PAGE = 28,
    ACTION_ID_SHOW_MAIN_PAGE = 29,
    ACTION_ID_SHOW_EVENT_QUEUE = 30,
    ACTION_ID_SHOW_CHANNEL_SETTINGS = 31,
    ACTION_ID_SHOW_SYS_SETTINGS = 32,
    ACTION_ID_SHOW_SYS_SETTINGS_TRIGGER = 33,
    ACTION_ID_SHOW_SYS_SETTINGS_IO = 34,
    ACTION_ID_SHOW_SYS_SETTINGS_DATE_TIME = 35,
    ACTION_ID_SHOW_SYS_SETTINGS_CAL = 36,
    ACTION_ID_SHOW_SYS_SETTINGS_CAL_CH = 37,
    ACTION_ID_SHOW_SYS_SETTINGS_SCREEN_CALIBRATION = 38,
    ACTION_ID_SHOW_SYS_SETTINGS_DISPLAY = 39,
    ACTION_ID_SHOW_SYS_SETTINGS_SERIAL = 40,
    ACTION_ID_SHOW_SYS_SETTINGS_ETHERNET = 41,
    ACTION_ID_SHOW_SYS_SETTINGS_PROTECTIONS = 42,
    ACTION_ID_SHOW_SYS_SETTINGS_AUX_OTP = 43,
    ACTION_ID_SHOW_SYS_SETTINGS_SOUND = 44,
    ACTION_ID_SHOW_SYS_SETTINGS_ENCODER = 45,
    ACTION_ID_SHOW_SYS_INFO = 46,
    ACTION_ID_SHOW_SYS_INFO2 = 47,
    ACTION_ID_SHOW_MAIN_HELP_PAGE = 48,
    ACTION_ID_SHOW_EDIT_MODE_STEP_HELP = 49,
    ACTION_ID_SHOW_EDIT_MODE_SLIDER_HELP = 50,
    ACTION_ID_SHOW_CH_SETTINGS_PROT = 51,
    ACTION_ID_SHOW_CH_SETTINGS_PROT_CLEAR = 52,
    ACTION_ID_SHOW_CH_SETTINGS_PROT_OCP = 53,
    ACTION_ID_SHOW_CH_SETTINGS_PROT_OVP = 54,
    ACTION_ID_SHOW_CH_SETTINGS_PROT_OPP = 55,
    ACTION_ID_SHOW_CH_SETTINGS_PROT_OTP = 56,
    ACTION_ID_SHOW_CH_SETTINGS_TRIGGER = 57,
    ACTION_ID_SHOW_CH_SETTINGS_LISTS = 58,
    ACTION_ID_SHOW_CH_SETTINGS_ADV = 59,
    ACTION_ID_SHOW_CH_SETTINGS_ADV_REMOTE = 60,
    ACTION_ID_SHOW_CH_SETTINGS_ADV_RANGES = 61,
    ACTION_ID_SHOW_CH_SETTINGS_ADV_TRACKING = 62,
    ACTION_ID_SHOW_CH_SETTINGS_ADV_COUPLING = 63,
    ACTION_ID_SHOW_CH_SETTINGS_ADV_VIEW = 64,
    ACTION_ID_SHOW_CH_SETTINGS_INFO = 65,
    ACTION_ID_SHOW_CH_SETTINGS_INFO_CAL = 66,
    ACTION_ID_SYS_SETTINGS_CAL_EDIT_PASSWORD = 67,
    ACTION_ID_SYS_SETTINGS_CAL_CH_WIZ_START = 68,
    ACTION_ID_SYS_SETTINGS_CAL_CH_WIZ_STEP_PREVIOUS = 69,
    ACTION_ID_SYS_SETTINGS_CAL_CH_WIZ_STEP_NEXT = 70,
    ACTION_ID_SYS_SETTINGS_CAL_CH_WIZ_STOP_AND_SHOW_PREVIOUS_PAGE = 71,
    ACTION_ID_SYS_SETTINGS_CAL_CH_WIZ_STOP_AND_SHOW_MAIN_PAGE = 72,
    ACTION_ID_SYS_SETTINGS_CAL_CH_WIZ_STEP_SET = 73,
    ACTION_ID_SYS_SETTINGS_CAL_CH_WIZ_STEP_SET_LEVEL_VALUE = 74,
    ACTION_ID_SYS_SETTINGS_CAL_CH_WIZ_SAVE = 75,
    ACTION_ID_SYS_SETTINGS_CAL_TOGGLE_ENABLE = 76,
    ACTION_ID_CH_SETTINGS_PROT_CLEAR = 77,
    ACTION_ID_CH_SETTINGS_PROT_CLEAR_AND_DISABLE = 78,
    ACTION_ID_CH_SETTINGS_PROT_TOGGLE_STATE = 79,
    ACTION_ID_CH_SETTINGS_PROT_EDIT_LIMIT = 80,
    ACTION_ID_CH_SETTINGS_PROT_EDIT_LEVEL = 81,
    ACTION_ID_CH_SETTINGS_PROT_EDIT_DELAY = 82,
    ACTION_ID_SET = 83,
    ACTION_ID_DISCARD = 84,
    ACTION_ID_EDIT_FIELD = 85,
    ACTION_ID_EVENT_QUEUE_PREVIOUS_PAGE = 86,
    ACTION_ID_EVENT_QUEUE_NEXT_PAGE = 87,
    ACTION_ID_CH_SETTINGS_ADV_REMOTE_TOGGLE_SENSE = 88,
    ACTION_ID_CH_SETTINGS_ADV_REMOTE_TOGGLE_PROGRAMMING = 89,
    ACTION_ID_DATE_TIME_SELECT_DST_RULE = 90,
    ACTION_ID_SHOW_USER_PROFILES = 91,
    ACTION_ID_SHOW_USER_PROFILES2 = 92,
    ACTION_ID_SHOW_USER_PROFILE_SETTINGS = 93,
    ACTION_ID_PROFILES_TOGGLE_AUTO_RECALL = 94,
    ACTION_ID_PROFILE_TOGGLE_IS_AUTO_RECALL_LOCATION = 95,
    ACTION_ID_PROFILE_RECALL = 96,
    ACTION_ID_PROFILE_SAVE = 97,
    ACTION_ID_PROFILE_DELETE = 98,
    ACTION_ID_PROFILE_EDIT_REMARK = 99,
    ACTION_ID_TOGGLE_CHANNELS_VIEW_MODE = 100,
    ACTION_ID_ETHERNET_TOGGLE = 101,
    ACTION_ID_ETHERNET_TOGGLE_DHCP = 102,
    ACTION_ID_ETHERNET_EDIT_MAC_ADDRESS = 103,
    ACTION_ID_ETHERNET_EDIT_STATIC_ADDRESS = 104,
    ACTION_ID_ETHERNET_EDIT_IP_ADDRESS = 105,
    ACTION_ID_ETHERNET_EDIT_DNS = 106,
    ACTION_ID_ETHERNET_EDIT_GATEWAY = 107,
    ACTION_ID_ETHERNET_EDIT_SUBNET_MASK = 108,
    ACTION_ID_ETHERNET_EDIT_SCPI_PORT = 109,
    ACTION_ID_CH_SETTINGS_ADV_COUPLING_UNCOUPLE = 110,
    ACTION_ID_CH_SETTINGS_ADV_COUPLING_SET_PARALLEL_INFO = 111,
    ACTION_ID_CH_SETTINGS_ADV_COUPLING_SET_SERIES_INFO = 112,
    ACTION_ID_CH_SETTINGS_ADV_COUPLING_SET_PARALLEL = 113,
    ACTION_ID_CH_SETTINGS_ADV_COUPLING_SET_SERIES = 114,
    ACTION_ID_CH_SETTINGS_ADV_TOGGLE_TRACKING_MODE = 115,
    ACTION_ID_SYS_SETTINGS_PROTECTIONS_TOGGLE_OUTPUT_PROTECTION_COUPLE = 116,
    ACTION_ID_SYS_SETTINGS_PROTECTIONS_TOGGLE_SHUTDOWN_WHEN_PROTECTION_TRIPPED = 117,
    ACTION_ID_SYS_SETTINGS_PROTECTIONS_TOGGLE_FORCE_DISABLING_ALL_OUTPUTS_ON_POWER_UP = 118,
    ACTION_ID_SYS_SETTINGS_PROTECTIONS_AUX_OTP_TOGGLE_STATE = 119,
    ACTION_ID_SYS_SETTINGS_PROTECTIONS_AUX_OTP_EDIT_LEVEL = 120,
    ACTION_ID_SYS_SETTINGS_PROTECTIONS_AUX_OTP_EDIT_DELAY = 121,
    ACTION_ID_SYS_SETTINGS_PROTECTIONS_AUX_OTP_CLEAR = 122,
    ACTION_ID_ON_LAST_ERROR_EVENT_ACTION = 123,
    ACTION_ID_EDIT_SYSTEM_PASSWORD = 124,
    ACTION_ID_SYS_FRONT_PANEL_LOCK = 125,
    ACTION_ID_SYS_FRONT_PANEL_UNLOCK = 126,
    ACTION_ID_SYS_SETTINGS_SOUND_TOGGLE = 127,
    ACTION_ID_SYS_SETTINGS_SOUND_TOGGLE_CLICK = 128,
    ACTION_ID_CH_SETTINGS_ADV_VIEW_EDIT_DISPLAY_VALUE1 = 129,
    ACTION_ID_CH_SETTINGS_ADV_VIEW_EDIT_DISPLAY_VALUE2 = 130,
    ACTION_ID_CH_SETTINGS_ADV_VIEW_SWAP_DISPLAY_VALUES = 131,
    ACTION_ID_CH_SETTINGS_ADV_VIEW_EDIT_YTVIEW_RATE = 132,
    ACTION_ID_ERROR_ALERT_ACTION = 133,
    ACTION_ID_SYS_SETTINGS_ENCODER_TOGGLE_CONFIRMATION_MODE = 134,
    ACTION_ID_TURN_DISPLAY_OFF = 135,
    ACTION_ID_CH_SETTINGS_TRIGGER_EDIT_TRIGGER_MODE = 136,
    ACTION_ID_CH_SETTINGS_TRIGGER_EDIT_VOLTAGE_TRIGGER_VALUE = 137,
    ACTION_ID_CH_SETTINGS_TRIGGER_EDIT_CURRENT_TRIGGER_VALUE = 138,
    ACTION_ID_CH_SETTINGS_TRIGGER_EDIT_LIST_COUNT = 139,
    ACTION_ID_CH_SETTINGS_TRIGGER_TOGGLE_OUTPUT_STATE = 140,
    ACTION_ID_CH_SETTINGS_TRIGGER_EDIT_ON_LIST_STOP = 141,
    ACTION_ID_CHANNEL_LISTS_PREVIOUS_PAGE = 142,
    ACTION_ID_CHANNEL_LISTS_NEXT_PAGE = 143,
    ACTION_ID_CHANNEL_LISTS_EDIT = 144,
    ACTION_ID_SHOW_CHANNEL_LISTS_INSERT_MENU = 145,
    ACTION_ID_SHOW_CHANNEL_LISTS_DELETE_MENU = 146,
    ACTION_ID_CHANNEL_LISTS_INSERT_ROW_ABOVE = 147,
    ACTION_ID_CHANNEL_LISTS_INSERT_ROW_BELOW = 148,
    ACTION_ID_CHANNEL_LISTS_DELETE_ROW = 149,
    ACTION_ID_CHANNEL_LISTS_CLEAR_COLUMN = 150,
    ACTION_ID_CHANNEL_LISTS_DELETE_ROWS = 151,
    ACTION_ID_CHANNEL_LISTS_DELETE_ALL = 152,
    ACTION_ID_CHANNEL_INITIATE_TRIGGER = 153,
    ACTION_ID_CHANNEL_SET_TO_FIXED = 154,
    ACTION_ID_CHANNEL_ENABLE_OUTPUT = 155,
    ACTION_ID_TRIGGER_SELECT_SOURCE = 156,
    ACTION_ID_TRIGGER_EDIT_DELAY = 157,
    ACTION_ID_TRIGGER_TOGGLE_INITIATE_CONTINUOUSLY = 158,
    ACTION_ID_TRIGGER_GENERATE_MANUAL = 159,
    ACTION_ID_TRIGGER_SHOW_GENERAL_SETTINGS = 160,
    ACTION_ID_SHOW_STAND_BY_MENU = 161,
    ACTION_ID_RESET = 162,
    ACTION_ID_CH_SETTINGS_ADV_RANGES_SELECT_MODE = 163,
    ACTION_ID_CH_SETTINGS_ADV_RANGES_TOGGLE_AUTO_RANGING = 164,
    ACTION_ID_IO_PIN_TOGGLE_POLARITY = 165,
    ACTION_ID_IO_PIN_SELECT_FUNCTION = 166,
    ACTION_ID_SERIAL_TOGGLE = 167,
    ACTION_ID_SERIAL_SELECT_PARITY = 168,
    ACTION_ID_NTP_TOGGLE = 169,
    ACTION_ID_NTP_EDIT_SERVER = 170,
    ACTION_ID_OPEN_APPLICATION = 171,
    ACTION_ID_SHOW_HOME = 172,
    ACTION_ID_SIMULATOR_LOAD = 173,
    ACTION_ID_SELECT_THEME = 174
};

void action_channel_toggle_output();
void action_edit();
void action_edit_mode_slider();
void action_edit_mode_step();
void action_edit_mode_keypad();
void action_exit_edit_mode();
void action_toggle_interactive_mode();
void action_non_interactive_enter();
void action_non_interactive_discard();
void action_keypad_key();
void action_keypad_space();
void action_keypad_back();
void action_keypad_clear();
void action_keypad_caps();
void action_keypad_ok();
void action_keypad_cancel();
void action_keypad_sign();
void action_keypad_unit();
void action_keypad_option1();
void action_keypad_option2();
void action_enter_touch_calibration();
void action_yes();
void action_no();
void action_ok();
void action_cancel();
void action_later();
void action_stand_by();
void action_show_previous_page();
void action_show_main_page();
void action_show_event_queue();
void action_show_channel_settings();
void action_show_sys_settings();
void action_show_sys_settings_trigger();
void action_show_sys_settings_io();
void action_show_sys_settings_date_time();
void action_show_sys_settings_cal();
void action_show_sys_settings_cal_ch();
void action_show_sys_settings_screen_calibration();
void action_show_sys_settings_display();
void action_show_sys_settings_serial();
void action_show_sys_settings_ethernet();
void action_show_sys_settings_protections();
void action_show_sys_settings_aux_otp();
void action_show_sys_settings_sound();
void action_show_sys_settings_encoder();
void action_show_sys_info();
void action_show_sys_info2();
void action_show_main_help_page();
void action_show_edit_mode_step_help();
void action_show_edit_mode_slider_help();
void action_show_ch_settings_prot();
void action_show_ch_settings_prot_clear();
void action_show_ch_settings_prot_ocp();
void action_show_ch_settings_prot_ovp();
void action_show_ch_settings_prot_opp();
void action_show_ch_settings_prot_otp();
void action_show_ch_settings_trigger();
void action_show_ch_settings_lists();
void action_show_ch_settings_adv();
void action_show_ch_settings_adv_remote();
void action_show_ch_settings_adv_ranges();
void action_show_ch_settings_adv_tracking();
void action_show_ch_settings_adv_coupling();
void action_show_ch_settings_adv_view();
void action_show_ch_settings_info();
void action_show_ch_settings_info_cal();
void action_sys_settings_cal_edit_password();
void action_sys_settings_cal_ch_wiz_start();
void action_sys_settings_cal_ch_wiz_step_previous();
void action_sys_settings_cal_ch_wiz_step_next();
void action_sys_settings_cal_ch_wiz_stop_and_show_previous_page();
void action_sys_settings_cal_ch_wiz_stop_and_show_main_page();
void action_sys_settings_cal_ch_wiz_step_set();
void action_sys_settings_cal_ch_wiz_step_set_level_value();
void action_sys_settings_cal_ch_wiz_save();
void action_sys_settings_cal_toggle_enable();
void action_ch_settings_prot_clear();
void action_ch_settings_prot_clear_and_disable();
void action_ch_settings_prot_toggle_state();
void action_ch_settings_prot_edit_limit();
void action_ch_settings_prot_edit_level();
void action_ch_settings_prot_edit_delay();
void action_set();
void action_discard();
void action_edit_field();
void action_event_queue_previous_page();
void action_event_queue_next_page();
void action_ch_settings_adv_remote_toggle_sense();
void action_ch_settings_adv_remote_toggle_programming();
void action_date_time_select_dst_rule();
void action_show_user_profiles();
void action_show_user_profiles2();
void action_show_user_profile_settings();
void action_profiles_toggle_auto_recall();
void action_profile_toggle_is_auto_recall_location();
void action_profile_recall();
void action_profile_save();
void action_profile_delete();
void action_profile_edit_remark();
void action_toggle_channels_view_mode();
void action_ethernet_toggle();
void action_ethernet_toggle_dhcp();
void action_ethernet_edit_mac_address();
void action_ethernet_edit_static_address();
void action_ethernet_edit_ip_address();
void action_ethernet_edit_dns();
void action_ethernet_edit_gateway();
void action_ethernet_edit_subnet_mask();
void action_ethernet_edit_scpi_port();
void action_ch_settings_adv_coupling_uncouple();
void action_ch_settings_adv_coupling_set_parallel_info();
void action_ch_settings_adv_coupling_set_series_info();
void action_ch_settings_adv_coupling_set_parallel();
void action_ch_settings_adv_coupling_set_series();
void action_ch_settings_adv_toggle_tracking_mode();
void action_sys_settings_protections_toggle_output_protection_couple();
void action_sys_settings_protections_toggle_shutdown_when_protection_tripped();
void action_sys_settings_protections_toggle_force_disabling_all_outputs_on_power_up();
void action_sys_settings_protections_aux_otp_toggle_state();
void action_sys_settings_protections_aux_otp_edit_level();
void action_sys_settings_protections_aux_otp_edit_delay();
void action_sys_settings_protections_aux_otp_clear();
void action_on_last_error_event_action();
void action_edit_system_password();
void action_sys_front_panel_lock();
void action_sys_front_panel_unlock();
void action_sys_settings_sound_toggle();
void action_sys_settings_sound_toggle_click();
void action_ch_settings_adv_view_edit_display_value1();
void action_ch_settings_adv_view_edit_display_value2();
void action_ch_settings_adv_view_swap_display_values();
void action_ch_settings_adv_view_edit_ytview_rate();
void action_error_alert_action();
void action_sys_settings_encoder_toggle_confirmation_mode();
void action_turn_display_off();
void action_ch_settings_trigger_edit_trigger_mode();
void action_ch_settings_trigger_edit_voltage_trigger_value();
void action_ch_settings_trigger_edit_current_trigger_value();
void action_ch_settings_trigger_edit_list_count();
void action_ch_settings_trigger_toggle_output_state();
void action_ch_settings_trigger_edit_on_list_stop();
void action_channel_lists_previous_page();
void action_channel_lists_next_page();
void action_channel_lists_edit();
void action_show_channel_lists_insert_menu();
void action_show_channel_lists_delete_menu();
void action_channel_lists_insert_row_above();
void action_channel_lists_insert_row_below();
void action_channel_lists_delete_row();
void action_channel_lists_clear_column();
void action_channel_lists_delete_rows();
void action_channel_lists_delete_all();
void action_channel_initiate_trigger();
void action_channel_set_to_fixed();
void action_channel_enable_output();
void action_trigger_select_source();
void action_trigger_edit_delay();
void action_trigger_toggle_initiate_continuously();
void action_trigger_generate_manual();
void action_trigger_show_general_settings();
void action_show_stand_by_menu();
void action_reset();
void action_ch_settings_adv_ranges_select_mode();
void action_ch_settings_adv_ranges_toggle_auto_ranging();
void action_io_pin_toggle_polarity();
void action_io_pin_select_function();
void action_serial_toggle();
void action_serial_select_parity();
void action_ntp_toggle();
void action_ntp_edit_server();
void action_open_application();
void action_show_home();
void action_simulator_load();
void action_select_theme();

extern ActionExecFunc g_actionExecFunctions[];

enum FontsEnum {
    FONT_ID_NONE = 0,
    FONT_ID_LARGE = 1,
    FONT_ID_SMALL = 2,
    FONT_ID_MEDIUM = 3,
    FONT_ID_ICONS = 4,
    FONT_ID_HEYDINGS16 = 5
};

enum BitmapsEnum {
    BITMAP_ID_NONE = 0,
    BITMAP_ID_PSU_ICON = 1,
    BITMAP_ID_SETTINGS_ICON = 2,
    BITMAP_ID_LOGO = 3,
    BITMAP_ID_BP_COUPLED = 4,
    BITMAP_ID_FRONT_PANEL = 5,
    BITMAP_ID_DCP505 = 6,
    BITMAP_ID_DCP405 = 7
};

enum StylesEnum {
    STYLE_ID_NONE = 0,
    STYLE_ID_CHANNEL_OFF_DISABLED = 1,
    STYLE_ID_ERROR_ALERT = 2,
    STYLE_ID_INFO_ALERT = 3,
    STYLE_ID_TOAST_ALERT = 4,
    STYLE_ID_SELECT_ENUM_ITEM_POPUP_CONTAINER = 5,
    STYLE_ID_SELECT_ENUM_ITEM_POPUP_ITEM = 6,
    STYLE_ID_SELECT_ENUM_ITEM_POPUP_DISABLED_ITEM = 7,
    STYLE_ID_DEFAULT = 8,
    STYLE_ID_DEFAULT_S = 9,
    STYLE_ID_EDIT_INFO_S = 10,
    STYLE_ID_TOUCH_CALIBRATION = 11,
    STYLE_ID_TOUCH_CALIBRATION_ACTIVE = 12,
    STYLE_ID_MENU_S = 13,
    STYLE_ID_BOTTOM_BUTTON_BACKGROUND = 14,
    STYLE_ID_BOTTOM_BUTTON = 15,
    STYLE_ID_HOME = 16,
    STYLE_ID_HOME_ICON = 17,
    STYLE_ID_HOME_ICON_ACTIVE = 18,
    STYLE_ID_DEFAULT_INVERSE = 19,
    STYLE_ID_CHANNEL_OFF = 20,
    STYLE_ID_MON_VALUE_L_RIGHT = 21,
    STYLE_ID_MON_VALUE_UR_L_RIGHT = 22,
    STYLE_ID_MON_VALUE_FOCUS = 23,
    STYLE_ID_MON_VALUE = 24,
    STYLE_ID_MON_DAC = 25,
    STYLE_ID_EDIT_S = 26,
    STYLE_ID_CHANNEL_ERROR = 27,
    STYLE_ID_BAR_GRAPH_U_DEFUALT = 28,
    STYLE_ID_BAR_GRAPH_TEXT = 29,
    STYLE_ID_BAR_GRAPH_SET_LINE = 30,
    STYLE_ID_BAR_GRAPH_LIMIT_LINE = 31,
    STYLE_ID_BAR_GRAPH_I_DEFAULT = 32,
    STYLE_ID_BAR_GRAPH_UNREGULATED = 33,
    STYLE_ID_EDIT_VALUE_S_RIGHT = 34,
    STYLE_ID_EDIT_VALUE_FOCUS_S_RIGHT = 35,
    STYLE_ID_EDIT_VALUE_ACTIVE_S_RIGHT = 36,
    STYLE_ID_MON_DAC_S = 37,
    STYLE_ID_EDIT_VALUE_ACTIVE_S_CENTER = 38,
    STYLE_ID_EDIT_VALUE_S_CENTERED = 39,
    STYLE_ID_BAR_GRAPH_TEXT_VERTICAL = 40,
    STYLE_ID_EDIT_S_FOCUS = 41,
    STYLE_ID_YT_GRAPH = 42,
    STYLE_ID_YT_GRAPH_U_DEFUALT = 43,
    STYLE_ID_YT_GRAPH_I_DEFAULT = 44,
    STYLE_ID_YT_GRAPH_UNREGULATED = 45,
    STYLE_ID_COUPLED_INFO_S = 46,
    STYLE_ID_EDIT_VALUE = 47,
    STYLE_ID_EDIT_VALUE_S_LEFT = 48,
    STYLE_ID_KEY = 49,
    STYLE_ID_KEY_ICONS = 50,
    STYLE_ID_KEY_DISABLED = 51,
    STYLE_ID_TAB_PAGE_SELECTED = 52,
    STYLE_ID_TAB_PAGE = 53,
    STYLE_ID_EDIT_VALUE_L = 54,
    STYLE_ID_VALUE = 55,
    STYLE_ID_VALUE_S = 56,
    STYLE_ID_NON_INTERACTIVE_BUTTON_S = 57,
    STYLE_ID_YELLOW_1 = 58,
    STYLE_ID_YELLOW_2 = 59,
    STYLE_ID_YELLOW_3 = 60,
    STYLE_ID_YELLOW_4 = 61,
    STYLE_ID_YELLOW_5 = 62,
    STYLE_ID_YELLOW_6 = 63,
    STYLE_ID_ERROR_ALERT_BUTTON = 64,
    STYLE_ID_YES_NO = 65,
    STYLE_ID_YES_NO_BUTTON = 66,
    STYLE_ID_YES_NO_BACKGROUND = 67,
    STYLE_ID_YES_NO_MESSAGE = 68,
    STYLE_ID_TEXT_MESSAGE = 69,
    STYLE_ID_ASYNC_OPERATION = 70,
    STYLE_ID_ASYNC_OPERATION_ACTION = 71,
    STYLE_ID_EVENT_INFO = 72,
    STYLE_ID_EVENT_WARNING = 73,
    STYLE_ID_EVENT_ERROR = 74,
    STYLE_ID_BOTTOM_BUTTON_DISABLED = 75,
    STYLE_ID_BOTTOM_BUTTON_TEXTUAL_S_LEFT = 76,
    STYLE_ID_EDIT_VALUE_ACTIVE_S_LEFT = 77,
    STYLE_ID_KEY_SPEC_ICONS = 78,
    STYLE_ID_KEY_SPEC = 79,
    STYLE_ID_NON_INTERACTIVE_BUTTON_S_DISABLED = 80,
    STYLE_ID_MAX_CURRENT_LIMIT_CAUSE = 81,
    STYLE_ID_DEFAULT_DISABLED_S_LEFT = 82,
    STYLE_ID_LIST_GRAPH_CURSOR = 83,
    STYLE_ID_YT_GRAPH_U_DEFUALT_LABEL = 84,
    STYLE_ID_YT_GRAPH_I_DEFAULT_LABEL = 85,
    STYLE_ID_EDIT_VALUE_FOCUS_S_CENTER = 86,
    STYLE_ID_EDIT_VALUE_LEFT = 87,
    STYLE_ID_EDIT_VALUE_ACTIVE_LEFT = 88,
    STYLE_ID_STATUS_BGND = 89,
    STYLE_ID_EDIT_VALUE_ACTIVE = 90,
    STYLE_ID_DEFAULT_DISABLED = 91,
    STYLE_ID_BOTTOM_BUTTON_TEXTUAL_S = 92,
    STYLE_ID_BOTTOM_BUTTON_TEXTUAL_S_DISABLED = 93,
    STYLE_ID_DARK_LINE = 94,
    STYLE_ID_DISPLAY_OFF = 95,
    STYLE_ID_DISPLAY_OFF_S = 96,
    STYLE_ID_SET_VALUE_S_RIGHT_BALANCED = 97,
    STYLE_ID_SET_VALUE_BALANCED = 98,
    STYLE_ID_SET_VALUE_FOCUS_BALANCED = 99,
    STYLE_ID_PROT_INDICATOR_S = 100,
    STYLE_ID_PROT_INDICATOR_SET_S = 101,
    STYLE_ID_PROT_INDICATOR_TRIP_S = 102,
    STYLE_ID_FRONT_PANEL_DEFAULT = 103,
    STYLE_ID_EVENT_INFO_ICON = 104,
    STYLE_ID_EVENT_WARNING_ICON = 105,
    STYLE_ID_EVENT_ERROR_ICON = 106,
    STYLE_ID_PROT_INDICATOR_INVALID_S = 107,
    STYLE_ID_BUTTON_INDICATOR_ON = 108,
    STYLE_ID_BUTTON_INDICATOR_ERROR = 109,
    STYLE_ID_BUTTON_INDICATOR_OFF = 110,
    STYLE_ID_PROT_INDICATOR_BLINK_S = 111,
    STYLE_ID_DEFAULT_S_LEFT = 112,
    STYLE_ID_DEFAULT_3 = 113
};

enum PagesEnum {
    PAGE_ID_WELCOME = 0,
    PAGE_ID_TOUCH_CALIBRATION_INTRO = 1,
    PAGE_ID_TOUCH_CALIBRATION = 2,
    PAGE_ID_TOUCH_CALIBRATION_YES_NO = 3,
    PAGE_ID_TOUCH_CALIBRATION_YES_NO_CANCEL = 4,
    PAGE_ID_SELF_TEST_RESULT = 5,
    PAGE_ID_HOME = 6,
    PAGE_ID_HOME_PSU = 7,
    PAGE_ID_MAIN = 8,
    PAGE_ID_MAIN_HELP = 9,
    PAGE_ID_EDIT_MODE_KEYPAD = 10,
    PAGE_ID_EDIT_MODE_STEP = 11,
    PAGE_ID_EDIT_MODE_STEP_HELP = 12,
    PAGE_ID_EDIT_MODE_SLIDER = 13,
    PAGE_ID_EDIT_MODE_SLIDER_HELP = 14,
    PAGE_ID_ERROR_ALERT_WITH_ACTION = 15,
    PAGE_ID_YES_NO = 16,
    PAGE_ID_ARE_YOU_SURE_WITH_MESSAGE = 17,
    PAGE_ID_YES_NO_LATER = 18,
    PAGE_ID_TEXT_MESSAGE = 19,
    PAGE_ID_ASYNC_OPERATION_IN_PROGRESS = 20,
    PAGE_ID_PROGRESS = 21,
    PAGE_ID_EVENT_QUEUE = 22,
    PAGE_ID_KEYPAD = 23,
    PAGE_ID_NUMERIC_KEYPAD = 24,
    PAGE_ID_CH_SETTINGS_PROT = 25,
    PAGE_ID_CH_SETTINGS_PROT_CLEAR = 26,
    PAGE_ID_CH_SETTINGS_PROT_OVP = 27,
    PAGE_ID_CH_SETTINGS_PROT_OCP = 28,
    PAGE_ID_CH_SETTINGS_PROT_OPP = 29,
    PAGE_ID_CH_SETTINGS_PROT_OTP = 30,
    PAGE_ID_CH_SETTINGS_TRIGGER = 31,
    PAGE_ID_CH_SETTINGS_LISTS = 32,
    PAGE_ID_CH_SETTINGS_LISTS_INSERT_MENU = 33,
    PAGE_ID_CH_SETTINGS_LISTS_DELETE_MENU = 34,
    PAGE_ID_CH_START_LIST = 35,
    PAGE_ID_CH_SETTINGS_ADV = 36,
    PAGE_ID_CH_SETTINGS_ADV_REMOTE = 37,
    PAGE_ID_CH_SETTINGS_ADV_RANGES = 38,
    PAGE_ID_CH_SETTINGS_ADV_TRACKING = 39,
    PAGE_ID_CH_SETTINGS_ADV_COUPLING = 40,
    PAGE_ID_CH_SETTINGS_ADV_COUPLING_INFO = 41,
    PAGE_ID_CH_SETTINGS_ADV_VIEW = 42,
    PAGE_ID_CH_SETTINGS_INFO = 43,
    PAGE_ID_SYS_SETTINGS = 44,
    PAGE_ID_SYS_SETTINGS_AUX_OTP = 45,
    PAGE_ID_SYS_SETTINGS_PROTECTIONS = 46,
    PAGE_ID_SYS_SETTINGS_TRIGGER = 47,
    PAGE_ID_SYS_SETTINGS_IO = 48,
    PAGE_ID_SYS_SETTINGS_DATE_TIME = 49,
    PAGE_ID_SYS_SETTINGS_ENCODER = 50,
    PAGE_ID_SYS_SETTINGS_SERIAL = 51,
    PAGE_ID_SYS_SETTINGS_ETHERNET = 52,
    PAGE_ID_SYS_SETTINGS_ETHERNET_STATIC = 53,
    PAGE_ID_SYS_SETTINGS_CAL = 54,
    PAGE_ID_SYS_SETTINGS_CAL_CH = 55,
    PAGE_ID_SYS_SETTINGS_CAL_CH_WIZ_STEP = 56,
    PAGE_ID_SYS_SETTINGS_CAL_CH_WIZ_FINISH = 57,
    PAGE_ID_SYS_SETTINGS_SCREEN_CALIBRATION = 58,
    PAGE_ID_SYS_SETTINGS_DISPLAY = 59,
    PAGE_ID_SYS_SETTINGS_SOUND = 60,
    PAGE_ID_USER_PROFILES = 61,
    PAGE_ID_USER_PROFILES2 = 62,
    PAGE_ID_USER_PROFILE_SETTINGS = 63,
    PAGE_ID_USER_PROFILE_0_SETTINGS = 64,
    PAGE_ID_SYS_INFO = 65,
    PAGE_ID_SYS_INFO2 = 66,
    PAGE_ID_STAND_BY_MENU = 67,
    PAGE_ID_ENTERING_STANDBY = 68,
    PAGE_ID_STANDBY = 69,
    PAGE_ID_DISPLAY_OFF = 70,
    PAGE_ID_CHANNELS = 71,
    PAGE_ID_PROFILE = 72,
    PAGE_ID_PROFILE_BASIC_INFO = 73,
    PAGE_ID_COUPLED_CHANNELS_DEFAULT_VIEW = 74,
    PAGE_ID_COUPLED_CHANNELS_HORIZONTAL_BAR_VIEW = 75,
    PAGE_ID_COUPLED_CHANNELS_VERTICAL_BAR_VIEW = 76,
    PAGE_ID_COUPLED_CHANNELS_YTBAR_VIEW = 77,
    PAGE_ID_OCP = 78,
    PAGE_ID_OVP = 79,
    PAGE_ID_OTP = 80,
    PAGE_ID_OPP = 81,
    PAGE_ID_FRONT_PANEL = 82,
    PAGE_ID_NUMERIC_KEYPAD2 = 83,
    PAGE_ID_OLD_STATUS_BAR = 84,
    PAGE_ID_OLD_TASK_BAR = 85,
    PAGE_ID_MINIMIZED_APP_VIEW = 86
};

extern const uint8_t assets[200891];
