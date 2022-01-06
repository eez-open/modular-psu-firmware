/* / mcu / sound.h
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
#include <ctype.h>
#include <math.h>
#include <string.h>

#include <eez/core/sound.h>

#include <eez/gui/gui.h>
#include <eez/gui/widgets/button.h>
#include <eez/gui/widgets/text.h>
#include <eez/gui/widgets/display_data.h>
#include <eez/gui/keypad.h>

#ifdef _MSC_VER
#pragma warning(disable : 4996)
#endif

#define CONF_GUI_KEYPAD_PASSWORD_LAST_CHAR_VISIBLE_DURATION_MS 500UL

namespace eez {
namespace gui {

void Keypad::pageAlloc() {
}

void Keypad::pageFree() {
    m_appContext = nullptr;
}

void Keypad::init(AppContext *appContext, const char *label_) {
    if (m_appContext != appContext) {
        if (m_appContext) {
            m_appContext->popPage();
        }
        m_appContext = appContext;
    }

    m_label[0] = 0;
    m_keypadText[0] = 0;
    m_cursorPosition = 0;
    m_xScroll = 0;
    m_okCallback = 0;
    m_cancelCallback = 0;
    m_setDefaultCallback = 0;
    m_keypadMode = KEYPAD_MODE_LOWERCASE;
    m_isPassword = false;

    if (label_) {
        stringCopy(m_label, sizeof(m_label), label_);
    } else {
        m_label[0] = 0;
    }
}

Value Keypad::getKeypadTextValue() {
    char text[MAX_KEYPAD_TEXT_LENGTH + 2];
    getKeypadText(text, sizeof(text));
    return Value::makeStringRef(text, strlen(text), 0x893cbf99);
}

void Keypad::getKeypadText(char *text, size_t count) {
    // text
    char *textPtr = text;

    if (*m_label) {
        stringCopy(textPtr, count, m_label);
        auto len = strlen(m_label);
        textPtr += len;
        count -= len;
    }

    if (m_isPassword) {
        int n = strlen(m_keypadText);
        if (n > 0) {
            int i;

            for (i = 0; i < n - 1; ++i) {
                *textPtr++ = '*';
            }

            uint32_t currentTimeMs = millis();
            if (
                currentTimeMs - m_lastKeyAppendTimeMs <=
                CONF_GUI_KEYPAD_PASSWORD_LAST_CHAR_VISIBLE_DURATION_MS
            ) {
                *textPtr++ = m_keypadText[i];
            } else {
                *textPtr++ = '*';
            }

            *textPtr++ = 0;
        } else {
            *textPtr = 0;
        }
    } else {
        stringCopy(textPtr, count, m_keypadText);
    }
}

void Keypad::start(AppContext *appContext, const char *label, const char *text, int minChars_, int maxChars_, bool isPassword_, void (*ok)(char *), void (*cancel)(), void (*setDefault)()) {
    init(appContext, label);

    m_okCallback = ok;
    m_cancelCallback = cancel;
    m_setDefaultCallback = setDefault;

    m_minChars = minChars_;
    m_maxChars = maxChars_;
    m_isPassword = isPassword_;

    if (text) {
        stringCopy(m_keypadText, sizeof(m_keypadText), text);
        m_cursorPosition = strlen(m_keypadText);
    } else {
        m_keypadText[0] = 0;
        m_cursorPosition = 0;
    }
    m_keypadMode = KEYPAD_MODE_LOWERCASE;
}

void Keypad::insertChar(char c) {
    int n = strlen(m_keypadText);
    if (n < m_maxChars && (n + (*m_label ? strlen(m_label) : 0)) < MAX_KEYPAD_TEXT_LENGTH) {
        for (int i = n; i >= m_cursorPosition; i--) {
            m_keypadText[i + 1] = m_keypadText[i];
        }
        m_keypadText[m_cursorPosition] = c;
        m_cursorPosition++;
        m_lastKeyAppendTimeMs = millis();
    } else {
        sound::playBeep();
    }
}

void Keypad::key() {
	auto widgetCursor = getFoundWidgetAtDown();
	auto widget = widgetCursor.widget;
	
	if (widget->type == WIDGET_TYPE_TEXT) {
		auto textWidget = (TextWidget *)widget;
		if (textWidget->text) {
			key(textWidget->text.ptr(widgetCursor.assets)[0]);
			return;
		}
	} else if (widget->type == WIDGET_TYPE_BUTTON) {
		auto buttonWidget = (ButtonWidget *)widget;
		if (buttonWidget->text) {
			key(buttonWidget->text.ptr(widgetCursor.assets)[0]);
			return;
		}
	}

	Value value = get(widgetCursor, widget->data);

	char str[10];
	value.toText(str, sizeof(str));
	key(str[0]);
}

void Keypad::key(char ch) {
    insertChar(ch);
}

void Keypad::space() {
    insertChar(' ');
}

void Keypad::back() {
    if (m_cursorPosition > 0) {
        int n = strlen(m_keypadText);
        for (int i = m_cursorPosition; i < n; i++) {
            m_keypadText[i - 1] = m_keypadText[i];
        }
        m_cursorPosition--;
        m_keypadText[n - 1] = 0;
    } else {
        sound::playBeep();
    }
}

void Keypad::clear() {
    m_keypadText[0] = 0;
    m_cursorPosition = 0;
}

void Keypad::sign() {
}

void Keypad::unit() {
}

bool Keypad::isOkEnabled() {
    int n = strlen(m_keypadText);
    return n >= m_minChars && n <= m_maxChars;
}

void Keypad::ok() {
    m_okCallback(m_keypadText);
}

void Keypad::cancel() {
    if (m_cancelCallback) {
        m_cancelCallback();
    } else {
        m_appContext->popPage();
    }
}

bool Keypad::canSetDefault() {
    return m_setDefaultCallback != nullptr;
}

void Keypad::setDefault() {
    if (m_setDefaultCallback) {
        m_setDefaultCallback();
    }
}

int Keypad::getCursorPosition() {
    return m_cursorPosition + strlen(m_label);
}

void Keypad::setCursorPosition(int cursorPosition) {
    m_cursorPosition = cursorPosition - strlen(m_label);

    if (m_cursorPosition < 0) {
        m_cursorPosition = 0;
    } else {
        int n = strlen(m_keypadText);
        if (m_cursorPosition > n) {
            m_cursorPosition = n;
        }
    }
}

int Keypad::getXScroll(const WidgetCursor &widgetCursor) {
    int x = DISPLAY_DATA_getCursorXPosition(getCursorPosition(), widgetCursor);
    
    x -= widgetCursor.x;

    if (x < m_xScroll + widgetCursor.widget->w / 4) {
        m_xScroll = MAX(x - widgetCursor.widget->w / 2, 0);
    } else if (m_xScroll + widgetCursor.widget->w < x + CURSOR_WIDTH) {
        m_xScroll = x + CURSOR_WIDTH - widgetCursor.widget->w;
    }

    return m_xScroll;
}

////////////////////////////////////////////////////////////////////////////////

NumericKeypadOptions::NumericKeypadOptions() {
    pageId = PAGE_ID_NUMERIC_KEYPAD;

    this->slotIndex = -1;
    this->subchannelIndex = -1;

    allowZero = false;
    min = NAN;
    max = NAN;

    flags.checkWhileTyping = false;
    flags.signButtonEnabled = false;
    flags.dotButtonEnabled = false;

    option1Action = NUMERIC_KEYPAD_OPTION_ACTION_NONE;
    option2Action = NUMERIC_KEYPAD_OPTION_ACTION_NONE;
    option3Action = NUMERIC_KEYPAD_OPTION_ACTION_NONE;

    editValueUnit = UNIT_UNKNOWN;

    encoderPrecision = 0.01f;
}

void NumericKeypadOptions::enableMaxButton() {
    option1Action = NUMERIC_KEYPAD_OPTION_ACTION_MAX;
    option1ButtonText = "max";
}

void NumericKeypadOptions::enableMinButton() {
    if (option2Action != NUMERIC_KEYPAD_OPTION_ACTION_NONE) {
        option3Action = NUMERIC_KEYPAD_OPTION_ACTION_MIN;
        option3ButtonText = "min";
    } else {
        option2Action = NUMERIC_KEYPAD_OPTION_ACTION_MIN;
        option2ButtonText = "min";
    }
}

void NumericKeypadOptions::enableDefButton() {
	if (option2Action != NUMERIC_KEYPAD_OPTION_ACTION_NONE) {
        option3Action = NUMERIC_KEYPAD_OPTION_ACTION_DEF;
        option3ButtonText = "def";
    } else {
        option2Action = NUMERIC_KEYPAD_OPTION_ACTION_DEF;
        option2ButtonText = "def";
    }
}

////////////////////////////////////////////////////////////////////////////////

void NumericKeypad::init(
    AppContext *appContext,
    const char *label,
    const Value &value,
    NumericKeypadOptions &options,
    void(*okFloat)(float),
    void(*okUint32)(uint32_t),
    void(*cancel)()
) {
    Keypad::init(appContext, label);

    m_okFloatCallback = okFloat;
    m_okUint32Callback = okUint32;
    m_cancelCallback = cancel;

    m_startValue = value;

    m_options = options;

    if (value.getType() == VALUE_TYPE_IP_ADDRESS) {
        m_options.flags.dotButtonEnabled = true;
    }

    m_options.editValueUnit = findDerivedUnit(m_startValue.getFloat(), m_startValue.getUnit());

    m_minChars = 0;
    m_maxChars = 16;

    reset();

    if (value.getType() == VALUE_TYPE_IP_ADDRESS) {
        ipAddressToString(value.getUInt32(), m_keypadText, sizeof(m_keypadText));
        m_cursorPosition = strlen(m_keypadText);
        m_state = BEFORE_DOT;
    }
}

bool NumericKeypad::isEditing() {
    return m_state != EMPTY && m_state != START;
}

char NumericKeypad::getDotSign() {
    if (m_startValue.getType() == VALUE_TYPE_TIME_ZONE) {
        return ':';
    }
    return '.';
}

void NumericKeypad::appendEditUnit(char *text, size_t maxTextLength) {
    stringAppendString(text, maxTextLength, " ");
    stringAppendString(text, maxTextLength, getUnitName(m_options.editValueUnit));
}

void NumericKeypad::getKeypadText(char *text, size_t count) {
    if (*m_label) {
        stringCopy(text, count, m_label);
        text += strlen(m_label);
    }

    getText(text, 16);
}

bool NumericKeypad::getText(char *text, size_t count) {
    if (m_state == START) {
        m_startValue.toText(text, count);
        return false;
    }
    stringCopy(text, count, m_keypadText);
    appendEditUnit(text, count);
    return true;
}

////////////////////////////////////////////////////////////////////////////////

Unit NumericKeypad::getEditUnit() {
    return m_options.editValueUnit;
}


bool NumericKeypad::isMicroAmperAllowed() {
    return true;
}

bool NumericKeypad::isAmperAllowed() {
	return true;
}

float NumericKeypad::getPrecision() {
    return m_options.encoderPrecision;
}

Unit NumericKeypad::getSwitchToUnit() {
    Unit unit = getSmallerUnit(m_options.editValueUnit, m_options.min, getPrecision());
    if (unit == UNIT_UNKNOWN) {
        unit = getBiggestUnit(m_options.editValueUnit, m_options.max);
    }

	if (unit == UNIT_UNKNOWN) {
		return m_options.editValueUnit;
	}

    if (unit == UNIT_MICRO_AMPER) {
        if (isMicroAmperAllowed()) {
            return UNIT_MICRO_AMPER;
        } else {
            return getBiggestUnit(m_options.editValueUnit, m_options.max);
        }
    }

    if (unit == UNIT_MICRO_AMPER_PP) {
        if (isMicroAmperAllowed()) {
            return UNIT_MICRO_AMPER_PP;
        } else {
            return getBiggestUnit(m_options.editValueUnit, m_options.max);
        }
    }

    if (unit == UNIT_AMPER) {
        if (isAmperAllowed()) {
            return UNIT_AMPER;
        } else {
            return getSmallestUnit(m_options.editValueUnit, m_options.min, getPrecision());
        }
    }

    if (unit == UNIT_AMPER_PP) {
        if (isAmperAllowed()) {
            return UNIT_AMPER_PP;
        } else {
            return getSmallestUnit(m_options.editValueUnit, m_options.min, getPrecision());
        }
    }
        
    return unit;
}

void NumericKeypad::toggleEditUnit() {
    m_options.editValueUnit = getSwitchToUnit();
}

int NumericKeypad::getCursorPosition() {
    if (m_state == START) {
        return -1;
    }

    return Keypad::getCursorPosition();
}

////////////////////////////////////////////////////////////////////////////////

double NumericKeypad::getValue() {
    const char *p = m_keypadText;

    bool empty = true;

    int a = 0;
    double b = 0;
    int sign = 1;

    if (*p == '-') {
        sign = -1;
        ++p;
    } else if (*p == '+') {
        ++p;
    }

    while (*p && *p != getDotSign()) {
        a = a * 10 + (*p - '0');
        ++p;
        empty = false;
    }

    if (*p) {
        const char *q = p + strlen(p) - 1;
        while (q != p) {
            b = (b + (*q - '0')) / 10;
            --q;
            empty = false;
        }
    }

    if (empty) {
        return NAN;
    }

    return sign * (a + b) * getUnitFactor(m_options.editValueUnit);
}

bool NumericKeypad::checkNumSignificantDecimalDigits() {
    return true;
}

void NumericKeypad::digit(int d) {
    if (m_state == START || m_state == EMPTY) {
        m_state = BEFORE_DOT;
        if (m_startValue.getType() == VALUE_TYPE_TIME_ZONE) {
            if (strlen(m_keypadText) == 0) {
                insertChar('+');
            }
        }
    } else {
        if (m_keypadText[m_cursorPosition] == '-' || m_keypadText[m_cursorPosition] == '+') {
            sound::playBeep();
            return;
        }
    }

    insertChar(d + '0');

    if (!checkNumSignificantDecimalDigits()) {
        back();
        sound::playBeep();
    }
}

void NumericKeypad::dot() {
    if (!m_options.flags.dotButtonEnabled) {
        return;
    }

    if (m_startValue.getType() == VALUE_TYPE_IP_ADDRESS) {
        if (m_state == EMPTY || m_state == START) {
            sound::playBeep();
        } else {
            insertChar(getDotSign());
        }
        return;
    }

    if (m_state == START || m_state == EMPTY) {
        if (m_startValue.getType() == VALUE_TYPE_TIME_ZONE) {
            if (strlen(m_keypadText) == 0) {
                insertChar('+');
            }
        }
        insertChar('0');
        m_state = BEFORE_DOT;
    }

    if (m_state == BEFORE_DOT && m_keypadText[m_cursorPosition] != '-' && m_keypadText[m_cursorPosition] != '+') {
        insertChar(getDotSign());
        m_state = AFTER_DOT;
    } else {
        sound::playBeep();
    }
}

void NumericKeypad::reset() {
    m_state = m_startValue.getType() != VALUE_TYPE_UNDEFINED ? START : EMPTY;
    m_keypadText[0] = 0;
    m_cursorPosition = 0;
}

void NumericKeypad::key(char ch) {
    if (ch >= '0' && ch <= '9') {
        digit(ch - '0');
    } else if (ch == '.') {
        dot();
    }
}

void NumericKeypad::space() {
    // DO NOTHING
}

void NumericKeypad::caps() {
    // DO NOTHING
}

void NumericKeypad::back() {
    if (m_cursorPosition > 0) {
        if (m_keypadText[m_cursorPosition - 1] == getDotSign()) {
            m_state = BEFORE_DOT;
        }

        Keypad::back();

        int n = strlen(m_keypadText);

        if (n == 1) {
            if (m_keypadText[0] == '+' || m_keypadText[0] == '-') {
                m_state = EMPTY;
            }
        } else if (n == 0) {
            m_state = EMPTY;
        }
    } else if (m_state == START) {
        m_state = EMPTY;
    } else {
        sound::playBeep();
    }
}

void NumericKeypad::clear() {
    if (m_state != START) {
        reset();
    } else {
        sound::playBeep();
    }
}

void NumericKeypad::sign() {
    if (m_options.flags.signButtonEnabled) {
        if (m_startValue.getType() == VALUE_TYPE_TIME_ZONE) {
            if (m_keypadText[0] == 0) {
                m_keypadText[0] = '-';
                m_keypadText[1] = 0;
                m_cursorPosition = 1;
                m_state = BEFORE_DOT;
            } else if (m_keypadText[0] == '-') {
                m_keypadText[0] = '+';
            } else {
                m_keypadText[0] = '-';
            }
        } else {
            if (m_keypadText[0] == '-') {
                stringCopy(m_keypadText, sizeof(m_keypadText), m_keypadText + 1);
                if (m_cursorPosition > 0) {
                    m_cursorPosition--;
                }
            } else if (m_keypadText[0] == '+') {
                m_keypadText[0] = '-';
            } else {
                auto n = strlen(m_keypadText);
                memmove(m_keypadText + 1, m_keypadText, n);
                m_keypadText[n + 1] = 0;
                m_keypadText[0] = '-';
                m_cursorPosition++;
            }

            if (m_state == START || m_state == EMPTY) {
                m_state = BEFORE_DOT;
            }
        }
    } else {
        // not supported
        sound::playBeep();
    }
}

void NumericKeypad::unit() {
    if (m_state == START) {
        m_state = EMPTY;
    }
    toggleEditUnit();
}

bool NumericKeypad::isOkEnabled() {
    return true;
}

void NumericKeypad::showLessThanMinErrorMessage() {
	m_appContext->errorMessageWithAction("Less than min. allowed value", nullptr, "Close");
}

void NumericKeypad::showGreaterThanMaxErrorMessage() {
	m_appContext->errorMessageWithAction("Greater than max. allowed value", nullptr, "Close");
}

void NumericKeypad::ok() {
    if (m_state == START) {
        if (m_startValue.getType() == VALUE_TYPE_IP_ADDRESS) {
            m_okFloatCallback(1.0f * m_startValue.getUInt32());
        } else if (m_startValue.getType() == VALUE_TYPE_TIME_ZONE) {
            m_okFloatCallback(m_startValue.getInt() / 100.0f);
        } else {
            m_okFloatCallback(m_startValue.isFloat() ? m_startValue.getFloat()
                                                     : m_startValue.getInt());
        }

        return;
    }

    if (m_state != EMPTY) {
        if (m_startValue.getType() == VALUE_TYPE_IP_ADDRESS) {
            uint32_t ipAddress;
            if (parseIpAddress(m_keypadText, strlen(m_keypadText), ipAddress)) {
                m_okUint32Callback(ipAddress);
                m_state = START;
                m_keypadText[0] = 0;
                m_cursorPosition = 0;
            } else {
				m_appContext->errorMessage("Invalid IP address format!");
            }

            return;
        } else {
            float value = (float)getValue();
            if (isNaN(value)) {
                sound::playBeep();
                return;
            }

            float EPSILON = 1E-9f;

            if (!isNaN(m_options.min) && value < m_options.min && !(value == 0 && m_options.allowZero)) {
                if (value < m_options.min - EPSILON) {
					showLessThanMinErrorMessage();
                    return;
                }
                value = m_options.min;
            } else if (!isNaN(m_options.max) && value > m_options.max) {
                if (value > m_options.max + EPSILON) {
					showGreaterThanMaxErrorMessage();
					return;
                }
                value = m_options.max;
            }

            m_okFloatCallback((float)value);

            return;
        }
    }

    sound::playBeep();
}

void NumericKeypad::cancel() {
    if (m_cancelCallback) {
        m_cancelCallback();
    } else {
        m_appContext->popPage();
    }
}

void NumericKeypad::setMaxValue() {
    m_okFloatCallback(m_options.max);
}

void NumericKeypad::setMinValue() {
    m_okFloatCallback(m_options.min);
}

void NumericKeypad::setDefValue() {
    m_okFloatCallback(m_options.def);
}

////////////////////////////////////////////////////////////////////////////////

void onKeypadTextTouch(const WidgetCursor &widgetCursor, Event &touchEvent) {
    if (touchEvent.type != EVENT_TYPE_TOUCH_DOWN && touchEvent.type != EVENT_TYPE_TOUCH_MOVE) {
        return;
    }

    Keypad *keypad = getActiveKeypad();
    if (keypad) {
        keypad->setCursorPosition(DISPLAY_DATA_getCharIndexAtPosition(touchEvent.x, widgetCursor));
    }
}

////////////////////////////////////////////////////////////////////////////////

void data_keypad_edit_unit(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        Keypad *keypad = getActiveKeypad();
        if (keypad) {
            value = getUnitName(keypad->getSwitchToUnit());
        }
    }
}

void data_keypad_text(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        Keypad *keypad = getActiveKeypad();
        if (keypad) {
            value = keypad->getKeypadTextValue();
        }
    } else if (operation == DATA_OPERATION_GET_TEXT_CURSOR_POSITION) {
        Keypad *keypad = getActiveKeypad();
        if (keypad) {
            value = keypad->getCursorPosition();
        }
    } else if (operation == DATA_OPERATION_GET_X_SCROLL) {
        Keypad *keypad = getActiveKeypad();
        if (keypad) {
            value = keypad->getXScroll(*(WidgetCursor *)value.getVoidPointer());
        }
    }
}

void data_keypad_mode(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        Keypad *keypad = getActiveKeypad();
        if (keypad) {
            value = keypad->m_keypadMode;
        }
    }
}

void data_keypad_option1_text(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        NumericKeypad *keypad = getActiveNumericKeypad();
        if (keypad) {
            value = Value(keypad->m_options.option1Action != NUMERIC_KEYPAD_OPTION_ACTION_NONE ? keypad->m_options.option1ButtonText : "");
        }
    }
}

void data_keypad_option1_enabled(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        NumericKeypad *keypad = getActiveNumericKeypad();
        if (keypad) {
            value = (int)(keypad->m_options.option1Action != NUMERIC_KEYPAD_OPTION_ACTION_NONE);
        }
    }
}

void data_keypad_option2_text(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        NumericKeypad *keypad = getActiveNumericKeypad();
        if (keypad) {
            value = Value(keypad->m_options.option2Action != NUMERIC_KEYPAD_OPTION_ACTION_NONE ? keypad->m_options.option2ButtonText : "");
        }
    }
}

void data_keypad_option2_enabled(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        NumericKeypad *keypad = getActiveNumericKeypad();
        if (keypad) {
            value = (int)(keypad->m_options.option2Action != NUMERIC_KEYPAD_OPTION_ACTION_NONE);
        }
    }
}

void data_keypad_option3_text(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        NumericKeypad *keypad = getActiveNumericKeypad();
        if (keypad) {
            value = Value(keypad->m_options.option3Action != NUMERIC_KEYPAD_OPTION_ACTION_NONE ? keypad->m_options.option3ButtonText : "");
        }
    }
}

void data_keypad_option3_enabled(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        NumericKeypad *keypad = getActiveNumericKeypad();
        if (keypad) {
            value = (int)(keypad->m_options.option3Action != NUMERIC_KEYPAD_OPTION_ACTION_NONE);
        }
    }
}

void data_keypad_sign_enabled(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        NumericKeypad *keypad = getActiveNumericKeypad();
        if (keypad) {
            value = (int)keypad->m_options.flags.signButtonEnabled;
        }
    }
}

void data_keypad_dot_enabled(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        NumericKeypad *keypad = getActiveNumericKeypad();
        if (keypad) {
            value = (int)keypad->m_options.flags.dotButtonEnabled;
        }
    }
}

void data_keypad_unit_enabled(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        NumericKeypad *keypad = getActiveNumericKeypad();
        if (keypad) {
            value = 
                getSmallestUnit(keypad->m_options.editValueUnit, keypad->m_options.min, keypad->getPrecision()) !=
                getBiggestUnit(keypad->m_options.editValueUnit, keypad->m_options.max);
        }
    }
}

void data_keypad_ok_enabled(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        Keypad *keypad = getActiveKeypad();
        if (keypad) {
            value = keypad->isOkEnabled() ? 1 : 0;
        }
    }
}

void data_keypad_can_set_default(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        Keypad *keypad = getActiveKeypad();
        if (keypad) {
            value = keypad->canSetDefault();
        }
    }
}

void action_keypad_key() {
    getActiveKeypad()->key();
}

void action_keypad_space() {
    getActiveKeypad()->space();
}

void action_keypad_back() {
    getActiveKeypad()->back();
}

void action_keypad_clear() {
    getActiveKeypad()->clear();
}

void action_toggle_keypad_mode() {
    auto keypad = getActiveKeypad();
    if (keypad->m_keypadMode == KEYPAD_MODE_LOWERCASE) {
        getActiveKeypad()->m_keypadMode = KEYPAD_MODE_UPPERCASE;
    } else if (keypad->m_keypadMode == KEYPAD_MODE_UPPERCASE) {
        getActiveKeypad()->m_keypadMode = KEYPAD_MODE_SYMBOL;
    } else {
        getActiveKeypad()->m_keypadMode = KEYPAD_MODE_LOWERCASE;
    }
}

void action_keypad_ok() {
    getActiveKeypad()->ok();
}

void action_keypad_cancel() {
    getActiveKeypad()->cancel();
}

void action_keypad_set_default() {
    getActiveKeypad()->setDefault();
}

void action_keypad_sign() {
    getActiveKeypad()->sign();
}

void action_keypad_unit() {
    getActiveKeypad()->unit();
}

void action_keypad_option1() {
    executeNumericKeypadOptionHook(1);
}

void action_keypad_option2() {
    executeNumericKeypadOptionHook(2);
}

void action_keypad_option3() {
    executeNumericKeypadOptionHook(3);
}

} // namespace gui
} // namespace eez
