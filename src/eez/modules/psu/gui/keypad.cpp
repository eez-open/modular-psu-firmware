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

#if OPTION_DISPLAY

#include <assert.h>
#include <ctype.h>
#include <math.h>
#include <string.h>

#include <eez/sound.h>
#include <eez/system.h>
#include <eez/index.h>

#include <eez/gui/gui.h>
#include <eez/gui/widgets/display_data.h>

#include <eez/modules/psu/psu.h>
#include <eez/modules/psu/channel_dispatcher.h>

#include <eez/modules/psu/gui/psu.h>
#include <eez/modules/psu/gui/edit_mode.h>
#include <eez/modules/psu/gui/keypad.h>

#if defined(EEZ_PLATFORM_SIMULATOR)
#include <eez/platform/simulator/front_panel.h>
#endif

#ifdef _MSC_VER
#pragma warning(disable : 4996)
#endif

#define CONF_GUI_KEYPAD_PASSWORD_LAST_CHAR_VISIBLE_DURATION_MS 500UL

namespace eez {
namespace psu {
namespace gui {

static Keypad g_keypad;
static NumericKeypad g_numericKeypad;

////////////////////////////////////////////////////////////////////////////////

Keypad *getActiveKeypad() {
    if (getActivePageId() == PAGE_ID_KEYPAD || getActivePageId() == PAGE_ID_NUMERIC_KEYPAD) {
        return (Keypad *)getActivePage();
    }
    
#if defined(EEZ_PLATFORM_SIMULATOR)
    if (g_frontPanelAppContext.getActivePageId() == PAGE_ID_FRONT_PANEL_NUMERIC_KEYPAD) {
        return (Keypad *)g_frontPanelAppContext.getActivePage();
    }
#endif

    if (getActivePageId() == PAGE_ID_EDIT_MODE_KEYPAD) {
        return edit_mode_keypad::g_keypad;
    }
    
    return 0;
}

NumericKeypad *getActiveNumericKeypad() {
    if (getActivePageId() == PAGE_ID_NUMERIC_KEYPAD) {
        return (NumericKeypad *)getActivePage();
    }
    
#if defined(EEZ_PLATFORM_SIMULATOR)
    if (g_frontPanelAppContext.getActivePageId() == PAGE_ID_FRONT_PANEL_NUMERIC_KEYPAD) {
        return (NumericKeypad *)g_frontPanelAppContext.getActivePage();
    }
#endif

    if (getActivePageId() == PAGE_ID_EDIT_MODE_KEYPAD) {
        return edit_mode_keypad::g_keypad;
    }
    
    return 0;
}

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
    char *text = &m_stateText[getCurrentStateBufferIndex()][0];
    getKeypadText(text, sizeof(m_stateText[0]));
    return Value(text);
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

void Keypad::startPush(const char *label, const char *text, int minChars_, int maxChars_, bool isPassword_, void (*ok)(char *), void (*cancel)(), void (*setDefault)()) {
    g_keypad.start(&g_psuAppContext, label, text, minChars_, maxChars_, isPassword_, ok, cancel, setDefault);
    pushPage(PAGE_ID_KEYPAD, &g_keypad);
}

void Keypad::startReplace(const char *label, const char *text, int minChars_, int maxChars_, bool isPassword_, void (*ok)(char *), void (*cancel)()) {
    g_keypad.start(&g_psuAppContext, label, text, minChars_, maxChars_, isPassword_, ok, cancel, nullptr);
    replacePage(PAGE_ID_KEYPAD, &g_keypad);
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
    auto textWidget = (TextWidget *)getFoundWidgetAtDown().widget;
    key(textWidget->text.ptr(getFoundWidgetAtDown().assets)[0]);
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

void Keypad::option1() {
}

void Keypad::option2() {
}

void Keypad::option3() {
}

void Keypad::setMaxValue() {
}

void Keypad::setMinValue() {
}

void Keypad::setDefValue() {
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

int Keypad::getCursorPostion() {
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
    int x = DISPLAY_DATA_getCursorXPosition(getCursorPostion(), widgetCursor);
    
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
    flags.option1ButtonEnabled = false;
    flags.option2ButtonEnabled = false;
    flags.option3ButtonEnabled = false;
    flags.signButtonEnabled = false;
    flags.dotButtonEnabled = false;

    editValueUnit = UNIT_UNKNOWN;

    encoderPrecision = 0.01f;
}

void NumericKeypadOptions::enableMaxButton() {
    flags.option1ButtonEnabled = true;
    option1ButtonText = "max";
    option1 = maxOption;
}

void NumericKeypadOptions::enableMinButton() {
    if (flags.option2ButtonEnabled) {
        flags.option3ButtonEnabled = true;
        option3ButtonText = "min";
        option3 = minOption;
    } else {
        flags.option2ButtonEnabled = true;
        option2ButtonText = "min";
        option2 = minOption;
    }
}

void NumericKeypadOptions::enableDefButton() {
    if (flags.option2ButtonEnabled) {
        flags.option3ButtonEnabled = true;
        option3ButtonText = "def";
        option3 = defOption;
    } else {
        flags.option2ButtonEnabled = true;
        option2ButtonText = "def";
        option2 = defOption;
    }
}

void NumericKeypadOptions::maxOption() {
    getActiveKeypad()->setMaxValue();
}

void NumericKeypadOptions::minOption() {
    getActiveKeypad()->setMinValue();
}

void NumericKeypadOptions::defOption() {
    getActiveKeypad()->setDefValue();
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

    if (m_startValue.isPico()) {
        switchToPico();
    } else if (m_startValue.isNano()) {
        switchToNano();
    } else if (m_startValue.isMicro()) {
        switchToMicro();
    } else if (m_startValue.isMilli()) {
        switchToMilli();
    } else if (m_startValue.isKilo()) {
        switchToKilo();
    } else if (m_startValue.isMega()) {
        switchToMega();
    }

    m_minChars = 0;
    m_maxChars = 16;

    reset();

    if (value.getType() == VALUE_TYPE_IP_ADDRESS) {
        ipAddressToString(value.getUInt32(), m_keypadText, sizeof(m_keypadText));
        m_cursorPosition = strlen(m_keypadText);
        m_state = BEFORE_DOT;
    }
}

NumericKeypad *NumericKeypad::start(
    AppContext *appContext,
    const char *label,
    const Value &value,
    NumericKeypadOptions &options,
    void (*okFloat)(float),
    void (*okUint32)(uint32_t),
    void (*cancel)()
) {
    g_numericKeypad.init(appContext, label, value, options, okFloat, okUint32, cancel);
    appContext->pushPage(options.pageId, &g_numericKeypad);
    return &g_numericKeypad;
}

NumericKeypad *NumericKeypad::start(
    const char *label,
    const Value &value,
    NumericKeypadOptions &options,
    void (*okFloat)(float),
    void (*okUint32)(uint32_t), 
    void (*cancel)()
) {
    return start(&g_psuAppContext, label, value, options, okFloat, okUint32, cancel);
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
        if (m_appContext->getActivePageId() == PAGE_ID_EDIT_MODE_KEYPAD) {
            edit_mode::getCurrentValue().toText(text, count);
        } else {
            m_startValue.toText(text, count);
        }
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

Unit NumericKeypad::getValueUnit() {
    if (m_options.editValueUnit == UNIT_MILLI_VOLT)
        return UNIT_VOLT;
    if (m_options.editValueUnit == UNIT_MILLI_VOLT_PP)
        return UNIT_VOLT_PP;
    if (m_options.editValueUnit == UNIT_MILLI_AMPER || m_options.editValueUnit == UNIT_MICRO_AMPER)
        return UNIT_AMPER;
    if (m_options.editValueUnit == UNIT_MILLI_AMPER_PP || m_options.editValueUnit == UNIT_MICRO_AMPER_PP)
        return UNIT_AMPER_PP;
    if (m_options.editValueUnit == UNIT_MILLI_SECOND)
        return UNIT_SECOND;
    if (m_options.editValueUnit == UNIT_MILLI_WATT)
        return UNIT_WATT;
    if (m_options.editValueUnit == UNIT_OHM)
        return UNIT_KOHM;
    if (m_options.editValueUnit == UNIT_KOHM)
        return UNIT_MOHM;
    if (m_options.editValueUnit == UNIT_MOHM)
        return UNIT_OHM;
    if (m_options.editValueUnit == UNIT_HERTZ)
        return UNIT_KHERTZ;
    if (m_options.editValueUnit == UNIT_KHERTZ)
        return UNIT_MHERTZ;
    if (m_options.editValueUnit == UNIT_MHERTZ)
        return UNIT_MILLI_HERTZ;
    if (m_options.editValueUnit == UNIT_MILLI_HERTZ)
        return UNIT_HERTZ;
    if (m_options.editValueUnit == UNIT_FARAD)
        return UNIT_MILLI_FARAD;
    if (m_options.editValueUnit == UNIT_MILLI_FARAD)
        return UNIT_MICRO_FARAD;
    if (m_options.editValueUnit == UNIT_MICRO_FARAD)
        return UNIT_NANO_FARAD;
    if (m_options.editValueUnit == UNIT_NANO_FARAD)
        return UNIT_PICO_FARAD;
    if (m_options.editValueUnit == UNIT_PICO_FARAD)
        return UNIT_FARAD;
    return m_options.editValueUnit;
}

bool NumericKeypad::isPico() {
    return m_options.editValueUnit == UNIT_PICO_FARAD;
}

bool NumericKeypad::isNano() {
    return m_options.editValueUnit == UNIT_NANO_FARAD;
}

bool NumericKeypad::isMicro() {
    return m_options.editValueUnit == UNIT_MICRO_AMPER || m_options.editValueUnit == UNIT_MICRO_AMPER_PP || m_options.editValueUnit == UNIT_MICRO_FARAD;
}

bool NumericKeypad::isMilli() {
    return m_options.editValueUnit == UNIT_MILLI_VOLT ||
        m_options.editValueUnit == UNIT_MILLI_VOLT_PP ||
        m_options.editValueUnit == UNIT_MILLI_AMPER ||
        m_options.editValueUnit == UNIT_MILLI_AMPER_PP ||
        m_options.editValueUnit == UNIT_MILLI_WATT ||
        m_options.editValueUnit == UNIT_MILLI_SECOND ||
        m_options.editValueUnit == UNIT_MILLI_FARAD ||
        m_options.editValueUnit == UNIT_MILLI_HERTZ;
}

bool NumericKeypad::isKilo() {
    return m_options.editValueUnit == UNIT_KOHM || m_options.editValueUnit == UNIT_KHERTZ;
}

bool NumericKeypad::isMega() {
    return m_options.editValueUnit == UNIT_MOHM || m_options.editValueUnit == UNIT_MHERTZ;
}

Unit NumericKeypad::getPicoUnit() {
    if (m_options.editValueUnit == UNIT_FARAD || m_options.editValueUnit == UNIT_MILLI_FARAD || m_options.editValueUnit == UNIT_MICRO_FARAD || m_options.editValueUnit == UNIT_NANO_FARAD || m_options.editValueUnit == UNIT_PICO_FARAD)
        return UNIT_PICO_FARAD;
    return m_options.editValueUnit;
}

Unit NumericKeypad::getNanoUnit() {
    if (m_options.editValueUnit == UNIT_FARAD || m_options.editValueUnit == UNIT_MILLI_FARAD || m_options.editValueUnit == UNIT_MICRO_FARAD || m_options.editValueUnit == UNIT_NANO_FARAD || m_options.editValueUnit == UNIT_PICO_FARAD)
        return UNIT_NANO_FARAD;
    return m_options.editValueUnit;
}

Unit NumericKeypad::getMicroUnit() {
    if (m_options.editValueUnit == UNIT_AMPER || m_options.editValueUnit == UNIT_MILLI_AMPER)
        return UNIT_MICRO_AMPER;
    if (m_options.editValueUnit == UNIT_AMPER_PP || m_options.editValueUnit == UNIT_MILLI_AMPER_PP)
        return UNIT_MICRO_AMPER_PP;
    if (m_options.editValueUnit == UNIT_FARAD || m_options.editValueUnit == UNIT_MILLI_FARAD || m_options.editValueUnit == UNIT_MICRO_FARAD || m_options.editValueUnit == UNIT_NANO_FARAD || m_options.editValueUnit == UNIT_PICO_FARAD)
        return UNIT_MICRO_FARAD;
    return m_options.editValueUnit;
}

Unit NumericKeypad::getMilliUnit() {
    if (m_options.editValueUnit == UNIT_VOLT)
        return UNIT_MILLI_VOLT;
    if (m_options.editValueUnit == UNIT_VOLT_PP)
        return UNIT_MILLI_VOLT_PP;
    if (m_options.editValueUnit == UNIT_AMPER || m_options.editValueUnit == UNIT_MICRO_AMPER)
        return UNIT_MILLI_AMPER;
    if (m_options.editValueUnit == UNIT_AMPER_PP || m_options.editValueUnit == UNIT_MICRO_AMPER_PP)
        return UNIT_MILLI_AMPER_PP;
    if (m_options.editValueUnit == UNIT_WATT)
        return UNIT_MILLI_WATT;
    if (m_options.editValueUnit == UNIT_SECOND)
        return UNIT_MILLI_SECOND;
    if (m_options.editValueUnit == UNIT_FARAD || m_options.editValueUnit == UNIT_MILLI_FARAD || m_options.editValueUnit == UNIT_MICRO_FARAD || m_options.editValueUnit == UNIT_NANO_FARAD || m_options.editValueUnit == UNIT_PICO_FARAD)
        return UNIT_MILLI_FARAD;
    if (m_options.editValueUnit == UNIT_HERTZ || m_options.editValueUnit == UNIT_MILLI_HERTZ || m_options.editValueUnit == UNIT_KHERTZ || m_options.editValueUnit == UNIT_MHERTZ)
        return UNIT_MILLI_HERTZ;
    return m_options.editValueUnit;
}

Unit NumericKeypad::getKiloUnit() {
    if (m_options.editValueUnit == UNIT_OHM || m_options.editValueUnit == UNIT_KOHM || m_options.editValueUnit == UNIT_MOHM)
        return UNIT_KOHM;
    if (m_options.editValueUnit == UNIT_HERTZ || m_options.editValueUnit == UNIT_MILLI_HERTZ || m_options.editValueUnit == UNIT_KHERTZ || m_options.editValueUnit == UNIT_MHERTZ)
        return UNIT_KHERTZ;
    return m_options.editValueUnit;
}

Unit NumericKeypad::getMegaUnit() {
    if (m_options.editValueUnit == UNIT_OHM || m_options.editValueUnit == UNIT_KOHM || m_options.editValueUnit == UNIT_MOHM)
        return UNIT_MOHM;
    if (m_options.editValueUnit == UNIT_HERTZ || m_options.editValueUnit == UNIT_MILLI_HERTZ || m_options.editValueUnit == UNIT_KHERTZ || m_options.editValueUnit == UNIT_MHERTZ)
        return UNIT_MHERTZ;
    return m_options.editValueUnit;
}

void NumericKeypad::switchToPico() {
    m_options.editValueUnit = getPicoUnit();
}

void NumericKeypad::switchToNano() {
    m_options.editValueUnit = getNanoUnit();
}

void NumericKeypad::switchToMicro() {
    m_options.editValueUnit = getMicroUnit();
}

void NumericKeypad::switchToMilli() {
    m_options.editValueUnit = getMilliUnit();
}

void NumericKeypad::switchToKilo() {
    m_options.editValueUnit = getKiloUnit();
}

void NumericKeypad::switchToMega() {
    m_options.editValueUnit = getMegaUnit();
}

bool NumericKeypad::isMicroAmperAllowed() {
    if (m_options.slotIndex != -1 && m_options.subchannelIndex != -1) {
        return g_slots[m_options.slotIndex]->isMicroAmperAllowed(m_options.subchannelIndex);
    }

    return eez::gui::isMicroAmperAllowed(g_focusCursor, g_focusDataId);
}

bool NumericKeypad::isAmperAllowed() {
    if (m_options.slotIndex != -1 && m_options.subchannelIndex != -1) {
        return g_slots[m_options.slotIndex]->isAmperAllowed(m_options.subchannelIndex);
    }

    return eez::gui::isAmperAllowed(g_focusCursor, g_focusDataId);
}

Unit NumericKeypad::getSwitchToUnit() {
    if (m_options.editValueUnit == UNIT_VOLT)
        return UNIT_MILLI_VOLT;
    if (m_options.editValueUnit == UNIT_VOLT_PP)
        return UNIT_MILLI_VOLT_PP;
    if (m_options.editValueUnit == UNIT_MILLI_VOLT)
        return UNIT_VOLT;
    if (m_options.editValueUnit == UNIT_MILLI_VOLT_PP)
        return UNIT_VOLT_PP;
    if (m_options.editValueUnit == UNIT_AMPER)
        return UNIT_MILLI_AMPER;
    if (m_options.editValueUnit == UNIT_AMPER_PP)
        return UNIT_MILLI_AMPER_PP;
    if (m_options.editValueUnit == UNIT_MILLI_AMPER) {
        if (isMicroAmperAllowed()) {
            return UNIT_MICRO_AMPER;
        } else {
            return UNIT_AMPER;
        }
    }
    if (m_options.editValueUnit == UNIT_MILLI_AMPER_PP) {
        if (isMicroAmperAllowed()) {
            return UNIT_MICRO_AMPER_PP;
        } else {
            return UNIT_AMPER_PP;
        }
    }
    if (m_options.editValueUnit == UNIT_MICRO_AMPER) {
        if (isAmperAllowed()) {
            return UNIT_AMPER;
        } else {
            return UNIT_MILLI_AMPER;
        }
    }
    if (m_options.editValueUnit == UNIT_MICRO_AMPER_PP) {
        if (isAmperAllowed()) {
            return UNIT_AMPER_PP;
        } else {
            return UNIT_MILLI_AMPER_PP;
        }
    }
    if (m_options.editValueUnit == UNIT_WATT)
        return UNIT_MILLI_WATT;
    if (m_options.editValueUnit == UNIT_MILLI_WATT)
        return UNIT_WATT;
    if (m_options.editValueUnit == UNIT_SECOND)
        return UNIT_MILLI_SECOND;
    if (m_options.editValueUnit == UNIT_MILLI_SECOND)
        return UNIT_SECOND;
    if (m_options.editValueUnit == UNIT_OHM)
        return UNIT_KOHM;
    if (m_options.editValueUnit == UNIT_KOHM)
        return UNIT_MOHM;
    if (m_options.editValueUnit == UNIT_MOHM)
        return UNIT_OHM;
    if (m_options.editValueUnit == UNIT_HERTZ) {
        if (m_options.max >= 1000.0f) {
            return UNIT_KHERTZ;
        } else if (m_options.min < 1.0f) {
            return UNIT_MILLI_HERTZ;
        }
        return UNIT_HERTZ;
    }
    if (m_options.editValueUnit == UNIT_KHERTZ) {
        if (m_options.max >= 1000000.0f) {
            return UNIT_MHERTZ;
        } else if (m_options.min < 1.0f) {
            return UNIT_MILLI_HERTZ;
        } else if (m_options.min < 1000.0f) {
            return UNIT_HERTZ;
        }
        return UNIT_KHERTZ;
    }
    if (m_options.editValueUnit == UNIT_MHERTZ) {
        if (m_options.min < 1.0f) {
            return UNIT_MILLI_HERTZ;
        } else if (m_options.min < 1000.0f) {
            return UNIT_HERTZ;
        } else if (m_options.min < 1000000.0f) {
            return UNIT_KHERTZ;
        }
        return UNIT_MHERTZ;
    }
    if (m_options.editValueUnit == UNIT_MILLI_HERTZ) {
        if (m_options.max >= 1.0f) {
            return UNIT_HERTZ;
        }
        return UNIT_MILLI_HERTZ;
    }
    if (m_options.editValueUnit == UNIT_FARAD)
        return UNIT_MILLI_FARAD;
    if (m_options.editValueUnit == UNIT_MILLI_FARAD)
        return UNIT_MICRO_FARAD;
    if (m_options.editValueUnit == UNIT_MICRO_FARAD)
        return UNIT_NANO_FARAD;
    if (m_options.editValueUnit == UNIT_NANO_FARAD)
        return UNIT_PICO_FARAD;
    if (m_options.editValueUnit == UNIT_PICO_FARAD)
        return UNIT_FARAD;
    return m_options.editValueUnit;
}

void NumericKeypad::toggleEditUnit() {
    m_options.editValueUnit = getSwitchToUnit();
}

int NumericKeypad::getCursorPostion() {
    if (m_state == START) {
        return -1;
    }

    return Keypad::getCursorPostion();
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

    double value = sign * (a + b);

    if (isPico()) {
        value /= 1E12f;
    } else if (isNano()) {
        value /= 1E9f;
    } else if (isMicro()) {
        value /= 1000000;
    } else if (isMilli()) {
        value /= 1000;
    } else if (isKilo()) {
        value *= 1000;
    } else if (isMega()) {
        value *= 1000000;
    }

    return value;
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

void NumericKeypad::option1() {
    if (m_options.flags.option1ButtonEnabled && m_options.option1) {
        m_options.option1();
    }
}

void NumericKeypad::option2() {
    if (m_options.flags.option2ButtonEnabled && m_options.option2) {
        m_options.option2();
    }
}

void NumericKeypad::option3() {
    if (m_options.flags.option3ButtonEnabled && m_options.option3) {
        m_options.option3();
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

bool NumericKeypad::isOkEnabled() {
    return true;
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
                errorMessage("Invalid IP address format!");
            }

            return;
        } else {
            double value = getValue();
            if (isNaN(value)) {
                sound::playBeep();
                return;
            }

            if (!isNaN(m_options.min) && (float)value < m_options.min && !(value == 0 && m_options.allowZero)) {
                psuErrorMessage(0, MakeLessThenMinMessageValue(m_options.min, m_startValue));
            } else if (!isNaN(m_options.max) && value > m_options.max) {
                psuErrorMessage(0, MakeGreaterThenMaxMessageValue(m_options.max, m_startValue));
            } else {
                m_okFloatCallback((float)value);
                return;
            }

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

#if OPTION_ENCODER

void NumericKeypad::onEncoderClicked() {
    if (m_state == START) {
        if (m_appContext->getActivePageId() == PAGE_ID_EDIT_MODE_KEYPAD) {
            return;
        }
    }
    ok();
}

void NumericKeypad::onEncoder(int counter) {
    if (m_state == START) {
        if (m_appContext->getActivePageId() == PAGE_ID_EDIT_MODE_KEYPAD) {
            return;
        }

        if (m_startValue.getType() == VALUE_TYPE_FLOAT) {
            float precision;
            auto channel = Channel::getBySlotIndex(m_options.slotIndex, m_options.subchannelIndex);
            if (channel) {
                precision = psu::channel_dispatcher::getValuePrecision(*channel, m_startValue.getUnit(), m_startValue.getFloat());
            } else {
                precision = m_options.encoderPrecision;
            }
            float newValue = m_startValue.getFloat() + counter * precision;
            newValue = roundPrec(newValue, precision);
            newValue = clamp(newValue, m_options.min, m_options.max);
            m_startValue = MakeValue(newValue, m_startValue.getUnit());
            return;
        } else if (m_startValue.getType() == VALUE_TYPE_INT) {
            int newValue = m_startValue.getInt() + counter;

            if (newValue < (int)m_options.min) {
                newValue = (int)m_options.min;
            }

            if (newValue > (int)m_options.max) {
                newValue = (int)m_options.max;
            }

            m_startValue = Value(newValue);
            return;
        }
    }

    sound::playBeep();
}

#endif


} // namespace gui
} // namespace psu
} // namespace eez

#endif
