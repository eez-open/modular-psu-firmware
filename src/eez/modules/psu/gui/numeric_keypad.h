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

#include <eez/modules/psu/gui/keypad.h>

namespace eez {
namespace psu {
namespace gui {

struct NumericKeypadOptions {
    NumericKeypadOptions();

	  int pageId;

    int channelIndex;

    Unit editValueUnit;

    float min;
    float max;
    float def;

    struct {
        unsigned checkWhileTyping : 1;
        unsigned option1ButtonEnabled : 1;
        unsigned option2ButtonEnabled : 1;
        unsigned signButtonEnabled : 1;
        unsigned dotButtonEnabled : 1;
    } flags;

    const char *option1ButtonText;
    void (*option1)();

    const char *option2ButtonText;
    void (*option2)();

    void enableMaxButton();
    void enableMinButton();
    void enableDefButton();

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
    void init(const char *label, const eez::gui::data::Value &value, NumericKeypadOptions &options,
              void (*okFloat)(float), void (*okUint32)(uint32_t), void (*cancel)());
    static NumericKeypad *start(const char *label, const eez::gui::data::Value &value,
                                NumericKeypadOptions &options, void (*okFloat)(float),
                                void (*okUint32)(uint32_t), void (*cancel)());

    bool isEditing();

    Unit getEditUnit();
    Unit getValueUnit();
    
    void switchToMilli();
    void switchToMicro();

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
    void setMaxValue();
    void setMinValue();
    void setDefValue();
    void ok();
    void cancel();

#if OPTION_ENCODER
    bool onEncoderClicked();
    bool onEncoder(int counter);
#endif

    Unit getSwitchToUnit();

    NumericKeypadOptions m_options;

  private:
    eez::gui::data::Value m_startValue;
    NumericKeypadState m_state;
    void (*m_okFloatCallback)(float);
    void (*m_okUint32Callback)(uint32_t);
    void (*m_cancelCallback)();

    void appendEditUnit(char *text);
    float getValue();
    char getDotSign();
    bool isMilli();
    bool isMicro();
    Unit getMilliUnit();
    Unit getMicroUnit();
    void toggleEditUnit();
    int getNumDecimalDigits();
    bool isValueValid();
    bool checkNumSignificantDecimalDigits();
    void digit(int d);
    void dot();
    void reset();
};

} // namespace gui
} // namespace psu
} // namespace eez
