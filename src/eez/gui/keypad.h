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

#include <eez/gui/gui.h>

namespace eez {
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

	void start(AppContext *appContext, const char *label, const char *text, int minChars_, int maxChars_, bool isPassword_, void(*ok)(char *), void(*cancel)(), void(*setDefault)());

    void pageAlloc();
    void pageFree();

    void key();
    virtual void key(char ch);
    virtual void space();
    virtual void back();
    virtual void clear();
    virtual void sign();
    virtual void unit();
    virtual bool isOkEnabled();
    virtual void ok();
    virtual void cancel();
    virtual bool canSetDefault();
    virtual void setDefault();

    virtual void getKeypadText(char *text, size_t count);
    Value getKeypadTextValue();

    void insertChar(char c);

    virtual Unit getSwitchToUnit() {
        return UNIT_UNKNOWN;
    }

    KeypadMode m_keypadMode;

    virtual int getCursorPostion();
    void setCursorPosition(int cursorPosition);

    int getXScroll(const WidgetCursor &widgetCursor);

protected:
    AppContext *m_appContext;

    char m_label[MAX_KEYPAD_LABEL_LENGTH + 1];
    char m_keypadText[MAX_KEYPAD_TEXT_LENGTH + 2];
    int m_cursorPosition;
    int m_xScroll;
    int m_minChars;
    int m_maxChars;

    void init(AppContext *appContext, const char *label);

private:
    bool m_isPassword;
    uint32_t m_lastKeyAppendTimeMs;

    void (*m_okCallback)(char *);
    void (*m_cancelCallback)();
    void (*m_setDefaultCallback)();
};

////////////////////////////////////////////////////////////////////////////////

enum {
    NUMERIC_KEYPAD_OPTION_ACTION_NONE,
    NUMERIC_KEYPAD_OPTION_ACTION_MAX,
    NUMERIC_KEYPAD_OPTION_ACTION_MIN,
    NUMERIC_KEYPAD_OPTION_ACTION_DEF,
    NUMERIC_KEYPAD_OPTION_USER = NUMERIC_KEYPAD_OPTION_ACTION_DEF + 1
};

struct NumericKeypadOptions {
    NumericKeypadOptions();

	int pageId;

    int slotIndex;
	int subchannelIndex;

    Unit editValueUnit;

    bool allowZero;
    float min;
    float max;
    float def;

    struct {
        unsigned checkWhileTyping : 1;
        unsigned signButtonEnabled : 1;
        unsigned dotButtonEnabled : 1;
    } flags;

    int option1Action;
    const char *option1ButtonText;

    int option2Action;
    const char *option2ButtonText;

    int option3Action;
    const char *option3ButtonText;

    float encoderPrecision;

	void enableMaxButton();
	void enableMinButton();
	void enableDefButton();
};

enum NumericKeypadState {
    START,
    EMPTY,
    D0,
    D1,
    DOT,
    D2,
    D3,

    BEFORE_DOT,
    AFTER_DOT
};

class NumericKeypad : public Keypad {
public:
    void init(
        AppContext *appContext,
        const char *label,
        const Value &value,
        NumericKeypadOptions &options,
        void (*okFloat)(float),
        void (*okUint32)(uint32_t),
        void (*cancel)()
    );

    bool isEditing();

    Unit getEditUnit();
    Unit getValueUnit();
    
    void switchToPico();
    void switchToNano();
    void switchToMicro();
    void switchToMilli();
    void switchToKilo();
    void switchToMega();

    void getKeypadText(char *text, size_t count) override;
    virtual bool getText(char *text, size_t count);

    void key(char ch) override;
    void space() override;
    void caps();
    void back() override;
    void clear() override;
    void sign() override;
    void unit() override;
    bool isOkEnabled() override;
    void ok() override;
    void cancel() override;

    void setMaxValue();
    void setMinValue();
    void setDefValue();

	virtual bool isMicroAmperAllowed();
	virtual bool isAmperAllowed();

    Unit getSwitchToUnit() override;

	virtual void showLessThanMinErrorMessage();
	virtual void showGreaterThanMaxErrorMessage();

    virtual int getCursorPostion() override;

    NumericKeypadOptions m_options;

protected:
    Value m_startValue;
    NumericKeypadState m_state;
    void (*m_okFloatCallback)(float);
    void (*m_okUint32Callback)(uint32_t);
    void (*m_cancelCallback)();

    void appendEditUnit(char *text, size_t maxTextLength);
    double getValue();
    char getDotSign();

    bool isPico();
    bool isNano();
    bool isMicro();
    bool isMilli();
    bool isKilo();
    bool isMega();

    Unit getPicoUnit();
    Unit getNanoUnit();
    Unit getMicroUnit();
    Unit getMilliUnit();
    Unit getKiloUnit();
    Unit getMegaUnit();

    void toggleEditUnit();
    bool checkNumSignificantDecimalDigits();
    void digit(int d);
    void dot();
    void reset();
};

// optionActionIndex: possible values are 1, 2 and 3 (for three possible optional actions)
void executeNumericKeypadOptionHook(int optionActionIndex);

////////////////////////////////////////////////////////////////////////////////

Keypad *getActiveKeypad();
NumericKeypad *getActiveNumericKeypad();


} // namespace gui
} // namespace eez
