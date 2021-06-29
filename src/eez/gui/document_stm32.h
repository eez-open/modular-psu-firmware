enum DataEnum {
    DATA_ID_NONE = 0,
    DATA_ID_CHANNELS = 1,
    DATA_ID_CHANNEL_STATUS = 2,
    DATA_ID_CHANNEL_OUTPUT_STATE = 3,
    DATA_ID_CHANNEL_IS_CC = 4,
    DATA_ID_CHANNEL_IS_CV = 5,
    DATA_ID_CHANNEL_U_SET = 6,
    DATA_ID_CHANNEL_DISPLAY_VALUE1_SET = 7,
    DATA_ID_CHANNEL_U_MON = 8,
    DATA_ID_CHANNEL_U_MON_DAC = 9,
    DATA_ID_CHANNEL_U_LIMIT = 10,
    DATA_ID_CHANNEL_DISPLAY_VALUE1_LIMIT = 11,
    DATA_ID_CHANNEL_U_EDIT = 12,
    DATA_ID_CHANNEL_I_SET = 13,
    DATA_ID_CHANNEL_DISPLAY_VALUE2_SET = 14,
    DATA_ID_CHANNEL_I_MON = 15,
    DATA_ID_CHANNEL_I_MON_DAC = 16,
    DATA_ID_CHANNEL_I_LIMIT = 17,
    DATA_ID_CHANNEL_DISPLAY_VALUE2_LIMIT = 18,
    DATA_ID_CHANNEL_I_EDIT = 19,
    DATA_ID_CHANNEL_P_MON = 20,
    DATA_ID_CHANNEL_OTHER_VALUE_MON = 21,
    DATA_ID_CHANNELS_VIEW_MODE = 22,
    DATA_ID_CHANNELS_VIEW_MODE_IN_DEFAULT = 23,
    DATA_ID_CHANNELS_VIEW_MODE_IN_MAX = 24,
    DATA_ID_SLOT_VIEW_TYPE = 25,
    DATA_ID_SLOT_MAX_CHANNEL_INDEX = 26,
    DATA_ID_SLOT_MAX_VIEW = 27,
    DATA_ID_CHANNEL_DISPLAY_VALUE1 = 28,
    DATA_ID_CHANNEL_DISPLAY_VALUE2 = 29,
    DATA_ID_CHANNEL_DISPLAY_VALUE3 = 30,
    DATA_ID_OVP = 31,
    DATA_ID_OCP = 32,
    DATA_ID_OPP = 33,
    DATA_ID_OTP_CH = 34,
    DATA_ID_OTP_AUX = 35,
    DATA_ID_ALERT_MESSAGE = 36,
    DATA_ID_ALERT_MESSAGE_2 = 37,
    DATA_ID_ALERT_MESSAGE_3 = 38,
    DATA_ID_EDIT_VALUE = 39,
    DATA_ID_EDIT_UNIT = 40,
    DATA_ID_EDIT_INFO = 41,
    DATA_ID_EDIT_MODE_INTERACTIVE_MODE_SELECTOR = 42,
    DATA_ID_EDIT_STEPS = 43,
    DATA_ID_MASTER_INFO = 44,
    DATA_ID_MASTER_TEST_RESULT = 45,
    DATA_ID_FIRMWARE_INFO = 46,
    DATA_ID_KEYPAD_TEXT = 47,
    DATA_ID_KEYPAD_MODE = 48,
    DATA_ID_KEYPAD_OPTION1_TEXT = 49,
    DATA_ID_KEYPAD_OPTION1_ENABLED = 50,
    DATA_ID_KEYPAD_OPTION2_TEXT = 51,
    DATA_ID_KEYPAD_OPTION2_ENABLED = 52,
    DATA_ID_KEYPAD_OPTION3_TEXT = 53,
    DATA_ID_KEYPAD_OPTION3_ENABLED = 54,
    DATA_ID_KEYPAD_SIGN_ENABLED = 55,
    DATA_ID_KEYPAD_DOT_ENABLED = 56,
    DATA_ID_KEYPAD_UNIT_ENABLED = 57,
    DATA_ID_CHANNEL_LABEL = 58,
    DATA_ID_CHANNEL_SHORT_LABEL = 59,
    DATA_ID_CHANNEL_TEMP_STATUS = 60,
    DATA_ID_CHANNEL_TEMP = 61,
    DATA_ID_CHANNEL_ON_TIME_TOTAL = 62,
    DATA_ID_CHANNEL_ON_TIME_LAST = 63,
    DATA_ID_CHANNEL_CALIBRATION_STATUS = 64,
    DATA_ID_CHANNEL_CALIBRATION_STATE = 65,
    DATA_ID_CHANNEL_CALIBRATION_DATE = 66,
    DATA_ID_CHANNEL_CALIBRATION_REMARK = 67,
    DATA_ID_CHANNEL_PROTECTION_STATUS = 68,
    DATA_ID_CHANNEL_PROTECTION_STATUS_OVP = 69,
    DATA_ID_CHANNEL_PROTECTION_STATUS_OCP = 70,
    DATA_ID_CHANNEL_PROTECTION_STATUS_OPP = 71,
    DATA_ID_CHANNEL_PROTECTION_STATUS_OTP = 72,
    DATA_ID_CHANNEL_PROTECTION_OVP_STATE = 73,
    DATA_ID_CHANNEL_PROTECTION_OVP_TYPE = 74,
    DATA_ID_CHANNEL_PROTECTION_OVP_LEVEL = 75,
    DATA_ID_CHANNEL_PROTECTION_OVP_DELAY = 76,
    DATA_ID_CHANNEL_PROTECTION_OVP_LIMIT = 77,
    DATA_ID_CHANNEL_PROTECTION_OCP_STATE = 78,
    DATA_ID_CHANNEL_PROTECTION_OCP_DELAY = 79,
    DATA_ID_CHANNEL_PROTECTION_OCP_LIMIT = 80,
    DATA_ID_CHANNEL_PROTECTION_OCP_MAX_CURRENT_LIMIT_CAUSE = 81,
    DATA_ID_CHANNEL_PROTECTION_OPP_STATE = 82,
    DATA_ID_CHANNEL_PROTECTION_OPP_LEVEL = 83,
    DATA_ID_CHANNEL_PROTECTION_OPP_DELAY = 84,
    DATA_ID_CHANNEL_PROTECTION_OPP_LIMIT = 85,
    DATA_ID_CHANNEL_PROTECTION_OTP_INSTALLED = 86,
    DATA_ID_CHANNEL_PROTECTION_OTP_STATE = 87,
    DATA_ID_CHANNEL_PROTECTION_OTP_LEVEL = 88,
    DATA_ID_CHANNEL_PROTECTION_OTP_DELAY = 89,
    DATA_ID_EVENT_QUEUE_LAST_EVENT_TYPE = 90,
    DATA_ID_EVENT_QUEUE_EVENTS = 91,
    DATA_ID_EVENT_QUEUE_EVENT_TYPE = 92,
    DATA_ID_EVENT_QUEUE_EVENT_MESSAGE = 93,
    DATA_ID_EVENT_QUEUE_SELECTED_EVENT_MESSAGE = 94,
    DATA_ID_CHANNEL_RSENSE_STATUS = 95,
    DATA_ID_CHANNEL_RPROG_INSTALLED = 96,
    DATA_ID_CHANNEL_RPROG_STATUS = 97,
    DATA_ID_CHANNEL_TRACKING_IS_ENABLED = 98,
    DATA_ID_CHANNEL_TRACKING_IS_ALLOWED = 99,
    DATA_ID_CHANNEL_IS_PAR_SER_COUPLED_OR_TRACKED = 100,
    DATA_ID_COUPLING_TYPE = 101,
    DATA_ID_CHANNEL_COUPLING_IS_SERIES = 102,
    DATA_ID_SYS_ON_TIME_TOTAL = 103,
    DATA_ID_SYS_ON_TIME_LAST = 104,
    DATA_ID_SYS_TEMP_AUX_STATUS = 105,
    DATA_ID_SYS_TEMP_AUX_OTP_STATE = 106,
    DATA_ID_SYS_TEMP_AUX_OTP_LEVEL = 107,
    DATA_ID_SYS_TEMP_AUX_OTP_DELAY = 108,
    DATA_ID_SYS_TEMP_AUX_OTP_IS_TRIPPED = 109,
    DATA_ID_SYS_TEMP_AUX = 110,
    DATA_ID_SYS_INFO_FIRMWARE_VER = 111,
    DATA_ID_SYS_INFO_SERIAL_NO = 112,
    DATA_ID_SYS_INFO_SCPI_VER = 113,
    DATA_ID_SYS_INFO_CPU = 114,
    DATA_ID_SYS_INFO_FAN_STATUS = 115,
    DATA_ID_SYS_INFO_FAN_SPEED = 116,
    DATA_ID_CHANNEL_BOARD_INFO_LABEL = 117,
    DATA_ID_CHANNEL_BOARD_INFO_REVISION = 118,
    DATA_ID_DATE_TIME_DATE = 119,
    DATA_ID_DATE_TIME_YEAR = 120,
    DATA_ID_DATE_TIME_MONTH = 121,
    DATA_ID_DATE_TIME_DAY = 122,
    DATA_ID_DATE_TIME_TIME = 123,
    DATA_ID_DATE_TIME_HOUR = 124,
    DATA_ID_DATE_TIME_MINUTE = 125,
    DATA_ID_DATE_TIME_SECOND = 126,
    DATA_ID_DATE_TIME_TIME_ZONE = 127,
    DATA_ID_DATE_TIME_DST = 128,
    DATA_ID_SET_PAGE_DIRTY = 129,
    DATA_ID_PROFILES_LIST = 130,
    DATA_ID_PROFILES_AUTO_RECALL_STATUS = 131,
    DATA_ID_PROFILES_AUTO_RECALL_LOCATION = 132,
    DATA_ID_PROFILE_STATUS = 133,
    DATA_ID_PROFILE_LABEL = 134,
    DATA_ID_PROFILE_REMARK = 135,
    DATA_ID_PROFILE_IS_AUTO_RECALL_LOCATION = 136,
    DATA_ID_PROFILE_CHANNEL_U_SET = 137,
    DATA_ID_PROFILE_CHANNEL_I_SET = 138,
    DATA_ID_PROFILE_CHANNEL_OUTPUT_STATE = 139,
    DATA_ID_ETHERNET_INSTALLED = 140,
    DATA_ID_ETHERNET_ENABLED = 141,
    DATA_ID_MQTT_ENABLED = 142,
    DATA_ID_ETHERNET_STATUS = 143,
    DATA_ID_ETHERNET_AND_MQTT_STATUS = 144,
    DATA_ID_MQTT_CONNECTION_STATE = 145,
    DATA_ID_ETHERNET_IP_ADDRESS = 146,
    DATA_ID_ETHERNET_DNS = 147,
    DATA_ID_ETHERNET_GATEWAY = 148,
    DATA_ID_ETHERNET_SUBNET_MASK = 149,
    DATA_ID_ETHERNET_SCPI_PORT = 150,
    DATA_ID_ETHERNET_IS_CONNECTED = 151,
    DATA_ID_ETHERNET_DHCP = 152,
    DATA_ID_ETHERNET_MAC = 153,
    DATA_ID_SYS_OUTPUT_PROTECTION_COUPLED = 154,
    DATA_ID_SYS_SHUTDOWN_WHEN_PROTECTION_TRIPPED = 155,
    DATA_ID_SYS_FORCE_DISABLING_ALL_OUTPUTS_ON_POWER_UP = 156,
    DATA_ID_SYS_PASSWORD_IS_SET = 157,
    DATA_ID_SYS_RL_STATE = 158,
    DATA_ID_SYS_SOUND_IS_ENABLED = 159,
    DATA_ID_SYS_SOUND_IS_CLICK_ENABLED = 160,
    DATA_ID_SYS_SOUND_ENABLED = 161,
    DATA_ID_CHANNEL_DISPLAY_VIEW_SETTINGS_DISPLAY_VALUES = 162,
    DATA_ID_CHANNEL_DISPLAY_VIEW_SETTINGS_DISPLAY_VALUE_LABEL = 163,
    DATA_ID_CHANNEL_DISPLAY_VIEW_SETTINGS_DISPLAY_VALUE = 164,
    DATA_ID_CHANNEL_DISPLAY_VIEW_SETTINGS_SCALE = 165,
    DATA_ID_CHANNEL_DISPLAY_VIEW_SETTINGS_IS_CUSTOM_SCALE = 166,
    DATA_ID_CHANNEL_DISPLAY_VIEW_SETTINGS_RANGE = 167,
    DATA_ID_CHANNEL_DISPLAY_VIEW_SETTINGS_YT_VIEW_RATE = 168,
    DATA_ID_SYS_ENCODER_CONFIRMATION_MODE = 169,
    DATA_ID_SYS_ENCODER_MOVING_UP_SPEED = 170,
    DATA_ID_SYS_ENCODER_MOVING_DOWN_SPEED = 171,
    DATA_ID_SYS_ENCODER_INSTALLED = 172,
    DATA_ID_SYS_DISPLAY_STATE = 173,
    DATA_ID_SYS_DISPLAY_BRIGHTNESS = 174,
    DATA_ID_CHANNEL_TRIGGER_MODE = 175,
    DATA_ID_CHANNEL_TRIGGER_ON_LIST_STOP = 176,
    DATA_ID_CHANNEL_U_TRIGGER_VALUE = 177,
    DATA_ID_CHANNEL_I_TRIGGER_VALUE = 178,
    DATA_ID_CHANNEL_LIST_COUNT = 179,
    DATA_ID_CHANNEL_LISTS = 180,
    DATA_ID_RAMP_AND_DELAY_LIST = 181,
    DATA_ID_CHANNEL_LIST_INDEX = 182,
    DATA_ID_CHANNEL_LIST_DWELL = 183,
    DATA_ID_CHANNEL_LIST_DWELL_ENABLED = 184,
    DATA_ID_CHANNEL_LIST_VOLTAGE = 185,
    DATA_ID_CHANNEL_LIST_VOLTAGE_ENABLED = 186,
    DATA_ID_CHANNEL_LIST_CURRENT = 187,
    DATA_ID_CHANNEL_LIST_CURRENT_ENABLED = 188,
    DATA_ID_CHANNEL_LISTS_PREVIOUS_PAGE_ENABLED = 189,
    DATA_ID_CHANNEL_LISTS_NEXT_PAGE_ENABLED = 190,
    DATA_ID_CHANNEL_LISTS_CURSOR = 191,
    DATA_ID_CHANNEL_LISTS_INSERT_MENU_ENABLED = 192,
    DATA_ID_CHANNEL_LISTS_DELETE_MENU_ENABLED = 193,
    DATA_ID_CHANNEL_LISTS_DELETE_ROW_ENABLED = 194,
    DATA_ID_CHANNEL_LISTS_CLEAR_COLUMN_ENABLED = 195,
    DATA_ID_CHANNEL_LISTS_DELETE_ROWS_ENABLED = 196,
    DATA_ID_CHANNEL_COUPLING_STATE = 197,
    DATA_ID_TRIGGER_SOURCE = 198,
    DATA_ID_DLOG_TRIGGER_SOURCE = 199,
    DATA_ID_TRIGGER_DELAY = 200,
    DATA_ID_TRIGGER_INITIATE_CONTINUOUSLY = 201,
    DATA_ID_TRIGGER_IS_INITIATED = 202,
    DATA_ID_TRIGGER_STATE = 203,
    DATA_ID_TRIGGER_IS_MANUAL = 204,
    DATA_ID_CHANNEL_HAS_SUPPORT_FOR_CURRENT_DUAL_RANGE = 205,
    DATA_ID_CHANNEL_RANGES_SUPPORTED = 206,
    DATA_ID_CHANNEL_RANGES_MODE = 207,
    DATA_ID_CHANNEL_RANGES_AUTO_RANGING = 208,
    DATA_ID_CHANNEL_RANGES_CURRENTLY_SELECTED = 209,
    DATA_ID_TEXT_MESSAGE = 210,
    DATA_ID_SERIAL_STATUS = 211,
    DATA_ID_SERIAL_IS_CONNECTED = 212,
    DATA_ID_CHANNEL_LIST_COUNTDOWN = 213,
    DATA_ID_CHANNEL_RAMP_COUNTDOWN = 214,
    DATA_ID_IO_PINS = 215,
    DATA_ID_IO_PINS_INHIBIT_STATE = 216,
    DATA_ID_IO_PIN_LABEL = 217,
    DATA_ID_IO_PIN_POLARITY = 218,
    DATA_ID_IO_PIN_FUNCTION_NAME = 219,
    DATA_ID_IO_PIN_UART_MODE = 220,
    DATA_ID_IO_PIN_UART_BAUD = 221,
    DATA_ID_IO_PIN_UART_PARITY = 222,
    DATA_ID_IO_PIN_UART_DATA_BITS = 223,
    DATA_ID_IO_PIN_UART_STOP_BITS = 224,
    DATA_ID_NTP_ENABLED = 225,
    DATA_ID_NTP_SERVER = 226,
    DATA_ID_SYS_DISPLAY_BACKGROUND_LUMINOSITY_STEP = 227,
    DATA_ID_PROGRESS = 228,
    DATA_ID_TOUCH_CALIBRATION_POINT = 229,
    DATA_ID_SELECTED_THEME = 230,
    DATA_ID_CHANNEL_TITLE = 231,
    DATA_ID_CHANNEL_SHORT_TITLE = 232,
    DATA_ID_CHANNEL_SHORT_TITLE_WITHOUT_TRACKING_ICON = 233,
    DATA_ID_CHANNEL_LONG_TITLE = 234,
    DATA_ID_ANIMATIONS_DURATION = 235,
    DATA_ID_SLOT_INFO = 236,
    DATA_ID_SLOT_TEST_RESULT = 237,
    DATA_ID_SLOT_DEFAULT_VIEW = 238,
    DATA_ID_SLOT1_CHANNEL_INDEX = 239,
    DATA_ID_SLOT2_CHANNEL_INDEX = 240,
    DATA_ID_SLOT3_CHANNEL_INDEX = 241,
    DATA_ID_SLOT1_NON_MAPPED_CHANNEL_INDEX = 242,
    DATA_ID_SLOT2_NON_MAPPED_CHANNEL_INDEX = 243,
    DATA_ID_SLOT3_NON_MAPPED_CHANNEL_INDEX = 244,
    DATA_ID_BATTERY = 245,
    DATA_ID_IO_PIN_STATE = 246,
    DATA_ID_CHANNEL_DPROG = 247,
    DATA_ID_CHANNEL_DPROG_INSTALLED = 248,
    DATA_ID_SYS_INFO_SDCARD_STATUS = 249,
    DATA_ID_SYS_INFO_HAS_ERROR = 250,
    DATA_ID_SYS_SETTINGS_HAS_ERROR = 251,
    DATA_ID_SYS_INFO_BATTERY_STATUS = 252,
    DATA_ID_SYS_FAN_MODE = 253,
    DATA_ID_SYS_FAN_SPEED = 254,
    DATA_ID_IS_COUPLING_TYPE_UNCOUPLED = 255,
    DATA_ID_IS_COUPLING_TYPE_PARALLEL = 256,
    DATA_ID_IS_COUPLING_TYPE_COMMON_GND = 257,
    DATA_ID_IS_COUPLING_TYPE_SPLIT_RAILS = 258,
    DATA_ID_IS_TRACKING_ALLOWED = 259,
    DATA_ID_IS_COUPLING_PARALLEL_ALLOWED = 260,
    DATA_ID_IS_COUPLING_SERIES_ALLOWED = 261,
    DATA_ID_CHANNEL_COUPLING_ENABLE_TRACKING_MODE = 262,
    DATA_ID_CHANNEL_ACTIVE_COUPLED_LED = 263,
    DATA_ID_IS_COUPLING_TYPE_SERIES = 264,
    DATA_ID_IS_COUPLED_OR_TRACKED = 265,
    DATA_ID_OVERLAY = 266,
    DATA_ID_CHANNELS_WITH_LIST_COUNTER_VISIBLE = 267,
    DATA_ID_CHANNELS_WITH_RAMP_COUNTER_VISIBLE = 268,
    DATA_ID_DLOG_STATE = 269,
    DATA_ID_DLOG_TOGGLE_STATE = 270,
    DATA_ID_RECORDING = 271,
    DATA_ID_CHANNEL_HISTORY_VALUES = 272,
    DATA_ID_RECORDING_READY = 273,
    DATA_ID_NONDRAG_OVERLAY = 274,
    DATA_ID_DLOG_SINGLE_VALUE_OVERLAY = 275,
    DATA_ID_DLOG_MULTIPLE_VALUES_OVERLAY = 276,
    DATA_ID_DLOG_VISIBLE_VALUES = 277,
    DATA_ID_DLOG_VISIBLE_VALUE_LABEL = 278,
    DATA_ID_DLOG_VISIBLE_VALUE_DIV = 279,
    DATA_ID_DLOG_VISIBLE_VALUE_OFFSET = 280,
    DATA_ID_DLOG_VISIBLE_VALUE_CURSOR = 281,
    DATA_ID_DLOG_CURRENT_TIME = 282,
    DATA_ID_DLOG_FILE_LENGTH = 283,
    DATA_ID_DLOG_X_AXIS_OFFSET = 284,
    DATA_ID_DLOG_X_AXIS_DIV = 285,
    DATA_ID_DLOG_X_AXIS_MAX_VALUE = 286,
    DATA_ID_DLOG_X_AXIS_MAX_VALUE_LABEL = 287,
    DATA_ID_IS_SINGLE_PAGE_ON_STACK = 288,
    DATA_ID_IS_SHOW_LIVE_RECORDING = 289,
    DATA_ID_DLOG_VIEW_STATE = 290,
    DATA_ID_DLOG_PERIOD = 291,
    DATA_ID_DLOG_PERIOD_SPEC = 292,
    DATA_ID_DLOG_DURATION = 293,
    DATA_ID_DLOG_FILE_NAME = 294,
    DATA_ID_CHANNEL_OFF_LABEL = 295,
    DATA_ID_FILE_MANAGER_FILES = 296,
    DATA_ID_FILE_MANAGER_FILE_NAME = 297,
    DATA_ID_FILE_MANAGER_FILE_DATE_TIME = 298,
    DATA_ID_FILE_MANAGER_FILE_SIZE = 299,
    DATA_ID_FILE_MANAGER_FILE_SELECTED = 300,
    DATA_ID_FILE_MANAGER_STATE = 301,
    DATA_ID_FILE_MANAGER_HAS_PARENT = 302,
    DATA_ID_FILE_MANAGER_CURRENT_DIRECTORY = 303,
    DATA_ID_FILE_MANAGER_FILE_TYPE = 304,
    DATA_ID_FILE_MANAGER_OPEN_FILE_ENABLED = 305,
    DATA_ID_FILE_MANAGER_UPLOAD_FILE_ENABLED = 306,
    DATA_ID_FILE_MANAGER_RENAME_FILE_ENABLED = 307,
    DATA_ID_FILE_MANAGER_DELETE_FILE_ENABLED = 308,
    DATA_ID_FILE_MANAGER_OPENED_IMAGE = 309,
    DATA_ID_SCRIPT_INFO = 310,
    DATA_ID_SCRIPT_IS_STARTED = 311,
    DATA_ID_KEYPAD_OK_ENABLED = 312,
    DATA_ID_ETHERNET_HOST_NAME = 313,
    DATA_ID_MQTT_HOST = 314,
    DATA_ID_MQTT_PORT = 315,
    DATA_ID_MQTT_USERNAME = 316,
    DATA_ID_MQTT_PASSWORD = 317,
    DATA_ID_MQTT_PERIOD = 318,
    DATA_ID_DLOG_VIEW_SHOW_LABELS = 319,
    DATA_ID_IS_ANY_COUPLING_ALLOWED = 320,
    DATA_ID_IS_COUPLING_SPLIT_RAILS_ALLOWED = 321,
    DATA_ID_LIST_COUNTER_LABEL = 322,
    DATA_ID_RAMP_COUNTER_LABEL = 323,
    DATA_ID_CHANNEL_FIRMWARE_VERSION = 324,
    DATA_ID_FILE_MANAGER_BROWSER_TITLE = 325,
    DATA_ID_CHANNEL_HAS_ERROR_SETTINGS = 326,
    DATA_ID_IS_RESET_BY_IWDG = 327,
    DATA_ID_FILE_MANAGER_BROWSER_IS_SAVE_DIALOG = 328,
    DATA_ID_FILE_MANAGER_STORAGE_INFO = 329,
    DATA_ID_FILE_MANAGER_STORAGE_ALARM = 330,
    DATA_ID_EVENT_QUEUE_IS_LONG_MESSAGE_TEXT = 331,
    DATA_ID_EVENT_QUEUE_EVENT_LONG_MESSAGE_OVERLAY = 332,
    DATA_ID_EVENT_QUEUE_EVENT_IS_SELECTED = 333,
    DATA_ID_DATE_TIME_FORMAT = 334,
    DATA_ID_DATE_TIME_FORMAT_IS_DMY = 335,
    DATA_ID_DATE_TIME_FORMAT_IS_24H = 336,
    DATA_ID_DATE_TIME_AM_PM = 337,
    DATA_ID_IS_MULTI_TRACKING = 338,
    DATA_ID_CHANNEL_COPY_AVAILABLE = 339,
    DATA_ID_CHANNEL_RSENSE_INSTALLED = 340,
    DATA_ID_FILE_MANAGER_SORT_FILES_OPTION = 341,
    DATA_ID_CHANNEL_INFO_BRAND = 342,
    DATA_ID_CHANNEL_INFO_SERIAL = 343,
    DATA_ID_FILE_MANAGER_LIST_VIEW = 344,
    DATA_ID_FILE_MANAGER_IS_LIST_VIEW_OPTION_AVAILABLE = 345,
    DATA_ID_FILE_MANAGER_FILE_ICON = 346,
    DATA_ID_FILE_MANAGER_LAYOUT = 347,
    DATA_ID_FILE_MANAGER_FILE_DESCRIPTION = 348,
    DATA_ID_TOUCH_RAW_X = 349,
    DATA_ID_TOUCH_RAW_Y = 350,
    DATA_ID_TOUCH_RAW_Z1 = 351,
    DATA_ID_TOUCH_RAW_PRESSED = 352,
    DATA_ID_TOUCH_CALIBRATED_X = 353,
    DATA_ID_TOUCH_CALIBRATED_Y = 354,
    DATA_ID_TOUCH_CALIBRATED_PRESSED = 355,
    DATA_ID_TOUCH_FILTERED_X = 356,
    DATA_ID_TOUCH_FILTERED_Y = 357,
    DATA_ID_TOUCH_FILTERED_PRESSED = 358,
    DATA_ID_CAN_SHOW_PREVIOUS_PAGE = 359,
    DATA_ID_FILE_MANAGER_FILE_HAS_DESCRIPTION = 360,
    DATA_ID_FILE_MANAGER_IMAGE_OPEN_STATE = 361,
    DATA_ID_FILE_MANAGER_IMAGE_OPEN_PROGRESS = 362,
    DATA_ID_ALERT_MESSAGE_IS_SET = 363,
    DATA_ID_ASYNC_PROGRESS = 364,
    DATA_ID_IO_PIN_FUNCTION = 365,
    DATA_ID_IO_PIN_PWM_FREQUENCY = 366,
    DATA_ID_IO_PIN_PWM_DUTY = 367,
    DATA_ID_IO_PIN_IS_UART_ENABLED = 368,
    DATA_ID_CHANNEL_VOLTAGE_RAMP_DURATION = 369,
    DATA_ID_CHANNEL_CURRENT_RAMP_DURATION = 370,
    DATA_ID_CHANNEL_OUTPUT_DELAY = 371,
    DATA_ID_CHANNEL_RAMP_STATE = 372,
    DATA_ID_RAMP_AND_DELAY_LIST_SCROLLBAR_ENABLED = 373,
    DATA_ID_MQTT_IN_ERROR = 374,
    DATA_ID_DLOG_VIEW_LEGEND_VIEW_OPTION = 375,
    DATA_ID_CALIBRATION_POINT_MEASURED_VALUE = 376,
    DATA_ID_CALIBRATION_POINT_SET_VALUE = 377,
    DATA_ID_CHANNEL_CALIBRATION_VALUE_TYPE = 378,
    DATA_ID_CHANNEL_CALIBRATION_VALUE_IS_VOLTAGE = 379,
    DATA_ID_CHANNEL_CALIBRATION_POINT_CAN_MOVE_TO_PREVIOUS = 380,
    DATA_ID_CHANNEL_CALIBRATION_POINT_CAN_MOVE_TO_NEXT = 381,
    DATA_ID_CHANNEL_CALIBRATION_POINT_CAN_DELETE = 382,
    DATA_ID_CHANNEL_CALIBRATION_POINT_INFO = 383,
    DATA_ID_CHANNEL_CALIBRATION_CHART = 384,
    DATA_ID_CHANNEL_CALIBRATION_POINT_CAN_SAVE = 385,
    DATA_ID_CHANNEL_CALIBRATION_CHART_ZOOM = 386,
    DATA_ID_SLOTS = 387,
    DATA_ID_SLOT_INDEX = 388,
    DATA_ID_SLOT_TITLE_DEF = 389,
    DATA_ID_SLOT_TITLE_MAX = 390,
    DATA_ID_SLOT_TITLE_SETTINGS = 391,
    DATA_ID_SLOT_SHORT_TITLE = 392,
    DATA_ID_SLOT_ERROR_MESSAGE = 393,
    DATA_ID_USB_MODE = 394,
    DATA_ID_USB_CURRENT_MODE = 395,
    DATA_ID_USB_DEVICE_CLASS = 396,
    DATA_ID_USB_KEYBOARD_STATE = 397,
    DATA_ID_CHANNELS_IS_2COL_VIEW = 398,
    DATA_ID_DISPLAY_TEST_COLOR_INDEX = 399,
    DATA_ID_CHANNEL_HAS_ADVANCED_OPTIONS = 400,
    DATA_ID_CHANNEL_HAS_FIRMWARE_UPDATE = 401,
    DATA_ID_CHANNEL_CALIBRATION_VALUE_TYPE_IS_SELECTABLE = 402,
    DATA_ID_SLOT_SERIAL_INFO = 403,
    DATA_ID_SLOT_IS_ENABLED = 404,
    DATA_ID_SLOT_IS_DCPSUPPLY_MODULE = 405,
    DATA_ID_USER_SWITCH_ACTION = 406,
    DATA_ID_NTP_REFRESH_FREQUENCY = 407,
    DATA_ID_CHANNEL_CALIBRATION_IS_POWER_CHANNEL = 408,
    DATA_ID_HAS_CUSTOM_BITMAP = 409,
    DATA_ID_CUSTOM_BITMAP = 410,
    DATA_ID_SLOT_INFO_WITH_FW_VER = 411,
    DATA_ID_MASTER_INFO_WITH_FW_VER = 412,
    DATA_ID_SLOT1_LABELS_AND_COLORS_VIEW = 413,
    DATA_ID_SLOT2_LABELS_AND_COLORS_VIEW = 414,
    DATA_ID_SLOT3_LABELS_AND_COLORS_VIEW = 415,
    DATA_ID_COLORS = 416,
    DATA_ID_KEYPAD_CAN_SET_DEFAULT = 417,
    DATA_ID_LABELS_AND_COLORS_PAGE_SLOT_TITLE = 418,
    DATA_ID_LABELS_AND_COLORS_PAGE_CHANNEL_TITLE = 419,
    DATA_ID_LABELS_AND_COLORS_IS_COLOR_MODIFIED = 420,
    DATA_ID_DLOG_ITEMS = 421,
    DATA_ID_DLOG_ITEMS_NUM_SELECTED = 422,
    DATA_ID_DLOG_ITEM_IS_CHECKED = 423,
    DATA_ID_DLOG_ITEM_CHANNEL = 424,
    DATA_ID_DLOG_ITEM_LABEL = 425,
    DATA_ID_DLOG_ITEMS_SCROLLBAR_ENABLED = 426,
    DATA_ID_DLOG_START_ENABLED = 427,
    DATA_ID_HAS_MULTIPLE_DISK_DRIVES = 428,
    DATA_ID_SELECTED_MASS_STORAGE_DEVICE = 429,
    DATA_ID_FILE_MANAGER_IS_SORT_FILES_OPTION_AVAILABLE = 430,
    DATA_ID_DLOG_PREVIEW_OVERLAY = 431,
    DATA_ID_DLOG_ITEM_IS_AVAILABLE = 432,
    DATA_ID_MODULE_IS_RESYNC_SUPPORTED = 433,
    DATA_ID_CHANNEL_IS_CALIBRATION_VALUE_SOURCE = 434,
    DATA_ID_CALIBRATION_POINT_CAN_EDIT_SET_VALUE = 435,
    DATA_ID_CALIBRATION_POINT_LIVE_MEASURED_VALUE = 436,
    DATA_ID_AC_MAINS = 437,
    DATA_ID_SELECTED_MCU_REVISION = 438,
    DATA_ID_IS_MCU_REVISION_SELECTED = 439,
    DATA_ID_SLOT_IS_2CH_VIEW = 440,
    DATA_ID_COLOR_IS_SELECTED = 441,
    DATA_ID_MASTER_ERROR_MESSAGE = 442,
    DATA_ID_DLOG_PERIOD_HAS_PREDEFINED_VALUES = 443,
    DATA_ID_DLOG_VIEW_SELECTED_TAB = 444,
    DATA_ID_DLOG_BOOKMARKS = 445,
    DATA_ID_DLOG_BOOKMARK_X_VALUE = 446,
    DATA_ID_DLOG_BOOKMARK_TEXT = 447,
    DATA_ID_DLOG_BOOKMARK_IS_SELECTED = 448,
    DATA_ID_DLOG_VIEW_IS_DRAWER_OPEN = 449,
    DATA_ID_DLOG_VIEW_IS_ZOOM_IN_ENABLED = 450,
    DATA_ID_DLOG_VIEW_IS_ZOOM_OUT_ENABLED = 451,
    DATA_ID_DLOG_BOOKMARKS_IS_SCROLLBAR_VISIBLE = 452,
    DATA_ID_DLOG_Y_VALUES_IS_SCROLLBAR_VISIBLE = 453,
    DATA_ID_DLOG_Y_VALUES = 454,
    DATA_ID_DLOG_Y_VALUE_LABEL = 455,
    DATA_ID_DLOG_Y_VALUE_IS_CHECKED = 456,
    DATA_ID_DLOG_Y_VALUE_IS_SELECTED = 457,
    DATA_ID_DLOG_Y_VALUE_IS_ENABLED = 458,
    DATA_ID_DLOG_Y_VALUE_OFFSET = 459,
    DATA_ID_DLOG_Y_VALUE_DIV = 460,
    DATA_ID_DLOG_Y_VALUE_CURSOR = 461,
    DATA_ID_FUNCTION_GENERATOR_CHANNELS = 462,
    DATA_ID_FUNCTION_GENERATOR_ANY_CHANNEL_SELECTED = 463,
    DATA_ID_FUNCTION_GENERATOR_CHANNEL = 464,
    DATA_ID_FUNCTION_GENERATOR_ITEM_LABEL = 465,
    DATA_ID_FUNCTION_GENERATOR_SELECTED_ITEM_LABEL = 466,
    DATA_ID_FUNCTION_GENERATOR_ITEM_IS_CHECKED = 467,
    DATA_ID_FUNCTION_GENERATOR_IS_MAX_SELECTED = 468,
    DATA_ID_FUNCTION_GENERATOR_NUM_SELECTED = 469,
    DATA_ID_FUNCTION_GENERATOR_ITEM_IS_SELECTED = 470,
    DATA_ID_FUNCTION_GENERATOR_WAVEFORM = 471,
    DATA_ID_FUNCTION_GENERATOR_WAVEFORM_LABEL = 472,
    DATA_ID_FUNCTION_GENERATOR_FREQUENCY = 473,
    DATA_ID_FUNCTION_GENERATOR_MODE = 474,
    DATA_ID_FUNCTION_GENERATOR_PHASE_SHIFT = 475,
    DATA_ID_FUNCTION_GENERATOR_AMPLITUDE = 476,
    DATA_ID_FUNCTION_GENERATOR_OFFSET = 477,
    DATA_ID_FUNCTION_GENERATOR_DUTY_CYCLE = 478,
    DATA_ID_FUNCTION_GENERATOR_CANVAS = 479,
    DATA_ID_FUNCTION_GENERATOR_HAS_MODE_SELECT = 480,
    DATA_ID_FUNCTION_GENERATOR_HAS_AMPLITUDE_AND_OFFSET = 481,
    DATA_ID_FUNCTION_GENERATOR_HAS_DUTY_CYCLE = 482,
    DATA_ID_FUNCTION_GENERATOR_IS_FREQ = 483,
    DATA_ID_FUNCTION_GENERATOR_IS_AMPL = 484,
    DATA_ID_FUNCTION_GENERATOR_IS_DC = 485,
    DATA_ID_FUNCTION_GENERATOR_ANY_SELECTED = 486,
    DATA_ID_OVERLAY_MINIMIZED = 487,
    DATA_ID_OVERLAY_HIDDEN = 488,
    DATA_ID_NEXT_CHANNEL_IN_MAX_VIEW_BUTTON_ENABLED = 489,
    DATA_ID_PREV_CHANNEL_IN_MAX_VIEW_BUTTON_ENABLED = 490,
    DATA_ID_IS_TOGGLE_CHANNELS_VIEW_MODE_ENABLED = 491,
    DATA_ID_DLOG_HAS_BOOKMARK = 492,
    DATA_ID_FUNCTION_GENERATOR_IS_ANY_CHANNEL_ACTIVE = 493,
    DATA_ID_TRIGGER_INITIATE_ALL = 494,
    DATA_ID_CHANNEL_OCP_TRIP_LEVEL_INFO = 495,
    DATA_ID_FUNCTION_GENERATOR_AMPLITUDE_LABEL = 496,
    DATA_ID_DIB_MIO168_DIN_PINS = 497,
    DATA_ID_DIB_MIO168_DIN_PINS_1_4 = 498,
    DATA_ID_DIB_MIO168_DIN_PINS_5_8 = 499,
    DATA_ID_DIB_MIO168_DIN_NO = 500,
    DATA_ID_DIB_MIO168_DIN_STATE = 501,
    DATA_ID_DIB_MIO168_DIN_RANGE = 502,
    DATA_ID_DIB_MIO168_DIN_LABEL = 503,
    DATA_ID_DIB_MIO168_DIN_LABEL_LABEL = 504,
    DATA_ID_DIB_MIO168_DIN_SPEED = 505,
    DATA_ID_DIB_MIO168_DOUT_PINS = 506,
    DATA_ID_DIB_MIO168_DOUT_PINS_1_4 = 507,
    DATA_ID_DIB_MIO168_DOUT_PINS_5_8 = 508,
    DATA_ID_DIB_MIO168_DOUT_NO = 509,
    DATA_ID_DIB_MIO168_DOUT_STATE = 510,
    DATA_ID_DIB_MIO168_DOUT_LABEL = 511,
    DATA_ID_DIB_MIO168_DOUT_LABEL_LABEL = 512,
    DATA_ID_DIB_MIO168_DOUT_TRIGGER_MODE = 513,
    DATA_ID_DIB_MIO168_DOUT_TRIGGER_IS_INITIATED = 514,
    DATA_ID_DIB_MIO168_DOUT_FUNCTION_LABEL = 515,
    DATA_ID_DIB_MIO168_AIN_CHANNELS = 516,
    DATA_ID_DIB_MIO168_AIN_LABEL = 517,
    DATA_ID_DIB_MIO168_AIN_LABEL_SHORT = 518,
    DATA_ID_DIB_MIO168_AIN_LABEL_LABEL = 519,
    DATA_ID_DIB_MIO168_AIN_VALUE = 520,
    DATA_ID_DIB_MIO168_AIN_MODE = 521,
    DATA_ID_DIB_MIO168_AIN_RANGE = 522,
    DATA_ID_DIB_MIO168_AIN_IS_DLOG_ACTIVE = 523,
    DATA_ID_DIB_MIO168_AIN_MODE_AND_RANGE = 524,
    DATA_ID_DIB_MIO168_AIN_MODE_IS_MULTIPLE_SELECTION_AVAILABLE = 525,
    DATA_ID_DIB_MIO168_AIN_RANGE_IS_MULTIPLE_SELECTION_AVAILABLE = 526,
    DATA_ID_DIB_MIO168_AIN_IS_OVERFLOW = 527,
    DATA_ID_DIB_MIO168_AIN_HAS_POWER_COLUMN = 528,
    DATA_ID_DIB_MIO168_AIN_HAS_P1 = 529,
    DATA_ID_DIB_MIO168_AIN_P1 = 530,
    DATA_ID_DIB_MIO168_AIN_HAS_P2 = 531,
    DATA_ID_DIB_MIO168_AIN_P2 = 532,
    DATA_ID_DIB_MIO168_AIN_HAS_EFFICIENCY = 533,
    DATA_ID_DIB_MIO168_AIN_CALC_EFFICIENCY = 534,
    DATA_ID_DIB_MIO168_AIN_EFFICIENCY = 535,
    DATA_ID_DIB_MIO168_AIN_ACTIVE_POWER = 536,
    DATA_ID_DIB_MIO168_AIN_REACTIVE_POWER = 537,
    DATA_ID_DIB_MIO168_AIN_VOLT_RMS = 538,
    DATA_ID_DIB_MIO168_AIN_CURR_RMS = 539,
    DATA_ID_DIB_MIO168_AIN_APPARENT_POWER = 540,
    DATA_ID_DIB_MIO168_AIN_POWER_FACTOR = 541,
    DATA_ID_DIB_MIO168_AIN_NPLC = 542,
    DATA_ID_DIB_MIO168_AIN_APERTURE = 543,
    DATA_ID_DIB_MIO168_AIN_EFFICIENCY_FORMULA = 544,
    DATA_ID_DIB_MIO168_AIN_IS_AUTO_RANGE = 545,
    DATA_ID_DIB_MIO168_AIN_IS_ERROR = 546,
    DATA_ID_DIB_MIO168_IS_NO_AFE = 547,
    DATA_ID_DIB_MIO168_AC_ANALYSIS_ENABLED = 548,
    DATA_ID_DIB_MIO168_AOUT_CHANNELS = 549,
    DATA_ID_DIB_MIO168_AOUT_LABEL = 550,
    DATA_ID_DIB_MIO168_AOUT_LABEL_SHORT = 551,
    DATA_ID_DIB_MIO168_AOUT_LABEL_LABEL = 552,
    DATA_ID_DIB_MIO168_AOUT_VALUE = 553,
    DATA_ID_DIB_MIO168_AOUT_VALUE_IS_OFF = 554,
    DATA_ID_DIB_MIO168_AC_ANALYSIS_ALLOWED = 555,
    DATA_ID_DIB_MIO168_AOUT_OUTPUT_ENABLED = 556,
    DATA_ID_DIB_MIO168_AOUT_OUTPUT_MODE = 557,
    DATA_ID_DIB_MIO168_AOUT_VOLTAGE_RANGE = 558,
    DATA_ID_DIB_MIO168_AOUT_CURRENT_RANGE = 559,
    DATA_ID_DIB_MIO168_AOUT_CHANNEL_HAS_SETTINGS = 560,
    DATA_ID_DIB_MIO168_AOUT_MODE_AND_RANGE = 561,
    DATA_ID_DIB_MIO168_AOUT_TRIGGER_MODE = 562,
    DATA_ID_DIB_MIO168_AOUT_TRIGGER_IS_INITIATED = 563,
    DATA_ID_DIB_MIO168_AOUT_FUNCTION_LABEL = 564,
    DATA_ID_DIB_MIO168_PWM_CHANNELS = 565,
    DATA_ID_DIB_MIO168_PWM_LABEL = 566,
    DATA_ID_DIB_MIO168_PWM_LABEL_SHORT = 567,
    DATA_ID_DIB_MIO168_PWM_LABEL_LABEL = 568,
    DATA_ID_DIB_MIO168_PWM_FREQ = 569,
    DATA_ID_DIB_MIO168_PWM_DUTY = 570,
    DATA_ID_DIB_MIO168_DIN_HAS_SPEED = 571,
    DATA_ID_DIB_MIO168_AFE_VERSION = 572,
    DATA_ID_DIB_MIO168_PAGER_LIST = 573,
    DATA_ID_DIB_MIO168_PAGER_IS_SELECTED = 574,
    DATA_ID_DIB_MIO168_PAGER_SELECTED_PAGE = 575,
    DATA_ID_DIB_MIO168_SELECTED_PAGE_IN_DEFAULT_VIEW = 576,
    DATA_ID_DIB_MIO168_IS_SETTINGS_PAGE_SELECTED = 577,
    DATA_ID_DIB_PREL6_RELAYS = 578,
    DATA_ID_DIB_PREL6_RELAY_IS_ON = 579,
    DATA_ID_DIB_PREL6_RELAY_LABEL = 580,
    DATA_ID_DIB_PREL6_RELAY_CYCLES = 581,
    DATA_ID_DIB_PREL6_RELAY_LABEL_LABEL = 582,
    DATA_ID_DIB_PREL6_IS_RELAY_1_OR_6 = 583,
    DATA_ID_DIB_SMX46_ROUTES = 584,
    DATA_ID_DIB_SMX46_ROUTE_OPEN = 585,
    DATA_ID_DIB_SMX46_X_LABELS = 586,
    DATA_ID_DIB_SMX46_X_LABEL = 587,
    DATA_ID_DIB_SMX46_Y_LABELS = 588,
    DATA_ID_DIB_SMX46_Y_LABEL = 589,
    DATA_ID_DIB_SMX46_AOUT_CHANNELS = 590,
    DATA_ID_DIB_SMX46_AOUT_VALUE = 591,
    DATA_ID_DIB_SMX46_AOUT_LABEL = 592,
    DATA_ID_DIB_SMX46_AOUT_TRIGGER_MODE = 593,
    DATA_ID_DIB_SMX46_AOUT_TRIGGER_IS_INITIATED = 594,
    DATA_ID_DIB_SMX46_AOUT_FUNCTION_LABEL = 595,
    DATA_ID_DIB_SMX46_RELAY_ON = 596,
    DATA_ID_DIB_SMX46_SIGNAL_RELAY_CYCLES = 597,
    DATA_ID_DIB_SMX46_POWER_RELAY_CYCLES = 598,
    DATA_ID_DIB_DCM220_SLOT_DEF_2CH_VIEW = 599,
    DATA_ID_DIB_DCM220_SLOT_2CH_CH1_INDEX = 600,
    DATA_ID_DIB_DCM220_SLOT_2CH_CH2_INDEX = 601,
    DATA_ID_DIB_DCM224_PWM_FREQUENCY = 602,
    DATA_ID_DIB_DCM224_PWM_DUTY = 603,
    DATA_ID_DIB_DCM224_COUNTERPHASE_FREQUENCY = 604,
    DATA_ID_DIB_DCM224_COUNTERPHASE_DITHERING = 605,
    DATA_ID_DIB_DCM224_PWM_ENABLED = 606,
    DATA_ID_DIB_DCM224_SLOT_DEF_2CH_VIEW = 607,
    DATA_ID_DIB_DCM224_SLOT_2CH_CH1_INDEX = 608,
    DATA_ID_DIB_DCM224_SLOT_2CH_CH2_INDEX = 609,
    DATA_ID_DIB_MUX14D_P1_RELAYS = 610,
    DATA_ID_DIB_MUX14D_P1_RELAY_LABEL = 611,
    DATA_ID_DIB_MUX14D_P1_RELAY_IS_FIRST = 612,
    DATA_ID_DIB_MUX14D_P1_RELAY_IS_ON = 613,
    DATA_ID_DIB_MUX14D_P1_RELAY_CYCLES = 614,
    DATA_ID_DIB_MUX14D_P1_RELAY_LABEL_LABEL = 615,
    DATA_ID_DIB_MUX14D_P2_RELAYS = 616,
    DATA_ID_DIB_MUX14D_P2_RELAY_LABEL = 617,
    DATA_ID_DIB_MUX14D_P2_RELAY_IS_FIRST = 618,
    DATA_ID_DIB_MUX14D_P2_RELAY_IS_ON = 619,
    DATA_ID_DIB_MUX14D_P2_RELAY_CYCLES = 620,
    DATA_ID_DIB_MUX14D_P2_RELAY_LABEL_LABEL = 621,
    DATA_ID_DIB_MUX14D_EXT_RELAY_IS_ON = 622,
    DATA_ID_DIB_MUX14D_EXT_RELAY_CYCLES = 623,
    DATA_ID_DIB_MUX14D_ADIB1_RELAY_IS_ON = 624,
    DATA_ID_DIB_MUX14D_ADIB1_RELAY_CYCLES = 625,
    DATA_ID_DIB_MUX14D_ADIB2_RELAY_IS_ON = 626,
    DATA_ID_DIB_MUX14D_ADIB2_RELAY_CYCLES = 627,
    DATA_ID_DIB_MUX14D_CJ_TEMP = 628,
    DATA_ID_DIB_MUX14D_MULTIPLE_CONNECTIONS = 629
};

void data_none(DataOperationEnum operation, Cursor cursor, Value &value);
void data_channels(DataOperationEnum operation, Cursor cursor, Value &value);
void data_channel_status(DataOperationEnum operation, Cursor cursor, Value &value);
void data_channel_output_state(DataOperationEnum operation, Cursor cursor, Value &value);
void data_channel_is_cc(DataOperationEnum operation, Cursor cursor, Value &value);
void data_channel_is_cv(DataOperationEnum operation, Cursor cursor, Value &value);
void data_channel_u_set(DataOperationEnum operation, Cursor cursor, Value &value);
void data_channel_display_value1_set(DataOperationEnum operation, Cursor cursor, Value &value);
void data_channel_u_mon(DataOperationEnum operation, Cursor cursor, Value &value);
void data_channel_u_mon_dac(DataOperationEnum operation, Cursor cursor, Value &value);
void data_channel_u_limit(DataOperationEnum operation, Cursor cursor, Value &value);
void data_channel_display_value1_limit(DataOperationEnum operation, Cursor cursor, Value &value);
void data_channel_u_edit(DataOperationEnum operation, Cursor cursor, Value &value);
void data_channel_i_set(DataOperationEnum operation, Cursor cursor, Value &value);
void data_channel_display_value2_set(DataOperationEnum operation, Cursor cursor, Value &value);
void data_channel_i_mon(DataOperationEnum operation, Cursor cursor, Value &value);
void data_channel_i_mon_dac(DataOperationEnum operation, Cursor cursor, Value &value);
void data_channel_i_limit(DataOperationEnum operation, Cursor cursor, Value &value);
void data_channel_display_value2_limit(DataOperationEnum operation, Cursor cursor, Value &value);
void data_channel_i_edit(DataOperationEnum operation, Cursor cursor, Value &value);
void data_channel_p_mon(DataOperationEnum operation, Cursor cursor, Value &value);
void data_channel_other_value_mon(DataOperationEnum operation, Cursor cursor, Value &value);
void data_channels_view_mode(DataOperationEnum operation, Cursor cursor, Value &value);
void data_channels_view_mode_in_default(DataOperationEnum operation, Cursor cursor, Value &value);
void data_channels_view_mode_in_max(DataOperationEnum operation, Cursor cursor, Value &value);
void data_slot_view_type(DataOperationEnum operation, Cursor cursor, Value &value);
void data_slot_max_channel_index(DataOperationEnum operation, Cursor cursor, Value &value);
void data_slot_max_view(DataOperationEnum operation, Cursor cursor, Value &value);
void data_channel_display_value1(DataOperationEnum operation, Cursor cursor, Value &value);
void data_channel_display_value2(DataOperationEnum operation, Cursor cursor, Value &value);
void data_channel_display_value3(DataOperationEnum operation, Cursor cursor, Value &value);
void data_ovp(DataOperationEnum operation, Cursor cursor, Value &value);
void data_ocp(DataOperationEnum operation, Cursor cursor, Value &value);
void data_opp(DataOperationEnum operation, Cursor cursor, Value &value);
void data_otp_ch(DataOperationEnum operation, Cursor cursor, Value &value);
void data_otp_aux(DataOperationEnum operation, Cursor cursor, Value &value);
void data_alert_message(DataOperationEnum operation, Cursor cursor, Value &value);
void data_alert_message_2(DataOperationEnum operation, Cursor cursor, Value &value);
void data_alert_message_3(DataOperationEnum operation, Cursor cursor, Value &value);
void data_edit_value(DataOperationEnum operation, Cursor cursor, Value &value);
void data_edit_unit(DataOperationEnum operation, Cursor cursor, Value &value);
void data_edit_info(DataOperationEnum operation, Cursor cursor, Value &value);
void data_edit_mode_interactive_mode_selector(DataOperationEnum operation, Cursor cursor, Value &value);
void data_edit_steps(DataOperationEnum operation, Cursor cursor, Value &value);
void data_master_info(DataOperationEnum operation, Cursor cursor, Value &value);
void data_master_test_result(DataOperationEnum operation, Cursor cursor, Value &value);
void data_firmware_info(DataOperationEnum operation, Cursor cursor, Value &value);
void data_keypad_text(DataOperationEnum operation, Cursor cursor, Value &value);
void data_keypad_mode(DataOperationEnum operation, Cursor cursor, Value &value);
void data_keypad_option1_text(DataOperationEnum operation, Cursor cursor, Value &value);
void data_keypad_option1_enabled(DataOperationEnum operation, Cursor cursor, Value &value);
void data_keypad_option2_text(DataOperationEnum operation, Cursor cursor, Value &value);
void data_keypad_option2_enabled(DataOperationEnum operation, Cursor cursor, Value &value);
void data_keypad_option3_text(DataOperationEnum operation, Cursor cursor, Value &value);
void data_keypad_option3_enabled(DataOperationEnum operation, Cursor cursor, Value &value);
void data_keypad_sign_enabled(DataOperationEnum operation, Cursor cursor, Value &value);
void data_keypad_dot_enabled(DataOperationEnum operation, Cursor cursor, Value &value);
void data_keypad_unit_enabled(DataOperationEnum operation, Cursor cursor, Value &value);
void data_channel_label(DataOperationEnum operation, Cursor cursor, Value &value);
void data_channel_short_label(DataOperationEnum operation, Cursor cursor, Value &value);
void data_channel_temp_status(DataOperationEnum operation, Cursor cursor, Value &value);
void data_channel_temp(DataOperationEnum operation, Cursor cursor, Value &value);
void data_channel_on_time_total(DataOperationEnum operation, Cursor cursor, Value &value);
void data_channel_on_time_last(DataOperationEnum operation, Cursor cursor, Value &value);
void data_channel_calibration_status(DataOperationEnum operation, Cursor cursor, Value &value);
void data_channel_calibration_state(DataOperationEnum operation, Cursor cursor, Value &value);
void data_channel_calibration_date(DataOperationEnum operation, Cursor cursor, Value &value);
void data_channel_calibration_remark(DataOperationEnum operation, Cursor cursor, Value &value);
void data_channel_protection_status(DataOperationEnum operation, Cursor cursor, Value &value);
void data_channel_protection_status_ovp(DataOperationEnum operation, Cursor cursor, Value &value);
void data_channel_protection_status_ocp(DataOperationEnum operation, Cursor cursor, Value &value);
void data_channel_protection_status_opp(DataOperationEnum operation, Cursor cursor, Value &value);
void data_channel_protection_status_otp(DataOperationEnum operation, Cursor cursor, Value &value);
void data_channel_protection_ovp_state(DataOperationEnum operation, Cursor cursor, Value &value);
void data_channel_protection_ovp_type(DataOperationEnum operation, Cursor cursor, Value &value);
void data_channel_protection_ovp_level(DataOperationEnum operation, Cursor cursor, Value &value);
void data_channel_protection_ovp_delay(DataOperationEnum operation, Cursor cursor, Value &value);
void data_channel_protection_ovp_limit(DataOperationEnum operation, Cursor cursor, Value &value);
void data_channel_protection_ocp_state(DataOperationEnum operation, Cursor cursor, Value &value);
void data_channel_protection_ocp_delay(DataOperationEnum operation, Cursor cursor, Value &value);
void data_channel_protection_ocp_limit(DataOperationEnum operation, Cursor cursor, Value &value);
void data_channel_protection_ocp_max_current_limit_cause(DataOperationEnum operation, Cursor cursor, Value &value);
void data_channel_protection_opp_state(DataOperationEnum operation, Cursor cursor, Value &value);
void data_channel_protection_opp_level(DataOperationEnum operation, Cursor cursor, Value &value);
void data_channel_protection_opp_delay(DataOperationEnum operation, Cursor cursor, Value &value);
void data_channel_protection_opp_limit(DataOperationEnum operation, Cursor cursor, Value &value);
void data_channel_protection_otp_installed(DataOperationEnum operation, Cursor cursor, Value &value);
void data_channel_protection_otp_state(DataOperationEnum operation, Cursor cursor, Value &value);
void data_channel_protection_otp_level(DataOperationEnum operation, Cursor cursor, Value &value);
void data_channel_protection_otp_delay(DataOperationEnum operation, Cursor cursor, Value &value);
void data_event_queue_last_event_type(DataOperationEnum operation, Cursor cursor, Value &value);
void data_event_queue_events(DataOperationEnum operation, Cursor cursor, Value &value);
void data_event_queue_event_type(DataOperationEnum operation, Cursor cursor, Value &value);
void data_event_queue_event_message(DataOperationEnum operation, Cursor cursor, Value &value);
void data_event_queue_selected_event_message(DataOperationEnum operation, Cursor cursor, Value &value);
void data_channel_rsense_status(DataOperationEnum operation, Cursor cursor, Value &value);
void data_channel_rprog_installed(DataOperationEnum operation, Cursor cursor, Value &value);
void data_channel_rprog_status(DataOperationEnum operation, Cursor cursor, Value &value);
void data_channel_tracking_is_enabled(DataOperationEnum operation, Cursor cursor, Value &value);
void data_channel_tracking_is_allowed(DataOperationEnum operation, Cursor cursor, Value &value);
void data_channel_is_par_ser_coupled_or_tracked(DataOperationEnum operation, Cursor cursor, Value &value);
void data_coupling_type(DataOperationEnum operation, Cursor cursor, Value &value);
void data_channel_coupling_is_series(DataOperationEnum operation, Cursor cursor, Value &value);
void data_sys_on_time_total(DataOperationEnum operation, Cursor cursor, Value &value);
void data_sys_on_time_last(DataOperationEnum operation, Cursor cursor, Value &value);
void data_sys_temp_aux_status(DataOperationEnum operation, Cursor cursor, Value &value);
void data_sys_temp_aux_otp_state(DataOperationEnum operation, Cursor cursor, Value &value);
void data_sys_temp_aux_otp_level(DataOperationEnum operation, Cursor cursor, Value &value);
void data_sys_temp_aux_otp_delay(DataOperationEnum operation, Cursor cursor, Value &value);
void data_sys_temp_aux_otp_is_tripped(DataOperationEnum operation, Cursor cursor, Value &value);
void data_sys_temp_aux(DataOperationEnum operation, Cursor cursor, Value &value);
void data_sys_info_firmware_ver(DataOperationEnum operation, Cursor cursor, Value &value);
void data_sys_info_serial_no(DataOperationEnum operation, Cursor cursor, Value &value);
void data_sys_info_scpi_ver(DataOperationEnum operation, Cursor cursor, Value &value);
void data_sys_info_cpu(DataOperationEnum operation, Cursor cursor, Value &value);
void data_sys_info_fan_status(DataOperationEnum operation, Cursor cursor, Value &value);
void data_sys_info_fan_speed(DataOperationEnum operation, Cursor cursor, Value &value);
void data_channel_board_info_label(DataOperationEnum operation, Cursor cursor, Value &value);
void data_channel_board_info_revision(DataOperationEnum operation, Cursor cursor, Value &value);
void data_date_time_date(DataOperationEnum operation, Cursor cursor, Value &value);
void data_date_time_year(DataOperationEnum operation, Cursor cursor, Value &value);
void data_date_time_month(DataOperationEnum operation, Cursor cursor, Value &value);
void data_date_time_day(DataOperationEnum operation, Cursor cursor, Value &value);
void data_date_time_time(DataOperationEnum operation, Cursor cursor, Value &value);
void data_date_time_hour(DataOperationEnum operation, Cursor cursor, Value &value);
void data_date_time_minute(DataOperationEnum operation, Cursor cursor, Value &value);
void data_date_time_second(DataOperationEnum operation, Cursor cursor, Value &value);
void data_date_time_time_zone(DataOperationEnum operation, Cursor cursor, Value &value);
void data_date_time_dst(DataOperationEnum operation, Cursor cursor, Value &value);
void data_set_page_dirty(DataOperationEnum operation, Cursor cursor, Value &value);
void data_profiles_list(DataOperationEnum operation, Cursor cursor, Value &value);
void data_profiles_auto_recall_status(DataOperationEnum operation, Cursor cursor, Value &value);
void data_profiles_auto_recall_location(DataOperationEnum operation, Cursor cursor, Value &value);
void data_profile_status(DataOperationEnum operation, Cursor cursor, Value &value);
void data_profile_label(DataOperationEnum operation, Cursor cursor, Value &value);
void data_profile_remark(DataOperationEnum operation, Cursor cursor, Value &value);
void data_profile_is_auto_recall_location(DataOperationEnum operation, Cursor cursor, Value &value);
void data_profile_channel_u_set(DataOperationEnum operation, Cursor cursor, Value &value);
void data_profile_channel_i_set(DataOperationEnum operation, Cursor cursor, Value &value);
void data_profile_channel_output_state(DataOperationEnum operation, Cursor cursor, Value &value);
void data_ethernet_installed(DataOperationEnum operation, Cursor cursor, Value &value);
void data_ethernet_enabled(DataOperationEnum operation, Cursor cursor, Value &value);
void data_mqtt_enabled(DataOperationEnum operation, Cursor cursor, Value &value);
void data_ethernet_status(DataOperationEnum operation, Cursor cursor, Value &value);
void data_ethernet_and_mqtt_status(DataOperationEnum operation, Cursor cursor, Value &value);
void data_mqtt_connection_state(DataOperationEnum operation, Cursor cursor, Value &value);
void data_ethernet_ip_address(DataOperationEnum operation, Cursor cursor, Value &value);
void data_ethernet_dns(DataOperationEnum operation, Cursor cursor, Value &value);
void data_ethernet_gateway(DataOperationEnum operation, Cursor cursor, Value &value);
void data_ethernet_subnet_mask(DataOperationEnum operation, Cursor cursor, Value &value);
void data_ethernet_scpi_port(DataOperationEnum operation, Cursor cursor, Value &value);
void data_ethernet_is_connected(DataOperationEnum operation, Cursor cursor, Value &value);
void data_ethernet_dhcp(DataOperationEnum operation, Cursor cursor, Value &value);
void data_ethernet_mac(DataOperationEnum operation, Cursor cursor, Value &value);
void data_sys_output_protection_coupled(DataOperationEnum operation, Cursor cursor, Value &value);
void data_sys_shutdown_when_protection_tripped(DataOperationEnum operation, Cursor cursor, Value &value);
void data_sys_force_disabling_all_outputs_on_power_up(DataOperationEnum operation, Cursor cursor, Value &value);
void data_sys_password_is_set(DataOperationEnum operation, Cursor cursor, Value &value);
void data_sys_rl_state(DataOperationEnum operation, Cursor cursor, Value &value);
void data_sys_sound_is_enabled(DataOperationEnum operation, Cursor cursor, Value &value);
void data_sys_sound_is_click_enabled(DataOperationEnum operation, Cursor cursor, Value &value);
void data_sys_sound_enabled(DataOperationEnum operation, Cursor cursor, Value &value);
void data_channel_display_view_settings_display_values(DataOperationEnum operation, Cursor cursor, Value &value);
void data_channel_display_view_settings_display_value_label(DataOperationEnum operation, Cursor cursor, Value &value);
void data_channel_display_view_settings_display_value(DataOperationEnum operation, Cursor cursor, Value &value);
void data_channel_display_view_settings_scale(DataOperationEnum operation, Cursor cursor, Value &value);
void data_channel_display_view_settings_is_custom_scale(DataOperationEnum operation, Cursor cursor, Value &value);
void data_channel_display_view_settings_range(DataOperationEnum operation, Cursor cursor, Value &value);
void data_channel_display_view_settings_yt_view_rate(DataOperationEnum operation, Cursor cursor, Value &value);
void data_sys_encoder_confirmation_mode(DataOperationEnum operation, Cursor cursor, Value &value);
void data_sys_encoder_moving_up_speed(DataOperationEnum operation, Cursor cursor, Value &value);
void data_sys_encoder_moving_down_speed(DataOperationEnum operation, Cursor cursor, Value &value);
void data_sys_encoder_installed(DataOperationEnum operation, Cursor cursor, Value &value);
void data_sys_display_state(DataOperationEnum operation, Cursor cursor, Value &value);
void data_sys_display_brightness(DataOperationEnum operation, Cursor cursor, Value &value);
void data_channel_trigger_mode(DataOperationEnum operation, Cursor cursor, Value &value);
void data_channel_trigger_on_list_stop(DataOperationEnum operation, Cursor cursor, Value &value);
void data_channel_u_trigger_value(DataOperationEnum operation, Cursor cursor, Value &value);
void data_channel_i_trigger_value(DataOperationEnum operation, Cursor cursor, Value &value);
void data_channel_list_count(DataOperationEnum operation, Cursor cursor, Value &value);
void data_channel_lists(DataOperationEnum operation, Cursor cursor, Value &value);
void data_ramp_and_delay_list(DataOperationEnum operation, Cursor cursor, Value &value);
void data_channel_list_index(DataOperationEnum operation, Cursor cursor, Value &value);
void data_channel_list_dwell(DataOperationEnum operation, Cursor cursor, Value &value);
void data_channel_list_dwell_enabled(DataOperationEnum operation, Cursor cursor, Value &value);
void data_channel_list_voltage(DataOperationEnum operation, Cursor cursor, Value &value);
void data_channel_list_voltage_enabled(DataOperationEnum operation, Cursor cursor, Value &value);
void data_channel_list_current(DataOperationEnum operation, Cursor cursor, Value &value);
void data_channel_list_current_enabled(DataOperationEnum operation, Cursor cursor, Value &value);
void data_channel_lists_previous_page_enabled(DataOperationEnum operation, Cursor cursor, Value &value);
void data_channel_lists_next_page_enabled(DataOperationEnum operation, Cursor cursor, Value &value);
void data_channel_lists_cursor(DataOperationEnum operation, Cursor cursor, Value &value);
void data_channel_lists_insert_menu_enabled(DataOperationEnum operation, Cursor cursor, Value &value);
void data_channel_lists_delete_menu_enabled(DataOperationEnum operation, Cursor cursor, Value &value);
void data_channel_lists_delete_row_enabled(DataOperationEnum operation, Cursor cursor, Value &value);
void data_channel_lists_clear_column_enabled(DataOperationEnum operation, Cursor cursor, Value &value);
void data_channel_lists_delete_rows_enabled(DataOperationEnum operation, Cursor cursor, Value &value);
void data_channel_coupling_state(DataOperationEnum operation, Cursor cursor, Value &value);
void data_trigger_source(DataOperationEnum operation, Cursor cursor, Value &value);
void data_dlog_trigger_source(DataOperationEnum operation, Cursor cursor, Value &value);
void data_trigger_delay(DataOperationEnum operation, Cursor cursor, Value &value);
void data_trigger_initiate_continuously(DataOperationEnum operation, Cursor cursor, Value &value);
void data_trigger_is_initiated(DataOperationEnum operation, Cursor cursor, Value &value);
void data_trigger_state(DataOperationEnum operation, Cursor cursor, Value &value);
void data_trigger_is_manual(DataOperationEnum operation, Cursor cursor, Value &value);
void data_channel_has_support_for_current_dual_range(DataOperationEnum operation, Cursor cursor, Value &value);
void data_channel_ranges_supported(DataOperationEnum operation, Cursor cursor, Value &value);
void data_channel_ranges_mode(DataOperationEnum operation, Cursor cursor, Value &value);
void data_channel_ranges_auto_ranging(DataOperationEnum operation, Cursor cursor, Value &value);
void data_channel_ranges_currently_selected(DataOperationEnum operation, Cursor cursor, Value &value);
void data_text_message(DataOperationEnum operation, Cursor cursor, Value &value);
void data_serial_status(DataOperationEnum operation, Cursor cursor, Value &value);
void data_serial_is_connected(DataOperationEnum operation, Cursor cursor, Value &value);
void data_channel_list_countdown(DataOperationEnum operation, Cursor cursor, Value &value);
void data_channel_ramp_countdown(DataOperationEnum operation, Cursor cursor, Value &value);
void data_io_pins(DataOperationEnum operation, Cursor cursor, Value &value);
void data_io_pins_inhibit_state(DataOperationEnum operation, Cursor cursor, Value &value);
void data_io_pin_label(DataOperationEnum operation, Cursor cursor, Value &value);
void data_io_pin_polarity(DataOperationEnum operation, Cursor cursor, Value &value);
void data_io_pin_function_name(DataOperationEnum operation, Cursor cursor, Value &value);
void data_io_pin_uart_mode(DataOperationEnum operation, Cursor cursor, Value &value);
void data_io_pin_uart_baud(DataOperationEnum operation, Cursor cursor, Value &value);
void data_io_pin_uart_parity(DataOperationEnum operation, Cursor cursor, Value &value);
void data_io_pin_uart_data_bits(DataOperationEnum operation, Cursor cursor, Value &value);
void data_io_pin_uart_stop_bits(DataOperationEnum operation, Cursor cursor, Value &value);
void data_ntp_enabled(DataOperationEnum operation, Cursor cursor, Value &value);
void data_ntp_server(DataOperationEnum operation, Cursor cursor, Value &value);
void data_sys_display_background_luminosity_step(DataOperationEnum operation, Cursor cursor, Value &value);
void data_progress(DataOperationEnum operation, Cursor cursor, Value &value);
void data_touch_calibration_point(DataOperationEnum operation, Cursor cursor, Value &value);
void data_selected_theme(DataOperationEnum operation, Cursor cursor, Value &value);
void data_channel_title(DataOperationEnum operation, Cursor cursor, Value &value);
void data_channel_short_title(DataOperationEnum operation, Cursor cursor, Value &value);
void data_channel_short_title_without_tracking_icon(DataOperationEnum operation, Cursor cursor, Value &value);
void data_channel_long_title(DataOperationEnum operation, Cursor cursor, Value &value);
void data_animations_duration(DataOperationEnum operation, Cursor cursor, Value &value);
void data_slot_info(DataOperationEnum operation, Cursor cursor, Value &value);
void data_slot_test_result(DataOperationEnum operation, Cursor cursor, Value &value);
void data_slot_default_view(DataOperationEnum operation, Cursor cursor, Value &value);
void data_slot1_channel_index(DataOperationEnum operation, Cursor cursor, Value &value);
void data_slot2_channel_index(DataOperationEnum operation, Cursor cursor, Value &value);
void data_slot3_channel_index(DataOperationEnum operation, Cursor cursor, Value &value);
void data_slot1_non_mapped_channel_index(DataOperationEnum operation, Cursor cursor, Value &value);
void data_slot2_non_mapped_channel_index(DataOperationEnum operation, Cursor cursor, Value &value);
void data_slot3_non_mapped_channel_index(DataOperationEnum operation, Cursor cursor, Value &value);
void data_battery(DataOperationEnum operation, Cursor cursor, Value &value);
void data_io_pin_state(DataOperationEnum operation, Cursor cursor, Value &value);
void data_channel_dprog(DataOperationEnum operation, Cursor cursor, Value &value);
void data_channel_dprog_installed(DataOperationEnum operation, Cursor cursor, Value &value);
void data_sys_info_sdcard_status(DataOperationEnum operation, Cursor cursor, Value &value);
void data_sys_info_has_error(DataOperationEnum operation, Cursor cursor, Value &value);
void data_sys_settings_has_error(DataOperationEnum operation, Cursor cursor, Value &value);
void data_sys_info_battery_status(DataOperationEnum operation, Cursor cursor, Value &value);
void data_sys_fan_mode(DataOperationEnum operation, Cursor cursor, Value &value);
void data_sys_fan_speed(DataOperationEnum operation, Cursor cursor, Value &value);
void data_is_coupling_type_uncoupled(DataOperationEnum operation, Cursor cursor, Value &value);
void data_is_coupling_type_parallel(DataOperationEnum operation, Cursor cursor, Value &value);
void data_is_coupling_type_common_gnd(DataOperationEnum operation, Cursor cursor, Value &value);
void data_is_coupling_type_split_rails(DataOperationEnum operation, Cursor cursor, Value &value);
void data_is_tracking_allowed(DataOperationEnum operation, Cursor cursor, Value &value);
void data_is_coupling_parallel_allowed(DataOperationEnum operation, Cursor cursor, Value &value);
void data_is_coupling_series_allowed(DataOperationEnum operation, Cursor cursor, Value &value);
void data_channel_coupling_enable_tracking_mode(DataOperationEnum operation, Cursor cursor, Value &value);
void data_channel_active_coupled_led(DataOperationEnum operation, Cursor cursor, Value &value);
void data_is_coupling_type_series(DataOperationEnum operation, Cursor cursor, Value &value);
void data_is_coupled_or_tracked(DataOperationEnum operation, Cursor cursor, Value &value);
void data_overlay(DataOperationEnum operation, Cursor cursor, Value &value);
void data_channels_with_list_counter_visible(DataOperationEnum operation, Cursor cursor, Value &value);
void data_channels_with_ramp_counter_visible(DataOperationEnum operation, Cursor cursor, Value &value);
void data_dlog_state(DataOperationEnum operation, Cursor cursor, Value &value);
void data_dlog_toggle_state(DataOperationEnum operation, Cursor cursor, Value &value);
void data_recording(DataOperationEnum operation, Cursor cursor, Value &value);
void data_channel_history_values(DataOperationEnum operation, Cursor cursor, Value &value);
void data_recording_ready(DataOperationEnum operation, Cursor cursor, Value &value);
void data_nondrag_overlay(DataOperationEnum operation, Cursor cursor, Value &value);
void data_dlog_single_value_overlay(DataOperationEnum operation, Cursor cursor, Value &value);
void data_dlog_multiple_values_overlay(DataOperationEnum operation, Cursor cursor, Value &value);
void data_dlog_visible_values(DataOperationEnum operation, Cursor cursor, Value &value);
void data_dlog_visible_value_label(DataOperationEnum operation, Cursor cursor, Value &value);
void data_dlog_visible_value_div(DataOperationEnum operation, Cursor cursor, Value &value);
void data_dlog_visible_value_offset(DataOperationEnum operation, Cursor cursor, Value &value);
void data_dlog_visible_value_cursor(DataOperationEnum operation, Cursor cursor, Value &value);
void data_dlog_current_time(DataOperationEnum operation, Cursor cursor, Value &value);
void data_dlog_file_length(DataOperationEnum operation, Cursor cursor, Value &value);
void data_dlog_x_axis_offset(DataOperationEnum operation, Cursor cursor, Value &value);
void data_dlog_x_axis_div(DataOperationEnum operation, Cursor cursor, Value &value);
void data_dlog_x_axis_max_value(DataOperationEnum operation, Cursor cursor, Value &value);
void data_dlog_x_axis_max_value_label(DataOperationEnum operation, Cursor cursor, Value &value);
void data_is_single_page_on_stack(DataOperationEnum operation, Cursor cursor, Value &value);
void data_is_show_live_recording(DataOperationEnum operation, Cursor cursor, Value &value);
void data_dlog_view_state(DataOperationEnum operation, Cursor cursor, Value &value);
void data_dlog_period(DataOperationEnum operation, Cursor cursor, Value &value);
void data_dlog_period_spec(DataOperationEnum operation, Cursor cursor, Value &value);
void data_dlog_duration(DataOperationEnum operation, Cursor cursor, Value &value);
void data_dlog_file_name(DataOperationEnum operation, Cursor cursor, Value &value);
void data_channel_off_label(DataOperationEnum operation, Cursor cursor, Value &value);
void data_file_manager_files(DataOperationEnum operation, Cursor cursor, Value &value);
void data_file_manager_file_name(DataOperationEnum operation, Cursor cursor, Value &value);
void data_file_manager_file_date_time(DataOperationEnum operation, Cursor cursor, Value &value);
void data_file_manager_file_size(DataOperationEnum operation, Cursor cursor, Value &value);
void data_file_manager_file_selected(DataOperationEnum operation, Cursor cursor, Value &value);
void data_file_manager_state(DataOperationEnum operation, Cursor cursor, Value &value);
void data_file_manager_has_parent(DataOperationEnum operation, Cursor cursor, Value &value);
void data_file_manager_current_directory(DataOperationEnum operation, Cursor cursor, Value &value);
void data_file_manager_file_type(DataOperationEnum operation, Cursor cursor, Value &value);
void data_file_manager_open_file_enabled(DataOperationEnum operation, Cursor cursor, Value &value);
void data_file_manager_upload_file_enabled(DataOperationEnum operation, Cursor cursor, Value &value);
void data_file_manager_rename_file_enabled(DataOperationEnum operation, Cursor cursor, Value &value);
void data_file_manager_delete_file_enabled(DataOperationEnum operation, Cursor cursor, Value &value);
void data_file_manager_opened_image(DataOperationEnum operation, Cursor cursor, Value &value);
void data_script_info(DataOperationEnum operation, Cursor cursor, Value &value);
void data_script_is_started(DataOperationEnum operation, Cursor cursor, Value &value);
void data_keypad_ok_enabled(DataOperationEnum operation, Cursor cursor, Value &value);
void data_ethernet_host_name(DataOperationEnum operation, Cursor cursor, Value &value);
void data_mqtt_host(DataOperationEnum operation, Cursor cursor, Value &value);
void data_mqtt_port(DataOperationEnum operation, Cursor cursor, Value &value);
void data_mqtt_username(DataOperationEnum operation, Cursor cursor, Value &value);
void data_mqtt_password(DataOperationEnum operation, Cursor cursor, Value &value);
void data_mqtt_period(DataOperationEnum operation, Cursor cursor, Value &value);
void data_dlog_view_show_labels(DataOperationEnum operation, Cursor cursor, Value &value);
void data_is_any_coupling_allowed(DataOperationEnum operation, Cursor cursor, Value &value);
void data_is_coupling_split_rails_allowed(DataOperationEnum operation, Cursor cursor, Value &value);
void data_list_counter_label(DataOperationEnum operation, Cursor cursor, Value &value);
void data_ramp_counter_label(DataOperationEnum operation, Cursor cursor, Value &value);
void data_channel_firmware_version(DataOperationEnum operation, Cursor cursor, Value &value);
void data_file_manager_browser_title(DataOperationEnum operation, Cursor cursor, Value &value);
void data_channel_has_error_settings(DataOperationEnum operation, Cursor cursor, Value &value);
void data_is_reset_by_iwdg(DataOperationEnum operation, Cursor cursor, Value &value);
void data_file_manager_browser_is_save_dialog(DataOperationEnum operation, Cursor cursor, Value &value);
void data_file_manager_storage_info(DataOperationEnum operation, Cursor cursor, Value &value);
void data_file_manager_storage_alarm(DataOperationEnum operation, Cursor cursor, Value &value);
void data_event_queue_is_long_message_text(DataOperationEnum operation, Cursor cursor, Value &value);
void data_event_queue_event_long_message_overlay(DataOperationEnum operation, Cursor cursor, Value &value);
void data_event_queue_event_is_selected(DataOperationEnum operation, Cursor cursor, Value &value);
void data_date_time_format(DataOperationEnum operation, Cursor cursor, Value &value);
void data_date_time_format_is_dmy(DataOperationEnum operation, Cursor cursor, Value &value);
void data_date_time_format_is_24h(DataOperationEnum operation, Cursor cursor, Value &value);
void data_date_time_am_pm(DataOperationEnum operation, Cursor cursor, Value &value);
void data_is_multi_tracking(DataOperationEnum operation, Cursor cursor, Value &value);
void data_channel_copy_available(DataOperationEnum operation, Cursor cursor, Value &value);
void data_channel_rsense_installed(DataOperationEnum operation, Cursor cursor, Value &value);
void data_file_manager_sort_files_option(DataOperationEnum operation, Cursor cursor, Value &value);
void data_channel_info_brand(DataOperationEnum operation, Cursor cursor, Value &value);
void data_channel_info_serial(DataOperationEnum operation, Cursor cursor, Value &value);
void data_file_manager_list_view(DataOperationEnum operation, Cursor cursor, Value &value);
void data_file_manager_is_list_view_option_available(DataOperationEnum operation, Cursor cursor, Value &value);
void data_file_manager_file_icon(DataOperationEnum operation, Cursor cursor, Value &value);
void data_file_manager_layout(DataOperationEnum operation, Cursor cursor, Value &value);
void data_file_manager_file_description(DataOperationEnum operation, Cursor cursor, Value &value);
void data_touch_raw_x(DataOperationEnum operation, Cursor cursor, Value &value);
void data_touch_raw_y(DataOperationEnum operation, Cursor cursor, Value &value);
void data_touch_raw_z1(DataOperationEnum operation, Cursor cursor, Value &value);
void data_touch_raw_pressed(DataOperationEnum operation, Cursor cursor, Value &value);
void data_touch_calibrated_x(DataOperationEnum operation, Cursor cursor, Value &value);
void data_touch_calibrated_y(DataOperationEnum operation, Cursor cursor, Value &value);
void data_touch_calibrated_pressed(DataOperationEnum operation, Cursor cursor, Value &value);
void data_touch_filtered_x(DataOperationEnum operation, Cursor cursor, Value &value);
void data_touch_filtered_y(DataOperationEnum operation, Cursor cursor, Value &value);
void data_touch_filtered_pressed(DataOperationEnum operation, Cursor cursor, Value &value);
void data_can_show_previous_page(DataOperationEnum operation, Cursor cursor, Value &value);
void data_file_manager_file_has_description(DataOperationEnum operation, Cursor cursor, Value &value);
void data_file_manager_image_open_state(DataOperationEnum operation, Cursor cursor, Value &value);
void data_file_manager_image_open_progress(DataOperationEnum operation, Cursor cursor, Value &value);
void data_alert_message_is_set(DataOperationEnum operation, Cursor cursor, Value &value);
void data_async_progress(DataOperationEnum operation, Cursor cursor, Value &value);
void data_io_pin_function(DataOperationEnum operation, Cursor cursor, Value &value);
void data_io_pin_pwm_frequency(DataOperationEnum operation, Cursor cursor, Value &value);
void data_io_pin_pwm_duty(DataOperationEnum operation, Cursor cursor, Value &value);
void data_io_pin_is_uart_enabled(DataOperationEnum operation, Cursor cursor, Value &value);
void data_channel_voltage_ramp_duration(DataOperationEnum operation, Cursor cursor, Value &value);
void data_channel_current_ramp_duration(DataOperationEnum operation, Cursor cursor, Value &value);
void data_channel_output_delay(DataOperationEnum operation, Cursor cursor, Value &value);
void data_channel_ramp_state(DataOperationEnum operation, Cursor cursor, Value &value);
void data_ramp_and_delay_list_scrollbar_enabled(DataOperationEnum operation, Cursor cursor, Value &value);
void data_mqtt_in_error(DataOperationEnum operation, Cursor cursor, Value &value);
void data_dlog_view_legend_view_option(DataOperationEnum operation, Cursor cursor, Value &value);
void data_calibration_point_measured_value(DataOperationEnum operation, Cursor cursor, Value &value);
void data_calibration_point_set_value(DataOperationEnum operation, Cursor cursor, Value &value);
void data_channel_calibration_value_type(DataOperationEnum operation, Cursor cursor, Value &value);
void data_channel_calibration_value_is_voltage(DataOperationEnum operation, Cursor cursor, Value &value);
void data_channel_calibration_point_can_move_to_previous(DataOperationEnum operation, Cursor cursor, Value &value);
void data_channel_calibration_point_can_move_to_next(DataOperationEnum operation, Cursor cursor, Value &value);
void data_channel_calibration_point_can_delete(DataOperationEnum operation, Cursor cursor, Value &value);
void data_channel_calibration_point_info(DataOperationEnum operation, Cursor cursor, Value &value);
void data_channel_calibration_chart(DataOperationEnum operation, Cursor cursor, Value &value);
void data_channel_calibration_point_can_save(DataOperationEnum operation, Cursor cursor, Value &value);
void data_channel_calibration_chart_zoom(DataOperationEnum operation, Cursor cursor, Value &value);
void data_slots(DataOperationEnum operation, Cursor cursor, Value &value);
void data_slot_index(DataOperationEnum operation, Cursor cursor, Value &value);
void data_slot_title_def(DataOperationEnum operation, Cursor cursor, Value &value);
void data_slot_title_max(DataOperationEnum operation, Cursor cursor, Value &value);
void data_slot_title_settings(DataOperationEnum operation, Cursor cursor, Value &value);
void data_slot_short_title(DataOperationEnum operation, Cursor cursor, Value &value);
void data_slot_error_message(DataOperationEnum operation, Cursor cursor, Value &value);
void data_usb_mode(DataOperationEnum operation, Cursor cursor, Value &value);
void data_usb_current_mode(DataOperationEnum operation, Cursor cursor, Value &value);
void data_usb_device_class(DataOperationEnum operation, Cursor cursor, Value &value);
void data_usb_keyboard_state(DataOperationEnum operation, Cursor cursor, Value &value);
void data_channels_is_2col_view(DataOperationEnum operation, Cursor cursor, Value &value);
void data_display_test_color_index(DataOperationEnum operation, Cursor cursor, Value &value);
void data_channel_has_advanced_options(DataOperationEnum operation, Cursor cursor, Value &value);
void data_channel_has_firmware_update(DataOperationEnum operation, Cursor cursor, Value &value);
void data_channel_calibration_value_type_is_selectable(DataOperationEnum operation, Cursor cursor, Value &value);
void data_slot_serial_info(DataOperationEnum operation, Cursor cursor, Value &value);
void data_slot_is_enabled(DataOperationEnum operation, Cursor cursor, Value &value);
void data_slot_is_dcpsupply_module(DataOperationEnum operation, Cursor cursor, Value &value);
void data_user_switch_action(DataOperationEnum operation, Cursor cursor, Value &value);
void data_ntp_refresh_frequency(DataOperationEnum operation, Cursor cursor, Value &value);
void data_channel_calibration_is_power_channel(DataOperationEnum operation, Cursor cursor, Value &value);
void data_has_custom_bitmap(DataOperationEnum operation, Cursor cursor, Value &value);
void data_custom_bitmap(DataOperationEnum operation, Cursor cursor, Value &value);
void data_slot_info_with_fw_ver(DataOperationEnum operation, Cursor cursor, Value &value);
void data_master_info_with_fw_ver(DataOperationEnum operation, Cursor cursor, Value &value);
void data_slot1_labels_and_colors_view(DataOperationEnum operation, Cursor cursor, Value &value);
void data_slot2_labels_and_colors_view(DataOperationEnum operation, Cursor cursor, Value &value);
void data_slot3_labels_and_colors_view(DataOperationEnum operation, Cursor cursor, Value &value);
void data_colors(DataOperationEnum operation, Cursor cursor, Value &value);
void data_keypad_can_set_default(DataOperationEnum operation, Cursor cursor, Value &value);
void data_labels_and_colors_page_slot_title(DataOperationEnum operation, Cursor cursor, Value &value);
void data_labels_and_colors_page_channel_title(DataOperationEnum operation, Cursor cursor, Value &value);
void data_labels_and_colors_is_color_modified(DataOperationEnum operation, Cursor cursor, Value &value);
void data_dlog_items(DataOperationEnum operation, Cursor cursor, Value &value);
void data_dlog_items_num_selected(DataOperationEnum operation, Cursor cursor, Value &value);
void data_dlog_item_is_checked(DataOperationEnum operation, Cursor cursor, Value &value);
void data_dlog_item_channel(DataOperationEnum operation, Cursor cursor, Value &value);
void data_dlog_item_label(DataOperationEnum operation, Cursor cursor, Value &value);
void data_dlog_items_scrollbar_enabled(DataOperationEnum operation, Cursor cursor, Value &value);
void data_dlog_start_enabled(DataOperationEnum operation, Cursor cursor, Value &value);
void data_has_multiple_disk_drives(DataOperationEnum operation, Cursor cursor, Value &value);
void data_selected_mass_storage_device(DataOperationEnum operation, Cursor cursor, Value &value);
void data_file_manager_is_sort_files_option_available(DataOperationEnum operation, Cursor cursor, Value &value);
void data_dlog_preview_overlay(DataOperationEnum operation, Cursor cursor, Value &value);
void data_dlog_item_is_available(DataOperationEnum operation, Cursor cursor, Value &value);
void data_module_is_resync_supported(DataOperationEnum operation, Cursor cursor, Value &value);
void data_channel_is_calibration_value_source(DataOperationEnum operation, Cursor cursor, Value &value);
void data_calibration_point_can_edit_set_value(DataOperationEnum operation, Cursor cursor, Value &value);
void data_calibration_point_live_measured_value(DataOperationEnum operation, Cursor cursor, Value &value);
void data_ac_mains(DataOperationEnum operation, Cursor cursor, Value &value);
void data_selected_mcu_revision(DataOperationEnum operation, Cursor cursor, Value &value);
void data_is_mcu_revision_selected(DataOperationEnum operation, Cursor cursor, Value &value);
void data_slot_is_2ch_view(DataOperationEnum operation, Cursor cursor, Value &value);
void data_color_is_selected(DataOperationEnum operation, Cursor cursor, Value &value);
void data_master_error_message(DataOperationEnum operation, Cursor cursor, Value &value);
void data_dlog_period_has_predefined_values(DataOperationEnum operation, Cursor cursor, Value &value);
void data_dlog_view_selected_tab(DataOperationEnum operation, Cursor cursor, Value &value);
void data_dlog_bookmarks(DataOperationEnum operation, Cursor cursor, Value &value);
void data_dlog_bookmark_x_value(DataOperationEnum operation, Cursor cursor, Value &value);
void data_dlog_bookmark_text(DataOperationEnum operation, Cursor cursor, Value &value);
void data_dlog_bookmark_is_selected(DataOperationEnum operation, Cursor cursor, Value &value);
void data_dlog_view_is_drawer_open(DataOperationEnum operation, Cursor cursor, Value &value);
void data_dlog_view_is_zoom_in_enabled(DataOperationEnum operation, Cursor cursor, Value &value);
void data_dlog_view_is_zoom_out_enabled(DataOperationEnum operation, Cursor cursor, Value &value);
void data_dlog_bookmarks_is_scrollbar_visible(DataOperationEnum operation, Cursor cursor, Value &value);
void data_dlog_y_values_is_scrollbar_visible(DataOperationEnum operation, Cursor cursor, Value &value);
void data_dlog_y_values(DataOperationEnum operation, Cursor cursor, Value &value);
void data_dlog_y_value_label(DataOperationEnum operation, Cursor cursor, Value &value);
void data_dlog_y_value_is_checked(DataOperationEnum operation, Cursor cursor, Value &value);
void data_dlog_y_value_is_selected(DataOperationEnum operation, Cursor cursor, Value &value);
void data_dlog_y_value_is_enabled(DataOperationEnum operation, Cursor cursor, Value &value);
void data_dlog_y_value_offset(DataOperationEnum operation, Cursor cursor, Value &value);
void data_dlog_y_value_div(DataOperationEnum operation, Cursor cursor, Value &value);
void data_dlog_y_value_cursor(DataOperationEnum operation, Cursor cursor, Value &value);
void data_function_generator_channels(DataOperationEnum operation, Cursor cursor, Value &value);
void data_function_generator_any_channel_selected(DataOperationEnum operation, Cursor cursor, Value &value);
void data_function_generator_channel(DataOperationEnum operation, Cursor cursor, Value &value);
void data_function_generator_item_label(DataOperationEnum operation, Cursor cursor, Value &value);
void data_function_generator_selected_item_label(DataOperationEnum operation, Cursor cursor, Value &value);
void data_function_generator_item_is_checked(DataOperationEnum operation, Cursor cursor, Value &value);
void data_function_generator_is_max_selected(DataOperationEnum operation, Cursor cursor, Value &value);
void data_function_generator_num_selected(DataOperationEnum operation, Cursor cursor, Value &value);
void data_function_generator_item_is_selected(DataOperationEnum operation, Cursor cursor, Value &value);
void data_function_generator_waveform(DataOperationEnum operation, Cursor cursor, Value &value);
void data_function_generator_waveform_label(DataOperationEnum operation, Cursor cursor, Value &value);
void data_function_generator_frequency(DataOperationEnum operation, Cursor cursor, Value &value);
void data_function_generator_mode(DataOperationEnum operation, Cursor cursor, Value &value);
void data_function_generator_phase_shift(DataOperationEnum operation, Cursor cursor, Value &value);
void data_function_generator_amplitude(DataOperationEnum operation, Cursor cursor, Value &value);
void data_function_generator_offset(DataOperationEnum operation, Cursor cursor, Value &value);
void data_function_generator_duty_cycle(DataOperationEnum operation, Cursor cursor, Value &value);
void data_function_generator_canvas(DataOperationEnum operation, Cursor cursor, Value &value);
void data_function_generator_has_mode_select(DataOperationEnum operation, Cursor cursor, Value &value);
void data_function_generator_has_amplitude_and_offset(DataOperationEnum operation, Cursor cursor, Value &value);
void data_function_generator_has_duty_cycle(DataOperationEnum operation, Cursor cursor, Value &value);
void data_function_generator_is_freq(DataOperationEnum operation, Cursor cursor, Value &value);
void data_function_generator_is_ampl(DataOperationEnum operation, Cursor cursor, Value &value);
void data_function_generator_is_dc(DataOperationEnum operation, Cursor cursor, Value &value);
void data_function_generator_any_selected(DataOperationEnum operation, Cursor cursor, Value &value);
void data_overlay_minimized(DataOperationEnum operation, Cursor cursor, Value &value);
void data_overlay_hidden(DataOperationEnum operation, Cursor cursor, Value &value);
void data_next_channel_in_max_view_button_enabled(DataOperationEnum operation, Cursor cursor, Value &value);
void data_prev_channel_in_max_view_button_enabled(DataOperationEnum operation, Cursor cursor, Value &value);
void data_is_toggle_channels_view_mode_enabled(DataOperationEnum operation, Cursor cursor, Value &value);
void data_dlog_has_bookmark(DataOperationEnum operation, Cursor cursor, Value &value);
void data_function_generator_is_any_channel_active(DataOperationEnum operation, Cursor cursor, Value &value);
void data_trigger_initiate_all(DataOperationEnum operation, Cursor cursor, Value &value);
void data_channel_ocp_trip_level_info(DataOperationEnum operation, Cursor cursor, Value &value);
void data_function_generator_amplitude_label(DataOperationEnum operation, Cursor cursor, Value &value);
void data_dib_mio168_din_pins(DataOperationEnum operation, Cursor cursor, Value &value);
void data_dib_mio168_din_pins_1_4(DataOperationEnum operation, Cursor cursor, Value &value);
void data_dib_mio168_din_pins_5_8(DataOperationEnum operation, Cursor cursor, Value &value);
void data_dib_mio168_din_no(DataOperationEnum operation, Cursor cursor, Value &value);
void data_dib_mio168_din_state(DataOperationEnum operation, Cursor cursor, Value &value);
void data_dib_mio168_din_range(DataOperationEnum operation, Cursor cursor, Value &value);
void data_dib_mio168_din_label(DataOperationEnum operation, Cursor cursor, Value &value);
void data_dib_mio168_din_label_label(DataOperationEnum operation, Cursor cursor, Value &value);
void data_dib_mio168_din_speed(DataOperationEnum operation, Cursor cursor, Value &value);
void data_dib_mio168_dout_pins(DataOperationEnum operation, Cursor cursor, Value &value);
void data_dib_mio168_dout_pins_1_4(DataOperationEnum operation, Cursor cursor, Value &value);
void data_dib_mio168_dout_pins_5_8(DataOperationEnum operation, Cursor cursor, Value &value);
void data_dib_mio168_dout_no(DataOperationEnum operation, Cursor cursor, Value &value);
void data_dib_mio168_dout_state(DataOperationEnum operation, Cursor cursor, Value &value);
void data_dib_mio168_dout_label(DataOperationEnum operation, Cursor cursor, Value &value);
void data_dib_mio168_dout_label_label(DataOperationEnum operation, Cursor cursor, Value &value);
void data_dib_mio168_dout_trigger_mode(DataOperationEnum operation, Cursor cursor, Value &value);
void data_dib_mio168_dout_trigger_is_initiated(DataOperationEnum operation, Cursor cursor, Value &value);
void data_dib_mio168_dout_function_label(DataOperationEnum operation, Cursor cursor, Value &value);
void data_dib_mio168_ain_channels(DataOperationEnum operation, Cursor cursor, Value &value);
void data_dib_mio168_ain_label(DataOperationEnum operation, Cursor cursor, Value &value);
void data_dib_mio168_ain_label_short(DataOperationEnum operation, Cursor cursor, Value &value);
void data_dib_mio168_ain_label_label(DataOperationEnum operation, Cursor cursor, Value &value);
void data_dib_mio168_ain_value(DataOperationEnum operation, Cursor cursor, Value &value);
void data_dib_mio168_ain_mode(DataOperationEnum operation, Cursor cursor, Value &value);
void data_dib_mio168_ain_range(DataOperationEnum operation, Cursor cursor, Value &value);
void data_dib_mio168_ain_is_dlog_active(DataOperationEnum operation, Cursor cursor, Value &value);
void data_dib_mio168_ain_mode_and_range(DataOperationEnum operation, Cursor cursor, Value &value);
void data_dib_mio168_ain_mode_is_multiple_selection_available(DataOperationEnum operation, Cursor cursor, Value &value);
void data_dib_mio168_ain_range_is_multiple_selection_available(DataOperationEnum operation, Cursor cursor, Value &value);
void data_dib_mio168_ain_is_overflow(DataOperationEnum operation, Cursor cursor, Value &value);
void data_dib_mio168_ain_has_power_column(DataOperationEnum operation, Cursor cursor, Value &value);
void data_dib_mio168_ain_has_p1(DataOperationEnum operation, Cursor cursor, Value &value);
void data_dib_mio168_ain_p1(DataOperationEnum operation, Cursor cursor, Value &value);
void data_dib_mio168_ain_has_p2(DataOperationEnum operation, Cursor cursor, Value &value);
void data_dib_mio168_ain_p2(DataOperationEnum operation, Cursor cursor, Value &value);
void data_dib_mio168_ain_has_efficiency(DataOperationEnum operation, Cursor cursor, Value &value);
void data_dib_mio168_ain_calc_efficiency(DataOperationEnum operation, Cursor cursor, Value &value);
void data_dib_mio168_ain_efficiency(DataOperationEnum operation, Cursor cursor, Value &value);
void data_dib_mio168_ain_active_power(DataOperationEnum operation, Cursor cursor, Value &value);
void data_dib_mio168_ain_reactive_power(DataOperationEnum operation, Cursor cursor, Value &value);
void data_dib_mio168_ain_volt_rms(DataOperationEnum operation, Cursor cursor, Value &value);
void data_dib_mio168_ain_curr_rms(DataOperationEnum operation, Cursor cursor, Value &value);
void data_dib_mio168_ain_apparent_power(DataOperationEnum operation, Cursor cursor, Value &value);
void data_dib_mio168_ain_power_factor(DataOperationEnum operation, Cursor cursor, Value &value);
void data_dib_mio168_ain_nplc(DataOperationEnum operation, Cursor cursor, Value &value);
void data_dib_mio168_ain_aperture(DataOperationEnum operation, Cursor cursor, Value &value);
void data_dib_mio168_ain_efficiency_formula(DataOperationEnum operation, Cursor cursor, Value &value);
void data_dib_mio168_ain_is_auto_range(DataOperationEnum operation, Cursor cursor, Value &value);
void data_dib_mio168_ain_is_error(DataOperationEnum operation, Cursor cursor, Value &value);
void data_dib_mio168_is_no_afe(DataOperationEnum operation, Cursor cursor, Value &value);
void data_dib_mio168_ac_analysis_enabled(DataOperationEnum operation, Cursor cursor, Value &value);
void data_dib_mio168_aout_channels(DataOperationEnum operation, Cursor cursor, Value &value);
void data_dib_mio168_aout_label(DataOperationEnum operation, Cursor cursor, Value &value);
void data_dib_mio168_aout_label_short(DataOperationEnum operation, Cursor cursor, Value &value);
void data_dib_mio168_aout_label_label(DataOperationEnum operation, Cursor cursor, Value &value);
void data_dib_mio168_aout_value(DataOperationEnum operation, Cursor cursor, Value &value);
void data_dib_mio168_aout_value_is_off(DataOperationEnum operation, Cursor cursor, Value &value);
void data_dib_mio168_ac_analysis_allowed(DataOperationEnum operation, Cursor cursor, Value &value);
void data_dib_mio168_aout_output_enabled(DataOperationEnum operation, Cursor cursor, Value &value);
void data_dib_mio168_aout_output_mode(DataOperationEnum operation, Cursor cursor, Value &value);
void data_dib_mio168_aout_voltage_range(DataOperationEnum operation, Cursor cursor, Value &value);
void data_dib_mio168_aout_current_range(DataOperationEnum operation, Cursor cursor, Value &value);
void data_dib_mio168_aout_channel_has_settings(DataOperationEnum operation, Cursor cursor, Value &value);
void data_dib_mio168_aout_mode_and_range(DataOperationEnum operation, Cursor cursor, Value &value);
void data_dib_mio168_aout_trigger_mode(DataOperationEnum operation, Cursor cursor, Value &value);
void data_dib_mio168_aout_trigger_is_initiated(DataOperationEnum operation, Cursor cursor, Value &value);
void data_dib_mio168_aout_function_label(DataOperationEnum operation, Cursor cursor, Value &value);
void data_dib_mio168_pwm_channels(DataOperationEnum operation, Cursor cursor, Value &value);
void data_dib_mio168_pwm_label(DataOperationEnum operation, Cursor cursor, Value &value);
void data_dib_mio168_pwm_label_short(DataOperationEnum operation, Cursor cursor, Value &value);
void data_dib_mio168_pwm_label_label(DataOperationEnum operation, Cursor cursor, Value &value);
void data_dib_mio168_pwm_freq(DataOperationEnum operation, Cursor cursor, Value &value);
void data_dib_mio168_pwm_duty(DataOperationEnum operation, Cursor cursor, Value &value);
void data_dib_mio168_din_has_speed(DataOperationEnum operation, Cursor cursor, Value &value);
void data_dib_mio168_afe_version(DataOperationEnum operation, Cursor cursor, Value &value);
void data_dib_mio168_pager_list(DataOperationEnum operation, Cursor cursor, Value &value);
void data_dib_mio168_pager_is_selected(DataOperationEnum operation, Cursor cursor, Value &value);
void data_dib_mio168_pager_selected_page(DataOperationEnum operation, Cursor cursor, Value &value);
void data_dib_mio168_selected_page_in_default_view(DataOperationEnum operation, Cursor cursor, Value &value);
void data_dib_mio168_is_settings_page_selected(DataOperationEnum operation, Cursor cursor, Value &value);
void data_dib_prel6_relays(DataOperationEnum operation, Cursor cursor, Value &value);
void data_dib_prel6_relay_is_on(DataOperationEnum operation, Cursor cursor, Value &value);
void data_dib_prel6_relay_label(DataOperationEnum operation, Cursor cursor, Value &value);
void data_dib_prel6_relay_cycles(DataOperationEnum operation, Cursor cursor, Value &value);
void data_dib_prel6_relay_label_label(DataOperationEnum operation, Cursor cursor, Value &value);
void data_dib_prel6_is_relay_1_or_6(DataOperationEnum operation, Cursor cursor, Value &value);
void data_dib_smx46_routes(DataOperationEnum operation, Cursor cursor, Value &value);
void data_dib_smx46_route_open(DataOperationEnum operation, Cursor cursor, Value &value);
void data_dib_smx46_x_labels(DataOperationEnum operation, Cursor cursor, Value &value);
void data_dib_smx46_x_label(DataOperationEnum operation, Cursor cursor, Value &value);
void data_dib_smx46_y_labels(DataOperationEnum operation, Cursor cursor, Value &value);
void data_dib_smx46_y_label(DataOperationEnum operation, Cursor cursor, Value &value);
void data_dib_smx46_aout_channels(DataOperationEnum operation, Cursor cursor, Value &value);
void data_dib_smx46_aout_value(DataOperationEnum operation, Cursor cursor, Value &value);
void data_dib_smx46_aout_label(DataOperationEnum operation, Cursor cursor, Value &value);
void data_dib_smx46_aout_trigger_mode(DataOperationEnum operation, Cursor cursor, Value &value);
void data_dib_smx46_aout_trigger_is_initiated(DataOperationEnum operation, Cursor cursor, Value &value);
void data_dib_smx46_aout_function_label(DataOperationEnum operation, Cursor cursor, Value &value);
void data_dib_smx46_relay_on(DataOperationEnum operation, Cursor cursor, Value &value);
void data_dib_smx46_signal_relay_cycles(DataOperationEnum operation, Cursor cursor, Value &value);
void data_dib_smx46_power_relay_cycles(DataOperationEnum operation, Cursor cursor, Value &value);
void data_dib_dcm220_slot_def_2ch_view(DataOperationEnum operation, Cursor cursor, Value &value);
void data_dib_dcm220_slot_2ch_ch1_index(DataOperationEnum operation, Cursor cursor, Value &value);
void data_dib_dcm220_slot_2ch_ch2_index(DataOperationEnum operation, Cursor cursor, Value &value);
void data_dib_dcm224_pwm_frequency(DataOperationEnum operation, Cursor cursor, Value &value);
void data_dib_dcm224_pwm_duty(DataOperationEnum operation, Cursor cursor, Value &value);
void data_dib_dcm224_counterphase_frequency(DataOperationEnum operation, Cursor cursor, Value &value);
void data_dib_dcm224_counterphase_dithering(DataOperationEnum operation, Cursor cursor, Value &value);
void data_dib_dcm224_pwm_enabled(DataOperationEnum operation, Cursor cursor, Value &value);
void data_dib_dcm224_slot_def_2ch_view(DataOperationEnum operation, Cursor cursor, Value &value);
void data_dib_dcm224_slot_2ch_ch1_index(DataOperationEnum operation, Cursor cursor, Value &value);
void data_dib_dcm224_slot_2ch_ch2_index(DataOperationEnum operation, Cursor cursor, Value &value);
void data_dib_mux14d_p1_relays(DataOperationEnum operation, Cursor cursor, Value &value);
void data_dib_mux14d_p1_relay_label(DataOperationEnum operation, Cursor cursor, Value &value);
void data_dib_mux14d_p1_relay_is_first(DataOperationEnum operation, Cursor cursor, Value &value);
void data_dib_mux14d_p1_relay_is_on(DataOperationEnum operation, Cursor cursor, Value &value);
void data_dib_mux14d_p1_relay_cycles(DataOperationEnum operation, Cursor cursor, Value &value);
void data_dib_mux14d_p1_relay_label_label(DataOperationEnum operation, Cursor cursor, Value &value);
void data_dib_mux14d_p2_relays(DataOperationEnum operation, Cursor cursor, Value &value);
void data_dib_mux14d_p2_relay_label(DataOperationEnum operation, Cursor cursor, Value &value);
void data_dib_mux14d_p2_relay_is_first(DataOperationEnum operation, Cursor cursor, Value &value);
void data_dib_mux14d_p2_relay_is_on(DataOperationEnum operation, Cursor cursor, Value &value);
void data_dib_mux14d_p2_relay_cycles(DataOperationEnum operation, Cursor cursor, Value &value);
void data_dib_mux14d_p2_relay_label_label(DataOperationEnum operation, Cursor cursor, Value &value);
void data_dib_mux14d_ext_relay_is_on(DataOperationEnum operation, Cursor cursor, Value &value);
void data_dib_mux14d_ext_relay_cycles(DataOperationEnum operation, Cursor cursor, Value &value);
void data_dib_mux14d_adib1_relay_is_on(DataOperationEnum operation, Cursor cursor, Value &value);
void data_dib_mux14d_adib1_relay_cycles(DataOperationEnum operation, Cursor cursor, Value &value);
void data_dib_mux14d_adib2_relay_is_on(DataOperationEnum operation, Cursor cursor, Value &value);
void data_dib_mux14d_adib2_relay_cycles(DataOperationEnum operation, Cursor cursor, Value &value);
void data_dib_mux14d_cj_temp(DataOperationEnum operation, Cursor cursor, Value &value);
void data_dib_mux14d_multiple_connections(DataOperationEnum operation, Cursor cursor, Value &value);

typedef void (*DataOperationsFunction)(DataOperationEnum operation, Cursor cursor, Value &value);

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
    ACTION_ID_SHOW_SYS_SETTINGS_SCREEN_CALIBRATION = 35,
    ACTION_ID_SHOW_SYS_SETTINGS_DISPLAY = 36,
    ACTION_ID_SHOW_SYS_SETTINGS_SERIAL = 37,
    ACTION_ID_SHOW_SYS_SETTINGS_ETHERNET = 38,
    ACTION_ID_SHOW_SYS_SETTINGS_PROTECTIONS = 39,
    ACTION_ID_SHOW_SYS_SETTINGS_AUX_OTP = 40,
    ACTION_ID_SHOW_SYS_SETTINGS_SOUND = 41,
    ACTION_ID_SHOW_SYS_SETTINGS_ENCODER = 42,
    ACTION_ID_SHOW_SYS_INFO = 43,
    ACTION_ID_SHOW_EDIT_MODE_STEP_HELP = 44,
    ACTION_ID_SHOW_EDIT_MODE_SLIDER_HELP = 45,
    ACTION_ID_SHOW_CH_SETTINGS = 46,
    ACTION_ID_SHOW_CH_SETTINGS_PROT_OCP = 47,
    ACTION_ID_SHOW_CH_SETTINGS_PROT_OVP = 48,
    ACTION_ID_SHOW_CH_SETTINGS_PROT_OPP = 49,
    ACTION_ID_SHOW_CH_SETTINGS_PROT_OTP = 50,
    ACTION_ID_SHOW_CH_SETTINGS_TRIGGER = 51,
    ACTION_ID_SHOW_CH_SETTINGS_LISTS = 52,
    ACTION_ID_SHOW_CH_SETTINGS_ADV_OPTIONS = 53,
    ACTION_ID_SHOW_CH_SETTINGS_ADV_RANGES = 54,
    ACTION_ID_SHOW_CH_SETTINGS_ADV_VIEW = 55,
    ACTION_ID_SHOW_CH_SETTINGS_INFO = 56,
    ACTION_ID_SHOW_CH_SETTINGS_CAL = 57,
    ACTION_ID_EDIT_CALIBRATION_PASSWORD = 58,
    ACTION_ID_CH_SETTINGS_CALIBRATION_START_CALIBRATION = 59,
    ACTION_ID_CH_SETTINGS_CALIBRATION_TOGGLE_ENABLE = 60,
    ACTION_ID_CH_SETTINGS_PROT_CLEAR = 61,
    ACTION_ID_CH_SETTINGS_PROT_TOGGLE_STATE = 62,
    ACTION_ID_CH_SETTINGS_PROT_TOGGLE_TYPE = 63,
    ACTION_ID_CH_SETTINGS_PROT_EDIT_LIMIT = 64,
    ACTION_ID_CH_SETTINGS_PROT_EDIT_LEVEL = 65,
    ACTION_ID_CH_SETTINGS_PROT_EDIT_DELAY = 66,
    ACTION_ID_SET = 67,
    ACTION_ID_DISCARD = 68,
    ACTION_ID_EDIT_FIELD = 69,
    ACTION_ID_CH_SETTINGS_ADV_REMOTE_TOGGLE_SENSE = 70,
    ACTION_ID_CH_SETTINGS_ADV_REMOTE_TOGGLE_PROGRAMMING = 71,
    ACTION_ID_DATE_TIME_SELECT_DST_RULE = 72,
    ACTION_ID_SHOW_USER_PROFILES = 73,
    ACTION_ID_SHOW_USER_PROFILE_SETTINGS = 74,
    ACTION_ID_PROFILES_TOGGLE_AUTO_RECALL = 75,
    ACTION_ID_PROFILE_TOGGLE_IS_AUTO_RECALL_LOCATION = 76,
    ACTION_ID_PROFILE_RECALL = 77,
    ACTION_ID_PROFILE_SAVE = 78,
    ACTION_ID_PROFILE_DELETE = 79,
    ACTION_ID_PROFILE_EDIT_REMARK = 80,
    ACTION_ID_TOGGLE_CHANNELS_VIEW_MODE = 81,
    ACTION_ID_ETHERNET_TOGGLE = 82,
    ACTION_ID_MQTT_TOGGLE = 83,
    ACTION_ID_ETHERNET_TOGGLE_DHCP = 84,
    ACTION_ID_ETHERNET_EDIT_MAC_ADDRESS = 85,
    ACTION_ID_ETHERNET_EDIT_STATIC_ADDRESS = 86,
    ACTION_ID_ETHERNET_EDIT_IP_ADDRESS = 87,
    ACTION_ID_ETHERNET_EDIT_DNS = 88,
    ACTION_ID_ETHERNET_EDIT_GATEWAY = 89,
    ACTION_ID_ETHERNET_EDIT_SUBNET_MASK = 90,
    ACTION_ID_ETHERNET_EDIT_SCPI_PORT = 91,
    ACTION_ID_SYS_SETTINGS_PROTECTIONS_TOGGLE_OUTPUT_PROTECTION_COUPLE = 92,
    ACTION_ID_SYS_SETTINGS_PROTECTIONS_TOGGLE_SHUTDOWN_WHEN_PROTECTION_TRIPPED = 93,
    ACTION_ID_SYS_SETTINGS_PROTECTIONS_TOGGLE_FORCE_DISABLING_ALL_OUTPUTS_ON_POWER_UP = 94,
    ACTION_ID_SYS_SETTINGS_PROTECTIONS_AUX_OTP_TOGGLE_STATE = 95,
    ACTION_ID_SYS_SETTINGS_PROTECTIONS_AUX_OTP_EDIT_LEVEL = 96,
    ACTION_ID_SYS_SETTINGS_PROTECTIONS_AUX_OTP_EDIT_DELAY = 97,
    ACTION_ID_SYS_SETTINGS_PROTECTIONS_AUX_OTP_CLEAR = 98,
    ACTION_ID_EDIT_SYSTEM_PASSWORD = 99,
    ACTION_ID_SYS_FRONT_PANEL_LOCK = 100,
    ACTION_ID_SYS_FRONT_PANEL_UNLOCK = 101,
    ACTION_ID_SYS_SETTINGS_SOUND_TOGGLE = 102,
    ACTION_ID_SYS_SETTINGS_SOUND_TOGGLE_CLICK = 103,
    ACTION_ID_CH_SETTINGS_ADV_VIEW_EDIT = 104,
    ACTION_ID_SYS_SETTINGS_ENCODER_TOGGLE_CONFIRMATION_MODE = 105,
    ACTION_ID_TURN_DISPLAY_OFF = 106,
    ACTION_ID_CH_SETTINGS_TRIGGER_EDIT_TRIGGER_MODE = 107,
    ACTION_ID_CH_SETTINGS_LISTS_EDIT_LIST_COUNT = 108,
    ACTION_ID_CH_SETTINGS_LISTS_EDIT_ON_LIST_STOP = 109,
    ACTION_ID_CHANNEL_LISTS_PREVIOUS_PAGE = 110,
    ACTION_ID_CHANNEL_LISTS_NEXT_PAGE = 111,
    ACTION_ID_CHANNEL_LISTS_EDIT = 112,
    ACTION_ID_SHOW_CHANNEL_LISTS_INSERT_MENU = 113,
    ACTION_ID_SHOW_CHANNEL_LISTS_DELETE_MENU = 114,
    ACTION_ID_CHANNEL_LISTS_INSERT_ROW_ABOVE = 115,
    ACTION_ID_CHANNEL_LISTS_INSERT_ROW_BELOW = 116,
    ACTION_ID_CHANNEL_LISTS_DELETE_ROW = 117,
    ACTION_ID_CHANNEL_LISTS_CLEAR_COLUMN = 118,
    ACTION_ID_CHANNEL_LISTS_DELETE_ROWS = 119,
    ACTION_ID_CHANNEL_LISTS_DELETE_ALL = 120,
    ACTION_ID_TRIGGER_SELECT_SOURCE = 121,
    ACTION_ID_TRIGGER_EDIT_DELAY = 122,
    ACTION_ID_TRIGGER_TOGGLE_INITIATE_CONTINUOUSLY = 123,
    ACTION_ID_TRIGGER_GENERATE_MANUAL = 124,
    ACTION_ID_TRIGGER_SHOW_GENERAL_SETTINGS = 125,
    ACTION_ID_SHOW_STAND_BY_MENU = 126,
    ACTION_ID_RESET = 127,
    ACTION_ID_CH_SETTINGS_ADV_RANGES_SELECT_MODE = 128,
    ACTION_ID_CH_SETTINGS_ADV_RANGES_TOGGLE_AUTO_RANGING = 129,
    ACTION_ID_IO_PIN_TOGGLE_POLARITY = 130,
    ACTION_ID_IO_PIN_SELECT_FUNCTION = 131,
    ACTION_ID_IO_PIN_SELECT_UART_MODE = 132,
    ACTION_ID_IO_PIN_SELECT_UART_BAUD = 133,
    ACTION_ID_IO_PIN_SELECT_UART_PARITY = 134,
    ACTION_ID_IO_PIN_SELECT_UART_DATA_BITS = 135,
    ACTION_ID_IO_PIN_SELECT_UART_STOP_BITS = 136,
    ACTION_ID_NTP_TOGGLE = 137,
    ACTION_ID_NTP_EDIT_SERVER = 138,
    ACTION_ID_SELECT_THEME = 139,
    ACTION_ID_TOGGLE_CHANNELS_MAX_VIEW = 140,
    ACTION_ID_EDIT_ANIMATIONS_DURATION = 141,
    ACTION_ID_RESTART = 142,
    ACTION_ID_USER_SWITCH_CLICKED = 143,
    ACTION_ID_IO_PIN_TOGGLE_STATE = 144,
    ACTION_ID_CH_SETTINGS_ADV_TOGGLE_DPROG = 145,
    ACTION_ID_SYS_SETTINGS_FAN_TOGGLE_MODE = 146,
    ACTION_ID_SYS_SETTINGS_FAN_EDIT_SPEED = 147,
    ACTION_ID_SHOW_SYS_SETTINGS_TRACKING = 148,
    ACTION_ID_SHOW_SYS_SETTINGS_COUPLING = 149,
    ACTION_ID_SET_COUPLING_UNCOUPLED = 150,
    ACTION_ID_SET_COUPLING_PARALLEL = 151,
    ACTION_ID_SET_COUPLING_SERIES = 152,
    ACTION_ID_SET_COUPLING_COMMON_GND = 153,
    ACTION_ID_SET_COUPLING_SPLIT_RAILS = 154,
    ACTION_ID_TOGGLE_CHANNEL_TRACKING = 155,
    ACTION_ID_TOGGLE_ENABLE_TRACKING_MODE_IN_COUPLING = 156,
    ACTION_ID_DRAG_OVERLAY = 157,
    ACTION_ID_DLOG_TOGGLE = 158,
    ACTION_ID_SHOW_DLOG_VIEW = 159,
    ACTION_ID_SELECT_USER_SWITCH_ACTION = 160,
    ACTION_ID_SHOW_FILE_MANAGER = 161,
    ACTION_ID_FILE_MANAGER_SELECT_FILE = 162,
    ACTION_ID_FILE_MANAGER_GO_TO_PARENT = 163,
    ACTION_ID_FILE_MANAGER_OPEN_FILE = 164,
    ACTION_ID_FILE_MANAGER_UPLOAD_FILE = 165,
    ACTION_ID_FILE_MANAGER_RENAME_FILE = 166,
    ACTION_ID_FILE_MANAGER_DELETE_FILE = 167,
    ACTION_ID_SHOW_DLOG_PARAMS = 168,
    ACTION_ID_DLOG_EDIT_PERIOD = 169,
    ACTION_ID_DLOG_EDIT_DURATION = 170,
    ACTION_ID_DLOG_EDIT_FILE_NAME = 171,
    ACTION_ID_DLOG_VIEW_TOGGLE_DRAWER = 172,
    ACTION_ID_FILE_MANAGER_SORT_BY = 173,
    ACTION_ID_DLOG_AUTO_SCALE = 174,
    ACTION_ID_DLOG_UPLOAD = 175,
    ACTION_ID_ETHERNET_EDIT_HOST_NAME = 176,
    ACTION_ID_MQTT_EDIT_HOST = 177,
    ACTION_ID_MQTT_EDIT_PORT = 178,
    ACTION_ID_MQTT_EDIT_USERNAME = 179,
    ACTION_ID_MQTT_EDIT_PASSWORD = 180,
    ACTION_ID_MQTT_EDIT_PERIOD = 181,
    ACTION_ID_SHOW_SYS_SETTINGS_MQTT = 182,
    ACTION_ID_DLOG_SCALE_TO_FIT = 183,
    ACTION_ID_DLOG_VIEW_TOGGLE_LABELS = 184,
    ACTION_ID_DLOG_VIEW_SELECT_VISIBLE_VALUE = 185,
    ACTION_ID_CHANNEL_UPDATE_FIRMWARE = 186,
    ACTION_ID_SHUTDOWN = 187,
    ACTION_ID_SHOW_CHANNEL_LISTS_FILE_MENU = 188,
    ACTION_ID_CHANNEL_LISTS_FILE_IMPORT = 189,
    ACTION_ID_CHANNEL_LISTS_FILE_EXPORT = 190,
    ACTION_ID_FILE_MANAGER_NEW_FILE = 191,
    ACTION_ID_PROFILE_IMPORT = 192,
    ACTION_ID_PROFILE_EXPORT = 193,
    ACTION_ID_EVENT_QUEUE_FILTER = 194,
    ACTION_ID_EVENT_QUEUE_SELECT_EVENT = 195,
    ACTION_ID_DATE_TIME_SELECT_FORMAT = 196,
    ACTION_ID_DATE_TIME_TOGGLE_AM_PM = 197,
    ACTION_ID_EDIT_NO_FOCUS = 198,
    ACTION_ID_UNTRACK_ALL = 199,
    ACTION_ID_CH_SETTINGS_COPY = 200,
    ACTION_ID_SHOW_SYS_SETTINGS_ETHERNET_ERROR = 201,
    ACTION_ID_FILE_MANAGER_SELECT_LIST_VIEW = 202,
    ACTION_ID_SHOW_SYS_SETTINGS_RAMP_AND_DELAY = 203,
    ACTION_ID_CHANNEL_TOGGLE_RAMP_STATE = 204,
    ACTION_ID_DLOG_VIEW_SELECT_LEGEND_VIEW_OPTION = 205,
    ACTION_ID_SELECT_CHANNEL_CALIBRATION_VALUE_TYPE = 206,
    ACTION_ID_CHANNEL_CALIBRATION_POINT_NEXT = 207,
    ACTION_ID_CHANNEL_CALIBRATION_POINT_PREVIOUS = 208,
    ACTION_ID_CHANNEL_CALIBRATION_POINT_DELETE = 209,
    ACTION_ID_CHANNEL_CALIBRATION_POINT_SAVE = 210,
    ACTION_ID_CHANNEL_CALIBRATION_CHART_ZOOM = 211,
    ACTION_ID_CH_SETTINGS_CALIBRATION_VIEW_POINTS = 212,
    ACTION_ID_SHOW_TOUCH_CALIBRATION_INTRO = 213,
    ACTION_ID_TOGGLE_SLOT_MAX_VIEW = 214,
    ACTION_ID_SHOW_SLOT_SETTINGS = 215,
    ACTION_ID_SELECT_USB_MODE = 216,
    ACTION_ID_SELECT_USB_DEVICE_CLASS = 217,
    ACTION_ID_TOGGLE_DISPLAY_TEST_COLOR_INDEX = 218,
    ACTION_ID_SHOW_DISPLAY_TEST_PAGE = 219,
    ACTION_ID_EDIT_NTP_REFRESH_FREQUENCY = 220,
    ACTION_ID_KEYPAD_OPTION3 = 221,
    ACTION_ID_SHOW_LABELS_AND_COLORS = 222,
    ACTION_ID_CHANGE_SLOT_LABEL = 223,
    ACTION_ID_CHANGE_CHANNEL_LABEL = 224,
    ACTION_ID_SHOW_CHANNEL_COLOR_PICKER = 225,
    ACTION_ID_PICK_COLOR = 226,
    ACTION_ID_KEYPAD_SET_DEFAULT = 227,
    ACTION_ID_SHOW_SLOT_COLOR_PICKER = 228,
    ACTION_ID_PICK_DEFAULT_COLOR = 229,
    ACTION_ID_DLOG_ITEM_TOGGLE = 230,
    ACTION_ID_DLOG_TRIGGER_SELECT_SOURCE = 231,
    ACTION_ID_SELECT_MASS_STORAGE_DEVICE = 232,
    ACTION_ID_MODULE_RESYNC = 233,
    ACTION_ID_SELECT_AC_MAINS = 234,
    ACTION_ID_SHOW_SELECT_MCU_REVISION_PAGE = 235,
    ACTION_ID_SELECT_R2B4_REVISION = 236,
    ACTION_ID_SELECT_R3B3_REVISION = 237,
    ACTION_ID_SELECT_MCU_REVISION = 238,
    ACTION_ID_CH_SETTINGS_OVP_PROT_CLEAR = 239,
    ACTION_ID_CH_SETTINGS_OCP_PROT_CLEAR = 240,
    ACTION_ID_CH_SETTINGS_OPP_PROT_CLEAR = 241,
    ACTION_ID_CH_SETTINGS_OTP_PROT_CLEAR = 242,
    ACTION_ID_DLOG_PERIOD_SELECT_PREDEFINED_VALUE = 243,
    ACTION_ID_DLOG_VIEW_SELECT_BOOKMARKS_TAB = 244,
    ACTION_ID_DLOG_VIEW_SELECT_OPTIONS_TAB = 245,
    ACTION_ID_DLOG_VIEW_SELECT_BOOKMARK = 246,
    ACTION_ID_DLOG_VIEW_ZOOM_IN = 247,
    ACTION_ID_DLOG_VIEW_ZOOM_OUT = 248,
    ACTION_ID_DLOG_VIEW_SELECT_Y_VALUES_TAB = 249,
    ACTION_ID_DLOG_VIEW_SELECT_Y_VALUE = 250,
    ACTION_ID_DLOG_VIEW_TOGGLE_Y_VALUE_VISIBLE = 251,
    ACTION_ID_SCROLL = 252,
    ACTION_ID_SHOW_PINOUT = 253,
    ACTION_ID_SHOW_SYS_SETTINGS_FUNCTION_GENERATOR = 254,
    ACTION_ID_FUNCTION_GENERATOR_SELECT_WAVEFORM = 255,
    ACTION_ID_FUNCTION_GENERATOR_ITEM_TOGGLE_SELECTED = 256,
    ACTION_ID_FUNCTION_GENERATOR_ITEM_TOGGLE_CHECKED = 257,
    ACTION_ID_FUNCTION_GENERATOR_SELECT_CHANNELS = 258,
    ACTION_ID_FUNCTION_GENERATOR_MODE_SELECT_MODE = 259,
    ACTION_ID_SHOW_SYS_SETTINGS_FUNCTION_GENERATOR_SELECT_CHANNEL = 260,
    ACTION_ID_TRIGGER_MANUALLY = 261,
    ACTION_ID_FUNCTION_GENERATOR_TRIGGER = 262,
    ACTION_ID_FUNCTION_GENERATOR_TOGGLE_FREQ = 263,
    ACTION_ID_FUNCTION_GENERATOR_TOGGLE_AMPL = 264,
    ACTION_ID_FUNCTION_GENERATOR_DESELECT_ALL = 265,
    ACTION_ID_TOGGLE_OVERLAY_MINIMIZED = 266,
    ACTION_ID_HIDE_OVERLAY = 267,
    ACTION_ID_SHOW_OVERLAY = 268,
    ACTION_ID_HIDE_OVERLAY_LONG_TOUCH = 269,
    ACTION_ID_SHOW_NEXT_CHANNEL_IN_MAX_VIEW = 270,
    ACTION_ID_SHOW_PREV_CHANNEL_IN_MAX_VIEW = 271,
    ACTION_ID_FUNCTION_GENERATOR_SHOW_PREVIOUS_PAGE = 272,
    ACTION_ID_TRIGGER_TOGGLE_INITIATE_ALL = 273,
    ACTION_ID_DIB_MIO168_DIN_SHOW_CONFIGURATION = 274,
    ACTION_ID_DIB_MIO168_DIN_SELECT_RANGE = 275,
    ACTION_ID_DIB_MIO168_DIN_SELECT_SPEED = 276,
    ACTION_ID_DIB_MIO168_DOUT_TOGGLE_STATE = 277,
    ACTION_ID_DIB_MIO168_DOUT_SELECT_TRIGGER_MODE = 278,
    ACTION_ID_DIB_MIO168_DOUT_SHOW_CONFIGURATION = 279,
    ACTION_ID_DIB_MIO168_DOUT_SHOW_FUNCTION = 280,
    ACTION_ID_DIB_MIO168_AIN_SHOW_CONFIGURATION = 281,
    ACTION_ID_DIB_MIO168_AIN_SELECT_MODE = 282,
    ACTION_ID_DIB_MIO168_AIN_SELECT_RANGE = 283,
    ACTION_ID_DIB_MIO168_AIN_CHANGE_LABEL = 284,
    ACTION_ID_DIB_MIO168_AIN_SELECT_MODE_AND_RANGE = 285,
    ACTION_ID_DIB_MIO168_AIN_TOGGLE_EFFICIENCY_FORMULA = 286,
    ACTION_ID_DIB_MIO168_AOUT_TOGGLE_OUTPUT_ENABLED = 287,
    ACTION_ID_DIB_MIO168_AOUT_SELECT_OUTPUT_MODE = 288,
    ACTION_ID_DIB_MIO168_AOUT_SELECT_VOLTAGE_RANGE = 289,
    ACTION_ID_DIB_MIO168_AOUT_SELECT_CURRENT_RANGE = 290,
    ACTION_ID_DIB_MIO168_AOUT_SELECT_MODE_AND_RANGE = 291,
    ACTION_ID_DIB_MIO168_AOUT_SHOW_CONFIGURATION = 292,
    ACTION_ID_DIB_MIO168_AOUT_CHANGE_LABEL = 293,
    ACTION_ID_DIB_MIO168_AOUT_SELECT_TRIGGER_MODE = 294,
    ACTION_ID_DIB_MIO168_AOUT_SHOW_FUNCTION = 295,
    ACTION_ID_DIB_MIO168_SHOW_INFO = 296,
    ACTION_ID_DIB_MIO168_SHOW_CALIBRATION = 297,
    ACTION_ID_DIB_MIO168_SHOW_CHANNEL_LABELS = 298,
    ACTION_ID_DIB_MIO168_SHOW_DIN_CHANNEL_LABELS = 299,
    ACTION_ID_DIB_MIO168_SHOW_DOUT_CHANNEL_LABELS = 300,
    ACTION_ID_DIB_MIO168_CHANGE_DIN_LABEL = 301,
    ACTION_ID_DIB_MIO168_CHANGE_DOUT_LABEL = 302,
    ACTION_ID_DIB_MIO168_PWM_CHANGE_LABEL = 303,
    ACTION_ID_DIB_MIO168_PAGER_SELECT_PAGE = 304,
    ACTION_ID_DIB_MIO168_SHOW_SLOT_SETTINGS_IN_MAX_VIEW = 305,
    ACTION_ID_DIB_MIO168_SELECT_PAGE_IN_DEFAULT_VIEW = 306,
    ACTION_ID_DIB_MIO168_AIN_TOGGLE_AC_ANALYSIS = 307,
    ACTION_ID_DIB_PREL6_TOGGLE_RELAY = 308,
    ACTION_ID_DIB_PREL6_SHOW_RELAY_LABELS = 309,
    ACTION_ID_DIB_PREL6_CHANGE_RELAY_LABEL = 310,
    ACTION_ID_DIB_PREL6_SHOW_INFO = 311,
    ACTION_ID_DIB_SMX46_TOGGLE_ROUTE = 312,
    ACTION_ID_DIB_SMX46_TOGGLE_RELAY = 313,
    ACTION_ID_DIB_SMX46_SHOW_CONFIGURE_ROUTES = 314,
    ACTION_ID_DIB_SMX46_CLEAR_ALL_ROUTES = 315,
    ACTION_ID_DIB_SMX46_CLEAR_ALL_LABELS = 316,
    ACTION_ID_DIB_SMX46_SHOW_INFO = 317,
    ACTION_ID_DIB_SMX46_SHOW_RELAY_CYCLES = 318,
    ACTION_ID_DIB_SMX46_EDIT_Y_LABEL = 319,
    ACTION_ID_DIB_SMX46_EDIT_X_LABEL = 320,
    ACTION_ID_DIB_SMX46_CHANGE_SUBCHANNEL_LABEL = 321,
    ACTION_ID_DIB_SMX46_AOUT_SHOW_CONFIGURATION = 322,
    ACTION_ID_DIB_SMX46_AOUT_SELECT_TRIGGER_MODE = 323,
    ACTION_ID_DIB_SMX46_AOUT_SHOW_FUNCTION = 324,
    ACTION_ID_DIB_SMX46_AOUT_SHOW_CALIBRATION = 325,
    ACTION_ID_DIB_DCM224_CH_SETTINGS_ADV_TOGGLE_COUNTERPHASE_DITHERING = 326,
    ACTION_ID_DIB_DCM224_CH_SETTINGS_ADV_TOGGLE_PWM_ENABLED = 327,
    ACTION_ID_DIB_MUX14D_TOGGLE_P1_RELAY = 328,
    ACTION_ID_DIB_MUX14D_CHANGE_P1_RELAY_LABEL = 329,
    ACTION_ID_DIB_MUX14D_TOGGLE_P2_RELAY = 330,
    ACTION_ID_DIB_MUX14D_CHANGE_P2_RELAY_LABEL = 331,
    ACTION_ID_DIB_MUX14D_TOGGLE_EXT_RELAY = 332,
    ACTION_ID_DIB_MUX14D_TOGGLE_ADIB1_RELAY = 333,
    ACTION_ID_DIB_MUX14D_TOGGLE_ADIB2_RELAY = 334,
    ACTION_ID_DIB_MUX14D_SHOW_RELAY_LABELS = 335,
    ACTION_ID_DIB_MUX14D_SHOW_INFO = 336,
    ACTION_ID_DIB_MUX14D_SHOW_RELAY_CYCLES = 337,
    ACTION_ID_DIB_MUX14D_TOGGLE_MULTIPLE_CONNECTIONS = 338
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
void action_show_sys_settings_screen_calibration();
void action_show_sys_settings_display();
void action_show_sys_settings_serial();
void action_show_sys_settings_ethernet();
void action_show_sys_settings_protections();
void action_show_sys_settings_aux_otp();
void action_show_sys_settings_sound();
void action_show_sys_settings_encoder();
void action_show_sys_info();
void action_show_edit_mode_step_help();
void action_show_edit_mode_slider_help();
void action_show_ch_settings();
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
void action_edit_calibration_password();
void action_ch_settings_calibration_start_calibration();
void action_ch_settings_calibration_toggle_enable();
void action_ch_settings_prot_clear();
void action_ch_settings_prot_toggle_state();
void action_ch_settings_prot_toggle_type();
void action_ch_settings_prot_edit_limit();
void action_ch_settings_prot_edit_level();
void action_ch_settings_prot_edit_delay();
void action_set();
void action_discard();
void action_edit_field();
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
void action_ch_settings_adv_view_edit();
void action_sys_settings_encoder_toggle_confirmation_mode();
void action_turn_display_off();
void action_ch_settings_trigger_edit_trigger_mode();
void action_ch_settings_lists_edit_list_count();
void action_ch_settings_lists_edit_on_list_stop();
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
void action_io_pin_select_uart_mode();
void action_io_pin_select_uart_baud();
void action_io_pin_select_uart_parity();
void action_io_pin_select_uart_data_bits();
void action_io_pin_select_uart_stop_bits();
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
void action_file_manager_go_to_parent();
void action_file_manager_open_file();
void action_file_manager_upload_file();
void action_file_manager_rename_file();
void action_file_manager_delete_file();
void action_show_dlog_params();
void action_dlog_edit_period();
void action_dlog_edit_duration();
void action_dlog_edit_file_name();
void action_dlog_view_toggle_drawer();
void action_file_manager_sort_by();
void action_dlog_auto_scale();
void action_dlog_upload();
void action_ethernet_edit_host_name();
void action_mqtt_edit_host();
void action_mqtt_edit_port();
void action_mqtt_edit_username();
void action_mqtt_edit_password();
void action_mqtt_edit_period();
void action_show_sys_settings_mqtt();
void action_dlog_scale_to_fit();
void action_dlog_view_toggle_labels();
void action_dlog_view_select_visible_value();
void action_channel_update_firmware();
void action_shutdown();
void action_show_channel_lists_file_menu();
void action_channel_lists_file_import();
void action_channel_lists_file_export();
void action_file_manager_new_file();
void action_profile_import();
void action_profile_export();
void action_event_queue_filter();
void action_event_queue_select_event();
void action_date_time_select_format();
void action_date_time_toggle_am_pm();
void action_edit_no_focus();
void action_untrack_all();
void action_ch_settings_copy();
void action_show_sys_settings_ethernet_error();
void action_file_manager_select_list_view();
void action_show_sys_settings_ramp_and_delay();
void action_channel_toggle_ramp_state();
void action_dlog_view_select_legend_view_option();
void action_select_channel_calibration_value_type();
void action_channel_calibration_point_next();
void action_channel_calibration_point_previous();
void action_channel_calibration_point_delete();
void action_channel_calibration_point_save();
void action_channel_calibration_chart_zoom();
void action_ch_settings_calibration_view_points();
void action_show_touch_calibration_intro();
void action_toggle_slot_max_view();
void action_show_slot_settings();
void action_select_usb_mode();
void action_select_usb_device_class();
void action_toggle_display_test_color_index();
void action_show_display_test_page();
void action_edit_ntp_refresh_frequency();
void action_keypad_option3();
void action_show_labels_and_colors();
void action_change_slot_label();
void action_change_channel_label();
void action_show_channel_color_picker();
void action_pick_color();
void action_keypad_set_default();
void action_show_slot_color_picker();
void action_pick_default_color();
void action_dlog_item_toggle();
void action_dlog_trigger_select_source();
void action_select_mass_storage_device();
void action_module_resync();
void action_select_ac_mains();
void action_show_select_mcu_revision_page();
void action_select_r2b4_revision();
void action_select_r3b3_revision();
void action_select_mcu_revision();
void action_ch_settings_ovp_prot_clear();
void action_ch_settings_ocp_prot_clear();
void action_ch_settings_opp_prot_clear();
void action_ch_settings_otp_prot_clear();
void action_dlog_period_select_predefined_value();
void action_dlog_view_select_bookmarks_tab();
void action_dlog_view_select_options_tab();
void action_dlog_view_select_bookmark();
void action_dlog_view_zoom_in();
void action_dlog_view_zoom_out();
void action_dlog_view_select_y_values_tab();
void action_dlog_view_select_y_value();
void action_dlog_view_toggle_y_value_visible();
void action_scroll();
void action_show_pinout();
void action_show_sys_settings_function_generator();
void action_function_generator_select_waveform();
void action_function_generator_item_toggle_selected();
void action_function_generator_item_toggle_checked();
void action_function_generator_select_channels();
void action_function_generator_mode_select_mode();
void action_show_sys_settings_function_generator_select_channel();
void action_trigger_manually();
void action_function_generator_trigger();
void action_function_generator_toggle_freq();
void action_function_generator_toggle_ampl();
void action_function_generator_deselect_all();
void action_toggle_overlay_minimized();
void action_hide_overlay();
void action_show_overlay();
void action_hide_overlay_long_touch();
void action_show_next_channel_in_max_view();
void action_show_prev_channel_in_max_view();
void action_function_generator_show_previous_page();
void action_trigger_toggle_initiate_all();
void action_dib_mio168_din_show_configuration();
void action_dib_mio168_din_select_range();
void action_dib_mio168_din_select_speed();
void action_dib_mio168_dout_toggle_state();
void action_dib_mio168_dout_select_trigger_mode();
void action_dib_mio168_dout_show_configuration();
void action_dib_mio168_dout_show_function();
void action_dib_mio168_ain_show_configuration();
void action_dib_mio168_ain_select_mode();
void action_dib_mio168_ain_select_range();
void action_dib_mio168_ain_change_label();
void action_dib_mio168_ain_select_mode_and_range();
void action_dib_mio168_ain_toggle_efficiency_formula();
void action_dib_mio168_aout_toggle_output_enabled();
void action_dib_mio168_aout_select_output_mode();
void action_dib_mio168_aout_select_voltage_range();
void action_dib_mio168_aout_select_current_range();
void action_dib_mio168_aout_select_mode_and_range();
void action_dib_mio168_aout_show_configuration();
void action_dib_mio168_aout_change_label();
void action_dib_mio168_aout_select_trigger_mode();
void action_dib_mio168_aout_show_function();
void action_dib_mio168_show_info();
void action_dib_mio168_show_calibration();
void action_dib_mio168_show_channel_labels();
void action_dib_mio168_show_din_channel_labels();
void action_dib_mio168_show_dout_channel_labels();
void action_dib_mio168_change_din_label();
void action_dib_mio168_change_dout_label();
void action_dib_mio168_pwm_change_label();
void action_dib_mio168_pager_select_page();
void action_dib_mio168_show_slot_settings_in_max_view();
void action_dib_mio168_select_page_in_default_view();
void action_dib_mio168_ain_toggle_ac_analysis();
void action_dib_prel6_toggle_relay();
void action_dib_prel6_show_relay_labels();
void action_dib_prel6_change_relay_label();
void action_dib_prel6_show_info();
void action_dib_smx46_toggle_route();
void action_dib_smx46_toggle_relay();
void action_dib_smx46_show_configure_routes();
void action_dib_smx46_clear_all_routes();
void action_dib_smx46_clear_all_labels();
void action_dib_smx46_show_info();
void action_dib_smx46_show_relay_cycles();
void action_dib_smx46_edit_y_label();
void action_dib_smx46_edit_x_label();
void action_dib_smx46_change_subchannel_label();
void action_dib_smx46_aout_show_configuration();
void action_dib_smx46_aout_select_trigger_mode();
void action_dib_smx46_aout_show_function();
void action_dib_smx46_aout_show_calibration();
void action_dib_dcm224_ch_settings_adv_toggle_counterphase_dithering();
void action_dib_dcm224_ch_settings_adv_toggle_pwm_enabled();
void action_dib_mux14d_toggle_p1_relay();
void action_dib_mux14d_change_p1_relay_label();
void action_dib_mux14d_toggle_p2_relay();
void action_dib_mux14d_change_p2_relay_label();
void action_dib_mux14d_toggle_ext_relay();
void action_dib_mux14d_toggle_adib1_relay();
void action_dib_mux14d_toggle_adib2_relay();
void action_dib_mux14d_show_relay_labels();
void action_dib_mux14d_show_info();
void action_dib_mux14d_show_relay_cycles();
void action_dib_mux14d_toggle_multiple_connections();

extern ActionExecFunc g_actionExecFunctions[];

enum FontsEnum {
    FONT_ID_NONE = 0,
    FONT_ID_OSWALD12 = 1,
    FONT_ID_SHADOW = 2,
    FONT_ID_OSWALD24 = 3,
    FONT_ID_OSWALD17 = 4,
    FONT_ID_OSWALD14 = 5,
    FONT_ID_HEYDINGS14 = 6,
    FONT_ID_ROBOTO_CONDENSED_REGULAR = 7,
    FONT_ID_OSWALD20 = 8,
    FONT_ID_OSWALD48 = 9,
    FONT_ID_WEB_HOSTING_HUB24 = 10,
    FONT_ID_OSWALD38 = 11,
    FONT_ID_DIB_MUX14D_IMAGES = 12
};

enum BitmapsEnum {
    BITMAP_ID_NONE = 0,
    BITMAP_ID_MOUSE_CURSOR = 1,
    BITMAP_ID_LOGO = 2,
    BITMAP_ID_PROTECTION_CURVE = 3,
    BITMAP_ID_VOUT = 4,
    BITMAP_ID_PROTECTION_HW_TRIP_LEVEL = 5,
    BITMAP_ID_VSET = 6,
    BITMAP_ID_PROTECTION_TRIP_LEVEL = 7,
    BITMAP_ID_PROTECTION_DELAY = 8,
    BITMAP_ID_IOUT = 9,
    BITMAP_ID_POUT = 10,
    BITMAP_ID_COUPLED_PARALLEL = 11,
    BITMAP_ID_COUPLED_SERIES = 12,
    BITMAP_ID_COUPLED_CGND = 13,
    BITMAP_ID_COUPLED_SPLIT_RAILS = 14,
    BITMAP_ID_DIB_DCP405_COUPLED_SERIES_DEF_VIEW = 15,
    BITMAP_ID_DIB_DCP405_COUPLED_PARALLEL_DEF_VIEW = 16
};

enum StylesEnum {
    STYLE_ID_NONE = 0,
    STYLE_ID_DEFAULT = 1,
    STYLE_ID_DEFAULT_M_LEFT = 2,
    STYLE_ID_EDIT_VALUE_ACTIVE_M_CENTER = 3,
    STYLE_ID_EDIT_VALUE_ACTIVE_M_LEFT = 4,
    STYLE_ID_EDIT_VALUE_ACTIVE_S_LEFT = 5,
    STYLE_ID_EDIT_VALUE_ACTIVE_S_CENTER = 6,
    STYLE_ID_DEFAULT_S = 7,
    STYLE_ID_DEFAULT_S_LEFT = 8,
    STYLE_ID_STATUS_ICON_ENABLED = 9,
    STYLE_ID_STATUS_TITLE = 10,
    STYLE_ID_STATUS_BGND = 11,
    STYLE_ID_BUTTON_M = 12,
    STYLE_ID_BUTTON_M_DISABLED = 13,
    STYLE_ID_STATUS_ACTION = 14,
    STYLE_ID_DEFAULT_S_LEFT_WARNING = 15,
    STYLE_ID_DEFAULT_S_LEFT_OK = 16,
    STYLE_ID_DEFAULT_S_WARNING = 17,
    STYLE_ID_EDIT_VALUE_ACTIVE_S_CENTER_ICON = 18,
    STYLE_ID_DEFAULT_S_RIGHT_CONDENSED = 19,
    STYLE_ID_DEFAULT_S_LEFT_CONDENSED = 20,
    STYLE_ID_BAR_GRAPH_P_DEFAULT = 21,
    STYLE_ID_ERROR_ALERT = 22,
    STYLE_ID_ERROR_ALERT_BUTTON = 23,
    STYLE_ID_YT_GRAPH_U_DEFAULT = 24,
    STYLE_ID_YT_GRAPH_I_DEFAULT = 25,
    STYLE_ID_YT_GRAPH_P_DEFAULT = 26,
    STYLE_ID_YT_GRAPH_UNREGULATED = 27,
    STYLE_ID_YT_GRAPH_Y1 = 28,
    STYLE_ID_YT_GRAPH_Y2 = 29,
    STYLE_ID_YT_GRAPH_Y3 = 30,
    STYLE_ID_YT_GRAPH_Y4 = 31,
    STYLE_ID_YT_GRAPH_Y5 = 32,
    STYLE_ID_SELECT_ENUM_ITEM_POPUP_CONTAINER = 33,
    STYLE_ID_SELECT_ENUM_ITEM_POPUP_CONTAINER_S = 34,
    STYLE_ID_SELECT_ENUM_ITEM_POPUP_ITEM = 35,
    STYLE_ID_SELECT_ENUM_ITEM_POPUP_ITEM_S = 36,
    STYLE_ID_SELECT_ENUM_ITEM_POPUP_DISABLED_ITEM = 37,
    STYLE_ID_SELECT_ENUM_ITEM_POPUP_DISABLED_ITEM_S = 38,
    STYLE_ID_ENCODER_CURSOR_14_DISABLED = 39,
    STYLE_ID_ENCODER_CURSOR_14_RIGHT_DISABLED = 40,
    STYLE_ID_INFO_ALERT = 41,
    STYLE_ID_MENU_WITH_BUTTONS_MESSAGE = 42,
    STYLE_ID_MENU_WITH_BUTTONS_CONTAINER = 43,
    STYLE_ID_MENU_WITH_BUTTONS_BUTTON = 44,
    STYLE_ID_CHANNEL1 = 45,
    STYLE_ID_DEFAULT_M = 46,
    STYLE_ID_INLINE47 = 47,
    STYLE_ID_INLINE48 = 48,
    STYLE_ID_INLINE49 = 49,
    STYLE_ID_PROGRESS = 50,
    STYLE_ID_INLINE51 = 51,
    STYLE_ID_INLINE52 = 52,
    STYLE_ID_INLINE53 = 53,
    STYLE_ID_VALUE_S = 54,
    STYLE_ID_TOUCH_CALIBRATION = 55,
    STYLE_ID_NOTE_M = 56,
    STYLE_ID_TOUCH_CALIBRATION_POINT = 57,
    STYLE_ID_BUTTON_BGND = 58,
    STYLE_ID_STATUS_ICON_ENABLED_WITH_NEED_ATTENTION = 59,
    STYLE_ID_STATUS_ICON_ENABLED_AND_ERROR = 60,
    STYLE_ID_STATUS_ICON_ENABLED_AND_ACTIVE = 61,
    STYLE_ID_STATUS_ICON_ENABLED_BLINK = 62,
    STYLE_ID_STATUS_ICON_DISABLED = 63,
    STYLE_ID_DLOG_RECORD_BUTTON = 64,
    STYLE_ID_STATUS_ICON_DISABLED_BLINK = 65,
    STYLE_ID_OVERLAY = 66,
    STYLE_ID_OVERLAY_ICON = 67,
    STYLE_ID_OVERLAY_LEFT = 68,
    STYLE_ID_INLINE69 = 69,
    STYLE_ID_INLINE70 = 70,
    STYLE_ID_INLINE71 = 71,
    STYLE_ID_INLINE72 = 72,
    STYLE_ID_VALUE_S_LEFT = 73,
    STYLE_ID_EDIT_VALUE_M_LEFT = 74,
    STYLE_ID_KEY_ICONS = 75,
    STYLE_ID_KEY = 76,
    STYLE_ID_KEY_DISABLED = 77,
    STYLE_ID_INLINE78 = 78,
    STYLE_ID_TAB_PAGE_SELECTED = 79,
    STYLE_ID_TAB_PAGE = 80,
    STYLE_ID_BOTTOM_BUTTON_BACKGROUND = 81,
    STYLE_ID_BOTTOM_BUTTON = 82,
    STYLE_ID_INLINE83 = 83,
    STYLE_ID_INLINE84 = 84,
    STYLE_ID_INLINE85 = 85,
    STYLE_ID_INLINE86 = 86,
    STYLE_ID_INLINE87 = 87,
    STYLE_ID_INLINE88 = 88,
    STYLE_ID_BUTTON_S = 89,
    STYLE_ID_EDIT_VALUE_S_LEFT = 90,
    STYLE_ID_SLIDER_SHADE_1 = 91,
    STYLE_ID_SLIDER_SHADE_2 = 92,
    STYLE_ID_SLIDER_SHADE_3 = 93,
    STYLE_ID_SLIDER_SHADE_4 = 94,
    STYLE_ID_SLIDER_SHADE_5 = 95,
    STYLE_ID_YES_NO = 96,
    STYLE_ID_YES_NO_BUTTON = 97,
    STYLE_ID_YES_NO_MESSAGE = 98,
    STYLE_ID_INLINE99 = 99,
    STYLE_ID_TEXT_MESSAGE = 100,
    STYLE_ID_INLINE101 = 101,
    STYLE_ID_INLINE102 = 102,
    STYLE_ID_ASYNC_OPERATION = 103,
    STYLE_ID_INLINE104 = 104,
    STYLE_ID_INLINE105 = 105,
    STYLE_ID_INLINE106 = 106,
    STYLE_ID_INLINE107 = 107,
    STYLE_ID_INLINE108 = 108,
    STYLE_ID_INLINE109 = 109,
    STYLE_ID_INLINE110 = 110,
    STYLE_ID_INLINE111 = 111,
    STYLE_ID_DEFAULT_INVERSE = 112,
    STYLE_ID_SCROLLBAR_THUMB = 113,
    STYLE_ID_SCROLLBAR_BUTTON = 114,
    STYLE_ID_BOTTOM_BUTTON_TEXTUAL_S_LEFT = 115,
    STYLE_ID_INLINE116 = 116,
    STYLE_ID_KEY_SPEC_ICONS = 117,
    STYLE_ID_INLINE118 = 118,
    STYLE_ID_KEY_SPEC_ICONS_DISABLED = 119,
    STYLE_ID_INLINE120 = 120,
    STYLE_ID_INLINE121 = 121,
    STYLE_ID_INLINE122 = 122,
    STYLE_ID_INLINE123 = 123,
    STYLE_ID_EDIT_VALUE_S_CENTERED = 124,
    STYLE_ID_INLINE125 = 125,
    STYLE_ID_INLINE126 = 126,
    STYLE_ID_INLINE127 = 127,
    STYLE_ID_INLINE128 = 128,
    STYLE_ID_INLINE129 = 129,
    STYLE_ID_INLINE130 = 130,
    STYLE_ID_INLINE131 = 131,
    STYLE_ID_INLINE132 = 132,
    STYLE_ID_INLINE133 = 133,
    STYLE_ID_INLINE134 = 134,
    STYLE_ID_VALUE_M_LEFT = 135,
    STYLE_ID_INLINE136 = 136,
    STYLE_ID_MAX_CURRENT_LIMIT_CAUSE = 137,
    STYLE_ID_INLINE138 = 138,
    STYLE_ID_DEFAULT_DISABLED_M_LEFT = 139,
    STYLE_ID_ENCODER_CURSOR_17_ENABLED = 140,
    STYLE_ID_INLINE141 = 141,
    STYLE_ID_YT_GRAPH = 142,
    STYLE_ID_LIST_GRAPH_CURSOR = 143,
    STYLE_ID_YT_GRAPH_U_DEFAULT_LABEL = 144,
    STYLE_ID_YT_GRAPH_I_DEFAULT_LABEL = 145,
    STYLE_ID_ENCODER_CURSOR_14_ENABLED = 146,
    STYLE_ID_INLINE147 = 147,
    STYLE_ID_INLINE148 = 148,
    STYLE_ID_INLINE149 = 149,
    STYLE_ID_EDIT_VALUE_M_CENTERED = 150,
    STYLE_ID_INLINE151 = 151,
    STYLE_ID_INLINE152 = 152,
    STYLE_ID_INLINE153 = 153,
    STYLE_ID_BUTTON_S_DISABLED = 154,
    STYLE_ID_INLINE155 = 155,
    STYLE_ID_INLINE156 = 156,
    STYLE_ID_OPTION_TOGGLE_L_CENTER = 157,
    STYLE_ID_INLINE158 = 158,
    STYLE_ID_EVENT_ERROR = 159,
    STYLE_ID_DEFAULT_DISABLED_S_LEFT = 160,
    STYLE_ID_INLINE161 = 161,
    STYLE_ID_ICON_AND_TEXT_S = 162,
    STYLE_ID_INLINE163 = 163,
    STYLE_ID_INLINE164 = 164,
    STYLE_ID_INLINE165 = 165,
    STYLE_ID_VALUE_M = 166,
    STYLE_ID_INLINE167 = 167,
    STYLE_ID_INLINE168 = 168,
    STYLE_ID_INLINE169 = 169,
    STYLE_ID_DEFAULT_DISABLED = 170,
    STYLE_ID_INLINE171 = 171,
    STYLE_ID_INLINE172 = 172,
    STYLE_ID_EDIT_S = 173,
    STYLE_ID_INLINE174 = 174,
    STYLE_ID_INLINE175 = 175,
    STYLE_ID_INLINE176 = 176,
    STYLE_ID_INLINE177 = 177,
    STYLE_ID_INLINE178 = 178,
    STYLE_ID_INLINE179 = 179,
    STYLE_ID_INLINE180 = 180,
    STYLE_ID_INLINE181 = 181,
    STYLE_ID_INLINE182 = 182,
    STYLE_ID_INLINE183 = 183,
    STYLE_ID_INLINE184 = 184,
    STYLE_ID_INLINE185 = 185,
    STYLE_ID_INLINE186 = 186,
    STYLE_ID_INLINE187 = 187,
    STYLE_ID_INLINE188 = 188,
    STYLE_ID_INLINE189 = 189,
    STYLE_ID_INLINE190 = 190,
    STYLE_ID_INLINE191 = 191,
    STYLE_ID_INLINE192 = 192,
    STYLE_ID_INLINE193 = 193,
    STYLE_ID_INLINE194 = 194,
    STYLE_ID_INLINE195 = 195,
    STYLE_ID_INLINE196 = 196,
    STYLE_ID_INLINE197 = 197,
    STYLE_ID_INLINE198 = 198,
    STYLE_ID_INLINE199 = 199,
    STYLE_ID_INLINE200 = 200,
    STYLE_ID_INLINE201 = 201,
    STYLE_ID_INLINE202 = 202,
    STYLE_ID_INLINE203 = 203,
    STYLE_ID_INLINE204 = 204,
    STYLE_ID_INLINE205 = 205,
    STYLE_ID_INLINE206 = 206,
    STYLE_ID_INLINE207 = 207,
    STYLE_ID_EVENT_WARNING = 208,
    STYLE_ID_INLINE209 = 209,
    STYLE_ID_INLINE210 = 210,
    STYLE_ID_INLINE211 = 211,
    STYLE_ID_INLINE212 = 212,
    STYLE_ID_DISPLAY_OFF = 213,
    STYLE_ID_DISPLAY_OFF_S = 214,
    STYLE_ID_INLINE215 = 215,
    STYLE_ID_INLINE216 = 216,
    STYLE_ID_INLINE217 = 217,
    STYLE_ID_INLINE218 = 218,
    STYLE_ID_INLINE219 = 219,
    STYLE_ID_INLINE220 = 220,
    STYLE_ID_INLINE221 = 221,
    STYLE_ID_INLINE222 = 222,
    STYLE_ID_INLINE223 = 223,
    STYLE_ID_INLINE224 = 224,
    STYLE_ID_INLINE225 = 225,
    STYLE_ID_STATUS_LINE_SMALL_TITLE = 226,
    STYLE_ID_EDIT_VALUE_S_CENTERED_STATUS_LINE = 227,
    STYLE_ID_SCROLLBAR_TRACK = 228,
    STYLE_ID_INLINE229 = 229,
    STYLE_ID_INLINE230 = 230,
    STYLE_ID_INLINE231 = 231,
    STYLE_ID_INLINE232 = 232,
    STYLE_ID_INLINE233 = 233,
    STYLE_ID_INLINE234 = 234,
    STYLE_ID_INLINE235 = 235,
    STYLE_ID_INLINE236 = 236,
    STYLE_ID_INLINE237 = 237,
    STYLE_ID_INLINE238 = 238,
    STYLE_ID_INLINE239 = 239,
    STYLE_ID_INLINE240 = 240,
    STYLE_ID_INLINE241 = 241,
    STYLE_ID_INLINE242 = 242,
    STYLE_ID_INLINE243 = 243,
    STYLE_ID_INLINE244 = 244,
    STYLE_ID_INLINE245 = 245,
    STYLE_ID_INLINE246 = 246,
    STYLE_ID_INLINE247 = 247,
    STYLE_ID_INLINE248 = 248,
    STYLE_ID_INLINE249 = 249,
    STYLE_ID_INLINE250 = 250,
    STYLE_ID_INLINE251 = 251,
    STYLE_ID_INLINE252 = 252,
    STYLE_ID_INLINE253 = 253,
    STYLE_ID_INLINE254 = 254,
    STYLE_ID_INLINE255 = 255,
    STYLE_ID_INLINE256 = 256,
    STYLE_ID_INLINE257 = 257,
    STYLE_ID_INLINE258 = 258,
    STYLE_ID_INLINE259 = 259,
    STYLE_ID_INLINE260 = 260,
    STYLE_ID_INLINE261 = 261,
    STYLE_ID_INLINE262 = 262,
    STYLE_ID_INLINE263 = 263,
    STYLE_ID_INLINE264 = 264,
    STYLE_ID_DEFAULT_M_WARNING = 265,
    STYLE_ID_INLINE266 = 266,
    STYLE_ID_INLINE267 = 267,
    STYLE_ID_INLINE268 = 268,
    STYLE_ID_INLINE269 = 269,
    STYLE_ID_INLINE270 = 270,
    STYLE_ID_INLINE271 = 271,
    STYLE_ID_INLINE272 = 272,
    STYLE_ID_INLINE273 = 273,
    STYLE_ID_INLINE274 = 274,
    STYLE_ID_INLINE275 = 275,
    STYLE_ID_INLINE276 = 276,
    STYLE_ID_INLINE277 = 277,
    STYLE_ID_INLINE278 = 278,
    STYLE_ID_INLINE279 = 279,
    STYLE_ID_INLINE280 = 280,
    STYLE_ID_INLINE281 = 281,
    STYLE_ID_INLINE282 = 282,
    STYLE_ID_INLINE283 = 283,
    STYLE_ID_INLINE284 = 284,
    STYLE_ID_INLINE285 = 285,
    STYLE_ID_INLINE286 = 286,
    STYLE_ID_INLINE287 = 287,
    STYLE_ID_INLINE288 = 288,
    STYLE_ID_INLINE289 = 289,
    STYLE_ID_INLINE290 = 290,
    STYLE_ID_LABELS_AND_COLORS_SLOT = 291,
    STYLE_ID_INLINE292 = 292,
    STYLE_ID_INLINE293 = 293,
    STYLE_ID_INLINE294 = 294,
    STYLE_ID_INLINE295 = 295,
    STYLE_ID_INLINE296 = 296,
    STYLE_ID_INLINE297 = 297,
    STYLE_ID_INLINE298 = 298,
    STYLE_ID_INLINE299 = 299,
    STYLE_ID_INLINE300 = 300,
    STYLE_ID_INLINE301 = 301,
    STYLE_ID_INLINE302 = 302,
    STYLE_ID_TRIGGER_BUTTON_14 = 303,
    STYLE_ID_INLINE304 = 304,
    STYLE_ID_INLINE305 = 305,
    STYLE_ID_INLINE306 = 306,
    STYLE_ID_INLINE307 = 307,
    STYLE_ID_INLINE308 = 308,
    STYLE_ID_INLINE309 = 309,
    STYLE_ID_INLINE310 = 310,
    STYLE_ID_INLINE311 = 311,
    STYLE_ID_INLINE312 = 312,
    STYLE_ID_INLINE313 = 313,
    STYLE_ID_INLINE314 = 314,
    STYLE_ID_INLINE315 = 315,
    STYLE_ID_INLINE316 = 316,
    STYLE_ID_INLINE317 = 317,
    STYLE_ID_INLINE318 = 318,
    STYLE_ID_INLINE319 = 319,
    STYLE_ID_INLINE320 = 320,
    STYLE_ID_INLINE321 = 321,
    STYLE_ID_INLINE322 = 322,
    STYLE_ID_INLINE323 = 323,
    STYLE_ID_INLINE324 = 324,
    STYLE_ID_INLINE325 = 325,
    STYLE_ID_INLINE326 = 326,
    STYLE_ID_INLINE327 = 327,
    STYLE_ID_INLINE328 = 328,
    STYLE_ID_INLINE329 = 329,
    STYLE_ID_INLINE330 = 330,
    STYLE_ID_INLINE331 = 331,
    STYLE_ID_INLINE332 = 332,
    STYLE_ID_INLINE333 = 333,
    STYLE_ID_INLINE334 = 334,
    STYLE_ID_INLINE335 = 335,
    STYLE_ID_INLINE336 = 336,
    STYLE_ID_INLINE337 = 337,
    STYLE_ID_INLINE338 = 338,
    STYLE_ID_INLINE339 = 339,
    STYLE_ID_INLINE340 = 340,
    STYLE_ID_INLINE341 = 341,
    STYLE_ID_INLINE342 = 342,
    STYLE_ID_INLINE343 = 343,
    STYLE_ID_INLINE344 = 344,
    STYLE_ID_INLINE345 = 345,
    STYLE_ID_INLINE346 = 346,
    STYLE_ID_INLINE347 = 347,
    STYLE_ID_INLINE348 = 348,
    STYLE_ID_INLINE349 = 349,
    STYLE_ID_INLINE350 = 350,
    STYLE_ID_INLINE351 = 351,
    STYLE_ID_INLINE352 = 352,
    STYLE_ID_INLINE353 = 353,
    STYLE_ID_INLINE354 = 354,
    STYLE_ID_INLINE355 = 355,
    STYLE_ID_INLINE356 = 356,
    STYLE_ID_INLINE357 = 357,
    STYLE_ID_INLINE358 = 358,
    STYLE_ID_INLINE359 = 359,
    STYLE_ID_INLINE360 = 360,
    STYLE_ID_INLINE361 = 361,
    STYLE_ID_INLINE362 = 362,
    STYLE_ID_INLINE363 = 363,
    STYLE_ID_INLINE364 = 364,
    STYLE_ID_INLINE365 = 365,
    STYLE_ID_INLINE366 = 366,
    STYLE_ID_INLINE367 = 367,
    STYLE_ID_INLINE368 = 368,
    STYLE_ID_INLINE369 = 369,
    STYLE_ID_INLINE370 = 370,
    STYLE_ID_INLINE371 = 371,
    STYLE_ID_INLINE372 = 372,
    STYLE_ID_INLINE373 = 373,
    STYLE_ID_MON_VALUE_48_RIGHT = 374,
    STYLE_ID_INLINE375 = 375,
    STYLE_ID_MON_VALUE_20_LEFT = 376,
    STYLE_ID_BAR_GRAPH_U_DEFAULT = 377,
    STYLE_ID_BAR_GRAPH_TEXT = 378,
    STYLE_ID_BAR_GRAPH_SET_LINE = 379,
    STYLE_ID_BAR_GRAPH_LIMIT_LINE = 380,
    STYLE_ID_BAR_GRAPH_I_DEFAULT = 381,
    STYLE_ID_LABELS_AND_COLORS_CHANNEL = 382,
    STYLE_ID_INLINE383 = 383,
    STYLE_ID_TRIGGER_BUTTON_24 = 384,
    STYLE_ID_INLINE385 = 385,
    STYLE_ID_INLINE386 = 386,
    STYLE_ID_CHANNEL_OFF_BUTTON_38 = 387,
    STYLE_ID_INLINE388 = 388,
    STYLE_ID_CH_PROT_OFF = 389,
    STYLE_ID_CH_PROT_ON = 390,
    STYLE_ID_CH_PROT_TRIPPED = 391,
    STYLE_ID_INLINE392 = 392,
    STYLE_ID_INLINE393 = 393,
    STYLE_ID_MON_VALUE_38_RIGHT = 394,
    STYLE_ID_MON_VALUE_14_RIGHT = 395,
    STYLE_ID_ENCODER_CURSOR_14_RIGHT_ENABLED = 396,
    STYLE_ID_CHANNEL_OFF_BUTTON_20 = 397,
    STYLE_ID_DIB_MUX14D_IMAGE = 398,
    STYLE_ID_INLINE399 = 399,
    STYLE_ID_INLINE400 = 400,
    STYLE_ID_INLINE401 = 401,
    STYLE_ID_INLINE402 = 402,
    STYLE_ID_INLINE403 = 403,
    STYLE_ID_INLINE404 = 404,
    STYLE_ID_INLINE405 = 405
};

enum ThemesEnum {
    THEME_ID_LEGACY = 0,
    THEME_ID_LIGHT = 1,
    THEME_ID_DARK = 2
};

enum ColorsEnum {
    COLOR_ID_BACKGROUND = 0,
    COLOR_ID_TEXT_REGULAR = 1,
    COLOR_ID_ACTION_COLOR = 2,
    COLOR_ID_TEXT_SELECTED = 3,
    COLOR_ID_TEXT_ENABLE_BACKGROUND = 4,
    COLOR_ID_STATUS_LINE_TEXT = 5,
    COLOR_ID_STATUS_LINE_TEXT_DISABLED = 6,
    COLOR_ID_TEXT_DISABLED_BACKGROUND = 7,
    COLOR_ID_STATUS_LINE_BACKGROUND = 8,
    COLOR_ID_STATUS_OK = 9,
    COLOR_ID_STATUS_WARNING = 10,
    COLOR_ID_TEXT_SELECTABLE_BACKGROUND = 11,
    COLOR_ID_PROTECTION_TEXT = 12,
    COLOR_ID_TEXT_DISABLED = 13,
    COLOR_ID_BAR_GRAPH_VOLTAGE = 14,
    COLOR_ID_BAR_GRAPH_CURRENT = 15,
    COLOR_ID_BAR_GRAPH_POWER = 16,
    COLOR_ID_BAR_GRAPH_BACKGROUND = 17,
    COLOR_ID_BAR_GRAPH_TEXT = 18,
    COLOR_ID_BAR_GRAPH_TEXT_INSIDE = 19,
    COLOR_ID_BAR_GRAPH_SET_LINE = 20,
    COLOR_ID_POPUP_BACKGROUND_REGULAR = 21,
    COLOR_ID_POPUP_TEXT_SELECTED = 22,
    COLOR_ID_POPUP_BACKGROUND_WARNING = 23,
    COLOR_ID_POPUP_BACKGROUND_INFO = 24,
    COLOR_ID_TEXT_EDITED = 25,
    COLOR_ID_TEXT_VALUE = 26,
    COLOR_ID_DATA_LOGGING = 27,
    COLOR_ID_OVERLAY_BACKGROUND = 28,
    COLOR_ID_OVERLAY_DRAG_BACKGROUND = 29,
    COLOR_ID_OVERLAY_TEXT = 30,
    COLOR_ID_CHANNEL1 = 31,
    COLOR_ID_CHANNEL2 = 32,
    COLOR_ID_CHANNEL3 = 33,
    COLOR_ID_CHANNEL4 = 34,
    COLOR_ID_CHANNEL5 = 35,
    COLOR_ID_CHANNEL6 = 36,
    COLOR_ID_CHANNEL7 = 37,
    COLOR_ID_CHANNEL8 = 38,
    COLOR_ID_CHANNEL9 = 39,
    COLOR_ID_CHANNEL10 = 40,
    COLOR_ID_CHANNEL11 = 41,
    COLOR_ID_CHANNEL12 = 42,
    COLOR_ID_CHANNEL13 = 43,
    COLOR_ID_CHANNEL14 = 44,
    COLOR_ID_CHANNEL15 = 45,
    COLOR_ID_CHANNEL16 = 46,
    COLOR_ID_CHANNEL17 = 47,
    COLOR_ID_CHANNEL18 = 48,
    COLOR_ID_CHANNEL19 = 49,
    COLOR_ID_CHANNEL20 = 50,
    COLOR_ID_CHANNEL21 = 51,
    COLOR_ID_CHANNEL22 = 52,
    COLOR_ID_CHANNEL23 = 53,
    COLOR_ID_CHANNEL24 = 54,
    COLOR_ID_CHANNEL1_TEXT = 55,
    COLOR_ID_CHANNEL2_TEXT = 56,
    COLOR_ID_CHANNEL3_TEXT = 57,
    COLOR_ID_CHANNEL4_TEXT = 58,
    COLOR_ID_CHANNEL5_TEXT = 59,
    COLOR_ID_CHANNEL6_TEXT = 60,
    COLOR_ID_CHANNEL7_TEXT = 61,
    COLOR_ID_CHANNEL8_TEXT = 62,
    COLOR_ID_CHANNEL9_TEXT = 63,
    COLOR_ID_CHANNEL10_TEXT = 64,
    COLOR_ID_CHANNEL11_TEXT = 65,
    COLOR_ID_CHANNEL12_TEXT = 66,
    COLOR_ID_CHANNEL13_TEXT = 67,
    COLOR_ID_CHANNEL14_TEXT = 68,
    COLOR_ID_CHANNEL15_TEXT = 69,
    COLOR_ID_CHANNEL16_TEXT = 70,
    COLOR_ID_CHANNEL17_TEXT = 71,
    COLOR_ID_CHANNEL18_TEXT = 72,
    COLOR_ID_CHANNEL19_TEXT = 73,
    COLOR_ID_CHANNEL20_TEXT = 74,
    COLOR_ID_CHANNEL21_TEXT = 75,
    COLOR_ID_CHANNEL22_TEXT = 76,
    COLOR_ID_CHANNEL23_TEXT = 77,
    COLOR_ID_CHANNEL24_TEXT = 78,
    COLOR_ID_SCROLLBAR_TRACK = 79,
    COLOR_ID_SCROLLBAR_THUMB = 80,
    COLOR_ID_SCROLLBAR_BUTTON = 81,
    COLOR_ID_YT_GRAPH_GRID = 82,
    COLOR_ID_YT_GRAPH_BACKGROUND = 83,
    COLOR_ID_YT_GRAPH_CURSOR = 84,
    COLOR_ID_YT_GRAPH_Y1 = 85,
    COLOR_ID_YT_GRAPH_Y2 = 86,
    COLOR_ID_YT_GRAPH_Y3 = 87,
    COLOR_ID_YT_GRAPH_Y4 = 88,
    COLOR_ID_YT_GRAPH_Y5 = 89,
    COLOR_ID_LOADING = 90,
    COLOR_ID_PROGRESS = 91,
    COLOR_ID_PROGRESS_BACKGROUND = 92,
    COLOR_ID_BACKDROP = 93,
    COLOR_ID_PICK_COLOR = 94,
    COLOR_ID_LABELS_AND_COLORS_PAGE_SLOT_COLOR = 95,
    COLOR_ID_LABELS_AND_COLORS_PAGE_SLOT_COLOR_TEXT = 96,
    COLOR_ID_LABELS_AND_COLORS_PAGE_CHANNEL_COLOR = 97,
    COLOR_ID_LABELS_AND_COLORS_PAGE_CHANNEL_COLOR_TEXT = 98,
    COLOR_ID_BOOKMARK = 99,
    COLOR_ID_SELECTED_BOOKMARK = 100
};

enum PagesEnum {
    PAGE_ID_NONE = 0,
    PAGE_ID_SELECT_MCU_REVISION = 1,
    PAGE_ID_WELCOME = 2,
    PAGE_ID_WELCOME_800X480 = 3,
    PAGE_ID_TOUCH_CALIBRATION_INTRO = 4,
    PAGE_ID_TOUCH_CALIBRATION = 5,
    PAGE_ID_TOUCH_CALIBRATION_YES_NO = 6,
    PAGE_ID_TOUCH_CALIBRATION_YES_NO_CANCEL = 7,
    PAGE_ID_MAIN = 8,
    PAGE_ID_EDIT_MODE_KEYPAD = 9,
    PAGE_ID_EDIT_MODE_STEP = 10,
    PAGE_ID_EDIT_MODE_STEP_HELP = 11,
    PAGE_ID_EDIT_MODE_SLIDER = 12,
    PAGE_ID_EDIT_MODE_SLIDER_HELP = 13,
    PAGE_ID_YES_NO = 14,
    PAGE_ID_YES_NO_L = 15,
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
    PAGE_ID_SLOT_SETTINGS = 27,
    PAGE_ID_CH_SETTINGS = 28,
    PAGE_ID_CH_SETTINGS_PROT_OVP = 29,
    PAGE_ID_CH_SETTINGS_PROT_OCP = 30,
    PAGE_ID_CH_SETTINGS_PROT_OPP = 31,
    PAGE_ID_CH_SETTINGS_PROT_OTP = 32,
    PAGE_ID_CH_SETTINGS_TRIGGER = 33,
    PAGE_ID_CH_SETTINGS_LISTS = 34,
    PAGE_ID_CH_SETTINGS_LISTS_INSERT_MENU = 35,
    PAGE_ID_CH_SETTINGS_LISTS_FILE_MENU = 36,
    PAGE_ID_CH_SETTINGS_LISTS_DELETE_MENU = 37,
    PAGE_ID_CH_SETTINGS_CALIBRATION = 38,
    PAGE_ID_CH_SETTINGS_CALIBRATION_EDIT = 39,
    PAGE_ID_CH_SETTINGS_CALIBRATION_VIEW = 40,
    PAGE_ID_CH_SETTINGS_ADV_OPTIONS = 41,
    PAGE_ID_CH_SETTINGS_ADV_RANGES = 42,
    PAGE_ID_CH_SETTINGS_ADV_VIEW = 43,
    PAGE_ID_CH_SETTINGS_INFO = 44,
    PAGE_ID_SYS_SETTINGS = 45,
    PAGE_ID_SYS_SETTINGS_TEMPERATURE = 46,
    PAGE_ID_SYS_SETTINGS_PROTECTIONS = 47,
    PAGE_ID_SYS_SETTINGS_TRIGGER = 48,
    PAGE_ID_SYS_SETTINGS_IO = 49,
    PAGE_ID_SYS_SETTINGS_DATE_TIME = 50,
    PAGE_ID_SYS_SETTINGS_ENCODER = 51,
    PAGE_ID_SYS_SETTINGS_USB = 52,
    PAGE_ID_SYS_SETTINGS_ETHERNET = 53,
    PAGE_ID_SYS_SETTINGS_MQTT = 54,
    PAGE_ID_SYS_SETTINGS_ETHERNET_STATIC = 55,
    PAGE_ID_SYS_SETTINGS_SCREEN_CALIBRATION = 56,
    PAGE_ID_SYS_SETTINGS_DISPLAY = 57,
    PAGE_ID_SYS_SETTINGS_DISPLAY_TEST = 58,
    PAGE_ID_SYS_SETTINGS_SOUND = 59,
    PAGE_ID_SYS_SETTINGS_TRACKING = 60,
    PAGE_ID_SYS_SETTINGS_COUPLING = 61,
    PAGE_ID_SYS_SETTINGS_RAMP_AND_DELAY = 62,
    PAGE_ID_SYS_SETTINGS_FUNCTION_GENERATOR = 63,
    PAGE_ID_SYS_SETTINGS_FUNCTION_GENERATOR_SELECT_CHANNELS = 64,
    PAGE_ID_SYS_INFO = 65,
    PAGE_ID_SYS_SETTINGS_LABELS_AND_COLORS = 66,
    PAGE_ID_USER_PROFILES = 67,
    PAGE_ID_USER_PROFILE_SETTINGS = 68,
    PAGE_ID_USER_PROFILE_0_SETTINGS = 69,
    PAGE_ID_PROFILE_BASIC_INFO = 70,
    PAGE_ID_STAND_BY_MENU = 71,
    PAGE_ID_ENTERING_STANDBY = 72,
    PAGE_ID_STANDBY = 73,
    PAGE_ID_SAVING = 74,
    PAGE_ID_SHUTDOWN = 75,
    PAGE_ID_DISPLAY_OFF = 76,
    PAGE_ID_SLOT_DEF_VERT = 77,
    PAGE_ID_SLOT_DEF_VERT_VIEW_WITH_BORDER = 78,
    PAGE_ID_SLOT_DEF_VERT_VIEW_WITH_BORDER_2COL = 79,
    PAGE_ID_SLOT_DEF_VERT_TITLE = 80,
    PAGE_ID_SLOT_DEF_VERT_TITLE_2COL = 81,
    PAGE_ID_SLOT_DEF_VERT_ERROR = 82,
    PAGE_ID_SLOT_DEF_VERT_ERROR_2COL = 83,
    PAGE_ID_SLOT_DEF_VERT_NOT_INSTALLED = 84,
    PAGE_ID_SLOT_DEF_VERT_NOT_INSTALLED_2COL = 85,
    PAGE_ID_SLOT_DEF_HORZ = 86,
    PAGE_ID_SLOT_DEF_HORZ_VIEW_WITH_BORDER = 87,
    PAGE_ID_SLOT_DEF_HORZ_VIEW_WITH_BORDER_2COL = 88,
    PAGE_ID_SLOT_DEF_HORZ_TITLE = 89,
    PAGE_ID_SLOT_DEF_HORZ_TITLE_2COL = 90,
    PAGE_ID_SLOT_DEF_HORZ_ERROR = 91,
    PAGE_ID_SLOT_DEF_HORZ_ERROR_2COL = 92,
    PAGE_ID_SLOT_DEF_HORZ_NOT_INSTALLED = 93,
    PAGE_ID_SLOT_DEF_HORZ_NOT_INSTALLED_2COL = 94,
    PAGE_ID_SLOT_DEF_HORZ_EMPTY = 95,
    PAGE_ID_SLOT_DEF_HORZ_EMPTY_2COL = 96,
    PAGE_ID_SLOT_MAX_ERROR = 97,
    PAGE_ID_SLOT_MAX_NOT_INSTALLED = 98,
    PAGE_ID_DLOG_VIEW = 99,
    PAGE_ID_DLOG_VIEW_DRAWER = 100,
    PAGE_ID_DLOG_PARAMS = 101,
    PAGE_ID_IMAGE_VIEW = 102,
    PAGE_ID_PINOUT = 103,
    PAGE_ID_FILE_MANAGER = 104,
    PAGE_ID_FILE_BROWSER = 105,
    PAGE_ID_FILE_MENU = 106,
    PAGE_ID_FILE_MANAGER_DETAILS_VIEW = 107,
    PAGE_ID_FILE_MANAGER_SCRIPTS_VIEW = 108,
    PAGE_ID_FILE_MANAGER_SCRIPTS_ALTER_VIEW = 109,
    PAGE_ID_FILE_MANAGER_LARGE_ICONS_VIEW = 110,
    PAGE_ID_TOUCH_TEST = 111,
    PAGE_ID_FOCUS_VALUE_OVERLAY = 112,
    PAGE_ID_OVP_TYPE = 113,
    PAGE_ID_SLOT_LABELS_AND_COLORS = 114,
    PAGE_ID_COLOR_PICKER = 115,
    PAGE_ID_DIB_MIO168_SLOT_VIEW_DEF_VERT = 116,
    PAGE_ID_DIB_MIO168_SLOT_VIEW_DEF_HORZ = 117,
    PAGE_ID_DIB_MIO168_SLOT_VIEW_DEF_2COL_VERT = 118,
    PAGE_ID_DIB_MIO168_SLOT_VIEW_DEF_2COL_HORZ = 119,
    PAGE_ID_DIB_MIO168_SLOT_VIEW_MAX = 120,
    PAGE_ID_DIB_MIO168_LABELS_AND_COLORS = 121,
    PAGE_ID_DIB_MIO168_CHANNEL_LABELS = 122,
    PAGE_ID_DIB_MIO168_DIN_CHANNEL_LABELS = 123,
    PAGE_ID_DIB_MIO168_DOUT_CHANNEL_LABELS = 124,
    PAGE_ID_DIB_MIO168_SETTINGS = 125,
    PAGE_ID_DIB_MIO168_DIN_CONFIGURATION = 126,
    PAGE_ID_DIB_MIO168_DOUT_CONFIGURATION = 127,
    PAGE_ID_DIB_MIO168_AIN_CONFIGURATION = 128,
    PAGE_ID_DIB_MIO168_AOUT_DAC7760_CONFIGURATION = 129,
    PAGE_ID_DIB_MIO168_AOUT_DAC7563_CONFIGURATION = 130,
    PAGE_ID_DIB_MIO168_INFO = 131,
    PAGE_ID_DIB_PREL6_SLOT_VIEW_DEF_VERT = 132,
    PAGE_ID_DIB_PREL6_SLOT_VIEW_DEF_HORZ = 133,
    PAGE_ID_DIB_PREL6_SLOT_VIEW_DEF_2COL_VERT = 134,
    PAGE_ID_DIB_PREL6_SLOT_VIEW_DEF_2COL_HORZ = 135,
    PAGE_ID_DIB_PREL6_SLOT_VIEW_MAX = 136,
    PAGE_ID_DIB_PREL6_SETTINGS = 137,
    PAGE_ID_DIB_PREL6_LABELS_AND_COLORS = 138,
    PAGE_ID_DIB_PREL6_CHANNEL_LABELS = 139,
    PAGE_ID_DIB_PREL6_INFO = 140,
    PAGE_ID_DIB_SMX46_SLOT_VIEW_DEF_VERT = 141,
    PAGE_ID_DIB_SMX46_SLOT_VIEW_DEF_HORZ = 142,
    PAGE_ID_DIB_SMX46_SLOT_VIEW_DEF_2COL_VERT = 143,
    PAGE_ID_DIB_SMX46_SLOT_VIEW_DEF_2COL_HORZ = 144,
    PAGE_ID_DIB_SMX46_SLOT_VIEW_MAX = 145,
    PAGE_ID_DIB_SMX46_SETTINGS = 146,
    PAGE_ID_DIB_SMX46_AOUT_CONFIGURATION = 147,
    PAGE_ID_DIB_SMX46_CONFIGURE_ROUTES = 148,
    PAGE_ID_DIB_SMX46_INFO = 149,
    PAGE_ID_DIB_SMX46_RELAY_CYCLES = 150,
    PAGE_ID_DIB_SMX46_LABELS_AND_COLORS = 151,
    PAGE_ID_DIB_DCP405_SLOT_DEF_1CH_NUM_ON = 152,
    PAGE_ID_DIB_DCP405_SLOT_DEF_1CH_NUM_ON_2COL = 153,
    PAGE_ID_DIB_DCP405_SLOT_DEF_1CH_VBAR_ON = 154,
    PAGE_ID_DIB_DCP405_SLOT_DEF_1CH_VBAR_ON_2COL = 155,
    PAGE_ID_DIB_DCP405_LABELS_AND_COLORS = 156,
    PAGE_ID_DIB_DCP405_SLOT_DEF_1CH_VERT_OFF = 157,
    PAGE_ID_DIB_DCP405_SLOT_DEF_1CH_VERT_OFF_2COL = 158,
    PAGE_ID_DIB_DCP405_SLOT_DEF_1CH_VERT_COUPLED_SERIES = 159,
    PAGE_ID_DIB_DCP405_SLOT_DEF_1CH_VERT_COUPLED_SERIES_2COL = 160,
    PAGE_ID_DIB_DCP405_SLOT_DEF_1CH_VERT_COUPLED_PARALLEL = 161,
    PAGE_ID_DIB_DCP405_SLOT_DEF_1CH_VERT_COUPLED_PARALLEL_2COL = 162,
    PAGE_ID_DIB_DCP405_SLOT_DEF_1CH_HBAR_ON = 163,
    PAGE_ID_DIB_DCP405_SLOT_DEF_1CH_HBAR_ON_2COL = 164,
    PAGE_ID_DIB_DCP405_SLOT_DEF_1CH_YT_ON = 165,
    PAGE_ID_DIB_DCP405_SLOT_DEF_1CH_YT_ON_2COL = 166,
    PAGE_ID_DIB_DCP405_SLOT_DEF_1CH_HORZ_OFF = 167,
    PAGE_ID_DIB_DCP405_SLOT_DEF_1CH_HORZ_OFF_2COL = 168,
    PAGE_ID_DIB_DCP405_SLOT_DEF_1CH_HORZ_COUPLED_SERIES = 169,
    PAGE_ID_DIB_DCP405_SLOT_DEF_1CH_HORZ_COUPLED_SERIES_2COL = 170,
    PAGE_ID_DIB_DCP405_SLOT_DEF_1CH_HORZ_COUPLED_PARALLEL = 171,
    PAGE_ID_DIB_DCP405_SLOT_DEF_1CH_HORZ_COUPLED_PARALLEL_2COL = 172,
    PAGE_ID_DIB_DCP405_SLOT_MAX = 173,
    PAGE_ID_DIB_DCP405_VOLTAGE_MONITOR = 174,
    PAGE_ID_DIB_DCP405_CURRENT_MONITOR = 175,
    PAGE_ID_DIB_DCM220_SLOT_DEF_2CH_VERT = 176,
    PAGE_ID_DIB_DCM220_SLOT_DEF_2CH_VERT_WITH_BORDER = 177,
    PAGE_ID_DIB_DCM220_SLOT_DEF_2CH_VERT_ON = 178,
    PAGE_ID_DIB_DCM220_SLOT_DEF_2CH_VERT_OFF = 179,
    PAGE_ID_DIB_DCM220_SLOT_DEF_2CH_VERT_2COL = 180,
    PAGE_ID_DIB_DCM220_SLOT_DEF_2CH_VERT_WITH_BORDER_2COL = 181,
    PAGE_ID_DIB_DCM220_SLOT_DEF_2CH_VERT_ON_2COL = 182,
    PAGE_ID_DIB_DCM220_SLOT_DEF_2CH_VERT_OFF_2COL = 183,
    PAGE_ID_DIB_DCM220_SLOT_DEF_2CH_HORZ = 184,
    PAGE_ID_DIB_DCM220_SLOT_DEF_2CH_HORZ_WITH_BORDER = 185,
    PAGE_ID_DIB_DCM220_SLOT_DEF_2CH_HORZ_ON = 186,
    PAGE_ID_DIB_DCM220_SLOT_DEF_2CH_HORZ_OFF = 187,
    PAGE_ID_DIB_DCM220_SLOT_DEF_2CH_HORZ_2COL = 188,
    PAGE_ID_DIB_DCM220_SLOT_DEF_2CH_HORZ_WITH_BORDER_2COL = 189,
    PAGE_ID_DIB_DCM220_SLOT_DEF_2CH_HORZ_ON_2COL = 190,
    PAGE_ID_DIB_DCM220_SLOT_DEF_2CH_HORZ_OFF_2COL = 191,
    PAGE_ID_DIB_DCM220_SLOT_MAX = 192,
    PAGE_ID_DIB_DCM220_VOLTAGE_MONITOR = 193,
    PAGE_ID_DIB_DCM220_CURRENT_MONITOR = 194,
    PAGE_ID_DIB_DCM220_LABELS_AND_COLORS = 195,
    PAGE_ID_DIB_DCM220_LABELS_AND_COLORS_CH = 196,
    PAGE_ID_DIB_DCM224_CH_SETTINGS_ADV_OPTIONS = 197,
    PAGE_ID_DIB_DCM224_SLOT_DEF_2CH_VERT = 198,
    PAGE_ID_DIB_DCM224_SLOT_DEF_2CH_VERT_WITH_BORDER = 199,
    PAGE_ID_DIB_DCM224_SLOT_DEF_2CH_VERT_ON = 200,
    PAGE_ID_DIB_DCM224_SLOT_DEF_2CH_VERT_OFF = 201,
    PAGE_ID_DIB_DCM224_SLOT_DEF_2CH_VERT_2COL = 202,
    PAGE_ID_DIB_DCM224_SLOT_DEF_2CH_VERT_WITH_BORDER_2COL = 203,
    PAGE_ID_DIB_DCM224_SLOT_DEF_2CH_VERT_ON_2COL = 204,
    PAGE_ID_DIB_DCM224_SLOT_DEF_2CH_VERT_OFF_2COL = 205,
    PAGE_ID_DIB_DCM224_SLOT_DEF_2CH_HORZ = 206,
    PAGE_ID_DIB_DCM224_SLOT_DEF_2CH_HORZ_WITH_BORDER = 207,
    PAGE_ID_DIB_DCM224_SLOT_DEF_2CH_HORZ_ON = 208,
    PAGE_ID_DIB_DCM224_SLOT_DEF_2CH_HORZ_OFF = 209,
    PAGE_ID_DIB_DCM224_SLOT_DEF_2CH_HORZ_2COL = 210,
    PAGE_ID_DIB_DCM224_SLOT_DEF_2CH_HORZ_WITH_BORDER_2COL = 211,
    PAGE_ID_DIB_DCM224_SLOT_DEF_2CH_HORZ_ON_2COL = 212,
    PAGE_ID_DIB_DCM224_SLOT_DEF_2CH_HORZ_OFF_2COL = 213,
    PAGE_ID_DIB_DCM224_SLOT_MAX = 214,
    PAGE_ID_DIB_DCM224_VOLTAGE_MONITOR = 215,
    PAGE_ID_DIB_DCM224_CURRENT_MONITOR = 216,
    PAGE_ID_DIB_DCM224_LABELS_AND_COLORS = 217,
    PAGE_ID_DIB_DCM224_LABELS_AND_COLORS_CH = 218,
    PAGE_ID_DIB_MUX14D_SLOT_VIEW_DEF_VERT = 219,
    PAGE_ID_DIB_MUX14D_SLOT_VIEW_DEF_HORZ = 220,
    PAGE_ID_DIB_MUX14D_SLOT_VIEW_DEF_2COL_VERT = 221,
    PAGE_ID_DIB_MUX14D_SLOT_VIEW_DEF_2COL_HORZ = 222,
    PAGE_ID_DIB_MUX14D_SLOT_VIEW_MAX = 223,
    PAGE_ID_DIB_MUX14D_SETTINGS = 224,
    PAGE_ID_DIB_MUX14D_LABELS_AND_COLORS = 225,
    PAGE_ID_DIB_MUX14D_CHANNEL_LABELS = 226,
    PAGE_ID_DIB_MUX14D_INFO = 227,
    PAGE_ID_DIB_MUX14D_RELAY_CYCLES = 228
};

extern const uint8_t assets[386849];
