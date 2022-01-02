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

#include <eez/core/sound.h>

#include <eez/gui/keypad.h>
#include <eez/gui/widgets/display_data.h>

#include <bb3/psu/psu.h>
#include <bb3/psu/channel_dispatcher.h>
#include <bb3/psu/gui/psu.h>
#include <bb3/psu/gui/edit_mode.h>
#include <bb3/psu/gui/keypad.h>
#include <bb3/psu/gui/page_ch_settings.h>

#if defined(EEZ_PLATFORM_SIMULATOR)
#include <bb3/platform/simulator/front_panel.h>
#endif

namespace eez {
namespace psu {
namespace gui {

static Keypad g_keypad;
static NumericKeypad g_numericKeypad;

void startTextKeypad(const char *label, const char *text, int minChars_, int maxChars_, bool isPassword_, void(*ok)(char *), void(*cancel)(), void(*setDefault)()) {
	g_keypad.start(&g_psuAppContext, label, text, minChars_, maxChars_, isPassword_, ok, cancel, setDefault);
	g_psuAppContext.pushPage(PAGE_ID_KEYPAD, &g_keypad);
}

void startTextKeypadWithReplacePage(const char *label, const char *text, int minChars_, int maxChars_, bool isPassword_, void(*ok)(char *), void(*cancel)()) {
	g_keypad.start(&g_psuAppContext, label, text, minChars_, maxChars_, isPassword_, ok, cancel, nullptr);
	g_psuAppContext.replacePage(PAGE_ID_KEYPAD, &g_keypad);
}

////////////////////////////////////////////////////////////////////////////////

bool NumericKeypad::getText(char *text, size_t count) {
	if (m_state == START && m_appContext->getActivePageId() == PAGE_ID_EDIT_MODE_KEYPAD) {
		edit_mode::getCurrentValue().toText(text, count);
		return false;
	}
	return eez::gui::NumericKeypad::getText(text, count);
}

bool NumericKeypad::isMicroAmperAllowed() {
	if (m_options.slotIndex != -1 && m_options.subchannelIndex != -1) {
		return g_slots[m_options.slotIndex]->isMicroAmperAllowed(m_options.subchannelIndex);
	}

	WidgetCursor widgetCursor;
	widgetCursor = g_focusCursor;
	return eez::gui::isMicroAmperAllowed(widgetCursor, g_focusDataId);
}

bool NumericKeypad::isAmperAllowed() {
	if (m_options.slotIndex != -1 && m_options.subchannelIndex != -1) {
		return g_slots[m_options.slotIndex]->isAmperAllowed(m_options.subchannelIndex);
	}

	WidgetCursor widgetCursor;
	widgetCursor = g_focusCursor;
	return eez::gui::isAmperAllowed(widgetCursor, g_focusDataId);
}

void NumericKeypad::showLessThanMinErrorMessage() {
	psuErrorMessage(0, MakeLessThenMinMessageValue(m_options.min, m_startValue));
}

void NumericKeypad::showGreaterThanMaxErrorMessage() {
	psuErrorMessage(0, MakeGreaterThenMaxMessageValue(m_options.max, m_startValue));
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

float NumericKeypad::getPrecision() {
	auto channel = Channel::getBySlotIndex(m_options.slotIndex, m_options.subchannelIndex);
	if (channel) {
		return psu::channel_dispatcher::getValuePrecision(*channel, m_startValue.getUnit(), m_startValue.getFloat());
	}
	return m_options.encoderPrecision;
}

void NumericKeypad::onEncoder(int counter) {
	if (m_state == START) {
		if (m_appContext->getActivePageId() == PAGE_ID_EDIT_MODE_KEYPAD) {
			return;
		}

		if (m_startValue.getType() == VALUE_TYPE_FLOAT) {
			float precision = getPrecision();
			float newValue = m_startValue.getFloat() + counter * precision;
			newValue = roundPrec(newValue, precision);
			newValue = clamp(newValue, m_options.min, m_options.max);
			m_startValue = MakeValue(newValue, m_startValue.getUnit());
			return;
		} else if (m_startValue.getType() == VALUE_TYPE_INT32) {
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

////////////////////////////////////////////////////////////////////////////////

NumericKeypad *startNumericKeypad(
	AppContext *appContext,
	const char *label,
	const Value &value,
	NumericKeypadOptions &options,
	void(*okFloat)(float),
	void(*okUint32)(uint32_t),
	void(*cancel)()
) {
	g_numericKeypad.init(appContext, label, value, options, okFloat, okUint32, cancel);
	appContext->pushPage(options.pageId, &g_numericKeypad);
	return &g_numericKeypad;
}

NumericKeypad *startNumericKeypad(
	const char *label,
	const Value &value,
	NumericKeypadOptions &options,
	void(*okFloat)(float),
	void(*okUint32)(uint32_t),
	void(*cancel)()
) {
	return startNumericKeypad(&g_psuAppContext, label, value, options, okFloat, okUint32, cancel);
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

} // namespace gui
} // namespace psu
} // namespace eez

namespace eez {
namespace gui {

using namespace eez::psu;
using namespace eez::psu::gui;

extern void onSetInfinityValue();

void executeNumericKeypadOptionHook(int optionActionIndex) {
	auto numericKeypad = getActiveNumericKeypad();
	if (numericKeypad) {
		int optionAction = 
			optionActionIndex == 1 ? numericKeypad->m_options.option1Action :
			optionActionIndex == 2 ? numericKeypad->m_options.option2Action :
			optionActionIndex == 3 ? numericKeypad->m_options.option3Action :
			NUMERIC_KEYPAD_OPTION_ACTION_NONE;
		
		if (optionAction == NUMERIC_KEYPAD_OPTION_ACTION_MAX) {
			numericKeypad->setMaxValue();
		} else if (optionAction == NUMERIC_KEYPAD_OPTION_ACTION_MIN) {
			numericKeypad->setMinValue();
		} else if (optionAction == NUMERIC_KEYPAD_OPTION_ACTION_DEF) {
			numericKeypad->setDefValue();
		}
		else if (optionAction == NUMERIC_KEYPAD_OPTION_ACTION_LIST_COUNT_SET_TO_INFINITY) {
			ChSettingsListsPage::onListCountSetToInfinity();
		} else if (optionAction == NUMERIC_KEYPAD_OPTION_ACTION_SET_TO_INFINITY_VALUE) {
			onSetInfinityValue();
		}
#if defined(EEZ_PLATFORM_SIMULATOR)	
		else if (optionAction == NUMERIC_KEYPAD_OPTION_ACTION_SIMULATOR_DISCONNECT_LOAD) {
			g_frontPanelAppContext.popPage();
			channel_dispatcher::setLoadEnabled(*g_channel, false);
		} 
#endif
	}
}

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

} // gui
} // eez

#endif
