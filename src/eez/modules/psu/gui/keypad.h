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
    virtual void option3();
    virtual void setMaxValue();
    virtual void setMinValue();
    virtual void setDefValue();
    virtual bool isOkEnabled();
    virtual void ok();
    virtual void cancel();

    virtual void getKeypadText(char *text);
    Value getKeypadTextValue();

    void insertChar(char c);

    virtual Unit getSwitchToUnit() {
        return UNIT_UNKNOWN;
    }

    KeypadMode m_keypadMode;

    int getCursorPostion();
    void setCursorPosition(int cursorPosition);

    int getXScroll(const WidgetCursor &widgetCursor);

protected:
    AppContext *m_appContext;

    char m_stateText[2][MAX_KEYPAD_TEXT_LENGTH + 2];
    char m_label[MAX_KEYPAD_LABEL_LENGTH + 1];
    char m_keypadText[MAX_KEYPAD_TEXT_LENGTH + 2];
    int m_cursorPosition;
    int m_xScroll;
    int m_minChars;
    int m_maxChars;

    void init(AppContext *appContext, const char *label);

private:
    bool m_isPassword;
    uint32_t m_lastKeyAppendTime;

    void (*m_okCallback)(char *); // +2 for cursor and zero at the end
    void (*m_cancelCallback)();

    void start(AppContext *appContext, const char *label, const char *text, int minChars_, int maxChars_, bool isPassword_, void (*ok)(char *), void (*cancel)());
};

////////////////////////////////////////////////////////////////////////////////

struct NumericKeypadOptions {
    NumericKeypadOptions();

	int pageId;

    int channelIndex;

    Unit editValueUnit;

    bool allowZero;
    float min;
    float max;
    float def;

    struct {
        unsigned checkWhileTyping : 1;
        unsigned option1ButtonEnabled : 1;
        unsigned option2ButtonEnabled : 1;
        unsigned option3ButtonEnabled : 1;
        unsigned signButtonEnabled : 1;
        unsigned dotButtonEnabled : 1;
    } flags;

    const char *option1ButtonText;
    void (*option1)();

    const char *option2ButtonText;
    void (*option2)();

    const char *option3ButtonText;
    void (*option3)();

    void enableMinButton();
    void enableMaxButton();
    void enableDefButton();

    float encoderPrecision;

  private:
    static void maxOption();
    static void minOption();
    static void defOption();
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

    static NumericKeypad *start(
        AppContext *appContext,
        const char *label,
        const Value &value,
        NumericKeypadOptions &options,
        void(*okFloat)(float),
        void(*okUint32)(uint32_t), void(*cancel)()
    );

    static NumericKeypad *start(
        const char *label,
        const Value &value,
        NumericKeypadOptions &options,
        void(*okFloat)(float),
        void(*okUint32)(uint32_t),
        void(*cancel)()
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

    void getKeypadText(char *text);
    bool getText(char *text, int count);

    void key(char ch);
    void space();
    void caps();
    void back();
    void clear();
    void sign();
    void unit();
    void option1();
    void option2();
    void option3();
    void setMaxValue();
    void setMinValue();
    void setDefValue();
    bool isOkEnabled();
    void ok();
    void cancel();

#if OPTION_ENCODER
    void onEncoderClicked();
    void onEncoder(int counter);
#endif

    Unit getSwitchToUnit();

    NumericKeypadOptions m_options;

private:
    Value m_startValue;
    NumericKeypadState m_state;
    void (*m_okFloatCallback)(float);
    void (*m_okUint32Callback)(uint32_t);
    void (*m_cancelCallback)();

    void appendEditUnit(char *text);
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

////////////////////////////////////////////////////////////////////////////////

Keypad *getActiveKeypad();
NumericKeypad *getActiveNumericKeypad();
void onKeypadTextTouch(const WidgetCursor &widgetCursor, Event &touchEvent);

} // namespace gui
} // namespace psu
} // namespace eez
