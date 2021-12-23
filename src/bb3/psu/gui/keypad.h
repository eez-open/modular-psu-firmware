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

#include <eez/gui/keypad.h>

namespace eez {
namespace psu {
namespace gui {

////////////////////////////////////////////////////////////////////////////////

void startTextKeypad(const char *label, const char *text, int minChars, int maxChars, bool isPassword, void(*ok)(char *), void(*cancel)(), void(*setDefault)() = nullptr);
void startTextKeypadWithReplacePage(const char *label, const char *text, int minChars, int maxChars, bool isPassword, void(*ok)(char *), void(*cancel)());

////////////////////////////////////////////////////////////////////////////////

class NumericKeypad : public eez::gui::NumericKeypad {
public:
#if OPTION_ENCODER
	void onEncoderClicked() override;
	void onEncoder(int counter) override;
#endif

	float getPrecision() override;

private:
	bool getText(char *text, size_t count) override;
	bool isMicroAmperAllowed() override;
	bool isAmperAllowed() override;
	void showLessThanMinErrorMessage() override;
	void showGreaterThanMaxErrorMessage() override;
};

NumericKeypad *startNumericKeypad(
	AppContext *appContext,
	const char *label,
	const Value &value,
	NumericKeypadOptions &options,
	void(*okFloat)(float),
	void(*okUint32)(uint32_t),
	void(*cancel)()
);

NumericKeypad *startNumericKeypad(
	const char *label,
	const Value &value,
	NumericKeypadOptions &options,
	void(*okFloat)(float),
	void(*okUint32)(uint32_t),
	void(*cancel)()
);

enum {
	NUMERIC_KEYPAD_OPTION_ACTION_LIST_COUNT_SET_TO_INFINITY = NUMERIC_KEYPAD_OPTION_USER,
	NUMERIC_KEYPAD_OPTION_ACTION_SET_TO_INFINITY_VALUE,
#if defined(EEZ_PLATFORM_SIMULATOR)	
    NUMERIC_KEYPAD_OPTION_ACTION_SIMULATOR_DISCONNECT_LOAD
#endif
};

////////////////////////////////////////////////////////////////////////////////

void onKeypadTextTouch(const WidgetCursor &widgetCursor, Event &touchEvent);

} // namespace gui
} // namespace psu
} // namespace eez
