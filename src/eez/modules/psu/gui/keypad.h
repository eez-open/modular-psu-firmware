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

#include <eez/modules/psu/conf_advanced.h>

using namespace eez::gui;

namespace eez {
namespace psu {
namespace gui {

////////////////////////////////////////////////////////////////////////////////

enum KeypadMode {
    KEYPAD_MODE_LOWERCASE,
    KEYPAD_MODE_UPPERCASE,
    KEYPAD_MODE_SYMBOL
};

class Keypad : public Page {
  public:
    AppContext *getAppContext() { return m_appContext;  }

    void pageAlloc();
    void pageFree();

    static void startPush(const char *label, const char *text, int minChars, int maxChars, bool isPassword, void (*ok)(char *), void (*cancel)());
    static void startReplace(const char *label, const char *text, int minChars, int maxChars, bool isPassword, void (*ok)(char *), void (*cancel)());

    void key();
    virtual void key(char ch);
    virtual void space();
    virtual void back();
    virtual void clear();
    virtual void sign();
    virtual void unit();
    virtual void option1();
    virtual void option2();
    virtual void setMaxValue();
    virtual void setMinValue();
    virtual void setDefValue();
    virtual bool isOkEnabled();
    virtual void ok();
    virtual void cancel();

    virtual void getKeypadText(char *text);
    eez::gui::data::Value getKeypadTextValue();

    void appendChar(char c);
    void appendCursor(char *text);

    virtual Unit getSwitchToUnit() {
        return UNIT_UNKNOWN;
    }

    KeypadMode m_keypadMode;

protected:
    AppContext *m_appContext;

    char m_stateText[2][MAX_KEYPAD_TEXT_LENGTH + 2];
    char m_label[MAX_KEYPAD_LABEL_LENGTH + 1];
    char m_keypadText[MAX_KEYPAD_TEXT_LENGTH + 2];
    int m_minChars;
    int m_maxChars;

    void init(AppContext *appContext, const char *label);

private:
    bool m_isPassword;
    uint32_t m_lastKeyAppendTime;
    bool m_cursor;
    uint32_t m_lastCursorChangeTime;

    void (*m_okCallback)(char *); // +2 for cursor and zero at the end
    void (*m_cancelCallback)();

    void start(AppContext *appContext, const char *label, const char *text, int minChars_, int maxChars_, bool isPassword_, void (*ok)(char *), void (*cancel)());
};

class NumericKeypad;

Keypad *getActiveKeypad();
NumericKeypad *getActiveNumericKeypad();

} // namespace gui
} // namespace psu
} // namespace eez
