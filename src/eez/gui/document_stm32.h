enum DataEnum {
    DATA_ID_NONE = 0,
    DATA_ID_EDIT_ENABLED = 1,
    DATA_ID_CHANNELS = 2,
    DATA_ID_CHANNEL_STATUS = 3,
    DATA_ID_CHANNEL_OUTPUT_STATE = 4,
    DATA_ID_CHANNEL_IS_CC = 5,
    DATA_ID_CHANNEL_IS_CV = 6,
    DATA_ID_CHANNEL_U_SET = 7,
    DATA_ID_CHANNEL_U_MON = 8,
    DATA_ID_CHANNEL_U_MON_SNAPSHOT = 9,
    DATA_ID_CHANNEL_U_MON_DAC = 10,
    DATA_ID_CHANNEL_U_LIMIT = 11,
    DATA_ID_CHANNEL_U_EDIT = 12,
    DATA_ID_CHANNEL_I_SET = 13,
    DATA_ID_CHANNEL_I_MON = 14,
    DATA_ID_CHANNEL_I_MON_SNAPSHOT = 15,
    DATA_ID_CHANNEL_I_MON_DAC = 16,
    DATA_ID_CHANNEL_I_LIMIT = 17,
    DATA_ID_CHANNEL_I_EDIT = 18,
    DATA_ID_CHANNEL_P_MON = 19,
    DATA_ID_CHANNEL_P_MON_SNAPSHOT = 20,
    DATA_ID_CHANNEL_OTHER_VALUE_MON = 21,
    DATA_ID_CHANNELS_IS_MAX_VIEW = 22,
    DATA_ID_CHANNELS_VIEW_MODE = 23,
    DATA_ID_CHANNELS_VIEW_MODE_IN_DEFAULT = 24,
    DATA_ID_CHANNELS_VIEW_MODE_IN_MAX = 25,
    DATA_ID_SLOT_MAX_CHANNEL_INDEX = 26,
    DATA_ID_SLOT_MAX_VIEW = 27,
    DATA_ID_SLOT_MIN1_CHANNEL_INDEX = 28,
    DATA_ID_SLOT_MIN1_VIEW = 29,
    DATA_ID_SLOT_MIN2_CHANNEL_INDEX = 30,
    DATA_ID_SLOT_MIN2_VIEW = 31,
    DATA_ID_SLOT_MICRO1_VIEW = 32,
    DATA_ID_SLOT_MICRO2_VIEW = 33,
    DATA_ID_SLOT_MICRO3_VIEW = 34,
    DATA_ID_CHANNEL_DISPLAY_VALUE1 = 35,
    DATA_ID_CHANNEL_DISPLAY_VALUE2 = 36,
    DATA_ID_OVP = 37,
    DATA_ID_OCP = 38,
    DATA_ID_OPP = 39,
    DATA_ID_OTP_CH = 40,
    DATA_ID_OTP_AUX = 41,
    DATA_ID_ALERT_MESSAGE = 42,
    DATA_ID_ALERT_MESSAGE_2 = 43,
    DATA_ID_ALERT_MESSAGE_3 = 44,
    DATA_ID_EDIT_VALUE = 45,
    DATA_ID_EDIT_UNIT = 46,
    DATA_ID_EDIT_INFO = 47,
    DATA_ID_EDIT_MODE_INTERACTIVE_MODE_SELECTOR = 48,
    DATA_ID_EDIT_STEPS = 49,
    DATA_ID_MASTER_INFO = 50,
    DATA_ID_MASTER_TEST_RESULT = 51,
    DATA_ID_FIRMWARE_INFO = 52,
    DATA_ID_SELF_TEST_RESULT = 53,
    DATA_ID_KEYPAD_TEXT = 54,
    DATA_ID_KEYPAD_MODE = 55,
    DATA_ID_KEYPAD_OPTION1_TEXT = 56,
    DATA_ID_KEYPAD_OPTION1_ENABLED = 57,
    DATA_ID_KEYPAD_OPTION2_TEXT = 58,
    DATA_ID_KEYPAD_OPTION2_ENABLED = 59,
    DATA_ID_KEYPAD_SIGN_ENABLED = 60,
    DATA_ID_KEYPAD_DOT_ENABLED = 61,
    DATA_ID_KEYPAD_UNIT_ENABLED = 62,
    DATA_ID_CHANNEL_LABEL = 63,
    DATA_ID_CHANNEL_SHORT_LABEL = 64,
    DATA_ID_CHANNEL_TEMP_STATUS = 65,
    DATA_ID_CHANNEL_TEMP = 66,
    DATA_ID_CHANNEL_ON_TIME_TOTAL = 67,
    DATA_ID_CHANNEL_ON_TIME_LAST = 68,
    DATA_ID_CHANNEL_CALIBRATION_STATUS = 69,
    DATA_ID_CHANNEL_CALIBRATION_STATE = 70,
    DATA_ID_CHANNEL_CALIBRATION_DATE = 71,
    DATA_ID_CHANNEL_CALIBRATION_REMARK = 72,
    DATA_ID_CHANNEL_CALIBRATION_STEP_IS_SET_REMARK_STEP = 73,
    DATA_ID_CHANNEL_CALIBRATION_STEP_NUM = 74,
    DATA_ID_CHANNEL_CALIBRATION_STEP_STATUS = 75,
    DATA_ID_CHANNEL_CALIBRATION_STEP_LEVEL_VALUE = 76,
    DATA_ID_CHANNEL_CALIBRATION_STEP_VALUE = 77,
    DATA_ID_CHANNEL_CALIBRATION_STEP_PREV_ENABLED = 78,
    DATA_ID_CHANNEL_CALIBRATION_STEP_NEXT_ENABLED = 79,
    DATA_ID_CAL_CH_U_MIN = 80,
    DATA_ID_CAL_CH_U_MID = 81,
    DATA_ID_CAL_CH_U_MAX = 82,
    DATA_ID_CAL_CH_I0_MIN = 83,
    DATA_ID_CAL_CH_I0_MID = 84,
    DATA_ID_CAL_CH_I0_MAX = 85,
    DATA_ID_CAL_CH_I1_MIN = 86,
    DATA_ID_CAL_CH_I1_MID = 87,
    DATA_ID_CAL_CH_I1_MAX = 88,
    DATA_ID_CHANNEL_PROTECTION_STATUS = 89,
    DATA_ID_CHANNEL_PROTECTION_OVP_STATE = 90,
    DATA_ID_CHANNEL_PROTECTION_OVP_TYPE = 91,
    DATA_ID_CHANNEL_PROTECTION_OVP_LEVEL = 92,
    DATA_ID_CHANNEL_PROTECTION_OVP_DELAY = 93,
    DATA_ID_CHANNEL_PROTECTION_OVP_LIMIT = 94,
    DATA_ID_CHANNEL_PROTECTION_OCP_STATE = 95,
    DATA_ID_CHANNEL_PROTECTION_OCP_DELAY = 96,
    DATA_ID_CHANNEL_PROTECTION_OCP_LIMIT = 97,
    DATA_ID_CHANNEL_PROTECTION_OCP_MAX_CURRENT_LIMIT_CAUSE = 98,
    DATA_ID_CHANNEL_PROTECTION_OPP_STATE = 99,
    DATA_ID_CHANNEL_PROTECTION_OPP_LEVEL = 100,
    DATA_ID_CHANNEL_PROTECTION_OPP_DELAY = 101,
    DATA_ID_CHANNEL_PROTECTION_OPP_LIMIT = 102,
    DATA_ID_CHANNEL_PROTECTION_OTP_INSTALLED = 103,
    DATA_ID_CHANNEL_PROTECTION_OTP_STATE = 104,
    DATA_ID_CHANNEL_PROTECTION_OTP_LEVEL = 105,
    DATA_ID_CHANNEL_PROTECTION_OTP_DELAY = 106,
    DATA_ID_EVENT_QUEUE_LAST_EVENT_TYPE = 107,
    DATA_ID_EVENT_QUEUE_LAST_EVENT_MESSAGE = 108,
    DATA_ID_EVENT_QUEUE_EVENTS = 109,
    DATA_ID_EVENT_QUEUE_EVENTS_TYPE = 110,
    DATA_ID_EVENT_QUEUE_EVENTS_MESSAGE = 111,
    DATA_ID_EVENT_QUEUE_MULTIPLE_PAGES = 112,
    DATA_ID_EVENT_QUEUE_PREVIOUS_PAGE_ENABLED = 113,
    DATA_ID_EVENT_QUEUE_NEXT_PAGE_ENABLED = 114,
    DATA_ID_EVENT_QUEUE_PAGE_INFO = 115,
    DATA_ID_CHANNEL_RSENSE_STATUS = 116,
    DATA_ID_CHANNEL_RPROG_INSTALLED = 117,
    DATA_ID_CHANNEL_RPROG_STATUS = 118,
    DATA_ID_CHANNEL_TRACKING_IS_ENABLED = 119,
    DATA_ID_CHANNEL_TRACKING_IS_ALLOWED = 120,
    DATA_ID_CHANNEL_IS_PAR_SER_COUPLED_OR_TRACKED = 121,
    DATA_ID_COUPLING_TYPE = 122,
    DATA_ID_CHANNEL_COUPLING_IS_SERIES = 123,
    DATA_ID_SYS_ON_TIME_TOTAL = 124,
    DATA_ID_SYS_ON_TIME_LAST = 125,
    DATA_ID_SYS_TEMP_AUX_STATUS = 126,
    DATA_ID_SYS_TEMP_AUX_OTP_STATE = 127,
    DATA_ID_SYS_TEMP_AUX_OTP_LEVEL = 128,
    DATA_ID_SYS_TEMP_AUX_OTP_DELAY = 129,
    DATA_ID_SYS_TEMP_AUX_OTP_IS_TRIPPED = 130,
    DATA_ID_SYS_TEMP_AUX = 131,
    DATA_ID_SYS_INFO_FIRMWARE_VER = 132,
    DATA_ID_SYS_INFO_SERIAL_NO = 133,
    DATA_ID_SYS_INFO_SCPI_VER = 134,
    DATA_ID_SYS_INFO_CPU = 135,
    DATA_ID_SYS_INFO_FAN_STATUS = 136,
    DATA_ID_SYS_INFO_FAN_SPEED = 137,
    DATA_ID_CHANNEL_BOARD_INFO_LABEL = 138,
    DATA_ID_CHANNEL_BOARD_INFO_REVISION = 139,
    DATA_ID_DATE_TIME_DATE = 140,
    DATA_ID_DATE_TIME_YEAR = 141,
    DATA_ID_DATE_TIME_MONTH = 142,
    DATA_ID_DATE_TIME_DAY = 143,
    DATA_ID_DATE_TIME_TIME = 144,
    DATA_ID_DATE_TIME_HOUR = 145,
    DATA_ID_DATE_TIME_MINUTE = 146,
    DATA_ID_DATE_TIME_SECOND = 147,
    DATA_ID_DATE_TIME_TIME_ZONE = 148,
    DATA_ID_DATE_TIME_DST = 149,
    DATA_ID_SET_PAGE_DIRTY = 150,
    DATA_ID_PROFILES_LIST = 151,
    DATA_ID_PROFILES_AUTO_RECALL_STATUS = 152,
    DATA_ID_PROFILES_AUTO_RECALL_LOCATION = 153,
    DATA_ID_PROFILE_STATUS = 154,
    DATA_ID_PROFILE_LABEL = 155,
    DATA_ID_PROFILE_REMARK = 156,
    DATA_ID_PROFILE_IS_AUTO_RECALL_LOCATION = 157,
    DATA_ID_PROFILE_CHANNEL_U_SET = 158,
    DATA_ID_PROFILE_CHANNEL_I_SET = 159,
    DATA_ID_PROFILE_CHANNEL_OUTPUT_STATE = 160,
    DATA_ID_ETHERNET_INSTALLED = 161,
    DATA_ID_ETHERNET_ENABLED = 162,
    DATA_ID_MQTT_ENABLED = 163,
    DATA_ID_ETHERNET_STATUS = 164,
    DATA_ID_MQTT_CONNECTION_STATE = 165,
    DATA_ID_ETHERNET_IP_ADDRESS = 166,
    DATA_ID_ETHERNET_DNS = 167,
    DATA_ID_ETHERNET_GATEWAY = 168,
    DATA_ID_ETHERNET_SUBNET_MASK = 169,
    DATA_ID_ETHERNET_SCPI_PORT = 170,
    DATA_ID_ETHERNET_IS_CONNECTED = 171,
    DATA_ID_ETHERNET_DHCP = 172,
    DATA_ID_ETHERNET_MAC = 173,
    DATA_ID_CHANNEL_IS_VOLTAGE_BALANCED = 174,
    DATA_ID_CHANNEL_IS_CURRENT_BALANCED = 175,
    DATA_ID_SYS_OUTPUT_PROTECTION_COUPLED = 176,
    DATA_ID_SYS_SHUTDOWN_WHEN_PROTECTION_TRIPPED = 177,
    DATA_ID_SYS_FORCE_DISABLING_ALL_OUTPUTS_ON_POWER_UP = 178,
    DATA_ID_SYS_PASSWORD_IS_SET = 179,
    DATA_ID_SYS_RL_STATE = 180,
    DATA_ID_SYS_SOUND_IS_ENABLED = 181,
    DATA_ID_SYS_SOUND_IS_CLICK_ENABLED = 182,
    DATA_ID_SYS_SOUND_ENABLED = 183,
    DATA_ID_CHANNEL_DISPLAY_VIEW_SETTINGS_DISPLAY_VALUE1 = 184,
    DATA_ID_CHANNEL_DISPLAY_VIEW_SETTINGS_DISPLAY_VALUE2 = 185,
    DATA_ID_CHANNEL_DISPLAY_VIEW_SETTINGS_YT_VIEW_RATE = 186,
    DATA_ID_SYS_ENCODER_CONFIRMATION_MODE = 187,
    DATA_ID_SYS_ENCODER_MOVING_UP_SPEED = 188,
    DATA_ID_SYS_ENCODER_MOVING_DOWN_SPEED = 189,
    DATA_ID_SYS_ENCODER_INSTALLED = 190,
    DATA_ID_SYS_DISPLAY_STATE = 191,
    DATA_ID_SYS_DISPLAY_BRIGHTNESS = 192,
    DATA_ID_CHANNEL_TRIGGER_MODE = 193,
    DATA_ID_CHANNEL_TRIGGER_ON_LIST_STOP = 194,
    DATA_ID_CHANNEL_U_TRIGGER_VALUE = 195,
    DATA_ID_CHANNEL_I_TRIGGER_VALUE = 196,
    DATA_ID_CHANNEL_LIST_COUNT = 197,
    DATA_ID_CHANNEL_LISTS = 198,
    DATA_ID_CHANNEL_LIST_INDEX = 199,
    DATA_ID_CHANNEL_LIST_DWELL = 200,
    DATA_ID_CHANNEL_LIST_DWELL_ENABLED = 201,
    DATA_ID_CHANNEL_LIST_VOLTAGE = 202,
    DATA_ID_CHANNEL_LIST_VOLTAGE_ENABLED = 203,
    DATA_ID_CHANNEL_LIST_CURRENT = 204,
    DATA_ID_CHANNEL_LIST_CURRENT_ENABLED = 205,
    DATA_ID_CHANNEL_LISTS_PREVIOUS_PAGE_ENABLED = 206,
    DATA_ID_CHANNEL_LISTS_NEXT_PAGE_ENABLED = 207,
    DATA_ID_CHANNEL_LISTS_CURSOR = 208,
    DATA_ID_CHANNEL_LISTS_INSERT_MENU_ENABLED = 209,
    DATA_ID_CHANNEL_LISTS_DELETE_MENU_ENABLED = 210,
    DATA_ID_CHANNEL_LISTS_DELETE_ROW_ENABLED = 211,
    DATA_ID_CHANNEL_LISTS_CLEAR_COLUMN_ENABLED = 212,
    DATA_ID_CHANNEL_LISTS_DELETE_ROWS_ENABLED = 213,
    DATA_ID_TRIGGER_SOURCE = 214,
    DATA_ID_TRIGGER_DELAY = 215,
    DATA_ID_TRIGGER_INITIATE_CONTINUOUSLY = 216,
    DATA_ID_TRIGGER_IS_INITIATED = 217,
    DATA_ID_TRIGGER_IS_MANUAL = 218,
    DATA_ID_CHANNEL_HAS_SUPPORT_FOR_CURRENT_DUAL_RANGE = 219,
    DATA_ID_CHANNEL_RANGES_SUPPORTED = 220,
    DATA_ID_CHANNEL_RANGES_MODE = 221,
    DATA_ID_CHANNEL_RANGES_AUTO_RANGING = 222,
    DATA_ID_CHANNEL_RANGES_CURRENTLY_SELECTED = 223,
    DATA_ID_TEXT_MESSAGE = 224,
    DATA_ID_SERIAL_STATUS = 225,
    DATA_ID_SERIAL_ENABLED = 226,
    DATA_ID_SERIAL_IS_CONNECTED = 227,
    DATA_ID_SERIAL_BAUD = 228,
    DATA_ID_SERIAL_PARITY = 229,
    DATA_ID_CHANNEL_LIST_COUNTDOWN = 230,
    DATA_ID_IO_PINS = 231,
    DATA_ID_IO_PINS_INHIBIT_STATE = 232,
    DATA_ID_IO_PIN_NUMBER = 233,
    DATA_ID_IO_PIN_POLARITY = 234,
    DATA_ID_IO_PIN_FUNCTION = 235,
    DATA_ID_NTP_ENABLED = 236,
    DATA_ID_NTP_SERVER = 237,
    DATA_ID_ASYNC_OPERATION_THROBBER = 238,
    DATA_ID_SYS_DISPLAY_BACKGROUND_LUMINOSITY_STEP = 239,
    DATA_ID_PROGRESS = 240,
    DATA_ID_TOUCH_CALIBRATION_POINT = 241,
    DATA_ID_SELECTED_THEME = 242,
    DATA_ID_CHANNEL_TITLE = 243,
    DATA_ID_CHANNEL_SHORT_TITLE = 244,
    DATA_ID_CHANNEL_SHORT_TITLE_WITH_COLON = 245,
    DATA_ID_CHANNEL_SHORT_TITLE_WITHOUT_TRACKING_ICON = 246,
    DATA_ID_CHANNEL_LONG_TITLE = 247,
    DATA_ID_ANIMATIONS_DURATION = 248,
    DATA_ID_SLOT1_INFO = 249,
    DATA_ID_SLOT2_INFO = 250,
    DATA_ID_SLOT3_INFO = 251,
    DATA_ID_SLOT1_TEST_RESULT = 252,
    DATA_ID_SLOT2_TEST_RESULT = 253,
    DATA_ID_SLOT3_TEST_RESULT = 254,
    DATA_ID_SLOT_DEFAULT1_VIEW = 255,
    DATA_ID_SLOT_DEFAULT2_VIEW = 256,
    DATA_ID_SLOT_DEFAULT3_VIEW = 257,
    DATA_ID_SLOT1_CHANNEL_INDEX = 258,
    DATA_ID_SLOT2_CHANNEL_INDEX = 259,
    DATA_ID_SLOT3_CHANNEL_INDEX = 260,
    DATA_ID_SLOT_DEF_2CH_VIEW = 261,
    DATA_ID_SLOT_MAX_2CH_VIEW = 262,
    DATA_ID_SLOT_MIN_2CH_VIEW = 263,
    DATA_ID_SLOT_MICRO_2CH_VIEW = 264,
    DATA_ID_SLOT_2CH_CH1_INDEX = 265,
    DATA_ID_SLOT_2CH_CH2_INDEX = 266,
    DATA_ID_BATTERY = 267,
    DATA_ID_IO_PIN_STATE = 268,
    DATA_ID_CHANNEL_DPROG = 269,
    DATA_ID_CHANNEL_DPROG_INSTALLED = 270,
    DATA_ID_SYS_INFO_SDCARD_STATUS = 271,
    DATA_ID_SYS_INFO_HAS_ERROR = 272,
    DATA_ID_SYS_INFO_BATTERY_STATUS = 273,
    DATA_ID_SYS_FAN_MODE = 274,
    DATA_ID_SYS_FAN_SPEED = 275,
    DATA_ID_IS_COUPLING_TYPE_UNCOUPLED = 276,
    DATA_ID_IS_COUPLING_TYPE_PARALLEL = 277,
    DATA_ID_IS_COUPLING_TYPE_COMMON_GND = 278,
    DATA_ID_IS_COUPLING_TYPE_SPLIT_RAILS = 279,
    DATA_ID_IS_TRACKING_ALLOWED = 280,
    DATA_ID_IS_COUPLING_PARALLEL_ALLOWED = 281,
    DATA_ID_IS_COUPLING_SERIES_ALLOWED = 282,
    DATA_ID_CHANNEL_COUPLING_ENABLE_TRACKING_MODE = 283,
    DATA_ID_CHANNEL_ACTIVE_COUPLED_LED = 284,
    DATA_ID_IS_COUPLING_TYPE_SERIES = 285,
    DATA_ID_IS_COUPLED_OR_TRACKED = 286,
    DATA_ID_OVERLAY = 287,
    DATA_ID_CHANNELS_WITH_LIST_COUNTER_VISIBLE = 288,
    DATA_ID_DLOG_STATE = 289,
    DATA_ID_DLOG_TOGGLE_STATE = 290,
    DATA_ID_RECORDING = 291,
    DATA_ID_CHANNEL_HISTORY_VALUES = 292,
    DATA_ID_RECORDING_READY = 293,
    DATA_ID_NONDRAG_OVERLAY = 294,
    DATA_ID_DLOG_SINGLE_VALUE_OVERLAY = 295,
    DATA_ID_DLOG_MULTIPLE_VALUES_OVERLAY = 296,
    DATA_ID_DLOG_VISIBLE_VALUES = 297,
    DATA_ID_DLOG_VISIBLE_VALUE_LABEL = 298,
    DATA_ID_DLOG_VISIBLE_VALUE_DIV = 299,
    DATA_ID_DLOG_VISIBLE_VALUE_OFFSET = 300,
    DATA_ID_DLOG_VISIBLE_VALUE_CURSOR = 301,
    DATA_ID_DLOG_ALL_VALUES = 302,
    DATA_ID_DLOG_VALUE_LABEL = 303,
    DATA_ID_DLOG_CURRENT_TIME = 304,
    DATA_ID_DLOG_FILE_LENGTH = 305,
    DATA_ID_DLOG_X_AXIS_OFFSET = 306,
    DATA_ID_DLOG_X_AXIS_DIV = 307,
    DATA_ID_DLOG_X_AXIS_MAX_VALUE = 308,
    DATA_ID_DLOG_X_AXIS_MAX_VALUE_LABEL = 309,
    DATA_ID_IS_SINGLE_PAGE_ON_STACK = 310,
    DATA_ID_IS_SHOW_LIVE_RECORDING = 311,
    DATA_ID_DLOG_VIEW_STATE = 312,
    DATA_ID_DLOG_PERIOD = 313,
    DATA_ID_DLOG_DURATION = 314,
    DATA_ID_DLOG_VOLTAGE_ENABLED = 315,
    DATA_ID_DLOG_CURRENT_ENABLED = 316,
    DATA_ID_DLOG_POWER_ENABLED = 317,
    DATA_ID_DLOG_START_ENABLED = 318,
    DATA_ID_DLOG_FILE_NAME = 319,
    DATA_ID_DLOG_VALUE_STATE = 320,
    DATA_ID_SLOT_MAX_2CH_MIN_VIEW = 321,
    DATA_ID_CHANNEL_OFF_LABEL = 322,
    DATA_ID_FILE_MANAGER_FILES = 323,
    DATA_ID_FILE_MANAGER_FILE_NAME = 324,
    DATA_ID_FILE_MANAGER_FILE_DATE_TIME = 325,
    DATA_ID_FILE_MANAGER_FILE_SIZE = 326,
    DATA_ID_FILE_MANAGER_FILE_SELECTED = 327,
    DATA_ID_FILE_MANAGER_STATE = 328,
    DATA_ID_FILE_MANAGER_IS_ROOT_DIRECTORY = 329,
    DATA_ID_FILE_MANAGER_CURRENT_DIRECTORY = 330,
    DATA_ID_FILE_MANAGER_IS_DIRECTORY = 331,
    DATA_ID_FILE_MANAGER_FILE_TYPE = 332,
    DATA_ID_FILE_MANAGER_OPEN_FILE_ENABLED = 333,
    DATA_ID_FILE_MANAGER_UPLOAD_FILE_ENABLED = 334,
    DATA_ID_FILE_MANAGER_RENAME_FILE_ENABLED = 335,
    DATA_ID_FILE_MANAGER_DELETE_FILE_ENABLED = 336,
    DATA_ID_FILE_MANAGER_OPENED_IMAGE = 337,
    DATA_ID_SCRIPT_INFO = 338,
    DATA_ID_SCRIPT_IS_STARTED = 339,
    DATA_ID_DEBUG_TRACE_LOG = 340,
    DATA_ID_DEBUG_TRACE_LOG_LINE = 341,
    DATA_ID_KEYPAD_OK_ENABLED = 342,
    DATA_ID_ETHERNET_HOST_NAME = 343,
    DATA_ID_MQTT_HOST = 344,
    DATA_ID_MQTT_PORT = 345,
    DATA_ID_MQTT_USERNAME = 346,
    DATA_ID_MQTT_PASSWORD = 347,
    DATA_ID_MQTT_PERIOD = 348,
    DATA_ID_DEBUG_TRACE_LOG_IS_STOPPED = 349,
    DATA_ID_DLOG_VIEW_SHOW_LEGEND = 350,
    DATA_ID_DLOG_VIEW_SHOW_LABELS = 351,
    DATA_ID_IS_ANY_COUPLING_ALLOWED = 352,
    DATA_ID_IS_COUPLING_SPLIT_RAILS_ALLOWED = 353,
    DATA_ID_LIST_COUNTER_LABEL = 354,
    DATA_ID_MODULE_SPECIFIC_CH_SETTINGS = 355,
    DATA_ID_CHANNEL_FIRMWARE_VERSION = 356,
    DATA_ID_FILE_MANAGER_BROWSER_TITLE = 357,
    DATA_ID_CHANNEL_HAS_ERROR_SETTINGS = 358,
    DATA_ID_CHANNEL_SETTINGS_PAGE = 359,
    DATA_ID_IS_RESET_BY_IWDG = 360,
    DATA_ID_DLOG_ENABLED = 361,
    DATA_ID_FILE_MANAGER_BROWSER_IS_SAVE_DIALOG = 362
};

void data_none(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_edit_enabled(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_channels(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_channel_status(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_channel_output_state(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_channel_is_cc(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_channel_is_cv(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_channel_u_set(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_channel_u_mon(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_channel_u_mon_snapshot(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_channel_u_mon_dac(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_channel_u_limit(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_channel_u_edit(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_channel_i_set(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_channel_i_mon(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_channel_i_mon_snapshot(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_channel_i_mon_dac(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_channel_i_limit(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_channel_i_edit(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_channel_p_mon(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_channel_p_mon_snapshot(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_channel_other_value_mon(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_channels_is_max_view(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_channels_view_mode(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_channels_view_mode_in_default(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_channels_view_mode_in_max(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_slot_max_channel_index(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_slot_max_view(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_slot_min1_channel_index(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_slot_min1_view(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_slot_min2_channel_index(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_slot_min2_view(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_slot_micro1_view(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_slot_micro2_view(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_slot_micro3_view(DataOperationEnum operation, Cursor &cursor, Value &value);
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
void data_edit_mode_interactive_mode_selector(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_edit_steps(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_master_info(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_master_test_result(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_firmware_info(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_self_test_result(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_keypad_text(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_keypad_mode(DataOperationEnum operation, Cursor &cursor, Value &value);
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
void data_channel_protection_status(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_channel_protection_ovp_state(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_channel_protection_ovp_type(DataOperationEnum operation, Cursor &cursor, Value &value);
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
void data_channel_tracking_is_enabled(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_channel_tracking_is_allowed(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_channel_is_par_ser_coupled_or_tracked(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_coupling_type(DataOperationEnum operation, Cursor &cursor, Value &value);
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
void data_profiles_list(DataOperationEnum operation, Cursor &cursor, Value &value);
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
void data_mqtt_enabled(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_ethernet_status(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_mqtt_connection_state(DataOperationEnum operation, Cursor &cursor, Value &value);
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
void data_sys_sound_enabled(DataOperationEnum operation, Cursor &cursor, Value &value);
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
void data_touch_calibration_point(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_selected_theme(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_channel_title(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_channel_short_title(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_channel_short_title_with_colon(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_channel_short_title_without_tracking_icon(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_channel_long_title(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_animations_duration(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_slot1_info(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_slot2_info(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_slot3_info(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_slot1_test_result(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_slot2_test_result(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_slot3_test_result(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_slot_default1_view(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_slot_default2_view(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_slot_default3_view(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_slot1_channel_index(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_slot2_channel_index(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_slot3_channel_index(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_slot_def_2ch_view(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_slot_max_2ch_view(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_slot_min_2ch_view(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_slot_micro_2ch_view(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_slot_2ch_ch1_index(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_slot_2ch_ch2_index(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_battery(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_io_pin_state(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_channel_dprog(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_channel_dprog_installed(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_sys_info_sdcard_status(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_sys_info_has_error(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_sys_info_battery_status(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_sys_fan_mode(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_sys_fan_speed(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_is_coupling_type_uncoupled(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_is_coupling_type_parallel(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_is_coupling_type_common_gnd(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_is_coupling_type_split_rails(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_is_tracking_allowed(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_is_coupling_parallel_allowed(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_is_coupling_series_allowed(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_channel_coupling_enable_tracking_mode(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_channel_active_coupled_led(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_is_coupling_type_series(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_is_coupled_or_tracked(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_overlay(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_channels_with_list_counter_visible(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_dlog_state(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_dlog_toggle_state(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_recording(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_channel_history_values(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_recording_ready(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_nondrag_overlay(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_dlog_single_value_overlay(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_dlog_multiple_values_overlay(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_dlog_visible_values(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_dlog_visible_value_label(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_dlog_visible_value_div(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_dlog_visible_value_offset(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_dlog_visible_value_cursor(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_dlog_all_values(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_dlog_value_label(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_dlog_current_time(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_dlog_file_length(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_dlog_x_axis_offset(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_dlog_x_axis_div(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_dlog_x_axis_max_value(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_dlog_x_axis_max_value_label(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_is_single_page_on_stack(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_is_show_live_recording(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_dlog_view_state(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_dlog_period(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_dlog_duration(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_dlog_voltage_enabled(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_dlog_current_enabled(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_dlog_power_enabled(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_dlog_start_enabled(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_dlog_file_name(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_dlog_value_state(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_slot_max_2ch_min_view(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_channel_off_label(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_file_manager_files(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_file_manager_file_name(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_file_manager_file_date_time(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_file_manager_file_size(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_file_manager_file_selected(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_file_manager_state(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_file_manager_is_root_directory(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_file_manager_current_directory(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_file_manager_is_directory(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_file_manager_file_type(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_file_manager_open_file_enabled(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_file_manager_upload_file_enabled(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_file_manager_rename_file_enabled(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_file_manager_delete_file_enabled(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_file_manager_opened_image(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_script_info(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_script_is_started(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_debug_trace_log(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_debug_trace_log_line(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_keypad_ok_enabled(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_ethernet_host_name(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_mqtt_host(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_mqtt_port(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_mqtt_username(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_mqtt_password(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_mqtt_period(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_debug_trace_log_is_stopped(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_dlog_view_show_legend(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_dlog_view_show_labels(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_is_any_coupling_allowed(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_is_coupling_split_rails_allowed(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_list_counter_label(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_module_specific_ch_settings(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_channel_firmware_version(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_file_manager_browser_title(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_channel_has_error_settings(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_channel_settings_page(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_is_reset_by_iwdg(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_dlog_enabled(DataOperationEnum operation, Cursor &cursor, Value &value);
void data_file_manager_browser_is_save_dialog(DataOperationEnum operation, Cursor &cursor, Value &value);

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
    ACTION_ID_TOGGLE_KEYPAD_MODE = 14,
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
    ACTION_ID_SHOW_SYS_SETTINGS = 31,
    ACTION_ID_SHOW_SYS_SETTINGS_TRIGGER = 32,
    ACTION_ID_SHOW_SYS_SETTINGS_IO = 33,
    ACTION_ID_SHOW_SYS_SETTINGS_DATE_TIME = 34,
    ACTION_ID_SHOW_SYS_SETTINGS_CAL = 35,
    ACTION_ID_SHOW_SYS_SETTINGS_CAL_CH = 36,
    ACTION_ID_SHOW_SYS_SETTINGS_SCREEN_CALIBRATION = 37,
    ACTION_ID_SHOW_SYS_SETTINGS_DISPLAY = 38,
    ACTION_ID_SHOW_SYS_SETTINGS_SERIAL = 39,
    ACTION_ID_SHOW_SYS_SETTINGS_ETHERNET = 40,
    ACTION_ID_SHOW_SYS_SETTINGS_PROTECTIONS = 41,
    ACTION_ID_SHOW_SYS_SETTINGS_AUX_OTP = 42,
    ACTION_ID_SHOW_SYS_SETTINGS_SOUND = 43,
    ACTION_ID_SHOW_SYS_SETTINGS_ENCODER = 44,
    ACTION_ID_SHOW_SYS_INFO = 45,
    ACTION_ID_SHOW_MAIN_HELP_PAGE = 46,
    ACTION_ID_SHOW_EDIT_MODE_STEP_HELP = 47,
    ACTION_ID_SHOW_EDIT_MODE_SLIDER_HELP = 48,
    ACTION_ID_SHOW_CH_SETTINGS = 49,
    ACTION_ID_SHOW_CH_SETTINGS_PROT_CLEAR = 50,
    ACTION_ID_SHOW_CH_SETTINGS_PROT_OCP = 51,
    ACTION_ID_SHOW_CH_SETTINGS_PROT_OVP = 52,
    ACTION_ID_SHOW_CH_SETTINGS_PROT_OPP = 53,
    ACTION_ID_SHOW_CH_SETTINGS_PROT_OTP = 54,
    ACTION_ID_SHOW_CH_SETTINGS_TRIGGER = 55,
    ACTION_ID_SHOW_CH_SETTINGS_LISTS = 56,
    ACTION_ID_SHOW_CH_SETTINGS_ADV_OPTIONS = 57,
    ACTION_ID_SHOW_CH_SETTINGS_ADV_RANGES = 58,
    ACTION_ID_SHOW_CH_SETTINGS_ADV_VIEW = 59,
    ACTION_ID_SHOW_CH_SETTINGS_INFO = 60,
    ACTION_ID_SHOW_CH_SETTINGS_CAL = 61,
    ACTION_ID_SYS_SETTINGS_CAL_EDIT_PASSWORD = 62,
    ACTION_ID_SYS_SETTINGS_CAL_CH_WIZ_START = 63,
    ACTION_ID_SYS_SETTINGS_CAL_CH_WIZ_STEP_PREVIOUS = 64,
    ACTION_ID_SYS_SETTINGS_CAL_CH_WIZ_STEP_NEXT = 65,
    ACTION_ID_SYS_SETTINGS_CAL_CH_WIZ_STOP_AND_SHOW_PREVIOUS_PAGE = 66,
    ACTION_ID_SYS_SETTINGS_CAL_CH_WIZ_STOP_AND_SHOW_MAIN_PAGE = 67,
    ACTION_ID_SYS_SETTINGS_CAL_CH_WIZ_STEP_SET = 68,
    ACTION_ID_SYS_SETTINGS_CAL_CH_WIZ_STEP_SET_LEVEL_VALUE = 69,
    ACTION_ID_SYS_SETTINGS_CAL_CH_WIZ_SAVE = 70,
    ACTION_ID_SYS_SETTINGS_CAL_TOGGLE_ENABLE = 71,
    ACTION_ID_CH_SETTINGS_PROT_CLEAR = 72,
    ACTION_ID_CH_SETTINGS_PROT_CLEAR_AND_DISABLE = 73,
    ACTION_ID_CH_SETTINGS_PROT_TOGGLE_STATE = 74,
    ACTION_ID_CH_SETTINGS_PROT_TOGGLE_TYPE = 75,
    ACTION_ID_CH_SETTINGS_PROT_EDIT_LIMIT = 76,
    ACTION_ID_CH_SETTINGS_PROT_EDIT_LEVEL = 77,
    ACTION_ID_CH_SETTINGS_PROT_EDIT_DELAY = 78,
    ACTION_ID_SET = 79,
    ACTION_ID_DISCARD = 80,
    ACTION_ID_EDIT_FIELD = 81,
    ACTION_ID_EVENT_QUEUE_PREVIOUS_PAGE = 82,
    ACTION_ID_EVENT_QUEUE_NEXT_PAGE = 83,
    ACTION_ID_CH_SETTINGS_ADV_REMOTE_TOGGLE_SENSE = 84,
    ACTION_ID_CH_SETTINGS_ADV_REMOTE_TOGGLE_PROGRAMMING = 85,
    ACTION_ID_DATE_TIME_SELECT_DST_RULE = 86,
    ACTION_ID_SHOW_USER_PROFILES = 87,
    ACTION_ID_SHOW_USER_PROFILE_SETTINGS = 88,
    ACTION_ID_PROFILES_TOGGLE_AUTO_RECALL = 89,
    ACTION_ID_PROFILE_TOGGLE_IS_AUTO_RECALL_LOCATION = 90,
    ACTION_ID_PROFILE_RECALL = 91,
    ACTION_ID_PROFILE_SAVE = 92,
    ACTION_ID_PROFILE_DELETE = 93,
    ACTION_ID_PROFILE_EDIT_REMARK = 94,
    ACTION_ID_TOGGLE_CHANNELS_VIEW_MODE = 95,
    ACTION_ID_ETHERNET_TOGGLE = 96,
    ACTION_ID_MQTT_TOGGLE = 97,
    ACTION_ID_ETHERNET_TOGGLE_DHCP = 98,
    ACTION_ID_ETHERNET_EDIT_MAC_ADDRESS = 99,
    ACTION_ID_ETHERNET_EDIT_STATIC_ADDRESS = 100,
    ACTION_ID_ETHERNET_EDIT_IP_ADDRESS = 101,
    ACTION_ID_ETHERNET_EDIT_DNS = 102,
    ACTION_ID_ETHERNET_EDIT_GATEWAY = 103,
    ACTION_ID_ETHERNET_EDIT_SUBNET_MASK = 104,
    ACTION_ID_ETHERNET_EDIT_SCPI_PORT = 105,
    ACTION_ID_SYS_SETTINGS_PROTECTIONS_TOGGLE_OUTPUT_PROTECTION_COUPLE = 106,
    ACTION_ID_SYS_SETTINGS_PROTECTIONS_TOGGLE_SHUTDOWN_WHEN_PROTECTION_TRIPPED = 107,
    ACTION_ID_SYS_SETTINGS_PROTECTIONS_TOGGLE_FORCE_DISABLING_ALL_OUTPUTS_ON_POWER_UP = 108,
    ACTION_ID_SYS_SETTINGS_PROTECTIONS_AUX_OTP_TOGGLE_STATE = 109,
    ACTION_ID_SYS_SETTINGS_PROTECTIONS_AUX_OTP_EDIT_LEVEL = 110,
    ACTION_ID_SYS_SETTINGS_PROTECTIONS_AUX_OTP_EDIT_DELAY = 111,
    ACTION_ID_SYS_SETTINGS_PROTECTIONS_AUX_OTP_CLEAR = 112,
    ACTION_ID_EDIT_SYSTEM_PASSWORD = 113,
    ACTION_ID_SYS_FRONT_PANEL_LOCK = 114,
    ACTION_ID_SYS_FRONT_PANEL_UNLOCK = 115,
    ACTION_ID_SYS_SETTINGS_SOUND_TOGGLE = 116,
    ACTION_ID_SYS_SETTINGS_SOUND_TOGGLE_CLICK = 117,
    ACTION_ID_CH_SETTINGS_ADV_VIEW_EDIT_DISPLAY_VALUE1 = 118,
    ACTION_ID_CH_SETTINGS_ADV_VIEW_EDIT_DISPLAY_VALUE2 = 119,
    ACTION_ID_CH_SETTINGS_ADV_VIEW_SWAP_DISPLAY_VALUES = 120,
    ACTION_ID_CH_SETTINGS_ADV_VIEW_EDIT_YTVIEW_RATE = 121,
    ACTION_ID_SYS_SETTINGS_ENCODER_TOGGLE_CONFIRMATION_MODE = 122,
    ACTION_ID_TURN_DISPLAY_OFF = 123,
    ACTION_ID_CH_SETTINGS_TRIGGER_EDIT_TRIGGER_MODE = 124,
    ACTION_ID_CH_SETTINGS_TRIGGER_EDIT_VOLTAGE_TRIGGER_VALUE = 125,
    ACTION_ID_CH_SETTINGS_TRIGGER_EDIT_CURRENT_TRIGGER_VALUE = 126,
    ACTION_ID_CH_SETTINGS_TRIGGER_EDIT_LIST_COUNT = 127,
    ACTION_ID_CH_SETTINGS_TRIGGER_EDIT_ON_LIST_STOP = 128,
    ACTION_ID_CHANNEL_LISTS_PREVIOUS_PAGE = 129,
    ACTION_ID_CHANNEL_LISTS_NEXT_PAGE = 130,
    ACTION_ID_CHANNEL_LISTS_EDIT = 131,
    ACTION_ID_SHOW_CHANNEL_LISTS_INSERT_MENU = 132,
    ACTION_ID_SHOW_CHANNEL_LISTS_DELETE_MENU = 133,
    ACTION_ID_CHANNEL_LISTS_INSERT_ROW_ABOVE = 134,
    ACTION_ID_CHANNEL_LISTS_INSERT_ROW_BELOW = 135,
    ACTION_ID_CHANNEL_LISTS_DELETE_ROW = 136,
    ACTION_ID_CHANNEL_LISTS_CLEAR_COLUMN = 137,
    ACTION_ID_CHANNEL_LISTS_DELETE_ROWS = 138,
    ACTION_ID_CHANNEL_LISTS_DELETE_ALL = 139,
    ACTION_ID_CHANNEL_INITIATE_TRIGGER = 140,
    ACTION_ID_CHANNEL_SET_TO_FIXED = 141,
    ACTION_ID_CHANNEL_ENABLE_OUTPUT = 142,
    ACTION_ID_TRIGGER_SELECT_SOURCE = 143,
    ACTION_ID_TRIGGER_EDIT_DELAY = 144,
    ACTION_ID_TRIGGER_TOGGLE_INITIATE_CONTINUOUSLY = 145,
    ACTION_ID_TRIGGER_GENERATE_MANUAL = 146,
    ACTION_ID_TRIGGER_SHOW_GENERAL_SETTINGS = 147,
    ACTION_ID_SHOW_STAND_BY_MENU = 148,
    ACTION_ID_RESET = 149,
    ACTION_ID_CH_SETTINGS_ADV_RANGES_SELECT_MODE = 150,
    ACTION_ID_CH_SETTINGS_ADV_RANGES_TOGGLE_AUTO_RANGING = 151,
    ACTION_ID_IO_PIN_TOGGLE_POLARITY = 152,
    ACTION_ID_IO_PIN_SELECT_FUNCTION = 153,
    ACTION_ID_SERIAL_TOGGLE = 154,
    ACTION_ID_SERIAL_SELECT_PARITY = 155,
    ACTION_ID_NTP_TOGGLE = 156,
    ACTION_ID_NTP_EDIT_SERVER = 157,
    ACTION_ID_SELECT_THEME = 158,
    ACTION_ID_TOGGLE_CHANNELS_MAX_VIEW = 159,
    ACTION_ID_EDIT_ANIMATIONS_DURATION = 160,
    ACTION_ID_RESTART = 161,
    ACTION_ID_USER_SWITCH_CLICKED = 162,
    ACTION_ID_IO_PIN_TOGGLE_STATE = 163,
    ACTION_ID_CH_SETTINGS_ADV_TOGGLE_DPROG = 164,
    ACTION_ID_SYS_SETTINGS_FAN_TOGGLE_MODE = 165,
    ACTION_ID_SYS_SETTINGS_FAN_EDIT_SPEED = 166,
    ACTION_ID_SHOW_SYS_SETTINGS_TRACKING = 167,
    ACTION_ID_SHOW_SYS_SETTINGS_COUPLING = 168,
    ACTION_ID_SET_COUPLING_UNCOUPLED = 169,
    ACTION_ID_SET_COUPLING_PARALLEL = 170,
    ACTION_ID_SET_COUPLING_SERIES = 171,
    ACTION_ID_SET_COUPLING_COMMON_GND = 172,
    ACTION_ID_SET_COUPLING_SPLIT_RAILS = 173,
    ACTION_ID_TOGGLE_CHANNEL_TRACKING = 174,
    ACTION_ID_TOGGLE_ENABLE_TRACKING_MODE_IN_COUPLING = 175,
    ACTION_ID_DRAG_OVERLAY = 176,
    ACTION_ID_DLOG_TOGGLE = 177,
    ACTION_ID_SHOW_DLOG_VIEW = 178,
    ACTION_ID_SELECT_USER_SWITCH_ACTION = 179,
    ACTION_ID_SHOW_FILE_MANAGER = 180,
    ACTION_ID_FILE_MANAGER_SELECT_FILE = 181,
    ACTION_ID_FILE_MANAGER_GO_TO_PARENT_DIRECTORY = 182,
    ACTION_ID_FILE_MANAGER_OPEN_FILE = 183,
    ACTION_ID_FILE_MANAGER_UPLOAD_FILE = 184,
    ACTION_ID_FILE_MANAGER_RENAME_FILE = 185,
    ACTION_ID_FILE_MANAGER_DELETE_FILE = 186,
    ACTION_ID_SHOW_DLOG_PARAMS = 187,
    ACTION_ID_DLOG_VOLTAGE_TOGGLE = 188,
    ACTION_ID_DLOG_CURRENT_TOGGLE = 189,
    ACTION_ID_DLOG_POWER_TOGGLE = 190,
    ACTION_ID_DLOG_VALUE_TOGGLE = 191,
    ACTION_ID_DLOG_EDIT_PERIOD = 192,
    ACTION_ID_DLOG_EDIT_DURATION = 193,
    ACTION_ID_DLOG_EDIT_FILE_NAME = 194,
    ACTION_ID_DLOG_START_RECORDING = 195,
    ACTION_ID_DLOG_VIEW_SHOW_OVERLAY_OPTIONS = 196,
    ACTION_ID_FILE_MANAGER_SORT_BY = 197,
    ACTION_ID_DLOG_AUTO_SCALE = 198,
    ACTION_ID_SHOW_DEBUG_TRACE_LOG = 199,
    ACTION_ID_DLOG_UPLOAD = 200,
    ACTION_ID_ETHERNET_EDIT_HOST_NAME = 201,
    ACTION_ID_MQTT_EDIT_HOST = 202,
    ACTION_ID_MQTT_EDIT_PORT = 203,
    ACTION_ID_MQTT_EDIT_USERNAME = 204,
    ACTION_ID_MQTT_EDIT_PASSWORD = 205,
    ACTION_ID_MQTT_EDIT_PERIOD = 206,
    ACTION_ID_SHOW_SYS_SETTINGS_MQTT = 207,
    ACTION_ID_DEBUG_TRACE_LOG_TOGGLE = 208,
    ACTION_ID_DLOG_SCALE_TO_FIT = 209,
    ACTION_ID_DLOG_VIEW_TOGGLE_LEGEND = 210,
    ACTION_ID_DLOG_VIEW_TOGGLE_LABELS = 211,
    ACTION_ID_DLOG_VIEW_SELECT_VISIBLE_VALUE = 212,
    ACTION_ID_CHANNEL_UPDATE_FIRMWARE = 213,
    ACTION_ID_SHUTDOWN = 214,
    ACTION_ID_SHOW_CHANNEL_LISTS_FILE_MENU = 215,
    ACTION_ID_CHANNEL_LISTS_FILE_OPEN = 216,
    ACTION_ID_CHANNEL_LISTS_FILE_SAVE = 217,
    ACTION_ID_FILE_MANAGER_NEW_FILE = 218,
    ACTION_ID_PROFILE_IMPORT = 219,
    ACTION_ID_PROFILE_EXPORT = 220
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
void action_toggle_keypad_mode();
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
void action_show_main_help_page();
void action_show_edit_mode_step_help();
void action_show_edit_mode_slider_help();
void action_show_ch_settings();
void action_show_ch_settings_prot_clear();
void action_show_ch_settings_prot_ocp();
void action_show_ch_settings_prot_ovp();
void action_show_ch_settings_prot_opp();
void action_show_ch_settings_prot_otp();
void action_show_ch_settings_trigger();
void action_show_ch_settings_lists();
void action_show_ch_settings_adv_options();
void action_show_ch_settings_adv_ranges();
void action_show_ch_settings_adv_view();
void action_show_ch_settings_info();
void action_show_ch_settings_cal();
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
void action_ch_settings_prot_toggle_type();
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
void action_show_user_profile_settings();
void action_profiles_toggle_auto_recall();
void action_profile_toggle_is_auto_recall_location();
void action_profile_recall();
void action_profile_save();
void action_profile_delete();
void action_profile_edit_remark();
void action_toggle_channels_view_mode();
void action_ethernet_toggle();
void action_mqtt_toggle();
void action_ethernet_toggle_dhcp();
void action_ethernet_edit_mac_address();
void action_ethernet_edit_static_address();
void action_ethernet_edit_ip_address();
void action_ethernet_edit_dns();
void action_ethernet_edit_gateway();
void action_ethernet_edit_subnet_mask();
void action_ethernet_edit_scpi_port();
void action_sys_settings_protections_toggle_output_protection_couple();
void action_sys_settings_protections_toggle_shutdown_when_protection_tripped();
void action_sys_settings_protections_toggle_force_disabling_all_outputs_on_power_up();
void action_sys_settings_protections_aux_otp_toggle_state();
void action_sys_settings_protections_aux_otp_edit_level();
void action_sys_settings_protections_aux_otp_edit_delay();
void action_sys_settings_protections_aux_otp_clear();
void action_edit_system_password();
void action_sys_front_panel_lock();
void action_sys_front_panel_unlock();
void action_sys_settings_sound_toggle();
void action_sys_settings_sound_toggle_click();
void action_ch_settings_adv_view_edit_display_value1();
void action_ch_settings_adv_view_edit_display_value2();
void action_ch_settings_adv_view_swap_display_values();
void action_ch_settings_adv_view_edit_ytview_rate();
void action_sys_settings_encoder_toggle_confirmation_mode();
void action_turn_display_off();
void action_ch_settings_trigger_edit_trigger_mode();
void action_ch_settings_trigger_edit_voltage_trigger_value();
void action_ch_settings_trigger_edit_current_trigger_value();
void action_ch_settings_trigger_edit_list_count();
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
void action_select_theme();
void action_toggle_channels_max_view();
void action_edit_animations_duration();
void action_restart();
void action_user_switch_clicked();
void action_io_pin_toggle_state();
void action_ch_settings_adv_toggle_dprog();
void action_sys_settings_fan_toggle_mode();
void action_sys_settings_fan_edit_speed();
void action_show_sys_settings_tracking();
void action_show_sys_settings_coupling();
void action_set_coupling_uncoupled();
void action_set_coupling_parallel();
void action_set_coupling_series();
void action_set_coupling_common_gnd();
void action_set_coupling_split_rails();
void action_toggle_channel_tracking();
void action_toggle_enable_tracking_mode_in_coupling();
void action_drag_overlay();
void action_dlog_toggle();
void action_show_dlog_view();
void action_select_user_switch_action();
void action_show_file_manager();
void action_file_manager_select_file();
void action_file_manager_go_to_parent_directory();
void action_file_manager_open_file();
void action_file_manager_upload_file();
void action_file_manager_rename_file();
void action_file_manager_delete_file();
void action_show_dlog_params();
void action_dlog_voltage_toggle();
void action_dlog_current_toggle();
void action_dlog_power_toggle();
void action_dlog_value_toggle();
void action_dlog_edit_period();
void action_dlog_edit_duration();
void action_dlog_edit_file_name();
void action_dlog_start_recording();
void action_dlog_view_show_overlay_options();
void action_file_manager_sort_by();
void action_dlog_auto_scale();
void action_show_debug_trace_log();
void action_dlog_upload();
void action_ethernet_edit_host_name();
void action_mqtt_edit_host();
void action_mqtt_edit_port();
void action_mqtt_edit_username();
void action_mqtt_edit_password();
void action_mqtt_edit_period();
void action_show_sys_settings_mqtt();
void action_debug_trace_log_toggle();
void action_dlog_scale_to_fit();
void action_dlog_view_toggle_legend();
void action_dlog_view_toggle_labels();
void action_dlog_view_select_visible_value();
void action_channel_update_firmware();
void action_shutdown();
void action_show_channel_lists_file_menu();
void action_channel_lists_file_open();
void action_channel_lists_file_save();
void action_file_manager_new_file();
void action_profile_import();
void action_profile_export();

extern ActionExecFunc g_actionExecFunctions[];

enum FontsEnum {
    FONT_ID_NONE = 0,
    FONT_ID_SHADOW = 1,
    FONT_ID_OSWALD14 = 2,
    FONT_ID_OSWALD17 = 3,
    FONT_ID_OSWALD24 = 4,
    FONT_ID_OSWALD20 = 5,
    FONT_ID_HEYDINGS14 = 6,
    FONT_ID_WEB_HOSTING_HUB24 = 7,
    FONT_ID_OSWALD48 = 8,
    FONT_ID_OSWALD38 = 9,
    FONT_ID_ROBOTO_CONDENSED_REGULAR = 10
};

enum BitmapsEnum {
    BITMAP_ID_NONE = 0,
    BITMAP_ID_PSU_ICON = 1,
    BITMAP_ID_SETTINGS_ICON = 2,
    BITMAP_ID_LOGO = 3,
    BITMAP_ID_PROTECTION_CURVE = 4,
    BITMAP_ID_VOUT = 5,
    BITMAP_ID_PROTECTION_HW_TRIP_LEVEL = 6,
    BITMAP_ID_VSET = 7,
    BITMAP_ID_PROTECTION_TRIP_LEVEL = 8,
    BITMAP_ID_PROTECTION_DELAY = 9,
    BITMAP_ID_IOUT = 10,
    BITMAP_ID_POUT = 11,
    BITMAP_ID_COUPLED_PARALLEL = 12,
    BITMAP_ID_COUPLED_SERIES = 13,
    BITMAP_ID_COUPLED_CGND = 14,
    BITMAP_ID_COUPLED_SPLIT_RAILS = 15
};

enum StylesEnum {
    STYLE_ID_NONE = 0,
    STYLE_ID_ERROR_ALERT = 1,
    STYLE_ID_ERROR_ALERT_BUTTON = 2,
    STYLE_ID_YT_GRAPH_U_DEFAULT = 3,
    STYLE_ID_YT_GRAPH_I_DEFAULT = 4,
    STYLE_ID_YT_GRAPH_P_DEFAULT = 5,
    STYLE_ID_YT_GRAPH_UNREGULATED = 6,
    STYLE_ID_YT_GRAPH_Y1 = 7,
    STYLE_ID_YT_GRAPH_Y2 = 8,
    STYLE_ID_YT_GRAPH_Y3 = 9,
    STYLE_ID_YT_GRAPH_Y4 = 10,
    STYLE_ID_YT_GRAPH_Y5 = 11,
    STYLE_ID_SELECT_ENUM_ITEM_POPUP_CONTAINER = 12,
    STYLE_ID_SELECT_ENUM_ITEM_POPUP_CONTAINER_S = 13,
    STYLE_ID_SELECT_ENUM_ITEM_POPUP_ITEM = 14,
    STYLE_ID_SELECT_ENUM_ITEM_POPUP_ITEM_S = 15,
    STYLE_ID_SELECT_ENUM_ITEM_POPUP_DISABLED_ITEM = 16,
    STYLE_ID_SELECT_ENUM_ITEM_POPUP_DISABLED_ITEM_S = 17,
    STYLE_ID_INFO_ALERT = 18,
    STYLE_ID_MENU_WITH_BUTTONS_MESSAGE = 19,
    STYLE_ID_MENU_WITH_BUTTONS_CONTAINER = 20,
    STYLE_ID_MENU_WITH_BUTTONS_BUTTON = 21,
    STYLE_ID_CHANNEL1 = 22,
    STYLE_ID_CHANNEL2 = 23,
    STYLE_ID_CHANNEL3 = 24,
    STYLE_ID_CHANNEL4 = 25,
    STYLE_ID_CHANNEL5 = 26,
    STYLE_ID_CHANNEL6 = 27,
    STYLE_ID_CHANNEL1_INVERSE = 28,
    STYLE_ID_CHANNEL2_INVERSE = 29,
    STYLE_ID_CHANNEL3_INVERSE = 30,
    STYLE_ID_CHANNEL4_INVERSE = 31,
    STYLE_ID_CHANNEL5_INVERSE = 32,
    STYLE_ID_CHANNEL6_INVERSE = 33,
    STYLE_ID_DEFAULT = 34,
    STYLE_ID_STATUS_BGND = 35,
    STYLE_ID_STATUS_ICON_ENABLED_AND_ERROR = 36,
    STYLE_ID_STATUS_ICON_ENABLED = 37,
    STYLE_ID_DEFAULT_S_LEFT = 38,
    STYLE_ID_DEFAULT_S = 39,
    STYLE_ID_TOUCH_CALIBRATION = 40,
    STYLE_ID_BOTTOM_BUTTON_BACKGROUND = 41,
    STYLE_ID_BOTTOM_BUTTON = 42,
    STYLE_ID_STATUS_ICON_ENABLED_WITH_NEED_ATTENTION = 43,
    STYLE_ID_STATUS_ICON_ENABLED_AND_ACTIVE = 44,
    STYLE_ID_STATUS_ICON_DISABLED = 45,
    STYLE_ID_OVERLAY = 46,
    STYLE_ID_EDIT_VALUE_S_LEFT = 47,
    STYLE_ID_VALUE_S = 48,
    STYLE_ID_EDIT_VALUE_M_LEFT = 49,
    STYLE_ID_KEY_ICONS = 50,
    STYLE_ID_KEY = 51,
    STYLE_ID_TAB_PAGE_SELECTED = 52,
    STYLE_ID_TAB_PAGE = 53,
    STYLE_ID_DEFAULT_INVERSE = 54,
    STYLE_ID_NON_INTERACTIVE_BUTTON_S = 55,
    STYLE_ID_SLIDER_SHADE_1 = 56,
    STYLE_ID_SLIDER_SHADE_2 = 57,
    STYLE_ID_SLIDER_SHADE_3 = 58,
    STYLE_ID_SLIDER_SHADE_4 = 59,
    STYLE_ID_SLIDER_SHADE_5 = 60,
    STYLE_ID_YES_NO = 61,
    STYLE_ID_YES_NO_BUTTON = 62,
    STYLE_ID_YES_NO_MESSAGE = 63,
    STYLE_ID_TEXT_MESSAGE = 64,
    STYLE_ID_ASYNC_OPERATION = 65,
    STYLE_ID_EVENT_INFO = 66,
    STYLE_ID_EVENT_WARNING = 67,
    STYLE_ID_EVENT_ERROR = 68,
    STYLE_ID_BOTTOM_BUTTON_TEXTUAL_S_LEFT = 69,
    STYLE_ID_BOTTOM_BUTTON_DISABLED = 70,
    STYLE_ID_KEY_SPEC_ICONS = 71,
    STYLE_ID_KEY_SPEC_ICONS_DISABLED = 72,
    STYLE_ID_KEY_DISABLED = 73,
    STYLE_ID_EDIT_VALUE_S_CENTERED = 74,
    STYLE_ID_EDIT_VALUE_ACTIVE_S_CENTER = 75,
    STYLE_ID_EDIT_VALUE_ACTIVE_M_CENTER = 76,
    STYLE_ID_VALUE_M = 77,
    STYLE_ID_DEFAULT_DISABLED_M_LEFT = 78,
    STYLE_ID_YT_GRAPH = 79,
    STYLE_ID_LIST_GRAPH_CURSOR = 80,
    STYLE_ID_YT_GRAPH_U_DEFAULT_LABEL = 81,
    STYLE_ID_YT_GRAPH_I_DEFAULT_LABEL = 82,
    STYLE_ID_EDIT_VALUE_FOCUS_S_CENTER = 83,
    STYLE_ID_OPTION_TOGGLE_L_CENTER = 84,
    STYLE_ID_DEFAULT_M_LEFT = 85,
    STYLE_ID_DEFAULT_DISABLED_S_LEFT = 86,
    STYLE_ID_ICON_AND_TEXT_S = 87,
    STYLE_ID_ICON_AND_TEXT_S_DISABLED = 88,
    STYLE_ID_NON_INTERACTIVE_BUTTON_M = 89,
    STYLE_ID_DEFAULT_DISABLED = 90,
    STYLE_ID_EDIT_VALUE_M_CENTERED = 91,
    STYLE_ID_EDIT_S = 92,
    STYLE_ID_VALUE = 93,
    STYLE_ID_DEFAULT_M = 94,
    STYLE_ID_EDIT_VALUE_ACTIVE_S_LEFT = 95,
    STYLE_ID_DISPLAY_OFF = 96,
    STYLE_ID_DISPLAY_OFF_S = 97,
    STYLE_ID_DEFAULT_GUI_TMPL = 98,
    STYLE_ID_MON_VALUE_48_RIGHT = 99,
    STYLE_ID_MON_VALUE_20_LEFT = 100,
    STYLE_ID_ENCODER_CURSOR_14_DISABLED = 101,
    STYLE_ID_ENCODER_CURSOR_14_ENABLED = 102,
    STYLE_ID_ENCODER_CURSOR_14_FOCUSED = 103,
    STYLE_ID_BAR_GRAPH_U_DEFAULT = 104,
    STYLE_ID_BAR_GRAPH_TEXT = 105,
    STYLE_ID_BAR_GRAPH_LIMIT_LINE = 106,
    STYLE_ID_BAR_GRAPH_I_DEFAULT = 107,
    STYLE_ID_TRIGGER_BUTTON_24 = 108,
    STYLE_ID_MON_VALUE_14_RIGHT = 109,
    STYLE_ID_ENCODER_CURSOR_14_RIGHT_DISABLED = 110,
    STYLE_ID_ENCODER_CURSOR_14_RIGHT_ENABLED = 111,
    STYLE_ID_ENCODER_CURSOR_14_RIGHT_FOCUSED = 112,
    STYLE_ID_ENCODER_CURSOR_14_LEFT_DISABLED = 113,
    STYLE_ID_CHANNEL_OFF_BUTTON_20 = 114,
    STYLE_ID_TRIGGER_BUTTON_14 = 115,
    STYLE_ID_CHANNEL_OFF_BUTTON_38 = 116,
    STYLE_ID_INST_SELECTED = 117,
    STYLE_ID_MON_VALUE_38_RIGHT = 118,
    STYLE_ID_CH_PROT_OFF = 119,
    STYLE_ID_CH_PROT_ON = 120,
    STYLE_ID_CH_PROT_TRIPPED = 121,
    STYLE_ID_BUTTON_BGND = 122,
    STYLE_ID_MON_VALUE_24_RIGHT = 123,
    STYLE_ID_MON_VALUE_14_CENTER = 124,
    STYLE_ID_MON_VALUE_20_RIGHT = 125,
    STYLE_ID_MON_VALUE_14_LEFT = 126,
    STYLE_ID_CHANNEL_OFF_BUTTON_14 = 127,
    STYLE_ID_SCROLLBAR_TRACK = 128,
    STYLE_ID_SCROLLBAR_THUMB = 129,
    STYLE_ID_SCROLLBAR_BUTTON = 130,
    STYLE_ID_NON_INTERACTIVE_BUTTON_S_DISABLED = 131
};

enum ThemesEnum {
    THEME_ID_LEGACY = 0,
    THEME_ID_LIGHT = 1,
    THEME_ID_DARK = 2
};

enum ColorsEnum {
    COLOR_ID_BACKGROUND = 0,
    COLOR_ID_TEXT_REGULAR = 1,
    COLOR_ID_TEXT_ENABLED = 2,
    COLOR_ID_TEXT_SELECTED = 3,
    COLOR_ID_TEXT_ENABLE_BACKGROUND = 4,
    COLOR_ID_STATUS_LINE_TEXT = 5,
    COLOR_ID_STATUS_LINE_TEXT_DISABLED = 6,
    COLOR_ID_TEXT_DISABLED_BACKGROUND = 7,
    COLOR_ID_STATUS_LINE_BACKGROUND = 8,
    COLOR_ID_STATUS_OK = 9,
    COLOR_ID_STATUS_WARNING = 10,
    COLOR_ID_INST_MAX_HEADER_BGND = 11,
    COLOR_ID_INST_MIN_HEADER_BGND = 12,
    COLOR_ID_INST_MAX_HEADER_TEXT = 13,
    COLOR_ID_INST_MIN_HEADER_TEXT = 14,
    COLOR_ID_TEXT_SELECTABLE_BACKGROUND = 15,
    COLOR_ID_PROTECTION_TEXT = 16,
    COLOR_ID_TEXT_DISABLED = 17,
    COLOR_ID_BAR_GRAPH_VOLTAGE = 18,
    COLOR_ID_BAR_GRAPH_CURRENT = 19,
    COLOR_ID_BAR_GRAPH_POWER = 20,
    COLOR_ID_BAR_GRAPH_BACKGROUND = 21,
    COLOR_ID_BAR_GRAPH_TEXT = 22,
    COLOR_ID_BAR_GRAPH_TEXT_INSIDE = 23,
    COLOR_ID_BAR_GRAPH_SET_LINE = 24,
    COLOR_ID_ACTION_COLOR = 25,
    COLOR_ID_POPUP_BACKGROUND_REGULAR = 26,
    COLOR_ID_POPUP_TEXT_SELECTED = 27,
    COLOR_ID_POPUP_BACKGROUND_WARNING = 28,
    COLOR_ID_POPUP_BACKGROUND_INFO = 29,
    COLOR_ID_TEXT_EDITED = 30,
    COLOR_ID_TEXT_VALUE = 31,
    COLOR_ID_DATA_LOGGING = 32,
    COLOR_ID_OVERLAY_BACKGROUND = 33,
    COLOR_ID_OVERLAY_DRAG_BACKGROUND = 34,
    COLOR_ID_OVERLAY_TEXT = 35,
    COLOR_ID_CHANNEL1 = 36,
    COLOR_ID_CHANNEL2 = 37,
    COLOR_ID_CHANNEL3 = 38,
    COLOR_ID_CHANNEL4 = 39,
    COLOR_ID_CHANNEL5 = 40,
    COLOR_ID_CHANNEL6 = 41,
    COLOR_ID_SCROLLBAR_TRACK = 42,
    COLOR_ID_SCROLLBAR_THUMB = 43,
    COLOR_ID_SCROLLBAR_BUTTON = 44,
    COLOR_ID_YT_GRAPH_GRID = 45,
    COLOR_ID_YT_GRAPH_BACKGROUND = 46,
    COLOR_ID_YT_GRAPH_CURSOR = 47,
    COLOR_ID_YT_GRAPH_Y1 = 48,
    COLOR_ID_YT_GRAPH_Y2 = 49,
    COLOR_ID_YT_GRAPH_Y3 = 50,
    COLOR_ID_YT_GRAPH_Y4 = 51,
    COLOR_ID_YT_GRAPH_Y5 = 52
};

enum PagesEnum {
    PAGE_ID_A_MAIN = 0,
    PAGE_ID_A_MAIN_1 = 1,
    PAGE_ID_WELCOME = 2,
    PAGE_ID_TOUCH_CALIBRATION_INTRO = 3,
    PAGE_ID_TOUCH_CALIBRATION = 4,
    PAGE_ID_TOUCH_CALIBRATION_YES_NO = 5,
    PAGE_ID_TOUCH_CALIBRATION_YES_NO_CANCEL = 6,
    PAGE_ID_SELF_TEST_RESULT = 7,
    PAGE_ID_MAIN = 8,
    PAGE_ID_MAIN_HELP = 9,
    PAGE_ID_EDIT_MODE_KEYPAD = 10,
    PAGE_ID_EDIT_MODE_STEP = 11,
    PAGE_ID_EDIT_MODE_STEP_HELP = 12,
    PAGE_ID_EDIT_MODE_SLIDER = 13,
    PAGE_ID_EDIT_MODE_SLIDER_HELP = 14,
    PAGE_ID_YES_NO = 15,
    PAGE_ID_ARE_YOU_SURE_WITH_MESSAGE = 16,
    PAGE_ID_YES_NO_LATER = 17,
    PAGE_ID_YES_NO_FLASH_SLAVE = 18,
    PAGE_ID_INFO_RESTART = 19,
    PAGE_ID_TEXT_MESSAGE = 20,
    PAGE_ID_ASYNC_OPERATION_IN_PROGRESS = 21,
    PAGE_ID_PROGRESS = 22,
    PAGE_ID_PROGRESS_WITHOUT_ABORT = 23,
    PAGE_ID_EVENT_QUEUE = 24,
    PAGE_ID_KEYPAD = 25,
    PAGE_ID_NUMERIC_KEYPAD = 26,
    PAGE_ID_CH_SETTINGS = 27,
    PAGE_ID_CH_SETTINGS_OK = 28,
    PAGE_ID_CH_SETTINGS_ERROR = 29,
    PAGE_ID_CH_SETTINGS_ERROR_DCM220 = 30,
    PAGE_ID_CH_SETTINGS_DCP405_SPECIFIC = 31,
    PAGE_ID_CH_SETTINGS_DCP405B_SPECIFIC = 32,
    PAGE_ID_CH_SETTINGS_DCM220_SPECIFIC = 33,
    PAGE_ID_CH_SETTINGS_PROT_CLEAR = 34,
    PAGE_ID_CH_SETTINGS_PROT_OVP = 35,
    PAGE_ID_CH_SETTINGS_PROT_OCP = 36,
    PAGE_ID_CH_SETTINGS_PROT_OPP = 37,
    PAGE_ID_CH_SETTINGS_PROT_OTP = 38,
    PAGE_ID_CH_SETTINGS_TRIGGER = 39,
    PAGE_ID_CH_SETTINGS_LISTS = 40,
    PAGE_ID_CH_SETTINGS_LISTS_INSERT_MENU = 41,
    PAGE_ID_CH_SETTINGS_LISTS_FILE_MENU = 42,
    PAGE_ID_CH_SETTINGS_LISTS_DELETE_MENU = 43,
    PAGE_ID_CH_SETTINGS_CALIBRATION = 44,
    PAGE_ID_CH_START_LIST = 45,
    PAGE_ID_CH_SETTINGS_ADV_OPTIONS = 46,
    PAGE_ID_CH_SETTINGS_ADV_RANGES = 47,
    PAGE_ID_CH_SETTINGS_ADV_VIEW = 48,
    PAGE_ID_CH_SETTINGS_INFO = 49,
    PAGE_ID_SYS_SETTINGS = 50,
    PAGE_ID_SYS_SETTINGS_TEMPERATURE = 51,
    PAGE_ID_SYS_SETTINGS_PROTECTIONS = 52,
    PAGE_ID_SYS_SETTINGS_TRIGGER = 53,
    PAGE_ID_SYS_SETTINGS_IO = 54,
    PAGE_ID_SYS_SETTINGS_DATE_TIME = 55,
    PAGE_ID_SYS_SETTINGS_ENCODER = 56,
    PAGE_ID_SYS_SETTINGS_SERIAL = 57,
    PAGE_ID_SYS_SETTINGS_ETHERNET = 58,
    PAGE_ID_SYS_SETTINGS_MQTT = 59,
    PAGE_ID_SYS_SETTINGS_ETHERNET_STATIC = 60,
    PAGE_ID_SYS_SETTINGS_CAL = 61,
    PAGE_ID_SYS_SETTINGS_CAL_CH = 62,
    PAGE_ID_SYS_SETTINGS_CAL_CH_WIZ_STEP = 63,
    PAGE_ID_SYS_SETTINGS_CAL_CH_WIZ_FINISH = 64,
    PAGE_ID_SYS_SETTINGS_SCREEN_CALIBRATION = 65,
    PAGE_ID_SYS_SETTINGS_DISPLAY = 66,
    PAGE_ID_SYS_SETTINGS_SOUND = 67,
    PAGE_ID_SYS_SETTINGS_TRACKING = 68,
    PAGE_ID_SYS_SETTINGS_COUPLING = 69,
    PAGE_ID_USER_PROFILES = 70,
    PAGE_ID_USER_PROFILE_SETTINGS = 71,
    PAGE_ID_USER_PROFILE_0_SETTINGS = 72,
    PAGE_ID_PROFILE_BASIC_INFO = 73,
    PAGE_ID_SYS_INFO = 74,
    PAGE_ID_STAND_BY_MENU = 75,
    PAGE_ID_ENTERING_STANDBY = 76,
    PAGE_ID_STANDBY = 77,
    PAGE_ID_SAVING = 78,
    PAGE_ID_SHUTDOWN = 79,
    PAGE_ID_DISPLAY_OFF = 80,
    PAGE_ID_MINIMIZED_CHANNELS = 81,
    PAGE_ID_SLOT_DEF_VERT = 82,
    PAGE_ID_SLOT_DEF_VERT_TITLE = 83,
    PAGE_ID_SLOT_DEF_1CH_NUM_ON = 84,
    PAGE_ID_SLOT_DEF_1CH_VBAR_ON = 85,
    PAGE_ID_SLOT_DEF_1CH_VERT_OFF = 86,
    PAGE_ID_SLOT_DEF_1CH_VERT_COUPLED_SERIES = 87,
    PAGE_ID_SLOT_DEF_1CH_VERT_COUPLED_PARALLEL = 88,
    PAGE_ID_SLOT_DEF_2CH_VERT = 89,
    PAGE_ID_SLOT_DEF_2CH_VERT_ON = 90,
    PAGE_ID_SLOT_DEF_2CH_VERT_OFF = 91,
    PAGE_ID_SLOT_DEF_VERT_ERROR = 92,
    PAGE_ID_SLOT_DEF_VERT_NOT_INSTALLED = 93,
    PAGE_ID_SLOT_DEF_HORZ = 94,
    PAGE_ID_SLOT_DEF_HORZ_TITLE = 95,
    PAGE_ID_SLOT_DEF_1CH_HBAR_ON = 96,
    PAGE_ID_SLOT_DEF_1CH_YT_ON = 97,
    PAGE_ID_SLOT_DEF_1CH_HORZ_OFF = 98,
    PAGE_ID_SLOT_DEF_1CH_HORZ_COUPLED_SERIES = 99,
    PAGE_ID_SLOT_DEF_1CH_HORZ_COUPLED_PARALLEL = 100,
    PAGE_ID_SLOT_DEF_2CH_HORZ = 101,
    PAGE_ID_SLOT_DEF_2CH_HORZ_ON = 102,
    PAGE_ID_SLOT_DEF_2CH_HORZ_OFF = 103,
    PAGE_ID_SLOT_DEF_HORZ_ERROR = 104,
    PAGE_ID_SLOT_DEF_HORZ_NOT_INSTALLED = 105,
    PAGE_ID_SLOT_MAX_1CH_TITLE = 106,
    PAGE_ID_SLOT_MAX_1CH_NUM_ON = 107,
    PAGE_ID_SLOT_MAX_1CH_NUM_OFF = 108,
    PAGE_ID_SLOT_MAX_1CH_HBAR_ON = 109,
    PAGE_ID_SLOT_MAX_1CH_HBAR_OFF = 110,
    PAGE_ID_SLOT_MAX_1CH_YT_ON = 111,
    PAGE_ID_SLOT_MAX_1CH_YT_OFF = 112,
    PAGE_ID_SLOT_MAX_2CH = 113,
    PAGE_ID_SLOT_MAX_2CH_TITLE = 114,
    PAGE_ID_SLOT_MAX_2CH_NUM_ON = 115,
    PAGE_ID_SLOT_MAX_2CH_NUM_OFF = 116,
    PAGE_ID_SLOT_MAX_2CH_HBAR_ON = 117,
    PAGE_ID_SLOT_MAX_2CH_HBAR_OFF = 118,
    PAGE_ID_SLOT_MAX_2CH_YT_ON = 119,
    PAGE_ID_SLOT_MAX_2CH_YT_OFF = 120,
    PAGE_ID_SLOT_MAX_2CH_MIN_TITLE = 121,
    PAGE_ID_SLOT_MAX_2CH_MIN_ON = 122,
    PAGE_ID_SLOT_MAX_2CH_MIN_OFF = 123,
    PAGE_ID_SLOT_MAX_ERROR = 124,
    PAGE_ID_SLOT_MAX_NOT_INSTALLED = 125,
    PAGE_ID_SLOT_MIN_1CH_TITLE = 126,
    PAGE_ID_SLOT_MIN_1CH_ON = 127,
    PAGE_ID_SLOT_MIN_1CH_OFF = 128,
    PAGE_ID_SLOT_MIN_1CH_COUPLED_SERIES = 129,
    PAGE_ID_SLOT_MIN_1CH_COUPLED_PARALLEL = 130,
    PAGE_ID_SLOT_MIN_2CH_TITLE = 131,
    PAGE_ID_SLOT_MIN_2CH = 132,
    PAGE_ID_SLOT_MIN_2CH_ON = 133,
    PAGE_ID_SLOT_MIN_2CH_OFF = 134,
    PAGE_ID_SLOT_MIN_ERROR = 135,
    PAGE_ID_SLOT_MIN_NOT_INSTALLED = 136,
    PAGE_ID_SLOT_MICRO_1CH_TITLE = 137,
    PAGE_ID_SLOT_MICRO_1CH_ON = 138,
    PAGE_ID_SLOT_MICRO_1CH_OFF = 139,
    PAGE_ID_SLOT_MICRO_1CH_COUPLED_SERIES = 140,
    PAGE_ID_SLOT_MICRO_1CH_COUPLED_PARALLEL = 141,
    PAGE_ID_SLOT_MICRO_2CH = 142,
    PAGE_ID_SLOT_MICRO_2CH_TITLE = 143,
    PAGE_ID_SLOT_MICRO_2CH_ON = 144,
    PAGE_ID_SLOT_MICRO_2CH_OFF = 145,
    PAGE_ID_SLOT_MICRO_ERROR = 146,
    PAGE_ID_SLOT_MICRO_NOT_INSTALLED = 147,
    PAGE_ID_DLOG_VIEW = 148,
    PAGE_ID_IMAGE_VIEW = 149,
    PAGE_ID_DLOG_PARAMS = 150,
    PAGE_ID_DLOG_VIEW_OVERLAY_OPTIONS = 151,
    PAGE_ID_FILE_MANAGER = 152,
    PAGE_ID_FILE_BROWSER = 153,
    PAGE_ID_DEBUG_TRACE_LOG = 154,
    PAGE_ID_FILE_MENU = 155,
    PAGE_ID_FRONT_PANEL_EMPTY_SLOT = 156,
    PAGE_ID_DCM220_FRONT_PANEL_LED = 157
};

#ifdef OPTION_SDRAM
extern const uint8_t assets[300375];
#else
extern const uint8_t assets[1314152];
#endif
